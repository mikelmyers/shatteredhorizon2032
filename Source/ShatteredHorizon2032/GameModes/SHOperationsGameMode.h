// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SHGameMode.h"
#include "SHOperationsGameMode.generated.h"

class USHObjectiveSystem;
class USHPrimordiaDecisionEngine;
class ASHGameState;

// =========================================================================
//  Enums
// =========================================================================

/** High-level phases of an Operations mission. */
UENUM(BlueprintType)
enum class ESHOperationsPhase : uint8
{
	Setup        UMETA(DisplayName = "Setup"),
	Assault      UMETA(DisplayName = "Assault"),
	Consolidate  UMETA(DisplayName = "Consolidate"),
	Extract      UMETA(DisplayName = "Extract")
};

/** Ownership state of a map sector. */
UENUM(BlueprintType)
enum class ESHSectorState : uint8
{
	Friendly    UMETA(DisplayName = "Friendly"),
	Contested   UMETA(DisplayName = "Contested"),
	Enemy       UMETA(DisplayName = "Enemy"),
	Neutral     UMETA(DisplayName = "Neutral")
};

// =========================================================================
//  Structs
// =========================================================================

/** Represents one controllable sector of the operations map. */
USTRUCT(BlueprintType)
struct FSHSector
{
	GENERATED_BODY()

	/** Human-readable sector identifier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Operations")
	FName Name;

	/** World-space center of the sector. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Operations")
	FVector Location = FVector::ZeroVector;

	/** Radius defining the sector boundary (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Operations", meta = (ClampMin = "0.0"))
	float Radius = 5000.f;

	/** Current ownership state. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Operations")
	ESHSectorState State = ESHSectorState::Neutral;

	/** Estimated attacking force strength in the sector (0..1 normalized). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Operations", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AttackerStrength = 0.f;

	/** Estimated defending force strength in the sector (0..1 normalized). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Operations", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DefenderStrength = 0.f;

	/** Objective linked to this sector (invalid GUID = no linked objective). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Operations")
	FGuid LinkedObjectiveId;

	/** Score accumulated from holding / capturing this sector. */
	UPROPERTY(BlueprintReadOnly, Category = "SH|Operations")
	float SectorScore = 0.f;
};

/** Snapshot of per-sector score contribution. */
USTRUCT(BlueprintType)
struct FSHOperationScoreEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SH|Operations")
	FName SectorName;

	UPROPERTY(BlueprintReadOnly, Category = "SH|Operations")
	float Score = 0.f;
};

// =========================================================================
//  Delegates
// =========================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnSectorStateChanged,
	FName, SectorName,
	ESHSectorState, OldState,
	ESHSectorState, NewState);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnOperationsPhaseChanged,
	ESHOperationsPhase, OldPhase,
	ESHOperationsPhase, NewPhase);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnOperationEnded,
	bool, bVictory);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnOperationScoreChanged,
	float, NewTotalScore,
	const FSHOperationScoreEntry&, LatestEntry);

// =========================================================================
//  ASHOperationsGameMode
// =========================================================================

/**
 * ASHOperationsGameMode
 *
 * Large-scale, objective-based PvE game mode in which the player squad
 * must capture and hold a series of map sectors while the Primordia AI
 * dynamically allocates enemy forces to contested areas.
 *
 * The mission flows through four phases:
 *   Setup       – pre-mission planning; objectives are generated.
 *   Assault     – active combat to capture enemy-held sectors.
 *   Consolidate – defend captured sectors against counter-attacks.
 *   Extract     – move to the extraction point under pressure.
 *
 * Victory requires completing all primary objectives.
 * Defeat occurs if all sectors are lost or the operation timer expires.
 */
UCLASS()
class SHATTEREDHORIZON2032_API ASHOperationsGameMode : public ASHGameMode
{
	GENERATED_BODY()

public:
	ASHOperationsGameMode();

	// ------------------------------------------------------------------
	//  AGameModeBase overrides
	// ------------------------------------------------------------------
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ------------------------------------------------------------------
	//  Phase management
	// ------------------------------------------------------------------

