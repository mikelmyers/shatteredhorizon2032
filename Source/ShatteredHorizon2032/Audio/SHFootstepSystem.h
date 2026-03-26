// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "SHFootstepSystem.generated.h"

// -----------------------------------------------------------------------
//  Surface type — drives sound, VFX, and AI detection
// -----------------------------------------------------------------------

UENUM(BlueprintType)
enum class ESHSurfaceType : uint8
{
	Concrete    UMETA(DisplayName = "Concrete"),
	Asphalt     UMETA(DisplayName = "Asphalt"),
	Metal       UMETA(DisplayName = "Metal Grate"),
	Wood        UMETA(DisplayName = "Wood / Timber"),
	Tile        UMETA(DisplayName = "Ceramic Tile"),
	Dirt        UMETA(DisplayName = "Dirt / Earth"),
	Sand        UMETA(DisplayName = "Sand"),
	Mud         UMETA(DisplayName = "Mud"),
	Grass       UMETA(DisplayName = "Grass"),
	Gravel      UMETA(DisplayName = "Gravel"),
	Water       UMETA(DisplayName = "Shallow Water"),
	Snow        UMETA(DisplayName = "Snow"),
	Debris      UMETA(DisplayName = "Debris / Rubble"),
	Glass       UMETA(DisplayName = "Broken Glass"),
	Carpet      UMETA(DisplayName = "Carpet / Rug")
};

// -----------------------------------------------------------------------
//  Footstep sound entry
// -----------------------------------------------------------------------

USTRUCT(BlueprintType)
struct FSHFootstepSoundEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	ESHSurfaceType Surface = ESHSurfaceType::Concrete;

	/** Walk step sounds (randomly selected). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	TArray<TSoftObjectPtr<USoundBase>> WalkSounds;

	/** Run step sounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	TArray<TSoftObjectPtr<USoundBase>> RunSounds;

	/** Sprint step sounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	TArray<TSoftObjectPtr<USoundBase>> SprintSounds;

	/** Crouch step sounds (quieter). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	TArray<TSoftObjectPtr<USoundBase>> CrouchSounds;

	/** Prone crawl sounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	TArray<TSoftObjectPtr<USoundBase>> ProneSounds;

	/** Landing sound (from jump/vault). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	TArray<TSoftObjectPtr<USoundBase>> LandingSounds;

	/** Volume multiplier for this surface (metal louder than carpet). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep", meta = (ClampMin = "0", ClampMax = "3"))
	float VolumeMultiplier = 1.0f;

	/** AI detection range multiplier (metal grate = louder = detected further). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep", meta = (ClampMin = "0", ClampMax = "3"))
	float DetectionRangeMultiplier = 1.0f;

	/** Particle VFX for footstep (dust puff, water splash, mud splat). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Footstep")
	TSoftObjectPtr<UParticleSystem> StepVFX;
};

// -----------------------------------------------------------------------
//  Delegates
// -----------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnFootstepPlayed, ESHSurfaceType, Surface, FVector, Location, float, Volume);

// -----------------------------------------------------------------------
//  USHFootstepSystem
// -----------------------------------------------------------------------

/**
 * USHFootstepSystem
 *
 * Component that detects the surface type under the character's feet
 * and plays appropriate footstep sounds, spawns VFX, and reports to
 * the AI hearing system. Surfaces are identified via Physical Materials
 * on collision geometry.
 *
 * Surface detection → Sound selection → Volume calculation → AI report.
 */
UCLASS(ClassGroup = (SH), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHFootstepSystem : public UActorComponent
{
	GENERATED_BODY()

public:
	USHFootstepSystem();

	// ------------------------------------------------------------------
	//  Primary interface (called from AnimNotify or character Tick)
	// ------------------------------------------------------------------

	/**
	 * Called on each footstep event (animation notify or distance-based).
	 * Traces downward, identifies surface, plays sound, spawns VFX,
	 * and reports noise to AI.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Audio|Footstep")
	void PlayFootstep(bool bIsRightFoot);

	/** Play a landing sound (after vault/fall). */
	UFUNCTION(BlueprintCallable, Category = "SH|Audio|Footstep")
	void PlayLanding(float FallSpeed);

	// ------------------------------------------------------------------
	//  Queries
	// ------------------------------------------------------------------

	/** Get the current surface type under the character. */
	UFUNCTION(BlueprintPure, Category = "SH|Audio|Footstep")
	ESHSurfaceType GetCurrentSurface() const { return CurrentSurface; }

	/** Get the volume for the current movement mode. */
	UFUNCTION(BlueprintPure, Category = "SH|Audio|Footstep")
	float GetCurrentFootstepVolume() const;

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Footstep sound database keyed by surface type. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Footstep")
	TArray<FSHFootstepSoundEntry> FootstepDatabase;

	/** Trace distance below character for surface detection (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Footstep", meta = (ClampMin = "10"))
	float TraceDistance = 50.0f;

	/** Volume multiplier when crouching. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Footstep", meta = (ClampMin = "0", ClampMax = "1"))
	float CrouchVolumeMultiplier = 0.4f;

	/** Volume multiplier when sprinting. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Footstep", meta = (ClampMin = "1", ClampMax = "3"))
	float SprintVolumeMultiplier = 1.6f;

	/** Volume multiplier when prone crawling. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Footstep", meta = (ClampMin = "0", ClampMax = "1"))
	float ProneVolumeMultiplier = 0.2f;

	/** Mapping from UE Physical Material surface type to our surface enum. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Footstep")
	TMap<TEnumAsByte<EPhysicalSurface>, ESHSurfaceType> PhysMatMapping;

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Audio|Footstep")
	FOnFootstepPlayed OnFootstepPlayed;

protected:
	/** Trace downward and identify the surface. */
	ESHSurfaceType DetectSurface(FVector& OutImpactPoint) const;

	/** Find the footstep entry for a surface type. */
	const FSHFootstepSoundEntry* FindFootstepEntry(ESHSurfaceType Surface) const;

	/** Get the movement mode for sound selection. */
	enum class EMovementMode { Walk, Run, Sprint, Crouch, Prone };
	EMovementMode GetCurrentMovementMode() const;

	/** Initialize default footstep database if empty. */
	virtual void BeginPlay() override;
	void InitDefaultDatabase();

private:
	ESHSurfaceType CurrentSurface = ESHSurfaceType::Concrete;
};
