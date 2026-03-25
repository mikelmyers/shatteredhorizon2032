// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SHWeaponData.h"
#include "SHBallisticsSystem.generated.h"

class ASHProjectile;

// ---------------------------------------------------------------------------
// Ballistic query / result structs
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHBallisticHitResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bHit = false;

	UPROPERTY(BlueprintReadOnly)
	FVector HitLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	FVector HitNormal = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly)
	float DistanceTravelled = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float RemainingDamage = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float RemainingVelocity = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	ESHPenetrationMaterial SurfaceMaterial = ESHPenetrationMaterial::Dirt;

	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<AActor> HitActor = nullptr;

	UPROPERTY(BlueprintReadOnly)
	FName HitBoneName = NAME_None;

	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<UPhysicalMaterial> PhysMaterial = nullptr;
};

USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHPenetrationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bPenetrated = false;

	UPROPERTY(BlueprintReadOnly)
	FVector ExitPoint = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	FVector ExitDirection = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly)
	float DamageAfterPenetration = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float VelocityAfterPenetration = 0.0f;
};

USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHRicochetResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bRicocheted = false;

	UPROPERTY(BlueprintReadOnly)
	FVector RicochetOrigin = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly)
	FVector RicochetDirection = FVector::ForwardVector;

	UPROPERTY(BlueprintReadOnly)
	float RicochetSpeed = 0.0f;

	UPROPERTY(BlueprintReadOnly)
	float RicochetDamage = 0.0f;
};

USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHWindParams
{
	GENERATED_BODY()

	/** Wind direction (world space, normalized). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind")
	FVector WindDirection = FVector(1.0f, 0.0f, 0.0f);

	/** Wind speed in m/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind", meta = (ClampMin = "0.0"))
	float WindSpeedMPS = 0.0f;
};

// ---------------------------------------------------------------------------
// USHBallisticsSystem — world subsystem for all ballistic calculations
// ---------------------------------------------------------------------------

UCLASS()
class SHATTEREDHORIZON2032_API USHBallisticsSystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// -------------------------------------------------------------------
	// Wind
	// -------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "SH|Ballistics")
	void SetWindParams(const FSHWindParams& InWind);

	UFUNCTION(BlueprintPure, Category = "SH|Ballistics")
	const FSHWindParams& GetWindParams() const { return CurrentWind; }

	// -------------------------------------------------------------------
	// Core ballistic step — advance a projectile one tick
	// -------------------------------------------------------------------

	/**
	 * Simulate one physics sub-step for a projectile.
	 * @param InPosition      Current world position (cm).
	 * @param InVelocity      Current velocity vector (cm/s).
	 * @param DeltaTime       Time step.
	 * @param WeaponData      Source weapon data for BC / drag.
	 * @param OutPosition     New position after step.
	 * @param OutVelocity     New velocity after step.
	 */
	void StepProjectile(
		const FVector& InPosition,
		const FVector& InVelocity,
		float DeltaTime,
		const USHWeaponData* WeaponData,
		FVector& OutPosition,
		FVector& OutVelocity) const;

	// -------------------------------------------------------------------
	// Damage falloff
	// -------------------------------------------------------------------

	/** Calculate damage at a given distance, accounting for falloff. */
	UFUNCTION(BlueprintPure, Category = "SH|Ballistics")
	float CalcDamageAtDistance(const USHWeaponData* WeaponData, float DistanceCm) const;

	// -------------------------------------------------------------------
	// Penetration
	// -------------------------------------------------------------------

	/**
	 * Attempt penetration through a surface.
	 * @param HitResult       The trace hit to penetrate through.
	 * @param InDirection     Incoming bullet direction (normalised).
	 * @param InSpeed         Incoming speed (cm/s).
	 * @param InDamage        Incoming damage.
	 * @param WeaponData      Source weapon data.
	 * @param Material        Surface material type.
	 * @return Penetration outcome.
	 */
	FSHPenetrationResult TryPenetrate(
		const FHitResult& HitResult,
		const FVector& InDirection,
		float InSpeed,
		float InDamage,
		const USHWeaponData* WeaponData,
		ESHPenetrationMaterial Material) const;

	// -------------------------------------------------------------------
	// Ricochet
	// -------------------------------------------------------------------

	/**
	 * Determine if a round ricochets off a surface.
	 * @param HitResult       The trace hit.
	 * @param InDirection     Incoming bullet direction (normalised).
	 * @param InSpeed         Incoming speed (cm/s).
	 * @param InDamage        Incoming damage.
	 * @param Material        Surface material type.
	 * @return Ricochet outcome.
	 */
	FSHRicochetResult TryRicochet(
		const FHitResult& HitResult,
		const FVector& InDirection,
		float InSpeed,
		float InDamage,
		ESHPenetrationMaterial Material) const;

	// -------------------------------------------------------------------
	// Hitscan (close range)
	// -------------------------------------------------------------------

	/**
	 * Perform an instant hitscan trace with penetration support.
	 * @param Origin          Muzzle world location.
	 * @param Direction       Fire direction (normalised).
	 * @param WeaponData      Source weapon data.
	 * @param Instigator      Firing pawn (ignored in traces).
	 * @param OutHits         Ordered list of hit results (may include penetration exits).
	 */
	void PerformHitscan(
		const FVector& Origin,
		const FVector& Direction,
		const USHWeaponData* WeaponData,
		AActor* Instigator,
		TArray<FSHBallisticHitResult>& OutHits) const;

	// -------------------------------------------------------------------
	// Supersonic crack
	// -------------------------------------------------------------------

	/**
	 * Check if a near-miss supersonic crack should be triggered for a listener.
	 * @param BulletPosition  Current projectile position.
	 * @param BulletVelocity  Current projectile velocity (cm/s).
	 * @param ListenerPosition   The listener (player) position.
	 * @param OutCrackLocation   The point on the trajectory closest to the listener.
	 * @return True if a crack audio event should fire.
	 */
	bool ShouldTriggerSupersonicCrack(
		const FVector& BulletPosition,
		const FVector& BulletVelocity,
		const FVector& ListenerPosition,
		FVector& OutCrackLocation) const;

	// -------------------------------------------------------------------
	// Explosive / fragmentation
	// -------------------------------------------------------------------

	/**
	 * Process an explosive detonation.
	 * @param DetonationPoint World location of the explosion.
	 * @param WeaponData      Source weapon data (must have bIsExplosive).
	 * @param Instigator      The pawn responsible.
	 * @param DamageCauser    The actor causing damage (projectile, etc.).
	 */
	void ProcessExplosion(
		const FVector& DetonationPoint,
		const USHWeaponData* WeaponData,
		APawn* Instigator,
		AActor* DamageCauser) const;

	// -------------------------------------------------------------------
	// Material mapping helper
	// -------------------------------------------------------------------

	/** Map a UPhysicalMaterial to our penetration material enum. */
	UFUNCTION(BlueprintPure, Category = "SH|Ballistics")
	static ESHPenetrationMaterial PhysMaterialToSHMaterial(const UPhysicalMaterial* PhysMat);

private:
	/** Speed of sound in cm/s (343 m/s). */
	static constexpr float SpeedOfSoundCmS = 34300.0f;

	/** Air density at sea level in kg/m^3. */
	static constexpr float AirDensityKgM3 = 1.225f;

	/** Gravity in cm/s^2. */
	static constexpr float GravityCmS2 = 981.0f;

	/** Supersonic crack detection radius in cm (2 metres). */
	static constexpr float SupersonicCrackRadiusCm = 200.0f;

	/** Minimum angle (degrees) for ricochet to be possible. */
	static constexpr float MinRicochetAngleDeg = 5.0f;

	/** Maximum angle (degrees) for ricochet — above this, bullet embeds. */
	static constexpr float MaxRicochetAngleDeg = 30.0f;

	UPROPERTY()
	FSHWindParams CurrentWind;

	/** Estimate surface thickness by tracing backwards from the exit side. */
	float EstimateSurfaceThickness(
		const FHitResult& HitResult,
		const FVector& InDirection,
		float MaxDepthCm) const;

	/** Compute drag deceleration vector. */
	FVector ComputeDragDeceleration(
		const FVector& Velocity,
		float DragCoefficient,
		float CrossSectionCm2,
		float MassGrains) const;
};
