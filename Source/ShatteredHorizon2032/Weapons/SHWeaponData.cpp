// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "Weapons/SHWeaponData.h"

// ---------------------------------------------------------------------------
// USHWeaponData helpers
// ---------------------------------------------------------------------------

float USHWeaponData::GetMOAForStance(ESHStance Stance) const
{
	switch (Stance)
	{
	case ESHStance::Standing:  return Accuracy.StandingMOA;
	case ESHStance::Crouching: return Accuracy.CrouchingMOA;
	case ESHStance::Prone:     return Accuracy.ProneMOA;
	case ESHStance::Moving:    return Accuracy.MovingMOA;
	default:                   return Accuracy.StandingMOA;
	}
}

const FSHPenetrationEntry* USHWeaponData::FindPenetration(ESHPenetrationMaterial Material) const
{
	for (const FSHPenetrationEntry& Entry : PenetrationTable)
	{
		if (Entry.Material == Material)
		{
			return &Entry;
		}
	}
	return nullptr;
}

// ---------------------------------------------------------------------------
// Penetration table helpers
// ---------------------------------------------------------------------------

TArray<FSHPenetrationEntry> USHWeaponDataLibrary::MakeStandard556PenTable()
{
	TArray<FSHPenetrationEntry> Table;

	Table.Add({ ESHPenetrationMaterial::Drywall,  30.0f, 0.85f, 0.80f });
	Table.Add({ ESHPenetrationMaterial::Wood,     15.0f, 0.65f, 0.55f });
	Table.Add({ ESHPenetrationMaterial::Glass,    2.0f,  0.92f, 0.90f });
	Table.Add({ ESHPenetrationMaterial::Sandbag,  10.0f, 0.40f, 0.30f });
	Table.Add({ ESHPenetrationMaterial::Dirt,     25.0f, 0.35f, 0.25f });
	Table.Add({ ESHPenetrationMaterial::Concrete, 3.0f,  0.30f, 0.20f });
	Table.Add({ ESHPenetrationMaterial::Steel,    0.5f,  0.15f, 0.10f });
	Table.Add({ ESHPenetrationMaterial::Flesh,    40.0f, 0.60f, 0.50f });

	return Table;
}

TArray<FSHPenetrationEntry> USHWeaponDataLibrary::MakeStandard762PenTable()
{
	TArray<FSHPenetrationEntry> Table;

	Table.Add({ ESHPenetrationMaterial::Drywall,  45.0f, 0.90f, 0.85f });
	Table.Add({ ESHPenetrationMaterial::Wood,     25.0f, 0.70f, 0.60f });
	Table.Add({ ESHPenetrationMaterial::Glass,    3.0f,  0.95f, 0.92f });
	Table.Add({ ESHPenetrationMaterial::Sandbag,  18.0f, 0.50f, 0.40f });
	Table.Add({ ESHPenetrationMaterial::Dirt,     35.0f, 0.40f, 0.30f });
	Table.Add({ ESHPenetrationMaterial::Concrete, 5.0f,  0.35f, 0.25f });
	Table.Add({ ESHPenetrationMaterial::Steel,    1.2f,  0.20f, 0.15f });
	Table.Add({ ESHPenetrationMaterial::Flesh,    55.0f, 0.55f, 0.45f });

	return Table;
}

TArray<FSHPenetrationEntry> USHWeaponDataLibrary::MakeStandard9mmPenTable()
{
	TArray<FSHPenetrationEntry> Table;

	Table.Add({ ESHPenetrationMaterial::Drywall,  20.0f, 0.80f, 0.75f });
	Table.Add({ ESHPenetrationMaterial::Wood,     8.0f,  0.55f, 0.45f });
	Table.Add({ ESHPenetrationMaterial::Glass,    1.5f,  0.88f, 0.85f });
	Table.Add({ ESHPenetrationMaterial::Sandbag,  5.0f,  0.25f, 0.20f });
	Table.Add({ ESHPenetrationMaterial::Dirt,     15.0f, 0.20f, 0.15f });
	Table.Add({ ESHPenetrationMaterial::Concrete, 0.5f,  0.10f, 0.08f });
	Table.Add({ ESHPenetrationMaterial::Steel,    0.1f,  0.05f, 0.03f });
	Table.Add({ ESHPenetrationMaterial::Flesh,    30.0f, 0.65f, 0.55f });

	return Table;
}

