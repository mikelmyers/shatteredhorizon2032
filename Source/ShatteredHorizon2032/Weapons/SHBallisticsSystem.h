// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SHWeaponData.h"
#include "SHBallisticsSystem.generated.h"

class ASHProjectile;
struct FHitResult;

/* -----------------------------------------------------------------------
 *  Ballistic hit result with penetration / ricochet info
 * --------------------------------------------------------------------- */

USTRUCT(BlueprintType)
struct FSHBallisticHitResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Ballistics")
	FHitResult HitResult;

	UPROPERTY(BlueprintReadOnly, Category = "Ballistics")
	float ImpactVelocity = 0.0f; // cm/s

	UPROPERTY(BlueprintReadOnly, Category = "Ballistics")
	float DamageAtImpact = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Ballistics")
	float DistanceTraveledCm = 0.0f;

	/** True if the round penetrated the surface. */
	UPROPERTY(BlueprintReadOnly, Category = "Ballistics")
	bool bPenetrated = false;

	/** Velocity vector after penetration (if applicable). */
	UPROPERTY(BlueprintReadOnly, Category = "Ballistics")
	FVector PostPenetrationVelocity = FVector::ZeroVector;

	/** Damage remaining after penetration. */
	UPROPERTY(BlueprintReadOnly, Category = "Ballistics")
	float PostPenetrationDamage = 0.0f;

	/** True if the round ricocheted. */
	UPROPERTY(BlueprintReadOnly, Category = "Ballistics")
	bool bRicocheted = false;

	/** Ricochet direction (if applicable). */
	UPROPERTY(BlueprintReadOnly, Category = "Ballistics")
	FVector RicochetDirection = FVector::ZeroVector;

	/** Damage after ricochet. */
	UPROPERTY(BlueprintReadOnly, Category = "Ballistics")
	float RicochetDamage = 0.0f;

	/** Velocity after ricochet. */
	UPROPERTY(BlueprintReadOnly, Category = "Ballistics")
	float RicochetSpeed = 0.0f;
};

/* -----------------------------------------------------------------------
 *  Wind configuration
 * --------------------------------------------------------------------- */

USTRUCT(BlueprintType)
struct FSHWindState
{
	GENERATED_BODY()

	/** Wind direction in world space (normalized). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind")
	FVector Direction = FVector(1.0f, 0.0f, 0.0f);

	/** Wind speed in m/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind", meta = (ClampMin = "0"))
	float SpeedMPS = 0.0f;

	/** Random gust variation (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wind", meta = (ClampMin = "0", ClampMax = "1"))
	float GustVariation = 0.15f;
};

/* -----------------------------------------------------------------------
 *  Fragmentation / shrapnel data
 * --------------------------------------------------------------------- */

USTRUCT(BlueprintType)
struct FSHFragmentationParams
{
	GENERATED_BODY()

	/** Number of fragments to spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fragmentation", meta = (ClampMin = "0"))
	int32 FragmentCount = 30;

	/** Inner radius of lethal fragment damage (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fragmentation", meta = (ClampMin = "0"))
	float InnerRadiusCm = 500.0f;

	/** Outer radius of fragment travel (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fragmentation", meta = (ClampMin = "0"))
	float OuterRadiusCm = 1500.0f;

	/** Max damage per fragment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fragmentation", meta = (ClampMin = "0"))
	float FragmentDamage = 45.0f;

	/** Initial speed of fragments (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fragmentation", meta = (ClampMin = "0"))
	float FragmentSpeedCmS = 150000.0f;
};

/* -----------------------------------------------------------------------
 *  Material properties (for penetration / ricochet lookups)
 * --------------------------------------------------------------------- */

USTRUCT(BlueprintType)
struct FSHMaterialBallisticProps
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material")
	ESHPenetrableMaterial MaterialType = ESHPenetrableMaterial::Wood;

