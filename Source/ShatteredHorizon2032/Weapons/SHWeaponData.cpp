// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHWeaponData.h"

/* -----------------------------------------------------------------------
 *  Penetration table helpers
 * --------------------------------------------------------------------- */

void USHWeaponDataFactory::ApplyStandardPenetration_556(USHWeaponDataAsset* Data)
{
	if (!Data) return;

	Data->PenetrationTable.Empty();
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Drywall,   30.0f, 0.85f, 0.80f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Glass,     5.0f,  0.90f, 0.92f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Wood,      15.0f, 0.65f, 0.55f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Sandbag,   8.0f,  0.40f, 0.30f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Dirt,      12.0f, 0.50f, 0.40f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Concrete,  3.0f,  0.25f, 0.20f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Steel,     0.6f,  0.15f, 0.10f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Flesh,     35.0f, 1.00f, 0.70f });
}

void USHWeaponDataFactory::ApplyStandardPenetration_762(USHWeaponDataAsset* Data)
{
	if (!Data) return;

	Data->PenetrationTable.Empty();
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Drywall,   40.0f, 0.88f, 0.85f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Glass,     6.0f,  0.92f, 0.94f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Wood,      22.0f, 0.70f, 0.60f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Sandbag,   12.0f, 0.45f, 0.35f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Dirt,      18.0f, 0.55f, 0.45f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Concrete,  5.0f,  0.30f, 0.25f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Steel,     1.0f,  0.20f, 0.15f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Flesh,     45.0f, 1.00f, 0.65f });
}

void USHWeaponDataFactory::ApplyStandardPenetration_9mm(USHWeaponDataAsset* Data)
{
	if (!Data) return;

	Data->PenetrationTable.Empty();
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Drywall,   20.0f, 0.80f, 0.75f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Glass,     3.0f,  0.88f, 0.90f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Wood,      8.0f,  0.55f, 0.45f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Sandbag,   3.0f,  0.30f, 0.20f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Dirt,      6.0f,  0.40f, 0.30f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Concrete,  0.5f,  0.10f, 0.08f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Steel,     0.0f,  0.00f, 0.00f }); // Cannot penetrate
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Flesh,     25.0f, 1.00f, 0.75f });
}

void USHWeaponDataFactory::ApplyStandardPenetration_12ga(USHWeaponDataAsset* Data)
{
	if (!Data) return;

	Data->PenetrationTable.Empty();
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Drywall,   18.0f, 0.70f, 0.60f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Glass,     4.0f,  0.85f, 0.85f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Wood,      6.0f,  0.45f, 0.35f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Sandbag,   2.0f,  0.20f, 0.15f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Dirt,      4.0f,  0.30f, 0.25f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Concrete,  0.3f,  0.08f, 0.05f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Steel,     0.0f,  0.00f, 0.00f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Flesh,     30.0f, 1.00f, 0.55f });
}

void USHWeaponDataFactory::ApplyStandardPenetration_40mm(USHWeaponDataAsset* Data)
{
	if (!Data) return;

	// 40mm grenades don't really "penetrate" — they explode on impact.
	// Minimal penetration values; damage comes from the explosive radius.
	Data->PenetrationTable.Empty();
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Drywall,   50.0f, 0.95f, 0.90f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Glass,     10.0f, 0.98f, 0.95f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Wood,      5.0f,  0.60f, 0.50f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Sandbag,   0.0f,  0.00f, 0.00f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Concrete,  0.0f,  0.00f, 0.00f });
	Data->PenetrationTable.Add({ ESHPenetrableMaterial::Steel,     0.0f,  0.00f, 0.00f });
}

/* -----------------------------------------------------------------------
 *  M27 IAR — Infantry Automatic Rifle (5.56x45mm NATO)
 * --------------------------------------------------------------------- */

