// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "SHCoverSystem.generated.h"

class UEnvQueryGenerator;
class UPhysicalMaterial;

// ─────────────────────────────────────────────────────────────
//  Enums
// ─────────────────────────────────────────────────────────────

/** Quality of cover at a given position. */
UENUM(BlueprintType)
enum class ESHCoverQuality : uint8
{
	None	UMETA(DisplayName = "None"),
	Low		UMETA(DisplayName = "Low"),		// crouch-height partial cover
	Medium	UMETA(DisplayName = "Medium"),	// standing partial / crouch full
	High	UMETA(DisplayName = "High")		// full standing hard cover
};

/** Whether cover stops bullets or merely hides the occupant. */
UENUM(BlueprintType)
enum class ESHCoverType : uint8
{
	HardCover		UMETA(DisplayName = "Hard Cover"),		// stops projectiles
	SoftCover		UMETA(DisplayName = "Soft Cover"),		// concealment only
	Destructible	UMETA(DisplayName = "Destructible")		// hard but can be destroyed
};

/** Material penetration categories for cover objects. */
UENUM(BlueprintType)
enum class ESHCoverMaterial : uint8
{
	Concrete	UMETA(DisplayName = "Concrete"),
	Steel		UMETA(DisplayName = "Steel"),
	Brick		UMETA(DisplayName = "Brick"),
	Wood		UMETA(DisplayName = "Wood"),
	Sandbag		UMETA(DisplayName = "Sandbag"),
	Earth		UMETA(DisplayName = "Earth"),
	Vegetation	UMETA(DisplayName = "Vegetation"),	// concealment only
	Glass		UMETA(DisplayName = "Glass"),
	Vehicle		UMETA(DisplayName = "Vehicle")
};

/** Lean direction from cover. */
UENUM(BlueprintType)
enum class ESHLeanDirection : uint8
{
	None	UMETA(DisplayName = "None"),
	Left	UMETA(DisplayName = "Left"),
	Right	UMETA(DisplayName = "Right")
};

// ─────────────────────────────────────────────────────────────
//  Structs
// ─────────────────────────────────────────────────────────────

/** Result of a cover evaluation at a world position. */
USTRUCT(BlueprintType)
struct FSHCoverEvaluation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Cover")
	FVector Position = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Cover")
	ESHCoverQuality Quality = ESHCoverQuality::None;

	UPROPERTY(BlueprintReadOnly, Category = "Cover")
	ESHCoverType CoverType = ESHCoverType::HardCover;

	UPROPERTY(BlueprintReadOnly, Category = "Cover")
	ESHCoverMaterial Material = ESHCoverMaterial::Concrete;

	/** Height of the cover barrier in cm. */
	UPROPERTY(BlueprintReadOnly, Category = "Cover")
	float CoverHeight = 0.f;

	/** Thickness of the cover barrier in cm (affects penetration). */
	UPROPERTY(BlueprintReadOnly, Category = "Cover")
	float CoverThickness = 0.f;

	/** Number of threat directions this cover protects from (out of those checked). */
	UPROPERTY(BlueprintReadOnly, Category = "Cover")
	int32 ProtectedDirections = 0;

	/** Total directions evaluated. */
	UPROPERTY(BlueprintReadOnly, Category = "Cover")
	int32 TotalDirectionsEvaluated = 0;

	/** Whether a character can lean from this cover. */
	UPROPERTY(BlueprintReadOnly, Category = "Cover")
	bool bCanLeanLeft = false;

	UPROPERTY(BlueprintReadOnly, Category = "Cover")
	bool bCanLeanRight = false;

	/** Whether this cover is currently intact (for destructible cover). */
	UPROPERTY(BlueprintReadOnly, Category = "Cover")
	bool bIsIntact = true;

	/** Normalized cover score [0..1] combining quality, type, protection. */
	UPROPERTY(BlueprintReadOnly, Category = "Cover")
	float CoverScore = 0.f;

	/** The actor providing cover (wall, vehicle, etc). */
	UPROPERTY(BlueprintReadOnly, Category = "Cover")
	TWeakObjectPtr<AActor> CoverActor = nullptr;
};

