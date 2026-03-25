// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "SHTerrainManager.generated.h"

class ASHGameState;

/** Surface material types for the Taoyuan Beach operational area. */
UENUM(BlueprintType)
enum class ESHSurfaceType : uint8
{
	Default			UMETA(DisplayName = "Default"),
	SurfWater		UMETA(DisplayName = "Surf / Shallow Water"),
	WetSand			UMETA(DisplayName = "Wet Sand"),
	DrySand			UMETA(DisplayName = "Dry Sand"),
	Dune			UMETA(DisplayName = "Dune"),
	Grass			UMETA(DisplayName = "Grass / Vegetation"),
	Mud				UMETA(DisplayName = "Mud"),
	Dirt			UMETA(DisplayName = "Dirt"),
	Gravel			UMETA(DisplayName = "Gravel"),
	Concrete		UMETA(DisplayName = "Concrete"),
	Asphalt			UMETA(DisplayName = "Asphalt"),
	Metal			UMETA(DisplayName = "Metal"),
	Wood			UMETA(DisplayName = "Wood"),
	DeepWater		UMETA(DisplayName = "Deep Water"),
	Rock			UMETA(DisplayName = "Rock")
};

/** Beach zone classification (from sea to inland). */
UENUM(BlueprintType)
enum class ESHBeachZone : uint8
{
	Ocean			UMETA(DisplayName = "Ocean"),
	Surf			UMETA(DisplayName = "Surf Zone"),
	WetSand			UMETA(DisplayName = "Wet Sand"),
	DrySand			UMETA(DisplayName = "Dry Sand"),
	Dunes			UMETA(DisplayName = "Dunes"),
	VegetationLine	UMETA(DisplayName = "Vegetation Line"),
	Inland			UMETA(DisplayName = "Inland")
};

/** Properties of a surface type affecting gameplay. */
USTRUCT(BlueprintType)
struct FSHSurfaceProperties
{
	GENERATED_BODY()

	/** Movement speed multiplier (1.0 = normal). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface")
	float MovementSpeedMultiplier = 1.f;

	/** Movement sound volume multiplier (1.0 = normal). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface")
	float FootstepVolumeMultiplier = 1.f;

	/** Movement sound detection radius multiplier for AI. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface")
	float FootstepDetectionMultiplier = 1.f;

	/** Prone crawl speed multiplier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface")
	float CrawlSpeedMultiplier = 1.f;

	/** Can the player go prone on this surface? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface")
	bool bAllowProne = true;

	/** Does this surface leave visible tracks? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface")
	bool bLeavesFootprints = false;

	/** Is this surface flammable? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface")
	bool bFlammable = false;

	/** Particle effect to spawn on footstep. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface")
	TSoftObjectPtr<UParticleSystem> FootstepParticle;

	/** Sound cue for footsteps. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface")
	TSoftObjectPtr<USoundBase> FootstepSound;

	/** Sound cue for bullet impact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Surface")
	TSoftObjectPtr<USoundBase> BulletImpactSound;
};

/** Water depth query result. */
USTRUCT(BlueprintType)
struct FSHWaterDepthResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Water")
	bool bIsInWater = false;

	/** Depth in centimeters from surface. */
	UPROPERTY(BlueprintReadOnly, Category = "Water")
	float DepthCm = 0.f;

	/** Can the character wade at this depth? */
	UPROPERTY(BlueprintReadOnly, Category = "Water")
	bool bCanWade = false;

	/** Must the character swim at this depth? */
	UPROPERTY(BlueprintReadOnly, Category = "Water")
	bool bMustSwim = false;

	/** Water current direction and strength. */
	UPROPERTY(BlueprintReadOnly, Category = "Water")
	FVector CurrentVelocity = FVector::ZeroVector;
};

