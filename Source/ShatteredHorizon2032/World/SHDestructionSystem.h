// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SHDestructionSystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSH_Destruction, Log, All);

class UGeometryCollectionComponent;
class UStaticMeshComponent;
class UNiagaraSystem;
class USoundBase;

/** Destructible object category — determines fracture behavior. */
UENUM(BlueprintType)
enum class ESHDestructibleType : uint8
{
	Wall			UMETA(DisplayName = "Wall"),
	Roof			UMETA(DisplayName = "Roof"),
	Floor			UMETA(DisplayName = "Floor"),
	SupportColumn	UMETA(DisplayName = "Support Column"),
	Window			UMETA(DisplayName = "Window"),
	Door			UMETA(DisplayName = "Door"),
	Sandbag			UMETA(DisplayName = "Sandbag"),
	Fortification	UMETA(DisplayName = "Fortification"),
	VehicleWreck	UMETA(DisplayName = "Vehicle Wreck"),
	Tree			UMETA(DisplayName = "Tree"),
	Vegetation		UMETA(DisplayName = "Vegetation"),
	Furniture		UMETA(DisplayName = "Furniture"),
	Debris			UMETA(DisplayName = "Debris"),
	Generic			UMETA(DisplayName = "Generic")
};

/**
 * Destruction level — 10 progressive states from pristine to rubble.
 * Each state has distinct visual representation, gameplay impact, and AI implications.
 * This granularity ensures destruction feels continuous, not binary.
 */
UENUM(BlueprintType)
enum class ESHDestructionLevel : uint8
{
	Pristine		UMETA(DisplayName = "Pristine"),                // 100% — untouched
	Scratched		UMETA(DisplayName = "Scratched"),               // 95% — cosmetic marks, paint chipped
	BulletHoles		UMETA(DisplayName = "Bullet Holes"),            // 85% — visible penetration marks, small holes
	Chipped			UMETA(DisplayName = "Chipped / Cracked"),       // 75% — surface cracks, chunks missing
	Fractured		UMETA(DisplayName = "Fractured"),               // 65% — visible fracture lines, structural stress
	Breached		UMETA(DisplayName = "Breached"),                // 50% — hole large enough to see through (new sightline)
	PartialCollapse	UMETA(DisplayName = "Partial Collapse"),        // 35% — section fallen, traversable debris pile
	HeavyDamage		UMETA(DisplayName = "Heavy Damage"),            // 20% — most structure gone, some walls standing
	Skeleton		UMETA(DisplayName = "Skeleton"),                // 10% — only structural frame remains
	Destroyed		UMETA(DisplayName = "Destroyed / Rubble")       // 0% — fully collapsed, rubble pile only
};

/** Registered destructible actor with tracked state. */
USTRUCT(BlueprintType)
struct FSHDestructibleState
{
	GENERATED_BODY()

	/** Unique ID for this destructible. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 DestructibleID = 0;

	/** Weak reference to the actor. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TWeakObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	ESHDestructibleType Type = ESHDestructibleType::Generic;

	/** Maximum hit points. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxHealth = 100.f;

	/** Current hit points. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float CurrentHealth = 100.f;

	/** Current destruction level. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	ESHDestructionLevel DestructionLevel = ESHDestructionLevel::Pristine;

	/** Whether this object is a structural support (destroying it may cause cascading collapse). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsStructuralSupport = false;

	/** Building ID — used for structural integrity graph. Objects with same ID form a building. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 BuildingID = -1;

	/** Distance from player when last updated — used for LOD. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float DistanceFromPlayer = 0.f;

	/** World time when this was last damaged. */
	float LastDamageTime = 0.f;

	/** Number of bullet hits received (drives decal/hole placement). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 BulletHitCount = 0;

	/** Number of explosive hits received. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 ExplosiveHitCount = 0;

	/** Localized damage map — tracks damage concentration per surface region.
	 *  Key: region index (octant), Value: accumulated damage in that region.
	 *  When a region exceeds threshold, it creates a breach (new sightline). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TMap<int32, float> RegionalDamage;

	/** Whether this object has a breach (hole large enough for sightlines/traversal). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bHasBreach = false;

	/** Breach location in local space (for AI pathfinding and sightline checks). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector BreachLocationLocal = FVector::ZeroVector;

	/** Lean/sag angle in degrees. Structures under stress lean before collapsing. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float LeanAngleDeg = 0.0f;

	/** Maximum lean before collapse. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float MaxLeanBeforeCollapse = 8.0f;

	float GetHealthFraction() const { return (MaxHealth > 0.f) ? CurrentHealth / MaxHealth : 0.f; }
};

/** Crater generated by an explosion. */
USTRUCT(BlueprintType)
struct FSHCrater
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 CraterID = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector Location = FVector::ZeroVector;

