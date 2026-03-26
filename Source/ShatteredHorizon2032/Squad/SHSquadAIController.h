// Copyright Shattered Horizon 2032. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "SHSquadOrder.h"
#include "Perception/AIPerceptionTypes.h"
#include "SHSquadAIController.generated.h"

class ASHSquadMember;
class UBehaviorTreeComponent;
class UBlackboardComponent;
class UBehaviorTree;
class UAIPerceptionComponent;
class UAISenseConfig_Sight;
class UAISenseConfig_Hearing;
class UAISenseConfig_Damage;
class UEnvQuery;
class UEnvQueryInstanceBlueprintWrapper;

/* ───────────────────────────────────────────────────────────── */
/*  AI Combat State (Blackboard-mirrored)                       */
/* ───────────────────────────────────────────────────────────── */

UENUM(BlueprintType)
enum class ESHAICombatState : uint8
{
	Idle		UMETA(DisplayName = "Idle"),
	Patrolling	UMETA(DisplayName = "Patrolling"),
	Moving		UMETA(DisplayName = "Moving To Position"),
	InCover		UMETA(DisplayName = "In Cover"),
	Engaging	UMETA(DisplayName = "Engaging"),
	Suppressing	UMETA(DisplayName = "Suppressing"),
	Flanking	UMETA(DisplayName = "Flanking"),
	Breaching	UMETA(DisplayName = "Breaching"),
	Retreating	UMETA(DisplayName = "Retreating"),
	DraggingWounded UMETA(DisplayName = "Dragging Wounded"),
	Overwatch	UMETA(DisplayName = "Overwatch")
};

/* ───────────────────────────────────────────────────────────── */
/*  Threat Entry                                                */
/* ───────────────────────────────────────────────────────────── */

USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHThreatEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "AI|Threat")
	TWeakObjectPtr<AActor> ThreatActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "AI|Threat")
	FVector LastKnownLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "AI|Threat")
	float ThreatScore = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "AI|Threat")
	float TimeSinceLastSeen = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "AI|Threat")
	bool bIsCurrentlyVisible = false;

	UPROPERTY(BlueprintReadOnly, Category = "AI|Threat")
	bool bHasDamagedUs = false;

	UPROPERTY(BlueprintReadOnly, Category = "AI|Threat")
	float DistanceToUs = 0.f;
};

/* ───────────────────────────────────────────────────────────── */
/*  Cover Score Entry (EQS result cache)                        */
/* ───────────────────────────────────────────────────────────── */

USTRUCT()
struct FSHCoverCandidate
{
	GENERATED_BODY()

	FVector Location = FVector::ZeroVector;
	float Score = 0.f;
	bool bProvidesCoverFromThreat = false;
	bool bAllowsFiringOnThreat = false;
};

/* ───────────────────────────────────────────────────────────── */
/*  Blackboard Key Names (constants)                            */
/* ───────────────────────────────────────────────────────────── */

namespace SHBBKeys
{
	const FName CombatState		= TEXT("CombatState");
	const FName CurrentTarget	= TEXT("CurrentTarget");
	const FName TargetLocation	= TEXT("TargetLocation");
	const FName MoveToLocation	= TEXT("MoveToLocation");
	const FName HasOrder		= TEXT("HasOrder");
	const FName OrderType		= TEXT("OrderType");
	const FName IsInCover		= TEXT("IsInCover");
	const FName CoverLocation	= TEXT("CoverLocation");
	const FName NeedsReload		= TEXT("NeedsReload");
	const FName IsSuppressed	= TEXT("IsSuppressed");
	const FName IsWounded		= TEXT("IsWounded");
	const FName AmmoLow			= TEXT("AmmoLow");
	const FName ThreatCount		= TEXT("ThreatCount");
	const FName FormationPos	= TEXT("FormationPosition");
	const FName BuddyLocation	= TEXT("BuddyTeamPartnerLocation");
	const FName ShouldFire		= TEXT("ShouldFire");
	const FName CanSeeEnemy		= TEXT("CanSeeEnemy");
	const FName WoundedAlly		= TEXT("WoundedAllyActor");
	const FName SquadMorale		= TEXT("SquadMorale");
}

/* ───────────────────────────────────────────────────────────── */
/*  ASHSquadAIController                                        */
/* ───────────────────────────────────────────────────────────── */

