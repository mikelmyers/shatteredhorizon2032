// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHWorldStreamingManager.h"
#include "ShatteredHorizon2032/ShatteredHorizon2032.h"
#include "ShatteredHorizon2032/Core/SHGameState.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSH_World, Log, All);
DEFINE_LOG_CATEGORY(LogSH_World);

// ===================================================================
//  Constructor
// ===================================================================

ASHWorldStreamingManager::ASHWorldStreamingManager()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.0f; // Every frame for boundary checks

	FPSSamples.SetNum(FPS_SAMPLE_COUNT);
	for (int32 i = 0; i < FPS_SAMPLE_COUNT; ++i)
	{
		FPSSamples[i] = 60.f;
	}
}

// ===================================================================
//  Lifecycle
// ===================================================================

void ASHWorldStreamingManager::BeginPlay()
{
	Super::BeginPlay();

	CachedGameState = Cast<ASHGameState>(GetWorld()->GetGameState());

	UE_LOG(LogSH_World, Log, TEXT("WorldStreamingManager initialized. AO center: %s, extents: %.0f x %.0f UU"),
		*AOCenter.ToString(), AOHalfExtents.X * 2.f, AOHalfExtents.Y * 2.f);
}

void ASHWorldStreamingManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	UpdatePerformanceBudget(DeltaSeconds);
	UpdateBoundaryCheck(DeltaSeconds);

	PriorityUpdateAccumulator += DeltaSeconds;
	if (PriorityUpdateAccumulator >= PriorityUpdateInterval)
	{
		UpdateStreamingPriorities(PriorityUpdateAccumulator);
		PriorityUpdateAccumulator = 0.f;
	}

	ProcessStreamingQueue(DeltaSeconds);
	UpdateDistanceLOD(DeltaSeconds);
}

// ===================================================================
//  Public API
// ===================================================================

void ASHWorldStreamingManager::RegisterStreamingCell(const FGuid& CellGuid, const FVector& CellCenter)
{
	if (StreamingCells.Contains(CellGuid))
	{
		return;
	}

	FSHStreamingCellInfo CellInfo;
	CellInfo.CellGuid = CellGuid;
	CellInfo.CellCenter = CellCenter;
	CellInfo.Priority = ESHStreamingPriority::Low;
	StreamingCells.Add(CellGuid, CellInfo);

	UE_LOG(LogSH_World, Verbose, TEXT("Registered streaming cell %s at %s"),
		*CellGuid.ToString(), *CellCenter.ToString());
}

void ASHWorldStreamingManager::MarkCombatZone(const FGuid& CellGuid, bool bActive)
{
	if (FSHStreamingCellInfo* Cell = StreamingCells.Find(CellGuid))
	{
		Cell->bIsCombatZone = bActive;

		if (bActive && !Cell->bIsLoaded)
		{
			// Combat zones get immediate load
			ForceLoadCell(CellGuid);
		}

		UE_LOG(LogSH_World, Log, TEXT("Cell %s combat zone: %s"),
			*CellGuid.ToString(), bActive ? TEXT("ACTIVE") : TEXT("inactive"));
	}
}

void ASHWorldStreamingManager::ForceLoadCell(const FGuid& CellGuid)
{
	FSHStreamingCellInfo* Cell = StreamingCells.Find(CellGuid);
	if (!Cell)
	{
		return;
	}

	if (Cell->bIsLoaded)
	{
		return;
	}

	// In a full implementation this would call into the World Partition streaming source
	// to request the cell be loaded. For now we mark it and add to front of load queue.
	if (!LoadQueue.Contains(CellGuid))
	{
		LoadQueue.Insert(CellGuid, 0);
	}

	UE_LOG(LogSH_World, Log, TEXT("Force-load requested for cell %s"), *CellGuid.ToString());
}

void ASHWorldStreamingManager::ForceUnloadCell(const FGuid& CellGuid)
{
	FSHStreamingCellInfo* Cell = StreamingCells.Find(CellGuid);
	if (!Cell || !Cell->bIsLoaded)
	{
		return;
	}

	// Don't unload combat zones
	if (Cell->bIsCombatZone)
	{
		UE_LOG(LogSH_World, Warning, TEXT("Cannot force-unload combat zone cell %s"), *CellGuid.ToString());
		return;
	}

	if (!UnloadQueue.Contains(CellGuid))
	{
		UnloadQueue.Add(CellGuid);
	}
}

