// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SHCameraSystem.generated.h"

class UCameraComponent;
class USHFatigueSystem;
class USHSuppressionSystem;

/**
 * Camera effect context — snapshot of all inputs driving camera behavior.
 * Rebuilt each frame from character state.
 */
USTRUCT(BlueprintType)
struct FSHCameraContext
{
	GENERATED_BODY()

	/** Character ground speed (cm/s). */
	UPROPERTY(BlueprintReadOnly) float GroundSpeed = 0.f;

	/** True if the character is sprinting. */
	UPROPERTY(BlueprintReadOnly) bool bIsSprinting = false;

	/** True if aiming down sights. */
	UPROPERTY(BlueprintReadOnly) bool bIsADS = false;

	/** Normalized suppression [0..1]. */
	UPROPERTY(BlueprintReadOnly) float Suppression = 0.f;

	/** Normalized fatigue [0..1]. */
	UPROPERTY(BlueprintReadOnly) float Fatigue = 0.f;

	/** Hip FOV target. */
	UPROPERTY(BlueprintReadOnly) float HipFOV = 90.f;

	/** ADS FOV target (from weapon data). */
	UPROPERTY(BlueprintReadOnly) float ADSFOV = 55.f;

	/** ADS transition speed (seconds). */
	UPROPERTY(BlueprintReadOnly) float ADSTransitionTime = 0.2f;
};

/**
 * USHCameraSystem
 *
 * Drives first-person camera feel: head bob, weapon sway idle,
 * ADS FOV transitions, contextual camera shake (explosions,
 * suppression, sprint), and suppression visual effects (vignetting,
 * desaturation).
 *
 * Reads state from SHFatigueSystem and SHSuppressionSystem.
 * Designed to be added to ASHPlayerCharacter alongside the
 * existing UCameraComponent.
 */
UCLASS(ClassGroup = (Core), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHCameraSystem : public UActorComponent
{
	GENERATED_BODY()

public:
	USHCameraSystem();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
							   FActorComponentTickFunction* ThisTickFunction) override;

	// ------------------------------------------------------------------
	//  External triggers
	// ------------------------------------------------------------------

	/** Trigger explosion shake. Intensity scales inverse-square with distance. */
	UFUNCTION(BlueprintCallable, Category = "SH|Camera")
	void OnNearbyExplosion(const FVector& ExplosionOrigin, float MaxRadius = 3000.f);

	/** Force a one-shot directional flinch (e.g. taking damage). */
	UFUNCTION(BlueprintCallable, Category = "SH|Camera")
	void ApplyScreenPunch(const FVector& DamageDirection, float Intensity = 1.f);

	/** Update the camera context from character state each frame. */
	UFUNCTION(BlueprintCallable, Category = "SH|Camera")
	void SetCameraContext(const FSHCameraContext& InContext);

	// ------------------------------------------------------------------
	//  Queries
	// ------------------------------------------------------------------

	/** Current FOV after ADS lerp. */
	UFUNCTION(BlueprintPure, Category = "SH|Camera")
	float GetCurrentFOV() const { return CurrentFOV; }

	/** Current vignette intensity driven by suppression. */
	UFUNCTION(BlueprintPure, Category = "SH|Camera")
	float GetVignetteIntensity() const { return CurrentVignetteIntensity; }

	/** Current desaturation strength driven by suppression. */
	UFUNCTION(BlueprintPure, Category = "SH|Camera")
	float GetDesaturationStrength() const { return CurrentDesaturation; }

protected:
	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Base hip FOV. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Camera|FOV")
	float DefaultHipFOV = 90.f;

	/** FOV lerp speed (degrees per second). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Camera|FOV")
	float FOVInterpSpeed = 12.f;

	// --- Head bob ---

	/** Head bob amplitude while walking (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Camera|HeadBob")
	float WalkBobAmplitude = 1.2f;

	/** Head bob amplitude while sprinting (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Camera|HeadBob")
	float SprintBobAmplitude = 3.0f;

	/** Head bob frequency (cycles per second). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Camera|HeadBob")
	float BobFrequency = 2.4f;

	/** Head bob is suppressed during ADS. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Camera|HeadBob")
	float ADSBobReduction = 0.1f;

	// --- Camera shake ---

	/** Camera shake class for suppression. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Camera|Shake")
	TSubclassOf<UCameraShakeBase> SuppressionShakeClass;

	/** Camera shake class for explosions. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Camera|Shake")
	TSubclassOf<UCameraShakeBase> ExplosionShakeClass;

	/** Camera shake class for sprint rhythm. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Camera|Shake")
	TSubclassOf<UCameraShakeBase> SprintShakeClass;

	// --- Suppression FX ---

	/** Maximum vignette intensity at full suppression. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Camera|Suppression")
	float MaxVignetteIntensity = 0.6f;

	/** Maximum desaturation at full suppression. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Camera|Suppression")
	float MaxDesaturation = 0.5f;

	/** Suppression FX interpolation speed. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Camera|Suppression")
	float SuppressionFXInterpSpeed = 4.f;

	// --- Screen punch ---

	/** Max angular displacement for screen punch (degrees). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Camera|Punch")
	float MaxPunchAngle = 3.f;

	/** Screen punch recovery speed (degrees per second). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Camera|Punch")
	float PunchRecoverySpeed = 15.f;

private:
	void TickHeadBob(float DeltaTime);
	void TickFOVTransition(float DeltaTime);
	void TickSuppressionFX(float DeltaTime);
	void TickScreenPunch(float DeltaTime);

	/** Cached reference to the owning character's camera. */
	UPROPERTY()
	TObjectPtr<UCameraComponent> OwnerCamera = nullptr;

	FSHCameraContext Context;

	// --- Head bob state ---
	float BobTimer = 0.f;
	FVector CurrentBobOffset = FVector::ZeroVector;

	// --- FOV state ---
	float CurrentFOV = 90.f;
	float TargetFOV = 90.f;

	// --- Suppression FX state ---
	float CurrentVignetteIntensity = 0.f;
	float CurrentDesaturation = 0.f;

	// --- Screen punch state ---
	FRotator PunchRotation = FRotator::ZeroRotator;
	FRotator PunchVelocity = FRotator::ZeroRotator;
};
