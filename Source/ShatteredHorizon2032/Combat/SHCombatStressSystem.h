// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SHCombatStressSystem.generated.h"

/**
 * Stress source — what is contributing to the player's combat stress.
 * Multiple sources compound; stress accumulates faster than it decays.
 */
UENUM(BlueprintType)
enum class ESHStressSource : uint8
{
	IncomingFire		UMETA(DisplayName = "Incoming Fire"),
	NearExplosion		UMETA(DisplayName = "Near Explosion"),
	WitnessCasualty		UMETA(DisplayName = "Witness Casualty"),
	SustainedCombat		UMETA(DisplayName = "Sustained Combat"),
	PersonalWound		UMETA(DisplayName = "Personal Wound"),
	Isolation			UMETA(DisplayName = "Isolation"),
	FriendlyFire		UMETA(DisplayName = "Friendly Fire"),
};

/**
 * Combat Stress Reaction severity levels.
 * Maps to physiological heart-rate zones from doctrine.
 */
UENUM(BlueprintType)
enum class ESHStressLevel : uint8
{
	/** < 72 bpm equivalent. Normal function. */
	Calm		UMETA(DisplayName = "Calm"),

	/** 72-115 bpm. Heightened awareness, fine motor intact. */
	Alert		UMETA(DisplayName = "Alert"),

	/** 115-145 bpm. Complex motor skills begin degrading. */
	Stressed	UMETA(DisplayName = "Stressed"),

	/** 145-175 bpm. Tunnel vision, auditory exclusion, fine motor loss. */
	HighStress	UMETA(DisplayName = "High Stress"),

	/** > 175 bpm. Cognitive collapse, irrational behavior, time distortion. */
	CombatStressReaction UMETA(DisplayName = "Combat Stress Reaction"),
};

// -----------------------------------------------------------------------
//  Delegates
// -----------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStressLevelChanged, ESHStressLevel, NewLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCombatStressReaction);

/**
 * USHCombatStressSystem
 *
 * Models the physiological effects of combat stress on the player.
 * Distinct from suppression (which is immediate, round-proximity-based).
 * Stress accumulates from sustained combat exposure, proximity to
 * explosions, witnessing casualties, and being wounded.
 *
 * Effects at each level (cumulative):
 *   Alert:    Slight aim sway increase
 *   Stressed: Fine motor degradation (reload/aim), peripheral narrowing starts
 *   HighStress: Tunnel vision vignette, auditory exclusion, aim sway severe
 *   CSR:      Time distortion, cognitive narrowing, potential temporary freeze
 *
 * Doctrine: "Heart rate escalates from ~72 bpm at rest to 160+ bpm under fire."
 */
UCLASS(ClassGroup = (SH), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHCombatStressSystem : public UActorComponent
{
	GENERATED_BODY()

public:
	USHCombatStressSystem();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ------------------------------------------------------------------
	//  Stress input
	// ------------------------------------------------------------------

	/** Report a stress event with magnitude (0..1). */
	UFUNCTION(BlueprintCallable, Category = "SH|Stress")
	void AddStress(ESHStressSource Source, float Magnitude = 1.0f);

	/** Reset stress to calm (e.g., after long safe period). */
	UFUNCTION(BlueprintCallable, Category = "SH|Stress")
	void ResetStress();

	// ------------------------------------------------------------------
	//  Queries
	// ------------------------------------------------------------------

	/** Raw stress value [0..1]. */
	UFUNCTION(BlueprintPure, Category = "SH|Stress")
	float GetStressValue() const { return StressValue; }

	/** Simulated heart rate (bpm). */
	UFUNCTION(BlueprintPure, Category = "SH|Stress")
	float GetHeartRate() const { return CurrentHeartRate; }

	/** Current stress level bracket. */
	UFUNCTION(BlueprintPure, Category = "SH|Stress")
	ESHStressLevel GetStressLevel() const { return CurrentLevel; }

	// ------------------------------------------------------------------
	//  Effect multipliers (consumed by other systems)
	// ------------------------------------------------------------------

	/** Aim sway multiplier (1.0 = normal, higher = worse). */
	UFUNCTION(BlueprintPure, Category = "SH|Stress|Effects")
	float GetAimSwayMultiplier() const;

	/** Reload speed multiplier (1.0 = normal, higher = slower). */
	UFUNCTION(BlueprintPure, Category = "SH|Stress|Effects")
	float GetReloadSpeedMultiplier() const;

	/** Tunnel vision vignette intensity [0..1]. */
	UFUNCTION(BlueprintPure, Category = "SH|Stress|Effects")
	float GetTunnelVisionIntensity() const;

	/** Auditory exclusion strength [0..1]. 0 = full hearing, 1 = near-deaf. */
	UFUNCTION(BlueprintPure, Category = "SH|Stress|Effects")
	float GetAuditoryExclusionStrength() const;

	/** Time perception distortion factor. <1 = time feels slower. */
	UFUNCTION(BlueprintPure, Category = "SH|Stress|Effects")
	float GetTimeDistortionFactor() const;

	/** Cognitive narrowing — UI responsiveness penalty [0..1]. */
	UFUNCTION(BlueprintPure, Category = "SH|Stress|Effects")
	float GetCognitiveNarrowingFactor() const;

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Base stress impulse per source type. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Stress|Config")
	TMap<ESHStressSource, float> StressImpulses;

	/** Stress decay rate per second when no new stress is incoming. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Stress|Config")
	float DecayRate = 0.03f;

	/** Grace period (seconds) after last stress event before decay begins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Stress|Config")
	float DecayGracePeriod = 5.0f;

	/** Accelerated decay rate when in a safe position (in cover, no threats). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Stress|Config")
	float SafeDecayRate = 0.08f;

	/** Whether the character is currently in a safe position (set by external systems). */
	UPROPERTY(BlueprintReadWrite, Category = "SH|Stress")
	bool bIsInSafePosition = false;

	/** Resting heart rate (bpm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Stress|Config")
	float RestingHeartRate = 72.0f;

	/** Maximum heart rate under extreme stress (bpm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Stress|Config")
	float MaxHeartRate = 190.0f;

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Stress")
	FOnStressLevelChanged OnStressLevelChanged;

	UPROPERTY(BlueprintAssignable, Category = "SH|Stress")
	FOnCombatStressReaction OnCombatStressReaction;

private:
	void UpdateHeartRate(float DeltaTime);
	void UpdateStressLevel();

	/** Raw stress accumulator [0..1]. */
	float StressValue = 0.f;

	/** Simulated heart rate (bpm). Lerps toward target based on stress. */
	float CurrentHeartRate = 72.f;

	/** Current stress level bracket. */
	ESHStressLevel CurrentLevel = ESHStressLevel::Calm;

	/** Time since last stress impulse was received. */
	float TimeSinceLastStress = 0.f;

	/** Whether CSR has been triggered this combat (prevents re-triggering). */
	bool bCSRTriggered = false;
};
