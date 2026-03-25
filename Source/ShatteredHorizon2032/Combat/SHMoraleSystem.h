// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SHMoraleSystem.generated.h"

class USHSuppressionSystem;
class USHFatigueSystem;

// ─────────────────────────────────────────────────────────────
//  Enums
// ─────────────────────────────────────────────────────────────

/** Morale state brackets. */
UENUM(BlueprintType)
enum class ESHMoraleState : uint8
{
	Determined	UMETA(DisplayName = "Determined"),
	Steady		UMETA(DisplayName = "Steady"),
	Shaken		UMETA(DisplayName = "Shaken"),
	Breaking	UMETA(DisplayName = "Breaking"),
	Routed		UMETA(DisplayName = "Routed")
};

/** Events that can modify morale. */
UENUM(BlueprintType)
enum class ESHMoraleEvent : uint8
{
	// Positive
	NearbyFriendlies		UMETA(DisplayName = "Nearby Friendlies"),
	SuccessfulEngagement	UMETA(DisplayName = "Successful Engagement"),
	OfficerNearby			UMETA(DisplayName = "Officer Nearby"),
	InCover					UMETA(DisplayName = "In Cover"),
	Rallied					UMETA(DisplayName = "Rallied"),
	ReachedSafePosition		UMETA(DisplayName = "Reached Safe Position"),
	MedicTreatment			UMETA(DisplayName = "Medic Treatment"),
	EnemyKilled				UMETA(DisplayName = "Enemy Killed"),

	// Negative
	FriendlyCasualty		UMETA(DisplayName = "Friendly Casualty"),
	UnderHeavyFire			UMETA(DisplayName = "Under Heavy Fire"),
	Isolated				UMETA(DisplayName = "Isolated"),
	Wounded					UMETA(DisplayName = "Wounded"),
	LowAmmo				UMETA(DisplayName = "Low Ammo"),
	AllyBroken				UMETA(DisplayName = "Ally Broken"),
	OfficerDown				UMETA(DisplayName = "Officer Down"),
	SquadmateDown			UMETA(DisplayName = "Squadmate Down"),
	Flanked					UMETA(DisplayName = "Flanked")
};

/** AI behavioral response when morale breaks. */
UENUM(BlueprintType)
enum class ESHMoraleBreakBehavior : uint8
{
	RefuseOrders	UMETA(DisplayName = "Refuse Orders"),
	Retreat			UMETA(DisplayName = "Retreat"),
	Freeze			UMETA(DisplayName = "Freeze"),
	Surrender		UMETA(DisplayName = "Surrender")
};

// ─────────────────────────────────────────────────────────────
//  Delegates
// ─────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnMoraleStateChanged,
	AActor*, AffectedActor,
	ESHMoraleState, OldState,
	ESHMoraleState, NewState);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnMoraleBroken,
	AActor*, AffectedActor,
	ESHMoraleBreakBehavior, Behavior);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnMoraleRallied,
	AActor*, AffectedActor);

// ─────────────────────────────────────────────────────────────
//  Component
// ─────────────────────────────────────────────────────────────