/** Tracked destructible cover entity. */
USTRUCT(BlueprintType)
struct FSHDestructibleCover
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover")
	TWeakObjectPtr<AActor> CoverActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover")
	ESHCoverMaterial Material = ESHCoverMaterial::Sandbag;

	/** Current structural integrity [0..1]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover")
	float Integrity = 1.f;

	/** Maximum damage this cover can absorb before destruction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover")
	float MaxDurability = 500.f;

	/** Accumulated damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover")
	float AccumulatedDamage = 0.f;

	/** Cover quality when fully intact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover")
	ESHCoverQuality BaseQuality = ESHCoverQuality::Medium;
};

/** Dynamic cover object (flipped tables, vehicles, rubble piles). */
USTRUCT(BlueprintType)
struct FSHDynamicCoverEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover")
	TWeakObjectPtr<AActor> CoverActor = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover")
	ESHCoverQuality Quality = ESHCoverQuality::Low;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover")
	ESHCoverType CoverType = ESHCoverType::HardCover;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover")
	ESHCoverMaterial Material = ESHCoverMaterial::Wood;

	/** Extent box used for cover evaluation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Cover")
	FVector Extents = FVector(50.f, 50.f, 50.f);
};

// ─────────────────────────────────────────────────────────────
//  Delegates
// ─────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnCoverDestroyed,
	AActor*, CoverActor,
	ESHCoverMaterial, Material);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnCoverDamaged,
	AActor*, CoverActor,
	float, RemainingIntegrity);

// ─────────────────────────────────────────────────────────────
//  Subsystem
// ─────────────────────────────────────────────────────────────

/**
 * USHCoverSystem
 *
 * World subsystem that evaluates and tracks cover for all combatants.
 * Provides:
 *  - Raycast-based cover quality assessment from any position
 *  - Cover vs concealment distinction
 *  - Destructible cover tracking and degradation
 *  - EQS generators for AI cover selection
 *  - Lean-from-cover support for player characters
 *  - Material-based penetration data for the ballistics system
 *  - Dynamic cover registration (flipped objects, vehicles, rubble)
 */
