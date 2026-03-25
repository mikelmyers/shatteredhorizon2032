// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHMissionScriptRunner.h"

#include "ShatteredHorizon2032/Narrative/SHDialogueSystem.h"
#include "ShatteredHorizon2032/World/SHWeatherSystem.h"
#include "ShatteredHorizon2032/EW/SHElectronicWarfare.h"
#include "ShatteredHorizon2032/Combat/SHCallForFireSystem.h"
#include "ShatteredHorizon2032/Drones/SHDroneBase.h"
#include "ShatteredHorizon2032/AI/SHEnemyCharacter.h"
#include "ShatteredHorizon2032/Core/SHGameSessionManager.h"

#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/GameModeBase.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"

DEFINE_LOG_CATEGORY(LogSH_MissionScript);

// =====================================================================
//  Construction
// =====================================================================

USHMissionScriptRunner::USHMissionScriptRunner()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

// =====================================================================
//  Lifecycle
// =====================================================================

void USHMissionScriptRunner::BeginPlay()
{
	Super::BeginPlay();
	CacheSubsystems();
}

void USHMissionScriptRunner::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bMissionRunning || bMissionEnded)
	{
		return;
	}

	MissionElapsedTime += DeltaTime;
	PhaseElapsedTime += DeltaTime;

	TickPendingWaves(DeltaTime);
	TickPendingEvents(DeltaTime);
	TickPendingDialogue(DeltaTime);
	TickObjectiveMonitoring();
	TickAdvanceConditions(DeltaTime);
}

void USHMissionScriptRunner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	bMissionRunning = false;
	bMissionEnded = true;

	Super::EndPlay(EndPlayReason);
}

// =====================================================================
//  Mission lifecycle
// =====================================================================

void USHMissionScriptRunner::LoadMission(USHMissionDefinition* Definition)
{
	if (!Definition)
	{
		UE_LOG(LogSH_MissionScript, Error, TEXT("LoadMission called with null definition."));
		return;
	}

	if (Definition->Phases.Num() == 0)
	{
		UE_LOG(LogSH_MissionScript, Error, TEXT("LoadMission: Definition '%s' has zero phases."),
			*Definition->MissionName.ToString());
		return;
	}

	ActiveMission = Definition;
	CurrentPhaseIndex = 0;
	PhaseElapsedTime = 0.f;
	MissionElapsedTime = 0.f;
	bMissionRunning = false;
	bMissionEnded = false;

	PendingWaves.Empty();
	PendingEvents.Empty();
	PendingDialogue.Empty();
	ActiveObjectiveIds.Empty();

	UE_LOG(LogSH_MissionScript, Log, TEXT("Mission loaded: '%s' with %d phases."),
		*Definition->MissionName.ToString(), Definition->Phases.Num());
}

void USHMissionScriptRunner::StartMission()
{
	if (!ActiveMission)
	{
		UE_LOG(LogSH_MissionScript, Error, TEXT("StartMission called but no mission is loaded."));
		return;
	}

	if (bMissionRunning)
	{
		UE_LOG(LogSH_MissionScript, Warning, TEXT("StartMission: Mission is already running."));
		return;
	}

	CacheSubsystems();

	bMissionRunning = true;
	bMissionEnded = false;
	CurrentPhaseIndex = 0;
	PhaseElapsedTime = 0.f;
	MissionElapsedTime = 0.f;

	SetComponentTickEnabled(true);

	UE_LOG(LogSH_MissionScript, Log, TEXT("Mission started: '%s'"),
		*ActiveMission->MissionName.ToString());

	EnterCurrentPhase();
}

void USHMissionScriptRunner::AdvancePhase()
{
	if (!bMissionRunning || bMissionEnded || !ActiveMission)
	{
		return;
	}

	ExitCurrentPhase();

	CurrentPhaseIndex++;

	// Check if mission is complete (no more phases).
	if (CurrentPhaseIndex >= ActiveMission->Phases.Num())
	{
		CompleteMission(true);
		return;
	}

	PhaseElapsedTime = 0.f;
	EnterCurrentPhase();
}

// =====================================================================
//  Queries
// =====================================================================

ESHMissionPhase USHMissionScriptRunner::GetCurrentPhase() const
{
	if (ActiveMission && ActiveMission->Phases.IsValidIndex(CurrentPhaseIndex))
	{
		return ActiveMission->Phases[CurrentPhaseIndex].Phase;
	}
	return ESHMissionPhase::PreInvasion;
}

