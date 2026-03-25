// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SHWeaponData.h"
#include "SHProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UNiagaraComponent;
class USHBallisticsSystem;

UCLASS()
class SHATTEREDHORIZON2032_API ASHProjectile : public AActor
{
	GENERATED_BODY()

public:
	ASHProjectile();

	// -------------------------------------------------------------------
	// Initialization
	// -------------------------------------------------------------------

	/**
	 * Configure the projectile after spawn.
	 * @param InWeaponData   The weapon definition that fired this round.
	 * @param InMuzzleVelocity  Direction * speed (cm/s) at the muzzle.
	 * @param InDamage       Initial damage value.
	 * @param bInIsTracer    Whether this round should display a tracer.
	 * @param InInstigator   The pawn that fired.
	 */
	void InitProjectile(
		const USHWeaponData* InWeaponData,
		const FVector& InMuzzleVelocity,
		float InDamage,
		bool bInIsTracer,
		APawn* InInstigator);

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// -------------------------------------------------------------------
	// Components
	// -------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UNiagaraComponent> TracerVFX;

	// -------------------------------------------------------------------
	// State
	// -------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<const USHWeaponData> WeaponData;

	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	FVector CurrentVelocity;

	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	float CurrentDamage;

	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	float DistanceTravelled;

	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	bool bIsTracer;

	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	int32 PenetrationCount;

	UPROPERTY(BlueprintReadOnly, Category = "Projectile")
	int32 RicochetCount;

	/** Cached world subsystem pointer. */
	UPROPERTY(Transient)
	TObjectPtr<USHBallisticsSystem> BallisticsSystem;

	/** Previous frame position for sweep traces. */
	FVector PreviousPosition;

	/** Instigating pawn. */
	TWeakObjectPtr<APawn> InstigatorPawn;

	/** Elapsed lifetime in seconds. */
	float LifetimeElapsed;

	// -------------------------------------------------------------------
	// Configuration
	// -------------------------------------------------------------------

	/** Maximum number of surfaces the projectile can penetrate. */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	int32 MaxPenetrations = 3;

	/** Maximum number of ricochets before destroying. */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	int32 MaxRicochets = 2;

	/** Maximum lifetime in seconds before auto-destroy. */
	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float MaxLifetime = 6.0f;

	// -------------------------------------------------------------------
	// Tracer VFX
	// -------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	TObjectPtr<UNiagaraSystem> TracerNiagaraAsset;

	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	float TracerWidth = 0.3f;

	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	FLinearColor TracerColor = FLinearColor(1.0f, 0.6f, 0.2f, 1.0f);

	// -------------------------------------------------------------------
	// Impact VFX
	// -------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	TMap<ESHPenetrationMaterial, TObjectPtr<UNiagaraSystem>> ImpactEffects;

	UPROPERTY(EditDefaultsOnly, Category = "VFX")
	TObjectPtr<UNiagaraSystem> DefaultImpactEffect;

	// -------------------------------------------------------------------
	// Supersonic crack
	// -------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TObjectPtr<USoundBase> SupersonicCrackSound;

	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TObjectPtr<USoundAttenuation> CrackAttenuation;

	/** Set of player controllers that already heard the crack from this round. */
	TSet<TWeakObjectPtr<APlayerController>> CrackTriggeredForControllers;

	// -------------------------------------------------------------------
	// Internal methods
	// -------------------------------------------------------------------

private:
	/** Custom ballistic tick — overrides UProjectileMovementComponent physics. */
	void TickBallistics(float DeltaTime);

	/** Perform a sweep trace between previous and current position. */
	bool PerformSweepTrace(const FVector& Start, const FVector& End, FHitResult& OutHit) const;

	/** Process a hit — damage, penetration, ricochet, or stop. */
	void ProcessHit(const FHitResult& HitResult);

	/** Apply damage to the hit actor. */
	void ApplyDamageToActor(const FHitResult& HitResult, float Damage);

	/** Spawn impact VFX at hit location. */
	void SpawnImpactEffect(const FHitResult& HitResult, ESHPenetrationMaterial Material);

	/** Spawn a ricochet child projectile. */
	void SpawnRicochetProjectile(const struct FSHRicochetResult& RicochetResult);

	/** Check and trigger supersonic crack for nearby players. */
	void CheckSupersonicCracks();

	/** Activate or deactivate tracer visual. */
	void SetTracerVisible(bool bVisible);

	/** Check if projectile has exceeded max range. */
	bool HasExceededMaxRange() const;
};
