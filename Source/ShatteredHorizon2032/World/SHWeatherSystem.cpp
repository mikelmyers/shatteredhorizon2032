// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHWeatherSystem.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "../Core/SHGameMode.h"
#include "../Weapons/SHBallisticsSystem.h"
#include "../Audio/SHAudioSystem.h"

DEFINE_LOG_CATEGORY(LogSH_Weather);

// ======================================================================
//  Construction
// ======================================================================

USHWeatherSystem::USHWeatherSystem()
{
}

// ======================================================================
//  USubsystem interface
// ======================================================================

void USHWeatherSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Ensure dependent subsystems are initialized first.
	Collection.InitializeDependency<USHBallisticsSystem>();

	// Set sensible defaults — clear day, midday.
	CurrentState = BuildDefaultsForType(ESHWeatherType::Clear);
	CurrentState.TimeOfDay = ESHTimeOfDay::Midday;
	PreviousTimeOfDay = CurrentState.TimeOfDay;

	// Cache subsystem pointers.
	if (UWorld* World = GetWorld())
	{
		CachedBallisticsSystem = World->GetSubsystem<USHBallisticsSystem>();
		CachedAudioSystem = World->GetSubsystem<USHAudioSystem>();
	}

	ComputeGameplayEffects();

	UE_LOG(LogSH_Weather, Log, TEXT("WeatherSystem initialized. Default: Clear, Midday, %.0f°C"),
		CurrentState.Temperature);
}

void USHWeatherSystem::Deinitialize()
{
	CachedBallisticsSystem = nullptr;
	CachedAudioSystem = nullptr;

	UE_LOG(LogSH_Weather, Log, TEXT("WeatherSystem deinitialized."));
	Super::Deinitialize();
}

void USHWeatherSystem::Tick(float DeltaTime)
{
	// Step transition if active.
	if (bIsTransitioning)
	{
		TickWeatherTransition(DeltaTime);
	}

	// Update time of day from the game mode.
	TickTimeOfDay();

	// Recompute all gameplay effects from current state.
	ComputeGameplayEffects();

	// Feed data to downstream systems.
	FeedWindToBallistics();
	FeedSoundPropagationToAudio();
}

TStatId USHWeatherSystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USHWeatherSystem, STATGROUP_Tickables);
}

// ======================================================================
//  Weather control
// ======================================================================

void USHWeatherSystem::SetWeather(ESHWeatherType NewWeatherType)
{
	const ESHWeatherType OldType = CurrentState.WeatherType;

	if (bIsTransitioning)
	{
		// Cancel any in-progress transition.
		bIsTransitioning = false;
		TransitionElapsed = 0.0f;
	}

	// Preserve current time of day and then apply type defaults.
	const ESHTimeOfDay SavedTimeOfDay = CurrentState.TimeOfDay;
	CurrentState = BuildDefaultsForType(NewWeatherType);
	CurrentState.TimeOfDay = SavedTimeOfDay;

	ComputeGameplayEffects();
	FeedWindToBallistics();

	if (OldType != NewWeatherType)
	{
		OnWeatherChanged.Broadcast(OldType, NewWeatherType);

		UE_LOG(LogSH_Weather, Log, TEXT("Weather changed: %d -> %d | Vis=%.0fm Precip=%.2f Wind=%.1fm/s Temp=%.1f°C"),
			static_cast<int32>(OldType), static_cast<int32>(NewWeatherType),
			CurrentState.Visibility, CurrentState.Precipitation,
			CurrentState.WindSpeedMPS, CurrentState.Temperature);
	}
}

void USHWeatherSystem::SetWindState(FVector Direction, float SpeedMPS)
{
	CurrentState.WindDirection = Direction.GetSafeNormal();
	CurrentState.WindSpeedMPS = FMath::Max(0.0f, SpeedMPS);
}

void USHWeatherSystem::SetTemperature(float Celsius)
{
	CurrentState.Temperature = Celsius;
}

void USHWeatherSystem::TransitionWeather(ESHWeatherType Target, float DurationSeconds)
{
	if (DurationSeconds <= 0.0f)
	{
		// Instant transition.
		SetWeather(Target);
		return;
	}

	TransitionStartState = CurrentState;
	TransitionTargetState = BuildDefaultsForType(Target);
	// Preserve the time of day in the target — it comes from game mode, not weather type.
	TransitionTargetState.TimeOfDay = CurrentState.TimeOfDay;
	TransitionTargetType = Target;
	TransitionDuration = DurationSeconds;
	TransitionElapsed = 0.0f;
	bIsTransitioning = true;

	UE_LOG(LogSH_Weather, Log, TEXT("Weather transition started: %d -> %d over %.1fs"),
		static_cast<int32>(CurrentState.WeatherType), static_cast<int32>(Target), DurationSeconds);
}