void USHWeaponDataFactory::ApplyDefaults_M27_IAR(USHWeaponDataAsset* Data)
{
	if (!Data) return;

	Data->WeaponID            = FName(TEXT("M27_IAR"));
	Data->DisplayName         = FText::FromString(TEXT("M27 IAR"));
	Data->Category            = ESHWeaponCategory::AssaultRifle;

	Data->RoundsPerMinute     = 850.0f;
	Data->MuzzleVelocityMPS   = 940.0f;
	Data->EffectiveRangeM     = 550.0f;
	Data->MaxRangeM           = 3600.0f;
	Data->BaseDamage          = 34.0f;
	Data->DamageFalloffStartM = 100.0f;
	Data->MinDamageMultiplier = 0.22f;
	Data->HitscanRangeCm      = 2500.0f;
	Data->PelletsPerShot      = 1;

	Data->AvailableFireModes  = { ESHFireMode::Semi, ESHFireMode::Auto };
	Data->BurstCount          = 3;

	Data->MagazineCapacity    = 30;
	Data->MaxReserveAmmo      = 210;
	Data->TacticalReloadTime  = 2.0f;
	Data->EmptyReloadTime     = 2.7f;
	Data->bSingleRoundReload  = false;

	Data->RecoilPattern.VerticalRecoil      = 0.40f;
	Data->RecoilPattern.HorizontalRecoil    = 0.12f;
	Data->RecoilPattern.FirstShotMultiplier = 1.6f;
	Data->RecoilPattern.RecoveryRate        = 5.5f;
	Data->RecoilPattern.HorizontalPattern   = { 0.05f, -0.08f, 0.10f, -0.06f, 0.03f, -0.10f, 0.07f, -0.04f };

	Data->Ballistics.BallisticCoefficient = 0.304f;
	Data->Ballistics.DragCoefficient      = 0.295f;
	Data->Ballistics.BulletMassGrams      = 4.0f;
	Data->Ballistics.CrossSectionCm2      = 0.264f;

	Data->AccuracyModifiers.StandingMOA      = 2.8f;
	Data->AccuracyModifiers.CrouchingMOA     = 1.6f;
	Data->AccuracyModifiers.ProneMOA         = 0.8f;
	Data->AccuracyModifiers.MovingPenaltyMOA = 4.0f;
	Data->AccuracyModifiers.ADSMultiplier    = 0.30f;
	Data->AccuracyModifiers.SuppressionMaxMOA= 6.0f;
	Data->AccuracyModifiers.FatigueMaxMOA    = 3.0f;

	Data->ADSFOV              = 55.0f;
	Data->ADSTransitionTime   = 0.18f;

	Data->HeatConfig.HeatPerShot               = 0.012f;
	Data->HeatConfig.CooldownPerSecond         = 0.08f;
	Data->HeatConfig.MalfunctionHeatThreshold  = 0.75f;
	Data->HeatConfig.bCanOverheat              = false;

	Data->BaseMalfunctionChance = 0.0002f;
	Data->MalfunctionClearTime  = 1.5f;

	Data->TracerInterval        = 0;
	Data->MaxSwayDegrees        = 2.0f;
	Data->SwayFrequency         = 0.7f;
	Data->WeightKg              = 3.6f;

	ApplyStandardPenetration_556(Data);
}

/* -----------------------------------------------------------------------
 *  M4A1 Carbine (5.56x45mm NATO)
 * --------------------------------------------------------------------- */

