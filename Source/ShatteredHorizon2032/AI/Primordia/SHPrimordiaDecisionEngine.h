// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SHPrimordiaClient.h"
#include "SHPrimordiaDecisionEngine.generated.h"

class ASHEnemyAIController;
class ASHEnemyCharacter;

// -----------------------------------------------------------------------
//  Enums & structs
// -----------------------------------------------------------------------

/** Operational states for a squad-level tactical order. */
UENUM(BlueprintType)
enum class ESHTacticalOrderState : uint8
{
	Pending,
	InProgress,
	Stalled,
	Succeeded,
	Failed,
	Cancelled
};

/** Abstract squad-level tactical order derived from a Primordia directive. */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHTacticalOrder
{
	GENERATED_BODY()

	/** Unique order identifier. */
	UPROPERTY(BlueprintReadOnly)
	FGuid OrderId;

	/** Originating directive. */
	UPROPERTY(BlueprintReadOnly)
	FString SourceDirectiveId;

	/** Order type mirrored from the directive (attack, feint, hold, retreat, regroup, suppress). */
	UPROPERTY(BlueprintReadOnly)
	FString OrderType;

	/** Target world position. */
	UPROPERTY(BlueprintReadOnly)
	FVector TargetLocation = FVector::ZeroVector;

	/** Secondary/waypoint position. */
	UPROPERTY(BlueprintReadOnly)
	FVector SecondaryLocation = FVector::ZeroVector;

	/** Assigned squad ID. */
	UPROPERTY(BlueprintReadOnly)
	int32 SquadId = INDEX_NONE;

	/** Current state. */
	UPROPERTY(BlueprintReadOnly)
	ESHTacticalOrderState State = ESHTacticalOrderState::Pending;

	/** Game-time at which the order should begin executing. */
	UPROPERTY(BlueprintReadOnly)
	float ScheduledTime = 0.f;

	/** Time the order has been in InProgress. */
	UPROPERTY(BlueprintReadOnly)
	float ElapsedTime = 0.f;

	/** Max time to attempt before escalation / cancellation. */
	UPROPERTY(BlueprintReadOnly)
	float TimeoutSeconds = 120.f;

	/** Free-form parameters from the directive. */
	UPROPERTY(BlueprintReadOnly)
	TMap<FString, FString> Parameters;
};

/** Tracks the state of a single enemy squad for force allocation. */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHSquadAllocation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 SquadId = INDEX_NONE;

	/** Number of alive members. */
	UPROPERTY(BlueprintReadOnly)
	int32 ActiveMemberCount = 0;

	/** Maximum strength at spawn. */
	UPROPERTY(BlueprintReadOnly)
	int32 MaxMemberCount = 0;

	/** Whether this squad is currently assigned to an order. */
	UPROPERTY(BlueprintReadOnly)
	bool bAssigned = false;

	/** Whether this squad is held in reserve. */
	UPROPERTY(BlueprintReadOnly)
	bool bIsReserve = false;

	/** Current order being executed (if any). */
	UPROPERTY(BlueprintReadOnly)
	FGuid ActiveOrderId;

	/** Average morale across squad members (0..1). */
	UPROPERTY(BlueprintReadOnly)
	float AverageMorale = 1.0f;

	/** Strength ratio (alive / max). */
	float GetStrengthRatio() const { return MaxMemberCount > 0 ? static_cast<float>(ActiveMemberCount) / MaxMemberCount : 0.f; }
};

// -----------------------------------------------------------------------
//  Delegates
// -----------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSquadOrderIssued, int32, SquadId, const FSHTacticalOrder&, Order);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSquadOrderCompleted, int32, SquadId, ESHTacticalOrderState, FinalState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnReservesCommitted, int32, SquadId);

/**
 * USHPrimordiaDecisionEngine
 *
 * Translates high-level Primordia strategic directives into actionable
 * squad-level tactical orders.  Manages force allocation, timing
 * coordination for multi-axis assaults, reserve commitment, escalation,
 * and retreat/regroup decisions.
 */
UCLASS(BlueprintType, Blueprintable)
class SHATTEREDHORIZON2032_API USHPrimordiaDecisionEngine : public UObject
{
	GENERATED_BODY()

public:
	USHPrimordiaDecisionEngine();

	// ------------------------------------------------------------------
	//  Lifecycle
	// ------------------------------------------------------------------

	/** Initialize — call once after construction. */
	void Initialize();

	/** Tick the engine — drives order scheduling, escalation, and timeouts. */
	void Tick(float DeltaSeconds);

	// ------------------------------------------------------------------
	//  Directive ingestion (from PrimordiaClient)
	// ------------------------------------------------------------------

