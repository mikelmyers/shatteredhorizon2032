// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHGameSessionManager.h"

#include "Core/SHPlayerState.h"
#include "Progression/SHProgressionSystem.h"
#include "UI/SHMainMenuWidget.h"
#include "UI/SHLoadoutWidget.h"
#include "UI/SHLoadingScreenWidget.h"
#include "UI/SHAfterActionWidget.h"
#include "UI/SHPauseMenuWidget.h"

#include "CommonActivatableWidget.h"
#include "Widgets/CommonActivatableWidgetContainerBase.h"

#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY(LogSH_Session);

// =======================================================================
//  Subsystem lifecycle
// =======================================================================

void USHGameSessionManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Ensure the progression system is initialized before us.
	Collection.InitializeDependency<USHProgressionSystem>();

	CurrentState = ESHSessionState::MainMenu;
	PreviousState = ESHSessionState::MainMenu;
	StateBeforePause = ESHSessionState::InMission;
	ActiveSaveSlot = 0;
	bLastMissionVictory = false;
	MissionElapsedTime = 0.f;

	// Set up default difficulty XP multipliers if not configured.
	if (DifficultyXPMultipliers.Num() == 0)
	{
		DifficultyXPMultipliers.Add(1.0f);  // 0: Normal
		DifficultyXPMultipliers.Add(1.25f); // 1: Hard
		DifficultyXPMultipliers.Add(1.5f);  // 2: Veteran
		DifficultyXPMultipliers.Add(2.0f);  // 3: Realistic
		DifficultyXPMultipliers.Add(2.5f);  // 4: Iron Man
	}

	UE_LOG(LogSH_Session, Log, TEXT("[SHGameSessionManager] Initialized. Starting in MainMenu state."));
}

void USHGameSessionManager::Deinitialize()
{
	ClearActiveWidget();
	WidgetStack = nullptr;

	UE_LOG(LogSH_Session, Log, TEXT("[SHGameSessionManager] Deinitialized."));

	Super::Deinitialize();
}

// =======================================================================
//  State machine — public interface
// =======================================================================

bool USHGameSessionManager::TransitionTo(ESHSessionState NewState)
{
	if (NewState == CurrentState)
	{
		UE_LOG(LogSH_Session, Verbose,
			TEXT("[SHGameSessionManager] TransitionTo: Already in state %d. Ignoring."),
			static_cast<int32>(NewState));
		return true;
	}

	if (!IsTransitionValid(CurrentState, NewState))
	{
		UE_LOG(LogSH_Session, Warning,
			TEXT("[SHGameSessionManager] TransitionTo: Invalid transition from %d to %d. Denied."),
			static_cast<int32>(CurrentState), static_cast<int32>(NewState));
		return false;
	}

	const ESHSessionState OldState = CurrentState;

	UE_LOG(LogSH_Session, Log,
		TEXT("[SHGameSessionManager] Transition: %d -> %d"),
		static_cast<int32>(OldState), static_cast<int32>(NewState));

	ExitState(OldState);

	PreviousState = OldState;
	CurrentState = NewState;

	EnterState(NewState);

	OnSessionStateChanged.Broadcast(OldState, NewState);

	return true;
}

// =======================================================================
//  Campaign flow
// =======================================================================

void USHGameSessionManager::StartNewCampaign(int32 DifficultyLevel)
{
	UE_LOG(LogSH_Session, Log,
		TEXT("[SHGameSessionManager] Starting new campaign. Difficulty: %d"), DifficultyLevel);

	// Reset progression for a new campaign.
	if (USHProgressionSystem* Progression = GetProgressionSystem())
	{
		Progression->SetProgressionData(FSHProgressionData());
	}

	// Configure the first mission.
	CurrentMissionConfig = DefaultFirstMission;
	CurrentMissionConfig.DifficultyLevel = FMath::Clamp(DifficultyLevel, 0, 4);

	// Assign save slot 0 for new campaigns (auto-save slot).
	ActiveSaveSlot = 0;

	// Save initial progression state.
	if (USHProgressionSystem* Progression = GetProgressionSystem())
	{
		Progression->SaveProgression(GetSaveSlotName(ActiveSaveSlot));
	}

	TransitionTo(ESHSessionState::Briefing);
}