// ======================================================================
//  Queries
// ======================================================================

float USHWeatherSystem::GetTransitionProgress() const
{
	if (!bIsTransitioning || TransitionDuration <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp(TransitionElapsed / TransitionDuration, 0.0f, 1.0f);
}

// ======================================================================
//  Weather presets
// ======================================================================

void USHWeatherSystem::CreatePreset_TaipeiSummer()
{
	SetWeather(ESHWeatherType::Clear);
	SetTemperature(34.0f);
	SetWindState(FVector(0.3f, 0.9f, 0.0f), 2.5f);
	CurrentState.CloudCover = 0.2f;
	CurrentState.Visibility = 8000.0f;

	UE_LOG(LogSH_Weather, Log, TEXT("Preset applied: Taipei Summer"));
}

void USHWeatherSystem::CreatePreset_TyphoonApproach()
{
	SetWeather(ESHWeatherType::Storm);
	SetTemperature(24.0f);
	SetWindState(FVector(-0.7f, 0.7f, 0.0f), 28.0f);
	CurrentState.CloudCover = 1.0f;
	CurrentState.Precipitation = 0.9f;
	CurrentState.Visibility = 200.0f;

	UE_LOG(LogSH_Weather, Log, TEXT("Preset applied: Typhoon Approach"));
}

void USHWeatherSystem::CreatePreset_NightAssault()
{
	SetWeather(ESHWeatherType::Overcast);
	SetTemperature(22.0f);
	SetWindState(FVector(0.5f, -0.5f, 0.0f), 6.0f);
	CurrentState.CloudCover = 0.85f;
	CurrentState.Visibility = 3000.0f;
	// Time of day is driven by game mode, but we ensure the state reflects night.
	CurrentState.TimeOfDay = ESHTimeOfDay::Night;

	UE_LOG(LogSH_Weather, Log, TEXT("Preset applied: Night Assault"));
}

// ======================================================================
//  Internal tick helpers
// ======================================================================

void USHWeatherSystem::TickTimeOfDay()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Read time of day from the game mode.
	AGameModeBase* GameModeBase = World->GetAuthGameMode();
	ASHGameMode* GameMode = Cast<ASHGameMode>(GameModeBase);
	if (GameMode)
	{
		const float Hours = GameMode->GetTimeOfDayHours();
		const ESHTimeOfDay NewTimeOfDay = HoursToTimeOfDay(Hours);

		CurrentState.TimeOfDay = NewTimeOfDay;

		if (NewTimeOfDay != PreviousTimeOfDay)
		{
			OnTimeOfDayChanged.Broadcast(PreviousTimeOfDay, NewTimeOfDay);

			UE_LOG(LogSH_Weather, Log, TEXT("Time of day changed: %d -> %d (%.1fh)"),
				static_cast<int32>(PreviousTimeOfDay), static_cast<int32>(NewTimeOfDay), Hours);

			PreviousTimeOfDay = NewTimeOfDay;
		}
	}
}

void USHWeatherSystem::TickWeatherTransition(float DeltaTime)
{
	TransitionElapsed += DeltaTime;

	const float Alpha = FMath::Clamp(TransitionElapsed / TransitionDuration, 0.0f, 1.0f);

	// Use smooth-step for natural-feeling transitions.
	const float SmoothedAlpha = FMath::SmoothStep(0.0f, 1.0f, Alpha);

	// Preserve the time of day (game-mode driven, not interpolated).
	const ESHTimeOfDay SavedTimeOfDay = CurrentState.TimeOfDay;
	const ESHWeatherType OldType = CurrentState.WeatherType;

	CurrentState = LerpWeatherState(TransitionStartState, TransitionTargetState, SmoothedAlpha);
	CurrentState.TimeOfDay = SavedTimeOfDay;

	if (Alpha >= 1.0f)
	{
		// Transition complete.
		bIsTransitioning = false;
		CurrentState.WeatherType = TransitionTargetType;

		if (OldType != TransitionTargetType)
		{
			OnWeatherChanged.Broadcast(TransitionStartState.WeatherType, TransitionTargetType);
		}

		UE_LOG(LogSH_Weather, Log, TEXT("Weather transition complete: -> %d"),
			static_cast<int32>(TransitionTargetType));
	}
	else
	{
		// During transition, set weather type to the target once past halfway
		// so downstream systems react to the dominant condition.
		CurrentState.WeatherType = (SmoothedAlpha >= 0.5f) ? TransitionTargetType : TransitionStartState.WeatherType;
	}
}

