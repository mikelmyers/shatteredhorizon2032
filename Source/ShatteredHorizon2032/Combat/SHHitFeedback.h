// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SHDamageSystem.h"
#include "ShatteredHorizon2032/Weapons/SHBallisticsSystem.h"
#include "SHHitFeedback.generated.h"

class UAnimMontage;
class UNiagaraSystem;
class USHCameraSystem;
class USkeletalMeshComponent;

// ─────────────────────────────────────────────────────────────
//  Delegates
// ─────────────────────────────────────────────────────────────

/** Broadcast when the owning character takes damage — UI consumes this for directional indicators. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnHitIndicator,
	float, Angle,
	float, Intensity,
	float, Duration);

/** Broadcast when the owning character deals damage — UI consumes this for hit markers. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnHitMarkerTriggered,
	bool, bKill);

// ─────────────────────────────────────────────────────────────
//  Component
// ─────────────────────────────────────────────────────────────

/**
 * USHHitFeedback
 *
 * Actor component that provides visceral feedback when characters
 * take or deal damage. Handles four feedback channels:
 *
 *  1. Screen punch  — directional camera nudge via USHCameraSystem
 *  2. Enemy flinch  — animation montages on hit characters per wound zone
 *  3. Hit indicator — screen-space angle + intensity delegate for UI
 *  4. Impact VFX    — Niagara particle systems at impact point per surface material
 *
 * Attach to any combatant character alongside USHDamageSystem.
 * Call OnDamageReceived for incoming hits and OnDamageDealt for outgoing.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHHitFeedback : public UActorComponent
{
	GENERATED_BODY()

public:
	USHHitFeedback();

	// ------------------------------------------------------------------
	//  UActorComponent overrides
	// ------------------------------------------------------------------
	virtual void BeginPlay() override;

	// ------------------------------------------------------------------
	//  Damage hooks
	// ------------------------------------------------------------------

	/**
	 * Called when the owning character receives damage.
	 * Triggers screen punch, hit indicator, and impact VFX.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|HitFeedback")
	void OnDamageReceived(const FSHDamageInfo& Info, const FSHDamageResult& Result);

	/**
	 * Called when the owning character deals damage to another actor.
	 * Triggers hit marker delegate and enemy flinch montage.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|HitFeedback")
	void OnDamageDealt(const FSHDamageInfo& Info, const FSHDamageResult& Result);

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	/** Fired when a directional hit indicator should be displayed. */
	UPROPERTY(BlueprintAssignable, Category = "SH|HitFeedback")
	FOnHitIndicator OnHitIndicator;

	/** Fired when the player lands a hit on an enemy. */
	UPROPERTY(BlueprintAssignable, Category = "SH|HitFeedback")
	FOnHitMarkerTriggered OnHitMarkerTriggered;

protected:
	// ------------------------------------------------------------------
	//  Screen punch configuration
	// ------------------------------------------------------------------

	/** Base intensity multiplier for screen punch on damage received. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|HitFeedback|ScreenPunch")
	float PunchIntensity = 1.0f;

	/** Intensity multiplier applied on top of base when the hit is lethal. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|HitFeedback|ScreenPunch")
	float LethalPunchMultiplier = 2.0f;

	/** Maximum punch intensity clamp. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|HitFeedback|ScreenPunch")
	float MaxPunchIntensity = 3.0f;

	// ------------------------------------------------------------------
	//  Hit indicator configuration
	// ------------------------------------------------------------------

	/** Duration (seconds) the directional hit indicator stays visible. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|HitFeedback|Indicator")
	float IndicatorDuration = 1.5f;

	/** Minimum damage (post-armor) to trigger a hit indicator. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|HitFeedback|Indicator")
	float IndicatorMinDamage = 1.0f;

	// ------------------------------------------------------------------
	//  Enemy flinch configuration
	// ------------------------------------------------------------------

	/** Flinch montage per hit zone. Played on the hit character's mesh. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|HitFeedback|Flinch")
	TMap<ESHHitZone, TSoftObjectPtr<UAnimMontage>> FlinchMontages;

	/** Playback rate for flinch montages. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|HitFeedback|Flinch")
	float FlinchPlayRate = 1.2f;

	// ------------------------------------------------------------------
	//  Impact VFX configuration
	// ------------------------------------------------------------------

	/** Niagara system per surface material type. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|HitFeedback|VFX")
	TMap<ESHPenetrableMaterial, TSoftObjectPtr<UNiagaraSystem>> ImpactVFXMap;

	/** Default Niagara system used when no material-specific entry exists. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|HitFeedback|VFX")
	TSoftObjectPtr<UNiagaraSystem> DefaultImpactVFX;

	/** Scale applied to spawned impact VFX. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|HitFeedback|VFX")
	FVector ImpactVFXScale = FVector(1.0f);

private:
	// ------------------------------------------------------------------
	//  Internal helpers
	// ------------------------------------------------------------------

	/** Apply a directional screen punch through the camera system. */
	void ApplyScreenPunch(const FSHDamageInfo& Info, const FSHDamageResult& Result);

	/** Broadcast the hit indicator delegate with computed screen-space angle. */
	void BroadcastHitIndicator(const FSHDamageInfo& Info, const FSHDamageResult& Result);

	/** Trigger a flinch montage on the hit character. */
	void TriggerFlinchMontage(AActor* HitActor, ESHHitZone HitZone);

	/** Spawn impact VFX at the hit location. */
	void SpawnImpactVFX(const FVector& ImpactLocation, const FVector& ImpactNormal, ESHPenetrableMaterial SurfaceMaterial);

	/** Find the target character near the impact location and trigger flinch. */
	void TriggerFlinchOnTarget(const FSHDamageInfo& Info, const FSHDamageResult& Result);

	/**
	 * Compute the screen-space angle (degrees) from screen center to a world position.
	 * 0 = top, 90 = right, 180 = bottom, 270 = left.
	 * Returns false if the owner has no valid controller/camera.
	 */
	bool ComputeScreenAngle(const FVector& WorldOrigin, float& OutAngle) const;

	/**
	 * Determine the surface material from a physical material or hit result.
	 * Defaults to Flesh for character hits with no physical material override.
	 */
	ESHPenetrableMaterial DetermineSurfaceMaterial(const FVector& ImpactLocation) const;

	/** Cached camera system on the owning character. */
	UPROPERTY()
	TObjectPtr<USHCameraSystem> CachedCameraSystem = nullptr;
};
