// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SHOpticsSystem.generated.h"

class UCameraComponent;
class UPostProcessComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// ─────────────────────────────────────────────────────────────
//  Enums
// ─────────────────────────────────────────────────────────────

/** All available optical enhancement modes. */
UENUM(BlueprintType)
enum class ESHOpticsMode : uint8
{
	None				UMETA(DisplayName = "None"),
	NVG_Gen3			UMETA(DisplayName = "NVG Gen 3"),
	NVG_Gen4			UMETA(DisplayName = "NVG Gen 4"),
	Thermal_FLIR		UMETA(DisplayName = "Thermal FLIR"),
	Thermal_Weapon		UMETA(DisplayName = "Thermal Weapon Sight"),
	Scope_1x			UMETA(DisplayName = "Scope 1x"),
	Scope_4x			UMETA(DisplayName = "Scope 4x"),
	Scope_8x			UMETA(DisplayName = "Scope 8x"),
	Scope_12x			UMETA(DisplayName = "Scope 12x")
};

// ─────────────────────────────────────────────────────────────
//  Per-mode configuration
// ─────────────────────────────────────────────────────────────

/** Configuration data for a single optics mode. */
USTRUCT(BlueprintType)
struct FSHOpticsConfig
{
	GENERATED_BODY()

	/** Field of view override in degrees. 0 = use default camera FOV. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optics")
	float FOVOverride = 0.f;

	/** Magnification level (1.0 = no magnification). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optics")
	float MagnificationLevel = 1.f;

	/** Whether this mode is a night vision device. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optics")
	bool bIsNightVision = false;

	/** Whether this mode is a thermal imaging device. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optics")
	bool bIsThermal = false;

	/** NVG brightness amplification gain. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optics|NVG", meta = (EditCondition = "bIsNightVision"))
	float NVGGain = 1.f;

	/** Temperature differential (Celsius) needed for thermal to register a target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optics|Thermal", meta = (EditCondition = "bIsThermal"))
	float ThermalSensitivity = 2.f;

	/** Post-process material applied when this optic is active (green tint, thermal palette, etc.). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optics")
	TSoftObjectPtr<UMaterialInterface> PostProcessMaterial;

	/** Depth of field reduction factor [0..1]. 1 = full depth perception loss (NVG limitation). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DepthOfFieldReduction = 0.f;

	/** Image noise / grain factor [0..1]. NVG intensifier tube noise. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float NoiseFactor = 0.f;

	/** Battery drain rate in percent-per-second. 0 = no battery required. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optics", meta = (ClampMin = "0.0"))
	float BatteryDrainRate = 0.f;

	/** Seconds required to power on / activate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optics", meta = (ClampMin = "0.0"))
	float ActivationDelay = 0.f;

	/** NVG circular vignette (tube distortion) strength [0..1]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Optics|NVG", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TubeDistortionStrength = 0.f;
};

// ─────────────────────────────────────────────────────────────
//  Delegates
// ─────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnOpticsActivated,
	ESHOpticsMode, Mode);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnOpticsDeactivated,
	ESHOpticsMode, PreviousMode);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBatteryDepleted);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnFlashBlindness,
	float, Duration);

// ─────────────────────────────────────────────────────────────
//  Component
// ─────────────────────────────────────────────────────────────

/**
 * USHOpticsSystem
 *
 * Actor component that manages all optical enhancement devices for a
 * combatant character: night vision goggles (Gen 3 / Gen 4), thermal
 * imaging (FLIR / weapon-mounted), and weapon scopes with variable
 * magnification.
 *
 * Design principles (from the GDD):
 *  - Night: mutual blindness without NVGs; NVGs limit FOV and depth perception
 *  - Thermal: 500m+ detection range, affected by ambient temperature
 *  - All detection is physics-based; no gamified awareness meters
 *
 * Features:
 *  - Post-process material injection (green tint, tube vignette, thermal palette)
 *  - Battery management with recharge when powered off
 *  - NVG blooming / whiteout from bright light sources (explosions, flares)
 *  - NVG strobe vulnerability (enemy IR strobes can blind NVG users)
 *  - Scope magnification via FOV manipulation with breath sway amplification
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHOpticsSystem : public UActorComponent
{
	GENERATED_BODY()

public:
	USHOpticsSystem();

	// ------------------------------------------------------------------
	//  UActorComponent overrides
	// ------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ------------------------------------------------------------------
	//  Activation / mode selection
	// ------------------------------------------------------------------

	/** Power on the specified optic mode. Respects activation delay. */
	UFUNCTION(BlueprintCallable, Category = "SH|Optics")
	void ActivateOptics(ESHOpticsMode Mode);