// ======================================================================
//  Gameplay effect computation
// ======================================================================

void USHWeatherSystem::ComputeGameplayEffects()
{
	CurrentEffects.VisualDetectionMultiplier = ComputeVisualDetectionMultiplier();
	CurrentEffects.AcousticDetectionMultiplier = ComputeAcousticDetectionMultiplier();
	CurrentEffects.FineMotorPenalty = ComputeFineMotorPenalty();
	CurrentEffects.EquipmentReliabilityMultiplier = ComputeEquipmentReliabilityMultiplier();
	CurrentEffects.MovementSpeedMultiplier = ComputeMovementSpeedMultiplier();
	CurrentEffects.ThermalDetectionMultiplier = ComputeThermalDetectionMultiplier();
	CurrentEffects.SoundPropagationMultiplier = ComputeSoundPropagationMultiplier();

	// Ballistic wind force: direction * speed, in world space.
	CurrentEffects.BallisticWindForce = CurrentState.WindDirection.GetSafeNormal() * CurrentState.WindSpeedMPS;

	// NVG required at night.
	CurrentEffects.NVGRequired = (CurrentState.TimeOfDay == ESHTimeOfDay::Night);
}

float USHWeatherSystem::ComputeVisualDetectionMultiplier() const
{
	// Start at 1.0 and reduce based on conditions.
	float Multiplier = 1.0f;

	// Visibility-based reduction: normalize against max visibility.
	if (MaxVisibilityMeters > MinVisibilityMeters)
	{
		const float VisNormalized = FMath::Clamp(
			(CurrentState.Visibility - MinVisibilityMeters) / (MaxVisibilityMeters - MinVisibilityMeters),
			0.0f, 1.0f);
		Multiplier *= VisNormalized;
	}

	// Precipitation further reduces visual detection.
	// Light rain: minor reduction. Heavy rain: significant.
	Multiplier *= FMath::Lerp(1.0f, 0.4f, CurrentState.Precipitation);

	// Time of day reduction.
	switch (CurrentState.TimeOfDay)
	{
	case ESHTimeOfDay::Night:
		Multiplier *= 0.1f; // Near-blind without NVGs.
		break;
	case ESHTimeOfDay::Dusk:
	case ESHTimeOfDay::Dawn:
		Multiplier *= 0.5f; // Transitional lighting.
		break;
	default:
		break; // Full daylight — no additional penalty.
	}

	return FMath::Clamp(Multiplier, 0.0f, 1.0f);
}

float USHWeatherSystem::ComputeAcousticDetectionMultiplier() const
{
	float Multiplier = 1.0f;

	// Rain masks sound signatures.
	// Light rain: slight masking. Heavy rain: significant masking.
	Multiplier *= FMath::Lerp(1.0f, 0.35f, CurrentState.Precipitation);

	// Wind masks sound at lower speeds, but at high speeds carries it
	// in the downwind direction. For the scalar multiplier we model
	// the average case: moderate wind reduces general acoustic detection.
	if (CurrentState.WindSpeedMPS > 1.0f)
	{
		// Wind above 1 m/s starts masking. Above 15 m/s, strong masking.
		const float WindFactor = FMath::Clamp(CurrentState.WindSpeedMPS / 15.0f, 0.0f, 1.0f);
		Multiplier *= FMath::Lerp(1.0f, 0.5f, WindFactor);
	}

	// Storm conditions stack additional acoustic masking.
	if (CurrentState.WeatherType == ESHWeatherType::Storm)
	{
		Multiplier *= 0.6f;
	}

	return FMath::Clamp(Multiplier, 0.0f, 1.0f);
}

float USHWeatherSystem::ComputeFineMotorPenalty() const
{
	// Fine motor degradation from cold.
	// No penalty above ColdThresholdCelsius.
	// Maximum penalty at or below ExtremeColdCelsius.
	if (CurrentState.Temperature >= ColdThresholdCelsius)
	{
		return 0.0f;
	}

	if (CurrentState.Temperature <= ExtremeColdCelsius)
	{
		return 1.0f;
	}

	const float Range = ColdThresholdCelsius - ExtremeColdCelsius;
	if (Range <= 0.0f)
	{
		return 0.0f;
	}

	const float Alpha = (ColdThresholdCelsius - CurrentState.Temperature) / Range;

	// Use a curve so degradation accelerates as it gets colder.
	return FMath::Clamp(FMath::InterpEaseIn(0.0f, 1.0f, Alpha, 1.8f), 0.0f, 1.0f);
}