void USHWeaponDataFactory::ApplyDefaults_M4A1(USHWeaponDataAsset* Data)
{
	if (!Data) return;

	Data->WeaponID            = FName(TEXT("M4A1"));
	Data->DisplayName         = FText::FromString(TEXT("M4A1 Carbine"));
	Data->Category            = ESHWeaponCategory::AssaultRifle;

	Data->RoundsPerMinute     = 700.0f;
	Data->MuzzleVelocityMPS   = 910.0f;
	Data->EffectiveRangeM     = 500.0f;
	Data->MaxRangeM           = 3600.0f;
	Data->BaseDamage          = 33.0f;
	Data->DamageFalloffStartM = 80.0f;
	Data->MinDamageMultiplier = 0.20f;
	Data->HitscanRangeCm      = 2500.0f;
	Data->PelletsPerShot      = 1;

	Data->AvailableFireModes  = { ESHFireMode::Semi, ESHFireMode::Auto };
	Data->BurstCount          = 3;

	Data->MagazineCapacity    = 30;
	Data->MaxReserveAmmo      = 210;
	Data->TacticalReloadTime  = 2.1f;
	Data->EmptyReloadTime     = 2.8f;
	Data->bSingleRoundReload  = false;

	Data->RecoilPattern.VerticalRecoil      = 0.45f;
	Data->RecoilPattern.HorizontalRecoil    = 0.15f;
	Data->RecoilPattern.FirstShotMultiplier = 1.5f;
	Data->RecoilPattern.RecoveryRate        = 5.0f;
	Data->RecoilPattern.HorizontalPattern   = { 0.06f, -0.10f, 0.08f, -0.05f, 0.09f, -0.07f };

	Data->Ballistics.BallisticCoefficient = 0.290f;
	Data->Ballistics.DragCoefficient      = 0.310f;
	Data->Ballistics.BulletMassGrams      = 4.0f;
	Data->Ballistics.CrossSectionCm2      = 0.264f;

	Data->AccuracyModifiers.StandingMOA      = 3.0f;
	Data->AccuracyModifiers.CrouchingMOA     = 1.8f;
	Data->AccuracyModifiers.ProneMOA         = 0.9f;
	Data->AccuracyModifiers.MovingPenaltyMOA = 4.2f;
	Data->AccuracyModifiers.ADSMultiplier    = 0.35f;
	Data->AccuracyModifiers.SuppressionMaxMOA= 6.0f;
	Data->AccuracyModifiers.FatigueMaxMOA    = 3.5f;

	Data->ADSFOV              = 55.0f;
	Data->ADSTransitionTime   = 0.2f;

	Data->HeatConfig.HeatPerShot               = 0.014f;
	Data->HeatConfig.CooldownPerSecond         = 0.09f;
	Data->HeatConfig.MalfunctionHeatThreshold  = 0.70f;
	Data->HeatConfig.bCanOverheat              = false;

	Data->BaseMalfunctionChance = 0.0003f;
	Data->MalfunctionClearTime  = 1.5f;

	Data->TracerInterval        = 0;
	Data->MaxSwayDegrees        = 2.2f;
	Data->SwayFrequency         = 0.7f;
	Data->WeightKg              = 3.4f;

	ApplyStandardPenetration_556(Data);
}

/* -----------------------------------------------------------------------
 *  M249 SAW (5.56x45mm NATO, belt-fed)
 * --------------------------------------------------------------------- */

