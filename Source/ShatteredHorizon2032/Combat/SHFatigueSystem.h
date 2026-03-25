// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SHFatigueSystem.generated.h"

// ─────────────────────────────────────────────────────────────
//  Enums
// ─────────────────────────────────────────────────────────────

/** Fatigue severity brackets. */
UENUM(BlueprintType)
enum class ESHFatigueLevel : uint8
{
	Fresh		UMETA(DisplayName = "Fresh"),
	Winded		UMETA(DisplayName = "Winded"),
	Tired		UMETA(DisplayName = "Tired"),
	Exhausted	UMETA(DisplayName = "Exhausted"),
	Collapsed	UMETA(DisplayName = "Collapsed")
};

/** Activity types that drain stamina. */
UENUM(BlueprintType)
enum class ESHStaminaDrainType : uint8
{
	Sprint		UMETA(DisplayName = "Sprint"),
	Vault		UMETA(DisplayName = "Vault"),
	Melee		UMETA(DisplayName = "Melee"),
	HoldBreath	UMETA(DisplayName = "Hold Breath"),
	Swim		UMETA(DisplayName = "Swim"),
	Climb		UMETA(DisplayName = "Climb")
};

// ─────────────────────────────────────────────────────────────
//  Delegates
// ─────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnFatigueLevelChanged,
	ESHFatigueLevel, OldLevel,
	ESHFatigueLevel, NewLevel);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnStaminaDepleted,
	AActor*, Owner);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnBreathHoldReleased,
	float, RemainingStamina);

// ─────────────────────────────────────────────────────────────
//  Component
// ─────────────────────────────────────────────────────────────

/**
 * USHFatigueSystem
 *
 * Actor component that manages stamina and long-term fatigue for any
 * character (player or AI).  Stamina is a short-term resource drained
 * by physical exertion; fatigue is a long-term degradation that
 * accumulates over the course of a mission.
 *
 * Effects:
 *  - Weapon sway increases with fatigue
 *  - Sprint speed and duration decrease
 *  - Reload speed degrades
 *  - Breath control (hold breath to steady aim) consumes stamina
 *  - Injuries compound all fatigue penalties
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHFatigueSystem : public UActorComponent
{
	GENERATED_BODY()

public:
	USHFatigueSystem();

	// ------------------------------------------------------------------
	//  UActorComponent overrides
	// ------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ------------------------------------------------------------------
	//  Stamina management
	// ------------------------------------------------------------------

	/** Begin continuous stamina drain for a given activity. */
	UFUNCTION(BlueprintCallable, Category = "SH|Fatigue")
	void BeginStaminaDrain(ESHStaminaDrainType Activity);

	/** End continuous drain for the given activity. */
	UFUNCTION(BlueprintCallable, Category = "SH|Fatigue")
	void EndStaminaDrain(ESHStaminaDrainType Activity);

	/** Apply a one-shot stamina cost (e.g. a single vault or melee). */
	UFUNCTION(BlueprintCallable, Category = "SH|Fatigue")
	void ConsumeStamina(float Amount);

	/** Returns true if the character has enough stamina for the activity. */
	UFUNCTION(BlueprintPure, Category = "SH|Fatigue")
	bool CanPerformActivity(ESHStaminaDrainType Activity) const;

	// ------------------------------------------------------------------
	//  Breath control
	// ------------------------------------------------------------------

	/** Begin holding breath to steady aim. */
	UFUNCTION(BlueprintCallable, Category = "SH|Fatigue")
	void BeginHoldBreath();

	/** Release held breath (or forced when stamina runs out). */
	UFUNCTION(BlueprintCallable, Category = "SH|Fatigue")
	void EndHoldBreath();

	UFUNCTION(BlueprintPure, Category = "SH|Fatigue")
	bool IsHoldingBreath() const { return bIsHoldingBreath; }

	// ------------------------------------------------------------------
	//  Injury integration
	// ------------------------------------------------------------------

	/**
	 * Notify the fatigue system that the owning character has been injured.
	 * @param Severity	0..1 normalized wound severity.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Fatigue")
	void NotifyInjury(float Severity);

	/** Clear injury penalty (e.g. medic treated the wound). */
	UFUNCTION(BlueprintCallable, Category = "SH|Fatigue")
	void ClearInjuryPenalty();

	// ------------------------------------------------------------------
	//  Carried weight
	// ------------------------------------------------------------------

	/** Update carried weight in kg; affects recovery rate and stamina cost. */
	UFUNCTION(BlueprintCallable, Category = "SH|Fatigue")
	void SetCarriedWeight(float WeightKg);

	// ------------------------------------------------------------------
	//  Queries
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|Fatigue")
	float GetCurrentStamina() const { return CurrentStamina; }

	UFUNCTION(BlueprintPure, Category = "SH|Fatigue")
	float GetMaxStamina() const { return MaxStamina; }

	UFUNCTION(BlueprintPure, Category = "SH|Fatigue")
	float GetStaminaPercent() const { return MaxStamina > 0.f ? CurrentStamina / MaxStamina : 0.f; }

	UFUNCTION(BlueprintPure, Category = "SH|Fatigue")
	float GetLongTermFatigue() const { return LongTermFatigue; }

	UFUNCTION(BlueprintPure, Category = "SH|Fatigue")
	ESHFatigueLevel GetFatigueLevel() const { return CurrentFatigueLevel; }

	/** Weapon sway multiplier driven by fatigue (1.0 = baseline). */
	UFUNCTION(BlueprintPure, Category = "SH|Fatigue")
	float GetWeaponSwayMultiplier() const;

	/** Sprint speed multiplier (1.0 = full speed). */
	UFUNCTION(BlueprintPure, Category = "SH|Fatigue")
	float GetSprintSpeedMultiplier() const;

	/** Reload speed multiplier (1.0 = normal). */
	UFUNCTION(BlueprintPure, Category = "SH|Fatigue")
	float GetReloadSpeedMultiplier() const;

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Fatigue")
	FOnFatigueLevelChanged OnFatigueLevelChanged;

	UPROPERTY(BlueprintAssignable, Category = "SH|Fatigue")
	FOnStaminaDepleted OnStaminaDepleted;

	UPROPERTY(BlueprintAssignable, Category = "SH|Fatigue")
	FOnBreathHoldReleased OnBreathHoldReleased;