/** Elevation query result for tactical calculations. */
USTRUCT(BlueprintType)
struct FSHElevationData
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Elevation")
	float Elevation = 0.f;

	/** Slope angle in degrees (0 = flat, 90 = vertical). */
	UPROPERTY(BlueprintReadOnly, Category = "Elevation")
	float SlopeDegrees = 0.f;

	/** Surface normal at sampled point. */
	UPROPERTY(BlueprintReadOnly, Category = "Elevation")
	FVector SurfaceNormal = FVector::UpVector;

	/** Relative height advantage compared to a reference point. */
	UPROPERTY(BlueprintReadOnly, Category = "Elevation")
	float HeightAdvantage = 0.f;

	/** Is this position in defilade (protected from fire from below)? */
	UPROPERTY(BlueprintReadOnly, Category = "Elevation")
	bool bIsDefilade = false;
};

/** Vegetation interaction data. */
USTRUCT(BlueprintType)
struct FSHVegetationState
{
	GENERATED_BODY()

	/** Is the vegetation at this location disturbed (parted by movement)? */
	UPROPERTY(BlueprintReadOnly, Category = "Vegetation")
	bool bIsDisturbed = false;

	/** Is the vegetation burning? */
	UPROPERTY(BlueprintReadOnly, Category = "Vegetation")
	bool bIsOnFire = false;

	/** Concealment value (0 = fully exposed, 1 = fully concealed). */
	UPROPERTY(BlueprintReadOnly, Category = "Vegetation")
	float ConcealmentValue = 0.f;

	/** Time until vegetation springs back to undisturbed (seconds). */
	UPROPERTY(BlueprintReadOnly, Category = "Vegetation")
	float RecoveryTimeRemaining = 0.f;
};

/**
 * ASHTerrainManager
 *
 * Central terrain and environment system for the Taoyuan Beach operational area.
 * Manages surface types, beach zones, water depth, vegetation interaction,
 * elevation data, and weather effects on terrain. All queries are designed for
 * high-frequency use by movement, AI, and audio systems.
 */
UCLASS()
class SHATTEREDHORIZON2032_API ASHTerrainManager : public AActor
{
	GENERATED_BODY()

public:
	ASHTerrainManager();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ------------------------------------------------------------------
	//  Surface / Material Queries
	// ------------------------------------------------------------------

	/** Get the surface type at a world position (traces downward). */
	UFUNCTION(BlueprintCallable, Category = "SH|Terrain")
	ESHSurfaceType GetSurfaceTypeAtLocation(const FVector& WorldLocation) const;

	/** Get full surface properties for a surface type. */
	UFUNCTION(BlueprintPure, Category = "SH|Terrain")
	const FSHSurfaceProperties& GetSurfaceProperties(ESHSurfaceType SurfaceType) const;

	/** Get the beach zone for a world position. */
	UFUNCTION(BlueprintCallable, Category = "SH|Terrain")
	ESHBeachZone GetBeachZoneAtLocation(const FVector& WorldLocation) const;

	/** Get movement speed multiplier at a position (combines surface + weather + water). */
	UFUNCTION(BlueprintCallable, Category = "SH|Terrain")
	float GetMovementMultiplierAtLocation(const FVector& WorldLocation) const;

	// ------------------------------------------------------------------
	//  Footstep Sound System
	// ------------------------------------------------------------------

	/** Get the footstep sound for a given surface and stance. */
	UFUNCTION(BlueprintCallable, Category = "SH|Terrain|Audio")
	USoundBase* GetFootstepSound(ESHSurfaceType SurfaceType, bool bIsSprinting, bool bIsCrouched, bool bIsProne) const;

	/** Get footstep particle effect for a surface type. */
	UFUNCTION(BlueprintCallable, Category = "SH|Terrain|Audio")
	UParticleSystem* GetFootstepParticle(ESHSurfaceType SurfaceType) const;

	/** Get the AI-audible range of a footstep at this location. */
	UFUNCTION(BlueprintPure, Category = "SH|Terrain|Audio")
	float GetFootstepAudibleRange(const FVector& WorldLocation, bool bIsSprinting, bool bIsCrouched) const;

	// ------------------------------------------------------------------
	//  Water System
	// ------------------------------------------------------------------