TArray<FSHPenetrationEntry> USHWeaponDataLibrary::MakeStandard12GaugePenTable()
{
	TArray<FSHPenetrationEntry> Table;

	// Buckshot — lower penetration per pellet
	Table.Add({ ESHPenetrationMaterial::Drywall,  12.0f, 0.70f, 0.60f });
	Table.Add({ ESHPenetrationMaterial::Wood,     5.0f,  0.40f, 0.30f });
	Table.Add({ ESHPenetrationMaterial::Glass,    1.0f,  0.85f, 0.80f });
	Table.Add({ ESHPenetrationMaterial::Sandbag,  2.0f,  0.15f, 0.10f });
	Table.Add({ ESHPenetrationMaterial::Dirt,     8.0f,  0.10f, 0.08f });
	Table.Add({ ESHPenetrationMaterial::Concrete, 0.2f,  0.05f, 0.03f });
	Table.Add({ ESHPenetrationMaterial::Steel,    0.0f,  0.00f, 0.00f }); // no pen
	Table.Add({ ESHPenetrationMaterial::Flesh,    25.0f, 0.50f, 0.40f });

	return Table;
}

TArray<FSHPenetrationEntry> USHWeaponDataLibrary::MakeStandard40mmPenTable()
{
	TArray<FSHPenetrationEntry> Table;

	// 40mm HE — minimal penetration, damage is from explosion
	Table.Add({ ESHPenetrationMaterial::Drywall,  5.0f,  0.90f, 0.85f });
	Table.Add({ ESHPenetrationMaterial::Wood,     3.0f,  0.80f, 0.70f });
	Table.Add({ ESHPenetrationMaterial::Glass,    1.0f,  0.95f, 0.90f });
	Table.Add({ ESHPenetrationMaterial::Sandbag,  0.0f,  0.00f, 0.00f });
	Table.Add({ ESHPenetrationMaterial::Concrete, 0.0f,  0.00f, 0.00f });
	Table.Add({ ESHPenetrationMaterial::Steel,    0.0f,  0.00f, 0.00f });

	return Table;
}

// ---------------------------------------------------------------------------
// Weapon presets
// ---------------------------------------------------------------------------

void USHWeaponDataLibrary::ApplyPreset_M27IAR(USHWeaponData* Data)
{
	if (!Data) return;

	Data->WeaponID          = FName(TEXT("M27_IAR"));
	Data->DisplayName       = FText::FromString(TEXT("M27 IAR"));
	Data->Category          = ESHWeaponCategory::AssaultRifle;

	Data->FireRateRPM       = 700.0f;
	Data->MuzzleVelocityMPS = 940.0f;
	Data->EffectiveRangeM   = 550.0f;
	Data->MaxRangeM         = 3600.0f;
	Data->HitscanThresholdM = 50.0f;
	Data->AvailableFireModes = { ESHFireMode::Semi, ESHFireMode::Auto };
	Data->BurstCount        = 3;

	Data->BaseDamage            = 36.0f;
	Data->DamageFalloffAtRange  = 0.50f;
	Data->PelletCount           = 1;
	Data->bIsExplosive          = false;

	Data->Ballistics.BallisticCoefficient = 0.304f;
	Data->Ballistics.DragCoefficient      = 0.295f;
	Data->Ballistics.BulletMassGrains     = 62.0f;
	Data->Ballistics.CrossSectionArea     = 0.257f;

	Data->PenetrationTable = MakeStandard556PenTable();

	Data->Recoil.VerticalRecoil      = 0.42f;
	Data->Recoil.HorizontalRecoil    = 0.12f;
	Data->Recoil.FirstShotMultiplier = 1.4f;
	Data->Recoil.RecoveryRate        = 5.5f;
	Data->Recoil.MaxVerticalRecoil   = 7.0f;
	Data->Recoil.HorizontalBias     = 0.05f;

	Data->Accuracy.StandingMOA  = 3.8f;
	Data->Accuracy.CrouchingMOA = 2.2f;
	Data->Accuracy.ProneMOA     = 1.0f;
	Data->Accuracy.MovingMOA    = 6.5f;
	Data->Accuracy.ADSMultiplier = 0.32f;
	Data->Accuracy.SuppressionSpreadPerPoint = 0.04f;
	Data->Accuracy.FatigueSpreadPerPoint     = 0.02f;

	Data->MagazineCapacity   = 30;
	Data->MaxReserveAmmo     = 210;
	Data->TacticalReloadTime = 2.0f;
	Data->EmptyReloadTime    = 2.7f;

	Data->BaseMalfunctionChance      = 0.0001f;
	Data->SustainedFireMalfunctionRate = 0.00004f;
	Data->DirtMalfunctionRate        = 0.0001f;
	Data->MalfunctionClearTime       = 2.8f;

	Data->HeatPerShot        = 0.010f;
	Data->HeatDissipationRate = 0.09f;
	Data->OverheatThreshold  = 1.0f;

	Data->ADSTime          = 0.22f;
	Data->ADSFOVMultiplier = 0.75f;

	Data->BaseSwayAmplitude      = 0.5f;
	Data->SwayFrequency          = 1.1f;
	Data->ExhaustedSwayMultiplier = 2.8f;

	Data->TracerInterval = 0;
}

