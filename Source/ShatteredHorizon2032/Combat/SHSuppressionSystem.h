// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SHSuppressionSystem.generated.h"

class AActor;
class ACharacter;
class UPostProcessComponent;

// ─────────────────────────────────────────────────────────────
//  Enums
// ─────────────────────────────────────────────────────────────

/** Suppression intensity bracket. */
UENUM(BlueprintType)
enum class ESHSuppressionLevel : uint8
{
	None	UMETA(DisplayName = "None"),
	Light	UMETA(DisplayName = "Light"),
	Medium	UMETA(DisplayName = "Medium"),
	Heavy	UMETA(DisplayName = "Heavy"),
	Pinned	UMETA(DisplayName = "Pinned")
};

/** Caliber categories that determine suppression magnitude. */
UENUM(BlueprintType)
enum class ESHCaliber : uint8
{
	Pistol			UMETA(DisplayName = "Pistol"),
	IntermediateRifle	UMETA(DisplayName = "Intermediate Rifle"),	// 5.56, 5.45
	FullPowerRifle		UMETA(DisplayName = "Full-Power Rifle"),	// 7.62x51, 7.62x54R
	HeavyMG			UMETA(DisplayName = "Heavy MG"),			// 12.7 / .50 cal
	Shotgun			UMETA(DisplayName = "Shotgun"),
	Explosive		UMETA(DisplayName = "Explosive"),
	Autocannon		UMETA(DisplayName = "Autocannon")			// 20-30 mm
};

