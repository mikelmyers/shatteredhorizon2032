// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHGameMode.h"
#include "SHGameState.h"
#include "SHPlayerController.h"
#include "SHPlayerCharacter.h"
#include "SHPlayerState.h"
#include "AI/SHEnemyCharacter.h"
#include "AI/Primordia/SHPrimordiaDecisionEngine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"

ASHGameMode::ASHGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f; // tick every frame

	GameStateClass = ASHGameState::StaticClass();
	PlayerControllerClass = ASHPlayerController::StaticClass();
	DefaultPawnClass = ASHPlayerCharacter::StaticClass();
	PlayerStateClass = ASHPlayerState::StaticClass();
}

// =======================================================================
//  AGameModeBase overrides
// =======================================================================

void ASHGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	TimeOfDayHours = StartingTimeOfDay;
}

void ASHGameMode::StartPlay()
{
	Super::StartPlay();

	SHGameState = GetGameState<ASHGameState>();
	if (SHGameState)
	{
		SHGameState->SetMissionPhase(CurrentPhase);
		SHGameState->SetTimeOfDay(TimeOfDayHours);
	}

	// Build the initial reinforcement schedule for the PreInvasion phase.
	BuildReinforcementSchedule(CurrentPhase);

	UE_LOG(LogTemp, Log, TEXT("[SHGameMode] StartPlay — Phase: PreInvasion, Time: %.1f h"), TimeOfDayHours);
}

void ASHGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	TickPhaseLogic(DeltaSeconds);
	TickReinforcementScheduler(DeltaSeconds);
	TickTimeOfDay(DeltaSeconds);
	TickPrimordiaAI(DeltaSeconds);
}

void ASHGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	Super::HandleStartingNewPlayer_Implementation(NewPlayer);

	// Inform the new player of the current phase and objectives.
	if (ASHPlayerController* SHPC = Cast<ASHPlayerController>(NewPlayer))
	{
		// Future: push current objective state to the player's HUD.
	}
}

// =======================================================================
//  Phase management
// =======================================================================

void ASHGameMode::AdvanceToPhase(ESHMissionPhase NewPhase)
{
	if (NewPhase == CurrentPhase)
	{
		return;
	}

	const ESHMissionPhase OldPhase = CurrentPhase;
	CurrentPhase = NewPhase;
	PhaseElapsedTime = 0.f;

	// Clear wave tracking for the new phase.
	PendingWaves.Empty();
	DispatchedWaveIndices.Empty();
	BuildReinforcementSchedule(NewPhase);

	if (SHGameState)
	{
		SHGameState->SetMissionPhase(NewPhase);
	}

	UE_LOG(LogTemp, Log, TEXT("[SHGameMode] Phase advanced: %d -> %d"), static_cast<int32>(OldPhase), static_cast<int32>(NewPhase));

	// Request immediate AI evaluation after phase change.
	RequestPrimordiaEvaluation();
}

bool ASHGameMode::ShouldAdvancePhase() const
{
	if (!SHGameState)
	{
		return false;
	}

	switch (CurrentPhase)
	{
	case ESHMissionPhase::PreInvasion:
		return PhaseElapsedTime >= PreInvasionDuration;

	case ESHMissionPhase::BeachAssault:
	{
		// Transition to urban fallback when the primary defensive line is breached
		// or enemy force projection exceeds threshold.
		const float EnemyStrength = SHGameState->GetEnemyForceStrengthNormalized();
		const float PrimaryLineIntegrity = SHGameState->GetDefensiveLineIntegrity(ESHDefensiveLine::Primary);
		return (EnemyStrength >= FallbackEnemyStrengthThreshold) || (PrimaryLineIntegrity <= 0.2f);
	}

	case ESHMissionPhase::UrbanFallback:
	{
		// Counterattack becomes available when the final line is under threat
		// and friendly reinforcements have arrived.
		const float FinalLineIntegrity = SHGameState->GetDefensiveLineIntegrity(ESHDefensiveLine::Final);
		return FinalLineIntegrity <= CounterattackIntegrityThreshold;
	}

	case ESHMissionPhase::Counterattack:
		// Terminal phase — mission ends via objective completion or failure.
		return false;

	default:
		return false;
	}
}

// =======================================================================
//  Reinforcements
// =======================================================================

void ASHGameMode::TriggerReinforcementWave(const FSHReinforcementWave& Wave)
{
	ExecuteWaveSpawn(Wave);
}

