// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHAudioSystem.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundSubmix.h"
#include "Sound/SoundEffectSubmix.h"
#include "AudioDevice.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"

// =====================================================================
//  Subsystem lifecycle
// =====================================================================

void USHAudioSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ActiveEvents.Reserve(MaxTrackedEvents);
	BindToWorldTick();
}

void USHAudioSystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (TickDelegateHandle.IsValid())
		{
			World->OnWorldPreActorTick.Remove(TickDelegateHandle);
			TickDelegateHandle.Reset();
		}
	}

	// Remove any applied submix effects.
	if (bExclusionEffectApplied && WorldAudioSubmix && ExclusionFilterPreset)
	{
		WorldAudioSubmix->RemoveSubmixEffect(ExclusionFilterPreset);
		bExclusionEffectApplied = false;
	}

	ActiveEvents.Empty();
	Super::Deinitialize();
}

void USHAudioSystem::BindToWorldTick()
{
	if (UWorld* World = GetWorld())
	{
		TickDelegateHandle = World->OnWorldPreActorTick.AddLambda(
			[this](UWorld* /*World*/, ELevelTick /*TickType*/, float DeltaSeconds)
			{
				Tick(DeltaSeconds);
			});
	}
}

// =====================================================================
//  Tick
// =====================================================================

void USHAudioSystem::Tick(float DeltaSeconds)
{
	TickCombatEvents(DeltaSeconds);
	TickCombatIntensity(DeltaSeconds);
	TickAuditoryExclusion(DeltaSeconds);
	TickMixBusLevels(DeltaSeconds);
}

// =====================================================================
//  Combat event registration
// =====================================================================

void USHAudioSystem::RegisterCombatEvent(FVector Location, float Intensity)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FSHCombatAudioEvent NewEvent;
	NewEvent.Location = Location;
	NewEvent.Intensity = FMath::Clamp(Intensity, 0.f, 1.f);
	NewEvent.Timestamp = World->GetTimeSeconds();

	// If we're at capacity, replace the oldest event.
	if (ActiveEvents.Num() >= MaxTrackedEvents)
	{
		// Find the oldest event.
		int32 OldestIndex = 0;
		float OldestTime = ActiveEvents[0].Timestamp;
		for (int32 i = 1; i < ActiveEvents.Num(); ++i)
		{
			if (ActiveEvents[i].Timestamp < OldestTime)
			{
				OldestTime = ActiveEvents[i].Timestamp;
				OldestIndex = i;
			}
		}
		ActiveEvents[OldestIndex] = NewEvent;
	}
	else
	{
		ActiveEvents.Add(NewEvent);
	}
}

// =====================================================================
//  Stress / Auditory exclusion
// =====================================================================

void USHAudioSystem::SetStressLevel(float Level)
{
	CurrentStressLevel = FMath::Clamp(Level, 0.f, 1.f);
}

// =====================================================================
//  Doppler / Supersonic
// =====================================================================

float USHAudioSystem::ComputeDopplerPitchShift(
	const FVector& ProjectilePosition,
	const FVector& ProjectileVelocity,
	const FVector& ListenerPosition)
{
	// Direction from projectile to listener.
	const FVector ToListener = ListenerPosition - ProjectilePosition;
	const float Distance = ToListener.Size();

	if (Distance < KINDA_SMALL_NUMBER)
	{
		return 1.f;
	}

	const FVector DirectionToListener = ToListener / Distance;

	// Radial velocity component: positive = approaching, negative = receding.
	// We use the source (projectile) velocity projected onto the direction to the listener.
	const float SourceRadialVelocity = FVector::DotProduct(ProjectileVelocity, DirectionToListener);

	// Classic Doppler formula: f_observed = f_source * (v_sound) / (v_sound - v_source_radial)
	// v_source_radial is positive when approaching the listener.
	const float Denominator = SpeedOfSoundCmS - SourceRadialVelocity;

	// Clamp to avoid division by zero or near-zero (Mach cone singularity).
	const float ClampedDenominator = FMath::Max(Denominator, SpeedOfSoundCmS * 0.05f);

	return SpeedOfSoundCmS / ClampedDenominator;
}