bool USHGameSessionManager::ContinueCampaign(int32 SaveSlot)
{
	UE_LOG(LogSH_Session, Log,
		TEXT("[SHGameSessionManager] Continuing campaign from save slot %d."), SaveSlot);

	USHProgressionSystem* Progression = GetProgressionSystem();
	if (!Progression)
	{
		UE_LOG(LogSH_Session, Error,
			TEXT("[SHGameSessionManager] ContinueCampaign: Progression system unavailable."));
		return false;
	}

	const FString SlotName = GetSaveSlotName(SaveSlot);
	if (!Progression->LoadProgression(SlotName))
	{
		UE_LOG(LogSH_Session, Warning,
			TEXT("[SHGameSessionManager] ContinueCampaign: No save data found for slot '%s'."),
			*SlotName);
		return false;
	}

	ActiveSaveSlot = SaveSlot;

	// Use the default first mission config as a fallback. In a full
	// implementation the save file would include the next mission ID
	// and the campaign state, which would be used to look up the
	// correct FSHMissionConfig from a data table or campaign graph.
	CurrentMissionConfig = DefaultFirstMission;

	TransitionTo(ESHSessionState::Briefing);
	return true;
}

// =======================================================================
//  Mission lifecycle
// =======================================================================

void USHGameSessionManager::StartMission(const FSHMissionConfig& Config)
{
	if (!Config.IsValid())
	{
		UE_LOG(LogSH_Session, Error,
			TEXT("[SHGameSessionManager] StartMission: Invalid mission config (MissionId: %s, MapName: %s). Aborting."),
			*Config.MissionId.ToString(), *Config.MapName);
		return;
	}

	CurrentMissionConfig = Config;
	MissionElapsedTime = 0.f;
	bLastMissionVictory = false;

	UE_LOG(LogSH_Session, Log,
		TEXT("[SHGameSessionManager] Starting mission '%s' on map '%s' (Difficulty: %d, Coop: %s)."),
		*Config.MissionId.ToString(), *Config.MapName,
		Config.DifficultyLevel, Config.bIsCoop ? TEXT("Yes") : TEXT("No"));

	// Transition to Loading, which will show the loading screen,
	// then travel to the map.
	TransitionTo(ESHSessionState::Loading);
}

void USHGameSessionManager::EndMission(bool bVictory)
{
	if (CurrentState != ESHSessionState::InMission && CurrentState != ESHSessionState::Paused)
	{
		UE_LOG(LogSH_Session, Warning,
			TEXT("[SHGameSessionManager] EndMission called but not in mission (state: %d). Ignoring."),
			static_cast<int32>(CurrentState));
		return;
	}

	bLastMissionVictory = bVictory;

	UE_LOG(LogSH_Session, Log,
		TEXT("[SHGameSessionManager] Ending mission '%s'. Victory: %s."),
		*CurrentMissionConfig.MissionId.ToString(),
		bVictory ? TEXT("Yes") : TEXT("No"));

	// If paused, unpause first.
	if (CurrentState == ESHSessionState::Paused)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				PC->SetPause(false);
			}
		}
	}

	// Gather engagement records and compute XP.
	FSHEngagementRecord EngagementRecord;
	FSHXPBreakdown XPBreakdown;
	FSHRankProgress RankProgress;

	UWorld* World = GetWorld();
	if (World)
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (ASHPlayerState* PS = PC->GetPlayerState<ASHPlayerState>())
			{
				EngagementRecord = PS->GetEngagementRecord();
			}
		}
	}

	// Compute XP from engagement record.
	XPBreakdown = ComputeXPAward(EngagementRecord, bVictory);

	// Push XP to progression system.
	if (USHProgressionSystem* Progression = GetProgressionSystem())
	{
		const int32 TotalXP = XPBreakdown.GetTotal();
		if (TotalXP > 0)
		{
			Progression->AwardXP(TotalXP, FName(TEXT("MissionComplete")));
		}

		// Build rank progress data for the AAR widget.
		RankProgress.CurrentRankName = USHProgressionSystem::GetRankDisplayName(Progression->GetCurrentRank());

		// Calculate next rank name.
		const uint8 CurrentRankValue = static_cast<uint8>(Progression->GetCurrentRank());
		const uint8 MaxRankValue = static_cast<uint8>(ESHRank::GunnerySergeant);
		if (CurrentRankValue < MaxRankValue)
		{
			const ESHRank NextRank = static_cast<ESHRank>(CurrentRankValue + 1);
			RankProgress.NextRankName = USHProgressionSystem::GetRankDisplayName(NextRank);
		}
		else
		{
			RankProgress.NextRankName = FText::FromString(TEXT("Max Rank"));
		}

		RankProgress.CurrentXP = Progression->GetCurrentXP();
		RankProgress.RequiredXP = FMath::Max(1, Progression->GetXPToNextRank() + Progression->GetCurrentXP());

		// Auto-save progression after mission.
		Progression->SaveProgression(GetSaveSlotName(ActiveSaveSlot));
	}

	// Broadcast mission end event.
	OnMissionEnded.Broadcast(bVictory, CurrentMissionConfig);

	// Transition to AfterAction. The EnterState handler will populate the AAR widget.
	// We need to cache the data so EnterState can use it.
	// Store the data in a way the widget can access it — we'll populate on enter.
	TransitionTo(ESHSessionState::AfterAction);

	// Populate the AAR widget if it was created during EnterState.
	if (USHAfterActionWidget* AARWidget = Cast<USHAfterActionWidget>(ActiveWidget))
	{
		const FText AIAnalysis = bVictory
			? FText::FromString(TEXT("Mission successful. Objectives completed within acceptable parameters. Tactical performance analysis available."))
			: FText::FromString(TEXT("Mission failed. Tactical review indicates areas for improvement. Recommending additional training exercises."));

		AARWidget->SetAfterActionData(EngagementRecord, AIAnalysis, XPBreakdown, RankProgress);
	}
}

