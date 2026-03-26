// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHWeaponAttachmentSystem.h"

USHWeaponAttachmentSystem::USHWeaponAttachmentSystem()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// =====================================================================
//  Attachment management
// =====================================================================

bool USHWeaponAttachmentSystem::EquipAttachment(const FSHWeaponAttachment& Attachment)
{
	// Replace existing attachment in the same slot.
	EquippedAttachments.Add(Attachment.Slot, Attachment);
	RecalculateModifiers();
	OnAttachmentChanged.Broadcast(Attachment.Slot, Attachment.AttachmentID);
	return true;
}

bool USHWeaponAttachmentSystem::RemoveAttachment(ESHAttachmentSlot Slot)
{
	if (EquippedAttachments.Remove(Slot) > 0)
	{
		RecalculateModifiers();
		OnAttachmentChanged.Broadcast(Slot, NAME_None);
		return true;
	}
	return false;
}

void USHWeaponAttachmentSystem::RemoveAllAttachments()
{
	EquippedAttachments.Empty();
	RecalculateModifiers();
}

bool USHWeaponAttachmentSystem::HasAttachment(ESHAttachmentSlot Slot) const
{
	return EquippedAttachments.Contains(Slot);
}

bool USHWeaponAttachmentSystem::GetAttachment(ESHAttachmentSlot Slot, FSHWeaponAttachment& OutAttachment) const
{
	if (const FSHWeaponAttachment* Found = EquippedAttachments.Find(Slot))
	{
		OutAttachment = *Found;
		return true;
	}
	return false;
}

// =====================================================================
//  Modifier queries
// =====================================================================

FSHAttachmentModifiers USHWeaponAttachmentSystem::GetAggregateModifiers() const
{
	return CachedModifiers;
}

bool USHWeaponAttachmentSystem::IsSuppressed() const
{
	if (const FSHWeaponAttachment* Muzzle = EquippedAttachments.Find(ESHAttachmentSlot::Muzzle))
	{
		return Muzzle->SoundVolumeMultiplier < 0.5f; // Anything that cuts volume by more than half.
	}
	return false;
}

bool USHWeaponAttachmentSystem::HasBipod() const
{
	return EquippedAttachments.Contains(ESHAttachmentSlot::Bipod);
}

float USHWeaponAttachmentSystem::GetOpticMagnification() const
{
	if (const FSHWeaponAttachment* Optic = EquippedAttachments.Find(ESHAttachmentSlot::Optic))
	{
		return Optic->Magnification;
	}
	return 0.0f;
}

// =====================================================================
//  Modifier calculation
// =====================================================================

void USHWeaponAttachmentSystem::RecalculateModifiers()
{
	FSHAttachmentModifiers Mods;

	for (const auto& Pair : EquippedAttachments)
	{
		const FSHWeaponAttachment& Att = Pair.Value;

		Mods.TotalWeightKg += Att.WeightKg;
		Mods.MOAMultiplier *= Att.MOAMultiplier;
		Mods.ADSAccuracyMultiplier *= Att.ADSAccuracyMultiplier;
		Mods.VerticalRecoilMultiplier *= Att.VerticalRecoilMultiplier;
		Mods.HorizontalRecoilMultiplier *= Att.HorizontalRecoilMultiplier;
		Mods.ADSSpeedMultiplier *= Att.ADSSpeedMultiplier;
		Mods.SoundVolumeMultiplier *= Att.SoundVolumeMultiplier;
		Mods.SoundDetectionRangeMultiplier *= Att.SoundDetectionRangeMultiplier;
		Mods.MuzzleVelocityDelta += Att.MuzzleVelocityDelta;

		if (Att.Magnification > Mods.Magnification)
		{
			Mods.Magnification = Att.Magnification;
			Mods.OpticFOV = Att.OpticFOV;
		}
	}

	CachedModifiers = Mods;
}

// =====================================================================
//  Default attachment database — doctrine-accurate specifications
// =====================================================================

FSHWeaponAttachment USHAttachmentDatabase::Create_ACOG_4x()
{
	FSHWeaponAttachment A;
	A.AttachmentID = FName(TEXT("ACOG_4x"));
	A.DisplayName = FText::FromString(TEXT("Trijicon ACOG 4x32"));
	A.Slot = ESHAttachmentSlot::Optic;
	A.WeightKg = 0.30f;
	A.Magnification = 4.0f;
	A.OpticFOV = 36.8f; // ACOG: 36.8 degree FOV
	A.ADSSpeedMultiplier = 1.1f; // Slightly slower ADS with magnified optic
	A.MountSocket = FName(TEXT("OpticMount"));
	return A;
}

