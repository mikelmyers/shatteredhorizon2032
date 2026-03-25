// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "SHAIPerceptionConfig.h"
#include "Perception/AIPerceptionTypes.h"
#include "SHEnemyAIController.generated.h"

class ASHEnemyCharacter;
class USHAIPerceptionConfig;
class UAIPerceptionComponent;
class UBehaviorTreeComponent;
class UBlackboardComponent;
class UBehaviorTree;
struct FSHTacticalOrder;

// -----------------------------------------------------------------------
//  Enums
// -----------------------------------------------------------------------

/** Individual combat stance / micro-behavior. */
UENUM(BlueprintType)
enum class ESHCombatBehavior : uint8
{
	/** Moving to cover under fire. */
	SeekingCover,

	/** In cover, peeking/firing. */
	FiringFromCover,

	/** Advancing under covering fire. */
	BoundingAdvance,

	/** Flanking around enemy position. */
	Flanking,

	/** Laying suppressive fire without precise aim. */
	Suppressing,

	/** Throwing grenade (frag or smoke). */
	ThrowingGrenade,

	/** Retreating to rally point. */
	Retreating,

	/** Regrouping with squad. */
	Regrouping,

	/** Executing Primordia tactical order directly. */
	ExecutingOrder,

	/** Idle / patrol when no threat is known. */
	Patrolling,

	/** Investigating suspicious stimulus. */
	Investigating,

	/** Searching for lost contact. */
	Searching,

	/** Holding position / defensive. */
	Holding
};

// -----------------------------------------------------------------------
//  Blackboard key names (constants used throughout)
// -----------------------------------------------------------------------

namespace SHBBKeys
{
	const FName TargetActor          = TEXT("TargetActor");
	const FName TargetLocation       = TEXT("TargetLocation");
	const FName CoverLocation        = TEXT("CoverLocation");
	const FName PatrolPoint          = TEXT("PatrolPoint");
	const FName AwarenessState       = TEXT("AwarenessState");
	const FName CombatBehavior       = TEXT("CombatBehavior");
	const FName MoraleState          = TEXT("MoraleState");
	const FName SuppressionLevel     = TEXT("SuppressionLevel");
	const FName HasTacticalOrder     = TEXT("HasTacticalOrder");
	const FName TacticalOrderType    = TEXT("TacticalOrderType");
	const FName TacticalOrderTarget  = TEXT("TacticalOrderTarget");
	const FName NearbySquadMates     = TEXT("NearbySquadMates");
	const FName ShouldUseGrenade     = TEXT("ShouldUseGrenade");
	const FName GrenadeTarget        = TEXT("GrenadeTarget");
	const FName FlankingTarget       = TEXT("FlankingTarget");
	const FName IsWounded            = TEXT("IsWounded");
	const FName HasSurrendered       = TEXT("HasSurrendered");
}

// -----------------------------------------------------------------------

/**
 * ASHEnemyAIController
 *
 * AI controller for PLA enemy soldiers.  Drives a Behavior Tree with a
 * Primordia override layer that can inject tactical orders from the
 * Decision Engine.  Manages perception (sight, hearing, damage), cover
 * usage, fire-and-maneuver, grenades, flanking, suppression, retreat,
 * and squad coordination.
 *
 * Difficulty is expressed through tactical intelligence (perception
 * config, decision quality, coordination) rather than health/damage
 * scaling.
 */
UCLASS(BlueprintType, Blueprintable)
class SHATTEREDHORIZON2032_API ASHEnemyAIController : public AAIController
{
	GENERATED_BODY()

public:
	ASHEnemyAIController(const FObjectInitializer& ObjectInitializer);

	// ------------------------------------------------------------------
	//  AAIController overrides
	// ------------------------------------------------------------------

	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void UpdateControlRotation(float DeltaTime, bool bUpdatePawn) override;

	// ------------------------------------------------------------------
	//  Perception
	// ------------------------------------------------------------------