	/** Force-advance to a specific operations phase (authority only). */
	UFUNCTION(BlueprintCallable, Category = "SH|Operations")
	void AdvanceToOperationsPhase(ESHOperationsPhase NewPhase);

	/** Query the current operations phase. */
	UFUNCTION(BlueprintPure, Category = "SH|Operations")
	ESHOperationsPhase GetOperationsPhase() const { return OperationsPhase; }

	/** Elapsed time in the current operations phase (seconds). */
	UFUNCTION(BlueprintPure, Category = "SH|Operations")
	float GetOperationsPhaseElapsedTime() const { return OperationsPhaseElapsedTime; }

	/** Total elapsed operation time (seconds). */
	UFUNCTION(BlueprintPure, Category = "SH|Operations")
	float GetTotalOperationTime() const { return TotalOperationTime; }

	// ------------------------------------------------------------------
	//  Sector management
	// ------------------------------------------------------------------

	/** Register a new sector with the operations system. Returns the index. */
	UFUNCTION(BlueprintCallable, Category = "SH|Operations")
	int32 AddSector(const FSHSector& Sector);

	/** Get the current state of a sector by name. */
	UFUNCTION(BlueprintPure, Category = "SH|Operations")
	ESHSectorState GetSectorState(FName SectorName) const;

	/** Get a read-only copy of a sector by name. Returns false if not found. */
	UFUNCTION(BlueprintCallable, Category = "SH|Operations")
	bool GetSector(FName SectorName, FSHSector& OutSector) const;

	/** Retrieve all registered sectors. */
	UFUNCTION(BlueprintPure, Category = "SH|Operations")
	const TArray<FSHSector>& GetAllSectors() const { return Sectors; }

	/** Manually set a sector's ownership state (e.g. from scripted events). */
	UFUNCTION(BlueprintCallable, Category = "SH|Operations")
	void SetSectorState(FName SectorName, ESHSectorState NewState);

	/** Get the number of sectors in a given state. */
	UFUNCTION(BlueprintPure, Category = "SH|Operations")
	int32 GetSectorCountByState(ESHSectorState State) const;

	// ------------------------------------------------------------------
	//  Score
	// ------------------------------------------------------------------

	/** Total operation score across all sectors. */
	UFUNCTION(BlueprintPure, Category = "SH|Operations")
	float GetTotalOperationScore() const { return TotalOperationScore; }

	/** Per-sector score breakdown. */
	UFUNCTION(BlueprintPure, Category = "SH|Operations")
	TArray<FSHOperationScoreEntry> GetScoreBreakdown() const;

	// ------------------------------------------------------------------
	//  Victory / defeat
	// ------------------------------------------------------------------

	/** Whether the operation has ended (victory or defeat). */
	UFUNCTION(BlueprintPure, Category = "SH|Operations")
	bool IsOperationComplete() const { return bOperationEnded; }

	/** True if the operation ended in victory. Only valid when IsOperationComplete() is true. */
	UFUNCTION(BlueprintPure, Category = "SH|Operations")
	bool IsVictory() const { return bVictory; }

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	/** Broadcast when any sector changes ownership state. */
	UPROPERTY(BlueprintAssignable, Category = "SH|Operations")
	FOnSectorStateChanged OnSectorStateChanged;

	/** Broadcast when the operations phase changes. */
	UPROPERTY(BlueprintAssignable, Category = "SH|Operations")
	FOnOperationsPhaseChanged OnOperationsPhaseChanged;

	/** Broadcast when the operation ends (victory or defeat). */
	UPROPERTY(BlueprintAssignable, Category = "SH|Operations")
	FOnOperationEnded OnOperationEnded;

	/** Broadcast when the operation score changes. */
	UPROPERTY(BlueprintAssignable, Category = "SH|Operations")
	FOnOperationScoreChanged OnOperationScoreChanged;

protected:
	// ------------------------------------------------------------------
	//  Internal tick helpers
	// ------------------------------------------------------------------

	/** Evaluate sector force balances and update ownership states. */
	void TickSectors(float DeltaSeconds);

