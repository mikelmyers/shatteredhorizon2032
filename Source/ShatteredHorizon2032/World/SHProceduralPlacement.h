// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SHProceduralPlacement.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSH_ProceduralPlacement, Log, All);

class AActor;
class USHTerrainManager;
class USHCoverSystem;

// ======================================================================
//  Enums
// ======================================================================

/** Category of procedurally placed battlefield content. */
UENUM(BlueprintType)
enum class ESHPlacementCategory : uint8
{
	Cover			UMETA(DisplayName = "Cover"),			// Sandbags, barriers, wrecks
	Vegetation		UMETA(DisplayName = "Vegetation"),		// Trees, bushes, grass
	EnemyPosition	UMETA(DisplayName = "Enemy Position"),	// Spawn points, patrol routes
	SupplyCache		UMETA(DisplayName = "Supply Cache"),	// Ammo, medical
	CivilianDebris	UMETA(DisplayName = "Civilian Debris"),	// Abandoned vehicles, furniture
	Fortification	UMETA(DisplayName = "Fortification")	// Bunkers, trenches, wire
};

/** Preferred terrain type for placement. */
UENUM(BlueprintType)
enum class ESHPreferredTerrain : uint8
{
	Any				UMETA(DisplayName = "Any"),
	Flat			UMETA(DisplayName = "Flat"),
	Elevated		UMETA(DisplayName = "Elevated"),
	LowGround		UMETA(DisplayName = "Low Ground"),
	NearRoad		UMETA(DisplayName = "Near Road"),
	NearBuilding	UMETA(DisplayName = "Near Building")
};

// ======================================================================
//  Structs
// ======================================================================

/** Rule describing how to place one class of procedural actor. */
USTRUCT(BlueprintType)
struct FSHPlacementRule
{
	GENERATED_BODY()

	/** Category this rule belongs to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	ESHPlacementCategory Category = ESHPlacementCategory::Cover;

	/** Actor class to spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	TSubclassOf<AActor> ActorClass = nullptr;

	/** Minimum number of instances to spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (ClampMin = "0"))
	int32 MinCount = 1;

	/** Maximum number of instances to spawn. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (ClampMin = "0"))
	int32 MaxCount = 5;

	/** Minimum spacing between instances in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (ClampMin = "0.0"))
	float MinSpacing = 500.f;

	/** Preferred terrain type for placement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	ESHPreferredTerrain PreferredTerrain = ESHPreferredTerrain::Any;

	/** Maximum spawn radius from the battlefield center in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (ClampMin = "0.0"))
	float SpawnRadius = 50000.f;

	/** Align spawned actor rotation to the surface normal. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	bool bAlignToSurface = true;

	/** Apply random yaw rotation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	bool bRandomRotation = true;

	/** For enemy positions: validate sightlines to approach routes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement")
	bool bCheckLineOfSight = false;

	/** Probability weight for selection when choosing which rules to apply. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Placement", meta = (ClampMin = "0.0"))
	float Weight = 1.f;
};

/** Result describing a single placement attempt. */
USTRUCT(BlueprintType)
struct FSHPlacementResult
{
	GENERATED_BODY()

	/** The actor class that was placed (or attempted). */
	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	TSubclassOf<AActor> ActorClass = nullptr;

	/** Transform where the actor was spawned. */
	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	FTransform SpawnTransform = FTransform::Identity;

	/** Category of the placed content. */
	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	ESHPlacementCategory Category = ESHPlacementCategory::Cover;

	/** Whether the actor was actually spawned successfully. */
	UPROPERTY(BlueprintReadOnly, Category = "Placement")
	bool bWasPlaced = false;
};

// ======================================================================
//  Delegates
// ======================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSHGenerationComplete, int32, TotalPlacedCount);

// ======================================================================
//  Subsystem
// ======================================================================

/**
 * USHProceduralPlacement
 *
 * World subsystem that procedurally generates battlefield content for each
 * mission instance. Uses deterministic seed-based generation with Poisson disk
 * sampling for natural distribution, terrain-aware validation, and tactically
 * meaningful enemy position placement. Controls what varies between
 * playthroughs to ensure battlefield variety and replayability.
 */
UCLASS()
class SHATTEREDHORIZON2032_API USHProceduralPlacement : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	USHProceduralPlacement();

	// ------------------------------------------------------------------
	//  USubsystem interface
	// ------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	// ------------------------------------------------------------------
	//  Primary API
	// ------------------------------------------------------------------

	/**
	 * Generate all procedural battlefield content.
	 *
	 * @param Seed		Deterministic seed — same seed produces identical layouts.
	 * @param Rules		Placement rules describing what to generate.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Procedural")
	void GenerateBattlefield(int32 Seed, const TArray<FSHPlacementRule>& Rules);

	/**
	 * Generate content for a single category using the stored rules.
	 *
	 * @param Category	Which category to generate.
	 * @param Seed		Deterministic seed.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Procedural")
	void GenerateCategory(ESHPlacementCategory Category, int32 Seed);

	/** Remove all procedurally placed actors from the world. */
	UFUNCTION(BlueprintCallable, Category = "SH|Procedural")
	void ClearGeneratedContent();