void ASHGameMode::BuildReinforcementSchedule(ESHMissionPhase Phase)
{
	if (const TArray<FSHReinforcementWave>* Templates = PhaseReinforcementTemplates.Find(Phase))
	{
		PendingWaves = *Templates;
	}
	else
	{
		// Procedural fallback — generate a basic schedule.
		PendingWaves.Empty();

		switch (Phase)
		{
		case ESHMissionPhase::BeachAssault:
		{
			FSHReinforcementWave Initial;
			Initial.DelayFromPhaseStart = 10.f;
			Initial.InfantryCount = 24;
			Initial.AmphibiousCount = 4;
			Initial.SpawnZoneTag = FName(TEXT("Beach_East"));
			PendingWaves.Add(Initial);

			FSHReinforcementWave Second;
			Second.DelayFromPhaseStart = 90.f;
			Second.InfantryCount = 36;
			Second.ArmorCount = 2;
			Second.AmphibiousCount = 6;
			Second.bIncludesAirSupport = true;
			Second.SpawnZoneTag = FName(TEXT("Beach_West"));
			PendingWaves.Add(Second);

			FSHReinforcementWave Third;
			Third.DelayFromPhaseStart = 210.f;
			Third.InfantryCount = 48;
			Third.ArmorCount = 4;
			Third.AmphibiousCount = 8;
			Third.bIncludesAirSupport = true;
			Third.SpawnZoneTag = FName(TEXT("Beach_Center"));
			PendingWaves.Add(Third);
			break;
		}
		case ESHMissionPhase::UrbanFallback:
		{
			FSHReinforcementWave Wave;
			Wave.DelayFromPhaseStart = 30.f;
			Wave.InfantryCount = 40;
			Wave.ArmorCount = 6;
			Wave.SpawnZoneTag = FName(TEXT("Urban_North"));
			PendingWaves.Add(Wave);
			break;
		}
		case ESHMissionPhase::Counterattack:
		{
			FSHReinforcementWave FriendlyReinforcement;
			FriendlyReinforcement.DelayFromPhaseStart = 0.f;
			FriendlyReinforcement.InfantryCount = 60;
			FriendlyReinforcement.ArmorCount = 8;
			FriendlyReinforcement.bIncludesAirSupport = true;
			FriendlyReinforcement.SpawnZoneTag = FName(TEXT("Rear_Staging"));
			PendingWaves.Add(FriendlyReinforcement);
			break;
		}
		default:
			break;
		}
	}

	DispatchedWaveIndices.Empty();
}

void ASHGameMode::ExecuteWaveSpawn(const FSHReinforcementWave& Wave)
{
	UE_LOG(LogTemp, Log, TEXT("[SHGameMode] Spawning wave — Infantry: %d, Armor: %d, Amphibious: %d, Air: %s, Zone: %s"),
		Wave.InfantryCount, Wave.ArmorCount, Wave.AmphibiousCount,
		Wave.bIncludesAirSupport ? TEXT("Yes") : TEXT("No"),
		*Wave.SpawnZoneTag.ToString());

	if (SHGameState)
	{
		// Update force disposition on the replicated game state.
		SHGameState->AddEnemyForces(Wave.InfantryCount, Wave.ArmorCount, Wave.AmphibiousCount);

		// Increase battle intensity when waves land.
		const float IntensityBump = (Wave.InfantryCount * 0.01f) + (Wave.ArmorCount * 0.05f);
		SHGameState->AddBattleIntensity(FMath::Clamp(IntensityBump, 0.f, 0.3f));
	}

	// Spawn infantry squads at tagged spawn points.
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Find spawn points tagged with the wave's spawn zone.
	TArray<AActor*> SpawnPoints;
	UGameplayStatics::GetAllActorsWithTag(World, Wave.SpawnZoneTag, SpawnPoints);

	if (SpawnPoints.Num() == 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SHGameMode] No spawn points found with tag '%s'. Cannot spawn wave."),
			*Wave.SpawnZoneTag.ToString());
		return;
	}

	// Spawn infantry in squads of 6 (doctrine PLA squad size).
	const int32 SquadSize = 6;
	const int32 SquadCount = FMath::CeilToInt(static_cast<float>(Wave.InfantryCount) / SquadSize);
	TArray<int32> NewSquadIds;

	for (int32 SquadIdx = 0; SquadIdx < SquadCount; ++SquadIdx)
	{
		const int32 MembersInThisSquad = FMath::Min(SquadSize, Wave.InfantryCount - (SquadIdx * SquadSize));
		const AActor* SpawnOrigin = SpawnPoints[SquadIdx % SpawnPoints.Num()];
		const FVector BaseLocation = SpawnOrigin->GetActorLocation();

		for (int32 MemberIdx = 0; MemberIdx < MembersInThisSquad; ++MemberIdx)
		{
			// Offset each member within the squad formation (spread ~200cm apart).
			const float AngleRad = (2.0f * PI * MemberIdx) / MembersInThisSquad;
			const FVector Offset(FMath::Cos(AngleRad) * 200.0f, FMath::Sin(AngleRad) * 200.0f, 0.0f);
			const FVector SpawnLocation = BaseLocation + Offset;

			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

			// Spawn the enemy character. The actual class is set via DefaultPawnClass
			// or a data-driven class reference. For now, use the EnemyCharacterClass.
			AActor* SpawnedEnemy = World->SpawnActor<AActor>(
				EnemyCharacterClass, SpawnLocation, FRotator::ZeroRotator, SpawnParams);

			if (SpawnedEnemy)
			{
				SpawnedEnemy->Tags.Add(Wave.SpawnZoneTag);
			}
		}

		// Register squad with Primordia Decision Engine if available.
		const int32 SquadId = NextSquadId++;
		NewSquadIds.Add(SquadId);

		UE_LOG(LogTemp, Log, TEXT("[SHGameMode] Spawned squad %d (%d members) at %s"),
			SquadId, MembersInThisSquad, *BaseLocation.ToString());
	}

	// Notify Primordia Decision Engine of new squads.
	if (PrimordiaDecisionEngine)
	{
		for (int32 SquadId : NewSquadIds)
		{
			PrimordiaDecisionEngine->RegisterSquad(SquadId, SquadSize, false);
		}
		PrimordiaDecisionEngine->OnReinforcementWaveSpawned(NewSquadIds);
	}

	// TODO(future): Spawn armored vehicles from Wave.ArmorCount and amphibious from Wave.AmphibiousCount.
	if (Wave.ArmorCount > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("[SHGameMode] Armor spawn (%d vehicles) — vehicle spawning not yet wired."), Wave.ArmorCount);
	}
}

