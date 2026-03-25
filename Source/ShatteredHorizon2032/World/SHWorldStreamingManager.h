// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SHWorldStreamingManager.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSH_Streaming, Log, All);

/** Streaming priority tier — determines load/unload order. */
UENUM(BlueprintType)
enum class ESHStreamingPriority : uint8
{
	Critical	UMETA(DisplayName = "Critical"),		// Player immediate area, always loaded
	High		UMETA(DisplayName = "High"),			// Combat areas, squad positions
	Medium		UMETA(DisplayName = "Medium"),			// Nearby terrain visible to player
	Low			UMETA(DisplayName = "Low"),				// Distant LOD terrain
	Background	UMETA(DisplayName = "Background")		// Far horizon, ocean
};

/** Out-of-bounds severity — escalates with distance from AO. */
UENUM(BlueprintType)
enum class ESHOutOfBoundsSeverity : uint8
{
	None		UMETA(DisplayName = "None"),
	Warning		UMETA(DisplayName = "Warning"),			// Radio reminder
	Urgent		UMETA(DisplayName = "Urgent"),			// Repeated warnings, objective marker
	Critical	UMETA(DisplayName = "Critical")			// Mission failure imminent
};

/** Runtime info about a streaming cell in the operational area. */
USTRUCT(BlueprintType)
struct FSHStreamingCellInfo
{
	GENERATED_BODY()

	/** Unique cell identifier from World Partition. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FName CellName;

	/** Cell center in world space. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector CellCenter = FVector::ZeroVector;

	/** Current streaming priority. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	ESHStreamingPriority Priority = ESHStreamingPriority::Medium;

	/** Distance from cell center to the player in cm. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float DistanceToPlayer = 0.f;

	/** Whether the cell is currently loaded. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bIsLoaded = false;

	/** Whether active combat is occurring in this cell. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bHasActiveCombat = false;

	/** Whether any squad members are present in this cell. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bHasSquadPresence = false;

	/** LOD level currently applied (0 = full, higher = reduced). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 CurrentLOD = 0;
};

/** Performance budget snapshot. */
USTRUCT(BlueprintType)
struct FSHPerformanceBudget
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float CurrentFPS = 60.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float TargetFPS = 60.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float FrameTimeMS = 16.67f;

	/** Memory used by streaming actors in MB. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float StreamingMemoryMB = 0.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float StreamingMemoryBudgetMB = 2048.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 LoadedCellCount = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 TotalCellCount = 0;

	/** True when we are under budget pressure and should shed load. */
	bool IsOverBudget() const { return CurrentFPS < TargetFPS * 0.9f || StreamingMemoryMB > StreamingMemoryBudgetMB * 0.95f; }
};

/** Delegate fired when out-of-bounds severity changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnOutOfBoundsChanged, ESHOutOfBoundsSeverity, NewSeverity, float, DistanceFromAO);

/** Delegate fired when performance budget threshold is crossed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPerformanceBudgetWarning, const FSHPerformanceBudget&, Budget);

/**
 * USHWorldStreamingManager
 *
 * World subsystem that manages open-world streaming for the Taoyuan Beach
 * operational area (~4km x 4km). Leverages UE5 World Partition and adds
 * gameplay-aware prioritisation: combat zones stay loaded, the player's
 * heading drives predictive loading, and a performance governor dynamically
 * adjusts LOD and cell count to maintain the 60fps target.
 *
 * There are NO invisible walls. The map is bounded by ocean to the west and
 * continuous terrain elsewhere. Out-of-bounds handling is purely narrative
 * (radio warnings) escalating to mission failure only at extreme range.
 */
UCLASS()
class SHATTEREDHORIZON2032_API USHWorldStreamingManager : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	USHWorldStreamingManager();

	// ------------------------------------------------------------------
	//  USubsystem interface
	// ------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// ------------------------------------------------------------------
	//  Query
	// ------------------------------------------------------------------

	/** Get current performance budget snapshot. */
	UFUNCTION(BlueprintPure, Category = "SH|Streaming")
	FSHPerformanceBudget GetPerformanceBudget() const { return PerformanceBudget; }

	/** Get the current out-of-bounds severity for the local player. */
	UFUNCTION(BlueprintPure, Category = "SH|Streaming")
	ESHOutOfBoundsSeverity GetOutOfBoundsSeverity() const { return CurrentOOBSeverity; }

	/** Distance from local player to the nearest AO boundary edge (cm). Returns 0 when inside. */
	UFUNCTION(BlueprintPure, Category = "SH|Streaming")
	float GetDistanceFromAO() const { return DistanceFromAO; }

	/** Get information about all tracked streaming cells. */
	UFUNCTION(BlueprintPure, Category = "SH|Streaming")
	const TArray<FSHStreamingCellInfo>& GetCellInfo() const { return CellInfos; }

	// ------------------------------------------------------------------
	//  Combat / squad integration
	// ------------------------------------------------------------------

	/** Mark a world location as having active combat — keeps cells loaded. */
	UFUNCTION(BlueprintCallable, Category = "SH|Streaming")
	void RegisterCombatZone(FVector WorldLocation, float RadiusCM);

	/** Remove a previously registered combat zone. */
	UFUNCTION(BlueprintCallable, Category = "SH|Streaming")
	void UnregisterCombatZone(FVector WorldLocation);

	/** Notify that a squad member is at this location — raises priority. */
	UFUNCTION(BlueprintCallable, Category = "SH|Streaming")
	void NotifySquadMemberPosition(int32 SquadMemberIndex, FVector WorldLocation);

	// ------------------------------------------------------------------
	//  LOD overrides
	// ------------------------------------------------------------------

	/** Force a LOD level for a cell by name (debug / cinematic use). */
	UFUNCTION(BlueprintCallable, Category = "SH|Streaming")
	void ForceCellLOD(FName CellName, int32 LODLevel);

	/** Clear all forced LOD overrides. */
	UFUNCTION(BlueprintCallable, Category = "SH|Streaming")
	void ClearForcedLODs();

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Streaming")
	FOnOutOfBoundsChanged OnOutOfBoundsChanged;

	UPROPERTY(BlueprintAssignable, Category = "SH|Streaming")
	FOnPerformanceBudgetWarning OnPerformanceBudgetWarning;

