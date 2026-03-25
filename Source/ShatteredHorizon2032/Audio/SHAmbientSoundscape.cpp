// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHAmbientSoundscape.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "Curves/CurveFloat.h"
#include "GameFramework/Actor.h"

// =====================================================================
//  Constructor
// =====================================================================

USHAmbientSoundscape::USHAmbientSoundscape()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;
}

// =====================================================================
//  UActorComponent overrides
// =====================================================================

void USHAmbientSoundscape::BeginPlay()
{
	Super::BeginPlay();

	// Clamp layer count.
	if (AmbientLayers.Num() > MaxAmbientLayers)
	{
		UE_LOG(LogTemp, Warning, TEXT("USHAmbientSoundscape: %d layers configured, max is %d. Truncating."),
			AmbientLayers.Num(), MaxAmbientLayers);
		AmbientLayers.SetNum(MaxAmbientLayers);
	}

	// Initialize all layers.
	for (FSHAmbientLayer& Layer : AmbientLayers)
	{
		Layer.CurrentVolume = 0.f;
		Layer.VolumeTarget = 0.f;
		Layer.bIsPlaying = false;
		Layer.AudioComponent = nullptr;
	}
}

void USHAmbientSoundscape::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	StopAllLayers();
	Super::EndPlay(EndPlayReason);
}

void USHAmbientSoundscape::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	for (FSHAmbientLayer& Layer : AmbientLayers)
	{
		// Compute the target volume for this layer.
		Layer.VolumeTarget = ComputeLayerTargetVolume(Layer);

		// Crossfade toward target.
		const float PreviousVolume = Layer.CurrentVolume;
		if (FMath::Abs(Layer.CurrentVolume - Layer.VolumeTarget) > KINDA_SMALL_NUMBER)
		{
			Layer.CurrentVolume = FMath::FInterpConstantTo(
				Layer.CurrentVolume, Layer.VolumeTarget,
				DeltaTime, Layer.CrossfadeSpeed);
		}

		// Apply global multiplier.
		const float FinalVolume = Layer.CurrentVolume * GlobalVolumeMultiplier;

		// Start/stop playback based on volume.
		if (FinalVolume > MinActiveVolume)
		{
			if (!Layer.bIsPlaying)
			{
				StartLayerPlayback(Layer);
			}

			if (Layer.AudioComponent)
			{
				Layer.AudioComponent->SetVolumeMultiplier(FinalVolume);
			}
		}
		else
		{
			if (Layer.bIsPlaying)
			{
				StopLayerPlayback(Layer);
			}
		}

		// Broadcast significant volume changes.
		if (FMath::Abs(Layer.CurrentVolume - PreviousVolume) > 0.01f)
		{
			OnAmbientLayerChanged.Broadcast(Layer.LayerName, Layer.CurrentVolume);
		}
	}
}

// =====================================================================
//  External input
// =====================================================================

void USHAmbientSoundscape::SetTimeOfDay(float NormalizedTime)
{
	CurrentTimeOfDay = FMath::Fmod(FMath::Max(NormalizedTime, 0.f), 1.f);
}

void USHAmbientSoundscape::SetTimeOfDayHours(float Hour)
{
	const float ClampedHour = FMath::Fmod(FMath::Max(Hour, 0.f), 24.f);
	CurrentTimeOfDay = ClampedHour / 24.f;
}

void USHAmbientSoundscape::SetRainIntensity(float Intensity)
{
	CurrentRainIntensity = FMath::Clamp(Intensity, 0.f, 1.f);
}

void USHAmbientSoundscape::SetWindIntensity(float Intensity)
{
	CurrentWindIntensity = FMath::Clamp(Intensity, 0.f, 1.f);
}

void USHAmbientSoundscape::SetCombatProximityIntensity(float Intensity)
{
	CurrentCombatIntensity = FMath::Clamp(Intensity, 0.f, 1.f);
}

// =====================================================================
//  Layer management
// =====================================================================

void USHAmbientSoundscape::ActivateLayer(FName LayerName)
{
	FSHAmbientLayer* Layer = FindLayerByName(LayerName);
	if (Layer && !Layer->bIsPlaying)
	{
		StartLayerPlayback(*Layer);
	}
}

void USHAmbientSoundscape::DeactivateLayer(FName LayerName)
{
	FSHAmbientLayer* Layer = FindLayerByName(LayerName);
	if (Layer)
	{
		// Set target to 0 — the tick loop will crossfade out and stop.
		Layer->VolumeTarget = 0.f;
	}
}

void USHAmbientSoundscape::StopAllLayers()
{
	for (FSHAmbientLayer& Layer : AmbientLayers)
	{
		StopLayerPlayback(Layer);
	}
}

// =====================================================================
//  Queries
// =====================================================================

