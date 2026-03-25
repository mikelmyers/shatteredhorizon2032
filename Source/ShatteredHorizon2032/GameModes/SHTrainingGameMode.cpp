// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHTrainingGameMode.h"
#include "Core/SHPlayerCharacter.h"
#include "Core/SHPlayerController.h"
#include "Core/SHGameState.h"
#include "Squad/SHSquadManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

ASHTrainingGameMode::ASHTrainingGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
}

// =======================================================================
//  AGameModeBase overrides
// =======================================================================

void ASHTrainingGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// Reset training state.
	CurrentSection = ESHTrainingSection::Movement;
	TotalTrainingTime = 0.f;
	SectionElapsedTime = 0.f;
	bTrainingSkipped = false;
	bCallForFireComplete = false;
	bCombinedArmsComplete = false;
	MovementTasks = FSHMovementTaskStatus();
	ShootingTasks = FSHShootingTaskStatus();
	SquadCommandTasks = FSHSquadCommandTaskStatus();
	TrainingTargets.Empty();
	NextTargetId = 1;

	UE_LOG(LogTemp, Log, TEXT("[SHTrainingGameMode] InitGame — Pre-deployment training initialized."));
}

void ASHTrainingGameMode::StartPlay()
{
	Super::StartPlay();

	UE_LOG(LogTemp, Log, TEXT("[SHTrainingGameMode] StartPlay — Beginning Movement & Maneuver section."));
}

void ASHTrainingGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bTrainingSkipped || CurrentSection == ESHTrainingSection::Complete)
	{
		return;
	}

	TotalTrainingTime += DeltaSeconds;
	SectionElapsedTime += DeltaSeconds;

	// Cache the player character on first available tick.
	if (!CachedPlayerCharacter)
	{
		if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0))
		{
			CachedPlayerCharacter = Cast<ASHPlayerCharacter>(PC->GetPawn());
		}
	}

	// Tick active training targets regardless of section.
	TickTrainingTargets(DeltaSeconds);

	// Tick the current section's logic.
	switch (CurrentSection)
	{
	case ESHTrainingSection::Movement:
		TickMovementSection(DeltaSeconds);
		break;
	case ESHTrainingSection::Shooting:
		TickShootingSection(DeltaSeconds);
		break;
	case ESHTrainingSection::SquadCommands:
		TickSquadCommandsSection(DeltaSeconds);
		break;
	case ESHTrainingSection::IndirectFire:
		TickIndirectFireSection(DeltaSeconds);
		break;
	case ESHTrainingSection::CombinedArms:
		TickCombinedArmsSection(DeltaSeconds);
		break;
	default:
		break;
	}
}

// =======================================================================
//  Section progression
// =======================================================================

void ASHTrainingGameMode::AdvanceToSection(ESHTrainingSection NewSection)
{
	if (NewSection == CurrentSection)
	{
		return;
	}

	// Enforce linear progression: only allow advancing to the next section in sequence.
	const uint8 CurrentIndex = static_cast<uint8>(CurrentSection);
	const uint8 NewIndex = static_cast<uint8>(NewSection);

	if (NewIndex != CurrentIndex + 1)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[SHTrainingGameMode] AdvanceToSection — Cannot skip from section %d to %d. Sections must be completed in order."),
			CurrentIndex, NewIndex);
		return;
	}

	const ESHTrainingSection PreviousSection = CurrentSection;
	CurrentSection = NewSection;
	SectionElapsedTime = 0.f;

	UE_LOG(LogTemp, Log, TEXT("[SHTrainingGameMode] Advanced from section %d to section %d. Elapsed: %.1f s"),
		static_cast<int32>(PreviousSection), static_cast<int32>(NewSection), TotalTrainingTime);

	// Broadcast section completed for the previous section.
	OnSectionCompleted.Broadcast(PreviousSection);

	// Set up the new section if needed.
	if (NewSection == ESHTrainingSection::Shooting)
	{
		SetupShootingRange();
	}

	// Check if training is now complete.
	if (NewSection == ESHTrainingSection::Complete)
	{
		UE_LOG(LogTemp, Log, TEXT("[SHTrainingGameMode] Pre-deployment training complete. Total time: %.1f seconds."),
			TotalTrainingTime);
		OnTrainingCompleted.Broadcast(TotalTrainingTime);
	}
}