	/** Query water depth and properties at a world position. */
	UFUNCTION(BlueprintCallable, Category = "SH|Terrain|Water")
	FSHWaterDepthResult GetWaterDepthAtLocation(const FVector& WorldLocation) const;

	/** Get the current direction for water at this position. */
	UFUNCTION(BlueprintPure, Category = "SH|Terrain|Water")
	FVector GetWaterCurrentAtLocation(const FVector& WorldLocation) const;

	// ------------------------------------------------------------------
	//  Elevation & Tactical
	// ------------------------------------------------------------------

	/** Get elevation data at a position with tactical context relative to a reference. */
	UFUNCTION(BlueprintCallable, Category = "SH|Terrain|Tactical")
	FSHElevationData GetElevationData(const FVector& WorldLocation, const FVector& ReferenceLocation) const;

	/** Compute whether a position has a height advantage over another. */
	UFUNCTION(BlueprintPure, Category = "SH|Terrain|Tactical")
	float ComputeHeightAdvantage(const FVector& FromLocation, const FVector& TargetLocation) const;

	/** Check line of sight through terrain only (ignores actors). */
	UFUNCTION(BlueprintCallable, Category = "SH|Terrain|Tactical")
	bool HasTerrainLineOfSight(const FVector& From, const FVector& To) const;

	// ------------------------------------------------------------------
	//  Vegetation Interaction
	// ------------------------------------------------------------------

	/** Disturb vegetation at a location (called when characters move through). */
	UFUNCTION(BlueprintCallable, Category = "SH|Terrain|Vegetation")
	void DisturbVegetation(const FVector& WorldLocation, float Radius);

	/** Set vegetation on fire at a location. */
	UFUNCTION(BlueprintCallable, Category = "SH|Terrain|Vegetation")
	void IgniteVegetation(const FVector& WorldLocation, float Radius);

	/** Query vegetation state at a location. */
	UFUNCTION(BlueprintCallable, Category = "SH|Terrain|Vegetation")
	FSHVegetationState GetVegetationState(const FVector& WorldLocation) const;

	/** Get concealment value at a position (from vegetation, terrain features). */
	UFUNCTION(BlueprintPure, Category = "SH|Terrain|Vegetation")
	float GetConcealmentAtLocation(const FVector& WorldLocation) const;

	// ------------------------------------------------------------------
	//  Weather Effects on Terrain
	// ------------------------------------------------------------------

	/** Called by the game state when weather changes. Updates terrain properties. */
	UFUNCTION(BlueprintCallable, Category = "SH|Terrain|Weather")
	void OnWeatherChanged(ESHWeatherCondition NewWeather);

	/** Get the current mud level at a position (0..1, affected by rain). */
	UFUNCTION(BlueprintPure, Category = "SH|Terrain|Weather")
	float GetMudLevelAtLocation(const FVector& WorldLocation) const;

	/** Get visibility modifier from weather at a location. */
	UFUNCTION(BlueprintPure, Category = "SH|Terrain|Weather")
	float GetWeatherVisibilityModifier() const { return WeatherVisibilityModifier; }

protected:
	// ------------------------------------------------------------------
	//  Internal
	// ------------------------------------------------------------------

	/** Perform a trace to find the physical material at a location. */
	ESHSurfaceType TraceForSurface(const FVector& WorldLocation) const;

	/** Map UE physical material to our surface type enum. */
	ESHSurfaceType PhysMatToSurfaceType(const UPhysicalMaterial* PhysMat) const;

	/** Update time-varying terrain state (vegetation recovery, fire spread, mud). */
	void TickVegetation(float DeltaSeconds);
	void TickFireSpread(float DeltaSeconds);
	void TickMudAccumulation(float DeltaSeconds);

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Surface properties lookup table (populated in constructor or via data table). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Terrain|Surfaces")
	TMap<ESHSurfaceType, FSHSurfaceProperties> SurfacePropertiesMap;