// =====================================================================
//  Phase management
// =====================================================================

void USHMissionScriptRunner::EnterCurrentPhase()
{
	if (!ActiveMission || !ActiveMission->Phases.IsValidIndex(CurrentPhaseIndex))
	{
		UE_LOG(LogSH_MissionScript, Error, TEXT("EnterCurrentPhase: Invalid phase index %d."), CurrentPhaseIndex);
		return;
	}

	const FSHPhaseDefinition& PhaseDef = ActiveMission->Phases[CurrentPhaseIndex];

	UE_LOG(LogSH_MissionScript, Log, TEXT("Entering phase %d: %s (duration: %.1fs)"),
		CurrentPhaseIndex, *UEnum::GetDisplayValueAsText(PhaseDef.Phase).ToString(),
		PhaseDef.PhaseDuration);

	// Populate pending queues from the phase definition, sorted by delay ascending.
	PendingWaves = PhaseDef.Waves;
	PendingWaves.Sort([](const FSHWaveDefinition& A, const FSHWaveDefinition& B)
	{
		return A.DelayFromPhaseStart < B.DelayFromPhaseStart;
	});

	PendingEvents = PhaseDef.ScriptedEvents;
	PendingEvents.Sort([](const FSHScriptedEventDefinition& A, const FSHScriptedEventDefinition& B)
	{
		return A.DelayFromPhaseStart < B.DelayFromPhaseStart;
	});

	PendingDialogue = PhaseDef.DialogueTriggers;
	PendingDialogue.Sort([](const FSHDialogueTrigger& A, const FSHDialogueTrigger& B)
	{
		return A.DelayFromPhaseStart < B.DelayFromPhaseStart;
	});

	// Activate phase objectives.
	ActivatePhaseObjectives(PhaseDef);

	// Trigger initial weather transition if configured.
	if (PhaseDef.InitialWeatherType != 255)
	{
		USHWeatherSystem* WeatherSys = GetWeatherSystem();
		if (WeatherSys)
		{
			const ESHWeatherType TargetWeather = static_cast<ESHWeatherType>(PhaseDef.InitialWeatherType);
			WeatherSys->TransitionWeather(TargetWeather, PhaseDef.WeatherTransitionDuration);

			UE_LOG(LogSH_MissionScript, Log, TEXT("Phase weather transition -> %s over %.1fs"),
				*UEnum::GetDisplayValueAsText(TargetWeather).ToString(),
				PhaseDef.WeatherTransitionDuration);
		}
	}

	// Notify the game mode of the phase change.
	ASHGameMode* GM = GetSHGameMode();
	if (GM)
	{
		GM->AdvanceToPhase(PhaseDef.Phase);
	}

	// Broadcast delegate.
	OnPhaseAdvanced.Broadcast(PhaseDef.Phase);
}

void USHMissionScriptRunner::ExitCurrentPhase()
{
	DeactivatePhaseObjectives();

	PendingWaves.Empty();
	PendingEvents.Empty();
	PendingDialogue.Empty();
}

// =====================================================================
//  Tick sub-routines
// =====================================================================

void USHMissionScriptRunner::TickAdvanceConditions(float DeltaTime)
{
	if (!ActiveMission || !ActiveMission->Phases.IsValidIndex(CurrentPhaseIndex))
	{
		return;
	}

	const FSHPhaseDefinition& PhaseDef = ActiveMission->Phases[CurrentPhaseIndex];

	// If there are no advance conditions, never auto-advance.
	if (PhaseDef.AdvanceConditions.Num() == 0)
	{
		return;
	}

	// Any single satisfied condition triggers advance.
	for (const FSHPhaseAdvanceRule& Rule : PhaseDef.AdvanceConditions)
	{
		bool bConditionMet = false;

		switch (Rule.Condition)
		{
		case ESHPhaseAdvanceCondition::AllObjectivesComplete:
			bConditionMet = EvaluateAllObjectivesComplete();
			break;

		case ESHPhaseAdvanceCondition::TimerExpired:
			bConditionMet = EvaluateTimerExpired();
			break;

		case ESHPhaseAdvanceCondition::EnemyBreachesLine:
			bConditionMet = EvaluateEnemyBreachesLine(Rule);
			break;

		case ESHPhaseAdvanceCondition::FriendlyForcesOverrun:
			bConditionMet = EvaluateFriendlyForcesOverrun(Rule);
			break;
		}

		if (bConditionMet)
		{
			UE_LOG(LogSH_MissionScript, Log,
				TEXT("Phase advance condition met: %s (Phase %d)"),
				*UEnum::GetDisplayValueAsText(Rule.Condition).ToString(),
				CurrentPhaseIndex);

			AdvancePhase();
			return; // AdvancePhase may change state; bail out of the loop.
		}
	}
}