int32 USHAmbientSoundscape::GetActiveLayerCount() const
{
	int32 Count = 0;
	for (const FSHAmbientLayer& Layer : AmbientLayers)
	{
		if (Layer.bIsPlaying)
		{
			++Count;
		}
	}
	return Count;
}

// =====================================================================
//  Internal — Volume computation
// =====================================================================

float USHAmbientSoundscape::ComputeLayerTargetVolume(const FSHAmbientLayer& Layer) const
{
	if (!Layer.Sound)
	{
		return 0.f;
	}

	float Volume = Layer.BaseVolume;

	// Time-of-day gating.
	if (!IsLayerActiveForTimeOfDay(Layer))
	{
		return 0.f;
	}

	// Time-of-day curve modulation.
	Volume *= GetTimeOfDayMultiplier(Layer);

	// Weather modulation.
	if (Layer.bWeatherDriven)
	{
		// Combine rain and wind into a weather factor.
		// Rain layers respond primarily to rain; wind layers can be set up separately.
		const float WeatherFactor = FMath::Max(CurrentRainIntensity, CurrentWindIntensity * 0.5f);
		Volume *= WeatherFactor;
	}

	// Combat proximity modulation.
	if (Layer.bCombatProximityDriven)
	{
		Volume *= CurrentCombatIntensity;
	}

	return FMath::Clamp(Volume, 0.f, 1.f);
}

bool USHAmbientSoundscape::IsLayerActiveForTimeOfDay(const FSHAmbientLayer& Layer) const
{
	// If both are zero, the layer is always active.
	if (FMath::IsNearlyZero(Layer.ActiveTimeRange.X) && FMath::IsNearlyZero(Layer.ActiveTimeRange.Y))
	{
		return true;
	}

	const float CurrentHour = CurrentTimeOfDay * 24.f;
	const float StartHour = Layer.ActiveTimeRange.X;
	const float EndHour = Layer.ActiveTimeRange.Y;

	// Handle ranges that wrap around midnight (e.g., 22:00 to 04:00).
	if (StartHour <= EndHour)
	{
		return CurrentHour >= StartHour && CurrentHour <= EndHour;
	}
	else
	{
		return CurrentHour >= StartHour || CurrentHour <= EndHour;
	}
}

float USHAmbientSoundscape::GetTimeOfDayMultiplier(const FSHAmbientLayer& Layer) const
{
	if (!Layer.TimeOfDayVolumeCurve)
	{
		return 1.f;
	}

	// Evaluate the curve with the normalized time of day.
	const float CurveValue = Layer.TimeOfDayVolumeCurve->GetFloatValue(CurrentTimeOfDay);
	return FMath::Clamp(CurveValue, 0.f, 1.f);
}

// =====================================================================
//  Internal — Audio component management
// =====================================================================

void USHAmbientSoundscape::EnsureAudioComponent(FSHAmbientLayer& Layer)
{
	if (Layer.AudioComponent)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner || !Layer.Sound)
	{
		return;
	}

	UAudioComponent* NewComp = NewObject<UAudioComponent>(Owner);
	if (!NewComp)
	{
		return;
	}

	NewComp->SetSound(Layer.Sound);
	NewComp->bAutoActivate = false;
	NewComp->bAutoDestroy = false;
	NewComp->bIsUISound = false;
	NewComp->bAllowSpatialization = false; // Ambient layers are non-spatialized (2D).
	NewComp->SetVolumeMultiplier(0.f);
	NewComp->RegisterComponentWithWorld(GetWorld());
	NewComp->AttachToComponent(Owner->GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);

	Layer.AudioComponent = NewComp;
}

void USHAmbientSoundscape::StartLayerPlayback(FSHAmbientLayer& Layer)
{
	if (Layer.bIsPlaying)
	{
		return;
	}

	EnsureAudioComponent(Layer);

	if (Layer.AudioComponent && Layer.Sound)
	{
		Layer.AudioComponent->SetSound(Layer.Sound);
		Layer.AudioComponent->SetVolumeMultiplier(Layer.CurrentVolume * GlobalVolumeMultiplier);
		Layer.AudioComponent->Play();
		Layer.bIsPlaying = true;
	}
}

void USHAmbientSoundscape::StopLayerPlayback(FSHAmbientLayer& Layer)
{
	if (!Layer.bIsPlaying)
	{
		return;
	}

	if (Layer.AudioComponent)
	{
		Layer.AudioComponent->Stop();
	}

	Layer.bIsPlaying = false;
	Layer.CurrentVolume = 0.f;
}

FSHAmbientLayer* USHAmbientSoundscape::FindLayerByName(FName LayerName)
{
	for (FSHAmbientLayer& Layer : AmbientLayers)
	{
		if (Layer.LayerName == LayerName)
		{
			return &Layer;
		}
	}
	return nullptr;
}