void USHWeaponDataFactory::ApplyDefaults_M249_SAW(USHWeaponDataAsset* Data)
{
	if (!Data) return;

	Data->WeaponID            = FName(TEXT("M249_SAW"));
	Data->DisplayName         = FText::FromString(TEXT("M249 SAW"));
	Data->Category            = ESHWeaponCategory::SAW;

	Data->RoundsPerMinute     = 775.0f;
	Data->MuzzleVelocityMPS   = 915.0f;
	Data->EffectiveRangeM     = 800.0f;
	Data->MaxRangeM           = 3600.0f;
	Data->BaseDamage          = 34.0f;
	Data->DamageFalloffStartM = 120.0f;
	Data->MinDamageMultiplier = 0.20f;
	Data->HitscanRangeCm      = 2500.0f;
	Data->PelletsPerShot      = 1;

	Data->AvailableFireModes  = { ESHFireMode::Auto };
	Data->BurstCount          = 3;

	Data->MagazineCapacity    = 200;
	Data->MaxReserveAmmo      = 600;
	Data->TacticalReloadTime  = 5.5f;
	Data->EmptyReloadTime     = 7.0f;
	Data->bSingleRoundReload  = false;

	Data->RecoilPattern.VerticalRecoil      = 0.55f;
	Data->RecoilPattern.HorizontalRecoil    = 0.25f;
	Data->RecoilPattern.FirstShotMultiplier = 1.3f;
	Data->RecoilPattern.RecoveryRate        = 3.0f;
	Data->RecoilPattern.HorizontalPattern   = { 0.10f, -0.12f, 0.15f, -0.08f, 0.06f, -0.14f, 0.11f, -0.09f, 0.13f, -0.07f };

	Data->Ballistics.BallisticCoefficient = 0.304f;
	Data->Ballistics.DragCoefficient      = 0.295f;
	Data->Ballistics.BulletMassGrams      = 4.0f;
	Data->Ballistics.CrossSectionCm2      = 0.264f;

	Data->AccuracyModifiers.StandingMOA      = 5.0f;
	Data->AccuracyModifiers.CrouchingMOA     = 3.2f;
	Data->AccuracyModifiers.ProneMOA         = 1.5f;  // Bipod bonus
	Data->AccuracyModifiers.MovingPenaltyMOA = 7.0f;
	Data->AccuracyModifiers.ADSMultiplier    = 0.50f;
	Data->AccuracyModifiers.SuppressionMaxMOA= 5.0f;
	Data->AccuracyModifiers.FatigueMaxMOA    = 5.0f;

	Data->ADSFOV              = 60.0f;
	Data->ADSTransitionTime   = 0.35f;

	Data->HeatConfig.HeatPerShot               = 0.008f;
	Data->HeatConfig.CooldownPerSecond         = 0.05f;
	Data->HeatConfig.MalfunctionHeatThreshold  = 0.65f;
	Data->HeatConfig.bCanOverheat              = true;
	Data->HeatConfig.OverheatThreshold         = 1.0f;
	Data->HeatConfig.CooldownResumeThreshold   = 0.4f;

	Data->BaseMalfunctionChance = 0.0005f;
	Data->MalfunctionClearTime  = 2.5f;

	Data->TracerInterval        = 5; // Every 5th round
	Data->MaxSwayDegrees        = 3.5f;
	Data->SwayFrequency         = 0.5f;
	Data->WeightKg              = 7.5f;

	ApplyStandardPenetration_556(Data);
}

/* -----------------------------------------------------------------------
 *  M110 SASS (7.62x51mm NATO)
 * --------------------------------------------------------------------- */

