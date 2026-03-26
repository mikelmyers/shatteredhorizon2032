// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.
//
// Weapon Data Doctrine Verification Tests
//
// These tests verify that every weapon in the arsenal has doctrine-accurate
// ballistic data. They are NOT rubber stamps — they cross-reference against
// real-world specifications and catch copy-paste errors, unit conversions,
// and data entry mistakes.

#include "Misc/AutomationTest.h"
#include "Weapons/SHWeaponData.h"

#if WITH_AUTOMATION_TESTS

// ========================================================================
//  Helper: Create a weapon DataAsset and apply factory defaults
// ========================================================================

static USHWeaponDataAsset* CreateWeaponData(void (*ApplyFn)(USHWeaponDataAsset*))
{
	USHWeaponDataAsset* Data = NewObject<USHWeaponDataAsset>();
	ApplyFn(Data);
	return Data;
}

// ========================================================================
//  Doctrine: Muzzle velocities must match real-world specifications
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponVelocity_M27IAR, "SH2032.Weapons.Velocity.M27_IAR",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWeaponVelocity_M27IAR::RunTest(const FString&)
{
	USHWeaponDataAsset* D = CreateWeaponData(USHWeaponDataFactory::ApplyDefaults_M27_IAR);
	// M27 IAR fires M855A1 at 940 m/s from a 16.5" barrel
	TestEqual(TEXT("M27 muzzle velocity"), D->MuzzleVelocityMPS, 940.0f);
	TestEqual(TEXT("M27 effective range"), D->EffectiveRangeM, 550.0f);
	TestTrue(TEXT("M27 max range > effective"), D->MaxRangeM > D->EffectiveRangeM);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponVelocity_M4A1, "SH2032.Weapons.Velocity.M4A1",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWeaponVelocity_M4A1::RunTest(const FString&)
{
	USHWeaponDataAsset* D = CreateWeaponData(USHWeaponDataFactory::ApplyDefaults_M4A1);
	// M4A1 fires M855A1 at 910 m/s from a 14.5" barrel (shorter than M27)
	TestEqual(TEXT("M4A1 muzzle velocity"), D->MuzzleVelocityMPS, 910.0f);
	// M4A1 should be SLOWER than M27 (shorter barrel)
	USHWeaponDataAsset* M27 = CreateWeaponData(USHWeaponDataFactory::ApplyDefaults_M27_IAR);
	TestTrue(TEXT("M4A1 slower than M27 (shorter barrel)"), D->MuzzleVelocityMPS < M27->MuzzleVelocityMPS);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponVelocity_M110, "SH2032.Weapons.Velocity.M110_SASS",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWeaponVelocity_M110::RunTest(const FString&)
{
	USHWeaponDataAsset* D = CreateWeaponData(USHWeaponDataFactory::ApplyDefaults_M110_SASS);
	// M110 SASS fires 7.62x51mm M118LR at 840 m/s
	TestEqual(TEXT("M110 muzzle velocity"), D->MuzzleVelocityMPS, 840.0f);
	// 7.62mm should do MORE damage per round than 5.56mm
	USHWeaponDataAsset* M4 = CreateWeaponData(USHWeaponDataFactory::ApplyDefaults_M4A1);
	TestTrue(TEXT("7.62mm > 5.56mm base damage"), D->BaseDamage > M4->BaseDamage);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponVelocity_SniperLapua, "SH2032.Weapons.Velocity.SniperLapua",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWeaponVelocity_SniperLapua::RunTest(const FString&)
{
	USHWeaponDataAsset* D = CreateWeaponData(USHWeaponDataFactory::ApplyDefaults_SniperLapua);
	// .338 Lapua Magnum: 915 m/s, effective to 1500m
	TestEqual(TEXT("Lapua muzzle velocity"), D->MuzzleVelocityMPS, 915.0f);
	TestTrue(TEXT("Lapua effective range >= 1200m"), D->EffectiveRangeM >= 1200.0f);
	TestTrue(TEXT("Lapua is categorized as sniper"), D->Category == ESHWeaponCategory::SniperRifle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponVelocity_M2Browning, "SH2032.Weapons.Velocity.M2_Browning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWeaponVelocity_M2Browning::RunTest(const FString&)
{
	USHWeaponDataAsset* D = CreateWeaponData(USHWeaponDataFactory::ApplyDefaults_M2_Browning);
	// .50 BMG: 930 m/s, effective to 1800m
	TestEqual(TEXT("M2 muzzle velocity"), D->MuzzleVelocityMPS, 930.0f);
	TestTrue(TEXT("M2 is heavy MG category"), D->Category == ESHWeaponCategory::HeavyMG);
	// .50 BMG should have highest base damage of any weapon
	USHWeaponDataAsset* Lapua = CreateWeaponData(USHWeaponDataFactory::ApplyDefaults_SniperLapua);
	TestTrue(TEXT(".50 BMG > .338 Lapua damage"), D->BaseDamage > Lapua->BaseDamage);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponVelocity_Type56, "SH2032.Weapons.Velocity.Type56",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWeaponVelocity_Type56::RunTest(const FString&)
{
	USHWeaponDataAsset* D = CreateWeaponData(USHWeaponDataFactory::ApplyDefaults_Type56);
	// Type 56 (AK variant) fires 7.62x39mm at 715 m/s
	TestEqual(TEXT("Type56 muzzle velocity"), D->MuzzleVelocityMPS, 715.0f);
	// 7.62x39mm has shorter effective range than 5.56 NATO
	USHWeaponDataAsset* M4 = CreateWeaponData(USHWeaponDataFactory::ApplyDefaults_M4A1);
	TestTrue(TEXT("Type56 shorter effective range than M4"), D->EffectiveRangeM < M4->EffectiveRangeM);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponVelocity_M17SIG, "SH2032.Weapons.Velocity.M17_SIG",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWeaponVelocity_M17SIG::RunTest(const FString&)
{
	USHWeaponDataAsset* D = CreateWeaponData(USHWeaponDataFactory::ApplyDefaults_M17_SIG);
	// M17 fires 9x19mm at 375 m/s, effective to 50m
	TestEqual(TEXT("M17 muzzle velocity"), D->MuzzleVelocityMPS, 375.0f);
	TestTrue(TEXT("M17 effective range <= 60m"), D->EffectiveRangeM <= 60.0f);
	TestTrue(TEXT("M17 is sidearm"), D->Category == ESHWeaponCategory::Sidearm);
	// Pistol should NOT penetrate steel
	bool bPenetratesSteel = false;
	for (const FSHPenetrationEntry& P : D->PenetrationTable)
	{
		if (P.Material == ESHPenetrableMaterial::Steel && P.MaxPenetrationCm > 0.0f)
		{
			bPenetratesSteel = true;
		}
	}
	TestFalse(TEXT("9mm cannot penetrate steel"), bPenetratesSteel);
	return true;
}

// ========================================================================
//  Cross-weapon sanity: physics must be internally consistent
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponPhysicsConsistency, "SH2032.Weapons.Physics.CrossWeaponConsistency",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWeaponPhysicsConsistency::RunTest(const FString&)
{
	USHWeaponDataAsset* M27 = CreateWeaponData(USHWeaponDataFactory::ApplyDefaults_M27_IAR);
	USHWeaponDataAsset* M4 = CreateWeaponData(USHWeaponDataFactory::ApplyDefaults_M4A1);
	USHWeaponDataAsset* M249 = CreateWeaponData(USHWeaponDataFactory::ApplyDefaults_M249_SAW);
	USHWeaponDataAsset* M110 = CreateWeaponData(USHWeaponDataFactory::ApplyDefaults_M110_SASS);
	USHWeaponDataAsset* M17 = CreateWeaponData(USHWeaponDataFactory::ApplyDefaults_M17_SIG);
	USHWeaponDataAsset* Lapua = CreateWeaponData(USHWeaponDataFactory::ApplyDefaults_SniperLapua);
	USHWeaponDataAsset* M2 = CreateWeaponData(USHWeaponDataFactory::ApplyDefaults_M2_Browning);
	USHWeaponDataAsset* QBZ = CreateWeaponData(USHWeaponDataFactory::ApplyDefaults_QBZ95);
	USHWeaponDataAsset* T56 = CreateWeaponData(USHWeaponDataFactory::ApplyDefaults_Type56);
	USHWeaponDataAsset* M320 = CreateWeaponData(USHWeaponDataFactory::ApplyDefaults_M320_GL);
	USHWeaponDataAsset* Shotgun = CreateWeaponData(USHWeaponDataFactory::ApplyDefaults_Mossberg590);

	// Heavier bullets should have higher bullet mass
	TestTrue(TEXT(".50 BMG heavier than .338 Lapua"), M2->Ballistics.BulletMassGrams > Lapua->Ballistics.BulletMassGrams);
	TestTrue(TEXT(".338 Lapua heavier than 7.62x51"), Lapua->Ballistics.BulletMassGrams > M110->Ballistics.BulletMassGrams);
	TestTrue(TEXT("7.62x51 heavier than 5.56x45"), M110->Ballistics.BulletMassGrams > M27->Ballistics.BulletMassGrams);
	TestTrue(TEXT("5.56x45 heavier than 9x19"), M27->Ballistics.BulletMassGrams > M17->Ballistics.BulletMassGrams);

	// Higher caliber = more penetration through wood
	float M2_WoodPen = 0, M27_WoodPen = 0, M17_WoodPen = 0;
	for (const auto& P : M2->PenetrationTable) { if (P.Material == ESHPenetrableMaterial::Wood) M2_WoodPen = P.MaxPenetrationCm; }
	for (const auto& P : M27->PenetrationTable) { if (P.Material == ESHPenetrableMaterial::Wood) M27_WoodPen = P.MaxPenetrationCm; }
	for (const auto& P : M17->PenetrationTable) { if (P.Material == ESHPenetrableMaterial::Wood) M17_WoodPen = P.MaxPenetrationCm; }
	TestTrue(TEXT(".50 BMG penetrates more wood than 5.56"), M2_WoodPen > M27_WoodPen);
	TestTrue(TEXT("5.56 penetrates more wood than 9mm"), M27_WoodPen > M17_WoodPen);

	// Snipers should have better accuracy (lower MOA) than assault rifles
	TestTrue(TEXT("M110 more accurate than M4 standing"), M110->AccuracyModifiers.StandingMOA < M4->AccuracyModifiers.StandingMOA);
	TestTrue(TEXT("Lapua more accurate than M110 prone"), Lapua->AccuracyModifiers.ProneMOA <= M110->AccuracyModifiers.ProneMOA);

	// Every weapon must have a positive weight
	TestTrue(TEXT("M27 has weight"), M27->WeightKg > 0.f);
	TestTrue(TEXT("M17 has weight"), M17->WeightKg > 0.f);
	TestTrue(TEXT("M2 has weight"), M2->WeightKg > 0.f);

	// M2 Browning should be the heaviest weapon
	TestTrue(TEXT("M2 heaviest weapon"), M2->WeightKg > Lapua->WeightKg);
	TestTrue(TEXT("M2 heaviest weapon vs M249"), M2->WeightKg > M249->WeightKg);

	// No weapon should have zero magazine capacity
	TestTrue(TEXT("M27 has magazine"), M27->MagazineCapacity > 0);
	TestTrue(TEXT("QBZ has magazine"), QBZ->MagazineCapacity > 0);
	TestTrue(TEXT("Type56 has magazine"), T56->MagazineCapacity > 0);

	// Fire rates: SAW/MG should be higher than rifles
	TestTrue(TEXT("M249 faster than M4"), M249->RoundsPerMinute > M4->RoundsPerMinute);
	TestTrue(TEXT("M2 slower than M249 (heavy MG)"), M2->RoundsPerMinute < M249->RoundsPerMinute);

	// Sidearm reload should be faster than rifle reload
	TestTrue(TEXT("M17 reloads faster than M27"), M17->TacticalReloadTime < M27->TacticalReloadTime);

	return true;
}

// ========================================================================
//  Edge case: weapons with special characteristics
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponSpecialCases, "SH2032.Weapons.SpecialCases",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWeaponSpecialCases::RunTest(const FString&)
{
	USHWeaponDataAsset* Shotgun = CreateWeaponData(USHWeaponDataFactory::ApplyDefaults_Mossberg590);
	USHWeaponDataAsset* M320 = CreateWeaponData(USHWeaponDataFactory::ApplyDefaults_M320_GL);

	// Shotgun should fire multiple pellets
	TestTrue(TEXT("Shotgun fires pellets"), Shotgun->PelletsPerShot > 1);

	// Shotgun should be single-round reload
	TestTrue(TEXT("Shotgun single-round reload"), Shotgun->bSingleRoundReload);

	// M320 should be slowest muzzle velocity (lobbed grenade)
	TestTrue(TEXT("M320 low velocity"), M320->MuzzleVelocityMPS < 100.0f);

	// M320 should be grenade launcher category
	TestTrue(TEXT("M320 is GL category"), M320->Category == ESHWeaponCategory::GrenadeLauncher);

	// Shotgun should have shortest effective range of all long guns
	USHWeaponDataAsset* M4 = CreateWeaponData(USHWeaponDataFactory::ApplyDefaults_M4A1);
	TestTrue(TEXT("Shotgun shorter range than M4"), Shotgun->EffectiveRangeM < M4->EffectiveRangeM);

	return true;
}

// ========================================================================
//  Penetration table completeness: every weapon needs a table
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWeaponPenetrationCompleteness, "SH2032.Weapons.Penetration.Completeness",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FWeaponPenetrationCompleteness::RunTest(const FString&)
{
	using ApplyFn = void(*)(USHWeaponDataAsset*);
	struct FWeaponEntry { const TCHAR* Name; ApplyFn Fn; };

	FWeaponEntry Weapons[] = {
		{ TEXT("M27_IAR"), USHWeaponDataFactory::ApplyDefaults_M27_IAR },
		{ TEXT("M4A1"), USHWeaponDataFactory::ApplyDefaults_M4A1 },
		{ TEXT("M249_SAW"), USHWeaponDataFactory::ApplyDefaults_M249_SAW },
		{ TEXT("M110_SASS"), USHWeaponDataFactory::ApplyDefaults_M110_SASS },
		{ TEXT("M17_SIG"), USHWeaponDataFactory::ApplyDefaults_M17_SIG },
		{ TEXT("M320_GL"), USHWeaponDataFactory::ApplyDefaults_M320_GL },
		{ TEXT("Mossberg590"), USHWeaponDataFactory::ApplyDefaults_Mossberg590 },
		{ TEXT("SniperLapua"), USHWeaponDataFactory::ApplyDefaults_SniperLapua },
		{ TEXT("M2_Browning"), USHWeaponDataFactory::ApplyDefaults_M2_Browning },
		{ TEXT("QBZ95"), USHWeaponDataFactory::ApplyDefaults_QBZ95 },
		{ TEXT("Type56"), USHWeaponDataFactory::ApplyDefaults_Type56 },
	};

	for (const FWeaponEntry& W : Weapons)
	{
		USHWeaponDataAsset* D = CreateWeaponData(W.Fn);
		TestTrue(FString::Printf(TEXT("%s has penetration table"), W.Name), D->PenetrationTable.Num() > 0);
		TestTrue(FString::Printf(TEXT("%s has positive base damage"), W.Name), D->BaseDamage > 0.f);
		TestTrue(FString::Printf(TEXT("%s has positive muzzle velocity"), W.Name), D->MuzzleVelocityMPS > 0.f);
		TestTrue(FString::Printf(TEXT("%s has ballistic coefficient"), W.Name), D->Ballistics.BallisticCoefficient > 0.f);

		// Penetration retention values must be in [0, 1]
		for (const FSHPenetrationEntry& P : D->PenetrationTable)
		{
			TestTrue(FString::Printf(TEXT("%s damage retention <= 1.0"), W.Name), P.DamageRetention <= 1.0f);
			TestTrue(FString::Printf(TEXT("%s damage retention >= 0.0"), W.Name), P.DamageRetention >= 0.0f);
			TestTrue(FString::Printf(TEXT("%s velocity retention <= 1.0"), W.Name), P.VelocityRetention <= 1.0f);
			TestTrue(FString::Printf(TEXT("%s velocity retention >= 0.0"), W.Name), P.VelocityRetention >= 0.0f);
		}
	}

	return true;
}

#endif // WITH_AUTOMATION_TESTS
