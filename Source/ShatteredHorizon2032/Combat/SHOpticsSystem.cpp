// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHOpticsSystem.h"
#include "Camera/CameraComponent.h"
#include "Components/PostProcessComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/Character.h"

// =====================================================================
//  Constructor
// =====================================================================

USHOpticsSystem::USHOpticsSystem()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

// =====================================================================
//  UActorComponent overrides
// =====================================================================

void USHOpticsSystem::BeginPlay()
{
	Super::BeginPlay();

	InitDefaultConfigs();
	InitDefaultAvailableModes();

	BatteryLevel = 1.f;
	bIsActive = false;
	bIsActivating = false;
	ActiveMode = ESHOpticsMode::None;

	// Cache the camera's default FOV.
	if (UCameraComponent* Camera = FindOwnerCamera())
	{
		CachedDefaultFOV = Camera->FieldOfView;
		DefaultFOV = CachedDefaultFOV;
	}
}

void USHOpticsSystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TickActivation(DeltaTime);
	TickBattery(DeltaTime);
	TickFlashBlindness(DeltaTime);

	// Update dynamic post-process parameters if active.
	if (bIsActive && ActivePostProcessMID)
	{
		const FSHOpticsConfig* Config = GetConfigForMode(ActiveMode);
		if (Config)
		{
			// Update noise factor (can fluctuate with battery level).
			const float EffectiveNoise = Config->NoiseFactor * FMath::Lerp(1.2f, 1.f, BatteryLevel);
			ActivePostProcessMID->SetScalarParameterValue(FName("NoiseFactor"), EffectiveNoise);

			// Update flash blindness intensity on the material.
			const float BlindIntensity = FlashBlindnessRemaining > 0.f ? 1.f : 0.f;
			ActivePostProcessMID->SetScalarParameterValue(FName("FlashBlindnessIntensity"), BlindIntensity);

			// NVG: update gain (dims as battery depletes).
			if (Config->bIsNightVision)
			{
				const float EffectiveGain = Config->NVGGain * FMath::Lerp(0.3f, 1.f, BatteryLevel);
				ActivePostProcessMID->SetScalarParameterValue(FName("NVGGain"), EffectiveGain);
				ActivePostProcessMID->SetScalarParameterValue(FName("TubeDistortion"), Config->TubeDistortionStrength);
			}

			// Thermal: update sensitivity based on ambient temperature.
			if (Config->bIsThermal)
			{
				const float Degradation = ComputeThermalDegradation();
				const float EffectiveSensitivity = Config->ThermalSensitivity * Degradation;
				ActivePostProcessMID->SetScalarParameterValue(FName("ThermalSensitivity"), EffectiveSensitivity);
			}
		}
	}
}

// =====================================================================
//  Default configuration
// =====================================================================