void USHGameSessionManager::RestartMission()
{
	if (!CurrentMissionConfig.IsValid())
	{
		UE_LOG(LogSH_Session, Warning,
			TEXT("[SHGameSessionManager] RestartMission: No valid mission config. Ignoring."));
		return;
	}

	UE_LOG(LogSH_Session, Log,
		TEXT("[SHGameSessionManager] Restarting mission '%s'."),
		*CurrentMissionConfig.MissionId.ToString());

	// If paused, unpause first.
	if (CurrentState == ESHSessionState::Paused)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				PC->SetPause(false);
			}
		}
	}

	PerformInterMissionCleanup();

	// Cache the config and re-start.
	const FSHMissionConfig ConfigCopy = CurrentMissionConfig;
	MissionElapsedTime = 0.f;

	StartMission(ConfigCopy);
}

void USHGameSessionManager::ReturnToMainMenu()
{
	UE_LOG(LogSH_Session, Log, TEXT("[SHGameSessionManager] Returning to main menu."));

	// If paused or in mission, unpause first.
	if (CurrentState == ESHSessionState::Paused || CurrentState == ESHSessionState::InMission)
	{
		UWorld* World = GetWorld();
		if (World)
		{
			if (APlayerController* PC = World->GetFirstPlayerController())
			{
				PC->SetPause(false);
			}
		}
	}

	PerformInterMissionCleanup();

	// Force state to something that allows MainMenu transition, then travel.
	const ESHSessionState OldState = CurrentState;
	ExitState(OldState);
	PreviousState = OldState;
	CurrentState = ESHSessionState::MainMenu;

	// Travel to the main menu map.
	TravelToMap(MainMenuMapName);

	EnterState(ESHSessionState::MainMenu);
	OnSessionStateChanged.Broadcast(OldState, ESHSessionState::MainMenu);
}

// =======================================================================
//  Pause management
// =======================================================================

void USHGameSessionManager::PauseGame()
{
	if (CurrentState != ESHSessionState::InMission)
	{
		UE_LOG(LogSH_Session, Verbose,
			TEXT("[SHGameSessionManager] PauseGame: Not in mission (state: %d). Ignoring."),
			static_cast<int32>(CurrentState));
		return;
	}

	StateBeforePause = CurrentState;

	UWorld* World = GetWorld();
	if (World)
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->SetPause(true);
		}
	}

	TransitionTo(ESHSessionState::Paused);
}

