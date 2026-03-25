// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "SHTerrainManager.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSH_Terrain, Log, All);

class USoundBase;
class UNiagaraSystem;
class UMaterialInterface;

/** Terrain surface types in the Taoyuan Beach operational area. */
UENUM(BlueprintType)
enum class ESHTerrainType : uint8
{
	DeepWater		UMETA(DisplayName = "Deep Water"),		// Ocean, swim required
	ShallowWater	UMETA(DisplayName = "Shallow Water"),	// Surf zone, wadeable
	WetSand			UMETA(DisplayName = "Wet Sand"),		// Tidal zone
	DrySand			UMETA(DisplayName = "Dry Sand"),		// Beach
	Dunes			UMETA(DisplayName = "Dunes"),			// Sand dunes, soft footing
	VegetationLine	UMETA(DisplayName = "Vegetation Line"),	// Scrub, coastal plants
	Grass			UMETA(DisplayName = "Grass"),			// Inland grass fields
	Dirt			UMETA(DisplayName = "Dirt"),				// Packed earth
	Mud				UMETA(DisplayName = "Mud"),				// Rain-softened ground
	Gravel			UMETA(DisplayName = "Gravel"),			// Roads, paths
	Concrete		UMETA(DisplayName = "Concrete"),		// Urban surfaces
	Asphalt			UMETA(DisplayName = "Asphalt"),			// Roads
	Metal			UMETA(DisplayName = "Metal"),			// Vehicle surfaces, grates
	Wood			UMETA(DisplayName = "Wood"),			// Boardwalks, structures
	Rock			UMETA(DisplayName = "Rock"),			// Exposed rock
	Rubble			UMETA(DisplayName = "Rubble")			// Destroyed building debris
};

/** Water depth classification at a world position. */
UENUM(BlueprintType)
enum class ESHWaterDepth : uint8
{
	None			UMETA(DisplayName = "None"),
	Ankle			UMETA(DisplayName = "Ankle"),			// < 30cm
	Knee			UMETA(DisplayName = "Knee"),			// 30-60cm
	Waist			UMETA(DisplayName = "Waist"),			// 60-120cm
	Chest			UMETA(DisplayName = "Chest"),			// 120-160cm, weapon raised
	Swimming		UMETA(DisplayName = "Swimming")			// > 160cm
};

/** Beach zone classification from sea to inland. */
UENUM(BlueprintType)
enum class ESHBeachZone : uint8
{
	Ocean			UMETA(DisplayName = "Ocean"),
	SurfZone		UMETA(DisplayName = "Surf Zone"),
	WetSand			UMETA(DisplayName = "Wet Sand"),
	DrySand			UMETA(DisplayName = "Dry Sand"),
	Dunes			UMETA(DisplayName = "Dunes"),
	VegetationLine	UMETA(DisplayName = "Vegetation Line"),
	Inland			UMETA(DisplayName = "Inland")
};

/** Properties of a terrain surface type affecting gameplay. */
USTRUCT(BlueprintType)
struct FSHTerrainProperties
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	ESHTerrainType TerrainType = ESHTerrainType::Dirt;

	/** Movement speed multiplier (1.0 = normal). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain", meta = (ClampMin = "0.1", ClampMax = "1.5"))
	float MovementSpeedMultiplier = 1.f;

	/** Sound volume multiplier for footsteps (1.0 = normal). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	float FootstepVolumeMultiplier = 1.f;

	/** How quickly the player tires when moving on this surface (multiplier). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	float StaminaDrainMultiplier = 1.f;

	/** Whether prone is effective on this surface for concealment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	bool bProneConcealmentEffective = true;

	/** Whether vehicles can traverse this surface. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	bool bVehicleTraversable = true;

	/** Leaves visible tracks/footprints. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	bool bLeavesTraces = false;

	/** Dust/splash particle system played on footstep. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	TSoftObjectPtr<UNiagaraSystem> FootstepParticle;

	/** Impact decal for bullet hits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	TSoftObjectPtr<UMaterialInterface> ImpactDecal;

	/** Corresponding physical material for physics interactions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Terrain")
	TSoftObjectPtr<UPhysicalMaterial> PhysicalMaterial;
};

/** Footstep sound set for a terrain type. */
USTRUCT(BlueprintType)
struct FSHFootstepSoundSet
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	ESHTerrainType TerrainType = ESHTerrainType::Dirt;

	/** Walk footstep sounds — randomly selected per step. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TArray<TSoftObjectPtr<USoundBase>> WalkSounds;

	/** Run/sprint footstep sounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TArray<TSoftObjectPtr<USoundBase>> RunSounds;

	/** Prone crawl sounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TArray<TSoftObjectPtr<USoundBase>> CrawlSounds;

	/** Landing sounds (from a fall/jump). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Audio")
	TArray<TSoftObjectPtr<USoundBase>> LandSounds;
};

/** Vegetation interaction data. */
USTRUCT(BlueprintType)
struct FSHVegetationInstance
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Health = 1.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bOnFire = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float FireSpreadTimer = 0.f;
};