void USHOpticsSystem::InitDefaultConfigs()
{
	if (OpticsConfigs.Num() > 0)
	{
		return;
	}

	// -- Gen 3 NVG --
	{
		FSHOpticsConfig Config;
		Config.FOVOverride = 40.f;
		Config.MagnificationLevel = 1.f;
		Config.bIsNightVision = true;
		Config.bIsThermal = false;
		Config.NVGGain = 3.5f;
		Config.ThermalSensitivity = 0.f;
		Config.DepthOfFieldReduction = 0.6f;
		Config.NoiseFactor = 0.35f;
		Config.BatteryDrainRate = 1.5f;		// ~67 seconds of use on a full charge
		Config.ActivationDelay = 0.8f;
		Config.TubeDistortionStrength = 0.7f;
		OpticsConfigs.Add(ESHOpticsMode::NVG_Gen3, Config);
	}

	// -- Gen 4 NVG (filmless / gated — better performance, less noise) --
	{
		FSHOpticsConfig Config;
		Config.FOVOverride = 40.f;
		Config.MagnificationLevel = 1.f;
		Config.bIsNightVision = true;
		Config.bIsThermal = false;
		Config.NVGGain = 5.f;
		Config.ThermalSensitivity = 0.f;
		Config.DepthOfFieldReduction = 0.4f;
		Config.NoiseFactor = 0.15f;
		Config.BatteryDrainRate = 2.f;		// Higher drain for better performance
		Config.ActivationDelay = 0.5f;
		Config.TubeDistortionStrength = 0.5f;
		OpticsConfigs.Add(ESHOpticsMode::NVG_Gen4, Config);
	}

	// -- Thermal FLIR (standalone goggle / vehicle-mounted) --
	{
		FSHOpticsConfig Config;
		Config.FOVOverride = 36.f;
		Config.MagnificationLevel = 1.f;
		Config.bIsNightVision = false;
		Config.bIsThermal = true;
		Config.NVGGain = 0.f;
		Config.ThermalSensitivity = 1.5f;	// Celsius differential
		Config.DepthOfFieldReduction = 0.3f;
		Config.NoiseFactor = 0.1f;
		Config.BatteryDrainRate = 2.5f;		// Thermal requires more power
		Config.ActivationDelay = 1.2f;		// Longer boot time for thermal sensor calibration
		Config.TubeDistortionStrength = 0.f;
		OpticsConfigs.Add(ESHOpticsMode::Thermal_FLIR, Config);
	}

	// -- Thermal Weapon Sight --
	{
		FSHOpticsConfig Config;
		Config.FOVOverride = 24.f;
		Config.MagnificationLevel = 2.f;
		Config.bIsNightVision = false;
		Config.bIsThermal = true;
		Config.NVGGain = 0.f;
		Config.ThermalSensitivity = 2.0f;
		Config.DepthOfFieldReduction = 0.2f;
		Config.NoiseFactor = 0.08f;
		Config.BatteryDrainRate = 3.f;
		Config.ActivationDelay = 1.5f;
		Config.TubeDistortionStrength = 0.f;
		OpticsConfigs.Add(ESHOpticsMode::Thermal_Weapon, Config);
	}

	// -- Scope 1x (red dot / holographic) --
	{
		FSHOpticsConfig Config;
		Config.FOVOverride = 0.f;			// No FOV change for 1x
		Config.MagnificationLevel = 1.f;
		Config.bIsNightVision = false;
		Config.bIsThermal = false;
		Config.NVGGain = 0.f;
		Config.ThermalSensitivity = 0.f;
		Config.DepthOfFieldReduction = 0.f;
		Config.NoiseFactor = 0.f;
		Config.BatteryDrainRate = 0.f;		// Red dot uses negligible power
		Config.ActivationDelay = 0.1f;
		Config.TubeDistortionStrength = 0.f;
		OpticsConfigs.Add(ESHOpticsMode::Scope_1x, Config);
	}

	// -- Scope 4x (ACOG-style) --
	{
		FSHOpticsConfig Config;
		Config.FOVOverride = 22.5f;			// 90 / 4
		Config.MagnificationLevel = 4.f;
		Config.bIsNightVision = false;
		Config.bIsThermal = false;
		Config.NVGGain = 0.f;
		Config.ThermalSensitivity = 0.f;
		Config.DepthOfFieldReduction = 0.f;
		Config.NoiseFactor = 0.f;
		Config.BatteryDrainRate = 0.f;
		Config.ActivationDelay = 0.15f;
		Config.TubeDistortionStrength = 0.f;
		OpticsConfigs.Add(ESHOpticsMode::Scope_4x, Config);
	}

	// -- Scope 8x (medium-range sniper) --
	{
		FSHOpticsConfig Config;
		Config.FOVOverride = 11.25f;		// 90 / 8
		Config.MagnificationLevel = 8.f;
		Config.bIsNightVision = false;
		Config.bIsThermal = false;
		Config.NVGGain = 0.f;
		Config.ThermalSensitivity = 0.f;
		Config.DepthOfFieldReduction = 0.f;
		Config.NoiseFactor = 0.f;
		Config.BatteryDrainRate = 0.f;
		Config.ActivationDelay = 0.2f;
		Config.TubeDistortionStrength = 0.f;
		OpticsConfigs.Add(ESHOpticsMode::Scope_8x, Config);
	}

	// -- Scope 12x (long-range sniper) --
	{
		FSHOpticsConfig Config;
		Config.FOVOverride = 7.5f;			// 90 / 12
		Config.MagnificationLevel = 12.f;
		Config.bIsNightVision = false;
		Config.bIsThermal = false;
		Config.NVGGain = 0.f;
		Config.ThermalSensitivity = 0.f;
		Config.DepthOfFieldReduction = 0.f;
		Config.NoiseFactor = 0.f;
		Config.BatteryDrainRate = 0.f;
		Config.ActivationDelay = 0.25f;
		Config.TubeDistortionStrength = 0.f;
		OpticsConfigs.Add(ESHOpticsMode::Scope_12x, Config);
	}
}

