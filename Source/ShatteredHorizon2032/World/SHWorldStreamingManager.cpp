// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHWorldStreamingManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/LevelStreaming.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogSH_Streaming);

// ======================================================================
//  Construction
// ======================================================================

USHWorldStreamingManager::USHWorldStreamingManager()
{
	FPSHistory.Reserve(FPSHistorySize);
}

// ======================================================================
//  USubsystem interface
// ======================================================================

void USHWorldStreamingManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	PerformanceBudget.TargetFPS = TargetFPS;
	PerformanceBudget.StreamingMemoryBudgetMB = MaxStreamingMemoryMB;

	UE_LOG(LogSH_Streaming, Log, TEXT("WorldStreamingManager initialised. AO bounds: [%s] to [%s]"),
		*AOBoundsMin.ToString(), *AOBoundsMax.ToString());
}

void USHWorldStreamingManager::Deinitialize()
{
	CellInfos.Empty();
	ActiveCombatZones.Empty();
	SquadMemberPositions.Empty();
	ForcedLODOverrides.Empty();

	Super::Deinitialize();
}

TStatId USHWorldStreamingManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USHWorldStreamingManager, STATGROUP_Tickables);
}

// ======================================================================
//  Tick
// ======================================================================

void USHWorldStreamingManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdatePlayerPosition();
	UpdatePerformanceBudget(DeltaTime);
	UpdateCellPriorities();
	UpdateStreamingRequests();
	UpdateLODs();
	UpdateOutOfBounds();
	PredictiveLoad();
}

// ======================================================================
//  Player tracking
// ======================================================================

void USHWorldStreamingManager::UpdatePlayerPosition()
{
	UWorld* World = GetWorld();
	if (!World) return;

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC) return;

	APawn* Pawn = PC->GetPawn();
	if (!Pawn) return;

	FVector PreviousLocation = PlayerLocation;
	PlayerLocation = Pawn->GetActorLocation();

	// Derive velocity from position delta if no movement component
	ACharacter* Character = Cast<ACharacter>(Pawn);
	if (Character && Character->GetCharacterMovement())
	{
		PlayerVelocity = Character->GetCharacterMovement()->Velocity;
	}
	else
	{
		float DT = GetWorld()->GetDeltaSeconds();
		if (DT > KINDA_SMALL_NUMBER)
		{
			PlayerVelocity = (PlayerLocation - PreviousLocation) / DT;
		}
	}
}

// ======================================================================
//  Cell priority update (staggered across frames)
// ======================================================================

void USHWorldStreamingManager::UpdateCellPriorities()
{
	if (CellInfos.Num() == 0) return;

	int32 StartIdx = CellUpdateCursor;
	int32 Count = FMath::Min(CellsPerFrameUpdate, CellInfos.Num());

	for (int32 i = 0; i < Count; ++i)
	{
		int32 Idx = (StartIdx + i) % CellInfos.Num();
		FSHStreamingCellInfo& Cell = CellInfos[Idx];

		Cell.DistanceToPlayer = FVector::Dist(Cell.CellCenter, PlayerLocation);

		// Check for combat zone overlap
		Cell.bHasActiveCombat = false;
		for (const auto& CombatZone : ActiveCombatZones)
		{
			if (FVector::Dist(Cell.CellCenter, CombatZone.Key) < CombatZone.Value)
			{
				Cell.bHasActiveCombat = true;
				break;
			}
		}

		// Check for squad member presence
		Cell.bHasSquadPresence = false;
		for (const auto& SquadPos : SquadMemberPositions)
		{
			// Assume cell radius of 25000 cm (250m) for presence check
			if (FVector::Dist(Cell.CellCenter, SquadPos.Value) < 25000.f)
			{
				Cell.bHasSquadPresence = true;
				break;
			}
		}

		Cell.Priority = ComputeCellPriority(Cell);
	}

	CellUpdateCursor = (StartIdx + Count) % FMath::Max(1, CellInfos.Num());
}

ESHStreamingPriority USHWorldStreamingManager::ComputeCellPriority(const FSHStreamingCellInfo& Cell) const
{
	float Dist = Cell.DistanceToPlayer;

	// Immediate area is always critical
	if (Dist < CriticalLoadRadius)
	{
		return ESHStreamingPriority::Critical;
	}

	// Combat zones are always at least High priority
	if (Cell.bHasActiveCombat)
	{
		return (Dist < HighLoadRadius) ? ESHStreamingPriority::Critical : ESHStreamingPriority::High;
	}

	// Squad members raise priority
	if (Cell.bHasSquadPresence && Dist < HighLoadRadius)
	{
		return ESHStreamingPriority::High;
	}

	if (Dist < HighLoadRadius)
	{
		return ESHStreamingPriority::Medium;
	}

	if (Dist < MaxStreamingRadius)
	{
		return ESHStreamingPriority::Low;
	}

	return ESHStreamingPriority::Background;
}

