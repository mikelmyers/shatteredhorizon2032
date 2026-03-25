// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SHWeatherSystem.generated.h"

class USHBallisticsSystem;
class USHAudioSystem;
class ASHGameMode;
struct FSHWindState;

DECLARE_LOG_CATEGORY_EXTERN(LogSH_Weather, Log, All);

/* -----------------------------------------------------------------------
 *  Weather type enum
 * --------------------------------------------------------------------- */

UENUM(BlueprintType)
enum class ESHWeatherType : uint8
{
	Clear         UMETA(DisplayName = "Clear"),
	Overcast      UMETA(DisplayName = "Overcast"),
	LightRain     UMETA(DisplayName = "Light Rain"),
	HeavyRain     UMETA(DisplayName = "Heavy Rain"),
	Fog           UMETA(DisplayName = "Fog"),
	DenseFog      UMETA(DisplayName = "Dense Fog"),
	Storm         UMETA(DisplayName = "Storm"),
	Snow          UMETA(DisplayName = "Snow"),
	ExtremeHeat   UMETA(DisplayName = "Extreme Heat")
};

/* -----------------------------------------------------------------------
 *  Time-of-day enum
 * --------------------------------------------------------------------- */

UENUM(BlueprintType)
enum class ESHTimeOfDay : uint8
{
	Dawn          UMETA(DisplayName = "Dawn"),
	Morning       UMETA(DisplayName = "Morning"),
	Midday        UMETA(DisplayName = "Midday"),
	Afternoon     UMETA(DisplayName = "Afternoon"),
	Dusk          UMETA(DisplayName = "Dusk"),
	Night         UMETA(DisplayName = "Night")
};

/* -----------------------------------------------------------------------
 *  Weather state — raw environmental parameters
 * --------------------------------------------------------------------- */

USTRUCT(BlueprintType)
struct FSHWeatherState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	ESHWeatherType WeatherType = ESHWeatherType::Clear;

	/** Wind direction in world space (normalized). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	FVector WindDirection = FVector(1.0f, 0.0f, 0.0f);

	/** Wind speed in meters per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0"))
	float WindSpeedMPS = 0.0f;

	/** Ambient temperature in degrees Celsius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	float Temperature = 25.0f;

	/** Visibility distance in meters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0"))
	float Visibility = 10000.0f;

	/** Precipitation intensity (0 = none, 1 = maximum). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0", ClampMax = "1"))
	float Precipitation = 0.0f;

	/** Cloud cover fraction (0 = clear sky, 1 = total overcast). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0", ClampMax = "1"))
	float CloudCover = 0.0f;

	/** Current time of day derived from the game mode. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	ESHTimeOfDay TimeOfDay = ESHTimeOfDay::Midday;
};

/* -----------------------------------------------------------------------
 *  Weather gameplay effects — computed each frame from weather state
 * --------------------------------------------------------------------- */

USTRUCT(BlueprintType)
struct FSHWeatherGameplayEffects
{
	GENERATED_BODY()

	/** Multiplier for visual detection ranges (rain/fog reduce). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Effects", meta = (ClampMin = "0", ClampMax = "1"))
	float VisualDetectionMultiplier = 1.0f;

	/** Multiplier for acoustic detection ranges (rain masks, wind carries). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Effects", meta = (ClampMin = "0", ClampMax = "1"))
	float AcousticDetectionMultiplier = 1.0f;

	/** Wind force vector fed to the ballistics system (world-space, Newtons-equivalent). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Effects")
	FVector BallisticWindForce = FVector::ZeroVector;

	/** Fine motor penalty from cold (0 = no penalty, 1 = fully degraded). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Effects", meta = (ClampMin = "0", ClampMax = "1"))
	float FineMotorPenalty = 0.0f;

	/** Equipment reliability multiplier (cold/wet increase malfunction chance). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Effects", meta = (ClampMin = "0", ClampMax = "1"))
	float EquipmentReliabilityMultiplier = 1.0f;

	/** Movement speed multiplier (mud from rain slows movement). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Effects", meta = (ClampMin = "0", ClampMax = "1"))
	float MovementSpeedMultiplier = 1.0f;

	/** Thermal detection multiplier (cold makes thermal imaging more effective). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Effects", meta = (ClampMin = "0", ClampMax = "1"))
	float ThermalDetectionMultiplier = 1.0f;

	/** True when NVGs are required for effective operations. */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Effects")
	bool NVGRequired = false;