FSHWeaponAttachment USHAttachmentDatabase::Create_LPVO_1_8x()
{
	FSHWeaponAttachment A;
	A.AttachmentID = FName(TEXT("LPVO_1_8x"));
	A.DisplayName = FText::FromString(TEXT("Vortex Razor Gen III 1-8x"));
	A.Slot = ESHAttachmentSlot::Optic;
	A.WeightKg = 0.62f; // Heavier than ACOG
	A.Magnification = 8.0f;
	A.OpticFOV = 14.0f; // At 8x
	A.ADSSpeedMultiplier = 1.15f;
	A.MountSocket = FName(TEXT("OpticMount"));
	return A;
}

FSHWeaponAttachment USHAttachmentDatabase::Create_Leupold_10x()
{
	FSHWeaponAttachment A;
	A.AttachmentID = FName(TEXT("Leupold_10x"));
	A.DisplayName = FText::FromString(TEXT("Leupold Mark 4 10x"));
	A.Slot = ESHAttachmentSlot::Optic;
	A.WeightKg = 0.66f;
	A.Magnification = 10.0f;
	A.OpticFOV = 11.1f;
	A.ADSSpeedMultiplier = 1.25f; // High-power scope slows ADS significantly
	A.MountSocket = FName(TEXT("OpticMount"));
	return A;
}

FSHWeaponAttachment USHAttachmentDatabase::Create_RedDot_Aimpoint()
{
	FSHWeaponAttachment A;
	A.AttachmentID = FName(TEXT("Aimpoint_CompM5"));
	A.DisplayName = FText::FromString(TEXT("Aimpoint CompM5"));
	A.Slot = ESHAttachmentSlot::Optic;
	A.WeightKg = 0.15f;
	A.Magnification = 1.0f;
	A.OpticFOV = 0.0f; // 1x = no FOV change
	A.ADSSpeedMultiplier = 0.92f; // Red dots are FASTER to ADS
	A.ADSAccuracyMultiplier = 0.85f; // Better target acquisition
	A.MountSocket = FName(TEXT("OpticMount"));
	return A;
}

FSHWeaponAttachment USHAttachmentDatabase::Create_Holographic_EOTech()
{
	FSHWeaponAttachment A;
	A.AttachmentID = FName(TEXT("EOTech_EXPS3"));
	A.DisplayName = FText::FromString(TEXT("EOTech EXPS3-0"));
	A.Slot = ESHAttachmentSlot::Optic;
	A.WeightKg = 0.32f;
	A.Magnification = 1.0f;
	A.OpticFOV = 0.0f;
	A.ADSSpeedMultiplier = 0.90f;
	A.ADSAccuracyMultiplier = 0.82f;
	A.MountSocket = FName(TEXT("OpticMount"));
	return A;
}

FSHWeaponAttachment USHAttachmentDatabase::Create_Suppressor_556()
{
	FSHWeaponAttachment A;
	A.AttachmentID = FName(TEXT("Suppressor_556"));
	A.DisplayName = FText::FromString(TEXT("KAC QDC 5.56mm Suppressor"));
	A.Slot = ESHAttachmentSlot::Muzzle;
	A.WeightKg = 0.52f;
	A.SoundVolumeMultiplier = 0.15f;        // ~85% sound reduction
	A.SoundDetectionRangeMultiplier = 0.20f; // AI hears at 20% normal range
	A.MuzzleVelocityDelta = -15.0f;          // Slight velocity loss
	A.VerticalRecoilMultiplier = 0.90f;      // Suppressors reduce felt recoil slightly
	A.ADSSpeedMultiplier = 1.05f;            // Slightly front-heavy
	A.MountSocket = FName(TEXT("MuzzleMount"));
	return A;
}

FSHWeaponAttachment USHAttachmentDatabase::Create_Suppressor_762()
{
	FSHWeaponAttachment A;
	A.AttachmentID = FName(TEXT("Suppressor_762"));
	A.DisplayName = FText::FromString(TEXT("KAC QDC 7.62mm Suppressor"));
	A.Slot = ESHAttachmentSlot::Muzzle;
	A.WeightKg = 0.68f;
	A.SoundVolumeMultiplier = 0.18f;
	A.SoundDetectionRangeMultiplier = 0.25f;
	A.MuzzleVelocityDelta = -20.0f;
	A.VerticalRecoilMultiplier = 0.88f;
	A.ADSSpeedMultiplier = 1.08f;
	A.MountSocket = FName(TEXT("MuzzleMount"));
	return A;
}