	/** Sea level elevation (Z coordinate in world space). */
	UPROPERTY(EditAnywhere, Category = "SH|Terrain|Water")
	float SeaLevelZ = 0.f;

	/** Maximum wadeable water depth in cm. */
	UPROPERTY(EditAnywhere, Category = "SH|Terrain|Water")
	float MaxWadeDepthCm = 120.f;

	/** Depth at which swimming begins in cm. */
	UPROPERTY(EditAnywhere, Category = "SH|Terrain|Water")
	float SwimDepthCm = 150.f;

	/** Base water current speed (cm/s). */
	UPROPERTY(EditAnywhere, Category = "SH|Terrain|Water")
	float BaseCurrentSpeed = 50.f;

	/** Water current direction (XY normalized). */
	UPROPERTY(EditAnywhere, Category = "SH|Terrain|Water")
	FVector2D CurrentDirection = FVector2D(1.f, 0.f);

	/** Beach zone Y-axis boundaries (distances from shoreline reference). */
	UPROPERTY(EditAnywhere, Category = "SH|Terrain|Beach")
	float ShorelineY = 0.f;

	UPROPERTY(EditAnywhere, Category = "SH|Terrain|Beach")
	float SurfZoneWidth = 3000.f;

	UPROPERTY(EditAnywhere, Category = "SH|Terrain|Beach")
	float WetSandWidth = 2000.f;

	UPROPERTY(EditAnywhere, Category = "SH|Terrain|Beach")
	float DrySandWidth = 4000.f;

	UPROPERTY(EditAnywhere, Category = "SH|Terrain|Beach")
	float DuneWidth = 3000.f;

	UPROPERTY(EditAnywhere, Category = "SH|Terrain|Beach")
	float VegetationLineWidth = 2000.f;

	/** Time for disturbed vegetation to recover (seconds). */
	UPROPERTY(EditAnywhere, Category = "SH|Terrain|Vegetation")
	float VegetationRecoveryTime = 8.f;

	/** Fire spread rate in cm/s. */
	UPROPERTY(EditAnywhere, Category = "SH|Terrain|Vegetation")
	float FireSpreadRate = 50.f;

	/** Rain accumulation rate for mud (0..1 per second of rain). */
	UPROPERTY(EditAnywhere, Category = "SH|Terrain|Weather")
	float MudAccumulationRate = 0.01f;

	/** Mud drying rate (0..1 per second without rain). */
	UPROPERTY(EditAnywhere, Category = "SH|Terrain|Weather")
	float MudDryingRate = 0.002f;

	/** Base footstep audible range (cm) for AI detection. */
	UPROPERTY(EditAnywhere, Category = "SH|Terrain|Audio")
	float BaseFootstepAudibleRange = 1500.f;

	/** Sprint footstep range multiplier. */
	UPROPERTY(EditAnywhere, Category = "SH|Terrain|Audio")
	float SprintFootstepMultiplier = 2.5f;

	/** Crouch footstep range multiplier. */
	UPROPERTY(EditAnywhere, Category = "SH|Terrain|Audio")
	float CrouchFootstepMultiplier = 0.4f;

private:
	/** Active vegetation disturbances. */
	struct FVegetationDisturbance
	{
		FVector Location;
		float Radius;
		float TimeRemaining;
	};
	TArray<FVegetationDisturbance> ActiveDisturbances;

	/** Active fire zones. */
	struct FFireZone
	{
		FVector Location;
		float Radius;
		float Lifetime;
	};
	TArray<FFireZone> ActiveFires;

	/** Current global mud level (0..1), affected by rain duration. */
	float GlobalMudLevel = 0.f;

	/** Current weather condition. */
	ESHWeatherCondition CurrentWeather = ESHWeatherCondition::Clear;

	/** Weather-based visibility modifier. */
	float WeatherVisibilityModifier = 1.f;

	/** Cached default surface properties. */
	FSHSurfaceProperties DefaultSurfaceProperties;

	/** Cached game state. */
	UPROPERTY()
	TObjectPtr<ASHGameState> CachedGameState = nullptr;
};