	/** Multiplier for sound propagation distances (affects audible ranges globally). */
	UPROPERTY(BlueprintReadOnly, Category = "Weather|Effects", meta = (ClampMin = "0", ClampMax = "1"))
	float SoundPropagationMultiplier = 1.0f;
};

/* -----------------------------------------------------------------------
 *  Delegates
 * --------------------------------------------------------------------- */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnWeatherChanged,
	ESHWeatherType, OldWeather,
	ESHWeatherType, NewWeather);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnTimeOfDayChanged,
	ESHTimeOfDay, OldTimeOfDay,
	ESHTimeOfDay, NewTimeOfDay);

/* -----------------------------------------------------------------------
 *  USHWeatherSystem — World Subsystem
 *
 *  Manages weather state and drives gameplay effects across all
 *  combat systems. Weather is a tactical condition: rain masks
 *  sound signatures, fog reduces visibility equally for all
 *  combatants, cold degrades fine motor skills, and wind deflects
 *  ballistic trajectories.
 * --------------------------------------------------------------------- */

UCLASS()
class SHATTEREDHORIZON2032_API USHWeatherSystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	USHWeatherSystem();

	// ------------------------------------------------------------------
	//  USubsystem interface
	// ------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	// ------------------------------------------------------------------
	//  Weather control
	// ------------------------------------------------------------------

	/** Immediately set the weather type and apply matching defaults. */
	UFUNCTION(BlueprintCallable, Category = "SH|Weather")
	void SetWeather(ESHWeatherType NewWeatherType);

	/** Set wind direction (normalized) and speed (m/s). */
	UFUNCTION(BlueprintCallable, Category = "SH|Weather")
	void SetWindState(FVector Direction, float SpeedMPS);

	/** Set ambient temperature in degrees Celsius. */
	UFUNCTION(BlueprintCallable, Category = "SH|Weather")
	void SetTemperature(float Celsius);

	/** Smoothly transition to a target weather type over DurationSeconds. */
	UFUNCTION(BlueprintCallable, Category = "SH|Weather")
	void TransitionWeather(ESHWeatherType Target, float DurationSeconds);

	// ------------------------------------------------------------------
	//  Queries
	// ------------------------------------------------------------------

	/** Get the current weather state. */
	UFUNCTION(BlueprintPure, Category = "SH|Weather")
	FSHWeatherState GetWeatherState() const { return CurrentState; }

	/** Get the computed gameplay effects for this frame. */
	UFUNCTION(BlueprintPure, Category = "SH|Weather")
	FSHWeatherGameplayEffects GetGameplayEffects() const { return CurrentEffects; }

	/** Get the current time of day (derived from game mode). */
	UFUNCTION(BlueprintPure, Category = "SH|Weather")
	ESHTimeOfDay GetTimeOfDay() const { return CurrentState.TimeOfDay; }

	/** Is a weather transition currently in progress? */
	UFUNCTION(BlueprintPure, Category = "SH|Weather")
	bool IsTransitioning() const { return bIsTransitioning; }

	/** Get transition progress (0-1, 0 if not transitioning). */
	UFUNCTION(BlueprintPure, Category = "SH|Weather")
	float GetTransitionProgress() const;

	// ------------------------------------------------------------------
	//  Weather presets — theatre-specific configurations
	// ------------------------------------------------------------------

	/** Hot, humid Taipei summer — clear, high temperature, light wind. */
	UFUNCTION(BlueprintCallable, Category = "SH|Weather|Presets")
	void CreatePreset_TaipeiSummer();

	/** Approaching typhoon — heavy rain, high wind, low visibility. */
	UFUNCTION(BlueprintCallable, Category = "SH|Weather|Presets")
	void CreatePreset_TyphoonApproach();

	/** Night assault — dark, overcast, moderate wind. */
	UFUNCTION(BlueprintCallable, Category = "SH|Weather|Presets")
	void CreatePreset_NightAssault();

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Weather")
	FOnWeatherChanged OnWeatherChanged;

	UPROPERTY(BlueprintAssignable, Category = "SH|Weather")
	FOnTimeOfDayChanged OnTimeOfDayChanged;