/** Result of a terrain query at a world position. */
USTRUCT(BlueprintType)
struct FSHTerrainQueryResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	bool bValid = false;

	UPROPERTY(BlueprintReadOnly)
	ESHTerrainType TerrainType = ESHTerrainType::Dirt;

	UPROPERTY(BlueprintReadOnly)
	ESHBeachZone BeachZone = ESHBeachZone::Inland;

	UPROPERTY(BlueprintReadOnly)
	ESHWaterDepth WaterDepth = ESHWaterDepth::None;

	UPROPERTY(BlueprintReadOnly)
	float WaterDepthCM = 0.f;

	UPROPERTY(BlueprintReadOnly)
	float Elevation = 0.f;

	UPROPERTY(BlueprintReadOnly)
	FVector SurfaceNormal = FVector::UpVector;

	UPROPERTY(BlueprintReadOnly)
	float SlopeAngleDegrees = 0.f;

	UPROPERTY(BlueprintReadOnly)
	FSHTerrainProperties Properties;

	/** Tactical advantage from elevation (0 = no advantage, positive = high ground). */
	UPROPERTY(BlueprintReadOnly)
	float TacticalElevationAdvantage = 0.f;
};

/** Delegate for weather change affecting terrain. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnTerrainWeatherChanged, ESHTerrainType, AffectedType, float, WetnessLevel);

/**
 * USHTerrainManager
 *
 * World subsystem managing terrain queries, surface properties, footstep sounds,
 * water depth, vegetation interaction, and weather effects on the Taoyuan Beach
 * operational area. All terrain types affect movement, sound propagation, and
 * tactical options authentically.
 */
UCLASS()
class SHATTEREDHORIZON2032_API USHTerrainManager : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	USHTerrainManager();

	// ------------------------------------------------------------------
	//  USubsystem interface
	// ------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// ------------------------------------------------------------------
	//  Terrain queries
	// ------------------------------------------------------------------

	/** Full terrain query at a world position. */
	UFUNCTION(BlueprintCallable, Category = "SH|Terrain")
	FSHTerrainQueryResult QueryTerrain(FVector WorldPosition) const;

	/** Quick terrain type lookup (cheaper than full query). */
	UFUNCTION(BlueprintPure, Category = "SH|Terrain")
	ESHTerrainType GetTerrainTypeAt(FVector WorldPosition) const;

	/** Get the beach zone for a given X position (distance from shoreline). */
	UFUNCTION(BlueprintPure, Category = "SH|Terrain")
	ESHBeachZone GetBeachZoneAt(FVector WorldPosition) const;

	/** Get water depth at a world position in centimeters. */
	UFUNCTION(BlueprintPure, Category = "SH|Terrain")
	float GetWaterDepthAt(FVector WorldPosition) const;

	/** Classify water depth. */
	UFUNCTION(BlueprintPure, Category = "SH|Terrain")
	ESHWaterDepth ClassifyWaterDepth(float DepthCM) const;

	/** Get terrain elevation at XY. */
	UFUNCTION(BlueprintPure, Category = "SH|Terrain")
	float GetElevationAt(FVector2D WorldXY) const;

	/** Compute tactical elevation advantage between attacker and defender. */
	UFUNCTION(BlueprintPure, Category = "SH|Terrain")
	float ComputeElevationAdvantage(FVector AttackerPos, FVector DefenderPos) const;

	/** Get movement speed multiplier at position, accounting for weather. */
	UFUNCTION(BlueprintPure, Category = "SH|Terrain")
	float GetMovementSpeedMultiplier(FVector WorldPosition) const;

	/** Get terrain properties for a terrain type. */
	UFUNCTION(BlueprintPure, Category = "SH|Terrain")
	FSHTerrainProperties GetTerrainProperties(ESHTerrainType Type) const;

	// ------------------------------------------------------------------
	//  Footstep sounds
	// ------------------------------------------------------------------

	/** Get a random footstep sound for the given terrain type and movement mode. */
	UFUNCTION(BlueprintCallable, Category = "SH|Terrain|Audio")
	USoundBase* GetFootstepSound(ESHTerrainType TerrainType, bool bIsSprinting, bool bIsCrawling) const;

	/** Get footstep volume multiplier at position (terrain + weather). */
	UFUNCTION(BlueprintPure, Category = "SH|Terrain|Audio")
	float GetFootstepVolumeMultiplier(FVector WorldPosition) const;

	// ------------------------------------------------------------------
	//  Weather effects on terrain
	// ------------------------------------------------------------------

	/** Notify the terrain system of a weather change. */
	UFUNCTION(BlueprintCallable, Category = "SH|Terrain|Weather")
	void OnWeatherChanged(ESHTerrainType PreviousWeather, float RainIntensity, float WindSpeed);

	/** Get current ground wetness (0 = dry, 1 = saturated). */
	UFUNCTION(BlueprintPure, Category = "SH|Terrain|Weather")
	float GetGroundWetness() const { return CurrentGroundWetness; }

	// ------------------------------------------------------------------
	//  Vegetation
	// ------------------------------------------------------------------

	/** Interact with vegetation at location (player moving through). */
	UFUNCTION(BlueprintCallable, Category = "SH|Terrain|Vegetation")
	void InteractWithVegetation(FVector WorldPosition, float InteractionRadius);

	/** Set vegetation on fire at a location. */
	UFUNCTION(BlueprintCallable, Category = "SH|Terrain|Vegetation")
	void IgniteVegetation(FVector WorldPosition, float Radius);

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Terrain")
	FOnTerrainWeatherChanged OnTerrainWeatherChanged;

