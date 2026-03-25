// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"
#include "SHWorldStreamingManager.generated.h"

class ASHGameState;
class ASHPlayerCharacter;

/** Priority tiers for streaming cells. */
UENUM(BlueprintType)
enum class ESHStreamingPriority : uint8
{
	Critical	UMETA(DisplayName = "Critical"),		// Player immediate vicinity
	High		UMETA(DisplayName = "High"),			// Combat zones, objective areas
	Medium		UMETA(DisplayName = "Medium"),			// Nearby explorable terrain
	Low			UMETA(DisplayName = "Low"),				// Distant landscape / LOD only
	Background	UMETA(DisplayName = "Background")		// Far horizon, ocean
};

/** Out-of-bounds escalation stage. */
UENUM(BlueprintType)
enum class ESHBoundaryStage : uint8
{
	InBounds		UMETA(DisplayName = "In Bounds"),
	Warning			UMETA(DisplayName = "Warning"),			// Radio warning
	Final			UMETA(DisplayName = "Final Warning"),	// Mission abort countdown
	MissionFailed	UMETA(DisplayName = "Mission Failed")	// Extreme range
};

/** Runtime info about a single streaming cell. */
USTRUCT(BlueprintType)
struct FSHStreamingCellInfo
{
	GENERATED_BODY()

	/** World Partition cell GUID. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FGuid CellGuid;

	/** Cell center in world space. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector CellCenter = FVector::ZeroVector;

	/** Distance from player (updated every priority pass). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float DistanceToPlayer = 0.f;

	/** Current streaming priority. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	ESHStreamingPriority Priority = ESHStreamingPriority::Low;

	/** True if this cell contains an active combat encounter. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bIsCombatZone = false;

	/** True if the cell is currently loaded. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bIsLoaded = false;

	/** Time this cell has been loaded (seconds). */
	float LoadedDuration = 0.f;
};

/** Performance snapshot for budget monitoring. */
USTRUCT(BlueprintType)
struct FSHPerformanceBudget
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float CurrentFPS = 60.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float FrameTimeMs = 16.67f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 LoadedCellCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 PendingLoadCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float StreamingMemoryMB = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bBudgetExceeded = false;
};

/**
 * ASHWorldStreamingManager
 *
 * Manages open-world streaming for the Taoyuan Beach ~4km x 4km operational area.
 * Leverages UE5 World Partition with custom priority logic:
 *   - Player-proximate cells load first
 *   - Active combat zones remain loaded regardless of distance
 *   - Distant terrain uses aggressive LOD
 *   - NO invisible walls; out-of-bounds uses escalating radio warnings
 *   - Performance budget enforcement maintains 60fps target
 */
UCLASS()
class SHATTEREDHORIZON2032_API ASHWorldStreamingManager : public AActor
{
	GENERATED_BODY()

public:
	ASHWorldStreamingManager();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ------------------------------------------------------------------
	//  Public API
	// ------------------------------------------------------------------

	/** Register a cell for managed streaming. Called during level initialization. */
	UFUNCTION(BlueprintCallable, Category = "SH|World")
	void RegisterStreamingCell(const FGuid& CellGuid, const FVector& CellCenter);

	/** Mark a cell as containing active combat (keeps it loaded). */
	UFUNCTION(BlueprintCallable, Category = "SH|World")
	void MarkCombatZone(const FGuid& CellGuid, bool bActive);

	/** Force-load a specific cell immediately. */
	UFUNCTION(BlueprintCallable, Category = "SH|World")
	void ForceLoadCell(const FGuid& CellGuid);

	/** Force-unload a specific cell (use with caution). */
	UFUNCTION(BlueprintCallable, Category = "SH|World")
	void ForceUnloadCell(const FGuid& CellGuid);

	/** Get the current out-of-bounds stage for the local player. */
	UFUNCTION(BlueprintPure, Category = "SH|World")
	ESHBoundaryStage GetBoundaryStage() const { return CurrentBoundaryStage; }

	/** Get current performance budget snapshot. */
	UFUNCTION(BlueprintPure, Category = "SH|World")
	const FSHPerformanceBudget& GetPerformanceBudget() const { return PerformanceBudget; }

	/** Query the LOD bias for a given world position (0 = full detail, higher = more aggressive LOD). */
	UFUNCTION(BlueprintPure, Category = "SH|World")
	int32 GetLODBiasForPosition(const FVector& WorldPosition) const;

	/** Register an objective location that should maintain higher streaming priority. */
	UFUNCTION(BlueprintCallable, Category = "SH|World")
	void RegisterObjectiveLocation(const FGuid& ObjectiveId, const FVector& Location);

	/** Remove an objective location when completed or failed. */
	UFUNCTION(BlueprintCallable, Category = "SH|World")
	void UnregisterObjectiveLocation(const FGuid& ObjectiveId);

	/** Delegate broadcast when boundary stage changes. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBoundaryStageChanged, ESHBoundaryStage, NewStage);
	UPROPERTY(BlueprintAssignable, Category = "SH|World")
	FOnBoundaryStageChanged OnBoundaryStageChanged;

protected:
	// ------------------------------------------------------------------
	//  Internal tick helpers
	// ------------------------------------------------------------------

	/** Re-evaluate streaming priorities based on player position and combat zones. */
	void UpdateStreamingPriorities(float DeltaSeconds);