/**
 * USHMoraleSystem
 *
 * Actor component tracking individual morale and psychological state.
 * Works for both player characters and AI combatants.
 *
 * - Morale is a 0..1 value mapped to discrete states
 * - Positive events (kills, cover, officer proximity) boost morale
 * - Negative events (casualties, heavy fire, isolation) lower it
 * - When morale breaks, AI may refuse orders, retreat, freeze, or surrender
 * - Player morale drives post-process effects but never removes control
 * - Squad-level morale is the aggregate of individuals; one breaking can cascade
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHMoraleSystem : public UActorComponent
{
	GENERATED_BODY()

public:
	USHMoraleSystem();

	// ------------------------------------------------------------------
	//  UActorComponent overrides
	// ------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ------------------------------------------------------------------
	//  Event reporting
	// ------------------------------------------------------------------

	/** Report a morale-affecting event. */
	UFUNCTION(BlueprintCallable, Category = "SH|Morale")
	void ReportMoraleEvent(ESHMoraleEvent Event);

	/** Direct morale adjustment (positive = boost, negative = penalty). */
	UFUNCTION(BlueprintCallable, Category = "SH|Morale")
	void AdjustMorale(float Delta);

	// ------------------------------------------------------------------
	//  Recovery / rally
	// ------------------------------------------------------------------

	/** Attempt to rally this character (requires officer or rally point). */
	UFUNCTION(BlueprintCallable, Category = "SH|Morale")
	bool AttemptRally(AActor* RallySource);

	/** Notify that the character has reached a safe/fortified position. */
	UFUNCTION(BlueprintCallable, Category = "SH|Morale")
	void NotifySafePosition();

	/** Notify that a medic has treated this character. */
	UFUNCTION(BlueprintCallable, Category = "SH|Morale")
	void NotifyMedicTreatment();

	// ------------------------------------------------------------------
	//  Squad integration
	// ------------------------------------------------------------------

	/** Notify that a squadmate's morale has broken (cascade check). */
	UFUNCTION(BlueprintCallable, Category = "SH|Morale")
	void NotifySquadmateBroken(AActor* BrokenSquadmate);

	/** Notify that a squadmate has been killed or incapacitated. */
	UFUNCTION(BlueprintCallable, Category = "SH|Morale")
	void NotifySquadmateCasualty(AActor* Casualty);

	/** Set the number of nearby friendlies (used for isolation checks). */
	UFUNCTION(BlueprintCallable, Category = "SH|Morale")
	void SetNearbyFriendlyCount(int32 Count);

	/** Set whether an officer is nearby. */
	UFUNCTION(BlueprintCallable, Category = "SH|Morale")
	void SetOfficerNearby(bool bNearby);

	// ------------------------------------------------------------------
	//  Queries
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|Morale")
	float GetMoraleValue() const { return MoraleValue; }

	UFUNCTION(BlueprintPure, Category = "SH|Morale")
	ESHMoraleState GetMoraleState() const { return CurrentState; }

	UFUNCTION(BlueprintPure, Category = "SH|Morale")
	bool IsBroken() const { return CurrentState == ESHMoraleState::Breaking || CurrentState == ESHMoraleState::Routed; }

	UFUNCTION(BlueprintPure, Category = "SH|Morale")
	bool IsRouted() const { return CurrentState == ESHMoraleState::Routed; }

	/** Whether this component belongs to a player-controlled character. */
	UFUNCTION(BlueprintPure, Category = "SH|Morale")
	bool IsPlayerControlled() const;

	/** Post-process desaturation driven by low morale (for player only). */
	UFUNCTION(BlueprintPure, Category = "SH|Morale|FX")
	float GetMoraleDesaturation() const;

	/** Post-process vignette driven by low morale (for player only). */
	UFUNCTION(BlueprintPure, Category = "SH|Morale|FX")
	float GetMoraleVignette() const;

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Morale")
	FOnMoraleStateChanged OnMoraleStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "SH|Morale")
	FOnMoraleBroken OnMoraleBroken;

	UPROPERTY(BlueprintAssignable, Category = "SH|Morale")
	FOnMoraleRallied OnMoraleRallied;

protected:
	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Starting morale value [0..1]. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Morale|Config")
	float InitialMorale = 0.75f;

	/** Morale event magnitudes (positive values boost, negative values penalize). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Morale|Config")
	TMap<ESHMoraleEvent, float> EventMagnitudes;

	/** Natural morale recovery per second when in a stable state. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Morale|Config")
	float PassiveRecoveryRate = 0.005f;

	/** Bonus recovery rate when an officer is nearby. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Morale|Config")
	float OfficerRecoveryBonus = 0.01f;

	/** Morale penalty per second while isolated (no friendlies in radius). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Morale|Config")
	float IsolationPenaltyRate = 0.008f;

	/** Cascade penalty applied when a squadmate breaks. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Morale|Config")
	float CascadePenalty = 0.15f;

	/** Rally morale restoration amount. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Morale|Config")
	float RallyRestorationAmount = 0.25f;

	/** Cooldown (seconds) between rally attempts on this character. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Morale|Config")
	float RallyCooldown = 30.f;

private:
	// ------------------------------------------------------------------
	//  Internal
	// ------------------------------------------------------------------
	void TickPassiveModifiers(float DeltaTime);
	void UpdateMoraleState();
	ESHMoraleState ComputeState(float Value) const;
	ESHMoraleBreakBehavior DetermineBreakBehavior() const;
	void InitDefaultEventMagnitudes();

	/** Morale value [0..1]. Higher is better. */
	UPROPERTY()
	float MoraleValue = 0.75f;

	UPROPERTY()
	ESHMoraleState CurrentState = ESHMoraleState::Steady;

	/** Number of friendlies within proximity (set externally). */
	int32 NearbyFriendlyCount = 0;

	/** Whether an officer/leader is nearby. */
	bool bOfficerNearby = false;

	/** World time of last rally on this character. */
	float LastRallyTime = -100.f;

	/** Track if we already fired cascade notification for current broken state. */
	bool bBrokenNotificationSent = false;
};