void USHWeaponDataLibrary::ApplyPreset_M4A1(USHWeaponData* Data)
{
	if (!Data) return;

	Data->WeaponID          = FName(TEXT("M4A1"));
	Data->DisplayName       = FText::FromString(TEXT("M4A1"));
	Data->Category          = ESHWeaponCategory::AssaultRifle;

	Data->FireRateRPM       = 850.0f;
	Data->MuzzleVelocityMPS = 910.0f;
	Data->EffectiveRangeM   = 500.0f;
	Data->MaxRangeM         = 3600.0f;
	Data->HitscanThresholdM = 50.0f;
	Data->AvailableFireModes = { ESHFireMode::Semi, ESHFireMode::Auto };
	Data->BurstCount        = 3;

	Data->BaseDamage            = 34.0f;
	Data->DamageFalloffAtRange  = 0.52f;
	Data->PelletCount           = 1;
	Data->bIsExplosive          = false;

	Data->Ballistics.BallisticCoefficient = 0.304f;
	Data->Ballistics.DragCoefficient      = 0.295f;
	Data->Ballistics.BulletMassGrains     = 62.0f;
	Data->Ballistics.CrossSectionArea     = 0.257f;

	Data->PenetrationTable = MakeStandard556PenTable();

	Data->Recoil.VerticalRecoil      = 0.48f;
	Data->Recoil.HorizontalRecoil    = 0.16f;
	Data->Recoil.FirstShotMultiplier = 1.5f;
	Data->Recoil.RecoveryRate        = 5.0f;
	Data->Recoil.MaxVerticalRecoil   = 8.0f;
	Data->Recoil.HorizontalBias     = -0.03f;

	Data->Accuracy.StandingMOA  = 4.0f;
	Data->Accuracy.CrouchingMOA = 2.5f;
	Data->Accuracy.ProneMOA     = 1.2f;
	Data->Accuracy.MovingMOA    = 7.0f;
	Data->Accuracy.ADSMultiplier = 0.35f;
	Data->Accuracy.SuppressionSpreadPerPoint = 0.04f;
	Data->Accuracy.FatigueSpreadPerPoint     = 0.02f;

	Data->MagazineCapacity   = 30;
	Data->MaxReserveAmmo     = 210;
	Data->TacticalReloadTime = 2.1f;
	Data->EmptyReloadTime    = 2.8f;

	Data->BaseMalfunctionChance        = 0.0002f;
	Data->SustainedFireMalfunctionRate = 0.00005f;
	Data->DirtMalfunctionRate          = 0.00012f;
	Data->MalfunctionClearTime         = 3.0f;

	Data->HeatPerShot         = 0.012f;
	Data->HeatDissipationRate = 0.08f;
	Data->OverheatThreshold   = 1.0f;

	Data->ADSTime          = 0.20f;
	Data->ADSFOVMultiplier = 0.75f;

	Data->BaseSwayAmplitude       = 0.55f;
	Data->SwayFrequency           = 1.2f;
	Data->ExhaustedSwayMultiplier = 3.0f;

	Data->TracerInterval = 0;
}