void USHAudioSystem::PlaySupersonicCrackWithDoppler(
	USoundBase* CrackSound,
	const FVector& ProjectilePosition,
	const FVector& ProjectileVelocity,
	const FVector& ListenerPosition)
{
	if (!CrackSound)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float PitchShift = ComputeDopplerPitchShift(
		ProjectilePosition, ProjectileVelocity, ListenerPosition);

	// Play at the projectile's position with the computed Doppler pitch.
	UGameplayStatics::PlaySoundAtLocation(
		World,
		CrackSound,
		ProjectilePosition,
		1.f,           // Volume
		PitchShift,    // Pitch
		0.f,           // Start time
		nullptr,       // Attenuation
		nullptr);      // Concurrency
}

// =====================================================================
//  Internal — Combat event maintenance
// =====================================================================

void USHAudioSystem::TickCombatEvents(float DeltaSeconds)
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float CurrentTime = World->GetTimeSeconds();

	// Remove expired events.
	for (int32 i = ActiveEvents.Num() - 1; i >= 0; --i)
	{
		const float Age = CurrentTime - ActiveEvents[i].Timestamp;
		if (Age > EventDecayTime)
		{
			ActiveEvents.RemoveAtSwap(i);
		}
	}

	// Count gunfire and explosion events.
	ActiveGunfireCount = 0;
	ActiveExplosionCount = 0;

	for (const FSHCombatAudioEvent& Event : ActiveEvents)
	{
		if (Event.Intensity >= ExplosionIntensityThreshold)
		{
			++ActiveExplosionCount;
		}
		else if (Event.Intensity >= GunfireIntensityThreshold)
		{
			++ActiveGunfireCount;
		}
	}
}

// =====================================================================
//  Internal — Combat intensity computation
// =====================================================================

void USHAudioSystem::TickCombatIntensity(float DeltaSeconds)
{
	RawCombatIntensity = ComputeRawIntensity();

	// Smooth the intensity transition.
	const float PreviousIntensity = CurrentCombatIntensity;
	CurrentCombatIntensity = FMath::FInterpTo(
		CurrentCombatIntensity, RawCombatIntensity,
		DeltaSeconds, IntensitySmoothingSpeed);

	CurrentCombatIntensity = FMath::Clamp(CurrentCombatIntensity, 0.f, 1.f);

	// Broadcast if changed meaningfully.
	if (FMath::Abs(CurrentCombatIntensity - PreviousIntensity) > 0.01f)
	{
		OnCombatIntensityChanged.Broadcast(CurrentCombatIntensity);
	}
}

float USHAudioSystem::ComputeRawIntensity() const
{
	const UWorld* World = GetWorld();
	if (!World || ActiveEvents.Num() == 0)
	{
		return 0.f;
	}

	const FVector ListenerPos = GetListenerPosition();
	const float CurrentTime = World->GetTimeSeconds();
	float TotalWeightedIntensity = 0.f;

	for (const FSHCombatAudioEvent& Event : ActiveEvents)
	{
		// Time-based decay: linear falloff over EventDecayTime.
		const float Age = CurrentTime - Event.Timestamp;
		const float TimeFactor = FMath::Clamp(1.f - (Age / EventDecayTime), 0.f, 1.f);

		// Distance-based attenuation.
		const float Distance = FVector::Dist(ListenerPos, Event.Location);
		float DistanceFactor;
		if (Distance <= FullWeightRadius)
		{
			DistanceFactor = 1.f;
		}
		else if (Distance >= MaxEventRadius)
		{
			DistanceFactor = 0.f;
		}
		else
		{
			DistanceFactor = 1.f - ((Distance - FullWeightRadius) / (MaxEventRadius - FullWeightRadius));
		}

		TotalWeightedIntensity += Event.Intensity * TimeFactor * DistanceFactor;
	}

	// Normalize: with many small events, sum can exceed 1. Use a soft saturation curve.
	// tanh-style: 1 - exp(-x) for a natural compression curve.
	const float Saturated = 1.f - FMath::Exp(-TotalWeightedIntensity * 2.f);
	return FMath::Clamp(Saturated, 0.f, 1.f);
}

FVector USHAudioSystem::GetListenerPosition() const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return FVector::ZeroVector;
	}

	const APlayerController* PC = World->GetFirstPlayerController();
	if (!PC)
	{
		return FVector::ZeroVector;
	}

	if (const APlayerCameraManager* CameraManager = PC->PlayerCameraManager)
	{
		return CameraManager->GetCameraLocation();
	}

	if (const APawn* PlayerPawn = PC->GetPawn())
	{
		return PlayerPawn->GetActorLocation();
	}

	return FVector::ZeroVector;
}

// =====================================================================
//  Internal — Auditory exclusion
// =====================================================================