void USHOpticsSystem::InitDefaultAvailableModes()
{
	if (AvailableModes.Num() > 0)
	{
		return;
	}

	// By default, all configured modes are available.
	for (const auto& Pair : OpticsConfigs)
	{
		AvailableModes.Add(Pair.Key);
	}
}

// =====================================================================
//  Activation / mode selection
// =====================================================================

void USHOpticsSystem::ActivateOptics(ESHOpticsMode Mode)
{
	if (Mode == ESHOpticsMode::None)
	{
		DeactivateOptics();
		return;
	}

	if (!AvailableModes.Contains(Mode))
	{
		return;
	}

	const FSHOpticsConfig* Config = GetConfigForMode(Mode);
	if (!Config)
	{
		return;
	}

	// If already active with the same mode, do nothing.
	if (bIsActive && ActiveMode == Mode)
	{
		return;
	}

	// If already active with a different mode, deactivate first.
	if (bIsActive)
	{
		RemoveOpticsEffects();
		bIsActive = false;
	}

	// Check battery for powered optics.
	if (Config->BatteryDrainRate > 0.f && BatteryLevel <= 0.f)
	{
		OnBatteryDepleted.Broadcast();
		return;
	}

	// Start activation delay.
	if (Config->ActivationDelay > KINDA_SMALL_NUMBER)
	{
		bIsActivating = true;
		ActivationElapsed = 0.f;
		PendingMode = Mode;
	}
	else
	{
		// Instant activation.
		ActiveMode = Mode;
		bIsActive = true;
		bIsActivating = false;
		ApplyOpticsEffects(Mode);
		OnOpticsActivated.Broadcast(Mode);
	}
}

void USHOpticsSystem::DeactivateOptics()
{
	// Cancel pending activation.
	if (bIsActivating)
	{
		bIsActivating = false;
		PendingMode = ESHOpticsMode::None;
		ActivationElapsed = 0.f;
	}

	if (!bIsActive)
	{
		return;
	}

	const ESHOpticsMode PreviousMode = ActiveMode;

	RemoveOpticsEffects();
	bIsActive = false;
	ActiveMode = ESHOpticsMode::None;

	// Clear flash blindness when deactivating NVG.
	FlashBlindnessRemaining = 0.f;

	OnOpticsDeactivated.Broadcast(PreviousMode);
}

void USHOpticsSystem::CycleOpticsMode()
{
	const ESHOpticsMode NextMode = GetNextAvailableMode();

	if (NextMode == ESHOpticsMode::None)
	{
		if (bIsActive)
		{
			DeactivateOptics();
		}
		return;
	}

	if (bIsActive || bIsActivating)
	{
		DeactivateOptics();
	}

	ActivateOptics(NextMode);
}

void USHOpticsSystem::SetOpticsMode(ESHOpticsMode Mode)
{
	if (Mode == ESHOpticsMode::None)
	{
		DeactivateOptics();
		return;
	}

	if (bIsActive || bIsActivating)
	{
		DeactivateOptics();
	}

	ActivateOptics(Mode);
}

// =====================================================================
//  Queries
// =====================================================================

float USHOpticsSystem::GetThermalDetectionBonus() const
{
	if (!bIsActive)
	{
		return 1.f;
	}

	const FSHOpticsConfig* Config = GetConfigForMode(ActiveMode);
	if (!Config || !Config->bIsThermal)
	{
		return 1.f;
	}

	// Base multiplier degraded by ambient temperature proximity to body temp.
	const float Degradation = ComputeThermalDegradation();
	return BaseThermalDetectionMultiplier * Degradation;
}

float USHOpticsSystem::GetNVGBrightnessGain() const
{
	if (!bIsActive)
	{
		return 0.f;
	}

	const FSHOpticsConfig* Config = GetConfigForMode(ActiveMode);
	if (!Config || !Config->bIsNightVision)
	{
		return 0.f;
	}

	// Gain diminishes as battery depletes.
	return Config->NVGGain * FMath::Lerp(0.3f, 1.f, BatteryLevel);
}

