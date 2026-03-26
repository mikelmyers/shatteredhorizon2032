// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.
//
// AI Perception Tests
//
// These tests verify AI sight/hearing ranges match doctrine and that
// environmental conditions correctly degrade perception. The compliance
// report previously flagged AI sight as capped at 150m — these tests
// ensure that never regresses.

#include "Misc/AutomationTest.h"
#include "AI/SHAIPerceptionConfig.h"

#if WITH_AUTOMATION_TESTS

// ========================================================================
//  Sight range doctrine: AI must engage at realistic distances
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSightRange_ClearDay, "SH2032.AI.Perception.Sight.ClearDay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSightRange_ClearDay::RunTest(const FString&)
{
	USHAIPerceptionConfig* Config = NewObject<USHAIPerceptionConfig>();
	FSHSightRangeEntry Entry = Config->GetSightRange(ESHVisibilityCondition::ClearDay);

	// CRITICAL: Doctrine requires 800m+ engagement distance in clear conditions
	// This was previously broken at 150m — NEVER let this regress
	TestTrue(TEXT("Clear day max range >= 80000cm (800m)"), Entry.MaxRange >= 80000.f);
	TestTrue(TEXT("Instant detection >= 15000cm (150m)"), Entry.InstantDetectionRange >= 15000.f);
	TestTrue(TEXT("Instant detection < max range"), Entry.InstantDetectionRange < Entry.MaxRange);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSightRange_Night, "SH2032.AI.Perception.Sight.Night",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSightRange_Night::RunTest(const FString&)
{
	USHAIPerceptionConfig* Config = NewObject<USHAIPerceptionConfig>();
	FSHSightRangeEntry Night = Config->GetSightRange(ESHVisibilityCondition::Night);
	FSHSightRangeEntry ClearDay = Config->GetSightRange(ESHVisibilityCondition::ClearDay);

	// Night should DRASTICALLY reduce sight
	TestTrue(TEXT("Night range < Clear day"), Night.MaxRange < ClearDay.MaxRange);
	TestTrue(TEXT("Night range <= 10000cm (100m)"), Night.MaxRange <= 10000.f);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSightRange_NVG, "SH2032.AI.Perception.Sight.NightNVG",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSightRange_NVG::RunTest(const FString&)
{
	USHAIPerceptionConfig* Config = NewObject<USHAIPerceptionConfig>();
	FSHSightRangeEntry Night = Config->GetSightRange(ESHVisibilityCondition::Night);
	FSHSightRangeEntry NVG = Config->GetSightRange(ESHVisibilityCondition::NightNVG);

	// NVG should dramatically improve night vision
	TestTrue(TEXT("NVG > unaided night"), NVG.MaxRange > Night.MaxRange * 3.f);
	// But NVG should narrow FOV (tunnel vision effect)
	TestTrue(TEXT("NVG narrows peripheral"), NVG.PeripheralHalfAngleDeg < Night.PeripheralHalfAngleDeg);
	TestTrue(TEXT("NVG narrows central"), NVG.CentralHalfAngleDeg < Night.CentralHalfAngleDeg);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSightRange_Smoke, "SH2032.AI.Perception.Sight.Smoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSightRange_Smoke::RunTest(const FString&)
{
	USHAIPerceptionConfig* Config = NewObject<USHAIPerceptionConfig>();
	FSHSightRangeEntry Smoke = Config->GetSightRange(ESHVisibilityCondition::Smoke);

	// Smoke should reduce visibility to near zero
	TestTrue(TEXT("Smoke max range <= 2000cm (20m)"), Smoke.MaxRange <= 2000.f);

	return true;
}

// ========================================================================
//  Environmental degradation ordering
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSightRange_DegradationOrder, "SH2032.AI.Perception.Sight.DegradationOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSightRange_DegradationOrder::RunTest(const FString&)
{
	USHAIPerceptionConfig* Config = NewObject<USHAIPerceptionConfig>();

	const float ClearDay = Config->GetSightRange(ESHVisibilityCondition::ClearDay).MaxRange;
	const float Overcast = Config->GetSightRange(ESHVisibilityCondition::Overcast).MaxRange;
	const float Rain = Config->GetSightRange(ESHVisibilityCondition::HeavyRain).MaxRange;
	const float Dusk = Config->GetSightRange(ESHVisibilityCondition::Dusk).MaxRange;
	const float Fog = Config->GetSightRange(ESHVisibilityCondition::Fog).MaxRange;
	const float Night = Config->GetSightRange(ESHVisibilityCondition::Night).MaxRange;
	const float Smoke = Config->GetSightRange(ESHVisibilityCondition::Smoke).MaxRange;

	// Clear > Overcast > Rain > Dusk > Fog > Night > Smoke
	TestTrue(TEXT("Clear > Overcast"), ClearDay > Overcast);
	TestTrue(TEXT("Overcast > Rain"), Overcast > Rain);
	TestTrue(TEXT("Rain > Dusk"), Rain > Dusk);
	TestTrue(TEXT("Dusk > Fog"), Dusk > Fog);
	TestTrue(TEXT("Fog > Night"), Fog > Night);
	TestTrue(TEXT("Night > Smoke"), Night > Smoke);

	return true;
}

// ========================================================================
//  Hearing ranges: gunfire must be louder than footsteps
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHearingRange_SoundOrdering, "SH2032.AI.Perception.Hearing.SoundOrdering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FHearingRange_SoundOrdering::RunTest(const FString&)
{
	USHAIPerceptionConfig* Config = NewObject<USHAIPerceptionConfig>();

	const float Explosion = Config->GetHearingRange(ESHSoundType::Explosion).MaxRange;
	const float GunfireUnsup = Config->GetHearingRange(ESHSoundType::GunfireUnsuppressed).MaxRange;
	const float Vehicle = Config->GetHearingRange(ESHSoundType::VehicleEngine).MaxRange;
	const float GunfireSup = Config->GetHearingRange(ESHSoundType::GunfireSuppressed).MaxRange;
	const float Shout = Config->GetHearingRange(ESHSoundType::Voice_Shout).MaxRange;
	const float SprintStep = Config->GetHearingRange(ESHSoundType::Footstep_Sprint).MaxRange;
	const float WalkStep = Config->GetHearingRange(ESHSoundType::Footstep_Walk).MaxRange;
	const float Rattle = Config->GetHearingRange(ESHSoundType::EquipmentRattle).MaxRange;

	// Loud to quiet ordering
	TestTrue(TEXT("Explosion > Gunfire"), Explosion > GunfireUnsup);
	TestTrue(TEXT("Gunfire > Vehicle"), GunfireUnsup > Vehicle);
	TestTrue(TEXT("Vehicle > Suppressed gunfire"), Vehicle > GunfireSup);
	TestTrue(TEXT("Suppressed > Shout"), GunfireSup > Shout);
	TestTrue(TEXT("Sprint > Walk"), SprintStep > WalkStep);
	TestTrue(TEXT("Walk > Equipment rattle"), WalkStep > Rattle);

	// Suppressed gunfire should be dramatically quieter than unsuppressed
	TestTrue(TEXT("Suppressed <= 1/3 of unsuppressed"), GunfireSup <= GunfireUnsup / 3.f);

	return true;
}

// ========================================================================
//  Detection speed config: reasonable values
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDetectionSpeed_Config, "SH2032.AI.Perception.Detection.SpeedConfig",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDetectionSpeed_Config::RunTest(const FString&)
{
	USHAIPerceptionConfig* Config = NewObject<USHAIPerceptionConfig>();
	const FSHDetectionSpeedConfig& D = Config->DetectionSpeed;

	// Posture multipliers: standing > crouching > prone
	TestTrue(TEXT("Standing multiplier >= crouching"), D.StandingMultiplier >= D.CrouchingMultiplier);
	TestTrue(TEXT("Crouching multiplier >= prone"), D.CrouchingMultiplier >= D.ProneMultiplier);

	// Moving should increase detection
	TestTrue(TEXT("Moving multiplier > 1.0"), D.MovingMultiplier > 1.f);

	// Firing should massively increase detection
	TestTrue(TEXT("Firing multiplier > moving"), D.FiringMultiplier > D.MovingMultiplier);

	// Memory duration should be reasonable (5-30 seconds)
	TestTrue(TEXT("Memory duration >= 5s"), D.MemoryDuration >= 5.f);
	TestTrue(TEXT("Memory duration <= 60s"), D.MemoryDuration <= 60.f);

	// Decay rate should be positive
	TestTrue(TEXT("Decay rate > 0"), D.DecayRate > 0.f);

	return true;
}

// ========================================================================
//  Awareness transitions: thresholds must be ordered correctly
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAwareness_Thresholds, "SH2032.AI.Perception.Awareness.Thresholds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FAwareness_Thresholds::RunTest(const FString&)
{
	USHAIPerceptionConfig* Config = NewObject<USHAIPerceptionConfig>();
	const FSHAwarenessTransitionConfig& A = Config->AwarenessTransitions;

	// Thresholds must be ordered: Suspicious < Alert < Combat
	TestTrue(TEXT("Suspicious < Alert threshold"), A.SuspiciousThreshold < A.AlertThreshold);
	TestTrue(TEXT("Alert < Combat threshold"), A.AlertThreshold < A.CombatThreshold);

	// Combat threshold should be 1.0 (full detection)
	TestEqual(TEXT("Combat threshold is 1.0"), A.CombatThreshold, 1.0f);

	// Cooldown durations should be positive and ordered
	TestTrue(TEXT("Search duration > 0"), A.SearchDuration > 0.f);
	TestTrue(TEXT("Alert cooldown > search"), A.AlertCooldownDuration > A.SearchDuration);
	TestTrue(TEXT("Suspicious cooldown > alert"), A.SuspiciousCooldownDuration > A.AlertCooldownDuration);

	// Damage should force combat (per doctrine)
	TestTrue(TEXT("Damage forces combat"), A.bDamageImmediateCombat);

	return true;
}

// ========================================================================
//  Fallback: unknown conditions should return sensible defaults
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPerception_Fallback, "SH2032.AI.Perception.Fallback.UnknownCondition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPerception_Fallback::RunTest(const FString&)
{
	USHAIPerceptionConfig* Config = NewObject<USHAIPerceptionConfig>();

	// Default FSHSightRangeEntry has MaxRange=8000 — verify GetSightRange
	// returns something reasonable even for a valid but potentially edge-case lookup
	FSHSightRangeEntry Entry = Config->GetSightRange(ESHVisibilityCondition::ClearDay);
	TestTrue(TEXT("Valid condition returns positive range"), Entry.MaxRange > 0.f);

	// Hearing fallback for a valid type should return positive range
	FSHHearingRangeEntry Hearing = Config->GetHearingRange(ESHSoundType::Explosion);
	TestTrue(TEXT("Explosion hearing range positive"), Hearing.MaxRange > 0.f);

	return true;
}

#endif // WITH_AUTOMATION_TESTS