void USHMissionScriptRunner::TickPendingWaves(float DeltaTime)
{
	// Process from the front — list is sorted by delay ascending.
	while (PendingWaves.Num() > 0)
	{
		const FSHWaveDefinition& NextWave = PendingWaves[0];
		if (PhaseElapsedTime < NextWave.DelayFromPhaseStart)
		{
			break; // Not yet time for this wave.
		}

		FSHWaveDefinition WaveToSpawn = PendingWaves[0];
		PendingWaves.RemoveAt(0, EAllowShrinking::No);

		SpawnWave(WaveToSpawn);
	}
}

void USHMissionScriptRunner::TickPendingEvents(float DeltaTime)
{
	while (PendingEvents.Num() > 0)
	{
		const FSHScriptedEventDefinition& NextEvent = PendingEvents[0];
		if (PhaseElapsedTime < NextEvent.DelayFromPhaseStart)
		{
			break;
		}

		FSHScriptedEventDefinition EventToRun = PendingEvents[0];
		PendingEvents.RemoveAt(0, EAllowShrinking::No);

		ExecuteScriptedEvent(EventToRun);
	}
}

void USHMissionScriptRunner::TickPendingDialogue(float DeltaTime)
{
	while (PendingDialogue.Num() > 0)
	{
		const FSHDialogueTrigger& NextLine = PendingDialogue[0];
		if (PhaseElapsedTime < NextLine.DelayFromPhaseStart)
		{
			break;
		}

		FSHDialogueTrigger DialogueToPlay = PendingDialogue[0];
		PendingDialogue.RemoveAt(0, EAllowShrinking::No);

		USHDialogueSystem* DialogueSys = GetDialogueSystem();
		if (DialogueSys)
		{
			FSHDialogueLine Line;
			Line.Speaker = DialogueToPlay.Speaker;
			Line.Line = DialogueToPlay.Line;
			Line.AudioCue = DialogueToPlay.AudioCue;
			Line.Duration = DialogueToPlay.Duration;
			Line.Priority = DialogueToPlay.Priority;

			DialogueSys->PlayDialogueLine(Line);

			UE_LOG(LogSH_MissionScript, Verbose, TEXT("Dialogue triggered: %s - \"%s\""),
				*DialogueToPlay.Speaker.ToString(),
				*DialogueToPlay.Line.ToString());
		}
	}
}

void USHMissionScriptRunner::TickObjectiveMonitoring()
{
	USHObjectiveSystem* ObjSys = GetObjectiveSystem();
	if (!ObjSys)
	{
		return;
	}

	for (const FGuid& ObjId : ActiveObjectiveIds)
	{
		const FSHObjective* Obj = ObjSys->GetObjective(ObjId);
		if (!Obj)
		{
			continue;
		}

		// Critical objective failure ends the mission.
		if (Obj->Status == ESHObjectiveStatus::Failed && Obj->Priority == ESHObjectivePriority::Primary)
		{
			FText Reason = FText::Format(
				NSLOCTEXT("SHMission", "CritObjFailed", "Critical objective failed: {0}"),
				Obj->DisplayName);

			FailMission(Reason);
			return;
		}
	}
}

// =====================================================================
//  Advance condition evaluators
// =====================================================================

bool USHMissionScriptRunner::EvaluateAllObjectivesComplete() const
{
	USHObjectiveSystem* ObjSys = GetObjectiveSystem();
	if (!ObjSys)
	{
		return false;
	}

	if (ActiveObjectiveIds.Num() == 0)
	{
		return false;
	}

	// All Primary objectives in the current phase must be completed.
	for (const FGuid& ObjId : ActiveObjectiveIds)
	{
		const FSHObjective* Obj = ObjSys->GetObjective(ObjId);
		if (!Obj)
		{
			continue;
		}

		if (Obj->Priority == ESHObjectivePriority::Primary &&
			Obj->Status != ESHObjectiveStatus::Completed)
		{
			return false;
		}
	}

	return true;
}