void USHWeaponDataFactory::ApplyDefaults_M110_SASS(USHWeaponDataAsset* Data)
{
	if (!Data) return;

	Data->WeaponID            = FName(TEXT("M110_SASS"));
	Data->DisplayName         = FText::FromString(TEXT("M110 SASS"));
	Data->Category            = ESHWeaponCategory::DMR;

	Data->RoundsPerMinute     = 120.0f; // Semi-auto, practical rate
	Data->MuzzleVelocityMPS   = 783.0f;
	Data->EffectiveRangeM     = 800.0f;
	Data->MaxRangeM           = 3800.0f;
	Data->BaseDamage          = 55.0f;
	Data->DamageFalloffStartM = 200.0f;
	Data->MinDamageMultiplier = 0.30f;
	Data->HitscanRangeCm      = 2500.0f;
	Data->PelletsPerShot      = 1;

	Data->AvailableFireModes  = { ESHFireMode::Semi };
	Data->BurstCount          = 1;

	Data->MagazineCapacity    = 20;
	Data->MaxReserveAmmo      = 100;
	Data->TacticalReloadTime  = 2.5f;
	Data->EmptyReloadTime     = 3.2f;
	Data->bSingleRoundReload  = false;

	Data->RecoilPattern.VerticalRecoil      = 1.2f;
	Data->RecoilPattern.HorizontalRecoil    = 0.20f;
	Data->RecoilPattern.FirstShotMultiplier = 1.0f;
	Data->RecoilPattern.RecoveryRate        = 3.5f;
	Data->RecoilPattern.HorizontalPattern   = { 0.08f, -0.06f, 0.10f, -0.12f };

	Data->Ballistics.BallisticCoefficient = 0.462f;
	Data->Ballistics.DragCoefficient      = 0.250f;
	Data->Ballistics.BulletMassGrams      = 11.34f; // 175 gr
	Data->Ballistics.CrossSectionCm2      = 0.456f;

	Data->AccuracyModifiers.StandingMOA      = 2.5f;
	Data->AccuracyModifiers.CrouchingMOA     = 1.5f;
	Data->AccuracyModifiers.ProneMOA         = 0.6f;
	Data->AccuracyModifiers.MovingPenaltyMOA = 5.0f;
	Data->AccuracyModifiers.ADSMultiplier    = 0.20f;
	Data->AccuracyModifiers.SuppressionMaxMOA= 4.0f;
	Data->AccuracyModifiers.FatigueMaxMOA    = 4.0f;

	Data->ADSFOV              = 30.0f; // Scope
	Data->ADSTransitionTime   = 0.25f;

	Data->HeatConfig.HeatPerShot               = 0.025f;
	Data->HeatConfig.CooldownPerSecond         = 0.10f;
	Data->HeatConfig.MalfunctionHeatThreshold  = 0.80f;
	Data->HeatConfig.bCanOverheat              = false;

	Data->BaseMalfunctionChance = 0.0001f;
	Data->MalfunctionClearTime  = 1.8f;

	Data->TracerInterval        = 0;
	Data->MaxSwayDegrees        = 3.0f;
	Data->SwayFrequency         = 0.6f;
	Data->WeightKg              = 5.0f;

	ApplyStandardPenetration_762(Data);
}

/* -----------------------------------------------------------------------
 *  M17 SIG Sauer (9x19mm Parabellum)
 * --------------------------------------------------------------------- */

void USHWeaponDataFactory::ApplyDefaults_M17_SIG(USHWeaponDataAsset* Data)
{
	if (!Data) return;

	Data->WeaponID            = FName(TEXT("M17_SIG"));
	Data->DisplayName         = FText::FromString(TEXT("M17 (SIG P320)"));
	Data->Category            = ESHWeaponCategory::Sidearm;

	Data->RoundsPerMinute     = 400.0f;
	Data->MuzzleVelocityMPS   = 375.0f;
	Data->EffectiveRangeM     = 50.0f;
	Data->MaxRangeM           = 1800.0f;
	Data->BaseDamage          = 28.0f;
	Data->DamageFalloffStartM = 25.0f;
	Data->MinDamageMultiplier = 0.30f;
	Data->HitscanRangeCm      = 2000.0f;
	Data->PelletsPerShot      = 1;

	Data->AvailableFireModes  = { ESHFireMode::Semi };
	Data->BurstCount          = 1;

	Data->MagazineCapacity    = 17;
	Data->MaxReserveAmmo      = 68;
	Data->TacticalReloadTime  = 1.5f;
	Data->EmptyReloadTime     = 1.9f;
	Data->bSingleRoundReload  = false;

	Data->RecoilPattern.VerticalRecoil      = 0.8f;
	Data->RecoilPattern.HorizontalRecoil    = 0.25f;
	Data->RecoilPattern.FirstShotMultiplier = 1.0f;
	Data->RecoilPattern.RecoveryRate        = 7.0f;
	Data->RecoilPattern.HorizontalPattern   = { 0.10f, -0.12f, 0.08f, -0.06f };

	Data->Ballistics.BallisticCoefficient = 0.145f;
	Data->Ballistics.DragCoefficient      = 0.380f;
	Data->Ballistics.BulletMassGrams      = 7.45f; // 115 gr
	Data->Ballistics.CrossSectionCm2      = 0.636f;

	Data->AccuracyModifiers.StandingMOA      = 4.0f;
	Data->AccuracyModifiers.CrouchingMOA     = 3.0f;
	Data->AccuracyModifiers.ProneMOA         = 2.5f;
	Data->AccuracyModifiers.MovingPenaltyMOA = 3.0f;
	Data->AccuracyModifiers.ADSMultiplier    = 0.50f;
	Data->AccuracyModifiers.SuppressionMaxMOA= 5.0f;
	Data->AccuracyModifiers.FatigueMaxMOA    = 3.0f;

	Data->ADSFOV              = 65.0f;
	Data->ADSTransitionTime   = 0.12f;

	Data->HeatConfig.HeatPerShot               = 0.020f;
	Data->HeatConfig.CooldownPerSecond         = 0.12f;
	Data->HeatConfig.MalfunctionHeatThreshold  = 0.80f;
	Data->HeatConfig.bCanOverheat              = false;

	Data->BaseMalfunctionChance = 0.0001f;
	Data->MalfunctionClearTime  = 1.2f;

	Data->TracerInterval        = 0;
	Data->MaxSwayDegrees        = 1.5f;
	Data->SwayFrequency         = 0.9f;
	Data->WeightKg              = 0.83f;

	ApplyStandardPenetration_9mm(Data);
}