bool USHOpticsSystem::IsVulnerableToStrobe() const
{
	if (!bIsActive)
	{
		return false;
	}

	const FSHOpticsConfig* Config = GetConfigForMode(ActiveMode);
	return Config && Config->bIsNightVision;
}

float USHOpticsSystem::GetCurrentFOV() const
{
	if (!bIsActive)
	{
		return DefaultFOV;
	}

	const FSHOpticsConfig* Config = GetConfigForMode(ActiveMode);
	if (!Config || Config->FOVOverride <= 0.f)
	{
		return DefaultFOV;
	}

	return Config->FOVOverride;
}

float USHOpticsSystem::GetCurrentMagnification() const
{
	if (!bIsActive)
	{
		return 1.f;
	}

	const FSHOpticsConfig* Config = GetConfigForMode(ActiveMode);
	if (!Config)
	{
		return 1.f;
	}

	return Config->MagnificationLevel;
}

float USHOpticsSystem::GetBreathSwayMultiplier() const
{
	if (!bIsActive)
	{
		return 1.f;
	}

	const FSHOpticsConfig* Config = GetConfigForMode(ActiveMode);
	if (!Config)
	{
		return 1.f;
	}

	// Breath sway is amplified proportional to magnification.
	// At 1x, multiplier is 1.0. At 12x, multiplier is ~3.5.
	// Formula: 1 + (mag - 1) * 0.227 — gives roughly 1.0 at 1x, 3.5 at 12x.
	return 1.f + (Config->MagnificationLevel - 1.f) * 0.227f;
}

// =====================================================================
//  External events
// =====================================================================

void USHOpticsSystem::ApplyFlashBlindness(float Duration)
{
	if (Duration <= 0.f)
	{
		return;
	}

	// Flash blindness is most severe when NVG is active (gain amplifies the flash).
	float EffectiveDuration = Duration;

	if (bIsActive)
	{
		const FSHOpticsConfig* Config = GetConfigForMode(ActiveMode);
		if (Config && Config->bIsNightVision)
		{
			// NVG amplifies the flash duration based on gain.
			EffectiveDuration *= FMath::Lerp(1.f, 2.5f, FMath::GetMappedRangeValueClamped(
				FVector2D(1.f, 6.f), FVector2D(0.f, 1.f), Config->NVGGain));
		}
	}

	// Stack with existing blindness (take the longer remaining).
	FlashBlindnessRemaining = FMath::Max(FlashBlindnessRemaining, EffectiveDuration);

	OnFlashBlindness.Broadcast(EffectiveDuration);
}

void USHOpticsSystem::NotifyAmbientLightChange(float LightIntensity)
{
	CurrentAmbientLight = FMath::Clamp(LightIntensity, 0.f, 1.f);

	// Check for NVG blooming / automatic whiteout.
	if (bIsActive && CurrentAmbientLight > NVGBloomThreshold)
	{
		const FSHOpticsConfig* Config = GetConfigForMode(ActiveMode);
		if (Config && Config->bIsNightVision)
		{
			// Intensity above threshold determines whiteout severity.
			const float ExcessLight = (CurrentAmbientLight - NVGBloomThreshold) / (1.f - NVGBloomThreshold);
			const float WhiteoutDuration = NVGBloomWhiteoutDuration * ExcessLight;
			ApplyFlashBlindness(WhiteoutDuration);
		}
	}
}

void USHOpticsSystem::SetAmbientTemperature(float TemperatureCelsius)
{
	AmbientTemperature = TemperatureCelsius;
}

void USHOpticsSystem::SetModeAvailable(ESHOpticsMode Mode, bool bAvailable)
{
	if (Mode == ESHOpticsMode::None)
	{
		return;
	}

	if (bAvailable)
	{
		AvailableModes.Add(Mode);
	}
	else
	{
		AvailableModes.Remove(Mode);

		// If the currently active mode was removed, deactivate.
		if (bIsActive && ActiveMode == Mode)
		{
			DeactivateOptics();
		}
		if (bIsActivating && PendingMode == Mode)
		{
			bIsActivating = false;
			PendingMode = ESHOpticsMode::None;
		}
	}
}

// =====================================================================
//  Internal: post-process effects
// =====================================================================