bool USHMissionScriptRunner::EvaluateTimerExpired() const
{
	if (!ActiveMission || !ActiveMission->Phases.IsValidIndex(CurrentPhaseIndex))
	{
		return false;
	}

	const float Duration = ActiveMission->Phases[CurrentPhaseIndex].PhaseDuration;
	if (Duration <= 0.f)
	{
		return false; // No time limit.
	}

	return PhaseElapsedTime >= Duration;
}

bool USHMissionScriptRunner::EvaluateEnemyBreachesLine(const FSHPhaseAdvanceRule& Rule) const
{
	// Determine zone center — use the rule's zone tag to find a tagged actor,
	// or fall back to the configured default defensive zone center.
	FVector ZoneCenter = DefensiveZoneCenter;
	float ZoneRadius = DefensiveZoneRadius;

	if (!Rule.ZoneTag.IsNone())
	{
		UWorld* World = GetWorld();
		if (World)
		{
			TArray<AActor*> TaggedActors;
			UGameplayStatics::GetAllActorsWithTag(World, Rule.ZoneTag, TaggedActors);
			if (TaggedActors.Num() > 0)
			{
				ZoneCenter = TaggedActors[0]->GetActorLocation();
			}
		}
	}

	const int32 EnemyCount = CountEnemiesInRadius(ZoneCenter, ZoneRadius);

	// Ratio of enemies relative to initial friendly count.
	if (InitialFriendlyCount <= 0)
	{
		return false;
	}

	const float Ratio = static_cast<float>(EnemyCount) / static_cast<float>(InitialFriendlyCount);
	return Ratio >= Rule.Threshold;
}

bool USHMissionScriptRunner::EvaluateFriendlyForcesOverrun(const FSHPhaseAdvanceRule& Rule) const
{
	if (InitialFriendlyCount <= 0)
	{
		return false;
	}

	const int32 AliveCount = CountFriendliesAlive();
	const float CasualtyRatio = 1.f - (static_cast<float>(AliveCount) / static_cast<float>(InitialFriendlyCount));
	return CasualtyRatio >= Rule.Threshold;
}

// =====================================================================
//  Event execution
// =====================================================================

void USHMissionScriptRunner::ExecuteScriptedEvent(const FSHScriptedEventDefinition& Event)
{
	UE_LOG(LogSH_MissionScript, Log, TEXT("Executing scripted event: %s (type: %s)"),
		*Event.EventId.ToString(),
		*UEnum::GetDisplayValueAsText(Event.EventType).ToString());

	switch (Event.EventType)
	{
	case ESHScriptedEventType::ArtilleryBarrage:
		ExecuteArtilleryBarrage(Event);
		break;

	case ESHScriptedEventType::DroneSwarm:
		ExecuteDroneSwarm(Event);
		break;

	case ESHScriptedEventType::EWJammingZone:
		ExecuteEWJammingZone(Event);
		break;

	case ESHScriptedEventType::WeatherChange:
		ExecuteWeatherChange(Event);
		break;

	case ESHScriptedEventType::Dialogue:
		ExecuteDialogue(Event);
		break;

	case ESHScriptedEventType::AirStrike:
		ExecuteAirStrike(Event);
		break;
	}

	OnScriptedEventTriggered.Broadcast(Event.EventId);
}