	/** Called when the AI perception system detects a stimulus. */
	UFUNCTION()
	void OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus);

	/** Get current awareness state. */
	UFUNCTION(BlueprintPure, Category = "SH|AI")
	ESHAwarenessState GetAwarenessState() const { return AwarenessState; }

	/** Get the current primary target. */
	UFUNCTION(BlueprintPure, Category = "SH|AI")
	AActor* GetPrimaryTarget() const;

	/** Manually set a target (e.g., from squad leader). */
	UFUNCTION(BlueprintCallable, Category = "SH|AI")
	void SetPrimaryTarget(AActor* NewTarget);

	// ------------------------------------------------------------------
	//  Tactical orders (Primordia Decision Engine interface)
	// ------------------------------------------------------------------

	/** Receive a tactical order from the Decision Engine. */
	UFUNCTION(BlueprintCallable, Category = "SH|AI|Primordia")
	void ReceiveTacticalOrder(const FSHTacticalOrder& Order);

	/** Clear the current tactical order. */
	UFUNCTION(BlueprintCallable, Category = "SH|AI|Primordia")
	void ClearTacticalOrder();

	/** Query whether a Primordia order is active. */
	UFUNCTION(BlueprintPure, Category = "SH|AI|Primordia")
	bool HasActiveTacticalOrder() const { return bHasTacticalOrder; }

	// ------------------------------------------------------------------
	//  Combat behaviors
	// ------------------------------------------------------------------

	/** Request the AI to find and move to cover. */
	UFUNCTION(BlueprintCallable, Category = "SH|AI|Combat")
	bool FindAndMoveToCover(FVector ThreatLocation);

	/** Request the AI to perform a flanking maneuver. */
	UFUNCTION(BlueprintCallable, Category = "SH|AI|Combat")
	bool AttemptFlank(FVector EnemyLocation);

	/** Request suppressive fire on a location. */
	UFUNCTION(BlueprintCallable, Category = "SH|AI|Combat")
	void OrderSuppressiveFire(FVector TargetLocation, float Duration);

	/** Evaluate and potentially throw a grenade. */
	UFUNCTION(BlueprintCallable, Category = "SH|AI|Combat")
	bool EvaluateGrenadeUsage();

	/** Begin retreat to a rally point. */
	UFUNCTION(BlueprintCallable, Category = "SH|AI|Combat")
	void BeginRetreat(FVector RallyPoint);

	/** Change firing position (don't stay in one spot too long). */
	UFUNCTION(BlueprintCallable, Category = "SH|AI|Combat")
	bool ChangeFirePosition();

	// ------------------------------------------------------------------
	//  Squad coordination
	// ------------------------------------------------------------------

	/** Report a target to all squad controllers within range. */
	UFUNCTION(BlueprintCallable, Category = "SH|AI|Squad")
	void ShareTargetWithSquad(AActor* Target);

	/** Check if any squad mate already has this target to avoid over-concentration. */
	UFUNCTION(BlueprintCallable, Category = "SH|AI|Squad")
	bool IsTargetAlreadyCoveredBySquad(AActor* Target) const;

	/** Get the squad's current average position. */
	UFUNCTION(BlueprintPure, Category = "SH|AI|Squad")
	FVector GetSquadCentroid() const;

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Behavior Tree asset to run. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|AI|Config")
	TObjectPtr<UBehaviorTree> BehaviorTreeAsset;

	/** How often to re-evaluate cover position while in combat (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|AI|Config")
	float CoverReevaluationInterval = 8.f;

	/** Max time to stay in one firing position before moving (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|AI|Config")
	float MaxTimeInFirePosition = 12.f;

	/** Maximum number of targets the AI tracks simultaneously. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|AI|Config")
	int32 MaxTrackedTargets = 5;

	/** Radius to search for cover points (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|AI|Config")
	float CoverSearchRadius = 3000.f;

	/** Minimum distance from threat for a cover position to be considered. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|AI|Config")
	float MinCoverDistFromThreat = 500.f;

	/** Grenade throw range (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|AI|Config")
	float GrenadeThrowRange = 3000.f;

	/** How long to search after losing contact (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|AI|Config")
	float SearchDuration = 30.f;

protected:
	// ------------------------------------------------------------------
	//  Internal — Perception processing
	// ------------------------------------------------------------------

	/** Accumulate detection on a sensed actor and transition awareness. */
	void ProcessSightStimulus(AActor* Actor, const FAIStimulus& Stimulus);

	/** Process hearing stimulus. */
	void ProcessHearingStimulus(AActor* Actor, const FAIStimulus& Stimulus);

	/** Process damage stimulus. */
	void ProcessDamageStimulus(AActor* Actor, const FAIStimulus& Stimulus);

	/** Update awareness state based on current detection levels. */
	void UpdateAwarenessState(float DeltaSeconds);

	/** Get the current visibility condition based on environment. */
	ESHVisibilityCondition GetCurrentVisibilityCondition() const;

	// ------------------------------------------------------------------
	//  Internal — Behavior
	// ------------------------------------------------------------------

	/** Select the best combat behavior based on situation. */
	ESHCombatBehavior EvaluateBestBehavior() const;

	/** Update blackboard with current state. */
	void UpdateBlackboard();

	/** Tick the cover reevaluation timer. */
	void TickCoverLogic(float DeltaSeconds);

	/** Tick the tactical order execution. */
	void TickTacticalOrder(float DeltaSeconds);

	/** Find a flanking position around a target. */
	FVector ComputeFlankingPosition(FVector EnemyLocation) const;

	/** Find optimal cover point. */
	bool FindCoverPoint(FVector ThreatLocation, FVector& OutCoverPoint) const;

	/** Evaluate if smoke grenade would be beneficial. */
	bool ShouldUseSmokeGrenade() const;

	/** Score a potential target for engagement priority. */
	float ScoreTarget(AActor* Target) const;

	/** Select the best target from known threats. */
	AActor* SelectBestTarget() const;

	// ------------------------------------------------------------------
	//  Components
	// ------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|AI|Components")
	TObjectPtr<UAIPerceptionComponent> AIPerceptionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|AI|Components")
	TObjectPtr<UBehaviorTreeComponent> BehaviorTreeComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|AI|Components")
	TObjectPtr<UBlackboardComponent> BlackboardComp;