float ASHTrainingGameMode::GetSectionProgress() const
{
	switch (CurrentSection)
	{
	case ESHTrainingSection::Movement:
	{
		float Progress = 0.f;
		if (MovementTasks.bSprintToWaypoint) Progress += 0.25f;
		if (MovementTasks.bVaultObstacle) Progress += 0.25f;
		if (MovementTasks.bGoProne) Progress += 0.25f;
		if (MovementTasks.bLean) Progress += 0.25f;
		return Progress;
	}

	case ESHTrainingSection::Shooting:
	{
		float Progress = 0.f;
		if (ShootingTasks.bHit50m) Progress += 0.25f;
		if (ShootingTasks.bHit100m) Progress += 0.25f;
		if (ShootingTasks.bHit200m) Progress += 0.25f;
		if (ShootingTasks.bHit300m) Progress += 0.25f;
		return Progress;
	}

	case ESHTrainingSection::SquadCommands:
	{
		float Progress = 0.f;
		if (SquadCommandTasks.bIssuedMoveOrder) Progress += 0.333f;
		if (SquadCommandTasks.bIssuedSuppressOrder) Progress += 0.333f;
		if (SquadCommandTasks.bIssuedFlankOrder) Progress += 0.334f;
		return Progress;
	}

	case ESHTrainingSection::IndirectFire:
		return bCallForFireComplete ? 1.f : 0.f;

	case ESHTrainingSection::CombinedArms:
		return bCombinedArmsComplete ? 1.f : 0.f;

	case ESHTrainingSection::Complete:
		return 1.f;

	default:
		return 0.f;
	}
}

void ASHTrainingGameMode::SkipTraining()
{
	if (bTrainingSkipped || CurrentSection == ESHTrainingSection::Complete)
	{
		return;
	}

	bTrainingSkipped = true;
	const ESHTrainingSection PreviousSection = CurrentSection;
	CurrentSection = ESHTrainingSection::Complete;

	UE_LOG(LogTemp, Log,
		TEXT("[SHTrainingGameMode] Training skipped by returning player. Was at section %d, elapsed: %.1f s."),
		static_cast<int32>(PreviousSection), TotalTrainingTime);

	// Clear any active targets.
	TrainingTargets.Empty();

	OnTrainingCompleted.Broadcast(TotalTrainingTime);
}

// =======================================================================
//  Target management
// =======================================================================

int32 ASHTrainingGameMode::SpawnPopUpTarget(FVector Location, float DistanceMeters, float ExposureDuration)
{
	FSHTrainingTarget Target;
	Target.TargetId = NextTargetId++;
	Target.Location = Location;
	Target.DistanceMeters = DistanceMeters;
	Target.ExposureDuration = ExposureDuration;
	Target.TimeRemaining = ExposureDuration;
	Target.bIsActive = true;
	Target.bIsHit = false;

	TrainingTargets.Add(Target);

	UE_LOG(LogTemp, Log, TEXT("[SHTrainingGameMode] Pop-up target #%d spawned at %.0fm, exposure: %.1fs"),
		Target.TargetId, DistanceMeters, ExposureDuration);

	return Target.TargetId;
}