void USHMissionScriptRunner::ExecuteArtilleryBarrage(const FSHScriptedEventDefinition& Event)
{
	// Try to use the CallForFire system on the game mode first.
	ASHGameMode* GM = GetSHGameMode();
	if (GM)
	{
		USHCallForFireSystem* CFF = GM->FindComponentByClass<USHCallForFireSystem>();
		if (CFF)
		{
			// Determine ordnance type based on intensity.
			const ESHOrdnanceType OrdnanceType = (Event.Intensity >= 0.7f)
				? ESHOrdnanceType::Artillery155mm
				: ESHOrdnanceType::Artillery105mm;

			CFF->RequestFireMission(OrdnanceType, Event.Location, Event.Count);

			UE_LOG(LogSH_MissionScript, Log,
				TEXT("Artillery barrage: %d rounds at [%.0f, %.0f, %.0f]"),
				Event.Count, Event.Location.X, Event.Location.Y, Event.Location.Z);
			return;
		}
	}

	// Fallback: spawn explosion sequences directly via radial damage.
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UE_LOG(LogSH_MissionScript, Log,
		TEXT("Artillery barrage (fallback): %d impacts around [%.0f, %.0f, %.0f]"),
		Event.Count, Event.Location.X, Event.Location.Y, Event.Location.Z);

	for (int32 i = 0; i < Event.Count; ++i)
	{
		const float RoundDelay = static_cast<float>(i) * (Event.Duration / FMath::Max(1, Event.Count));

		FTimerHandle TimerHandle;
		FVector ImpactLocation = Event.Location;

		// Apply random dispersion within the radius.
		const FVector2D Offset2D = FMath::RandPointInCircle(Event.Radius);
		ImpactLocation.X += Offset2D.X;
		ImpactLocation.Y += Offset2D.Y;

		FTimerDelegate TimerDelegate;
		TimerDelegate.BindWeakLambda(this, [this, ImpactLocation]()
		{
			UWorld* CurrentWorld = GetWorld();
			if (!CurrentWorld)
			{
				return;
			}

			UGameplayStatics::ApplyRadialDamage(
				CurrentWorld,
				500.f,           // BaseDamage
				ImpactLocation,
				2000.f,          // DamageRadius
				nullptr,         // DamageTypeClass
				TArray<AActor*>(), // IgnoreActors
				GetOwner(),      // DamageCauser
				nullptr,         // InstigatedByController
				true,            // bDoFullDamage
				ECC_Visibility);
		});

		World->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, RoundDelay, false);
	}
}

void USHMissionScriptRunner::ExecuteDroneSwarm(const FSHScriptedEventDefinition& Event)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Resolve the drone class, falling back to default if not specified.
	UClass* DroneClassToSpawn = nullptr;
	if (!Event.DroneClass.IsNull())
	{
		DroneClassToSpawn = Event.DroneClass.LoadSynchronous();
	}

	if (!DroneClassToSpawn)
	{
		UE_LOG(LogSH_MissionScript, Warning,
			TEXT("DroneSwarm event '%s': No drone class specified or load failed. Skipping."),
			*Event.EventId.ToString());
		return;
	}

	UE_LOG(LogSH_MissionScript, Log, TEXT("Spawning drone swarm: %d drones of class %s"),
		Event.Count, *DroneClassToSpawn->GetName());

	for (int32 i = 0; i < Event.Count; ++i)
	{
		// Spread drones in a circle around the target location.
		const float Angle = (2.f * PI * static_cast<float>(i)) / FMath::Max(1, Event.Count);
		FVector SpawnOffset(
			FMath::Cos(Angle) * Event.Radius * 0.5f,
			FMath::Sin(Angle) * Event.Radius * 0.5f,
			500.f); // Altitude offset.

		FVector SpawnLocation = Event.Location + SpawnOffset;
		FRotator SpawnRotation = FRotator::ZeroRotator;

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		ASHDroneBase* SpawnedDrone = World->SpawnActor<ASHDroneBase>(
			DroneClassToSpawn, SpawnLocation, SpawnRotation, SpawnParams);

		if (SpawnedDrone)
		{
			SpawnedDrone->Affiliation = Event.bFriendly
				? ESHDroneAffiliation::Friendly
				: ESHDroneAffiliation::Enemy;

			SpawnedDrone->LaunchDrone(SpawnLocation, SpawnRotation);
			SpawnedDrone->SetFlightTarget(Event.Location);
		}
	}
}

void USHMissionScriptRunner::ExecuteEWJammingZone(const FSHScriptedEventDefinition& Event)
{
	ASHElectronicWarfare* EW = GetEWManager();
	if (!EW)
	{
		UE_LOG(LogSH_MissionScript, Warning,
			TEXT("EWJammingZone event '%s': No EW manager found."), *Event.EventId.ToString());
		return;
	}

	const ESHEWEffectType EffectType = ESHEWEffectType::CommsJamming;

	EW->CreateEWZone(
		Event.Location,
		Event.Radius,
		Event.Intensity,
		EffectType,
		Event.bFriendly,
		Event.Duration);

	UE_LOG(LogSH_MissionScript, Log,
		TEXT("EW Jamming Zone created: center [%.0f, %.0f, %.0f], radius %.0f, strength %.2f, duration %.1fs"),
		Event.Location.X, Event.Location.Y, Event.Location.Z,
		Event.Radius, Event.Intensity, Event.Duration);
}