// ─────────────────────────────────────────────────────────────
//  Per-character suppression state
// ─────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FSHSuppressionEntry
{
	GENERATED_BODY()

	/** Normalized suppression value [0..1]. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Suppression")
	float SuppressionValue = 0.f;

	/** Discrete bracket derived from SuppressionValue. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Suppression")
	ESHSuppressionLevel Level = ESHSuppressionLevel::None;

	/** World time of the last suppression impulse. */
	UPROPERTY()
	float LastImpulseTime = -100.f;

	/** Accumulated aim wobble multiplier (1 = no wobble). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Suppression")
	float AimWobbleMultiplier = 1.f;

	/** Movement speed multiplier imposed by suppression. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Suppression")
	float MovementSpeedMultiplier = 1.f;

	/** Whether the character can effectively ADS right now. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Suppression")
	bool bCanADS = true;

	/** Whether the character is considered pinned (for AI). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Suppression")
	bool bIsPinned = false;
};

// ─────────────────────────────────────────────────────────────
//  Delegates
// ─────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnSuppressionLevelChanged,
	AActor*, AffectedActor,
	ESHSuppressionLevel, OldLevel,
	ESHSuppressionLevel, NewLevel);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnActorPinned,
	AActor*, PinnedActor,
	float, SuppressionValue);

// ─────────────────────────────────────────────────────────────
//  Subsystem
// ─────────────────────────────────────────────────────────────

/**
 * USHSuppressionSystem
 *
 * Game-instance subsystem that tracks suppression state for every
 * combatant character.  Incoming rounds within a proximity radius
 * accumulate suppression; it decays over time when fire ceases.
 *
 * Suppression drives:
 *  - Aim wobble / ADS denial
 *  - Camera shake
 *  - Movement speed penalties
 *  - Audio effects (heartbeat, tinnitus, muffled world audio)
 *  - Post-process effects (desaturation, vignette, motion blur)
 *  - AI decision-making (pinned AI will not advance)
 */
UCLASS()
class SHATTEREDHORIZON2032_API USHSuppressionSystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ------------------------------------------------------------------
	//  Subsystem lifecycle
	// ------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Called every frame from a world tick delegate. */
	void Tick(float DeltaSeconds);

	// ------------------------------------------------------------------
	//  Registration
	// ------------------------------------------------------------------

	/** Register a character to be tracked by the suppression system. */
	UFUNCTION(BlueprintCallable, Category = "SH|Suppression")
	void RegisterCharacter(AActor* Character);

	/** Remove a character (e.g. on death). */
	UFUNCTION(BlueprintCallable, Category = "SH|Suppression")
	void UnregisterCharacter(AActor* Character);

	// ------------------------------------------------------------------
	//  Suppression impulses
	// ------------------------------------------------------------------

	/**
	 * Report an incoming round near a world position.
	 * All registered characters within ProximityRadius receive suppression.
	 *
	 * @param ImpactLocation	World-space point where the round passed / impacted.
	 * @param Caliber			Caliber class of the projectile.
	 * @param Instigator		The shooter (used to avoid self-suppression).
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Suppression")
	void ReportIncomingRound(const FVector& ImpactLocation, ESHCaliber Caliber, AActor* Instigator);

	/**
	 * Apply an immediate explosion suppression spike to everyone in radius.
	 *
	 * @param Origin		Explosion center.
	 * @param OuterRadius	Maximum suppression falloff radius.
	 * @param InnerRadius	Radius within which suppression is maximal.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Suppression")
	void ReportExplosion(const FVector& Origin, float OuterRadius, float InnerRadius);

	// ------------------------------------------------------------------
	//  Queries
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|Suppression")
	float GetSuppressionValue(const AActor* Character) const;

	UFUNCTION(BlueprintPure, Category = "SH|Suppression")
	ESHSuppressionLevel GetSuppressionLevel(const AActor* Character) const;

	UFUNCTION(BlueprintPure, Category = "SH|Suppression")
	FSHSuppressionEntry GetSuppressionEntry(const AActor* Character) const;

	UFUNCTION(BlueprintPure, Category = "SH|Suppression")
	bool IsCharacterPinned(const AActor* Character) const;

	/** Return the aim wobble multiplier (1.0 = no wobble, higher = worse). */
	UFUNCTION(BlueprintPure, Category = "SH|Suppression")
	float GetAimWobbleMultiplier(const AActor* Character) const;

	/** Return the movement speed multiplier (1.0 = full speed). */
	UFUNCTION(BlueprintPure, Category = "SH|Suppression")
	float GetMovementSpeedMultiplier(const AActor* Character) const;

	// ------------------------------------------------------------------
	//  Post-process / audio helpers
	// ------------------------------------------------------------------

	/**
	 * Compute a 0..1 desaturation strength for the local player.
	 * Intended for use by the HUD / camera manager.
	 */
	UFUNCTION(BlueprintPure, Category = "SH|Suppression|FX")
	float GetDesaturationStrength(const AActor* Character) const;

	UFUNCTION(BlueprintPure, Category = "SH|Suppression|FX")
	float GetVignetteIntensity(const AActor* Character) const;

	UFUNCTION(BlueprintPure, Category = "SH|Suppression|FX")
	float GetTinnitusVolume(const AActor* Character) const;

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Suppression")
	FOnSuppressionLevelChanged OnSuppressionLevelChanged;

	UPROPERTY(BlueprintAssignable, Category = "SH|Suppression")
	FOnActorPinned OnActorPinned;

protected:
	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Radius (cm) within which a passing round applies suppression. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Suppression|Config")
	float ProximityRadius = 500.f;

	/** Base suppression impulse per caliber. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Suppression|Config")
	TMap<ESHCaliber, float> CaliberSuppressionValues;

	/** Suppression decay rate per second when not under fire. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Suppression|Config")
	float DecayRatePerSecond = 0.15f;

	/** Grace period (seconds) after last impulse before decay begins. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Suppression|Config")
	float DecayGracePeriod = 1.5f;

	/** Camera shake class applied during Medium+ suppression. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Suppression|FX")
	TSubclassOf<UCameraShakeBase> SuppressionCameraShake;

	/** Scale applied to camera shake based on suppression (lerped 0..1). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Suppression|FX")
	float MaxCameraShakeScale = 1.5f;

private:
	// ------------------------------------------------------------------
	//  Internal helpers
	// ------------------------------------------------------------------
	void ApplySuppressionImpulse(AActor* Character, float Impulse, float WorldTime);
	void UpdateDerivedState(AActor* Character, FSHSuppressionEntry& Entry);
	ESHSuppressionLevel ComputeLevel(float Value) const;
	void InitDefaultCaliberValues();

	/** Bind to world tick delegate. */
	void BindToWorldTick();
	FDelegateHandle TickDelegateHandle;

	/** Per-character suppression data. */
	UPROPERTY()
	TMap<TObjectPtr<AActor>, FSHSuppressionEntry> SuppressionMap;
};