void USHGameSessionManager::ResumeGame()
{
	if (CurrentState != ESHSessionState::Paused)
	{
		UE_LOG(LogSH_Session, Verbose,
			TEXT("[SHGameSessionManager] ResumeGame: Not paused (state: %d). Ignoring."),
			static_cast<int32>(CurrentState));
		return;
	}

	UWorld* World = GetWorld();
	if (World)
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			PC->SetPause(false);
		}
	}

	TransitionTo(StateBeforePause);
}

// =======================================================================
//  Widget management
// =======================================================================

void USHGameSessionManager::SetWidgetStack(UCommonActivatableWidgetContainerBase* InWidgetStack)
{
	WidgetStack = InWidgetStack;

	UE_LOG(LogSH_Session, Log,
		TEXT("[SHGameSessionManager] Widget stack set: %s"),
		InWidgetStack ? *InWidgetStack->GetName() : TEXT("null"));
}

// =======================================================================
//  State transition validation
// =======================================================================

bool USHGameSessionManager::IsTransitionValid(ESHSessionState From, ESHSessionState To) const
{
	// Define the valid transition graph. Each state lists which states
	// it can transition to.
	switch (From)
	{
	case ESHSessionState::MainMenu:
		return To == ESHSessionState::MissionSelect
			|| To == ESHSessionState::LoadoutScreen
			|| To == ESHSessionState::Briefing
			|| To == ESHSessionState::CoopLobby;

	case ESHSessionState::MissionSelect:
		return To == ESHSessionState::LoadoutScreen
			|| To == ESHSessionState::Briefing
			|| To == ESHSessionState::MainMenu
			|| To == ESHSessionState::CoopLobby;

	case ESHSessionState::LoadoutScreen:
		return To == ESHSessionState::Briefing
			|| To == ESHSessionState::MissionSelect
			|| To == ESHSessionState::MainMenu;

	case ESHSessionState::Briefing:
		return To == ESHSessionState::Loading
			|| To == ESHSessionState::LoadoutScreen
			|| To == ESHSessionState::MainMenu;

	case ESHSessionState::Loading:
		return To == ESHSessionState::InMission
			|| To == ESHSessionState::MainMenu; // Abort during load

	case ESHSessionState::InMission:
		return To == ESHSessionState::Paused
			|| To == ESHSessionState::AfterAction
			|| To == ESHSessionState::Loading; // Restart

	case ESHSessionState::Paused:
		return To == ESHSessionState::InMission     // Resume
			|| To == ESHSessionState::Loading       // Restart
			|| To == ESHSessionState::AfterAction;  // Mission end while paused

	case ESHSessionState::AfterAction:
		return To == ESHSessionState::MainMenu
			|| To == ESHSessionState::MissionSelect
			|| To == ESHSessionState::Briefing; // Next mission in campaign

	case ESHSessionState::CoopLobby:
		return To == ESHSessionState::LoadoutScreen
			|| To == ESHSessionState::Briefing
			|| To == ESHSessionState::MainMenu;

	default:
		return false;
	}
}

// =======================================================================
//  State exit / enter
// =======================================================================

void USHGameSessionManager::ExitState(ESHSessionState State)
{
	UE_LOG(LogSH_Session, Verbose,
		TEXT("[SHGameSessionManager] Exiting state %d."), static_cast<int32>(State));

	switch (State)
	{
	case ESHSessionState::MainMenu:
	case ESHSessionState::MissionSelect:
	case ESHSessionState::LoadoutScreen:
	case ESHSessionState::Briefing:
	case ESHSessionState::AfterAction:
	case ESHSessionState::CoopLobby:
		ClearActiveWidget();
		break;

	case ESHSessionState::Loading:
		ClearActiveWidget();
		break;

	case ESHSessionState::InMission:
		// Nothing to tear down — gameplay systems are map-resident.
		break;

	case ESHSessionState::Paused:
		ClearActiveWidget();
		break;

	default:
		break;
	}
}