void USHMissionScriptRunner::ExecuteWeatherChange(const FSHScriptedEventDefinition& Event)
{
	USHWeatherSystem* WeatherSys = GetWeatherSystem();
	if (!WeatherSys)
	{
		UE_LOG(LogSH_MissionScript, Warning,
			TEXT("WeatherChange event '%s': No weather system found."), *Event.EventId.ToString());
		return;
	}

	const ESHWeatherType TargetWeather = static_cast<ESHWeatherType>(Event.WeatherTypeValue);
	WeatherSys->TransitionWeather(TargetWeather, Event.Duration);

	UE_LOG(LogSH_MissionScript, Log, TEXT("Weather transition -> %s over %.1fs"),
		*UEnum::GetDisplayValueAsText(TargetWeather).ToString(), Event.Duration);
}

void USHMissionScriptRunner::ExecuteDialogue(const FSHScriptedEventDefinition& Event)
{
	// A dialogue event embedded as a scripted event (not using the dedicated
	// dialogue trigger queue). This allows dialogue to be placed at arbitrary
	// points in the event timeline.
	USHDialogueSystem* DialogueSys = GetDialogueSystem();
	if (!DialogueSys)
	{
		return;
	}

	FSHDialogueLine Line;
	Line.Speaker = FText::FromName(Event.EventId); // Use EventId as fallback speaker.
	Line.Line = FText::GetEmpty();
	Line.Duration = Event.Duration;
	Line.Priority = static_cast<int32>(Event.Intensity * 200.f);

	DialogueSys->PlayDialogueLine(Line);
}

void USHMissionScriptRunner::ExecuteAirStrike(const FSHScriptedEventDefinition& Event)
{
	// Air strikes are executed as a high-intensity artillery barrage with
	// faster interval and larger blast radius.
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	UE_LOG(LogSH_MissionScript, Log,
		TEXT("Air strike: %d impacts at [%.0f, %.0f, %.0f] over %.1fs"),
		Event.Count, Event.Location.X, Event.Location.Y, Event.Location.Z, Event.Duration);

	for (int32 i = 0; i < Event.Count; ++i)
	{
		const float RoundDelay = static_cast<float>(i) * (Event.Duration / FMath::Max(1, Event.Count));

		FVector ImpactLocation = Event.Location;
		const FVector2D Offset2D = FMath::RandPointInCircle(Event.Radius * 0.7f);
		ImpactLocation.X += Offset2D.X;
		ImpactLocation.Y += Offset2D.Y;

		FTimerHandle TimerHandle;
		FTimerDelegate TimerDelegate;
		TimerDelegate.BindWeakLambda(this, [this, ImpactLocation]()
		{
			UWorld* CurrentWorld = GetWorld();
			if (!CurrentWorld)
			{
				return;
			}

			UGameplayStatics::ApplyRadialDamage(
				CurrentWorld,
				800.f,           // BaseDamage — higher than artillery.
				ImpactLocation,
				3000.f,          // DamageRadius — wider than artillery.
				nullptr,
				TArray<AActor*>(),
				GetOwner(),
				nullptr,
				true,
				ECC_Visibility);
		});

		World->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, RoundDelay, false);
	}
}

// =====================================================================
//  Wave spawning
// =====================================================================

void USHMissionScriptRunner::SpawnWave(const FSHWaveDefinition& WaveDef)
{
	ASHGameMode* GM = GetSHGameMode();
	if (!GM)
	{
		UE_LOG(LogSH_MissionScript, Warning,
			TEXT("SpawnWave: No valid game mode. Cannot spawn reinforcement wave."));
		return;
	}

	// Convert FSHWaveDefinition → FSHReinforcementWave.
	FSHReinforcementWave ReinfWave;
	ReinfWave.DelayFromPhaseStart = 0.f; // Already handled by the script runner timing.
	ReinfWave.InfantryCount = WaveDef.InfantryCount;
	ReinfWave.ArmorCount = WaveDef.ArmorCount;
	ReinfWave.AmphibiousCount = WaveDef.AmphibiousCount;
	ReinfWave.bIncludesAirSupport = WaveDef.bIncludesAirSupport;
	ReinfWave.SpawnZoneTag = WaveDef.SpawnZoneTag;

	GM->TriggerReinforcementWave(ReinfWave);

	UE_LOG(LogSH_MissionScript, Log,
		TEXT("Wave spawned: %d infantry, %d armor, %d amphibious (zone: %s)"),
		WaveDef.InfantryCount, WaveDef.ArmorCount, WaveDef.AmphibiousCount,
		*WaveDef.SpawnZoneTag.ToString());
}