int32 ASHWorldStreamingManager::GetLODBiasForPosition(const FVector& WorldPosition) const
{
	const ASHPlayerCharacter* Player = const_cast<ASHWorldStreamingManager*>(this)->GetLocalPlayer();
	if (!Player)
	{
		return 0;
	}

	const float Distance = FVector::Dist(Player->GetActorLocation(), WorldPosition);

	for (int32 i = 0; i < LODDistanceThresholds.Num(); ++i)
	{
		if (Distance < LODDistanceThresholds[i])
		{
			return i;
		}
	}

	return LODDistanceThresholds.Num();
}

void ASHWorldStreamingManager::RegisterObjectiveLocation(const FGuid& ObjectiveId, const FVector& Location)
{
	ObjectiveLocations.Add(ObjectiveId, Location);
}

void ASHWorldStreamingManager::UnregisterObjectiveLocation(const FGuid& ObjectiveId)
{
	ObjectiveLocations.Remove(ObjectiveId);
}

// ===================================================================
//  Internal: Streaming Priorities
// ===================================================================

void ASHWorldStreamingManager::UpdateStreamingPriorities(float DeltaSeconds)
{
	const ASHPlayerCharacter* Player = GetLocalPlayer();
	if (!Player)
	{
		return;
	}

	const FVector PlayerLoc = Player->GetActorLocation();

	// Build sorted load/unload queues
	LoadQueue.Reset();
	UnloadQueue.Reset();

	for (auto& Pair : StreamingCells)
	{
		FSHStreamingCellInfo& Cell = Pair.Value;
		Cell.DistanceToPlayer = FVector::Dist(PlayerLoc, Cell.CellCenter);

		// Determine priority
		ESHStreamingPriority NewPriority;
		if (Cell.DistanceToPlayer < CriticalRadius)
		{
			NewPriority = ESHStreamingPriority::Critical;
		}
		else if (Cell.DistanceToPlayer < HighRadius || Cell.bIsCombatZone)
		{
			NewPriority = ESHStreamingPriority::High;
		}
		else if (Cell.DistanceToPlayer < MediumRadius)
		{
			NewPriority = ESHStreamingPriority::Medium;
		}
		else
		{
			NewPriority = ESHStreamingPriority::Low;
		}

		// Boost cells near objective locations
		for (const auto& ObjPair : ObjectiveLocations)
		{
			const float ObjDist = FVector::Dist(Cell.CellCenter, ObjPair.Value);
			if (ObjDist < HighRadius && NewPriority > ESHStreamingPriority::High)
			{
				NewPriority = ESHStreamingPriority::High;
				break;
			}
		}

		Cell.Priority = NewPriority;

		// Queue load/unload decisions
		if (!Cell.bIsLoaded && NewPriority <= ESHStreamingPriority::Medium)
		{
			LoadQueue.Add(Cell.CellGuid);
		}
		else if (Cell.bIsLoaded && NewPriority >= ESHStreamingPriority::Background && !Cell.bIsCombatZone)
		{
			// Only unload very distant non-combat cells
			UnloadQueue.Add(Cell.CellGuid);
		}

		if (Cell.bIsLoaded)
		{
			Cell.LoadedDuration += DeltaSeconds;
		}
	}

	// Sort load queue: Critical first, then by distance
	LoadQueue.Sort([this](const FGuid& A, const FGuid& B)
	{
		const FSHStreamingCellInfo& CellA = StreamingCells[A];
		const FSHStreamingCellInfo& CellB = StreamingCells[B];
		if (CellA.Priority != CellB.Priority)
		{
			return CellA.Priority < CellB.Priority; // Lower enum = higher priority
		}
		return CellA.DistanceToPlayer < CellB.DistanceToPlayer;
	});
}

// ===================================================================
//  Internal: Process Streaming Queue
// ===================================================================