void USHWeaponDataLibrary::ApplyPreset_M249SAW(USHWeaponData* Data)
{
	if (!Data) return;

	Data->WeaponID          = FName(TEXT("M249_SAW"));
	Data->DisplayName       = FText::FromString(TEXT("M249 SAW"));
	Data->Category          = ESHWeaponCategory::SAW;

	Data->FireRateRPM       = 800.0f;
	Data->MuzzleVelocityMPS = 915.0f;
	Data->EffectiveRangeM   = 800.0f;
	Data->MaxRangeM         = 3600.0f;
	Data->HitscanThresholdM = 50.0f;
	Data->AvailableFireModes = { ESHFireMode::Auto };
	Data->BurstCount        = 3;

	Data->BaseDamage            = 35.0f;
	Data->DamageFalloffAtRange  = 0.48f;
	Data->PelletCount           = 1;
	Data->bIsExplosive          = false;

	Data->Ballistics.BallisticCoefficient = 0.304f;
	Data->Ballistics.DragCoefficient      = 0.295f;
	Data->Ballistics.BulletMassGrains     = 62.0f;
	Data->Ballistics.CrossSectionArea     = 0.257f;

	Data->PenetrationTable = MakeStandard556PenTable();

	Data->Recoil.VerticalRecoil      = 0.55f;
	Data->Recoil.HorizontalRecoil    = 0.25f;
	Data->Recoil.FirstShotMultiplier = 1.3f;
	Data->Recoil.RecoveryRate        = 3.5f;
	Data->Recoil.MaxVerticalRecoil   = 10.0f;
	Data->Recoil.HorizontalBias     = 0.0f;

	Data->Accuracy.StandingMOA  = 6.0f;
	Data->Accuracy.CrouchingMOA = 4.0f;
	Data->Accuracy.ProneMOA     = 1.8f; // bipod assumed
	Data->Accuracy.MovingMOA    = 10.0f;
	Data->Accuracy.ADSMultiplier = 0.45f;
	Data->Accuracy.SuppressionSpreadPerPoint = 0.03f;
	Data->Accuracy.FatigueSpreadPerPoint     = 0.03f;

	Data->MagazineCapacity   = 200;
	Data->MaxReserveAmmo     = 600;
	Data->TacticalReloadTime = 5.5f;
	Data->EmptyReloadTime    = 7.0f;

	Data->BaseMalfunctionChance        = 0.0003f;
	Data->SustainedFireMalfunctionRate = 0.00008f;
	Data->DirtMalfunctionRate          = 0.00015f;
	Data->MalfunctionClearTime         = 4.0f;

	Data->HeatPerShot         = 0.008f;
	Data->HeatDissipationRate = 0.05f;
	Data->OverheatThreshold   = 1.0f;

	Data->ADSTime          = 0.35f;
	Data->ADSFOVMultiplier = 0.80f;

	Data->BaseSwayAmplitude       = 1.0f;
	Data->SwayFrequency           = 0.9f;
	Data->ExhaustedSwayMultiplier = 3.5f;

	Data->TracerInterval = 5; // every 5th round is tracer
}