protected:
	// ------------------------------------------------------------------
	//  Internal tick helpers
	// ------------------------------------------------------------------

	/** Update time of day from the game mode's TimeOfDayHours. */
	void TickTimeOfDay();

	/** Step the weather transition interpolation. */
	void TickWeatherTransition(float DeltaTime);

	/** Recompute FSHWeatherGameplayEffects from current state. */
	void ComputeGameplayEffects();

	/** Push wind data to USHBallisticsSystem. */
	void FeedWindToBallistics();

	/** Push sound propagation data to USHAudioSystem. */
	void FeedSoundPropagationToAudio();

	// ------------------------------------------------------------------
	//  Effect computation helpers
	// ------------------------------------------------------------------

	/** Compute visual detection multiplier from weather + time of day. */
	float ComputeVisualDetectionMultiplier() const;

	/** Compute acoustic detection multiplier from precipitation and wind. */
	float ComputeAcousticDetectionMultiplier() const;

	/** Compute fine motor penalty from temperature. */
	float ComputeFineMotorPenalty() const;

	/** Compute equipment reliability from temperature and precipitation. */
	float ComputeEquipmentReliabilityMultiplier() const;

	/** Compute movement speed multiplier from precipitation (mud). */
	float ComputeMovementSpeedMultiplier() const;

	/** Compute thermal detection multiplier from temperature. */
	float ComputeThermalDetectionMultiplier() const;

	/** Compute sound propagation multiplier from weather conditions. */
	float ComputeSoundPropagationMultiplier() const;

	/** Build default weather parameters for a given weather type. */
	static FSHWeatherState BuildDefaultsForType(ESHWeatherType Type);

	/** Convert game mode hours (0-24) to ESHTimeOfDay. */
	static ESHTimeOfDay HoursToTimeOfDay(float Hours);

	/** Linearly interpolate between two weather states. */
	static FSHWeatherState LerpWeatherState(const FSHWeatherState& A, const FSHWeatherState& B, float Alpha);

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Temperature (Celsius) below which fine motor degradation begins. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Weather|Config")
	float ColdThresholdCelsius = 5.0f;

	/** Temperature (Celsius) at which fine motor degradation is maximum. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Weather|Config")
	float ExtremeColdCelsius = -15.0f;

	/** Wind speed (m/s) at which ballistic deflection becomes significant. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Weather|Config")
	float SignificantWindSpeedMPS = 5.0f;

	/** Maximum visibility distance in meters (clear day, no obstruction). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Weather|Config")
	float MaxVisibilityMeters = 10000.0f;

	/** Minimum visibility distance in meters (dense fog / zero visibility). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Weather|Config")
	float MinVisibilityMeters = 30.0f;

	/** Precipitation level above which ground becomes muddy (slows movement). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Weather|Config", meta = (ClampMin = "0", ClampMax = "1"))
	float MudPrecipitationThreshold = 0.4f;

	/** Maximum movement speed penalty from mud (0 = no penalty, 1 = cannot move). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Weather|Config", meta = (ClampMin = "0", ClampMax = "1"))
	float MaxMudMovementPenalty = 0.3f;

private:
	// ------------------------------------------------------------------
	//  Runtime state
	// ------------------------------------------------------------------

	/** Current weather state. */
	UPROPERTY()
	FSHWeatherState CurrentState;

	/** Computed gameplay effects (refreshed every tick). */
	FSHWeatherGameplayEffects CurrentEffects;

	/** Previous time of day (for change detection). */
	ESHTimeOfDay PreviousTimeOfDay = ESHTimeOfDay::Midday;

	// --- Transition ---

	/** Whether a weather transition is in progress. */
	bool bIsTransitioning = false;

	/** Weather state at the start of the transition. */
	FSHWeatherState TransitionStartState;

	/** Target weather state for the transition end. */
	FSHWeatherState TransitionTargetState;

	/** Target weather type (for delegate broadcast). */
	ESHWeatherType TransitionTargetType = ESHWeatherType::Clear;

	/** Total duration of the transition in seconds. */
	float TransitionDuration = 0.0f;

	/** Elapsed time within the transition. */
	float TransitionElapsed = 0.0f;

	// --- Cached subsystem pointers ---

	UPROPERTY()
	TObjectPtr<USHBallisticsSystem> CachedBallisticsSystem = nullptr;

	UPROPERTY()
	TObjectPtr<USHAudioSystem> CachedAudioSystem = nullptr;
};