	/** Crater outer radius in cm. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Radius = 200.f;

	/** Crater depth in cm. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float Depth = 50.f;

	/** World time when created. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float CreationTime = 0.f;

	/** Whether this crater provides meaningful cover (large enough). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bProvidesCover = false;
};

/** Persistent debris piece. */
USTRUCT(BlueprintType)
struct FSHDebrisInfo
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 DebrisID = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	TWeakObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	FVector Location = FVector::ZeroVector;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	float CreationTime = 0.f;

	/** Distance from player at last check. */
	float DistanceFromPlayer = 0.f;

	/** Whether physics simulation is still active. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bPhysicsActive = true;
};

/** Building structural integrity data. */
USTRUCT(BlueprintType)
struct FSHBuildingIntegrity
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 BuildingID = -1;

	/** Total structural supports in this building. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 TotalSupports = 0;

	/** Remaining intact supports. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	int32 RemainingSupports = 0;

	/** Structural integrity (0 = collapsed, 1 = fully intact). */
	float GetIntegrity() const { return (TotalSupports > 0) ? static_cast<float>(RemainingSupports) / TotalSupports : 0.f; }

	/** Threshold below which cascading collapse triggers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float CollapseThreshold = 0.3f;

	/** Whether collapse has already been triggered. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly)
	bool bCollapseTriggered = false;
};

/** Delegate fired when a destructible is destroyed. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDestructibleDestroyed, int32, DestructibleID, ESHDestructibleType, Type);

/** Delegate fired when a building collapses. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBuildingCollapse, int32, BuildingID);

/** Delegate fired when a crater is created. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCraterCreated, const FSHCrater&, Crater);

/**
 * USHDestructionSystem
 *
 * World subsystem managing environmental destruction using UE5 Chaos.
 * Tracks destructible object health, structural integrity of buildings,
 * crater generation, persistent debris, and performance-managed cleanup.
 * All destruction is deterministic and replicable.
 */
UCLASS()
class SHATTEREDHORIZON2032_API USHDestructionSystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	USHDestructionSystem();

	// ------------------------------------------------------------------
	//  USubsystem interface
	// ------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// ------------------------------------------------------------------
	//  Registration
	// ------------------------------------------------------------------

	/** Register a destructible actor with the system. Returns a unique ID. */
	UFUNCTION(BlueprintCallable, Category = "SH|Destruction")
	int32 RegisterDestructible(AActor* Actor, ESHDestructibleType Type, float MaxHealth, bool bIsStructuralSupport = false, int32 BuildingID = -1);

	/** Unregister a destructible (e.g., when it's fully cleaned up). */
	UFUNCTION(BlueprintCallable, Category = "SH|Destruction")
	void UnregisterDestructible(int32 DestructibleID);

	// ------------------------------------------------------------------
	//  Damage
	// ------------------------------------------------------------------

	/** Apply damage to a destructible by ID. Returns remaining health. */
	UFUNCTION(BlueprintCallable, Category = "SH|Destruction")
	float ApplyDamage(int32 DestructibleID, float DamageAmount, FVector ImpactPoint, FVector ImpactNormal, AActor* DamageCauser = nullptr);

	/** Apply area damage (explosion) to all destructibles within radius. */
	UFUNCTION(BlueprintCallable, Category = "SH|Destruction")
	void ApplyAreaDamage(FVector Origin, float Radius, float BaseDamage, AActor* DamageCauser = nullptr);

	/** Apply damage to a specific actor (looks up registration). */
	UFUNCTION(BlueprintCallable, Category = "SH|Destruction")
	float ApplyDamageToActor(AActor* Actor, float DamageAmount, FVector ImpactPoint, FVector ImpactNormal, AActor* DamageCauser = nullptr);

	// ------------------------------------------------------------------
	//  Craters
	// ------------------------------------------------------------------

	/** Create an explosion crater at the given location. */
	UFUNCTION(BlueprintCallable, Category = "SH|Destruction")
	FSHCrater CreateCrater(FVector Location, float Radius, float Depth);

	/** Query craters within a radius (for cover detection). */
	UFUNCTION(BlueprintCallable, Category = "SH|Destruction")
	TArray<FSHCrater> QueryCratersInRadius(FVector Location, float Radius) const;