private:
	// ------------------------------------------------------------------
	//  State
	// ------------------------------------------------------------------

	/** Cached reference to the possessed enemy character. */
	UPROPERTY()
	TObjectPtr<ASHEnemyCharacter> EnemyCharacter;

	/** Perception config from the character (cached at OnPossess). */
	UPROPERTY()
	TObjectPtr<USHAIPerceptionConfig> PerceptionConfig;

	/** Current awareness level. */
	ESHAwarenessState AwarenessState = ESHAwarenessState::Unaware;

	/** Current combat behavior. */
	ESHCombatBehavior CurrentBehavior = ESHCombatBehavior::Patrolling;

	/** Per-actor detection accumulation. */
	TMap<TWeakObjectPtr<AActor>, float> DetectionAccumulation;

	/** Known threats (detected actors). */
	TArray<TWeakObjectPtr<AActor>> KnownThreats;

	/** Time in current fire position. */
	float TimeInFirePosition = 0.f;

	/** Timer for cover reevaluation. */
	float CoverReevalTimer = 0.f;

	/** Timer for awareness state cooldown. */
	float AwarenessCooldownTimer = 0.f;

	/** Whether a Primordia tactical order is active. */
	bool bHasTacticalOrder = false;

	/** The current tactical order type. */
	FString CurrentOrderType;

	/** The current tactical order target location. */
	FVector CurrentOrderTarget = FVector::ZeroVector;

	/** Time spent executing the current tactical order. */
	float TacticalOrderElapsed = 0.f;

	/** Location of last known threat for search behavior. */
	FVector LastKnownThreatLocation = FVector::ZeroVector;
};