protected:
	// ------------------------------------------------------------------
	//  Tick helpers
	// ------------------------------------------------------------------
	void UpdatePlayerPosition();
	void UpdateCellPriorities();
	void UpdateStreamingRequests();
	void UpdateLODs();
	void UpdatePerformanceBudget(float DeltaTime);
	void UpdateOutOfBounds();
	void PredictiveLoad();

	/** Compute the streaming priority for a cell based on gameplay state. */
	ESHStreamingPriority ComputeCellPriority(const FSHStreamingCellInfo& Cell) const;

	/** Compute the LOD level for a cell. */
	int32 ComputeCellLOD(const FSHStreamingCellInfo& Cell) const;

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** AO bounding box — min corner in world units. The area within which the player is "in bounds". */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Streaming|AO")
	FVector AOBoundsMin = FVector(-200000.f, -200000.f, -50000.f);

	/** AO bounding box — max corner. ~4km x 4km x 1km vertical. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Streaming|AO")
	FVector AOBoundsMax = FVector(200000.f, 200000.f, 50000.f);

	/** Distance outside AO before first radio warning (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Streaming|OOB")
	float OOBWarningDistance = 5000.f;

	/** Distance outside AO before urgent warnings (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Streaming|OOB")
	float OOBUrgentDistance = 15000.f;

	/** Distance outside AO that triggers mission failure (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Streaming|OOB")
	float OOBCriticalDistance = 30000.f;

	/** Radius around the player that is always fully loaded (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Streaming|Distances")
	float CriticalLoadRadius = 15000.f;

	/** Radius for high-priority loading (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Streaming|Distances")
	float HighLoadRadius = 40000.f;

	/** Maximum radius for any streaming (cm). Beyond this, cells unload. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Streaming|Distances")
	float MaxStreamingRadius = 120000.f;

	/** Radius around a combat zone that keeps cells loaded (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Streaming|Combat")
	float CombatKeepLoadedRadius = 20000.f;

	/** FPS target — the streaming governor adjusts load to meet this. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Streaming|Performance")
	float TargetFPS = 60.f;

	/** Maximum streaming memory budget in MB. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Streaming|Performance")
	float MaxStreamingMemoryMB = 2048.f;

	/** How far ahead (cm) to predict loading based on player velocity. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Streaming|Prediction")
	float PredictiveLoadAheadDistance = 10000.f;

	/** LOD distance thresholds — index is LOD level, value is min distance (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Streaming|LOD")
	TArray<float> LODDistanceThresholds = { 0.f, 20000.f, 50000.f, 80000.f };

private:
	// Runtime state -------------------------------------------------------

	/** Cached local player location. */
	FVector PlayerLocation = FVector::ZeroVector;

	/** Cached local player velocity for predictive loading. */
	FVector PlayerVelocity = FVector::ZeroVector;

	/** Current distance outside AO (0 if inside). */
	float DistanceFromAO = 0.f;

	/** Current OOB severity. */
	ESHOutOfBoundsSeverity CurrentOOBSeverity = ESHOutOfBoundsSeverity::None;

	/** Performance metrics. */
	FSHPerformanceBudget PerformanceBudget;

	/** FPS averaging buffer. */
	TArray<float> FPSHistory;
	static constexpr int32 FPSHistorySize = 30;

	/** All tracked streaming cells. */
	UPROPERTY()
	TArray<FSHStreamingCellInfo> CellInfos;

	/** Active combat zones: center -> radius. */
	TMap<FVector, float> ActiveCombatZones;

	/** Squad member positions indexed by member index. */
	TMap<int32, FVector> SquadMemberPositions;

	/** Forced LOD overrides: cell name -> LOD level. */
	TMap<FName, int32> ForcedLODOverrides;

	/** Timer handle for OOB radio warning cooldown. */
	FTimerHandle OOBWarningTimerHandle;

	/** Whether we've already fired an OOB warning recently. */
	bool bOOBWarningOnCooldown = false;

	/** Accumulator for staggering cell updates across frames. */
	int32 CellUpdateCursor = 0;

	/** How many cells to update per frame to spread the cost. */
	static constexpr int32 CellsPerFrameUpdate = 16;
};