UCLASS()
class SHATTEREDHORIZON2032_API USHCoverSystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ------------------------------------------------------------------
	//  Subsystem lifecycle
	// ------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ------------------------------------------------------------------
	//  Cover evaluation
	// ------------------------------------------------------------------

	/**
	 * Evaluate cover quality at a world position against a single threat direction.
	 *
	 * @param Position			World position to evaluate.
	 * @param ThreatDirection	Normalized direction FROM the threat TO the position.
	 * @return Cover evaluation result.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Cover")
	FSHCoverEvaluation EvaluateCoverFromDirection(const FVector& Position, const FVector& ThreatDirection) const;

	/**
	 * Evaluate cover quality against multiple threat directions.
	 *
	 * @param Position			World position to evaluate.
	 * @param ThreatLocations	Array of world-space threat positions.
	 * @return Aggregate cover evaluation.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Cover")
	FSHCoverEvaluation EvaluateCoverMultiThreat(const FVector& Position, const TArray<FVector>& ThreatLocations) const;

	/**
	 * Quick check: is this position in any cover at all?
	 */
	UFUNCTION(BlueprintPure, Category = "SH|Cover")
	bool IsPositionInCover(const FVector& Position, const FVector& ThreatDirection) const;

	/**
	 * Get a cover score (0..1) combining quality, material, and intactness.
	 */
	UFUNCTION(BlueprintPure, Category = "SH|Cover")
	float GetCoverScore(const FVector& Position, const FVector& ThreatDirection) const;

	// ------------------------------------------------------------------
	//  Cover vs concealment
	// ------------------------------------------------------------------

	/**
	 * Returns true if the position is concealed (hidden) but not behind hard cover.
	 * Typical: tall grass, smoke, thin foliage.
	 */
	UFUNCTION(BlueprintPure, Category = "SH|Cover")
	bool IsConcealment(const FVector& Position, const FVector& ThreatDirection) const;

	// ------------------------------------------------------------------
	//  Destructible cover
	// ------------------------------------------------------------------

	/** Register an actor as destructible cover. */
	UFUNCTION(BlueprintCallable, Category = "SH|Cover")
	void RegisterDestructibleCover(AActor* CoverActor, ESHCoverMaterial Material, float MaxDurability, ESHCoverQuality BaseQuality);

	/** Report damage to a cover actor; may destroy it. */
	UFUNCTION(BlueprintCallable, Category = "SH|Cover")
	void DamageCover(AActor* CoverActor, float DamageAmount);

	/** Query the integrity of a destructible cover actor [0..1]. Returns -1 if not tracked. */
	UFUNCTION(BlueprintPure, Category = "SH|Cover")
	float GetCoverIntegrity(const AActor* CoverActor) const;

	/** Returns true if this cover actor is destroyed. */
	UFUNCTION(BlueprintPure, Category = "SH|Cover")
	bool IsCoverDestroyed(const AActor* CoverActor) const;

	// ------------------------------------------------------------------
	//  Dynamic cover
	// ------------------------------------------------------------------

	/** Register a newly created dynamic cover object (flipped table, etc). */
	UFUNCTION(BlueprintCallable, Category = "SH|Cover")
	void RegisterDynamicCover(AActor* CoverActor, ESHCoverQuality Quality, ESHCoverType Type, ESHCoverMaterial Material, FVector Extents);

	/** Unregister dynamic cover (object destroyed or moved). */
	UFUNCTION(BlueprintCallable, Category = "SH|Cover")
	void UnregisterDynamicCover(AActor* CoverActor);

	// ------------------------------------------------------------------
	//  Lean system
	// ------------------------------------------------------------------

	/**
	 * Check if a character can lean in a direction from their current position.
	 *
	 * @param Character		The leaning character.
	 * @param Direction		Left or Right.
	 * @return True if there is room to lean and an edge to lean around.
	 */
	UFUNCTION(BlueprintPure, Category = "SH|Cover")
	bool CanLean(const AActor* Character, ESHLeanDirection Direction) const;

	/**
	 * Compute the lean offset and rotation for the camera/mesh.
	 *
	 * @param Character		The leaning character.
	 * @param Direction		Left or Right.
	 * @param OutOffset		World-space offset to apply.
	 * @param OutRotation	Rotation to apply.
	 * @return True if lean is valid.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Cover")
	bool ComputeLeanTransform(const AActor* Character, ESHLeanDirection Direction, FVector& OutOffset, FRotator& OutRotation) const;

	// ------------------------------------------------------------------
	//  Material penetration
	// ------------------------------------------------------------------

	/**
	 * Get the penetration resistance for a cover material.
	 * Higher values mean more damage reduction / higher chance to stop rounds.
	 */
	UFUNCTION(BlueprintPure, Category = "SH|Cover")
	float GetMaterialPenetrationResistance(ESHCoverMaterial Material) const;

	/**
	 * Check whether a projectile with the given penetration rating can
	 * pass through cover of the given material and thickness.
	 *
	 * @param Material			Cover material.
	 * @param ThicknessCm		Barrier thickness in cm.
	 * @param PenetrationRating	Projectile penetration value.
	 * @param OutDamageMultiplier Damage multiplier after passing through (0 if stopped).
	 * @return True if the projectile penetrates.
	 */
	UFUNCTION(BlueprintPure, Category = "SH|Cover")
	bool CanPenetratecover(ESHCoverMaterial Material, float ThicknessCm, int32 PenetrationRating, float& OutDamageMultiplier) const;

	// ------------------------------------------------------------------
	//  EQS integration
	// ------------------------------------------------------------------

	/**
	 * Find the best cover positions within a radius, suitable for EQS generators.
	 *
	 * @param Origin			Search center.
	 * @param SearchRadius		Search radius in cm.
	 * @param ThreatLocation	Primary threat to evaluate against.
	 * @param MaxResults		Maximum number of positions to return.
	 * @return Array of evaluated cover positions sorted by score (best first).
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Cover|AI")
	TArray<FSHCoverEvaluation> FindBestCoverPositions(const FVector& Origin, float SearchRadius, const FVector& ThreatLocation, int32 MaxResults = 10) const;

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Cover")
	FOnCoverDestroyed OnCoverDestroyed;

	UPROPERTY(BlueprintAssignable, Category = "SH|Cover")
	FOnCoverDamaged OnCoverDamaged;

protected:
	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Default standing character height for cover evaluation (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Cover|Config")
	float StandingHeight = 180.f;

	/** Default crouching character height (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Cover|Config")
	float CrouchHeight = 100.f;

	/** Raycast distance for cover checks (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Cover|Config")
	float CoverCheckDistance = 150.f;

	/** Lateral offset for lean checks (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Cover|Config")
	float LeanOffset = 45.f;

	/** Material penetration resistance table. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Cover|Config")
	TMap<ESHCoverMaterial, float> MaterialPenetrationResistanceMap;

	/** Number of angular samples for multi-directional cover evaluation. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Cover|Config")
	int32 CoverSampleDirections = 8;

	/** Trace channel used for cover evaluation. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Cover|Config")
	TEnumAsByte<ECollisionChannel> CoverTraceChannel = ECC_Visibility;

private:
	// ------------------------------------------------------------------
	//  Internal
	// ------------------------------------------------------------------
	FSHCoverEvaluation EvaluateSingleDirection(const FVector& Position, const FVector& ThreatDir, const UWorld* World) const;
	ESHCoverQuality HeightToCoverQuality(float Height) const;
	ESHCoverMaterial PhysMatToCoverMaterial(const UPhysicalMaterial* PhysMat) const;
	float ComputeCoverScore(const FSHCoverEvaluation& Eval) const;
	void InitDefaultPenetrationValues();

	/** Tracked destructible cover objects. */
	UPROPERTY()
	TMap<TObjectPtr<AActor>, FSHDestructibleCover> DestructibleCoverMap;

	/** Tracked dynamic cover objects. */
	UPROPERTY()
	TMap<TObjectPtr<AActor>, FSHDynamicCoverEntry> DynamicCoverMap;
};
