// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SHAmbientSoundscape.generated.h"

class UAudioComponent;
class USoundBase;
class UCurveFloat;

// ─────────────────────────────────────────────────────────────
//  Ambient layer data
// ─────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct FSHAmbientLayer
{
	GENERATED_BODY()

	/** Display name for editor identification. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient|Layer")
	FName LayerName = NAME_None;

	/** Sound asset to play for this layer. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient|Layer")
	TObjectPtr<USoundBase> Sound = nullptr;

	/** Base volume when fully active [0..1]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient|Layer", meta = (ClampMin = "0", ClampMax = "1"))
	float BaseVolume = 1.f;

	/** Current target volume [0..1]. Driven by time-of-day, weather, combat proximity, etc. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Ambient|Layer")
	float VolumeTarget = 0.f;

	/** Crossfade speed (units per second). Higher = faster transitions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient|Layer", meta = (ClampMin = "0.01"))
	float CrossfadeSpeed = 0.5f;

	/**
	 * Optional volume curve over normalized time-of-day [0..1].
	 * X axis: 0 = midnight, 0.25 = 06:00, 0.5 = noon, 0.75 = 18:00.
	 * Y axis: volume multiplier [0..1].
	 * If null, the layer is not modulated by time of day.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient|Layer")
	TObjectPtr<UCurveFloat> TimeOfDayVolumeCurve = nullptr;

	/**
	 * Time-of-day range during which this layer is active.
	 * X = start hour (0-24), Y = end hour (0-24).
	 * If both are 0, the layer is always active (time range ignored, curve still applies).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient|Layer")
	FVector2D ActiveTimeRange = FVector2D::ZeroVector;

	/** If true, this layer responds to weather (rain intensity modulates volume). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient|Layer")
	bool bWeatherDriven = false;

	/** If true, this layer responds to nearby combat intensity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ambient|Layer")
	bool bCombatProximityDriven = false;

	/** Runtime: current interpolated volume. */
	UPROPERTY()
	float CurrentVolume = 0.f;

	/** Runtime: spawned audio component for this layer. */
	UPROPERTY()
	TObjectPtr<UAudioComponent> AudioComponent = nullptr;

	/** Runtime: whether this layer is currently active (playing). */
	UPROPERTY()
	bool bIsPlaying = false;
};

// ─────────────────────────────────────────────────────────────
//  Delegates
// ─────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnAmbientLayerChanged,
	FName, LayerName,
	float, NewVolume);

// ─────────────────────────────────────────────────────────────
//  USHAmbientSoundscape — Actor Component
// ─────────────────────────────────────────────────────────────