// =======================================================================
//  Dynamic objectives
// =======================================================================

FGuid ASHGameMode::CreateDynamicObjective(const FText& Name, const FText& Desc, FVector Location, ESHMissionPhase Phase)
{
	FSHDynamicObjective Obj;
	Obj.ObjectiveId = FGuid::NewGuid();
	Obj.DisplayName = Name;
	Obj.Description = Desc;
	Obj.WorldLocation = Location;
	Obj.RelevantPhase = Phase;

	ActiveObjectives.Add(Obj.ObjectiveId, Obj);
	ReplicateObjectivesToGameState();

	UE_LOG(LogTemp, Log, TEXT("[SHGameMode] Created objective: %s — %s"), *Obj.ObjectiveId.ToString(), *Name.ToString());
	return Obj.ObjectiveId;
}

void ASHGameMode::CompleteObjective(const FGuid& ObjectiveId)
{
	if (FSHDynamicObjective* Obj = ActiveObjectives.Find(ObjectiveId))
	{
		Obj->bIsComplete = true;
		Obj->Progress = 1.f;
		ReplicateObjectivesToGameState();

		UE_LOG(LogTemp, Log, TEXT("[SHGameMode] Objective completed: %s"), *ObjectiveId.ToString());
	}
}

void ASHGameMode::FailObjective(const FGuid& ObjectiveId)
{
	if (FSHDynamicObjective* Obj = ActiveObjectives.Find(ObjectiveId))
	{
		Obj->bIsFailed = true;
		ReplicateObjectivesToGameState();

		UE_LOG(LogTemp, Log, TEXT("[SHGameMode] Objective failed: %s"), *ObjectiveId.ToString());
	}
}

void ASHGameMode::ReplicateObjectivesToGameState()
{
	if (SHGameState)
	{
		TArray<FSHDynamicObjective> Objectives;
		ActiveObjectives.GenerateValueArray(Objectives);
		SHGameState->SetActiveObjectives(Objectives);
	}
}

// =======================================================================
//  Primordia AI interface
// =======================================================================

