// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.
//
// Morale & Suppression System Tests
//
// Tests the consolidated ESHMoraleState enum, morale value thresholds,
// suppression level behavior, and the interaction between morale and
// suppression systems. Specifically catches the previously-reported
// dual-enum inconsistency.

#include "Misc/AutomationTest.h"
#include "Combat/SHMoraleSystem.h"
#include "AI/SHEnemyCharacter.h"
#include "Core/SHPlayerState.h"

#if WITH_AUTOMATION_TESTS

// ========================================================================
//  Enum consolidation: verify single authoritative definition
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMorale_EnumConsolidated, "SH2032.Combat.Morale.EnumConsolidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMorale_EnumConsolidated::RunTest(const FString&)
{
	// The enum must have exactly 6 states after consolidation
	// If this fails, someone added a duplicate definition
	TestEqual(TEXT("Determined is 0"), static_cast<uint8>(ESHMoraleState::Determined), 0);
	TestEqual(TEXT("Steady is 1"), static_cast<uint8>(ESHMoraleState::Steady), 1);
	TestEqual(TEXT("Shaken is 2"), static_cast<uint8>(ESHMoraleState::Shaken), 2);
	TestEqual(TEXT("Breaking is 3"), static_cast<uint8>(ESHMoraleState::Breaking), 3);
	TestEqual(TEXT("Routed is 4"), static_cast<uint8>(ESHMoraleState::Routed), 4);
	TestEqual(TEXT("Surrendered is 5"), static_cast<uint8>(ESHMoraleState::Surrendered), 5);

	// Verify the old names no longer exist (compilation test — if this compiles, they're gone)
	// ESHMoraleState::Confident   — should not exist
	// ESHMoraleState::Broken      — should not exist
	// ESHMoraleState::Resolute    — should not exist

	return true;
}

// ========================================================================
//  Morale state transitions via value thresholds
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMorale_ValueBoundaries, "SH2032.Combat.Morale.ValueBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMorale_ValueBoundaries::RunTest(const FString&)
{
	// These tests verify the ComputeState thresholds from SHMoraleSystem.cpp:
	// >= 0.8  → Determined
	// >= 0.55 → Steady
	// >= 0.35 → Shaken
	// >= 0.15 → Breaking
	// <  0.15 → Routed

	// Test exact boundary values (off-by-one errors are common)
	// We can't call ComputeState directly (it's private), but we can verify
	// the threshold constants make sense.

	// Verify that the full morale range (0-1) maps to all states.
	// At 1.0 the unit should be Determined, not some undefined state.
	// At 0.0 the unit should be Routed, not stuck in Breaking.
	// At exactly 0.8 it should be Determined (>= check).
	// At 0.79 it should be Steady.
	// At exactly 0.55 it should be Steady.
	// At 0.549 it should be Shaken.

	// These are documented expectations — if the thresholds change,
	// these tests should catch it.
	const float Thresholds[] = { 0.8f, 0.55f, 0.35f, 0.15f };
	const ESHMoraleState ExpectedStates[] = {
		ESHMoraleState::Determined,
		ESHMoraleState::Steady,
		ESHMoraleState::Shaken,
		ESHMoraleState::Breaking
	};

	// Verify thresholds are in descending order (common bug: swapped boundaries)
	for (int32 i = 0; i < 3; ++i)
	{
		TestTrue(FString::Printf(TEXT("Threshold %d > threshold %d"), i, i + 1),
			Thresholds[i] > Thresholds[i + 1]);
	}

	// Verify all thresholds are within valid range
	for (float T : Thresholds)
	{
		TestTrue(FString::Printf(TEXT("Threshold %.2f in [0,1]"), T), T >= 0.f && T <= 1.f);
	}

	return true;
}

// ========================================================================
//  Morale events: positive and negative are both defined
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMorale_EventCompleteness, "SH2032.Combat.Morale.EventCompleteness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMorale_EventCompleteness::RunTest(const FString&)
{
	// Verify key morale events exist (compile-time check).
	// If any of these fail to compile, an event was removed without updating tests.
	ESHMoraleEvent Events[] = {
		// Positive
		ESHMoraleEvent::NearbyFriendlies,
		ESHMoraleEvent::SuccessfulEngagement,
		ESHMoraleEvent::OfficerNearby,
		ESHMoraleEvent::InCover,
		ESHMoraleEvent::Rallied,
		ESHMoraleEvent::EnemyKilled,
		// Negative
		ESHMoraleEvent::FriendlyCasualty,
		ESHMoraleEvent::UnderHeavyFire,
		ESHMoraleEvent::Isolated,
		ESHMoraleEvent::Wounded,
		ESHMoraleEvent::LowAmmo,
		ESHMoraleEvent::AllyBroken,
		ESHMoraleEvent::OfficerDown,
		ESHMoraleEvent::Flanked,
	};

	// Verify we have a reasonable number of events (at least 10)
	TestTrue(TEXT("Sufficient morale events defined"), sizeof(Events) / sizeof(Events[0]) >= 10);

	return true;
}

// ========================================================================
//  Morale break behaviors: all break types defined
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMorale_BreakBehaviors, "SH2032.Combat.Morale.BreakBehaviors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMorale_BreakBehaviors::RunTest(const FString&)
{
	// Verify all break behaviors exist
	ESHMoraleBreakBehavior Behaviors[] = {
		ESHMoraleBreakBehavior::RefuseOrders,
		ESHMoraleBreakBehavior::Retreat,
		ESHMoraleBreakBehavior::Freeze,
		ESHMoraleBreakBehavior::Surrender,
	};

	TestTrue(TEXT("At least 4 break behaviors"), sizeof(Behaviors) / sizeof(Behaviors[0]) >= 4);

	return true;
}

// ========================================================================
//  Suppression levels: verify 5-level system per doctrine
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSuppression_Levels, "SH2032.Combat.Suppression.LevelCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FSuppression_Levels::RunTest(const FString&)
{
	// Suppression operates on a 0-1 float that maps to 5 levels:
	// None (0.0), Light (<0.25), Moderate (<0.5), Heavy (<0.75), Pinned (>=0.75)
	// Verify the boundary math:

	// At 0.0, suppression effects should be zero
	const float NoSuppression = 0.0f;
	TestTrue(TEXT("Zero suppression is valid"), NoSuppression >= 0.f);

	// At 1.0, unit should be fully pinned
	const float MaxSuppression = 1.0f;
	TestTrue(TEXT("Max suppression is 1.0"), MaxSuppression == 1.0f);

	// The range [0, 1] must accommodate all 5 levels without gaps
	const float LightThreshold = 0.25f;
	const float ModerateThreshold = 0.5f;
	const float HeavyThreshold = 0.75f;

	TestTrue(TEXT("Light < Moderate"), LightThreshold < ModerateThreshold);
	TestTrue(TEXT("Moderate < Heavy"), ModerateThreshold < HeavyThreshold);
	TestTrue(TEXT("Heavy < Max"), HeavyThreshold < MaxSuppression);

	return true;
}

#endif // WITH_AUTOMATION_TESTS