void ASHWorldStreamingManager::ProcessStreamingQueue(float DeltaSeconds)
{
	// Throttle loads if budget is exceeded
	const int32 MaxLoads = PerformanceBudget.bBudgetExceeded ? 1 : MaxLoadsPerTick;

	int32 LoadsThisTick = 0;
	for (const FGuid& CellGuid : LoadQueue)
	{
		if (LoadsThisTick >= MaxLoads)
		{
			break;
		}

		FSHStreamingCellInfo* Cell = StreamingCells.Find(CellGuid);
		if (!Cell || Cell->bIsLoaded)
		{
			continue;
		}

		// In a production implementation, this issues an async load request to
		// the World Partition streaming source. The cell's actors and landscape
		// components are streamed in at the appropriate LOD level.
		Cell->bIsLoaded = true;
		Cell->LoadedDuration = 0.f;
		++LoadsThisTick;

		PerformanceBudget.LoadedCellCount++;

		UE_LOG(LogSH_World, Verbose, TEXT("Loading cell %s (priority: %d, dist: %.0f)"),
			*CellGuid.ToString(), static_cast<int32>(Cell->Priority), Cell->DistanceToPlayer);
	}

	// Process unloads (less aggressive to avoid pop-in)
	if (!PerformanceBudget.bBudgetExceeded)
	{
		return; // Only unload under budget pressure
	}

	for (const FGuid& CellGuid : UnloadQueue)
	{
		FSHStreamingCellInfo* Cell = StreamingCells.Find(CellGuid);
		if (!Cell || !Cell->bIsLoaded)
		{
			continue;
		}

		// Don't unload cells the player recently visited
		if (Cell->LoadedDuration < 30.f)
		{
			continue;
		}

		Cell->bIsLoaded = false;
		PerformanceBudget.LoadedCellCount = FMath::Max(0, PerformanceBudget.LoadedCellCount - 1);

		UE_LOG(LogSH_World, Verbose, TEXT("Unloading cell %s to reclaim budget"), *CellGuid.ToString());
	}
}

// ===================================================================
//  Internal: Performance Budget
// ===================================================================

void ASHWorldStreamingManager::UpdatePerformanceBudget(float DeltaSeconds)
{
	if (DeltaSeconds <= 0.f)
	{
		return;
	}

	// Sample FPS
	const float CurrentFPS = 1.f / DeltaSeconds;
	FPSSamples[FPSSampleIndex] = CurrentFPS;
	FPSSampleIndex = (FPSSampleIndex + 1) % FPS_SAMPLE_COUNT;

	// Compute rolling average
	float Sum = 0.f;
	for (int32 i = 0; i < FPS_SAMPLE_COUNT; ++i)
	{
		Sum += FPSSamples[i];
	}
	PerformanceBudget.CurrentFPS = Sum / static_cast<float>(FPS_SAMPLE_COUNT);
	PerformanceBudget.FrameTimeMs = 1000.f / FMath::Max(PerformanceBudget.CurrentFPS, 1.f);

	// Memory estimate (in production, query actual streaming memory usage)
	PerformanceBudget.StreamingMemoryMB = PerformanceBudget.LoadedCellCount * 64.f; // rough estimate

	// Pending loads
	PerformanceBudget.PendingLoadCount = 0;
	for (const auto& Pair : StreamingCells)
	{
		if (!Pair.Value.bIsLoaded && Pair.Value.Priority <= ESHStreamingPriority::Medium)
		{
			PerformanceBudget.PendingLoadCount++;
		}
	}

	// Budget check
	const bool bFPSBudgetExceeded = PerformanceBudget.CurrentFPS < (TargetFPS * 0.9f); // 10% grace
	const bool bMemoryBudgetExceeded = PerformanceBudget.StreamingMemoryMB > MaxStreamingMemoryMB;
	PerformanceBudget.bBudgetExceeded = bFPSBudgetExceeded || bMemoryBudgetExceeded;

	if (PerformanceBudget.bBudgetExceeded)
	{
		UE_LOG(LogSH_World, Warning, TEXT("Performance budget exceeded: FPS=%.1f (target=%.0f), Mem=%.0f MB (max=%.0f MB)"),
			PerformanceBudget.CurrentFPS, TargetFPS,
			PerformanceBudget.StreamingMemoryMB, MaxStreamingMemoryMB);
	}
}

// ===================================================================
//  Internal: Boundary Check (No Invisible Walls)
// ===================================================================