void USHGameSessionManager::EnterState(ESHSessionState State)
{
	UE_LOG(LogSH_Session, Verbose,
		TEXT("[SHGameSessionManager] Entering state %d."), static_cast<int32>(State));

	switch (State)
	{
	case ESHSessionState::MainMenu:
		if (MainMenuWidgetClass)
		{
			ActiveWidget = PushWidget(MainMenuWidgetClass);
		}
		break;

	case ESHSessionState::MissionSelect:
		// MissionSelect widget class would be configured similarly.
		// For now, log that we're in the state.
		UE_LOG(LogSH_Session, Log,
			TEXT("[SHGameSessionManager] Entered MissionSelect. Awaiting mission config from UI."));
		break;

	case ESHSessionState::LoadoutScreen:
		if (LoadoutWidgetClass)
		{
			ActiveWidget = PushWidget(LoadoutWidgetClass);
		}
		break;

	case ESHSessionState::Briefing:
		// The briefing system is an actor component that lives in the level.
		// We log the state entry; the briefing widget/system will be driven
		// by the level blueprint or a dedicated briefing level.
		UE_LOG(LogSH_Session, Log,
			TEXT("[SHGameSessionManager] Entered Briefing for mission '%s'."),
			*CurrentMissionConfig.MissionId.ToString());
		break;

	case ESHSessionState::Loading:
	{
		if (LoadingScreenWidgetClass)
		{
			USHLoadingScreenWidget* LoadingWidget = Cast<USHLoadingScreenWidget>(
				PushWidget(LoadingScreenWidgetClass));

			if (LoadingWidget)
			{
				LoadingWidget->SetMissionInfo(
					CurrentMissionConfig.MissionName,
					FText::FromString(FString::Printf(
						TEXT("Loading %s..."), *CurrentMissionConfig.MapName)));
				LoadingWidget->SetLoadingProgress(0.f);
			}

			ActiveWidget = LoadingWidget;
		}

		// Initiate level travel.
		if (CurrentMissionConfig.IsValid())
		{
			// Build the travel URL with game mode override if specified.
			FString TravelURL = CurrentMissionConfig.MapName;
			if (CurrentMissionConfig.GameModeClass)
			{
				TravelURL += FString::Printf(TEXT("?game=%s"),
					*CurrentMissionConfig.GameModeClass->GetPathName());
			}

			TravelToMap(TravelURL);
		}
		break;
	}

	case ESHSessionState::InMission:
		UE_LOG(LogSH_Session, Log,
			TEXT("[SHGameSessionManager] Entered InMission. Gameplay active."));
		MissionElapsedTime = 0.f;
		break;

	case ESHSessionState::Paused:
		if (PauseMenuWidgetClass)
		{
			ActiveWidget = PushWidget(PauseMenuWidgetClass);
		}
		break;

	case ESHSessionState::AfterAction:
		if (AfterActionWidgetClass)
		{
			ActiveWidget = PushWidget(AfterActionWidgetClass);
			// The caller (EndMission) will populate the widget after this call.
		}
		break;

	case ESHSessionState::CoopLobby:
		UE_LOG(LogSH_Session, Log,
			TEXT("[SHGameSessionManager] Entered CoopLobby. Awaiting players."));
		break;

	default:
		break;
	}
}

// =======================================================================
//  Widget helpers
// =======================================================================

UCommonActivatableWidget* USHGameSessionManager::PushWidget(TSubclassOf<UCommonActivatableWidget> WidgetClass)
{
	if (!WidgetClass)
	{
		UE_LOG(LogSH_Session, Warning,
			TEXT("[SHGameSessionManager] PushWidget: Null widget class."));
		return nullptr;
	}

	if (!WidgetStack)
	{
		UE_LOG(LogSH_Session, Warning,
			TEXT("[SHGameSessionManager] PushWidget: No widget stack set. Call SetWidgetStack first."));
		return nullptr;
	}

	UCommonActivatableWidget* Widget = WidgetStack->AddWidget(*WidgetClass);
	if (Widget)
	{
		UE_LOG(LogSH_Session, Verbose,
			TEXT("[SHGameSessionManager] Pushed widget: %s"),
			*WidgetClass->GetName());
	}
	else
	{
		UE_LOG(LogSH_Session, Warning,
			TEXT("[SHGameSessionManager] Failed to push widget: %s"),
			*WidgetClass->GetName());
	}

	return Widget;
}

void USHGameSessionManager::ClearActiveWidget()
{
	if (ActiveWidget)
	{
		ActiveWidget->DeactivateWidget();
		ActiveWidget = nullptr;
	}
}