void USHOpticsSystem::ApplyOpticsEffects(ESHOpticsMode Mode)
{
	const FSHOpticsConfig* Config = GetConfigForMode(Mode);
	if (!Config)
	{
		return;
	}

	// Apply FOV override.
	if (Config->FOVOverride > 0.f)
	{
		if (UCameraComponent* Camera = FindOwnerCamera())
		{
			Camera->SetFieldOfView(Config->FOVOverride);
		}
	}

	// Create or update post-process component and material.
	UMaterialInterface* BaseMaterial = Config->PostProcessMaterial.LoadSynchronous();
	if (BaseMaterial)
	{
		// Create the post-process component if we don't have one yet.
		if (!OwnedPostProcessComponent)
		{
			AActor* Owner = GetOwner();
			if (Owner)
			{
				OwnedPostProcessComponent = NewObject<UPostProcessComponent>(Owner, FName("SHOpticsPostProcess"));
				OwnedPostProcessComponent->SetupAttachment(Owner->GetRootComponent());
				OwnedPostProcessComponent->RegisterComponent();
				OwnedPostProcessComponent->bUnbound = true;
				OwnedPostProcessComponent->Priority = 10.f;
			}
		}

		if (OwnedPostProcessComponent)
		{
			// Create a dynamic material instance so we can drive parameters at runtime.
			ActivePostProcessMID = UMaterialInstanceDynamic::Create(BaseMaterial, this);
			if (ActivePostProcessMID)
			{
				// Set initial parameters.
				ActivePostProcessMID->SetScalarParameterValue(FName("NoiseFactor"), Config->NoiseFactor);
				ActivePostProcessMID->SetScalarParameterValue(FName("DepthOfFieldReduction"), Config->DepthOfFieldReduction);
				ActivePostProcessMID->SetScalarParameterValue(FName("FlashBlindnessIntensity"), 0.f);

				if (Config->bIsNightVision)
				{
					ActivePostProcessMID->SetScalarParameterValue(FName("NVGGain"), Config->NVGGain);
					ActivePostProcessMID->SetScalarParameterValue(FName("TubeDistortion"), Config->TubeDistortionStrength);
				}

				if (Config->bIsThermal)
				{
					const float Degradation = ComputeThermalDegradation();
					ActivePostProcessMID->SetScalarParameterValue(FName("ThermalSensitivity"), Config->ThermalSensitivity * Degradation);
				}

				// Add the blendable to the post-process settings.
				OwnedPostProcessComponent->Settings.WeightedBlendables.Array.Empty();
				OwnedPostProcessComponent->Settings.WeightedBlendables.Array.Add(
					FWeightedBlendable(1.f, ActivePostProcessMID));
				OwnedPostProcessComponent->SetActive(true);
			}
		}
	}
}

void USHOpticsSystem::RemoveOpticsEffects()
{
	// Restore camera FOV.
	if (UCameraComponent* Camera = FindOwnerCamera())
	{
		Camera->SetFieldOfView(DefaultFOV);
	}

	// Remove post-process effects.
	if (OwnedPostProcessComponent)
	{
		OwnedPostProcessComponent->Settings.WeightedBlendables.Array.Empty();
		OwnedPostProcessComponent->SetActive(false);
	}

	ActivePostProcessMID = nullptr;
}

UCameraComponent* USHOpticsSystem::FindOwnerCamera() const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return nullptr;
	}

	// Prefer a camera component directly on the owner.
	UCameraComponent* Camera = Owner->FindComponentByClass<UCameraComponent>();
	return Camera;
}

// =====================================================================
//  Internal: tick helpers
// =====================================================================

void USHOpticsSystem::TickBattery(float DeltaTime)
{
	if (bIsActive)
	{
		const FSHOpticsConfig* Config = GetConfigForMode(ActiveMode);
		if (Config && Config->BatteryDrainRate > 0.f)
		{
			BatteryLevel -= Config->BatteryDrainRate * DeltaTime * 0.01f; // Convert percent-per-second to [0..1]

			if (BatteryLevel <= 0.f)
			{
				BatteryLevel = 0.f;
				DeactivateOptics();
				OnBatteryDepleted.Broadcast();
			}
		}
	}
	else if (!bIsActivating)
	{
		// Recharge battery when optics are off.
		if (BatteryLevel < 1.f)
		{
			BatteryLevel = FMath::Min(BatteryLevel + BatteryRechargeRate * DeltaTime * 0.01f, 1.f);
		}
	}
}