protected:
	// ------------------------------------------------------------------
	//  Internal
	// ------------------------------------------------------------------
	void InitializeTerrainProperties();
	void InitializeBeachZoneThresholds();
	void TickVegetationFire(float DeltaTime);
	void TickWeatherEffects(float DeltaTime);

	ESHTerrainType DetermineTerrainFromPhysMat(const UPhysicalMaterial* PhysMat) const;
	ESHTerrainType DetermineTerrainFromTrace(FVector WorldPosition) const;

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Terrain property definitions per type. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Terrain")
	TMap<ESHTerrainType, FSHTerrainProperties> TerrainPropertyMap;

	/** Footstep sound sets per terrain type. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Terrain|Audio")
	TArray<FSHFootstepSoundSet> FootstepSoundSets;

	/** Beach zone distance thresholds from shoreline (cm, sorted seaward to inland). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Terrain|Beach")
	float ShorelineYPosition = -150000.f;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Terrain|Beach")
	float SurfZoneWidth = 5000.f;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Terrain|Beach")
	float WetSandWidth = 3000.f;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Terrain|Beach")
	float DrySandWidth = 8000.f;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Terrain|Beach")
	float DuneWidth = 6000.f;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Terrain|Beach")
	float VegetationLineWidth = 4000.f;

	/** Water level in Z world units. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Terrain|Water")
	float WaterLevelZ = 0.f;

	/** Maximum swim depth — water below this is inaccessible (drowning). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Terrain|Water")
	float MaxSwimDepthCM = 500.f;

	/** Rain intensity threshold to start generating mud. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Terrain|Weather")
	float MudGenerationRainThreshold = 0.3f;

	/** Rate at which ground wetness increases per second at max rain. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Terrain|Weather")
	float WetnessIncreaseRate = 0.05f;

	/** Rate at which ground dries per second with no rain. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Terrain|Weather")
	float WetnessDryRate = 0.01f;

	/** Fire spread rate per second (radius expansion). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Terrain|Vegetation")
	float FireSpreadRateCMPerSec = 50.f;

	/** Fire spread radius before fire exhausts. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Terrain|Vegetation")
	float MaxFireSpreadRadius = 2000.f;

private:
	// Runtime state -------------------------------------------------------

	float CurrentGroundWetness = 0.f;
	float CurrentRainIntensity = 0.f;
	float CurrentWindSpeed = 0.f;

	/** Active vegetation fire instances. */
	UPROPERTY()
	TArray<FSHVegetationInstance> ActiveFires;

	/** Cached terrain trace channel. */
	static constexpr ECollisionChannel TerrainTraceChannel = ECC_Visibility;
};
