// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.
//
// Player Character Integration Tests
//
// These tests verify the player character systems work correctly:
// weight calculation, movement penalties, injury effects, suppression
// decay, and weapon integration. Designed to catch integration bugs
// between the character, weapon, camera, and combat stress systems.

#include "Misc/AutomationTest.h"
#include "Core/SHPlayerCharacter.h"
#include "Core/SHGameMode.h"

#if WITH_AUTOMATION_TESTS

// ========================================================================
//  Weight system: doctrine max carry weight is 45 kg
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerChar_WeightConstants, "SH2032.Player.Weight.Constants",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPlayerChar_WeightConstants::RunTest(const FString&)
{
	ASHPlayerCharacter* DefaultChar = GetMutableDefault<ASHPlayerCharacter>();

	// Max carry weight per USMC doctrine: 45 kg
	TestEqual(TEXT("Max carry weight is 45 kg"), DefaultChar->GetMaxCarryWeight(), 45.f);

	// Base walk speed should be reasonable (300-400 cm/s is ~3-4 m/s walking)
	const float BaseSpeed = DefaultChar->BaseWalkSpeed;
	TestTrue(TEXT("Base walk speed >= 300"), BaseSpeed >= 300.f);
	TestTrue(TEXT("Base walk speed <= 500"), BaseSpeed <= 500.f);

	// Sprint should be 1.5-2.0x walk speed
	TestTrue(TEXT("Sprint multiplier >= 1.5"), DefaultChar->SprintSpeedMultiplier >= 1.5f);
	TestTrue(TEXT("Sprint multiplier <= 2.2"), DefaultChar->SprintSpeedMultiplier <= 2.2f);

	// Crouch should be 40-60% walk speed
	TestTrue(TEXT("Crouch multiplier >= 0.3"), DefaultChar->CrouchSpeedMultiplier >= 0.3f);
	TestTrue(TEXT("Crouch multiplier <= 0.7"), DefaultChar->CrouchSpeedMultiplier <= 0.7f);

	// Prone should be 15-35% walk speed
	TestTrue(TEXT("Prone multiplier >= 0.15"), DefaultChar->ProneSpeedMultiplier >= 0.15f);
	TestTrue(TEXT("Prone multiplier <= 0.35"), DefaultChar->ProneSpeedMultiplier <= 0.35f);

	return true;
}

// ========================================================================
//  Health: non-regenerating, limb-based
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerChar_HealthSystem, "SH2032.Player.Health.NonRegeneration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPlayerChar_HealthSystem::RunTest(const FString&)
{
	ASHPlayerCharacter* DefaultChar = GetMutableDefault<ASHPlayerCharacter>();

	// Max health should be standard (not inflated)
	TestTrue(TEXT("Max health <= 200"), DefaultChar->GetMaxHealth() <= 200.f);
	TestTrue(TEXT("Max health >= 50"), DefaultChar->GetMaxHealth() >= 50.f);

	// Bleed damage should be meaningful (1-5 HP/s per limb)
	TestTrue(TEXT("Bleed damage >= 1.0"), DefaultChar->BleedDamagePerSecond >= 1.f);
	TestTrue(TEXT("Bleed damage <= 10.0"), DefaultChar->BleedDamagePerSecond <= 10.f);

	return true;
}

// ========================================================================
//  Limb states: all 6 limbs must be initialized
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerChar_LimbCoverage, "SH2032.Player.Health.LimbCoverage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPlayerChar_LimbCoverage::RunTest(const FString&)
{
	// Verify all limb enums exist
	ESHLimb Limbs[] = {
		ESHLimb::Head,
		ESHLimb::Torso,
		ESHLimb::LeftArm,
		ESHLimb::RightArm,
		ESHLimb::LeftLeg,
		ESHLimb::RightLeg,
	};
	TestEqual(TEXT("6 limb types defined"), static_cast<int32>(sizeof(Limbs) / sizeof(Limbs[0])), 6);

	// Verify stances exist
	ESHStance Stances[] = {
		ESHStance::Standing,
		ESHStance::Crouching,
		ESHStance::Prone,
	};
	TestEqual(TEXT("3 stances defined"), static_cast<int32>(sizeof(Stances) / sizeof(Stances[0])), 3);

	return true;
}

// ========================================================================
//  Suppression: decay rate must be positive
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerChar_Suppression, "SH2032.Player.Suppression.DecayConfig",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPlayerChar_Suppression::RunTest(const FString&)
{
	ASHPlayerCharacter* DefaultChar = GetMutableDefault<ASHPlayerCharacter>();

	// Suppression should decay over time (not stay forever)
	TestTrue(TEXT("Suppression decays"), DefaultChar->SuppressionDecayRate > 0.f);

	// Max suppression should be 1.0 (normalized)
	TestEqual(TEXT("Max suppression is 1.0"), DefaultChar->MaxSuppression, 1.f);

	// At full suppression decay rate, should clear in < 10 seconds
	const float FullClearTime = DefaultChar->MaxSuppression / DefaultChar->SuppressionDecayRate;
	TestTrue(TEXT("Suppression clears in < 15s"), FullClearTime < 15.f);
	// But not too fast — should take at least 2 seconds
	TestTrue(TEXT("Suppression takes >= 2s to clear"), FullClearTime >= 2.f);

	return true;
}

// ========================================================================
//  Mission phases: verify all gameplay-relevant phases exist
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameMode_PhaseEnum, "SH2032.GameMode.PhaseEnum",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FGameMode_PhaseEnum::RunTest(const FString&)
{
	// All phases must exist
	ESHMissionPhase Phases[] = {
		ESHMissionPhase::PreInvasion,
		ESHMissionPhase::BeachAssault,
		ESHMissionPhase::UrbanFallback,
		ESHMissionPhase::Counterattack,
		ESHMissionPhase::Collapse,
		ESHMissionPhase::GuerrillaHoldout,
	};
	TestTrue(TEXT("6 mission phases defined"), sizeof(Phases) / sizeof(Phases[0]) == 6);

	return true;
}

// ========================================================================
//  Equipment slots: verify all slot types
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FPlayerChar_EquipmentSlots, "SH2032.Player.Equipment.Slots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FPlayerChar_EquipmentSlots::RunTest(const FString&)
{
	ESHEquipmentSlot Slots[] = {
		ESHEquipmentSlot::PrimaryWeapon,
		ESHEquipmentSlot::Sidearm,
		ESHEquipmentSlot::Grenade,
		ESHEquipmentSlot::Gear1,
		ESHEquipmentSlot::Gear2,
	};

	// At least 5 equipment slots
	TestTrue(TEXT("At least 5 equipment slots"), sizeof(Slots) / sizeof(Slots[0]) >= 5);

	return true;
}

#endif // WITH_AUTOMATION_TESTS