float USHWeatherSystem::ComputeEquipmentReliabilityMultiplier() const
{
	float Reliability = 1.0f;

	// Cold reduces reliability (mechanisms stiffen, lubricant thickens).
	if (CurrentState.Temperature < ColdThresholdCelsius)
	{
		const float ColdPenalty = ComputeFineMotorPenalty();
		// Up to 40% reliability reduction at extreme cold.
		Reliability *= FMath::Lerp(1.0f, 0.6f, ColdPenalty);
	}

	// Wet conditions reduce reliability (water ingress, corrosion risk).
	// Precipitation above 0.3 starts affecting equipment.
	if (CurrentState.Precipitation > 0.3f)
	{
		const float WetFactor = FMath::Clamp((CurrentState.Precipitation - 0.3f) / 0.7f, 0.0f, 1.0f);
		// Up to 25% reliability reduction in heavy rain.
		Reliability *= FMath::Lerp(1.0f, 0.75f, WetFactor);
	}

	// Extreme heat can also degrade equipment.
	if (CurrentState.Temperature > 45.0f)
	{
		const float HeatFactor = FMath::Clamp((CurrentState.Temperature - 45.0f) / 15.0f, 0.0f, 1.0f);
		Reliability *= FMath::Lerp(1.0f, 0.85f, HeatFactor);
	}

	return FMath::Clamp(Reliability, 0.0f, 1.0f);
}

float USHWeatherSystem::ComputeMovementSpeedMultiplier() const
{
	float SpeedMult = 1.0f;

	// Mud from sustained rain. Precipitation above threshold creates mud.
	if (CurrentState.Precipitation > MudPrecipitationThreshold)
	{
		const float MudFactor = FMath::Clamp(
			(CurrentState.Precipitation - MudPrecipitationThreshold) / (1.0f - MudPrecipitationThreshold),
			0.0f, 1.0f);
		SpeedMult *= (1.0f - MaxMudMovementPenalty * MudFactor);
	}

	// Snow reduces movement speed.
	if (CurrentState.WeatherType == ESHWeatherType::Snow)
	{
		SpeedMult *= 0.8f;
	}

	// Extreme wind impedes movement at very high speeds (> 20 m/s).
	if (CurrentState.WindSpeedMPS > 20.0f)
	{
		const float WindPenalty = FMath::Clamp((CurrentState.WindSpeedMPS - 20.0f) / 30.0f, 0.0f, 1.0f);
		SpeedMult *= FMath::Lerp(1.0f, 0.7f, WindPenalty);
	}

	return FMath::Clamp(SpeedMult, 0.0f, 1.0f);
}

float USHWeatherSystem::ComputeThermalDetectionMultiplier() const
{
	// Thermal imaging is more effective when ambient temperature is lower,
	// because the temperature differential between a warm body and the
	// environment is greater.
	//
	// At 35°C+ ambient, thermal is least effective (bodies blend in).
	// At 0°C and below, thermal is extremely effective.

	constexpr float HighTempCelsius = 35.0f;
	constexpr float LowTempCelsius = 0.0f;

	if (CurrentState.Temperature >= HighTempCelsius)
	{
		return 0.4f; // Reduced effectiveness in extreme heat.
	}

	if (CurrentState.Temperature <= LowTempCelsius)
	{
		return 1.0f; // Maximum thermal contrast.
	}

	// Linear interpolation between low and high temp ranges.
	const float Alpha = (HighTempCelsius - CurrentState.Temperature) / (HighTempCelsius - LowTempCelsius);
	return FMath::Lerp(0.4f, 1.0f, Alpha);
}