void USHWeaponDataLibrary::ApplyPreset_M110SASS(USHWeaponData* Data)
{
	if (!Data) return;

	Data->WeaponID          = FName(TEXT("M110_SASS"));
	Data->DisplayName       = FText::FromString(TEXT("M110 SASS"));
	Data->Category          = ESHWeaponCategory::DMR;

	Data->FireRateRPM       = 120.0f;
	Data->MuzzleVelocityMPS = 783.0f;
	Data->EffectiveRangeM   = 800.0f;
	Data->MaxRangeM         = 3800.0f;
	Data->HitscanThresholdM = 60.0f;
	Data->AvailableFireModes = { ESHFireMode::Semi };
	Data->BurstCount        = 2;

	Data->BaseDamage            = 55.0f;
	Data->DamageFalloffAtRange  = 0.60f;
	Data->PelletCount           = 1;
	Data->bIsExplosive          = false;

	Data->Ballistics.BallisticCoefficient = 0.462f;
	Data->Ballistics.DragCoefficient      = 0.250f;
	Data->Ballistics.BulletMassGrains     = 175.0f;
	Data->Ballistics.CrossSectionArea     = 0.470f;

	Data->PenetrationTable = MakeStandard762PenTable();

	Data->Recoil.VerticalRecoil      = 0.80f;
	Data->Recoil.HorizontalRecoil    = 0.10f;
	Data->Recoil.FirstShotMultiplier = 1.0f;
	Data->Recoil.RecoveryRate        = 4.0f;
	Data->Recoil.MaxVerticalRecoil   = 6.0f;
	Data->Recoil.HorizontalBias     = 0.0f;

	Data->Accuracy.StandingMOA  = 3.0f;
	Data->Accuracy.CrouchingMOA = 1.5f;
	Data->Accuracy.ProneMOA     = 0.7f;
	Data->Accuracy.MovingMOA    = 8.0f;
	Data->Accuracy.ADSMultiplier = 0.20f;
	Data->Accuracy.SuppressionSpreadPerPoint = 0.05f;
	Data->Accuracy.FatigueSpreadPerPoint     = 0.03f;

	Data->MagazineCapacity   = 20;
	Data->MaxReserveAmmo     = 100;
	Data->TacticalReloadTime = 2.5f;
	Data->EmptyReloadTime    = 3.2f;

	Data->BaseMalfunctionChance        = 0.0001f;
	Data->SustainedFireMalfunctionRate = 0.00003f;
	Data->DirtMalfunctionRate          = 0.0001f;
	Data->MalfunctionClearTime         = 3.0f;

	Data->HeatPerShot         = 0.020f;
	Data->HeatDissipationRate = 0.10f;
	Data->OverheatThreshold   = 1.0f;

	Data->ADSTime          = 0.30f;
	Data->ADSFOVMultiplier = 0.45f; // scope zoom

	Data->BaseSwayAmplitude       = 0.8f;
	Data->SwayFrequency           = 1.0f;
	Data->ExhaustedSwayMultiplier = 3.5f;

	Data->TracerInterval = 0;
}

