// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SHWeaponData.h"
#include "SHBallisticsSystem.h"
#include "SHProjectile.generated.h"

class UProjectileMovementComponent;
class USphereComponent;
class UParticleSystemComponent;
class UNiagaraComponent;

/* -----------------------------------------------------------------------
 *  ASHProjectile — Physical projectile with ballistic simulation
 * --------------------------------------------------------------------- */

UCLASS(Blueprintable)
class SHATTEREDHORIZON2032_API ASHProjectile : public AActor
{
	GENERATED_BODY()

public:
	ASHProjectile();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/* --- Initialization --- */

	/**
	 * Initialize projectile parameters after spawn.
	 * Called by ASHWeaponBase::SpawnProjectile().
	 */
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void InitializeProjectile(
		const FVector& InitialVelocity,
		float InBaseDamage,
		const FSHBallisticCoefficients& InBC,
		float InMaxRangeCm,
		float InDamageFalloffStartCm,
		float InMinDamageMultiplier,
		bool bInIsTracer,
		AActor* InWeaponOwner);

	/* --- Components --- */

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile|Components")
	TObjectPtr<USphereComponent> CollisionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile|Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	/** Tracer visual — only active for tracer rounds. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Projectile|Components")
	TObjectPtr<UParticleSystemComponent> TracerVFXComp;

	/* --- Configuration (set via defaults or BP) --- */

	/** Default tracer particle system. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|VFX")
	TObjectPtr<UParticleSystem> TracerParticle;

	/** Impact effects per surface type. Map physical surface type index to particle. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|VFX")
	TMap<TEnumAsByte<EPhysicalSurface>, TObjectPtr<UParticleSystem>> ImpactEffects;

	/** Default impact effect if no surface-specific one is found. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|VFX")
	TObjectPtr<UParticleSystem> DefaultImpactEffect;

	/** Supersonic crack sound cue. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Audio")
	TObjectPtr<USoundBase> SupersonicCrackSound;

	/** Ricochet sound. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Audio")
	TObjectPtr<USoundBase> RicochetSound;

	/** Impact sound. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Audio")
	TObjectPtr<USoundBase> ImpactSound;

	/** Maximum number of surfaces this projectile can penetrate. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Penetration", meta = (ClampMin = "0"))
	int32 MaxPenetrations = 3;

	/** Maximum number of ricochets allowed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Ricochet", meta = (ClampMin = "0"))
	int32 MaxRicochets = 2;

	/** If true, this is an explosive projectile (grenade launcher). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Explosive")
	bool bIsExplosive = false;

	/** Explosion radius (cm). Only relevant if bIsExplosive. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Explosive", meta = (ClampMin = "0", EditCondition = "bIsExplosive"))
	float ExplosionRadiusCm = 750.0f;

	/** Explosion damage at center. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Explosive", meta = (ClampMin = "0", EditCondition = "bIsExplosive"))
	float ExplosionDamage = 200.0f;

	/** Fragmentation parameters for explosive rounds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Explosive", meta = (EditCondition = "bIsExplosive"))
	FSHFragmentationParams FragmentationParams;

	/** Explosion VFX. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Explosive", meta = (EditCondition = "bIsExplosive"))
	TObjectPtr<UParticleSystem> ExplosionVFX;

	/** Explosion sound. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Explosive", meta = (EditCondition = "bIsExplosive"))
	TObjectPtr<USoundBase> ExplosionSound;

	/** Projectile lifetime in seconds. Safety net for cleanup. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile", meta = (ClampMin = "0.1"))
	float MaxLifetime = 10.0f;

protected:
	/* --- Collision Handling --- */

	UFUNCTION()
	void OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	/** Process a hit — damage, penetration, ricochet. */
	void HandleImpact(const FHitResult& HitResult);

	/** Apply damage to the hit actor. */
	void ApplyDamage(AActor* HitActor, float Damage, const FHitResult& HitResult);

	/** Spawn impact VFX and sound at the hit point. */
	void SpawnImpactEffects(const FHitResult& HitResult);

	/** Handle explosive detonation. */
	void Detonate(const FVector& Location);

	/** Continue projectile after penetration. */
	void ContinueAfterPenetration(const FSHBallisticHitResult& BallisticResult, const FHitResult& OriginalHit);

	/** Spawn a ricochet (new trajectory for existing projectile). */
	void ApplyRicochet(const FSHBallisticHitResult& BallisticResult, const FHitResult& OriginalHit);

	/** Check for supersonic crack near players. */
	void CheckSupersonicCrack();

	/** Disable the projectile and schedule destroy. */
	void DeactivateAndDestroy();

	/* --- Custom Ballistic Simulation --- */

	/**
	 * We override projectile movement with our own ballistic step.
	 * UProjectileMovementComponent is used primarily for its sweep/collision detection.
	 * Our StepSimulation from USHBallisticsSystem drives the actual physics.
	 */
	void CustomBallisticTick(float DeltaTime);

	/* --- State --- */

	UPROPERTY()
	FVector SimulatedVelocity = FVector::ZeroVector;

	float BaseDamage = 0.0f;
	float DamageFalloffStartCm = 0.0f;
	float MinDamageMultiplier = 0.25f;
	float MaxRangeCm = 0.0f;
	float MuzzleVelocityCmS = 0.0f;
	float DistanceTraveled = 0.0f;

	FSHBallisticCoefficients BallisticCoeffs;

	int32 PenetrationCount = 0;
	int32 RicochetCount = 0;

	bool bIsTracer = false;
	bool bIsActive = true;

	/** Spawn location — used to compute total distance traveled. */
	FVector SpawnLocation = FVector::ZeroVector;

	UPROPERTY()
	TWeakObjectPtr<AActor> WeaponOwner;

	/** Tracks which players have already heard the supersonic crack from this round. */
	UPROPERTY()
	TSet<TObjectPtr<AActor>> SupersonicCrackNotifiedActors;

	/** Reference to the world ballistics subsystem. Cached on BeginPlay. */
	UPROPERTY()
	TObjectPtr<USHBallisticsSystem> BallisticsSystem;

	/** Penetration table from the firing weapon (cached on init). */
	TArray<FSHPenetrationEntry> PenetrationTable;
};