// =====================================================================
//  Objective management
// =====================================================================

void USHMissionScriptRunner::ActivatePhaseObjectives(const FSHPhaseDefinition& PhaseDef)
{
	USHObjectiveSystem* ObjSys = GetObjectiveSystem();
	if (!ObjSys)
	{
		UE_LOG(LogSH_MissionScript, Warning,
			TEXT("ActivatePhaseObjectives: No objective system available."));
		return;
	}

	ActiveObjectiveIds.Empty(PhaseDef.Objectives.Num());

	for (const FSHPhaseObjectiveDefinition& ObjDef : PhaseDef.Objectives)
	{
		FSHObjective NewObj;
		NewObj.ObjectiveId = FGuid::NewGuid();
		NewObj.DisplayName = ObjDef.DisplayName;
		NewObj.Description = ObjDef.Description;
		NewObj.Type = ObjDef.Type;
		NewObj.Priority = ObjDef.Priority;
		NewObj.Status = ESHObjectiveStatus::Inactive;
		NewObj.WorldLocation = ObjDef.WorldLocation;
		NewObj.AreaRadius = ObjDef.AreaRadius;
		NewObj.DefendDuration = ObjDef.DefendDuration;
		NewObj.TimeLimit = ObjDef.TimeLimit;
		NewObj.RequiredPresence = ObjDef.RequiredPresence;
		NewObj.Progress = 0.f;
		NewObj.ElapsedTime = 0.f;

		FGuid AddedId = ObjSys->AddObjective(NewObj);
		ObjSys->ActivateObjective(AddedId);

		ActiveObjectiveIds.Add(AddedId);

		UE_LOG(LogSH_MissionScript, Log, TEXT("Objective activated: '%s' (%s, %s)"),
			*ObjDef.DisplayName.ToString(),
			*UEnum::GetDisplayValueAsText(ObjDef.Type).ToString(),
			*UEnum::GetDisplayValueAsText(ObjDef.Priority).ToString());
	}
}

void USHMissionScriptRunner::DeactivatePhaseObjectives()
{
	USHObjectiveSystem* ObjSys = GetObjectiveSystem();

	if (ObjSys)
	{
		for (const FGuid& ObjId : ActiveObjectiveIds)
		{
			const FSHObjective* Obj = ObjSys->GetObjective(ObjId);
			if (Obj && Obj->Status == ESHObjectiveStatus::Active)
			{
				// Objectives that were not completed are expired rather than failed
				// (the phase itself moved on).
				// The objective system handles Expired via FailObjective for simplicity;
				// a dedicated ExpireObjective could be added later.
				ObjSys->FailObjective(ObjId);
			}
		}
	}

	ActiveObjectiveIds.Empty();
}

// =====================================================================
//  Completion
// =====================================================================

void USHMissionScriptRunner::CompleteMission(bool bVictory)
{
	if (bMissionEnded)
	{
		return;
	}

	bMissionEnded = true;
	bMissionRunning = false;
	SetComponentTickEnabled(false);

	ExitCurrentPhase();

	UE_LOG(LogSH_MissionScript, Log, TEXT("Mission %s: %s (elapsed: %.1fs)"),
		bVictory ? TEXT("COMPLETE") : TEXT("FAILED"),
		ActiveMission ? *ActiveMission->MissionName.ToString() : TEXT("Unknown"),
		MissionElapsedTime);

	OnMissionComplete.Broadcast(bVictory);

	// Notify the GameSessionManager.
	USHGameSessionManager* SessionMgr = GetSessionManager();
	if (SessionMgr)
	{
		SessionMgr->EndMission(bVictory);
	}
}