void USHOpticsSystem::TickFlashBlindness(float DeltaTime)
{
	if (FlashBlindnessRemaining > 0.f)
	{
		FlashBlindnessRemaining = FMath::Max(FlashBlindnessRemaining - DeltaTime, 0.f);
	}
}

void USHOpticsSystem::TickActivation(float DeltaTime)
{
	if (!bIsActivating)
	{
		return;
	}

	const FSHOpticsConfig* Config = GetConfigForMode(PendingMode);
	if (!Config)
	{
		bIsActivating = false;
		PendingMode = ESHOpticsMode::None;
		return;
	}

	ActivationElapsed += DeltaTime;

	if (ActivationElapsed >= Config->ActivationDelay)
	{
		// Activation complete.
		bIsActivating = false;
		ActiveMode = PendingMode;
		PendingMode = ESHOpticsMode::None;
		bIsActive = true;
		ActivationElapsed = 0.f;

		ApplyOpticsEffects(ActiveMode);
		OnOpticsActivated.Broadcast(ActiveMode);
	}
}

ESHOpticsMode USHOpticsSystem::GetNextAvailableMode() const
{
	// Define the cycling order (matches enum order).
	static const ESHOpticsMode CycleOrder[] =
	{
		ESHOpticsMode::NVG_Gen3,
		ESHOpticsMode::NVG_Gen4,
		ESHOpticsMode::Thermal_FLIR,
		ESHOpticsMode::Thermal_Weapon,
		ESHOpticsMode::Scope_1x,
		ESHOpticsMode::Scope_4x,
		ESHOpticsMode::Scope_8x,
		ESHOpticsMode::Scope_12x
	};
	static const int32 CycleCount = UE_ARRAY_COUNT(CycleOrder);

	if (AvailableModes.Num() == 0)
	{
		return ESHOpticsMode::None;
	}

	// Find the current mode's position in the cycle order.
	const ESHOpticsMode CurrentMode = bIsActive ? ActiveMode : (bIsActivating ? PendingMode : ESHOpticsMode::None);
	int32 StartIndex = -1;

	for (int32 i = 0; i < CycleCount; ++i)
	{
		if (CycleOrder[i] == CurrentMode)
		{
			StartIndex = i;
			break;
		}
	}

	// Search forward from the next position, wrapping around.
	for (int32 Offset = 1; Offset <= CycleCount; ++Offset)
	{
		const int32 Index = (StartIndex + Offset) % CycleCount;
		const ESHOpticsMode Candidate = CycleOrder[Index];
		if (AvailableModes.Contains(Candidate))
		{
			return Candidate;
		}
	}

	return ESHOpticsMode::None;
}

const FSHOpticsConfig* USHOpticsSystem::GetConfigForMode(ESHOpticsMode Mode) const
{
	return OpticsConfigs.Find(Mode);
}

float USHOpticsSystem::ComputeThermalDegradation() const
{
	// As ambient temperature approaches body temperature, thermal contrast decreases.
	// At 20C ambient: full effectiveness (degradation = 1.0).
	// At 37C ambient: minimal effectiveness (degradation approaches 0.2).
	const float TempDifferential = FMath::Abs(BodyTemperatureRef - AmbientTemperature);
	const float MaxDifferential = BodyTemperatureRef - ThermalDegradationTemp; // Typically 7 degrees

	if (TempDifferential >= BodyTemperatureRef - ThermalDegradationTemp + 10.f)
	{
		// Well below degradation threshold — full effectiveness.
		return 1.f;
	}

	if (AmbientTemperature >= BodyTemperatureRef)
	{
		// Ambient at or above body temp — near-minimum effectiveness.
		return 0.2f;
	}

	if (AmbientTemperature <= ThermalDegradationTemp)
	{
		// Below the degradation onset — full effectiveness.
		return 1.f;
	}

	// Linear interpolation between full and degraded in the transition zone.
	const float Alpha = (AmbientTemperature - ThermalDegradationTemp) / FMath::Max(BodyTemperatureRef - ThermalDegradationTemp, 1.f);
	return FMath::Lerp(1.f, 0.2f, Alpha);
}