	/** Set the random seed for subsequent generation. */
	UFUNCTION(BlueprintCallable, Category = "SH|Procedural")
	void SetSeed(int32 NewSeed);

	/** Query all actors placed in a specific category. */
	UFUNCTION(BlueprintCallable, Category = "SH|Procedural")
	TArray<AActor*> GetPlacedActors(ESHPlacementCategory Category) const;

	/** Get all placement results from the last generation pass. */
	UFUNCTION(BlueprintPure, Category = "SH|Procedural")
	const TArray<FSHPlacementResult>& GetPlacementResults() const { return PlacementResults; }

	/** Get the current seed. */
	UFUNCTION(BlueprintPure, Category = "SH|Procedural")
	int32 GetCurrentSeed() const { return CurrentSeed; }

	// ------------------------------------------------------------------
	//  Default rule sets
	// ------------------------------------------------------------------

	/** Create a default set of cover placement rules. */
	UFUNCTION(BlueprintCallable, Category = "SH|Procedural|Defaults")
	static TArray<FSHPlacementRule> CreateDefaultCoverRules();

	/** Create a default set of vegetation placement rules. */
	UFUNCTION(BlueprintCallable, Category = "SH|Procedural|Defaults")
	static TArray<FSHPlacementRule> CreateDefaultVegetationRules();

	/** Create a default set of enemy position placement rules. */
	UFUNCTION(BlueprintCallable, Category = "SH|Procedural|Defaults")
	static TArray<FSHPlacementRule> CreateDefaultEnemyPositionRules();

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	/** Broadcast when generation completes with the total number of placed actors. */
	UPROPERTY(BlueprintAssignable, Category = "SH|Procedural")
	FOnSHGenerationComplete OnGenerationComplete;

protected:
	// ------------------------------------------------------------------
	//  Internal — spawn location generation
	// ------------------------------------------------------------------

	/**
	 * Find valid spawn locations for a placement rule using Poisson disk sampling.
	 *
	 * @param Rule			Placement rule to satisfy.
	 * @param Count			Desired number of locations.
	 * @param OutTransforms	Resulting valid transforms.
	 */
	void FindValidSpawnLocations(const FSHPlacementRule& Rule, int32 Count, TArray<FTransform>& OutTransforms);

	/**
	 * Validate a single candidate location against all constraints.
	 *
	 * @param Location		Candidate world location.
	 * @param Rule			Placement rule with constraints.
	 * @return True if the location passes all checks.
	 */
	bool ValidateLocation(FVector Location, const FSHPlacementRule& Rule) const;

	/**
	 * Trace down from a location and compute surface-aligned rotation.
	 *
	 * @param Location		Input XY location (Z will be adjusted to ground).
	 * @param OutRotation	Rotation aligned to the surface normal.
	 * @return True if a valid surface was found.
	 */
	bool AlignToSurface(FVector& Location, FRotator& OutRotation) const;

	/**
	 * Check whether an enemy at FromLocation has useful sightlines.
	 *
	 * @param FromLocation	Enemy position.
	 * @param ToTarget		Target location (approach route point).
	 * @return True if a clear line of sight exists.
	 */
	bool CheckSightlines(FVector FromLocation, FVector ToTarget) const;

	// ------------------------------------------------------------------
	//  Internal — Poisson disk sampling
	// ------------------------------------------------------------------

	/**
	 * Generate candidate points via Poisson disk sampling for natural distribution.
	 *
	 * @param Center		Center of the sampling area.
	 * @param Radius		Outer radius of the sampling disk.
	 * @param MinDistance	Minimum distance between points.
	 * @param MaxPoints		Maximum number of points to generate.
	 * @param OutPoints		Resulting point locations (XY, Z = 0).
	 */
	void PoissonDiskSample(FVector2D Center, float Radius, float MinDistance,
						   int32 MaxPoints, TArray<FVector2D>& OutPoints);

	// ------------------------------------------------------------------
	//  Internal — enemy tactical placement
	// ------------------------------------------------------------------

	/** Generate enemy positions with tactical clustering and sightline checks. */
	void GenerateEnemyPositions(const TArray<FSHPlacementRule>& EnemyRules, FRandomStream& RNG);

	/** Find nearby cover positions for an enemy location. */
	bool HasNearbyCover(FVector Location, float SearchRadius) const;

	/** Generate a squad cluster of 3–5 positions around a squad center. */
	void GenerateSquadCluster(FVector SquadCenter, const FSHPlacementRule& Rule,
							  int32 SquadSize, FRandomStream& RNG,
							  TArray<FTransform>& OutTransforms);