	/** Density factor (higher = harder to penetrate). Normalized, steel = 1.0. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material", meta = (ClampMin = "0"))
	float DensityFactor = 0.3f;

	/** Ricochet angle threshold (degrees). Rounds hitting at angles shallower than this may ricochet. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material", meta = (ClampMin = "0", ClampMax = "90"))
	float RicochetAngleThreshold = 15.0f;

	/** Base ricochet probability when within the angle threshold. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material", meta = (ClampMin = "0", ClampMax = "1"))
	float BaseRicochetProbability = 0.3f;

	/** Fraction of speed retained on ricochet. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Material", meta = (ClampMin = "0", ClampMax = "1"))
	float RicochetSpeedRetention = 0.4f;
};

/* -----------------------------------------------------------------------
 *  USHBallisticsSystem — World Subsystem
 * --------------------------------------------------------------------- */

UCLASS()
class SHATTEREDHORIZON2032_API USHBallisticsSystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/* --- Wind --- */

	UFUNCTION(BlueprintCallable, Category = "Ballistics|Wind")
	void SetWind(const FSHWindState& InWind) { CurrentWind = InWind; }

	UFUNCTION(BlueprintPure, Category = "Ballistics|Wind")
	FSHWindState GetWind() const { return CurrentWind; }

	/* --- Core Simulation --- */

	/**
	 * Step the ballistic simulation for a single tick.
	 * @param Position      Current position (cm, world space). Updated in-place.
	 * @param Velocity      Current velocity (cm/s, world space). Updated in-place.
	 * @param BC            Ballistic coefficients for this round.
	 * @param DeltaTime     Frame delta time.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ballistics|Simulation")
	void StepSimulation(
		UPARAM(ref) FVector& Position,
		UPARAM(ref) FVector& Velocity,
		const FSHBallisticCoefficients& BC,
		float DeltaTime) const;

	/**
	 * Calculate the drag force vector for the current velocity.
	 */
	FVector CalculateDragForce(const FVector& Velocity, const FSHBallisticCoefficients& BC) const;

	/**
	 * Calculate wind deflection force for the current conditions.
	 */
	FVector CalculateWindForce(const FVector& Velocity, const FSHBallisticCoefficients& BC) const;

	/* --- Damage --- */

	/**
	 * Calculate damage at a given distance, accounting for falloff.
	 * @param BaseDamage             Muzzle damage.
	 * @param FalloffStartCm         Distance at which falloff begins (cm).
	 * @param MaxRangeCm             Maximum range (cm).
	 * @param MinDamageMultiplier    Minimum damage fraction at max range.
	 * @param DistanceTraveledCm     How far the round has traveled (cm).
	 */
	UFUNCTION(BlueprintPure, Category = "Ballistics|Damage")
	static float CalculateDamageAtDistance(
		float BaseDamage,
		float FalloffStartCm,
		float MaxRangeCm,
		float MinDamageMultiplier,
		float DistanceTraveledCm);

	/* --- Penetration --- */

	/**
	 * Evaluate whether a projectile penetrates a surface.
	 * @param HitResult              The trace hit result.
	 * @param IncomingVelocity       Projectile velocity at impact (cm/s).
	 * @param CurrentDamage          Damage at point of impact.
	 * @param PenetrationData        Weapon's penetration entry for this material.
	 * @param MuzzleVelocityCmS      Original muzzle velocity (cm/s) for scaling.
	 * @param OutResult              Populated with penetration/ricochet info.
	 * @return True if the round continues (penetration or ricochet).
	 */
	UFUNCTION(BlueprintCallable, Category = "Ballistics|Penetration")
	bool EvaluateImpact(
		const FHitResult& HitResult,
		const FVector& IncomingVelocity,
		float CurrentDamage,
		const FSHPenetrationEntry& PenetrationData,
		float MuzzleVelocityCmS,
		FSHBallisticHitResult& OutResult) const;

	/* --- Ricochet --- */

	/**
	 * Determine if a ricochet occurs based on impact angle and material.
	 */
	bool EvaluateRicochet(
		const FHitResult& HitResult,
		const FVector& IncomingVelocity,
		const FSHMaterialBallisticProps& MaterialProps,
		FVector& OutRicochetDir,
		float& OutSpeedRetention) const;

	/* --- Material Lookup --- */