float USHWeatherSystem::ComputeSoundPropagationMultiplier() const
{
	float Propagation = 1.0f;

	// Rain absorbs and scatters sound energy.
	Propagation *= FMath::Lerp(1.0f, 0.5f, CurrentState.Precipitation);

	// Wind effects on propagation: high wind creates turbulent mixing
	// that disrupts clean sound propagation over distance.
	if (CurrentState.WindSpeedMPS > 3.0f)
	{
		const float WindFactor = FMath::Clamp((CurrentState.WindSpeedMPS - 3.0f) / 20.0f, 0.0f, 1.0f);
		Propagation *= FMath::Lerp(1.0f, 0.55f, WindFactor);
	}

	// Fog slightly dampens sound propagation (moisture absorption).
	if (CurrentState.WeatherType == ESHWeatherType::Fog || CurrentState.WeatherType == ESHWeatherType::DenseFog)
	{
		Propagation *= 0.85f;
	}

	// Cold air is denser and conducts sound better at short range,
	// but temperature inversions can bend sound. Net effect: slight boost.
	if (CurrentState.Temperature < 0.0f)
	{
		Propagation *= 1.1f;
	}

	return FMath::Clamp(Propagation, 0.0f, 1.0f);
}

// ======================================================================
//  Downstream system feeds
// ======================================================================

void USHWeatherSystem::FeedWindToBallistics()
{
	if (!CachedBallisticsSystem)
	{
		if (UWorld* World = GetWorld())
		{
			CachedBallisticsSystem = World->GetSubsystem<USHBallisticsSystem>();
		}
	}

	if (CachedBallisticsSystem)
	{
		FSHWindState Wind;
		Wind.Direction = CurrentState.WindDirection.GetSafeNormal();
		Wind.SpeedMPS = CurrentState.WindSpeedMPS;

		// Gust variation scales with overall wind speed: calm winds have
		// low variation, storm winds gust heavily.
		Wind.GustVariation = FMath::Clamp(CurrentState.WindSpeedMPS / 30.0f, 0.05f, 0.5f);

		CachedBallisticsSystem->SetWind(Wind);
	}
}

void USHWeatherSystem::FeedSoundPropagationToAudio()
{
	if (!CachedAudioSystem)
	{
		if (UWorld* World = GetWorld())
		{
			CachedAudioSystem = World->GetSubsystem<USHAudioSystem>();
		}
	}

	if (CachedAudioSystem)
	{
		// Weather affects combat audio perception: rain masks sounds, wind carries them.
		// Use the stress level interface to modulate audio when weather is severe —
		// heavy rain at propagation 0.6 means sounds are 40% quieter/shorter range.
		//
		// Additionally, register the weather itself as a persistent low-intensity
		// audio event so the mixing system accounts for environmental noise floor.
		const float PropMult = CurrentEffects.SoundPropagationMultiplier;
		if (PropMult < 0.9f)
		{
			// Weather is degrading sound propagation — register as ambient noise.
			// Higher degradation = more ambient noise masking.
			const float WeatherNoiseIntensity = (1.0f - PropMult) * 0.3f;
			CachedAudioSystem->RegisterCombatEvent(FVector::ZeroVector, WeatherNoiseIntensity);
		}
	}
}

// ======================================================================
//  Weather state utilities
// ======================================================================

FSHWeatherState USHWeatherSystem::BuildDefaultsForType(ESHWeatherType Type)
{
	FSHWeatherState State;
	State.WeatherType = Type;

	switch (Type)
	{
	case ESHWeatherType::Clear:
		State.WindDirection = FVector(1.0f, 0.0f, 0.0f);
		State.WindSpeedMPS = 2.0f;
		State.Temperature = 25.0f;
		State.Visibility = 10000.0f;
		State.Precipitation = 0.0f;
		State.CloudCover = 0.1f;
		break;

	case ESHWeatherType::Overcast:
		State.WindDirection = FVector(0.7f, 0.7f, 0.0f);
		State.WindSpeedMPS = 4.0f;
		State.Temperature = 20.0f;
		State.Visibility = 6000.0f;
		State.Precipitation = 0.0f;
		State.CloudCover = 0.75f;
		break;

	case ESHWeatherType::LightRain:
		State.WindDirection = FVector(0.5f, 0.5f, 0.0f);
		State.WindSpeedMPS = 5.0f;
		State.Temperature = 18.0f;
		State.Visibility = 4000.0f;
		State.Precipitation = 0.3f;
		State.CloudCover = 0.85f;
		break;

	case ESHWeatherType::HeavyRain:
		State.WindDirection = FVector(0.3f, 0.7f, 0.0f);
		State.WindSpeedMPS = 10.0f;
		State.Temperature = 16.0f;
		State.Visibility = 1500.0f;
		State.Precipitation = 0.8f;
		State.CloudCover = 1.0f;
		break;

	case ESHWeatherType::Fog:
		State.WindDirection = FVector(1.0f, 0.0f, 0.0f);
		State.WindSpeedMPS = 1.0f;
		State.Temperature = 12.0f;
		State.Visibility = 500.0f;
		State.Precipitation = 0.0f;
		State.CloudCover = 0.5f;
		break;

	case ESHWeatherType::DenseFog:
		State.WindDirection = FVector(1.0f, 0.0f, 0.0f);
		State.WindSpeedMPS = 0.5f;
		State.Temperature = 10.0f;
		State.Visibility = 100.0f;
		State.Precipitation = 0.0f;
		State.CloudCover = 0.6f;
		break;

	case ESHWeatherType::Storm:
		State.WindDirection = FVector(-0.5f, 0.8f, 0.0f);
		State.WindSpeedMPS = 25.0f;
		State.Temperature = 18.0f;
		State.Visibility = 300.0f;
		State.Precipitation = 0.95f;
		State.CloudCover = 1.0f;
		break;

	case ESHWeatherType::Snow:
		State.WindDirection = FVector(0.6f, -0.3f, 0.0f);
		State.WindSpeedMPS = 6.0f;
		State.Temperature = -5.0f;
		State.Visibility = 800.0f;
		State.Precipitation = 0.5f;
		State.CloudCover = 0.9f;
		break;

	case ESHWeatherType::ExtremeHeat:
		State.WindDirection = FVector(0.0f, 1.0f, 0.0f);
		State.WindSpeedMPS = 1.5f;
		State.Temperature = 48.0f;
		State.Visibility = 7000.0f; // Heat haze reduces max clarity.
		State.Precipitation = 0.0f;
		State.CloudCover = 0.05f;
		break;
	}

	return State;
}