// ======================================================================
//  Streaming requests
// ======================================================================

void USHWorldStreamingManager::UpdateStreamingRequests()
{
	// Under budget pressure, shed low-priority cells
	bool bUnderPressure = PerformanceBudget.IsOverBudget();

	for (FSHStreamingCellInfo& Cell : CellInfos)
	{
		bool bShouldBeLoaded = false;

		switch (Cell.Priority)
		{
		case ESHStreamingPriority::Critical:
			bShouldBeLoaded = true;
			break;
		case ESHStreamingPriority::High:
			bShouldBeLoaded = true;
			break;
		case ESHStreamingPriority::Medium:
			bShouldBeLoaded = !bUnderPressure;
			break;
		case ESHStreamingPriority::Low:
			bShouldBeLoaded = !bUnderPressure && Cell.DistanceToPlayer < MaxStreamingRadius * 0.75f;
			break;
		case ESHStreamingPriority::Background:
			bShouldBeLoaded = false;
			break;
		}

		if (bShouldBeLoaded && !Cell.bIsLoaded)
		{
			// Request load via World Partition streaming source
			// In production this calls into UWorldPartitionSubsystem to add streaming sources
			Cell.bIsLoaded = true;
			PerformanceBudget.LoadedCellCount++;

			UE_LOG(LogSH_Streaming, Verbose, TEXT("Requesting load for cell %s (Priority: %d, Distance: %.0f)"),
				*Cell.CellName.ToString(), static_cast<int32>(Cell.Priority), Cell.DistanceToPlayer);
		}
		else if (!bShouldBeLoaded && Cell.bIsLoaded)
		{
			// Request unload — never unload cells with active combat
			if (!Cell.bHasActiveCombat)
			{
				Cell.bIsLoaded = false;
				PerformanceBudget.LoadedCellCount = FMath::Max(0, PerformanceBudget.LoadedCellCount - 1);

				UE_LOG(LogSH_Streaming, Verbose, TEXT("Requesting unload for cell %s (Priority: %d)"),
					*Cell.CellName.ToString(), static_cast<int32>(Cell.Priority));
			}
		}
	}

	PerformanceBudget.TotalCellCount = CellInfos.Num();
}

// ======================================================================
//  LOD management
// ======================================================================

void USHWorldStreamingManager::UpdateLODs()
{
	for (FSHStreamingCellInfo& Cell : CellInfos)
	{
		if (!Cell.bIsLoaded) continue;

		// Check forced override
		if (const int32* ForcedLOD = ForcedLODOverrides.Find(Cell.CellName))
		{
			Cell.CurrentLOD = *ForcedLOD;
			continue;
		}

		Cell.CurrentLOD = ComputeCellLOD(Cell);
	}
}

int32 USHWorldStreamingManager::ComputeCellLOD(const FSHStreamingCellInfo& Cell) const
{
	float Dist = Cell.DistanceToPlayer;

	// Under budget pressure, push LODs one level higher
	int32 PressureOffset = PerformanceBudget.IsOverBudget() ? 1 : 0;

	// Combat areas get best LOD regardless of distance
	if (Cell.bHasActiveCombat && Dist < CombatKeepLoadedRadius)
	{
		return FMath::Clamp(PressureOffset, 0, LODDistanceThresholds.Num() - 1);
	}

	int32 LODLevel = 0;
	for (int32 i = LODDistanceThresholds.Num() - 1; i >= 0; --i)
	{
		if (Dist >= LODDistanceThresholds[i])
		{
			LODLevel = i;
			break;
		}
	}

	return FMath::Clamp(LODLevel + PressureOffset, 0, LODDistanceThresholds.Num() - 1);
}

// ======================================================================
//  Performance budget
// ======================================================================

void USHWorldStreamingManager::UpdatePerformanceBudget(float DeltaTime)
{
	if (DeltaTime > KINDA_SMALL_NUMBER)
	{
		float InstantFPS = 1.f / DeltaTime;

		if (FPSHistory.Num() >= FPSHistorySize)
		{
			FPSHistory.RemoveAt(0, 1, false);
		}
		FPSHistory.Add(InstantFPS);

		// Running average
		float Sum = 0.f;
		for (float Val : FPSHistory)
		{
			Sum += Val;
		}
		PerformanceBudget.CurrentFPS = Sum / FPSHistory.Num();
	}

	PerformanceBudget.FrameTimeMS = DeltaTime * 1000.f;

	// Estimate streaming memory — in production, query RHI memory stats
	PerformanceBudget.StreamingMemoryMB = PerformanceBudget.LoadedCellCount * 32.f; // ~32MB per cell estimate

	// Fire warning delegate if over budget
	if (PerformanceBudget.IsOverBudget())
	{
		OnPerformanceBudgetWarning.Broadcast(PerformanceBudget);

		UE_LOG(LogSH_Streaming, Warning,
			TEXT("Performance budget exceeded: FPS=%.1f (target=%.1f), Memory=%.0fMB (budget=%.0fMB)"),
			PerformanceBudget.CurrentFPS, PerformanceBudget.TargetFPS,
			PerformanceBudget.StreamingMemoryMB, PerformanceBudget.StreamingMemoryBudgetMB);
	}
}