// =======================================================================
//  Mission helpers
// =======================================================================

void USHGameSessionManager::TravelToMap(const FString& MapPath)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogSH_Session, Error,
			TEXT("[SHGameSessionManager] TravelToMap: No world context."));
		return;
	}

	UE_LOG(LogSH_Session, Log,
		TEXT("[SHGameSessionManager] Travelling to map: %s"), *MapPath);

	UGameplayStatics::OpenLevel(World, *MapPath, true);
}

void USHGameSessionManager::PerformInterMissionCleanup()
{
	UE_LOG(LogSH_Session, Log, TEXT("[SHGameSessionManager] Performing inter-mission cleanup."));

	ClearActiveWidget();

	// Request garbage collection to free assets from the previous level.
	if (GEngine)
	{
		GEngine->ForceGarbageCollection(true);
	}

	// Reset mission-specific runtime state.
	MissionElapsedTime = 0.f;
	bLastMissionVictory = false;

	UE_LOG(LogSH_Session, Verbose, TEXT("[SHGameSessionManager] Inter-mission cleanup complete."));
}

FSHXPBreakdown USHGameSessionManager::ComputeXPAward(const FSHEngagementRecord& Record, bool bVictory) const
{
	FSHXPBreakdown Breakdown;

	const float Multiplier = GetDifficultyMultiplier();

	// Combat XP: kills, assists, vehicle kills.
	Breakdown.CombatXP = FMath::RoundToInt32(
		static_cast<float>(
			Record.Kills * XPPerKill
			+ Record.Assists * XPPerAssist
			+ Record.VehicleKills * XPPerVehicleKill
		) * Multiplier);

	// Objective XP.
	Breakdown.ObjectiveXP = FMath::RoundToInt32(
		static_cast<float>(Record.ObjectivesCompleted * XPPerObjective) * Multiplier);

	// Teamwork XP: revives and squad order completion.
	const int32 RawTeamwork = Record.Revives * XPPerRevive
		+ Record.SquadOrdersCompleted * 30; // 30 XP per completed squad order
	Breakdown.TeamworkXP = FMath::RoundToInt32(static_cast<float>(RawTeamwork) * Multiplier);

	// Accuracy bonus.
	const float Accuracy = Record.GetAccuracy();
	if (Accuracy >= AccuracyBonusThreshold && Record.ShotsFired > 10)
	{
		Breakdown.AccuracyBonusXP = FMath::RoundToInt32(
			static_cast<float>(AccuracyBonusXP) * Multiplier);
	}

	// Survival / victory bonus.
	if (bVictory)
	{
		Breakdown.SurvivalBonusXP = FMath::RoundToInt32(
			static_cast<float>(XPVictoryBonus) * Multiplier);
	}

	UE_LOG(LogSH_Session, Log,
		TEXT("[SHGameSessionManager] XP Breakdown — Combat: %d, Objective: %d, Teamwork: %d, Accuracy: %d, Survival: %d, Total: %d (Multiplier: %.2f)"),
		Breakdown.CombatXP, Breakdown.ObjectiveXP, Breakdown.TeamworkXP,
		Breakdown.AccuracyBonusXP, Breakdown.SurvivalBonusXP,
		Breakdown.GetTotal(), Multiplier);

	return Breakdown;
}

float USHGameSessionManager::GetDifficultyMultiplier() const
{
	const int32 Difficulty = CurrentMissionConfig.DifficultyLevel;
	if (DifficultyXPMultipliers.IsValidIndex(Difficulty))
	{
		return DifficultyXPMultipliers[Difficulty];
	}

	// Fallback: return the last defined multiplier or 1.0.
	if (DifficultyXPMultipliers.Num() > 0)
	{
		return DifficultyXPMultipliers.Last();
	}

	return 1.0f;
}

FString USHGameSessionManager::GetSaveSlotName(int32 SlotIndex) const
{
	return FString::Printf(TEXT("%s_Slot%d"), *CampaignSavePrefix, SlotIndex);
}

USHProgressionSystem* USHGameSessionManager::GetProgressionSystem() const
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		return GI->GetSubsystem<USHProgressionSystem>();
	}
	return nullptr;
}