	// ------------------------------------------------------------------
	//  Queries
	// ------------------------------------------------------------------

	/** Get the destruction state of a registered destructible. */
	UFUNCTION(BlueprintPure, Category = "SH|Destruction")
	bool GetDestructibleState(int32 DestructibleID, FSHDestructibleState& OutState) const;

	/** Get structural integrity of a building. */
	UFUNCTION(BlueprintPure, Category = "SH|Destruction")
	float GetBuildingIntegrity(int32 BuildingID) const;

	/** Get total active debris count. */
	UFUNCTION(BlueprintPure, Category = "SH|Destruction")
	int32 GetActiveDebrisCount() const { return ActiveDebris.Num(); }

	/** Get total crater count. */
	UFUNCTION(BlueprintPure, Category = "SH|Destruction")
	int32 GetCraterCount() const { return Craters.Num(); }

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Destruction")
	FOnDestructibleDestroyed OnDestructibleDestroyed;

	UPROPERTY(BlueprintAssignable, Category = "SH|Destruction")
	FOnBuildingCollapse OnBuildingCollapse;

	UPROPERTY(BlueprintAssignable, Category = "SH|Destruction")
	FOnCraterCreated OnCraterCreated;

protected:
	// ------------------------------------------------------------------
	//  Internal
	// ------------------------------------------------------------------
	void UpdateDestructionLevels();
	void UpdateStructuralIntegrity();
	void TriggerBuildingCollapse(int32 BuildingID);
	void SpawnDebrisForDestructible(const FSHDestructibleState& State, FVector ImpactPoint);
	void TickDebrisCleanup(float DeltaTime);
	void TickPerformanceManagement(float DeltaTime);
	void ActivateChaosDestruction(AActor* Actor, ESHDestructionLevel Level, FVector ImpactPoint, FVector ImpactDirection);

	/** Determine destruction level from health fraction. */
	ESHDestructionLevel ComputeDestructionLevel(float HealthFraction) const;

	/** Get default max health for a destructible type. */
	float GetDefaultMaxHealth(ESHDestructibleType Type) const;

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Maximum number of active debris pieces before cleanup starts. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Destruction|Performance")
	int32 MaxActiveDebris = 500;

	/** Maximum number of craters before oldest are removed. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Destruction|Performance")
	int32 MaxCraters = 100;

	/** Distance at which debris physics is put to sleep (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Destruction|Performance")
	float DebrisPhysicsSleepDistance = 10000.f;

	/** Distance at which distant destruction uses simplified LOD (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Destruction|Performance")
	float DestructionLODDistance = 20000.f;

	/** Time in seconds before settled debris becomes static (optimisation). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Destruction|Performance")
	float DebrisSettleTime = 10.f;

	/** Maximum age of debris before cleanup (seconds). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Destruction|Performance")
	float MaxDebrisAge = 300.f;

	/** Minimum crater radius that provides cover (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Destruction|Craters")
	float MinCoverCraterRadius = 100.f;

	/** Damage falloff exponent for area damage (2.0 = inverse-square). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Destruction|Damage")
	float AreaDamageFalloffExponent = 2.f;

	/** Destruction VFX per type — index matches ESHDestructibleType. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Destruction|VFX")
	TMap<ESHDestructibleType, TSoftObjectPtr<UNiagaraSystem>> DestructionVFX;

	/** Destruction sounds per type. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Destruction|Audio")
	TMap<ESHDestructibleType, TSoftObjectPtr<USoundBase>> DestructionSounds;

private:
	// Runtime state -------------------------------------------------------

	/** All registered destructibles keyed by ID. */
	TMap<int32, FSHDestructibleState> Destructibles;

	/** Actor-to-ID reverse lookup. */
	TMap<TWeakObjectPtr<AActor>, int32> ActorToIDMap;

	/** Building structural integrity data. */
	TMap<int32, FSHBuildingIntegrity> BuildingIntegrityMap;

	/** Active craters. */
	UPROPERTY()
	TArray<FSHCrater> Craters;

	/** Active debris. */
	TArray<FSHDebrisInfo> ActiveDebris;

	/** Next unique IDs. */
	int32 NextDestructibleID = 1;
	int32 NextCraterID = 1;
	int32 NextDebrisID = 1;

	/** Cached player location for distance checks. */
	FVector CachedPlayerLocation = FVector::ZeroVector;

	/** Performance management tick accumulator. */
	float PerfManagementAccumulator = 0.f;
	static constexpr float PerfManagementInterval = 1.f;
};
