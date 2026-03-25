// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "Combat/SHCombatStressSystem.h"

USHCombatStressSystem::USHCombatStressSystem()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.05f; // 20 Hz — sufficient for physiological model

	// Default stress impulses per source (doctrine-aligned).
	StressImpulses.Add(ESHStressSource::IncomingFire,     0.08f);
	StressImpulses.Add(ESHStressSource::NearExplosion,    0.25f);
	StressImpulses.Add(ESHStressSource::WitnessCasualty,  0.15f);
	StressImpulses.Add(ESHStressSource::SustainedCombat,  0.02f); // Per-tick while in combat
	StressImpulses.Add(ESHStressSource::PersonalWound,    0.20f);
	StressImpulses.Add(ESHStressSource::Isolation,        0.05f);
	StressImpulses.Add(ESHStressSource::FriendlyFire,     0.30f);
}

void USHCombatStressSystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TimeSinceLastStress += DeltaTime;

	// Decay stress over time.
	if (TimeSinceLastStress > DecayGracePeriod)
	{
		const float Rate = bIsInSafePosition ? SafeDecayRate : DecayRate;
		StressValue = FMath::Max(0.f, StressValue - Rate * DeltaTime);
	}

	UpdateHeartRate(DeltaTime);
	UpdateStressLevel();
}

// ======================================================================
//  Stress input
// ======================================================================

void USHCombatStressSystem::AddStress(ESHStressSource Source, float Magnitude)
{
	const float* BaseImpulse = StressImpulses.Find(Source);
	const float Impulse = BaseImpulse ? (*BaseImpulse * Magnitude) : (0.1f * Magnitude);

	// Diminishing returns at high stress — harder to push from 0.8 to 1.0.
	const float Headroom = 1.0f - StressValue;
	const float EffectiveImpulse = Impulse * (0.4f + 0.6f * Headroom);

	StressValue = FMath::Clamp(StressValue + EffectiveImpulse, 0.f, 1.f);
	TimeSinceLastStress = 0.f;
}

void USHCombatStressSystem::ResetStress()
{
	StressValue = 0.f;
	CurrentHeartRate = RestingHeartRate;
	CurrentLevel = ESHStressLevel::Calm;
	bCSRTriggered = false;
}

// ======================================================================
//  Heart rate model
// ======================================================================

void USHCombatStressSystem::UpdateHeartRate(float DeltaTime)
{
	// Target heart rate is a function of stress value.
	const float TargetHR = RestingHeartRate + StressValue * (MaxHeartRate - RestingHeartRate);

	// Heart rate lags behind stress (physiological inertia).
	// Rises faster than it falls — adrenaline onset is rapid, cooldown is slow.
	const float RiseRate = 40.f; // bpm/sec
	const float FallRate = 12.f; // bpm/sec
	const float Rate = (TargetHR > CurrentHeartRate) ? RiseRate : FallRate;

	CurrentHeartRate = FMath::FInterpTo(CurrentHeartRate, TargetHR, DeltaTime, Rate / 30.f);
}

// ======================================================================
//  Level evaluation
// ======================================================================

void USHCombatStressSystem::UpdateStressLevel()
{
	// Map heart rate to stress levels (per doctrine heart-rate zones).
	ESHStressLevel NewLevel;

	if (CurrentHeartRate < 100.f)
		NewLevel = ESHStressLevel::Calm;
	else if (CurrentHeartRate < 130.f)
		NewLevel = ESHStressLevel::Alert;
	else if (CurrentHeartRate < 155.f)
		NewLevel = ESHStressLevel::Stressed;
	else if (CurrentHeartRate < 175.f)
		NewLevel = ESHStressLevel::HighStress;
	else
		NewLevel = ESHStressLevel::CombatStressReaction;

	if (NewLevel != CurrentLevel)
	{
		CurrentLevel = NewLevel;
		OnStressLevelChanged.Broadcast(CurrentLevel);

		if (CurrentLevel == ESHStressLevel::CombatStressReaction && !bCSRTriggered)
		{
			bCSRTriggered = true;
			OnCombatStressReaction.Broadcast();
		}
	}
}

// ======================================================================
//  Effect multipliers
// ======================================================================

float USHCombatStressSystem::GetAimSwayMultiplier() const
{
	// Calm: 1.0x, Alert: 1.1x, Stressed: 1.4x, HighStress: 2.2x, CSR: 3.5x
	switch (CurrentLevel)
	{
	case ESHStressLevel::Calm:                  return 1.0f;
	case ESHStressLevel::Alert:                 return 1.1f;
	case ESHStressLevel::Stressed:              return 1.4f;
	case ESHStressLevel::HighStress:            return 2.2f;
	case ESHStressLevel::CombatStressReaction:  return 3.5f;
	default:                                     return 1.0f;
	}
}

float USHCombatStressSystem::GetReloadSpeedMultiplier() const
{
	// Fine motor degradation: fumbling with magazines under stress.
	// Multiplier >1 means slower reload.
	switch (CurrentLevel)
	{
	case ESHStressLevel::Calm:                  return 1.0f;
	case ESHStressLevel::Alert:                 return 1.0f;
	case ESHStressLevel::Stressed:              return 1.15f;
	case ESHStressLevel::HighStress:            return 1.4f;
	case ESHStressLevel::CombatStressReaction:  return 1.8f;
	default:                                     return 1.0f;
	}
}

float USHCombatStressSystem::GetTunnelVisionIntensity() const
{
	// Progressive vignetting. 0 = no effect, 1 = severe.
	switch (CurrentLevel)
	{
	case ESHStressLevel::Calm:                  return 0.0f;
	case ESHStressLevel::Alert:                 return 0.0f;
	case ESHStressLevel::Stressed:              return 0.1f;
	case ESHStressLevel::HighStress:            return 0.4f;
	case ESHStressLevel::CombatStressReaction:  return 0.7f;
	default:                                     return 0.0f;
	}
}

float USHCombatStressSystem::GetAuditoryExclusionStrength() const
{
	// Selective audio dropout at peak stress.
	switch (CurrentLevel)
	{
	case ESHStressLevel::Calm:                  return 0.0f;
	case ESHStressLevel::Alert:                 return 0.0f;
	case ESHStressLevel::Stressed:              return 0.05f;
	case ESHStressLevel::HighStress:            return 0.35f;
	case ESHStressLevel::CombatStressReaction:  return 0.65f;
	default:                                     return 0.0f;
	}
}

float USHCombatStressSystem::GetTimeDistortionFactor() const
{
	// Subtle time scaling. <1.0 = time feels slower (adrenaline dump).
	// Only kicks in at HighStress+.
	switch (CurrentLevel)
	{
	case ESHStressLevel::Calm:                  return 1.0f;
	case ESHStressLevel::Alert:                 return 1.0f;
	case ESHStressLevel::Stressed:              return 1.0f;
	case ESHStressLevel::HighStress:            return 0.92f;
	case ESHStressLevel::CombatStressReaction:  return 0.82f;
	default:                                     return 1.0f;
	}
}

float USHCombatStressSystem::GetCognitiveNarrowingFactor() const
{
	// Affects interface usability — menu response delay, compass reading difficulty.
	switch (CurrentLevel)
	{
	case ESHStressLevel::Calm:                  return 0.0f;
	case ESHStressLevel::Alert:                 return 0.0f;
	case ESHStressLevel::Stressed:              return 0.1f;
	case ESHStressLevel::HighStress:            return 0.3f;
	case ESHStressLevel::CombatStressReaction:  return 0.6f;
	default:                                     return 0.0f;
	}
}