	/**
	 * Get the material ballistic properties for a given physical material.
	 * Uses the physical material's surface type or a registered mapping.
	 */
	UFUNCTION(BlueprintPure, Category = "Ballistics|Material")
	FSHMaterialBallisticProps GetMaterialProperties(ESHPenetrableMaterial Material) const;

	/**
	 * Determine the penetrable material type from a hit result's physical material.
	 * Override this to map your physical material setup.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ballistics|Material")
	virtual ESHPenetrableMaterial ClassifyMaterial(const FHitResult& HitResult) const;

	/* --- Supersonic Crack --- */

	/**
	 * Check if a near-miss supersonic crack should be triggered for a listener.
	 * @param ProjectilePosition     Current position of the round (cm).
	 * @param ProjectileVelocity     Current velocity (cm/s).
	 * @param ListenerPosition       Position of the potential listener (cm).
	 * @param NearMissRadiusCm       Radius within which the crack is audible.
	 * @return True if the round passed close enough and is supersonic.
	 */
	UFUNCTION(BlueprintPure, Category = "Ballistics|Audio")
	static bool ShouldTriggerSupersonicCrack(
		const FVector& ProjectilePosition,
		const FVector& ProjectileVelocity,
		const FVector& ListenerPosition,
		float NearMissRadiusCm = 300.0f);

	/**
	 * Compute the time delay between the supersonic crack (heard at bullet pass)
	 * and the muzzle report (thump) arriving at the listener position.
	 * Doctrine: crack arrives first, thump arrives distance/343 seconds later.
	 * This delay encodes shooter distance for the player.
	 *
	 * @param ShooterPosition      World position of the shooter (cm).
	 * @param ListenerPosition     World position of the listener (cm).
	 * @return Delay in seconds between crack and thump. 0 if subsonic.
	 */
	UFUNCTION(BlueprintPure, Category = "Ballistics|Audio")
	static float ComputeCrackThumpDelay(
		const FVector& ShooterPosition,
		const FVector& ListenerPosition);

	/**
	 * Compute the total time for the muzzle report to reach the listener.
	 * @return Time in seconds for sound to travel from shooter to listener.
	 */
	UFUNCTION(BlueprintPure, Category = "Ballistics|Audio")
	static float ComputeMuzzleReportDelay(
		const FVector& ShooterPosition,
		const FVector& ListenerPosition);

	/* --- Fragmentation --- */

	/**
	 * Spawn fragmentation traces from an explosion point.
	 * Performs radial line traces and applies damage to hit actors.
	 * @param Origin                 Explosion center.
	 * @param Params                 Fragmentation parameters.
	 * @param Instigator             The controller responsible.
	 * @param DamageCauser           The actor that caused the damage.
	 */
	UFUNCTION(BlueprintCallable, Category = "Ballistics|Fragmentation")
	void SpawnFragmentation(
		const FVector& Origin,
		const FSHFragmentationParams& Params,
		AController* Instigator,
		AActor* DamageCauser) const;

	/* --- Tracer Visibility --- */

	/**
	 * Determine if a round should be a visible tracer.
	 * @param RoundNumber    The sequential round number (1-based).
	 * @param TracerInterval Every Nth round is a tracer (0 = none).
	 */
	UFUNCTION(BlueprintPure, Category = "Ballistics|Tracers")
	static bool IsTracerRound(int32 RoundNumber, int32 TracerInterval);

protected:
	UPROPERTY(BlueprintReadOnly, Category = "Ballistics|Wind")
	FSHWindState CurrentWind;

	/** Material property table. Populated in Initialize(). */
	UPROPERTY()
	TArray<FSHMaterialBallisticProps> MaterialTable;

	/** Speed of sound in cm/s (at sea level, 20C). */
	static constexpr float SpeedOfSoundCmS = 34300.0f;

	/** Air density at sea level in kg/m^3. */
	static constexpr float AirDensityKgM3 = 1.225f;

	/** Gravity in cm/s^2. */
	static constexpr float GravityCmS2 = 981.0f;

	void InitializeMaterialTable();
};