/**
 * USHAmbientSoundscape
 *
 * Actor component on the player character that manages a layered
 * ambient audio system. Up to 8 simultaneous ambient layers can
 * play, each driven by time-of-day, weather, and combat proximity.
 *
 * Layer categories:
 *  - Distant battle: increases with nearby combat intensity
 *  - Wind: always present, modulated by weather
 *  - Insects/birds: time-of-day driven (dawn chorus, midday, night)
 *  - Radio chatter: squad communication ambience
 *
 * Each layer crossfades independently to its target volume.
 */
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHAmbientSoundscape : public UActorComponent
{
	GENERATED_BODY()

public:
	USHAmbientSoundscape();

	// ------------------------------------------------------------------
	//  UActorComponent overrides
	// ------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ------------------------------------------------------------------
	//  External input
	// ------------------------------------------------------------------

	/**
	 * Set the current time of day for ambient modulation.
	 * @param NormalizedTime  0 = midnight, 0.25 = 06:00, 0.5 = noon, 0.75 = 18:00, 1.0 = midnight.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Audio|Ambient")
	void SetTimeOfDay(float NormalizedTime);

	/**
	 * Set the current hour of day (0-24). Converted internally to normalized time.
	 * @param Hour  Hour of day (0.0 to 24.0).
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Audio|Ambient")
	void SetTimeOfDayHours(float Hour);

	/**
	 * Set the rain intensity for weather-driven layers.
	 * @param Intensity  0 = no rain, 1 = heavy downpour.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Audio|Ambient")
	void SetRainIntensity(float Intensity);

	/**
	 * Set the wind intensity for weather-driven layers.
	 * @param Intensity  0 = calm, 1 = storm-force winds.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Audio|Ambient")
	void SetWindIntensity(float Intensity);

	/**
	 * Set the nearby combat intensity for combat-proximity-driven layers.
	 * Typically fed from USHAudioSystem::GetCombatIntensity().
	 * @param Intensity  0 = no combat, 1 = intense firefight.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Audio|Ambient")
	void SetCombatProximityIntensity(float Intensity);

	// ------------------------------------------------------------------
	//  Layer management
	// ------------------------------------------------------------------

	/** Activate a specific layer by name. */
	UFUNCTION(BlueprintCallable, Category = "SH|Audio|Ambient")
	void ActivateLayer(FName LayerName);

	/** Deactivate a specific layer by name (crossfades out). */
	UFUNCTION(BlueprintCallable, Category = "SH|Audio|Ambient")
	void DeactivateLayer(FName LayerName);

	/** Force all layers to immediately stop. */
	UFUNCTION(BlueprintCallable, Category = "SH|Audio|Ambient")
	void StopAllLayers();

	// ------------------------------------------------------------------
	//  Queries
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|Audio|Ambient")
	float GetTimeOfDay() const { return CurrentTimeOfDay; }

	UFUNCTION(BlueprintPure, Category = "SH|Audio|Ambient")
	float GetRainIntensity() const { return CurrentRainIntensity; }

	UFUNCTION(BlueprintPure, Category = "SH|Audio|Ambient")
	float GetWindIntensity() const { return CurrentWindIntensity; }

	UFUNCTION(BlueprintPure, Category = "SH|Audio|Ambient")
	float GetCombatProximityIntensity() const { return CurrentCombatIntensity; }

	UFUNCTION(BlueprintPure, Category = "SH|Audio|Ambient")
	int32 GetActiveLayerCount() const;

	/** Get a copy of the layer data array. */
	UFUNCTION(BlueprintPure, Category = "SH|Audio|Ambient")
	const TArray<FSHAmbientLayer>& GetLayers() const { return AmbientLayers; }

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Audio|Ambient")
	FOnAmbientLayerChanged OnAmbientLayerChanged;

protected:
	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Maximum number of simultaneous ambient layers. */
	static constexpr int32 MaxAmbientLayers = 8;

	/**
	 * Ambient layer definitions. Configure up to 8 in editor.
	 * Each layer has a sound cue, volume curve, and driving parameters.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Ambient|Config")
	TArray<FSHAmbientLayer> AmbientLayers;

	/** Global volume multiplier for all ambient layers. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Ambient|Config", meta = (ClampMin = "0", ClampMax = "2"))
	float GlobalVolumeMultiplier = 1.f;

	/** Minimum volume below which a layer is stopped entirely (to save resources). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio|Ambient|Config", meta = (ClampMin = "0", ClampMax = "0.1"))
	float MinActiveVolume = 0.01f;

private:
	// ------------------------------------------------------------------
	//  Internal helpers
	// ------------------------------------------------------------------

	/** Compute the target volume for a single layer given current conditions. */
	float ComputeLayerTargetVolume(const FSHAmbientLayer& Layer) const;

	/** Check if a layer should be active at the current time of day. */
	bool IsLayerActiveForTimeOfDay(const FSHAmbientLayer& Layer) const;

	/** Get the time-of-day volume multiplier for a layer. */
	float GetTimeOfDayMultiplier(const FSHAmbientLayer& Layer) const;

	/** Spawn or retrieve the audio component for a layer. */
	void EnsureAudioComponent(FSHAmbientLayer& Layer);

	/** Start playback for a layer. */
	void StartLayerPlayback(FSHAmbientLayer& Layer);

	/** Stop playback for a layer. */
	void StopLayerPlayback(FSHAmbientLayer& Layer);

	/** Find a layer by name. Returns nullptr if not found. */
	FSHAmbientLayer* FindLayerByName(FName LayerName);

	// ------------------------------------------------------------------
	//  Runtime state
	// ------------------------------------------------------------------

	/** Normalized time of day [0..1]. 0 = midnight, 0.5 = noon. */
	float CurrentTimeOfDay = 0.25f; // Default to 06:00.

	/** Rain intensity [0..1]. */
	float CurrentRainIntensity = 0.f;

	/** Wind intensity [0..1]. */
	float CurrentWindIntensity = 0.f;

	/** Nearby combat intensity [0..1]. */
	float CurrentCombatIntensity = 0.f;
};