void ASHTrainingGameMode::RegisterTargetHit(int32 TargetId)
{
	for (FSHTrainingTarget& Target : TrainingTargets)
	{
		if (Target.TargetId == TargetId && Target.bIsActive && !Target.bIsHit)
		{
			Target.bIsHit = true;
			Target.bIsActive = false;

			UE_LOG(LogTemp, Log, TEXT("[SHTrainingGameMode] Target #%d hit at %.0fm."),
				TargetId, Target.DistanceMeters);

			// Update shooting task status based on range band.
			if (CurrentSection == ESHTrainingSection::Shooting)
			{
				if (Target.DistanceMeters <= 75.f)
				{
					ShootingTasks.bHit50m = true;
				}
				else if (Target.DistanceMeters <= 125.f)
				{
					ShootingTasks.bHit100m = true;
				}
				else if (Target.DistanceMeters <= 250.f)
				{
					ShootingTasks.bHit200m = true;
				}
				else
				{
					ShootingTasks.bHit300m = true;
				}
			}

			return;
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[SHTrainingGameMode] RegisterTargetHit — Target #%d not found or already hit."),
		TargetId);
}

TArray<FSHTrainingTarget> ASHTrainingGameMode::GetActiveTargets() const
{
	TArray<FSHTrainingTarget> ActiveTargets;
	for (const FSHTrainingTarget& Target : TrainingTargets)
	{
		if (Target.bIsActive)
		{
			ActiveTargets.Add(Target);
		}
	}
	return ActiveTargets;
}

// =======================================================================
//  Event notifications
// =======================================================================

void ASHTrainingGameMode::NotifySprintToWaypoint()
{
	if (CurrentSection == ESHTrainingSection::Movement && !MovementTasks.bSprintToWaypoint)
	{
		MovementTasks.bSprintToWaypoint = true;
		UE_LOG(LogTemp, Log, TEXT("[SHTrainingGameMode] Movement — Sprint to waypoint: COMPLETE."));
	}
}

void ASHTrainingGameMode::NotifyVaultObstacle()
{
	if (CurrentSection == ESHTrainingSection::Movement && !MovementTasks.bVaultObstacle)
	{
		MovementTasks.bVaultObstacle = true;
		UE_LOG(LogTemp, Log, TEXT("[SHTrainingGameMode] Movement — Vault obstacle: COMPLETE."));
	}
}

void ASHTrainingGameMode::NotifyGoProne()
{
	if (CurrentSection == ESHTrainingSection::Movement && !MovementTasks.bGoProne)
	{
		MovementTasks.bGoProne = true;
		UE_LOG(LogTemp, Log, TEXT("[SHTrainingGameMode] Movement — Go prone: COMPLETE."));
	}
}

void ASHTrainingGameMode::NotifyLean()
{
	if (CurrentSection == ESHTrainingSection::Movement && !MovementTasks.bLean)
	{
		MovementTasks.bLean = true;
		UE_LOG(LogTemp, Log, TEXT("[SHTrainingGameMode] Movement — Lean: COMPLETE."));
	}
}

void ASHTrainingGameMode::NotifySquadMoveOrder()
{
	if (CurrentSection == ESHTrainingSection::SquadCommands && !SquadCommandTasks.bIssuedMoveOrder)
	{
		SquadCommandTasks.bIssuedMoveOrder = true;
		UE_LOG(LogTemp, Log, TEXT("[SHTrainingGameMode] Squad Commands — Move order: COMPLETE."));
	}
}

void ASHTrainingGameMode::NotifySquadSuppressOrder()
{
	if (CurrentSection == ESHTrainingSection::SquadCommands && !SquadCommandTasks.bIssuedSuppressOrder)
	{
		SquadCommandTasks.bIssuedSuppressOrder = true;
		UE_LOG(LogTemp, Log, TEXT("[SHTrainingGameMode] Squad Commands — Suppress order: COMPLETE."));
	}
}

void ASHTrainingGameMode::NotifySquadFlankOrder()
{
	if (CurrentSection == ESHTrainingSection::SquadCommands && !SquadCommandTasks.bIssuedFlankOrder)
	{
		SquadCommandTasks.bIssuedFlankOrder = true;
		UE_LOG(LogTemp, Log, TEXT("[SHTrainingGameMode] Squad Commands — Flank order: COMPLETE."));
	}
}

void ASHTrainingGameMode::NotifyCallForFire()
{
	if (CurrentSection == ESHTrainingSection::IndirectFire && !bCallForFireComplete)
	{
		bCallForFireComplete = true;
		UE_LOG(LogTemp, Log, TEXT("[SHTrainingGameMode] Indirect Fire — Call for fire on marked target: COMPLETE."));
	}
}

void ASHTrainingGameMode::NotifyCombinedArmsComplete()
{
	if (CurrentSection == ESHTrainingSection::CombinedArms && !bCombinedArmsComplete)
	{
		bCombinedArmsComplete = true;
		UE_LOG(LogTemp, Log, TEXT("[SHTrainingGameMode] Combined Arms — Coordinated engagement: COMPLETE."));
	}
}

// =======================================================================
//  Internal tick helpers
// =======================================================================

void ASHTrainingGameMode::TickMovementSection(float DeltaSeconds)
{
	if (MovementTasks.IsComplete())
	{
		CompleteCurrentSection();
	}
}

void ASHTrainingGameMode::TickShootingSection(float DeltaSeconds)
{
	// Check if all range bands have been hit.
	if (ShootingTasks.IsComplete())
	{
		CompleteCurrentSection();
		return;
	}

	// Ensure there are active targets on the range. If all current targets
	// have expired or been hit, spawn new ones for the range bands still needed.
	bool bHasActiveTarget = false;
	for (const FSHTrainingTarget& Target : TrainingTargets)
	{
		if (Target.bIsActive)
		{
			bHasActiveTarget = true;
			break;
		}
	}

	if (!bHasActiveTarget)
	{
		// Spawn targets for bands not yet qualified.
		const float RangeBands[] = { 50.f, 100.f, 200.f, 300.f };
		const bool* HitFlags[] = { &ShootingTasks.bHit50m, &ShootingTasks.bHit100m,
									&ShootingTasks.bHit200m, &ShootingTasks.bHit300m };
		const float ExposureTimes[] = { 6.f, 6.f, 8.f, 10.f };

		for (int32 i = 0; i < 4; ++i)
		{
			if (!(*HitFlags[i]))
			{
				const FVector TargetLocation = FiringLineOrigin
					+ FiringRangeDirection * (RangeBands[i] * 100.f); // Convert meters to cm.
				SpawnPopUpTarget(TargetLocation, RangeBands[i], ExposureTimes[i]);
			}
		}
	}
}

void ASHTrainingGameMode::TickSquadCommandsSection(float DeltaSeconds)
{
	if (SquadCommandTasks.IsComplete())
	{
		CompleteCurrentSection();
	}
}

void ASHTrainingGameMode::TickIndirectFireSection(float DeltaSeconds)
{
	if (bCallForFireComplete)
	{
		CompleteCurrentSection();
	}
}

void ASHTrainingGameMode::TickCombinedArmsSection(float DeltaSeconds)
{
	if (bCombinedArmsComplete)
	{
		CompleteCurrentSection();
	}
}

void ASHTrainingGameMode::TickTrainingTargets(float DeltaSeconds)
{
	for (FSHTrainingTarget& Target : TrainingTargets)
	{
		if (!Target.bIsActive)
		{
			continue;
		}

		Target.TimeRemaining -= DeltaSeconds;
		if (Target.TimeRemaining <= 0.f)
		{
			// Target auto-lowers after exposure duration expires.
			Target.bIsActive = false;
			Target.TimeRemaining = 0.f;

			UE_LOG(LogTemp, Verbose, TEXT("[SHTrainingGameMode] Target #%d at %.0fm expired."),
				Target.TargetId, Target.DistanceMeters);
		}
	}
}

void ASHTrainingGameMode::CompleteCurrentSection()
{
	switch (CurrentSection)
	{
	case ESHTrainingSection::Movement:
		AdvanceToSection(ESHTrainingSection::Shooting);
		break;
	case ESHTrainingSection::Shooting:
		AdvanceToSection(ESHTrainingSection::SquadCommands);
		break;
	case ESHTrainingSection::SquadCommands:
		AdvanceToSection(ESHTrainingSection::IndirectFire);
		break;
	case ESHTrainingSection::IndirectFire:
		AdvanceToSection(ESHTrainingSection::CombinedArms);
		break;
	case ESHTrainingSection::CombinedArms:
		AdvanceToSection(ESHTrainingSection::Complete);
		break;
	default:
		break;
	}
}

void ASHTrainingGameMode::SetupShootingRange()
{
	// Clear any existing targets from previous section.
	TrainingTargets.Empty();

	UE_LOG(LogTemp, Log, TEXT("[SHTrainingGameMode] Marksmanship range initialized. Firing line at (%.0f, %.0f, %.0f)."),
		FiringLineOrigin.X, FiringLineOrigin.Y, FiringLineOrigin.Z);

	// Spawn initial targets at all four range bands.
	const float RangeBands[] = { 50.f, 100.f, 200.f, 300.f };
	const float ExposureTimes[] = { 6.f, 6.f, 8.f, 10.f };

	for (int32 i = 0; i < 4; ++i)
	{
		const FVector TargetLocation = FiringLineOrigin
			+ FiringRangeDirection * (RangeBands[i] * 100.f);
		SpawnPopUpTarget(TargetLocation, RangeBands[i], ExposureTimes[i]);
	}
}