void ASHWorldStreamingManager::UpdateBoundaryCheck(float DeltaSeconds)
{
	const ASHPlayerCharacter* Player = GetLocalPlayer();
	if (!Player)
	{
		return;
	}

	const float BoundaryDist = ComputeBoundaryDistance(Player->GetActorLocation());
	ESHBoundaryStage NewStage = ESHBoundaryStage::InBounds;

	if (BoundaryDist > MissionFailDistance)
	{
		NewStage = ESHBoundaryStage::MissionFailed;
	}
	else if (BoundaryDist > FinalWarningDistance)
	{
		NewStage = ESHBoundaryStage::Final;
	}
	else if (BoundaryDist > WarningDistance)
	{
		NewStage = ESHBoundaryStage::Warning;
	}

	// Track time at final warning for countdown
	if (NewStage == ESHBoundaryStage::Final)
	{
		FinalWarningTimer += DeltaSeconds;
		if (FinalWarningTimer >= MissionFailCountdown)
		{
			NewStage = ESHBoundaryStage::MissionFailed;
		}
	}
	else
	{
		FinalWarningTimer = 0.f;
	}

	// Broadcast stage changes
	if (NewStage != CurrentBoundaryStage)
	{
		const ESHBoundaryStage PreviousStage = CurrentBoundaryStage;
		CurrentBoundaryStage = NewStage;
		BroadcastBoundaryWarning(NewStage);
		OnBoundaryStageChanged.Broadcast(NewStage);

		UE_LOG(LogSH_World, Log, TEXT("Boundary stage changed: %d -> %d (distance: %.0f UU)"),
			static_cast<int32>(PreviousStage), static_cast<int32>(NewStage), BoundaryDist);
	}
}

float ASHWorldStreamingManager::ComputeBoundaryDistance(const FVector& PlayerLocation) const
{
	// Compute signed distance from the AO rectangle.
	// Negative = inside, positive = outside.
	const FVector2D Offset(
		FMath::Abs(PlayerLocation.X - AOCenter.X) - AOHalfExtents.X,
		FMath::Abs(PlayerLocation.Y - AOCenter.Y) - AOHalfExtents.Y
	);

	// If both components are negative, the player is inside the AO
	if (Offset.X <= 0.f && Offset.Y <= 0.f)
	{
		return FMath::Max(Offset.X, Offset.Y); // Negative = inside
	}

	// Outside: compute distance from the nearest edge
	const FVector2D Clamped(FMath::Max(Offset.X, 0.f), FMath::Max(Offset.Y, 0.f));
	return Clamped.Size();
}

void ASHWorldStreamingManager::BroadcastBoundaryWarning(ESHBoundaryStage Stage)
{
	switch (Stage)
	{
	case ESHBoundaryStage::Warning:
		// In production: trigger radio VO line and subtitle
		UE_LOG(LogSH_World, Log, TEXT("RADIO: %s"), *WarningRadioLine.ToString());
		break;

	case ESHBoundaryStage::Final:
		UE_LOG(LogSH_World, Warning, TEXT("RADIO: %s"), *FinalWarningRadioLine.ToString());
		break;

	case ESHBoundaryStage::MissionFailed:
		UE_LOG(LogSH_World, Error, TEXT("Mission failed: player left operational area."));
		// In production: trigger mission failure through the game mode
		break;

	case ESHBoundaryStage::InBounds:
		UE_LOG(LogSH_World, Log, TEXT("Player returned to AO."));
		break;
	}
}

// ===================================================================
//  Internal: Distance LOD
// ===================================================================

void ASHWorldStreamingManager::UpdateDistanceLOD(float DeltaSeconds)
{
	// In production, this iterates visible streaming cells and adjusts
	// LOD bias on their landscape and static mesh components based on
	// distance tier. Distant structures use simplified meshes, terrain
	// uses lower tessellation, and foliage density is reduced.
	// This is called at the same cadence as priority updates.
}

// ===================================================================
//  Helpers
// ===================================================================

ASHPlayerCharacter* ASHWorldStreamingManager::GetLocalPlayer() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return nullptr;
	}

	return Cast<ASHPlayerCharacter>(PC->GetPawn());
}
