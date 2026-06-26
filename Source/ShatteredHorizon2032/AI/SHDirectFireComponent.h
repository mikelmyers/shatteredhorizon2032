// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SHDirectFireComponent.generated.h"

class USoundBase;
class UParticleSystem;
class USoundAttenuation;

/**
 * USHDirectFireComponent
 *
 * Lightweight autonomous small-arms fire for AI characters (PLA enemies and
 * friendly squad). Provides target acquisition, line-of-sight checks, burst
 * fire with probabilistic hits, suppression on near misses, and muzzle
 * flash/report feedback — without requiring a behavior tree asset.
 *
 * Behavior-tree-driven fire (when authored) can disable this via bEnabled.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent), Config = Game)
class SHATTEREDHORIZON2032_API USHDirectFireComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USHDirectFireComponent();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	/** When false, the component does nothing (e.g. replaced by BT logic). */
	UPROPERTY(Config, EditAnywhere, Category = "SH|DirectFire")
	bool bEnabled = true;

	/** True = this shooter targets the player side (set on PLA enemies);
	 *  false = targets ASHEnemyCharacter actors (set on friendly squad). */
	UPROPERTY(EditAnywhere, Category = "SH|DirectFire")
	bool bTargetsPlayerSide = true;

	/** Max engagement range (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "SH|DirectFire")
	float MaxRangeCm = 40000.f;

	/** Base per-round hit probability at point blank. */
	UPROPERTY(Config, EditAnywhere, Category = "SH|DirectFire")
	float BaseHitChance = 0.30f;

	/** Damage applied per hit round. */
	UPROPERTY(Config, EditAnywhere, Category = "SH|DirectFire")
	float DamagePerRound = 13.f;

	/** Rounds per burst. */
	UPROPERTY(Config, EditAnywhere, Category = "SH|DirectFire")
	int32 BurstRounds = 3;

	/** Seconds between rounds within a burst. */
	UPROPERTY(Config, EditAnywhere, Category = "SH|DirectFire")
	float RoundInterval = 0.12f;

	/** Seconds between bursts (randomized ±40%). */
	UPROPERTY(Config, EditAnywhere, Category = "SH|DirectFire")
	float BurstCooldown = 2.6f;

	/** Rounds before a reload pause. */
	UPROPERTY(Config, EditAnywhere, Category = "SH|DirectFire")
	int32 MagazineRounds = 30;

	/** Reload pause duration (s). */
	UPROPERTY(Config, EditAnywhere, Category = "SH|DirectFire")
	float ReloadTime = 2.8f;

	/** Suppression added to the player per near miss. */
	UPROPERTY(Config, EditAnywhere, Category = "SH|DirectFire")
	float SuppressionPerNearMiss = 0.05f;

	// --- Fairness: human reaction + first-contact ranging-in ---
	// Without these, an enemy fires a full-accuracy shot the instant it sees you,
	// so first contact is unfairly lethal (the Ready-or-Not "crack shot" problem).

	/** Min/max human reaction delay before the first burst on a fresh sighting (s). */
	UPROPERTY(Config, EditAnywhere, Category = "SH|DirectFire|Fairness")
	float ReactionDelayMin = 0.45f;

	UPROPERTY(Config, EditAnywhere, Category = "SH|DirectFire|Fairness")
	float ReactionDelayMax = 1.1f;

	/** Seconds over which accuracy ramps from first-contact to full (ranging in). */
	UPROPERTY(Config, EditAnywhere, Category = "SH|DirectFire|Fairness")
	float AccuracyRampTime = 2.0f;

	/** Accuracy multiplier on the first shots after acquiring a target (0..1). */
	UPROPERTY(Config, EditAnywhere, Category = "SH|DirectFire|Fairness")
	float FirstContactAccuracyMult = 0.25f;

	/** Fire report sound. */
	UPROPERTY(Config, EditAnywhere, Category = "SH|DirectFire|FX")
	TSoftObjectPtr<USoundBase> FireSound;

	/** Attenuation for the fire report. */
	UPROPERTY(Config, EditAnywhere, Category = "SH|DirectFire|FX")
	TSoftObjectPtr<USoundAttenuation> FireAttenuation;

	/** Muzzle flash particle system. */
	UPROPERTY(Config, EditAnywhere, Category = "SH|DirectFire|FX")
	TSoftObjectPtr<UParticleSystem> MuzzleFlash;

	/** When true and no line of sight exists, the owner advances on its target
	 *  (fallback when no behavior tree / director is driving movement). */
	UPROPERTY(Config, EditAnywhere, Category = "SH|DirectFire")
	bool bAdvanceWhenNoLOS = true;

	/** Stop advancing within this range of the target (cm). */
	UPROPERTY(Config, EditAnywhere, Category = "SH|DirectFire")
	float AdvanceAcceptanceRadius = 2500.f;

	/** Re-issue the move order at this interval (s). */
	UPROPERTY(Config, EditAnywhere, Category = "SH|DirectFire")
	float AdvanceRepathInterval = 6.f;

protected:
	void AcquireTarget();
	bool HasLineOfSight(AActor* Target, FVector& OutAimPoint) const;
	FVector GetMuzzleLocation() const;
	void FireRound();
	bool IsOwnerCombatCapable() const;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CurrentTarget;

	float NextBurstTime = 0.f;
	float NextRoundTime = 0.f;
	int32 RoundsLeftInBurst = 0;
	int32 RoundsLeftInMag = 0;
	float NextTargetScanTime = 0.f;
	float NextRepathTime = 0.f;
	bool bHasLoggedFirstBurst = false;

	/** World time the current target was first acquired (for the accuracy ramp). */
	float TargetAcquiredTime = -1000.f;
};