// ======================================================================
//  Out-of-bounds handling (NO invisible walls)
// ======================================================================

void USHWorldStreamingManager::UpdateOutOfBounds()
{
	// Compute distance outside the AO bounding box
	FVector ClampedPos;
	ClampedPos.X = FMath::Clamp(PlayerLocation.X, AOBoundsMin.X, AOBoundsMax.X);
	ClampedPos.Y = FMath::Clamp(PlayerLocation.Y, AOBoundsMin.Y, AOBoundsMax.Y);
	ClampedPos.Z = FMath::Clamp(PlayerLocation.Z, AOBoundsMin.Z, AOBoundsMax.Z);

	DistanceFromAO = FVector::Dist(PlayerLocation, ClampedPos);

	ESHOutOfBoundsSeverity NewSeverity = ESHOutOfBoundsSeverity::None;

	if (DistanceFromAO >= OOBCriticalDistance)
	{
		NewSeverity = ESHOutOfBoundsSeverity::Critical;
	}
	else if (DistanceFromAO >= OOBUrgentDistance)
	{
		NewSeverity = ESHOutOfBoundsSeverity::Urgent;
	}
	else if (DistanceFromAO >= OOBWarningDistance)
	{
		NewSeverity = ESHOutOfBoundsSeverity::Warning;
	}

	if (NewSeverity != CurrentOOBSeverity)
	{
		ESHOutOfBoundsSeverity PreviousSeverity = CurrentOOBSeverity;
		CurrentOOBSeverity = NewSeverity;

		OnOutOfBoundsChanged.Broadcast(NewSeverity, DistanceFromAO);

		if (NewSeverity > PreviousSeverity)
		{
			switch (NewSeverity)
			{
			case ESHOutOfBoundsSeverity::Warning:
				UE_LOG(LogSH_Streaming, Log, TEXT("OOB Warning: Player is %.0fcm outside AO"), DistanceFromAO);
				// "You're leaving the AO, Sergeant" — handled by radio message system via delegate
				break;
			case ESHOutOfBoundsSeverity::Urgent:
				UE_LOG(LogSH_Streaming, Warning, TEXT("OOB Urgent: Player is %.0fcm outside AO"), DistanceFromAO);
				break;
			case ESHOutOfBoundsSeverity::Critical:
				UE_LOG(LogSH_Streaming, Error, TEXT("OOB Critical: Player is %.0fcm outside AO — mission failure imminent"), DistanceFromAO);
				break;
			default:
				break;
			}
		}
	}
}

// ======================================================================
//  Predictive loading
// ======================================================================

void USHWorldStreamingManager::PredictiveLoad()
{
	if (PlayerVelocity.SizeSquared() < 100.f) return; // Standing still

	// Predict where the player will be in ~2 seconds
	FVector PredictedLocation = PlayerLocation + PlayerVelocity.GetSafeNormal() * PredictiveLoadAheadDistance;

	for (FSHStreamingCellInfo& Cell : CellInfos)
	{
		if (Cell.bIsLoaded) continue;

		float DistToPredicted = FVector::Dist(Cell.CellCenter, PredictedLocation);
		if (DistToPredicted < CriticalLoadRadius)
		{
			// Pre-emptively raise priority
			if (Cell.Priority > ESHStreamingPriority::High)
			{
				Cell.Priority = ESHStreamingPriority::High;
			}
		}
	}
}

// ======================================================================
//  Combat / squad integration
// ======================================================================

void USHWorldStreamingManager::RegisterCombatZone(FVector WorldLocation, float RadiusCM)
{
	ActiveCombatZones.Add(WorldLocation, FMath::Max(RadiusCM, CombatKeepLoadedRadius));
	UE_LOG(LogSH_Streaming, Log, TEXT("Combat zone registered at %s, radius %.0f"), *WorldLocation.ToString(), RadiusCM);
}

void USHWorldStreamingManager::UnregisterCombatZone(FVector WorldLocation)
{
	ActiveCombatZones.Remove(WorldLocation);
	UE_LOG(LogSH_Streaming, Log, TEXT("Combat zone unregistered at %s"), *WorldLocation.ToString());
}

void USHWorldStreamingManager::NotifySquadMemberPosition(int32 SquadMemberIndex, FVector WorldLocation)
{
	SquadMemberPositions.Add(SquadMemberIndex, WorldLocation);
}

// ======================================================================
//  LOD overrides
// ======================================================================

void USHWorldStreamingManager::ForceCellLOD(FName CellName, int32 LODLevel)
{
	ForcedLODOverrides.Add(CellName, LODLevel);
}

void USHWorldStreamingManager::ClearForcedLODs()
{
	ForcedLODOverrides.Empty();
}