void USHAudioSystem::TickAuditoryExclusion(float DeltaSeconds)
{
	// Determine target exclusion based on stress and combat intensity.
	// Auditory exclusion triggers when stress exceeds threshold,
	// and its severity is modulated by combat intensity.
	const bool bShouldBeActive = CurrentStressLevel >= StressThreshold;

	float TargetExclusion = 0.f;
	if (bShouldBeActive)
	{
		// Scale by how far above the threshold we are.
		const float StressExcess = (CurrentStressLevel - StressThreshold) / (1.f - StressThreshold);
		// Combine with combat intensity for a more severe effect during firefights.
		TargetExclusion = FMath::Clamp(StressExcess * (0.5f + 0.5f * CurrentCombatIntensity), 0.f, 1.f);
	}

	// Ramp up/down the exclusion strength.
	if (TargetExclusion > ExclusionStrength)
	{
		const float RampRate = (ExclusionRampUpTime > KINDA_SMALL_NUMBER) ? (1.f / ExclusionRampUpTime) : 100.f;
		ExclusionStrength = FMath::Min(ExclusionStrength + RampRate * DeltaSeconds, TargetExclusion);
	}
	else
	{
		const float RampRate = (ExclusionRampDownTime > KINDA_SMALL_NUMBER) ? (1.f / ExclusionRampDownTime) : 100.f;
		ExclusionStrength = FMath::Max(ExclusionStrength - RampRate * DeltaSeconds, TargetExclusion);
	}

	bAuditoryExclusionActive = ExclusionStrength > KINDA_SMALL_NUMBER;

	// Apply or remove the submix effect.
	if (WorldAudioSubmix && ExclusionFilterPreset)
	{
		if (bAuditoryExclusionActive && !bExclusionEffectApplied)
		{
			WorldAudioSubmix->AddSubmixEffect(ExclusionFilterPreset);
			bExclusionEffectApplied = true;
		}
		else if (!bAuditoryExclusionActive && bExclusionEffectApplied)
		{
			WorldAudioSubmix->RemoveSubmixEffect(ExclusionFilterPreset);
			bExclusionEffectApplied = false;
		}
	}

	// Broadcast state changes.
	if (bAuditoryExclusionActive != bWasExclusionActive)
	{
		OnAuditoryExclusionChanged.Broadcast(bAuditoryExclusionActive);
		bWasExclusionActive = bAuditoryExclusionActive;
	}
}

// =====================================================================
//  Internal — Mix bus level management
// =====================================================================

void USHAudioSystem::TickMixBusLevels(float DeltaSeconds)
{
	// Target bus offsets driven by combat intensity.
	// At intensity 0: world bus at 0 dB, combat bus at 0 dB.
	// At intensity 1: world bus attenuated, combat bus boosted.
	const float TargetWorldOffset = CurrentCombatIntensity * MaxWorldBusAttenuation;
	const float TargetCombatOffset = CurrentCombatIntensity * MaxCombatBusBoost;

	// Additional world bus attenuation during auditory exclusion.
	const float ExclusionAttenuation = ExclusionStrength * -12.f; // Up to -12dB during full exclusion.
	const float FinalWorldTarget = TargetWorldOffset + ExclusionAttenuation;

	// Smoothly interpolate.
	CurrentWorldBusOffset = FMath::FInterpTo(CurrentWorldBusOffset, FinalWorldTarget, DeltaSeconds, 5.f);
	CurrentCombatBusOffset = FMath::FInterpTo(CurrentCombatBusOffset, TargetCombatOffset, DeltaSeconds, 5.f);

	// Convert dB offsets to linear gain and apply to submixes.
	if (WorldAudioSubmix)
	{
		const float WorldGain = FMath::Pow(10.f, CurrentWorldBusOffset / 20.f);
		WorldAudioSubmix->SetSubmixOutputVolume(WorldGain);
	}

	if (CombatAudioSubmix)
	{
		const float CombatGain = FMath::Pow(10.f, CurrentCombatBusOffset / 20.f);
		CombatAudioSubmix->SetSubmixOutputVolume(CombatGain);
	}

	// Dialogue bus: slightly ducked at high intensity but never fully suppressed.
	if (DialogueAudioSubmix)
	{
		const float DialogueDuck = CurrentCombatIntensity * -3.f; // Max -3dB.
		const float DialogueGain = FMath::Pow(10.f, DialogueDuck / 20.f);
		DialogueAudioSubmix->SetSubmixOutputVolume(DialogueGain);
	}
}
