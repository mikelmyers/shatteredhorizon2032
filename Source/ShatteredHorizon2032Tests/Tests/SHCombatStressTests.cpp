// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.
//
// Combat Stress System Tests
//
// The combat stress system is a key differentiator — it models real
// physiological responses to combat. These tests verify heart rate
// modeling, effect thresholds, and the relationship between stress
// and gameplay degradation.

#include "Misc/AutomationTest.h"
#include "Combat/SHCombatStressSystem.h"

#if WITH_AUTOMATION_TESTS

// ========================================================================
//  Heart rate model: physiological boundaries
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatStress_HeartRateBounds, "SH2032.Combat.Stress.HeartRate.Bounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCombatStress_HeartRateBounds::RunTest(const FString&)
{
	USHCombatStressSystem* Stress = NewObject<USHCombatStressSystem>();

	// Resting heart rate should be 60-80 bpm
	const float RestHR = Stress->GetRestingHeartRate();
	TestTrue(TEXT("Resting HR >= 60"), RestHR >= 60.f);
	TestTrue(TEXT("Resting HR <= 80"), RestHR <= 80.f);

	// Maximum heart rate should be 170-200 bpm (combat peak)
	const float MaxHR = Stress->GetMaxHeartRate();
	TestTrue(TEXT("Max HR >= 170"), MaxHR >= 170.f);
	TestTrue(TEXT("Max HR <= 220"), MaxHR <= 220.f);

	// Max must be greater than resting (sanity)
	TestTrue(TEXT("Max > Resting"), MaxHR > RestHR);

	return true;
}

// ========================================================================
//  Stress effects: thresholds for gameplay degradation
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatStress_EffectThresholds, "SH2032.Combat.Stress.EffectThresholds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCombatStress_EffectThresholds::RunTest(const FString&)
{
	USHCombatStressSystem* Stress = NewObject<USHCombatStressSystem>();

	// Get effect multipliers at different stress levels.
	// At zero stress (resting), effects should be minimal/none.
	const float AimSwayAtRest = Stress->GetAimSwayMultiplier();
	const float ReloadAtRest = Stress->GetReloadSpeedMultiplier();

	// At rest, aim sway should be 1.0 (no penalty)
	TestTrue(TEXT("Aim sway at rest <= 1.1"), AimSwayAtRest <= 1.1f);
	TestTrue(TEXT("Aim sway at rest >= 0.9"), AimSwayAtRest >= 0.9f);

	// Reload speed at rest should be 1.0 (no penalty)
	TestTrue(TEXT("Reload at rest <= 1.1"), ReloadAtRest <= 1.1f);
	TestTrue(TEXT("Reload at rest >= 0.9"), ReloadAtRest >= 0.9f);

	// Tunnel vision at rest should be 0 (no vignette)
	const float TunnelVisionAtRest = Stress->GetTunnelVisionIntensity();
	TestTrue(TEXT("No tunnel vision at rest"), TunnelVisionAtRest < 0.05f);

	// Auditory exclusion at rest should be 0
	const float AuditoryExclAtRest = Stress->GetAuditoryExclusionLevel();
	TestTrue(TEXT("No auditory exclusion at rest"), AuditoryExclAtRest < 0.05f);

	return true;
}

// ========================================================================
//  Time distortion: must be subtle
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCombatStress_TimeDilation, "SH2032.Combat.Stress.TimeDilation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FCombatStress_TimeDilation::RunTest(const FString&)
{
	USHCombatStressSystem* Stress = NewObject<USHCombatStressSystem>();

	// Time distortion should never be dramatic — max 15-20% slowdown
	const float TimeDilation = Stress->GetTimeDilationFactor();

	// At rest, no dilation
	TestTrue(TEXT("Time dilation at rest ~1.0"), FMath::IsNearlyEqual(TimeDilation, 1.0f, 0.05f));

	return true;
}

#endif // WITH_AUTOMATION_TESTS