	/** Evaluate auto-advance conditions for the current operations phase. */
	void TickOperationsPhaseLogic(float DeltaSeconds);

	/** Request the AI commander to allocate forces to contested sectors. */
	void TickAIForceAllocation(float DeltaSeconds);

	/** Accumulate sector-based scores for held friendly sectors. */
	void TickScoring(float DeltaSeconds);

	/** Check victory and defeat conditions. */
	void EvaluateEndConditions();

	// ------------------------------------------------------------------
	//  Phase transition helpers
	// ------------------------------------------------------------------

	/** True if the current phase should auto-advance. */
	bool ShouldAdvanceOperationsPhase() const;

	/** Build objectives for the current phase via the objective system. */
	void GeneratePhaseObjectives(ESHOperationsPhase Phase);

	// ------------------------------------------------------------------
	//  Sector helpers
	// ------------------------------------------------------------------

	/** Find a mutable sector by name. Returns nullptr if not found. */
	FSHSector* FindSectorMutable(FName SectorName);

	/** Find a const sector by name. Returns nullptr if not found. */
	const FSHSector* FindSector(FName SectorName) const;

	/** Recalculate a sector's state based on force strengths. */
	void RecalculateSectorState(FSHSector& Sector);

	/** End the operation with a result. */
	void EndOperation(bool bIsVictory);

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Duration of the Setup phase before auto-advancing to Assault (seconds). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Operations|Phase")
	float SetupPhaseDuration = 120.f;

	/**
	 * Fraction of sectors that must be Friendly to auto-advance from
	 * Assault to Consolidate.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Operations|Phase", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AssaultCompletionSectorRatio = 0.75f;

	/** Duration to hold sectors during Consolidate before extraction (seconds). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Operations|Phase")
	float ConsolidateHoldDuration = 180.f;

	/** Time limit for the Extract phase (seconds). Failure if exceeded. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Operations|Phase")
	float ExtractTimeLimit = 300.f;

	/** Global operation time limit (seconds). 0 = no limit. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Operations|Phase")
	float OperationTimeLimit = 3600.f;

	/** Force strength delta at which a sector becomes contested (0..1). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Operations|Sectors", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ContestedThreshold = 0.15f;

	/** Force strength ratio above which a side takes ownership. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Operations|Sectors", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OwnershipThreshold = 0.6f;

	/** Points per second awarded for each friendly-held sector. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Operations|Score")
	float FriendlySectorScoreRate = 10.f;

	/** Bonus score for capturing an enemy sector. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Operations|Score")
	float SectorCaptureBonus = 500.f;

	/** Interval at which the AI commander re-evaluates force allocation (seconds). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Operations|AI")
	float AIAllocationInterval = 8.f;

	/** Sectors defined in the map (can be populated in editor or at runtime). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Operations|Sectors")
	TArray<FSHSector> Sectors;

private:
	// ------------------------------------------------------------------
	//  Runtime state
	// ------------------------------------------------------------------

	/** Current operations phase. */
	UPROPERTY()
	ESHOperationsPhase OperationsPhase = ESHOperationsPhase::Setup;

	/** Elapsed time in the current operations phase. */
	float OperationsPhaseElapsedTime = 0.f;

	/** Total elapsed operation time. */
	float TotalOperationTime = 0.f;

	/** Cumulative operation score. */
	float TotalOperationScore = 0.f;

	/** Whether the operation has ended. */
	bool bOperationEnded = false;

	/** True if the operation ended in victory. */
	bool bVictory = false;

	/** AI force allocation tick accumulator. */
	float AIAllocationAccumulator = 0.f;

	/** Cached objective system subsystem pointer. */
	UPROPERTY()
	TObjectPtr<USHObjectiveSystem> ObjectiveSystem = nullptr;

	/** Cached game state pointer. */
	UPROPERTY()
	TObjectPtr<ASHGameState> CachedGameState = nullptr;

	/** Number of primary objectives generated for the current phase. */
	int32 PrimaryObjectiveCount = 0;
};