UCLASS()
class SHATTEREDHORIZON2032_API ASHSquadAIController : public AAIController
{
	GENERATED_BODY()

public:
	ASHSquadAIController();

protected:
	virtual void BeginPlay() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;
	virtual void Tick(float DeltaTime) override;

	/* ── Perception Callbacks ───────────────────────────────── */
	UFUNCTION()
	void OnPerceptionUpdated(const TArray<AActor*>& UpdatedActors);

	UFUNCTION()
	void OnTargetPerceptionInfoUpdated(AActor* Actor, FAIStimulus Stimulus);

public:
	/* ── Order Interface (called by ASHSquadMember) ─────────── */
	void OnOrderReceived(const FSHSquadOrder& Order);

	/* ── Behavior Tree ──────────────────────────────────────── */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|BehaviorTree")
	TObjectPtr<UBehaviorTree> SquadMemberBT;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|BehaviorTree")
	TObjectPtr<UBehaviorTreeComponent> BehaviorTreeComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|BehaviorTree")
	TObjectPtr<UBlackboardComponent> BlackboardComp;

	/* ── Perception ─────────────────────────────────────────── */

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Perception")
	TObjectPtr<UAIPerceptionComponent> PerceptionComp;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Perception")
	float SightRange = 8000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Perception")
	float SightAngle = 75.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Perception")
	float HearingRange = 5000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Perception")
	float LoseSightTime = 5.f;

	/* ── EQS Queries ────────────────────────────────────────── */

	/** Environment Query for finding cover positions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|EQS")
	TObjectPtr<UEnvQuery> FindCoverQuery;

	/** Environment Query for finding flanking routes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|EQS")
	TObjectPtr<UEnvQuery> FindFlankRouteQuery;

	/** Environment Query for finding overwatch positions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|EQS")
	TObjectPtr<UEnvQuery> FindOverwatchQuery;

	/* ── Threat Management ──────────────────────────────────── */

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Threat")
	TArray<FSHThreatEntry> ThreatList;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Threat")
	TWeakObjectPtr<AActor> PrimaryThreat;

	/** Time after which unseen threats are dropped from the list. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI|Threat")
	float ThreatForgetTime = 30.f;

	/* ── Combat State ───────────────────────────────────────── */

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|State")
	ESHAICombatState CombatState = ESHAICombatState::Idle;

	/* ── Cover System ───────────────────────────────────────── */

	/** Run the cover EQS query and process results. */
	UFUNCTION(BlueprintCallable, Category = "AI|Cover")
	void RequestCoverPosition();

	/** Cached best cover position from last EQS query. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Cover")
	FVector BestCoverPosition = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "AI|Cover")
	bool bHasValidCover = false;

	/* ── Tactical Movement ──────────────────────────────────── */

	/** Move to cover, avoiding open ground. Returns true if path found. */
	UFUNCTION(BlueprintCallable, Category = "AI|Movement")
	bool MoveToTacticalPosition(const FVector& Destination);

	/** Execute bounding overwatch: this member moves while buddy covers. */
	UFUNCTION(BlueprintCallable, Category = "AI|Movement")
	void ExecuteBoundingOverwatch(const FVector& Destination);

	/* ── Engagement ─────────────────────────────────────────── */

	/** Evaluate and select the best target from the threat list. */
	UFUNCTION(BlueprintCallable, Category = "AI|Combat")
	AActor* SelectBestTarget();

	/** Should we engage the current target at this range? */
	UFUNCTION(BlueprintPure, Category = "AI|Combat")
	bool ShouldEngageTarget(AActor* Target) const;

	/** Execute suppressive fire toward a location. */
	UFUNCTION(BlueprintCallable, Category = "AI|Combat")
	void ExecuteSuppressiveFire(const FVector& TargetLocation);

	/** Initiate a room clearing / breach sequence. */
	UFUNCTION(BlueprintCallable, Category = "AI|Combat")
	void ExecuteBreachProcedure(const FVector& EntryPoint, AActor* DoorActor);

	/* ── Medical / Casualty Response ──────────────────────────── */

	/** Check if any squad member is wounded and needs treatment. */
	UFUNCTION(BlueprintCallable, Category = "AI|Medical")
	AActor* FindWoundedAlly(float MaxRange = 3000.f) const;