	/** Receive a batch of directives and convert to tactical orders. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|DecisionEngine")
	void IngestDirectiveBatch(const FSHPrimordiaDirectiveBatch& Batch);

	// ------------------------------------------------------------------
	//  Squad registration
	// ------------------------------------------------------------------

	/** Register a squad so the engine can allocate orders to it. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|DecisionEngine")
	void RegisterSquad(int32 SquadId, int32 MemberCount, bool bReserve = false);

	/** Remove a squad (e.g. wiped out). */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|DecisionEngine")
	void UnregisterSquad(int32 SquadId);

	/** Update the alive count for a squad. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|DecisionEngine")
	void UpdateSquadStrength(int32 SquadId, int32 AliveMemberCount, float AverageMorale);

	// ------------------------------------------------------------------
	//  Order queries
	// ------------------------------------------------------------------

	/** Get the active order for a given squad (returns false if none). */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|DecisionEngine")
	bool GetActiveOrderForSquad(int32 SquadId, FSHTacticalOrder& OutOrder) const;

	/** Get all currently active orders. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|DecisionEngine")
	TArray<FSHTacticalOrder> GetAllActiveOrders() const;

	/** Cancel an active order. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|DecisionEngine")
	void CancelOrder(const FGuid& OrderId);

	// ------------------------------------------------------------------
	//  Fallback / local AI
	// ------------------------------------------------------------------

	/**
	 * Generate local fallback orders when Primordia is disconnected.
	 * Produces simple "hold position" or "advance to nearest objective" orders.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|DecisionEngine")
	void GenerateLocalFallbackOrders();

	// ------------------------------------------------------------------
	//  Wave spawning interface
	// ------------------------------------------------------------------

	/**
	 * Called by the GameMode when a reinforcement wave spawns.
	 * The engine assigns newly spawned squads to the most urgent pending orders.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|DecisionEngine")
	void OnReinforcementWaveSpawned(const TArray<int32>& NewSquadIds);

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Time (seconds) an order can stall before escalation kicks in. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Primordia|Config")
	float StallThresholdSeconds = 30.f;

	/** Casualty ratio at which a squad's order is considered failed and retreat is issued. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Primordia|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CasualtyRetreatThreshold = 0.5f;

	/** Fraction of total forces to hold in reserve. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Primordia|Config", meta = (ClampMin = "0.0", ClampMax = "0.5"))
	float ReserveRatio = 0.2f;

	/** Morale threshold below which a squad is pulled back to regroup. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Primordia|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MoraleRetreatThreshold = 0.25f;

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Primordia|DecisionEngine")
	FOnSquadOrderIssued OnSquadOrderIssued;

	UPROPERTY(BlueprintAssignable, Category = "SH|Primordia|DecisionEngine")
	FOnSquadOrderCompleted OnSquadOrderCompleted;

	UPROPERTY(BlueprintAssignable, Category = "SH|Primordia|DecisionEngine")
	FOnReservesCommitted OnReservesCommitted;

private:
	// ------------------------------------------------------------------
	//  Internal
	// ------------------------------------------------------------------

	/** Convert a single directive into one or more squad-level orders. */
	void DecomposeDirective(const FSHPrimordiaTacticalDirective& Directive);

	/** Allocate an available squad to an order based on proximity & strength. */
	int32 AllocateSquadForOrder(const FSHTacticalOrder& Order);

	/** Issue an order to a specific squad (notifies controllers). */
	void IssueOrderToSquad(int32 SquadId, FSHTacticalOrder& Order);

	/** Check for stalled orders and escalate by committing reserves. */
	void TickEscalation(float DeltaSeconds);

	/** Check for squads that have broken and issue retreat orders. */
	void TickMoraleAndRetreat(float DeltaSeconds);

	/** Check for completed/timed-out orders. */
	void TickOrderLifecycle(float DeltaSeconds);

	/** Commit a reserve squad to reinforce a stalled attack. */
	void CommitReserve(int32 ReserveSquadId, const FSHTacticalOrder& StalledOrder);

	/** Find the nearest unassigned squad (optionally reserve-only). */
	int32 FindNearestAvailableSquad(FVector Location, bool bReserveOnly = false) const;

	/** Count unassigned squads (optionally including reserves). */
	int32 GetAvailableSquadCount(bool bIncludeReserves) const;

	/** Get a squad allocation by ID (mutable). */
	FSHSquadAllocation* FindSquadAllocation(int32 SquadId);
	const FSHSquadAllocation* FindSquadAllocation(int32 SquadId) const;

	// ------------------------------------------------------------------
	//  State
	// ------------------------------------------------------------------

	/** All registered squads. */
	UPROPERTY()
	TArray<FSHSquadAllocation> SquadAllocations;

	/** Active and pending tactical orders. */
	TArray<FSHTacticalOrder> ActiveOrders;

	/** Queue of orders waiting for squad assignment. */
	TArray<FSHTacticalOrder> PendingOrders;

	/** Current game time for scheduling. */
	float CurrentGameTime = 0.f;
};