void ASHGameMode::RequestPrimordiaEvaluation()
{
	if (!SHGameState)
	{
		return;
	}

	// Gather situation report for the AI director.
	const float EnemyStrength = SHGameState->GetEnemyForceStrengthNormalized();
	const float FriendlyCasualties = SHGameState->GetFriendlyCasualtyRate();
	const float BattleIntensity = SHGameState->GetBattleIntensity();
	const float PrimaryIntegrity = SHGameState->GetDefensiveLineIntegrity(ESHDefensiveLine::Primary);

	// Determine AI aggression modifier based on situation.
	// Higher enemy strength and lower defender integrity increases aggression.
	float Aggression = FMath::Clamp(
		(EnemyStrength * 0.5f) + ((1.f - PrimaryIntegrity) * 0.3f) + (BattleIntensity * 0.2f),
		0.f, 1.f);

	// Feed back into game state for AI subsystems to read.
	SHGameState->SetAIAggressionModifier(Aggression);

	// Decide whether to generate flanking objectives or reinforcement requests.
	if (CurrentPhase == ESHMissionPhase::BeachAssault && EnemyStrength > 0.4f && PrimaryIntegrity > 0.3f)
	{
		// Enemy has strength and the line holds — attempt flanking.
		if (FMath::FRand() < Aggression * 0.3f)
		{
			CreateDynamicObjective(
				FText::FromString(TEXT("Repel Flanking Force")),
				FText::FromString(TEXT("Enemy forces are attempting to flank the primary defensive line.")),
				FVector(FMath::FRandRange(-5000.f, 5000.f), FMath::FRandRange(2000.f, 8000.f), 0.f),
				ESHMissionPhase::BeachAssault
			);
		}
	}

	UE_LOG(LogTemp, Verbose, TEXT("[SHGameMode] Primordia eval — Aggression: %.2f, EnemyStr: %.2f, Integrity: %.2f"),
		Aggression, EnemyStrength, PrimaryIntegrity);
}

// =======================================================================
//  Time-of-day
// =======================================================================

void ASHGameMode::SetTimeOfDayHours(float Hours)
{
	TimeOfDayHours = FMath::Fmod(Hours, 24.f);
	if (SHGameState)
	{
		SHGameState->SetTimeOfDay(TimeOfDayHours);
	}
}

void ASHGameMode::SetTimeScale(float Scale)
{
	TimeScaleMultiplier = FMath::Max(Scale, 0.f);
}

// =======================================================================
//  Tick helpers
// =======================================================================

void ASHGameMode::TickPhaseLogic(float DeltaSeconds)
{
	PhaseElapsedTime += DeltaSeconds;

	if (ShouldAdvancePhase())
	{
		switch (CurrentPhase)
		{
		case ESHMissionPhase::PreInvasion:
			AdvanceToPhase(ESHMissionPhase::BeachAssault);
			break;
		case ESHMissionPhase::BeachAssault:
			AdvanceToPhase(ESHMissionPhase::UrbanFallback);
			break;
		case ESHMissionPhase::UrbanFallback:
			AdvanceToPhase(ESHMissionPhase::Counterattack);
			break;
		default:
			break;
		}
	}
}

void ASHGameMode::TickReinforcementScheduler(float DeltaSeconds)
{
	for (int32 i = 0; i < PendingWaves.Num(); ++i)
	{
		if (DispatchedWaveIndices.Contains(i))
		{
			continue;
		}

		if (PhaseElapsedTime >= PendingWaves[i].DelayFromPhaseStart)
		{
			DispatchedWaveIndices.Add(i);
			ExecuteWaveSpawn(PendingWaves[i]);
		}
	}
}

void ASHGameMode::TickTimeOfDay(float DeltaSeconds)
{
	if (SecondsPerGameHour > 0.f)
	{
		const float HoursElapsed = (DeltaSeconds * TimeScaleMultiplier) / SecondsPerGameHour;
		TimeOfDayHours = FMath::Fmod(TimeOfDayHours + HoursElapsed, 24.f);

		if (SHGameState)
		{
			SHGameState->SetTimeOfDay(TimeOfDayHours);

			// Update visibility based on time-of-day.
			float Visibility = 1.f;
			if (TimeOfDayHours < 6.f || TimeOfDayHours > 20.f)
			{
				// Night — reduced visibility.
				Visibility = 0.3f;
			}
			else if (TimeOfDayHours < 7.f || TimeOfDayHours > 19.f)
			{
				// Dawn/dusk transition.
				const float TransitionAlpha = (TimeOfDayHours < 12.f)
					? (TimeOfDayHours - 6.f)
					: (20.f - TimeOfDayHours);
				Visibility = FMath::Lerp(0.3f, 1.f, FMath::Clamp(TransitionAlpha, 0.f, 1.f));
			}

			SHGameState->SetVisibility(Visibility);
		}
	}
}

void ASHGameMode::TickPrimordiaAI(float DeltaSeconds)
{
	PrimordiaTickAccumulator += DeltaSeconds;
	if (PrimordiaTickAccumulator >= PrimordiaTickInterval)
	{
		PrimordiaTickAccumulator -= PrimordiaTickInterval;
		RequestPrimordiaEvaluation();
	}
}