void USHWeaponDataLibrary::ApplyPreset_M17SIG(USHWeaponData* Data)
{
	if (!Data) return;

	Data->WeaponID          = FName(TEXT("M17_SIG"));
	Data->DisplayName       = FText::FromString(TEXT("M17 (SIG P320)"));
	Data->Category          = ESHWeaponCategory::Sidearm;

	Data->FireRateRPM       = 400.0f;
	Data->MuzzleVelocityMPS = 375.0f;
	Data->EffectiveRangeM   = 50.0f;
	Data->MaxRangeM         = 1800.0f;
	Data->HitscanThresholdM = 30.0f;
	Data->AvailableFireModes = { ESHFireMode::Semi };
	Data->BurstCount        = 2;

	Data->BaseDamage            = 28.0f;
	Data->DamageFalloffAtRange  = 0.45f;
	Data->PelletCount           = 1;
	Data->bIsExplosive          = false;

	Data->Ballistics.BallisticCoefficient = 0.165f;
	Data->Ballistics.DragCoefficient      = 0.380f;
	Data->Ballistics.BulletMassGrains     = 124.0f;
	Data->Ballistics.CrossSectionArea     = 0.636f;

	Data->PenetrationTable = MakeStandard9mmPenTable();

	Data->Recoil.VerticalRecoil      = 0.65f;
	Data->Recoil.HorizontalRecoil    = 0.20f;
	Data->Recoil.FirstShotMultiplier = 1.2f;
	Data->Recoil.RecoveryRate        = 6.0f;
	Data->Recoil.MaxVerticalRecoil   = 9.0f;
	Data->Recoil.HorizontalBias     = 0.0f;

	Data->Accuracy.StandingMOA  = 5.0f;
	Data->Accuracy.CrouchingMOA = 3.5f;
	Data->Accuracy.ProneMOA     = 2.5f;
	Data->Accuracy.MovingMOA    = 8.0f;
	Data->Accuracy.ADSMultiplier = 0.50f;
	Data->Accuracy.SuppressionSpreadPerPoint = 0.05f;
	Data->Accuracy.FatigueSpreadPerPoint     = 0.03f;

	Data->MagazineCapacity   = 17;
	Data->MaxReserveAmmo     = 68;
	Data->TacticalReloadTime = 1.5f;
	Data->EmptyReloadTime    = 2.0f;

	Data->BaseMalfunctionChance        = 0.0001f;
	Data->SustainedFireMalfunctionRate = 0.00002f;
	Data->DirtMalfunctionRate          = 0.00008f;
	Data->MalfunctionClearTime         = 2.5f;

	Data->HeatPerShot         = 0.015f;
	Data->HeatDissipationRate = 0.12f;
	Data->OverheatThreshold   = 1.0f;

	Data->ADSTime          = 0.15f;
	Data->ADSFOVMultiplier = 0.85f;

	Data->BaseSwayAmplitude       = 0.7f;
	Data->SwayFrequency           = 1.5f;
	Data->ExhaustedSwayMultiplier = 2.5f;

	Data->TracerInterval = 0;
}

void USHWeaponDataLibrary::ApplyPreset_M320GL(USHWeaponData* Data)
{
	if (!Data) return;

	Data->WeaponID          = FName(TEXT("M320_GL"));
	Data->DisplayName       = FText::FromString(TEXT("M320 Grenade Launcher"));
	Data->Category          = ESHWeaponCategory::GrenadeLauncher;

	Data->FireRateRPM       = 12.0f;
	Data->MuzzleVelocityMPS = 76.0f;
	Data->EffectiveRangeM   = 350.0f;
	Data->MaxRangeM         = 400.0f;
	Data->HitscanThresholdM = 0.0f; // always projectile
	Data->AvailableFireModes = { ESHFireMode::Semi };
	Data->BurstCount        = 2;

	Data->BaseDamage            = 150.0f;
	Data->DamageFalloffAtRange  = 0.90f;
	Data->PelletCount           = 1;
	Data->bIsExplosive          = true;
	Data->ExplosiveInnerRadiusM = 3.0f;
	Data->ExplosiveOuterRadiusM = 15.0f;
	Data->ShrapnelCount         = 30;
	Data->ShrapnelDamage        = 22.0f;

	Data->Ballistics.BallisticCoefficient = 0.035f;
	Data->Ballistics.DragCoefficient      = 0.800f;
	Data->Ballistics.BulletMassGrains     = 3200.0f; // ~207g
	Data->Ballistics.CrossSectionArea     = 12.56f;  // 40mm diameter

	Data->PenetrationTable = MakeStandard40mmPenTable();

	Data->Recoil.VerticalRecoil      = 2.0f;
	Data->Recoil.HorizontalRecoil    = 0.3f;
	Data->Recoil.FirstShotMultiplier = 1.0f;
	Data->Recoil.RecoveryRate        = 2.0f;
	Data->Recoil.MaxVerticalRecoil   = 5.0f;
	Data->Recoil.HorizontalBias     = 0.0f;

	Data->Accuracy.StandingMOA  = 6.0f;
	Data->Accuracy.CrouchingMOA = 4.0f;
	Data->Accuracy.ProneMOA     = 3.0f;
	Data->Accuracy.MovingMOA    = 12.0f;
	Data->Accuracy.ADSMultiplier = 0.60f;
	Data->Accuracy.SuppressionSpreadPerPoint = 0.06f;
	Data->Accuracy.FatigueSpreadPerPoint     = 0.04f;

	Data->MagazineCapacity   = 1;
	Data->MaxReserveAmmo     = 8;
	Data->TacticalReloadTime = 3.5f;
	Data->EmptyReloadTime    = 3.5f; // always same — single-shot

	Data->BaseMalfunctionChance        = 0.0001f;
	Data->SustainedFireMalfunctionRate = 0.0f;
	Data->DirtMalfunctionRate          = 0.0002f;
	Data->MalfunctionClearTime         = 4.0f;

	Data->HeatPerShot         = 0.0f;
	Data->HeatDissipationRate = 0.0f;
	Data->OverheatThreshold   = 1.0f;

	Data->ADSTime          = 0.35f;
	Data->ADSFOVMultiplier = 0.85f;

	Data->BaseSwayAmplitude       = 1.2f;
	Data->SwayFrequency           = 0.8f;
	Data->ExhaustedSwayMultiplier = 3.0f;

	Data->TracerInterval = 0;
}

