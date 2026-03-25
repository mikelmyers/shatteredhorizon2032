// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SHReverbZoneManager.generated.h"

class UAudioComponent;
class UReverbEffect;

// ─────────────────────────────────────────────────────────────
//  Environment type classification
// ─────────────────────────────────────────────────────────────

UENUM(BlueprintType)
enum class ESHEnvironmentType : uint8
{
	OpenField		UMETA(DisplayName = "Open Field"),
	UrbanCanyon		UMETA(DisplayName = "Urban Canyon"),
	Interior		UMETA(DisplayName = "Interior"),
	Tunnel			UMETA(DisplayName = "Tunnel"),
	Forest			UMETA(DisplayName = "Forest")
};

// ─────────────────────────────────────────────────────────────
//  Reverb preset per environment
// ─────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FSHReverbPreset
{
	GENERATED_BODY()

	/** The environment type this preset corresponds to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reverb")
	ESHEnvironmentType EnvironmentType = ESHEnvironmentType::OpenField;

	/** UE reverb effect asset for this environment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reverb")
	TObjectPtr<UReverbEffect> ReverbEffect = nullptr;

	/** Reverb volume (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reverb", meta = (ClampMin = "0", ClampMax = "1"))
	float Volume = 1.f;

	/** Blend priority (higher = preferred when environments overlap). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Reverb")
	float Priority = 1.f;
};

// ─────────────────────────────────────────────────────────────
//  Delegates
// ─────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnEnvironmentTypeChanged,
	ESHEnvironmentType, OldType,
	ESHEnvironmentType, NewType);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnEnclosureRatioChanged,
	float, NewRatio);

// ─────────────────────────────────────────────────────────────
//  USHReverbZoneManager — Actor Component
// ─────────────────────────────────────────────────────────────

/**
 * USHReverbZoneManager
 *
 * Actor component intended for the player character that performs
 * six-directional line traces each update to classify the surrounding
 * environment and drive reverb preset blending.
 *
 * Traces in Forward, Back, Left, Right, Up, and Down from the owning
 * actor. Hit distances are analyzed to compute an enclosure ratio
 * (0 = fully open air, 1 = fully enclosed) and classify the
 * environment type (OpenField, UrbanCanyon, Interior, Tunnel, Forest).
 *
 * The classification drives reverb effect blending via audio volumes
 * or direct submix parameter updates.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHReverbZoneManager : public UActorComponent
{
	GENERATED_BODY()

public:
	USHReverbZoneManager();

	// ------------------------------------------------------------------
	//  UActorComponent overrides
	// ------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ------------------------------------------------------------------
	//  Queries
	// ------------------------------------------------------------------

	/** Get the current classified environment type. */
	UFUNCTION(BlueprintPure, Category = "SH|Audio|Reverb")
	ESHEnvironmentType GetCurrentEnvironmentType() const { return CurrentEnvironmentType; }

	/**
	 * Get the enclosure ratio.
	 * 0 = fully open air (no traces hit geometry).
	 * 1 = fully enclosed (all traces hit within trace distance).
	 */
	UFUNCTION(BlueprintPure, Category = "SH|Audio|Reverb")
	float GetEnclosureRatio() const { return EnclosureRatio; }

	/** Get the trace hit distances for each of the 6 directions (Forward, Back, Left, Right, Up, Down). */
	UFUNCTION(BlueprintPure, Category = "SH|Audio|Reverb")
	const TArray<float>& GetTraceDistances() const { return TraceHitDistances; }

	/** Get the active reverb preset for the current environment. */
	UFUNCTION(BlueprintPure, Category = "SH|Audio|Reverb")
	FSHReverbPreset GetActiveReverbPreset() const;

	// ------------------------------------------------------------------
	//  Configuration setters
	// ------------------------------------------------------------------

	/** Override the trace distance at runtime. */
	UFUNCTION(BlueprintCallable, Category = "SH|Audio|Reverb")
	void SetTraceDistance(float NewDistance);

	/** Override the update interval at runtime. */
	UFUNCTION(BlueprintCallable, Category = "SH|Audio|Reverb")
	void SetUpdateInterval(float NewInterval);

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Audio|Reverb")
	FOnEnvironmentTypeChanged OnEnvironmentTypeChanged;

	UPROPERTY(BlueprintAssignable, Category = "SH|Audio|Reverb")
	FOnEnclosureRatioChanged OnEnclosureRatioChanged;

protected:
	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Maximum trace distance for environment probes (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Reverb|Config")
	float TraceDistance = 5000.f;

	/** Seconds between trace updates. Lower = more responsive, higher = cheaper. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Reverb|Config", meta = (ClampMin = "0.01"))
	float UpdateInterval = 0.1f;

	/** Interpolation speed for enclosure ratio smoothing. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Reverb|Config")
	float EnclosureInterpSpeed = 4.f;

	/** Reverb blend interpolation speed. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Reverb|Config")
	float ReverbBlendSpeed = 2.f;

	/**
	 * Threshold distance (cm) below which a trace direction counts as "enclosed."
	 * Used in environment classification heuristics.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Reverb|Config")
	float EnclosedThreshold = 1500.f;

	/**
	 * Threshold distance (cm) for "nearby wall" used in urban canyon detection.
	 * Lateral traces shorter than this with open sky indicate canyon geometry.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Reverb|Config")
	float UrbanCanyonWallThreshold = 2500.f;

	/** Trace channel to use for environment probes. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Reverb|Config")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECC_Visibility;

	/** Reverb presets per environment type. Configure in editor. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Reverb|Config")
	TArray<FSHReverbPreset> ReverbPresets;

private:
	// ------------------------------------------------------------------
	//  Internal helpers
	// ------------------------------------------------------------------

	/** Perform the six-directional line traces. */
	void PerformEnvironmentTraces();

	/** Classify the environment based on trace results. */
	ESHEnvironmentType ClassifyEnvironment() const;

	/** Apply reverb settings for the current environment. */
	void UpdateReverbSettings(float DeltaTime);

	/** Find the reverb preset for a given environment type. */
	const FSHReverbPreset* FindPresetForType(ESHEnvironmentType Type) const;

	// ------------------------------------------------------------------
	//  Trace directions
	// ------------------------------------------------------------------

	/** The six cardinal directions for tracing. */
	static const FVector TraceDirections[6];

	/** Number of trace directions. */
	static constexpr int32 NumTraceDirections = 6;

	// ------------------------------------------------------------------
	//  Runtime state
	// ------------------------------------------------------------------

	/** Hit distances for each of the 6 trace directions. TraceDistance if no hit. */
	TArray<float> TraceHitDistances;

	/** Current classified environment type. */
	ESHEnvironmentType CurrentEnvironmentType = ESHEnvironmentType::OpenField;

	/** Previous environment type (for change detection). */
	ESHEnvironmentType PreviousEnvironmentType = ESHEnvironmentType::OpenField;

	/** Current enclosure ratio [0..1]. */
	float EnclosureRatio = 0.f;

	/** Raw (unsmoothed) enclosure ratio from last trace update. */
	float RawEnclosureRatio = 0.f;

	/** Time accumulator for update interval. */
	float TimeSinceLastUpdate = 0.f;

	/** Current reverb blend weight [0..1]. */
	float CurrentReverbBlendWeight = 0.f;

	/** Target reverb blend weight. */
	float TargetReverbBlendWeight = 1.f;
};