void USHMissionScriptRunner::FailMission(const FText& Reason)
{
	if (bMissionEnded)
	{
		return;
	}

	UE_LOG(LogSH_MissionScript, Warning, TEXT("Mission FAILED: %s"), *Reason.ToString());

	OnMissionFailed.Broadcast(Reason);
	CompleteMission(false);
}

// =====================================================================
//  System caching
// =====================================================================

void USHMissionScriptRunner::CacheSubsystems()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	CachedObjectiveSystem = World->GetSubsystem<USHObjectiveSystem>();
	CachedDialogueSystem = World->GetSubsystem<USHDialogueSystem>();
	CachedWeatherSystem = World->GetSubsystem<USHWeatherSystem>();
	CachedEWManager = ASHElectronicWarfare::GetEWManager(this);

	AActor* OwnerActor = GetOwner();
	CachedGameMode = Cast<ASHGameMode>(OwnerActor);
	if (!CachedGameMode.IsValid())
	{
		// Try to get it from the world if the owner is not the game mode.
		AGameModeBase* WorldGM = World->GetAuthGameMode();
		CachedGameMode = Cast<ASHGameMode>(WorldGM);
	}
}

USHObjectiveSystem* USHMissionScriptRunner::GetObjectiveSystem() const
{
	if (CachedObjectiveSystem)
	{
		return CachedObjectiveSystem;
	}

	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<USHObjectiveSystem>() : nullptr;
}

USHDialogueSystem* USHMissionScriptRunner::GetDialogueSystem() const
{
	if (CachedDialogueSystem)
	{
		return CachedDialogueSystem;
	}

	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<USHDialogueSystem>() : nullptr;
}

USHWeatherSystem* USHMissionScriptRunner::GetWeatherSystem() const
{
	if (CachedWeatherSystem)
	{
		return CachedWeatherSystem;
	}

	UWorld* World = GetWorld();
	return World ? World->GetSubsystem<USHWeatherSystem>() : nullptr;
}

ASHElectronicWarfare* USHMissionScriptRunner::GetEWManager() const
{
	if (CachedEWManager.IsValid())
	{
		return CachedEWManager.Get();
	}

	return ASHElectronicWarfare::GetEWManager(this);
}

ASHGameMode* USHMissionScriptRunner::GetSHGameMode() const
{
	if (CachedGameMode.IsValid())
	{
		return CachedGameMode.Get();
	}

	UWorld* World = GetWorld();
	if (World)
	{
		return Cast<ASHGameMode>(World->GetAuthGameMode());
	}
	return nullptr;
}

USHGameSessionManager* USHMissionScriptRunner::GetSessionManager() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	UGameInstance* GI = World->GetGameInstance();
	if (!GI)
	{
		return nullptr;
	}

	return GI->GetSubsystem<USHGameSessionManager>();
}

// =====================================================================
//  Helper — spatial queries
// =====================================================================

int32 USHMissionScriptRunner::CountEnemiesInRadius(const FVector& Center, float Radius) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	int32 Count = 0;
	const float RadiusSq = Radius * Radius;

	for (TActorIterator<ASHEnemyCharacter> It(World); It; ++It)
	{
		ASHEnemyCharacter* Enemy = *It;
		if (!Enemy || Enemy->IsPendingKillPending())
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(Enemy->GetActorLocation(), Center);
		if (DistSq <= RadiusSq)
		{
			++Count;
		}
	}

	return Count;
}

int32 USHMissionScriptRunner::CountFriendliesAlive() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	// Count all player-controlled pawns and squad AI members that are alive.
	int32 AliveCount = 0;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC && PC->GetPawn() && !PC->GetPawn()->IsPendingKillPending())
		{
			++AliveCount;
		}
	}

	// Also count squad member AI characters (ASHSquadMember) if present.
	// Use a generic pawn iterator filtered by tag to avoid a hard dependency.
	TArray<AActor*> FriendlyActors;
	UGameplayStatics::GetAllActorsWithTag(World, FName(TEXT("Friendly")), FriendlyActors);
	for (AActor* Actor : FriendlyActors)
	{
		APawn* FriendlyPawn = Cast<APawn>(Actor);
		if (FriendlyPawn && !FriendlyPawn->IsPendingKillPending())
		{
			++AliveCount;
		}
	}

	return AliveCount;
}