	/** Begin moving to and treating a wounded squad member. */
	UFUNCTION(BlueprintCallable, Category = "AI|Medical")
	void BeginCasualtyResponse(AActor* WoundedAlly);

	/** Apply field treatment to a wounded squad member (bandage, tourniquet). */
	UFUNCTION(BlueprintCallable, Category = "AI|Medical")
	void ApplyFieldTreatment(AActor* WoundedAlly);

	/** Drag a wounded squad member to cover. */
	UFUNCTION(BlueprintCallable, Category = "AI|Medical")
	void BeginDragToSafety(AActor* WoundedAlly);

	/** Cancel casualty response (e.g., threat too close). */
	UFUNCTION(BlueprintCallable, Category = "AI|Medical")
	void AbortCasualtyResponse();

	/** Whether this squad member has the Corpsman role (priority medic). */
	UFUNCTION(BlueprintPure, Category = "AI|Medical")
	bool IsCorpsman() const;

	/** Whether this AI is currently treating a casualty. */
	UFUNCTION(BlueprintPure, Category = "AI|Medical")
	bool IsTreatingCasualty() const { return bIsTreatingCasualty; }

	/* ── Self-Preservation vs. Order Compliance ─────────────── */

	/** Returns a weight [0,1] for how much self-preservation overrides orders.
	    0 = blindly follow orders, 1 = prioritize staying alive. */
	UFUNCTION(BlueprintPure, Category = "AI|Decision")
	float GetSelfPreservationWeight() const;

	/** Should AI break from current order to seek cover? */
	UFUNCTION(BlueprintPure, Category = "AI|Decision")
	bool ShouldBreakForCover() const;

	/** Should AI reload now (prefers cover)? */
	UFUNCTION(BlueprintPure, Category = "AI|Decision")
	bool ShouldReloadNow() const;

	/* ── Blackboard Sync ────────────────────────────────────── */

	/** Push current state to blackboard for behavior tree consumption. */
	void SyncBlackboard();

private:
	/* ── References ─────────────────────────────────────────── */
	UPROPERTY()
	TWeakObjectPtr<ASHSquadMember> ControlledMember;

	/* ── Perception Config ──────────────────────────────────── */
	UPROPERTY()
	TObjectPtr<UAISenseConfig_Sight> SightConfig;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Hearing> HearingConfig;

	UPROPERTY()
	TObjectPtr<UAISenseConfig_Damage> DamageConfig;

	void SetupPerception();

	/* ── Threat Processing ──────────────────────────────────── */
	void UpdateThreatList(float DeltaTime);
	float CalculateThreatScore(const FSHThreatEntry& Entry) const;
	void ProcessSightStimulus(AActor* Actor, const FAIStimulus& Stimulus);
	void ProcessHearingStimulus(AActor* Actor, const FAIStimulus& Stimulus);
	void ProcessDamageStimulus(AActor* Actor, const FAIStimulus& Stimulus);

	/* ── EQS Callbacks ──────────────────────────────────────── */
	void OnCoverQueryFinished(TSharedPtr<FEnvQueryResult> Result);

	/* ── Cover Helpers ──────────────────────────────────────── */
	float EvaluateCoverQuality(const FVector& CoverPos) const;
	bool IsPositionInCover(const FVector& Position) const;

	/* ── Autonomous Decision Making ─────────────────────────── */
	void EvaluateAutonomousActions();
	void TryReturnFire();
	void TrySeekCover();

	/** Time accumulator for periodic EQS re-queries. */
	float CoverQueryCooldown = 0.f;
	static constexpr float CoverQueryInterval = 3.f;

	/** Burst fire tracking. */
	int32 BurstRoundsRemaining = 0;
	float BurstFireTimer = 0.f;
	float BurstFireRate = 0.1f; // 600 RPM default

	/* ── Medical State ──────────────────────────────────────── */
	UPROPERTY()
	TWeakObjectPtr<AActor> CurrentCasualty;

	bool bIsTreatingCasualty = false;
	float TreatmentTimer = 0.f;

	/** Time to apply field treatment (seconds). */
	static constexpr float FieldTreatmentDuration = 6.f;

	/** Time to drag a casualty to cover (seconds per 100cm). */
	static constexpr float DragSpeedCmPerSec = 80.f;
};
