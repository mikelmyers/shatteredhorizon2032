// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SHPrimordiaAstraea.generated.h"

/* -----------------------------------------------------------------------
 *  Structs
 * --------------------------------------------------------------------- */

USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHCognitiveState
{
	GENERATED_BODY()

	/** Current stress level (0 = calm, 1 = extreme stress). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cognitive", meta = (ClampMin = "0", ClampMax = "1"))
	float StressLevel = 0.0f;

	/**
	 * Decision quality (0 = severely impaired, 1 = optimal).
	 * Degrades as stress increases. Maps to AI decision cycle variance
	 * and accuracy in other subsystems.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cognitive", meta = (ClampMin = "0", ClampMax = "1"))
	float DecisionQuality = 1.0f;

	/** Alertness level (0 = drowsy/fatigued, 1 = fully alert). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cognitive", meta = (ClampMin = "0", ClampMax = "1"))
	float Alertness = 0.8f;

	/** Overall combat effectiveness derived from the other factors. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Cognitive", meta = (ClampMin = "0", ClampMax = "1"))
	float CombatEffectiveness = 1.0f;
};

/* -----------------------------------------------------------------------
 *  Delegates
 * --------------------------------------------------------------------- */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnCognitiveStateChanged,
	AActor*, AffectedActor,
	const FSHCognitiveState&, NewState);

/* -----------------------------------------------------------------------
 *  USHPrimordiaAstraea
 * --------------------------------------------------------------------- */

/**
 * USHPrimordiaAstraea
 *
 * Morale/cognitive state component extending the existing SHMoraleSystem
 * with deeper cognitive modeling. Tracks cumulative stress from sustained
 * combat exposure and models how stress degrades decision quality.
 *
 * Stress sources: incoming fire, casualties, failed orders, isolation,
 * ambush surprise.
 *
 * Stress recovery: cover, squad proximity, officer presence, time
 * without enemy contact.
 *
 * DecisionQuality output is consumed by other Primordia subsystems
 * (Aletheia, Simulon) to add variance and accuracy degradation.
 */
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHPrimordiaAstraea : public UActorComponent
{
	GENERATED_BODY()

public:
	USHPrimordiaAstraea();

	// ------------------------------------------------------------------
	//  UActorComponent overrides
	// ------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ------------------------------------------------------------------
	//  Stress input
	// ------------------------------------------------------------------

	/**
	 * Apply a stress impulse from a named source.
	 * @param Amount  Stress increment (0-1 range, added to current stress).
	 * @param Source  Named source for logging/tracking (e.g. "IncomingFire", "CasualtyNearby").
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Astraea")
	void ApplyStressor(float Amount, FName Source);

	// ------------------------------------------------------------------
	//  Recovery context (set by other systems each frame)
	// ------------------------------------------------------------------

	/** Set whether the unit is currently in cover. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Astraea")
	void SetInCover(bool bCover);

	/** Set the number of nearby squad members. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Astraea")
	void SetSquadProximityCount(int32 Count);

	/** Set whether an officer/leader is nearby. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Astraea")
	void SetOfficerPresent(bool bPresent);

	/** Set whether the unit is currently under active fire. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Astraea")
	void SetUnderFire(bool bFire);

	// ------------------------------------------------------------------
	//  Queries
	// ------------------------------------------------------------------

	/** Get the full cognitive state snapshot. */
	UFUNCTION(BlueprintPure, Category = "SH|Primordia|Astraea")
	FSHCognitiveState GetCognitiveState() const { return CognitiveState; }

	UFUNCTION(BlueprintPure, Category = "SH|Primordia|Astraea")
	float GetStressLevel() const { return CognitiveState.StressLevel; }

	UFUNCTION(BlueprintPure, Category = "SH|Primordia|Astraea")
	float GetDecisionQuality() const { return CognitiveState.DecisionQuality; }

	UFUNCTION(BlueprintPure, Category = "SH|Primordia|Astraea")
	float GetAlertness() const { return CognitiveState.Alertness; }

	UFUNCTION(BlueprintPure, Category = "SH|Primordia|Astraea")
	float GetCombatEffectiveness() const { return CognitiveState.CombatEffectiveness; }

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Primordia|Astraea")
	FOnCognitiveStateChanged OnCognitiveStateChanged;

protected:
	// ------------------------------------------------------------------
	//  Config — stress recovery rates
	// ------------------------------------------------------------------

	/** Base stress recovery per second (always active). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Primordia|Astraea|Config", meta = (ClampMin = "0"))
	float BaseRecoveryRate = 0.005f;

	/** Additional recovery per second while in cover. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Primordia|Astraea|Config", meta = (ClampMin = "0"))
	float CoverRecoveryBonus = 0.015f;

	/** Additional recovery per nearby squad member (per second). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Primordia|Astraea|Config", meta = (ClampMin = "0"))
	float SquadProximityRecoveryPerMember = 0.003f;

	/** Additional recovery when officer is present (per second). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Primordia|Astraea|Config", meta = (ClampMin = "0"))
	float OfficerRecoveryBonus = 0.01f;

	/** Additional recovery per second when not in contact (no fire for N seconds). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Primordia|Astraea|Config", meta = (ClampMin = "0"))
	float NoContactRecoveryBonus = 0.02f;

	/** Seconds without fire required to trigger no-contact recovery bonus. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Primordia|Astraea|Config", meta = (ClampMin = "0"))
	float NoContactThreshold = 15.0f;

	// ------------------------------------------------------------------
	//  Config — stress-to-quality mapping
	// ------------------------------------------------------------------

	/** Stress level at which decision quality starts degrading. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Primordia|Astraea|Config", meta = (ClampMin = "0", ClampMax = "1"))
	float StressQualityDegradationStart = 0.3f;

	/** Minimum decision quality at maximum stress. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Primordia|Astraea|Config", meta = (ClampMin = "0", ClampMax = "1"))
	float MinDecisionQuality = 0.2f;

	/** Alertness decay rate per second (simulates fatigue over long engagements). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Primordia|Astraea|Config", meta = (ClampMin = "0"))
	float AlertnessDecayRate = 0.001f;

	/** Alertness boost when a stressor is applied (adrenaline spike). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Primordia|Astraea|Config", meta = (ClampMin = "0"))
	float StressorAlertnessBoost = 0.1f;

private:
	void TickStressRecovery(float DeltaTime);
	void UpdateDerivedState();

	FSHCognitiveState CognitiveState;

	bool bInCover = false;
	int32 SquadProximityCount = 0;
	bool bOfficerPresent = false;
	bool bUnderFire = false;

	/** Time since last incoming fire. */
	float TimeSinceLastFire = 0.0f;

	/** Cumulative stress from individual stressors for logging. */
	TMap<FName, float> StressorHistory;
};