/* -----------------------------------------------------------------------
 *  M320 Grenade Launcher (40x46mm)
 * --------------------------------------------------------------------- */

void USHWeaponDataFactory::ApplyDefaults_M320_GL(USHWeaponDataAsset* Data)
{
	if (!Data) return;

	Data->WeaponID            = FName(TEXT("M320_GL"));
	Data->DisplayName         = FText::FromString(TEXT("M320 Grenade Launcher"));
	Data->Category            = ESHWeaponCategory::GrenadeLauncher;

	Data->RoundsPerMinute     = 7.0f;  // Very slow, single-shot
	Data->MuzzleVelocityMPS   = 76.0f;
	Data->EffectiveRangeM     = 350.0f;
	Data->MaxRangeM           = 400.0f;
	Data->BaseDamage          = 150.0f; // Direct hit; radial damage is separate
	Data->DamageFalloffStartM = 0.0f;
	Data->MinDamageMultiplier = 1.0f;   // No falloff for direct hit
	Data->HitscanRangeCm      = 0.0f;   // Always projectile
	Data->PelletsPerShot      = 1;

	Data->AvailableFireModes  = { ESHFireMode::Semi };
	Data->BurstCount          = 1;

	Data->MagazineCapacity    = 1;
	Data->MaxReserveAmmo      = 12;
	Data->TacticalReloadTime  = 3.0f;
	Data->EmptyReloadTime     = 3.0f;  // Same — always empty after shot
	Data->bSingleRoundReload  = false;

	Data->RecoilPattern.VerticalRecoil      = 3.0f;
	Data->RecoilPattern.HorizontalRecoil    = 0.5f;
	Data->RecoilPattern.FirstShotMultiplier = 1.0f;
	Data->RecoilPattern.RecoveryRate        = 2.0f;

	Data->Ballistics.BallisticCoefficient = 0.050f;
	Data->Ballistics.DragCoefficient      = 0.600f;
	Data->Ballistics.BulletMassGrams      = 230.0f; // ~230g grenade
	Data->Ballistics.CrossSectionCm2      = 12.57f;

	Data->AccuracyModifiers.StandingMOA      = 6.0f;
	Data->AccuracyModifiers.CrouchingMOA     = 4.0f;
	Data->AccuracyModifiers.ProneMOA         = 3.0f;
	Data->AccuracyModifiers.MovingPenaltyMOA = 8.0f;
	Data->AccuracyModifiers.ADSMultiplier    = 0.60f;
	Data->AccuracyModifiers.SuppressionMaxMOA= 8.0f;
	Data->AccuracyModifiers.FatigueMaxMOA    = 4.0f;

	Data->ADSFOV              = 60.0f;
	Data->ADSTransitionTime   = 0.30f;

	Data->HeatConfig.HeatPerShot               = 0.0f; // N/A
	Data->HeatConfig.CooldownPerSecond         = 0.0f;
	Data->HeatConfig.MalfunctionHeatThreshold  = 1.0f;
	Data->HeatConfig.bCanOverheat              = false;

	Data->BaseMalfunctionChance = 0.0001f;
	Data->MalfunctionClearTime  = 2.0f;

	Data->TracerInterval        = 0;
	Data->MaxSwayDegrees        = 3.0f;
	Data->SwayFrequency         = 0.5f;
	Data->WeightKg              = 1.5f;

	ApplyStandardPenetration_40mm(Data);
}