ESHTimeOfDay USHWeatherSystem::HoursToTimeOfDay(float Hours)
{
	// Wrap to 0-24 range.
	Hours = FMath::Fmod(Hours, 24.0f);
	if (Hours < 0.0f)
	{
		Hours += 24.0f;
	}

	// Dawn:      05:00 - 07:00
	// Morning:   07:00 - 11:00
	// Midday:    11:00 - 13:00
	// Afternoon: 13:00 - 17:00
	// Dusk:      17:00 - 19:30
	// Night:     19:30 - 05:00

	if (Hours >= 5.0f && Hours < 7.0f)
	{
		return ESHTimeOfDay::Dawn;
	}
	if (Hours >= 7.0f && Hours < 11.0f)
	{
		return ESHTimeOfDay::Morning;
	}
	if (Hours >= 11.0f && Hours < 13.0f)
	{
		return ESHTimeOfDay::Midday;
	}
	if (Hours >= 13.0f && Hours < 17.0f)
	{
		return ESHTimeOfDay::Afternoon;
	}
	if (Hours >= 17.0f && Hours < 19.5f)
	{
		return ESHTimeOfDay::Dusk;
	}

	return ESHTimeOfDay::Night;
}

FSHWeatherState USHWeatherSystem::LerpWeatherState(const FSHWeatherState& A, const FSHWeatherState& B, float Alpha)
{
	FSHWeatherState Result;

	// Weather type takes the source until halfway, then target.
	Result.WeatherType = (Alpha < 0.5f) ? A.WeatherType : B.WeatherType;

	// Interpolate wind direction via Slerp for smooth rotation.
	const FVector DirA = A.WindDirection.GetSafeNormal();
	const FVector DirB = B.WindDirection.GetSafeNormal();
	const FQuat QuatA = FQuat::FindBetweenNormals(FVector::ForwardVector, DirA);
	const FQuat QuatB = FQuat::FindBetweenNormals(FVector::ForwardVector, DirB);
	const FQuat QuatResult = FQuat::Slerp(QuatA, QuatB, Alpha);
	Result.WindDirection = QuatResult.RotateVector(FVector::ForwardVector);

	// Scalar interpolation for all other parameters.
	Result.WindSpeedMPS = FMath::Lerp(A.WindSpeedMPS, B.WindSpeedMPS, Alpha);
	Result.Temperature = FMath::Lerp(A.Temperature, B.Temperature, Alpha);
	Result.Visibility = FMath::Lerp(A.Visibility, B.Visibility, Alpha);
	Result.Precipitation = FMath::Lerp(A.Precipitation, B.Precipitation, Alpha);
	Result.CloudCover = FMath::Lerp(A.CloudCover, B.CloudCover, Alpha);

	// Time of day is not interpolated — it's driven by the game mode.
	Result.TimeOfDay = (Alpha < 0.5f) ? A.TimeOfDay : B.TimeOfDay;

	return Result;
}