	/** Power off the currently active optic. */
	UFUNCTION(BlueprintCallable, Category = "SH|Optics")
	void DeactivateOptics();

	/** Cycle through available optics modes in order. */
	UFUNCTION(BlueprintCallable, Category = "SH|Optics")
	void CycleOpticsMode();

	/** Switch directly to a specific optics mode. Deactivates first if already active. */
	UFUNCTION(BlueprintCallable, Category = "SH|Optics")
	void SetOpticsMode(ESHOpticsMode Mode);

	// ------------------------------------------------------------------
	//  Queries
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|Optics")
	ESHOpticsMode GetActiveMode() const { return ActiveMode; }

	UFUNCTION(BlueprintPure, Category = "SH|Optics")
	bool IsOpticsActive() const { return bIsActive; }

	/** Battery level normalized [0..1]. 1.0 = fully charged. */
	UFUNCTION(BlueprintPure, Category = "SH|Optics")
	float GetBatteryLevel() const { return BatteryLevel; }

	/** Returns the thermal detection multiplier for the current mode. */
	UFUNCTION(BlueprintPure, Category = "SH|Optics")
	float GetThermalDetectionBonus() const;

	/** Returns the current NVG brightness amplification gain. */
	UFUNCTION(BlueprintPure, Category = "SH|Optics")
	float GetNVGBrightnessGain() const;

	/** True if the owner is currently using NVGs and is vulnerable to IR strobe attacks. */
	UFUNCTION(BlueprintPure, Category = "SH|Optics")
	bool IsVulnerableToStrobe() const;

	/** Returns true if the optic is currently powering on (activation delay in progress). */
	UFUNCTION(BlueprintPure, Category = "SH|Optics")
	bool IsActivating() const { return bIsActivating; }

	/** Returns the current effective FOV (with optics override applied). */
	UFUNCTION(BlueprintPure, Category = "SH|Optics")
	float GetCurrentFOV() const;

	/** Returns the current magnification level. */
	UFUNCTION(BlueprintPure, Category = "SH|Optics")
	float GetCurrentMagnification() const;

	/** Returns true if flash blindness is currently active. */
	UFUNCTION(BlueprintPure, Category = "SH|Optics")
	bool IsFlashBlind() const { return FlashBlindnessRemaining > 0.f; }

	/** Returns the breath sway multiplier (amplified at high magnification). */
	UFUNCTION(BlueprintPure, Category = "SH|Optics")
	float GetBreathSwayMultiplier() const;

	// ------------------------------------------------------------------
	//  External events
	// ------------------------------------------------------------------