FSHWeaponAttachment USHAttachmentDatabase::Create_FlashHider()
{
	FSHWeaponAttachment A;
	A.AttachmentID = FName(TEXT("FlashHider"));
	A.DisplayName = FText::FromString(TEXT("A2 Flash Hider"));
	A.Slot = ESHAttachmentSlot::Muzzle;
	A.WeightKg = 0.07f;
	A.SoundVolumeMultiplier = 0.95f; // Minimal sound reduction
	A.MountSocket = FName(TEXT("MuzzleMount"));
	return A;
}

FSHWeaponAttachment USHAttachmentDatabase::Create_MuzzleBrake()
{
	FSHWeaponAttachment A;
	A.AttachmentID = FName(TEXT("MuzzleBrake"));
	A.DisplayName = FText::FromString(TEXT("Precision Armament M4-72 Brake"));
	A.Slot = ESHAttachmentSlot::Muzzle;
	A.WeightKg = 0.10f;
	A.VerticalRecoilMultiplier = 0.65f;     // Significant recoil reduction
	A.HorizontalRecoilMultiplier = 0.70f;
	A.SoundVolumeMultiplier = 1.15f;         // LOUDER — brakes redirect blast sideways
	A.SoundDetectionRangeMultiplier = 1.20f;
	A.MountSocket = FName(TEXT("MuzzleMount"));
	return A;
}

FSHWeaponAttachment USHAttachmentDatabase::Create_VerticalGrip()
{
	FSHWeaponAttachment A;
	A.AttachmentID = FName(TEXT("VertGrip"));
	A.DisplayName = FText::FromString(TEXT("Tango Down Vertical Grip"));
	A.Slot = ESHAttachmentSlot::Underbarrel;
	A.WeightKg = 0.12f;
	A.VerticalRecoilMultiplier = 0.82f;     // Good vertical control
	A.HorizontalRecoilMultiplier = 0.90f;
	A.MOAMultiplier = 0.95f;                // Slight accuracy improvement
	A.MountSocket = FName(TEXT("UnderbarrelMount"));
	return A;
}

FSHWeaponAttachment USHAttachmentDatabase::Create_AngledGrip()
{
	FSHWeaponAttachment A;
	A.AttachmentID = FName(TEXT("AngledGrip"));
	A.DisplayName = FText::FromString(TEXT("Magpul AFG-2 Angled Grip"));
	A.Slot = ESHAttachmentSlot::Underbarrel;
	A.WeightKg = 0.08f;
	A.HorizontalRecoilMultiplier = 0.75f;   // Better horizontal control than vert grip
	A.VerticalRecoilMultiplier = 0.92f;     // But less vertical help
	A.ADSSpeedMultiplier = 0.93f;           // Faster transitions
	A.MountSocket = FName(TEXT("UnderbarrelMount"));
	return A;
}

FSHWeaponAttachment USHAttachmentDatabase::Create_Bipod()
{
	FSHWeaponAttachment A;
	A.AttachmentID = FName(TEXT("HarrisBipod"));
	A.DisplayName = FText::FromString(TEXT("Harris S-BRM Bipod"));
	A.Slot = ESHAttachmentSlot::Bipod;
	A.WeightKg = 0.31f;
	// Bipod bonuses only apply when deployed (handled by weapon system),
	// but the attachment still has baseline ADS penalty from weight.
	A.ADSSpeedMultiplier = 1.10f; // Heavier front end
	A.MountSocket = FName(TEXT("BipodMount"));
	return A;
}

FSHWeaponAttachment USHAttachmentDatabase::Create_PEQ15_LaserIR()
{
	FSHWeaponAttachment A;
	A.AttachmentID = FName(TEXT("PEQ15"));
	A.DisplayName = FText::FromString(TEXT("AN/PEQ-15 ATPIAL"));
	A.Slot = ESHAttachmentSlot::Rail;
	A.WeightKg = 0.21f;
	// Laser/IR illuminator — visual aid, no direct stat change.
	// Gameplay effect handled by NVG/optics integration.
	A.MountSocket = FName(TEXT("RailMount_Top"));
	return A;
}

FSHWeaponAttachment USHAttachmentDatabase::Create_Flashlight()
{
	FSHWeaponAttachment A;
	A.AttachmentID = FName(TEXT("SureFire_M600"));
	A.DisplayName = FText::FromString(TEXT("SureFire M600 Scout Light"));
	A.Slot = ESHAttachmentSlot::Rail;
	A.WeightKg = 0.16f;
	// Flashlight illuminates but increases visual signature.
	A.MountSocket = FName(TEXT("RailMount_Side"));
	return A;
}