void USHWeaponDataLibrary::ApplyPreset_Mossberg590(USHWeaponData* Data)
{
	if (!Data) return;

	Data->WeaponID          = FName(TEXT("Mossberg_590"));
	Data->DisplayName       = FText::FromString(TEXT("Mossberg 590"));
	Data->Category          = ESHWeaponCategory::Shotgun;

	Data->FireRateRPM       = 80.0f;
	Data->MuzzleVelocityMPS = 400.0f;
	Data->EffectiveRangeM   = 40.0f;
	Data->MaxRangeM         = 200.0f;
	Data->HitscanThresholdM = 15.0f;
	Data->AvailableFireModes = { ESHFireMode::Semi }; // pump-action
	Data->BurstCount        = 2;

	Data->BaseDamage            = 15.0f; // per pellet
	Data->DamageFalloffAtRange  = 0.25f;
	Data->PelletCount           = 9; // 00 buckshot
	Data->bIsExplosive          = false;

	Data->Ballistics.BallisticCoefficient = 0.040f;
	Data->Ballistics.DragCoefficient      = 0.700f;
	Data->Ballistics.BulletMassGrains     = 54.0f; // per pellet
	Data->Ballistics.CrossSectionArea     = 0.573f;

	Data->PenetrationTable = MakeStandard12GaugePenTable();

	Data->Recoil.VerticalRecoil      = 1.8f;
	Data->Recoil.HorizontalRecoil    = 0.4f;
	Data->Recoil.FirstShotMultiplier = 1.0f;
	Data->Recoil.RecoveryRate        = 3.0f;
	Data->Recoil.MaxVerticalRecoil   = 6.0f;
	Data->Recoil.HorizontalBias     = 0.0f;

	Data->Accuracy.StandingMOA  = 8.0f;  // pellet spread
	Data->Accuracy.CrouchingMOA = 6.0f;
	Data->Accuracy.ProneMOA     = 5.0f;
	Data->Accuracy.MovingMOA    = 12.0f;
	Data->Accuracy.ADSMultiplier = 0.70f;
	Data->Accuracy.SuppressionSpreadPerPoint = 0.03f;
	Data->Accuracy.FatigueSpreadPerPoint     = 0.02f;

	Data->MagazineCapacity   = 8;
	Data->MaxReserveAmmo     = 32;
	Data->TacticalReloadTime = 0.6f; // per shell
	Data->EmptyReloadTime    = 0.6f;

	Data->BaseMalfunctionChance        = 0.0001f;
	Data->SustainedFireMalfunctionRate = 0.00001f;
	Data->DirtMalfunctionRate          = 0.00015f;
	Data->MalfunctionClearTime         = 3.5f;

	Data->HeatPerShot         = 0.025f;
	Data->HeatDissipationRate = 0.10f;
	Data->OverheatThreshold   = 1.0f;

	Data->ADSTime          = 0.25f;
	Data->ADSFOVMultiplier = 0.85f;

	Data->BaseSwayAmplitude       = 0.8f;
	Data->SwayFrequency           = 1.0f;
	Data->ExhaustedSwayMultiplier = 2.5f;

	Data->TracerInterval = 0;
}