	/** Check that multiple approach directions are covered by the set of positions. */
	bool ValidateApproachCoverage(const TArray<FTransform>& Positions,
								  const TArray<FVector>& ApproachRoutes) const;

	// ------------------------------------------------------------------
	//  Internal — spawning
	// ------------------------------------------------------------------

	/** Spawn an actor at the given transform and register it. */
	AActor* SpawnPlacedActor(TSubclassOf<AActor> ActorClass, const FTransform& Transform,
							 ESHPlacementCategory Category);

	// ------------------------------------------------------------------
	//  Internal — terrain helpers
	// ------------------------------------------------------------------

	/** Check slope angle at a world location in degrees. */
	float GetSlopeAngleAt(FVector Location) const;

	/** Check whether a location is in water. */
	bool IsInWater(FVector Location) const;

	/** Check whether a location overlaps with an existing building. */
	bool OverlapsBuilding(FVector Location, float Tolerance = 200.f) const;

	/** Check minimum spacing from all previously placed actors in the same category. */
	bool MeetsSpacingRequirement(FVector Location, ESHPlacementCategory Category, float MinSpacing) const;

	/** Check if a location is near a road (using NavMesh or road tag). */
	bool IsNearRoad(FVector Location, float Tolerance = 1000.f) const;

	/** Check if a location is near a building. */
	bool IsNearBuilding(FVector Location, float Tolerance = 1500.f) const;

	/** Check if a terrain preference is satisfied. */
	bool MatchesTerrainPreference(FVector Location, ESHPreferredTerrain Preference) const;

	/** Get likely player approach route points for enemy sightline validation. */
	TArray<FVector> GetApproachRoutePoints() const;

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Battlefield center — origin for procedural generation. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Procedural|Config")
	FVector BattlefieldCenter = FVector::ZeroVector;

	/** Maximum slope angle (degrees) for valid placement on flat terrain. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Procedural|Config")
	float MaxFlatSlopeAngle = 15.f;

	/** Minimum elevation considered "elevated" terrain (cm above avg). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Procedural|Config")
	float ElevatedThreshold = 500.f;

	/** Maximum elevation for "low ground" terrain (cm below avg). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Procedural|Config")
	float LowGroundThreshold = -300.f;

	/** Average terrain elevation — computed or configured (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Procedural|Config")
	float AverageTerrainElevation = 0.f;

	/** Squad cluster radius — enemy positions within a squad stay within this distance (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Procedural|Config")
	float SquadClusterRadius = 2000.f;

	/** Minimum squad size for enemy clusters. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Procedural|Config")
	int32 MinSquadSize = 3;

	/** Maximum squad size for enemy clusters. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Procedural|Config")
	int32 MaxSquadSize = 5;

	/** Search radius for nearby cover when placing enemy positions (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Procedural|Config")
	float EnemyCoverSearchRadius = 800.f;

	/** Number of approach route points to evaluate for sightline coverage. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Procedural|Config")
	int32 ApproachRoutePointCount = 8;

	/** Poisson disk sampling: max attempts per active point before giving up. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Procedural|Config")
	int32 PoissonMaxAttempts = 30;

	/** Maximum retries when searching for valid spawn locations. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Procedural|Config")
	int32 MaxLocationRetries = 200;

	/** Trace channel for ground detection. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Procedural|Config")
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_Visibility;

	/** Trace channel for sightline checks. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Procedural|Config")
	TEnumAsByte<ECollisionChannel> SightlineTraceChannel = ECC_Visibility;

	/** Height above ground to start downward traces (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Procedural|Config")
	float TraceStartHeight = 10000.f;

	/** Trace length downward (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Procedural|Config")
	float TraceLength = 20000.f;

	/** Eye height offset for sightline checks (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Procedural|Config")
	float SightlineEyeHeight = 150.f;

private:
	// ------------------------------------------------------------------
	//  Runtime state
	// ------------------------------------------------------------------

	/** Current generation seed. */
	int32 CurrentSeed = 0;

	/** Random stream driven by CurrentSeed. */
	FRandomStream RandomStream;

	/** All placement results from the last generation pass. */
	UPROPERTY()
	TArray<FSHPlacementResult> PlacementResults;

	/** Placed actors keyed by category. */
	TMap<ESHPlacementCategory, TArray<TWeakObjectPtr<AActor>>> PlacedActorsByCategory;

	/** All placed actor locations (flat, for spacing checks). */
	TMap<ESHPlacementCategory, TArray<FVector>> PlacedLocationsByCategory;

	/** Cached pointer to terrain manager subsystem. */
	UPROPERTY()
	TWeakObjectPtr<USHTerrainManager> CachedTerrainManager;

	/** Cached pointer to cover system subsystem. */
	UPROPERTY()
	TWeakObjectPtr<USHCoverSystem> CachedCoverSystem;

	/** Active rules stored from the last GenerateBattlefield call. */
	TArray<FSHPlacementRule> ActiveRules;
};