	/**
	 * Apply temporary flash blindness (bright light exposure while NVG active).
	 * @param Duration	Seconds of whiteout effect.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Optics")
	void ApplyFlashBlindness(float Duration);

	/**
	 * Notify the system of ambient light level change (for NVG blooming).
	 * @param LightIntensity	Normalized light intensity [0..1]. High values trigger blooming.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Optics")
	void NotifyAmbientLightChange(float LightIntensity);

	/**
	 * Set the current ambient temperature in Celsius (affects thermal sensitivity).
	 * Higher ambient temperatures reduce the thermal differential, making detection harder.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Optics")
	void SetAmbientTemperature(float TemperatureCelsius);

	/**
	 * Add or remove an optics mode from the available set.
	 * Only available modes can be cycled through.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Optics")
	void SetModeAvailable(ESHOpticsMode Mode, bool bAvailable);

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Optics")
	FOnOpticsActivated OnOpticsActivated;

	UPROPERTY(BlueprintAssignable, Category = "SH|Optics")
	FOnOpticsDeactivated OnOpticsDeactivated;

	UPROPERTY(BlueprintAssignable, Category = "SH|Optics")
	FOnBatteryDepleted OnBatteryDepleted;

	UPROPERTY(BlueprintAssignable, Category = "SH|Optics")
	FOnFlashBlindness OnFlashBlindness;

protected:
	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Map of optics mode to its configuration. Populated with defaults in BeginPlay if empty. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Optics|Config")
	TMap<ESHOpticsMode, FSHOpticsConfig> OpticsConfigs;

	/** Default camera FOV to restore when optics are deactivated (degrees). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Optics|Config")
	float DefaultFOV = 90.f;

	/** Battery recharge rate (percent-per-second) when optics are off. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Optics|Config")
	float BatteryRechargeRate = 2.f;

	/** Light intensity threshold above which NVG blooming begins [0..1]. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Optics|Config")
	float NVGBloomThreshold = 0.6f;

	/** Duration (seconds) of automatic whiteout when NVG bloom exceeds threshold. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Optics|Config")
	float NVGBloomWhiteoutDuration = 1.5f;

	/** Base thermal detection multiplier at ideal conditions. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Optics|Config")
	float BaseThermalDetectionMultiplier = 3.f;

	/**
	 * Ambient temperature (Celsius) above which thermal detection is degraded.
	 * As ambient temperature approaches body temperature, thermal contrast decreases.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Optics|Config")
	float ThermalDegradationTemp = 30.f;

	/** Body temperature reference for thermal contrast calculation (Celsius). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Optics|Config")
	float BodyTemperatureRef = 37.f;

private:
	// ------------------------------------------------------------------
	//  Internal helpers
	// ------------------------------------------------------------------
	void InitDefaultConfigs();
	void InitDefaultAvailableModes();

	/** Apply post-process material and FOV override for the given mode. */
	void ApplyOpticsEffects(ESHOpticsMode Mode);

	/** Remove post-process material and restore default FOV. */
	void RemoveOpticsEffects();

	/** Find the camera component on the owning actor. */
	UCameraComponent* FindOwnerCamera() const;

	/** Tick battery drain / recharge. */
	void TickBattery(float DeltaTime);

	/** Tick flash blindness recovery. */
	void TickFlashBlindness(float DeltaTime);

	/** Tick activation delay. */
	void TickActivation(float DeltaTime);

	/** Compute the next available mode after the current one. */
	ESHOpticsMode GetNextAvailableMode() const;

	/** Get the config for a specific mode. Returns nullptr if not configured. */
	const FSHOpticsConfig* GetConfigForMode(ESHOpticsMode Mode) const;

	/** Compute the thermal degradation factor based on ambient temperature [0..1]. */
	float ComputeThermalDegradation() const;

	// ------------------------------------------------------------------
	//  Runtime state
	// ------------------------------------------------------------------

	/** Currently active optics mode. */
	UPROPERTY()
	ESHOpticsMode ActiveMode = ESHOpticsMode::None;

	/** Mode being activated (during activation delay). */
	UPROPERTY()
	ESHOpticsMode PendingMode = ESHOpticsMode::None;

	/** Whether optics are currently active and fully powered on. */
	bool bIsActive = false;

	/** Whether we are in the activation delay period. */
	bool bIsActivating = false;

	/** Elapsed activation time (counts toward ActivationDelay). */
	float ActivationElapsed = 0.f;

	/** Battery level [0..1]. */
	float BatteryLevel = 1.f;

	/** Remaining flash blindness duration in seconds. */
	float FlashBlindnessRemaining = 0.f;

	/** Current ambient temperature (Celsius) for thermal calculations. */
	float AmbientTemperature = 20.f;

	/** Current ambient light intensity [0..1] for NVG blooming. */
	float CurrentAmbientLight = 0.f;

	/** Dynamic material instance for the active post-process effect. */
	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> ActivePostProcessMID;

	/** Cached reference to the owner's camera post-process component (created on demand). */
	UPROPERTY()
	TObjectPtr<UPostProcessComponent> OwnedPostProcessComponent;

	/** Set of optics modes available to this character. */
	UPROPERTY()
	TSet<ESHOpticsMode> AvailableModes;

	/** Cached default FOV from the camera at BeginPlay. */
	float CachedDefaultFOV = 90.f;
};