protected:
	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, Category = "SH|Fatigue|Config")
	float MaxStamina = 100.f;

	/** Base stamina recovery per second at rest. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Fatigue|Config")
	float BaseRecoveryRate = 8.f;

	/** Stamina drain rates per activity type (per second for continuous). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Fatigue|Config")
	TMap<ESHStaminaDrainType, float> DrainRates;

	/** One-shot stamina costs for burst activities. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Fatigue|Config")
	TMap<ESHStaminaDrainType, float> BurstCosts;

	/** Minimum stamina required per activity to start it. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Fatigue|Config")
	TMap<ESHStaminaDrainType, float> MinimumStaminaRequired;

	/** How quickly long-term fatigue accumulates (per second of continuous exertion). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Fatigue|Config")
	float LongTermFatigueRate = 0.002f;

	/** Weight threshold (kg) above which recovery is penalized. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Fatigue|Config")
	float WeightPenaltyThreshold = 15.f;

	/** Recovery rate penalty per kg above threshold. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Fatigue|Config")
	float RecoveryPenaltyPerKg = 0.15f;

	/** Maximum hold-breath duration in seconds (before stamina penalty). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Fatigue|Config")
	float MaxHoldBreathDuration = 6.f;

private:
	// ------------------------------------------------------------------
	//  Runtime state
	// ------------------------------------------------------------------
	void TickStaminaDrain(float DeltaTime);
	void TickStaminaRecovery(float DeltaTime);
	void TickLongTermFatigue(float DeltaTime);
	void UpdateFatigueLevel();
	float ComputeRecoveryRate() const;
	void InitDefaultDrainRates();

	UPROPERTY()
	float CurrentStamina = 100.f;

	/** Long-term fatigue [0..1]. Increases over the mission, never fully resets. */
	UPROPERTY()
	float LongTermFatigue = 0.f;

	UPROPERTY()
	ESHFatigueLevel CurrentFatigueLevel = ESHFatigueLevel::Fresh;

	/** Active continuous drain activities (bitmask-like set). */
	TSet<ESHStaminaDrainType> ActiveDrains;

	/** Injury penalty multiplier [0..1]. 0 = no injury, 1 = maximum. */
	float InjuryPenalty = 0.f;

	/** Current carried weight in kg. */
	float CarriedWeightKg = 0.f;

	/** Breath hold state. */
	bool bIsHoldingBreath = false;
	float BreathHoldElapsed = 0.f;

	/** Time since last stamina was fully depleted (for recovery delay). */
	float TimeSinceDepleted = 0.f;
	bool bWasDepleted = false;
};