/* -----------------------------------------------------------------------
 *  Mossberg 590 (12 Gauge)
 * --------------------------------------------------------------------- */

void USHWeaponDataFactory::ApplyDefaults_Mossberg590(USHWeaponDataAsset* Data)
{
	if (!Data) return;

	Data->WeaponID            = FName(TEXT("Mossberg590"));
	Data->DisplayName         = FText::FromString(TEXT("Mossberg 590"));
	Data->Category            = ESHWeaponCategory::Shotgun;

	Data->RoundsPerMinute     = 80.0f;  // Pump-action
	Data->MuzzleVelocityMPS   = 400.0f;
	Data->EffectiveRangeM     = 40.0f;
	Data->MaxRangeM           = 200.0f;
	Data->BaseDamage          = 15.0f;  // Per pellet, 9 pellets = 135 total
	Data->DamageFalloffStartM = 10.0f;
	Data->MinDamageMultiplier = 0.15f;
	Data->HitscanRangeCm      = 1500.0f;
	Data->PelletsPerShot      = 9;      // 00 Buckshot

	Data->AvailableFireModes  = { ESHFireMode::Semi }; // Pump
	Data->BurstCount          = 1;

	Data->MagazineCapacity    = 8;
	Data->MaxReserveAmmo      = 32;
	Data->TacticalReloadTime  = 0.55f;  // Per shell (uses single-round reload)
	Data->EmptyReloadTime     = 0.55f;
	Data->bSingleRoundReload  = true;
	Data->SingleRoundInsertTime = 0.55f;

	Data->RecoilPattern.VerticalRecoil      = 2.5f;
	Data->RecoilPattern.HorizontalRecoil    = 0.8f;
	Data->RecoilPattern.FirstShotMultiplier = 1.0f;
	Data->RecoilPattern.RecoveryRate        = 4.0f;

	Data->Ballistics.BallisticCoefficient = 0.070f;
	Data->Ballistics.DragCoefficient      = 0.500f;
	Data->Ballistics.BulletMassGrams      = 3.5f; // Per pellet
	Data->Ballistics.CrossSectionCm2      = 0.567f;

	Data->AccuracyModifiers.StandingMOA      = 8.0f; // Pellet spread
	Data->AccuracyModifiers.CrouchingMOA     = 6.0f;
	Data->AccuracyModifiers.ProneMOA         = 5.0f;
	Data->AccuracyModifiers.MovingPenaltyMOA = 4.0f;
	Data->AccuracyModifiers.ADSMultiplier    = 0.70f;
	Data->AccuracyModifiers.SuppressionMaxMOA= 4.0f;
	Data->AccuracyModifiers.FatigueMaxMOA    = 2.0f;

	Data->ADSFOV              = 65.0f;
	Data->ADSTransitionTime   = 0.18f;

	Data->HeatConfig.HeatPerShot               = 0.0f;
	Data->HeatConfig.CooldownPerSecond         = 0.0f;
	Data->HeatConfig.MalfunctionHeatThreshold  = 1.0f;
	Data->HeatConfig.bCanOverheat              = false;

	Data->BaseMalfunctionChance = 0.0001f;
	Data->MalfunctionClearTime  = 1.8f;

	Data->TracerInterval        = 0;
	Data->MaxSwayDegrees        = 2.0f;
	Data->SwayFrequency         = 0.6f;
	Data->WeightKg              = 3.2f;

	ApplyStandardPenetration_12ga(Data);
}
