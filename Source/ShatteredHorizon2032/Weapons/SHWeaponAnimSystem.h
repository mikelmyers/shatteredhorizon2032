// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SHWeaponData.h"
#include "SHWeaponBase.h"
#include "SHWeaponAnimSystem.generated.h"

/* -----------------------------------------------------------------------
 *  Weapon bob configuration per stance
 * --------------------------------------------------------------------- */

USTRUCT(BlueprintType)
struct FSHWeaponBobConfig
{
	GENERATED_BODY()

	/** Horizontal bob amplitude (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bob", meta = (ClampMin = "0"))
	float HorizontalAmplitude = 1.2f;

	/** Vertical bob amplitude (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bob", meta = (ClampMin = "0"))
	float VerticalAmplitude = 0.8f;

	/** Bob frequency multiplier (higher = faster oscillation). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Bob", meta = (ClampMin = "0"))
	float FrequencyMultiplier = 1.0f;
};

/* -----------------------------------------------------------------------
 *  USHWeaponAnimSystem — Procedural first-person weapon animation driver
 * --------------------------------------------------------------------- */

UCLASS(ClassGroup = (ShatteredHorizon), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHWeaponAnimSystem : public UActorComponent
{
	GENERATED_BODY()

public:
	USHWeaponAnimSystem();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/* --- Public Interface --- */

	/** Load recoil pattern data from weapon data asset. */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Animation")
	void SetRecoilPattern(const FSHRecoilPattern& InPattern);

	/** Called by the weapon each time a shot is fired. ShotIndex is 0-based consecutive shot count. */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Animation")
	void OnShotFired(int32 ShotIndex);

	/** Returns the combined procedural transform offset for all active effects. */
	UFUNCTION(BlueprintPure, Category = "Weapon|Animation")
	FTransform GetProceduralTransform() const;

	/** Set movement parameters from the owning character. */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Animation")
	void SetMovementState(float Speed, bool bSprinting, ESHStance Stance);

	/** Set ADS blend alpha (0 = hip, 1 = fully ADS). */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Animation")
	void SetADSAlpha(float Alpha);

	/** Set fatigue level (0-1) for breathing sway modulation. */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Animation")
	void SetFatigueLevel(float Level);

	/** Called when a reload begins. */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Animation")
	void OnReloadStarted();

	/** Called when a reload finishes. */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Animation")
	void OnReloadFinished();

	/* =================================================================
	 *  Configuration — Recoil
	 * =============================================================== */

	/** Scale applied to vertical recoil kick (cm of weapon pitch offset). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Recoil", meta = (ClampMin = "0"))
	float RecoilKickPitchScale = 1.8f;

	/** Scale applied to horizontal recoil kick (cm of weapon yaw offset). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Recoil", meta = (ClampMin = "0"))
	float RecoilKickYawScale = 0.6f;

	/** How far the weapon pushes back toward the camera per shot (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Recoil", meta = (ClampMin = "0"))
	float RecoilPushbackCm = 1.5f;

	/** Spring stiffness for recoil snap-back. Higher = snappier recovery. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Recoil", meta = (ClampMin = "0"))
	float RecoilRecoverySpeed = 12.0f;

	/** Damping ratio for recoil spring (0 = no damping, 1 = critically damped). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Recoil", meta = (ClampMin = "0", ClampMax = "2"))
	float RecoilRecoveryDamping = 0.75f;

	/* =================================================================
	 *  Configuration — Muzzle Climb
	 * =============================================================== */

	/** Maximum accumulated muzzle climb (degrees) before clamping. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|MuzzleClimb", meta = (ClampMin = "0"))
	float MaxMuzzleClimbDegrees = 8.0f;

	/** How fast muzzle climb recovers when the player counters with mouse (degrees/sec). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|MuzzleClimb", meta = (ClampMin = "0"))
	float MuzzleClimbRecoveryRate = 6.0f;

	/** Curve shape for muzzle climb recovery (1 = linear, < 1 = fast start, > 1 = slow start). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|MuzzleClimb", meta = (ClampMin = "0.1", ClampMax = "5"))
	float MuzzleClimbRecoveryCurveExp = 1.5f;

	/* =================================================================
	 *  Configuration — Weapon Bob
	 * =============================================================== */

	/** Bob configuration per stance. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Bob")
	FSHWeaponBobConfig BobStanding;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Bob")
	FSHWeaponBobConfig BobCrouching;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Bob")
	FSHWeaponBobConfig BobProne;

	/** Base bob frequency in Hz (cycles per second at a normalized run speed). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Bob", meta = (ClampMin = "0"))
	float BobBaseFrequency = 6.0f;

	/** Character speed that corresponds to full bob amplitude (cm/s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Bob", meta = (ClampMin = "1"))
	float BobMaxSpeedReference = 600.0f;

	/** Multiplier applied to bob when sprinting. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Bob", meta = (ClampMin = "0"))
	float BobSprintMultiplier = 1.4f;

	/** How quickly the bob amplitude fades in/out when starting/stopping movement (per second). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Bob", meta = (ClampMin = "0"))
	float BobBlendSpeed = 8.0f;

	/* =================================================================
	 *  Configuration — ADS Transition
	 * =============================================================== */

	/** Position offset applied when fully ADS (local space, cm). Typically brings weapon up and closer to center. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|ADS")
	FVector ADSPositionOffset = FVector(5.0f, 0.0f, -2.0f);

	/** Rotation offset applied when fully ADS (degrees, local space). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|ADS")
	FRotator ADSRotationOffset = FRotator(0.0f, 0.0f, 0.0f);

	/** Interpolation speed for ADS transition (per second). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|ADS", meta = (ClampMin = "0"))
	float ADSLerpSpeed = 10.0f;

	/* =================================================================
	 *  Configuration — Reload Motion
	 * =============================================================== */

	/** How far the weapon dips down during reload (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Reload", meta = (ClampMin = "0"))
	float ReloadDipAmountCm = 6.0f;

	/** Tilt angle during reload (degrees, rolls weapon to the side). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Reload", meta = (ClampMin = "0"))
	float ReloadTiltDegrees = 15.0f;

	/** Speed of the reload dip lerp (per second). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Reload", meta = (ClampMin = "0"))
	float ReloadMotionSpeed = 5.0f;

	/* =================================================================
	 *  Configuration — Breathing Sway (Scoped)
	 * =============================================================== */

	/** Base breathing sway amplitude (cm) when at zero fatigue and scoped. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Breathing", meta = (ClampMin = "0"))
	float BreathingSwayBaseAmplitude = 0.3f;

	/** Additional sway amplitude at full fatigue (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Breathing", meta = (ClampMin = "0"))
	float BreathingSwayFatigueAmplitude = 1.8f;

	/** Breathing cycle frequency (Hz). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Breathing", meta = (ClampMin = "0"))
	float BreathingFrequency = 0.25f;

	/** Frequency increase at full fatigue (additive Hz). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|Breathing", meta = (ClampMin = "0"))
	float BreathingFatigueFrequencyBonus = 0.35f;

	/* =================================================================
	 *  Configuration — Idle Sway (Perlin Noise)
	 * =============================================================== */

	/** Amplitude of idle sway in cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|IdleSway", meta = (ClampMin = "0"))
	float IdleSwayAmplitude = 0.4f;

	/** Frequency of Perlin noise sampling for idle sway. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|IdleSway", meta = (ClampMin = "0"))
	float IdleSwayFrequency = 0.8f;

	/** Rotation amplitude of idle sway (degrees). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Animation|IdleSway", meta = (ClampMin = "0"))
	float IdleSwayRotationDegrees = 0.6f;

protected:
	virtual void BeginPlay() override;

private:
	/* --- Tick Sub-systems --- */

	void TickRecoil(float DeltaTime);
	void TickMuzzleClimb(float DeltaTime);
	void TickWeaponBob(float DeltaTime);
	void TickADSTransition(float DeltaTime);
	void TickReloadMotion(float DeltaTime);
	void TickBreathingSway(float DeltaTime);
	void TickIdleSway(float DeltaTime);

	/* --- Helpers --- */

	const FSHWeaponBobConfig& GetBobConfigForStance(ESHStance InStance) const;
	float SamplePerlinNoise1D(float X) const;

	/* --- Recoil State --- */

	FSHRecoilPattern ActiveRecoilPattern;

	/** Current recoil kick offset (pitch, yaw, pushback). Decays over time via spring. */
	FVector RecoilKickCurrent = FVector::ZeroVector;

	/** Recoil kick velocity for spring simulation. */
	FVector RecoilKickVelocity = FVector::ZeroVector;

	/* --- Muzzle Climb State --- */

	/** Accumulated muzzle climb in degrees (vertical). */
	float MuzzleClimbAccumulated = 0.0f;

	/** Accumulated muzzle climb horizontal (degrees). */
	float MuzzleClimbHorizontal = 0.0f;

	/* --- Weapon Bob State --- */

	/** Time accumulator for bob oscillation. */
	float BobTime = 0.0f;

	/** Current bob blend alpha (0 = no bob, 1 = full bob). */
	float BobBlendAlpha = 0.0f;

	/** Current bob output offset. */
	FVector BobOffset = FVector::ZeroVector;

	/* --- ADS State --- */

	/** Current interpolated ADS alpha. */
	float CurrentADSAlpha = 0.0f;

	/** Target ADS alpha set externally. */
	float TargetADSAlpha = 0.0f;

	/* --- Reload State --- */

	/** True while a reload is in progress. */
	bool bIsReloading = false;

	/** Current reload motion blend (0 = idle, 1 = fully dipped). */
	float ReloadMotionAlpha = 0.0f;

	/* --- Breathing Sway State --- */

	float BreathingTime = 0.0f;
	FVector BreathingSwayOffset = FVector::ZeroVector;

	/* --- Idle Sway State --- */

	float IdleSwayTime = 0.0f;
	FVector IdleSwayOffset = FVector::ZeroVector;
	FRotator IdleSwayRotation = FRotator::ZeroRotator;

	/* --- External Input State --- */

	float CurrentMoveSpeed = 0.0f;
	bool bIsSprinting = false;
	ESHStance CurrentStance = ESHStance::Standing;
	float FatigueLevel = 0.0f;

	/* --- Cached Output Transforms --- */

	FVector ProceduralLocationOffset = FVector::ZeroVector;
	FRotator ProceduralRotationOffset = FRotator::ZeroRotator;
};