	/** Process the load/unload queue based on priorities and budget. */
	void ProcessStreamingQueue(float DeltaSeconds);

	/** Sample FPS and memory, adjust streaming aggressiveness if needed. */
	void UpdatePerformanceBudget(float DeltaSeconds);

	/** Evaluate player distance from AO boundaries, issue warnings. */
	void UpdateBoundaryCheck(float DeltaSeconds);

	/** Update LOD settings for distant terrain and structures. */
	void UpdateDistanceLOD(float DeltaSeconds);

	/** Get the local player character. */
	ASHPlayerCharacter* GetLocalPlayer() const;

	/** Compute boundary distance (negative = inside AO, positive = outside). */
	float ComputeBoundaryDistance(const FVector& PlayerLocation) const;

	/** Issue a radio warning for out-of-bounds. */
	void BroadcastBoundaryWarning(ESHBoundaryStage Stage);

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Operational area center in world space. */
	UPROPERTY(EditAnywhere, Category = "SH|World|Boundary")
	FVector AOCenter = FVector(0.f, 0.f, 0.f);

	/** Operational area half-extents (X/Y). ~4km x 4km = 200000 x 200000 UU. */
	UPROPERTY(EditAnywhere, Category = "SH|World|Boundary")
	FVector2D AOHalfExtents = FVector2D(200000.f, 200000.f);

	/** Distance beyond the AO edge before the first radio warning (UU). */
	UPROPERTY(EditAnywhere, Category = "SH|World|Boundary")
	float WarningDistance = 5000.f;

	/** Distance beyond the AO edge for the final warning (UU). */
	UPROPERTY(EditAnywhere, Category = "SH|World|Boundary")
	float FinalWarningDistance = 15000.f;

	/** Distance beyond the AO edge for mission failure (UU). */
	UPROPERTY(EditAnywhere, Category = "SH|World|Boundary")
	float MissionFailDistance = 30000.f;

	/** Time in seconds at final warning before mission failure triggers. */
	UPROPERTY(EditAnywhere, Category = "SH|World|Boundary")
	float MissionFailCountdown = 15.f;

	/** Radio warning line for initial out-of-bounds. */
	UPROPERTY(EditAnywhere, Category = "SH|World|Boundary")
	FText WarningRadioLine = NSLOCTEXT("SHWorld", "BoundaryWarn", "You're leaving the AO, Sergeant. Turn back.");

	/** Radio warning line for final out-of-bounds. */
	UPROPERTY(EditAnywhere, Category = "SH|World|Boundary")
	FText FinalWarningRadioLine = NSLOCTEXT("SHWorld", "BoundaryFinal", "Sergeant! You are well outside the operational area. Return immediately or the mission is scrubbed.");

	/** Target frame rate for performance budget. */
	UPROPERTY(EditAnywhere, Category = "SH|World|Performance")
	float TargetFPS = 60.f;

	/** Maximum streaming memory budget in MB. */
	UPROPERTY(EditAnywhere, Category = "SH|World|Performance")
	float MaxStreamingMemoryMB = 4096.f;

	/** Maximum number of cells to load per tick. */
	UPROPERTY(EditAnywhere, Category = "SH|World|Performance")
	int32 MaxLoadsPerTick = 2;

	/** Radius around the player where cells are Critical priority (UU). */
	UPROPERTY(EditAnywhere, Category = "SH|World|Streaming")
	float CriticalRadius = 15000.f;

	/** Radius for High priority (UU). */
	UPROPERTY(EditAnywhere, Category = "SH|World|Streaming")
	float HighRadius = 40000.f;

	/** Radius for Medium priority (UU). */
	UPROPERTY(EditAnywhere, Category = "SH|World|Streaming")
	float MediumRadius = 80000.f;

	/** How often to recalculate streaming priorities (seconds). */
	UPROPERTY(EditAnywhere, Category = "SH|World|Streaming")
	float PriorityUpdateInterval = 0.5f;

	/** LOD bias applied per distance tier. */
	UPROPERTY(EditAnywhere, Category = "SH|World|LOD")
	TArray<float> LODDistanceThresholds = { 15000.f, 40000.f, 80000.f, 150000.f };

private:
	/** All registered streaming cells. */
	UPROPERTY()
	TMap<FGuid, FSHStreamingCellInfo> StreamingCells;

	/** Cells queued for loading, sorted by priority. */
	TArray<FGuid> LoadQueue;

	/** Cells queued for unloading. */
	TArray<FGuid> UnloadQueue;

	/** Objective locations that boost nearby cell priority. */
	TMap<FGuid, FVector> ObjectiveLocations;

	/** Current boundary stage. */
	ESHBoundaryStage CurrentBoundaryStage = ESHBoundaryStage::InBounds;

	/** Time spent at final warning stage (for countdown). */
	float FinalWarningTimer = 0.f;

	/** Accumulated time since last priority update. */
	float PriorityUpdateAccumulator = 0.f;

	/** Performance budget snapshot. */
	FSHPerformanceBudget PerformanceBudget;

	/** FPS sampling ring buffer. */
	TArray<float> FPSSamples;
	int32 FPSSampleIndex = 0;
	static constexpr int32 FPS_SAMPLE_COUNT = 30;

	/** Cached game state reference. */
	UPROPERTY()
	TObjectPtr<ASHGameState> CachedGameState = nullptr;
};
