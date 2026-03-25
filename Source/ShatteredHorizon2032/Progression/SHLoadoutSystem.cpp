// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHLoadoutSystem.h"
#include "SHProgressionSystem.h"
#include "Core/SHPlayerCharacter.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"

USHLoadoutSystem::USHLoadoutSystem()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void USHLoadoutSystem::BeginPlay()
{
	Super::BeginPlay();

	InitializeDefaultWeights();
	InitializeDefaultAttachments();

	UE_LOG(LogTemp, Log, TEXT("[SHLoadoutSystem] BeginPlay — %d weapons, %d equipment items, %d attachments registered."),
		WeaponWeights.Num(), EquipmentWeights.Num(), AllAttachments.Num());
}

// =======================================================================
//  Loadout management
// =======================================================================

void USHLoadoutSystem::SetLoadout(const FSHLoadout& InLoadout)
{
	CurrentLoadout = InLoadout;

	UE_LOG(LogTemp, Log, TEXT("[SHLoadoutSystem] Loadout set — Primary: %s, Secondary: %s, Attachments: %d, Equipment: %d, Camo: %s"),
		*InLoadout.PrimaryWeapon.ToString(),
		*InLoadout.SecondaryWeapon.ToString(),
		InLoadout.PrimaryAttachments.Num(),
		InLoadout.Equipment.Num(),
		*InLoadout.CamoPattern.ToString());

	OnLoadoutChanged.Broadcast(CurrentLoadout);
}

FSHLoadoutValidationResult USHLoadoutSystem::ValidateLoadout() const
{
	return ValidateLoadoutData(CurrentLoadout);
}

FSHLoadoutValidationResult USHLoadoutSystem::ValidateLoadoutData(const FSHLoadout& InLoadout) const
{
	FSHLoadoutValidationResult Result;
	Result.bIsValid = true;

	const USHProgressionSystem* Progression = GetProgressionSystem();

	// --- Validate primary weapon ---
	if (InLoadout.PrimaryWeapon.IsNone())
	{
		Result.bIsValid = false;
		Result.Issues.Add(FText::FromString(TEXT("No primary weapon selected.")));
	}
	else if (Progression && !Progression->IsItemUnlocked(InLoadout.PrimaryWeapon))
	{
		// Check if it's a default weapon (default weapons are always available).
		if (!WeaponWeights.Contains(InLoadout.PrimaryWeapon))
		{
			Result.bIsValid = false;
			Result.Issues.Add(FText::FromString(
				FString::Printf(TEXT("Primary weapon '%s' is not recognized."), *InLoadout.PrimaryWeapon.ToString())));
		}
	}

	// --- Validate secondary weapon ---
	if (InLoadout.SecondaryWeapon.IsNone())
	{
		Result.bIsValid = false;
		Result.Issues.Add(FText::FromString(TEXT("No secondary weapon selected.")));
	}
	else if (Progression && !Progression->IsItemUnlocked(InLoadout.SecondaryWeapon))
	{
		if (!WeaponWeights.Contains(InLoadout.SecondaryWeapon))
		{
			Result.bIsValid = false;
			Result.Issues.Add(FText::FromString(
				FString::Printf(TEXT("Secondary weapon '%s' is not recognized."), *InLoadout.SecondaryWeapon.ToString())));
		}
	}

	// --- Validate attachments ---
	TMap<ESHAttachmentSlot, int32> SlotOccupancy;
	for (const FName& AttachmentId : InLoadout.PrimaryAttachments)
	{
		const FSHAttachmentData* AttData = FindAttachmentData(AttachmentId);
		if (!AttData)
		{
			Result.bIsValid = false;
			Result.Issues.Add(FText::FromString(
				FString::Printf(TEXT("Attachment '%s' not found in database."), *AttachmentId.ToString())));
			continue;
		}

		// Check rank requirement.
		if (Progression)
		{
			const uint8 PlayerRankValue = static_cast<uint8>(Progression->GetCurrentRank());
			const uint8 RequiredRankValue = static_cast<uint8>(AttData->RequiredRank);
			if (PlayerRankValue < RequiredRankValue)
			{
				Result.bIsValid = false;
				Result.Issues.Add(FText::FromString(
					FString::Printf(TEXT("Attachment '%s' requires rank %s."),
						*AttachmentId.ToString(),
						*USHProgressionSystem::GetRankAbbreviation(AttData->RequiredRank).ToString())));
			}
		}

		// Check unlock status.
		if (Progression && !Progression->IsItemUnlocked(AttachmentId))
		{
			Result.bIsValid = false;
			Result.Issues.Add(FText::FromString(
				FString::Printf(TEXT("Attachment '%s' is not unlocked."), *AttachmentId.ToString())));
		}

		// Check weapon compatibility.
		if (AttData->CompatibleWeapons.Num() > 0 && !AttData->CompatibleWeapons.Contains(InLoadout.PrimaryWeapon))
		{
			Result.bIsValid = false;
			Result.Issues.Add(FText::FromString(
				FString::Printf(TEXT("Attachment '%s' is not compatible with weapon '%s'."),
					*AttachmentId.ToString(), *InLoadout.PrimaryWeapon.ToString())));
		}

		// Check for duplicate slot usage.
		int32& Count = SlotOccupancy.FindOrAdd(AttData->Slot, 0);
		Count++;
		if (Count > 1)
		{
			Result.bIsValid = false;
			Result.Issues.Add(FText::FromString(
				FString::Printf(TEXT("Multiple attachments in the same slot: '%s' conflicts."),
					*AttachmentId.ToString())));
		}
	}

	// --- Validate equipment unlock status ---
	for (const FName& EquipId : InLoadout.Equipment)
	{
		if (Progression && !Progression->IsItemUnlocked(EquipId))
		{
			// Allow default equipment that's in the weight table.
			if (!EquipmentWeights.Contains(EquipId))
			{
				Result.bIsValid = false;
				Result.Issues.Add(FText::FromString(
					FString::Printf(TEXT("Equipment '%s' is not recognized."), *EquipId.ToString())));
			}
		}
	}

	// --- Validate weight ---
	const float TotalWeight = CalculateLoadoutWeight(InLoadout);
	if (TotalWeight > MaxCarryWeight)
	{
		Result.bIsValid = false;
		Result.Issues.Add(FText::FromString(
			FString::Printf(TEXT("Loadout exceeds maximum carry weight: %.1f kg / %.1f kg."),
				TotalWeight, MaxCarryWeight)));
	}

	return Result;
}

// =======================================================================
//  Attachment queries
// =======================================================================

TArray<FSHAttachmentData> USHLoadoutSystem::GetAvailableAttachments(FName WeaponId) const
{
	TArray<FSHAttachmentData> Compatible;

	for (const FSHAttachmentData& AttData : AllAttachments)
	{
		// Compatible if the weapon list is empty (universal) or contains the weapon.
		if (AttData.CompatibleWeapons.Num() == 0 || AttData.CompatibleWeapons.Contains(WeaponId))
		{
			Compatible.Add(AttData);
		}
	}

	return Compatible;
}

const FSHAttachmentData* USHLoadoutSystem::FindAttachmentData(FName AttachmentId) const
{
	for (const FSHAttachmentData& AttData : AllAttachments)
	{
		if (AttData.AttachmentId == AttachmentId)
		{
			return &AttData;
		}
	}
	return nullptr;
}

// =======================================================================
//  Weight calculations
// =======================================================================

float USHLoadoutSystem::GetTotalWeight() const
{
	return CalculateLoadoutWeight(CurrentLoadout);
}

float USHLoadoutSystem::CalculateLoadoutWeight(const FSHLoadout& InLoadout) const
{
	float TotalWeight = 0.f;

	// Primary weapon weight.
	TotalWeight += GetWeaponWeight(InLoadout.PrimaryWeapon);

	// Primary attachment weights.
	for (const FName& AttachmentId : InLoadout.PrimaryAttachments)
	{
		const FSHAttachmentData* AttData = FindAttachmentData(AttachmentId);
		if (AttData)
		{
			TotalWeight += AttData->WeightKg;
		}
	}

	// Secondary weapon weight.
	TotalWeight += GetWeaponWeight(InLoadout.SecondaryWeapon);

	// Equipment weights.
	for (const FName& EquipId : InLoadout.Equipment)
	{
		TotalWeight += GetEquipmentWeight(EquipId);
	}

	return TotalWeight;
}

// =======================================================================
//  Character application
// =======================================================================

void USHLoadoutSystem::ApplyLoadoutToCharacter()
{
	// Validate before applying.
	const FSHLoadoutValidationResult Validation = ValidateLoadout();
	if (!Validation.bIsValid)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SHLoadoutSystem] Cannot apply invalid loadout. Issues:"));
		for (const FText& Issue : Validation.Issues)
		{
			UE_LOG(LogTemp, Warning, TEXT("  - %s"), *Issue.ToString());
		}
		return;
	}

	// Find the player character to apply the loadout to.
	ASHPlayerCharacter* PlayerCharacter = nullptr;

	// Try to get from the owning actor's controller chain.
	if (AActor* Owner = GetOwner())
	{
		if (APlayerController* PC = Cast<APlayerController>(Owner))
		{
			PlayerCharacter = Cast<ASHPlayerCharacter>(PC->GetPawn());
		}
		else
		{
			// Owner might be a player state or the character itself.
			PlayerCharacter = Cast<ASHPlayerCharacter>(Owner);
			if (!PlayerCharacter)
			{
				if (APawn* OwnerPawn = Cast<APawn>(Owner))
				{
					PlayerCharacter = Cast<ASHPlayerCharacter>(OwnerPawn);
				}
			}
		}
	}

	if (!PlayerCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SHLoadoutSystem] ApplyLoadoutToCharacter — Could not find player character."));
		return;
	}

	// Apply primary weapon.
	if (!CurrentLoadout.PrimaryWeapon.IsNone())
	{
		// TODO: Spawn and attach the primary weapon actor from weapon data registry.
		UE_LOG(LogTemp, Log, TEXT("[SHLoadoutSystem] Equipping primary: %s"), *CurrentLoadout.PrimaryWeapon.ToString());
	}

	// Apply primary attachments.
	for (const FName& AttachmentId : CurrentLoadout.PrimaryAttachments)
	{
		const FSHAttachmentData* AttData = FindAttachmentData(AttachmentId);
		if (AttData)
		{
			// TODO: Apply attachment modifiers to the primary weapon instance.
			UE_LOG(LogTemp, Log, TEXT("[SHLoadoutSystem] Attaching %s (%s slot)"),
				*AttachmentId.ToString(),
				*UEnum::GetValueAsString(AttData->Slot));
		}
	}

	// Apply secondary weapon.
	if (!CurrentLoadout.SecondaryWeapon.IsNone())
	{
		// TODO: Spawn and attach the secondary weapon actor.
		UE_LOG(LogTemp, Log, TEXT("[SHLoadoutSystem] Equipping secondary: %s"), *CurrentLoadout.SecondaryWeapon.ToString());
	}

	// Apply equipment.
	for (const FName& EquipId : CurrentLoadout.Equipment)
	{
		// TODO: Add equipment to the player character's gear slots.
		UE_LOG(LogTemp, Log, TEXT("[SHLoadoutSystem] Equipping: %s"), *EquipId.ToString());
	}

	// Apply camo pattern.
	if (!CurrentLoadout.CamoPattern.IsNone())
	{
		// TODO: Set the character's material parameter collection for camo pattern.
		UE_LOG(LogTemp, Log, TEXT("[SHLoadoutSystem] Applying camo: %s"), *CurrentLoadout.CamoPattern.ToString());
	}

	// Recalculate the character's weight based on the new loadout.
	PlayerCharacter->RecalculateWeight();

	UE_LOG(LogTemp, Log, TEXT("[SHLoadoutSystem] Loadout applied to character. Total weight: %.1f kg"),
		GetTotalWeight());
}

// =======================================================================
//  Internal helpers
// =======================================================================

float USHLoadoutSystem::GetWeaponWeight(FName WeaponId) const
{
	if (WeaponId.IsNone())
	{
		return 0.f;
	}

	if (const float* Weight = WeaponWeights.Find(WeaponId))
	{
		return *Weight;
	}

	UE_LOG(LogTemp, Warning, TEXT("[SHLoadoutSystem] Unknown weapon ID '%s'. Returning default weight 3.5 kg."),
		*WeaponId.ToString());
	return 3.5f;
}

float USHLoadoutSystem::GetEquipmentWeight(FName EquipmentId) const
{
	if (EquipmentId.IsNone())
	{
		return 0.f;
	}

	if (const float* Weight = EquipmentWeights.Find(EquipmentId))
	{
		return *Weight;
	}

	UE_LOG(LogTemp, Warning, TEXT("[SHLoadoutSystem] Unknown equipment ID '%s'. Returning default weight 0.5 kg."),
		*EquipmentId.ToString());
	return 0.5f;
}

USHProgressionSystem* USHLoadoutSystem::GetProgressionSystem() const
{
	if (const UWorld* World = GetWorld())
	{
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			return GI->GetSubsystem<USHProgressionSystem>();
		}
	}
	return nullptr;
}

void USHLoadoutSystem::InitializeDefaultWeights()
{
	if (WeaponWeights.Num() > 0)
	{
		return;
	}

	// Standard USMC weapon weights (kg).
	WeaponWeights.Add(FName(TEXT("M27_IAR")),		3.6f);
	WeaponWeights.Add(FName(TEXT("M4A1")),			3.4f);
	WeaponWeights.Add(FName(TEXT("M249_SAW")),		7.5f);
	WeaponWeights.Add(FName(TEXT("M110_SASS")),		6.9f);
	WeaponWeights.Add(FName(TEXT("M17_SIG")),		0.83f);
	WeaponWeights.Add(FName(TEXT("M320_GL")),		1.5f);
	WeaponWeights.Add(FName(TEXT("Mossberg590")),	3.3f);

	if (EquipmentWeights.Num() > 0)
	{
		return;
	}

	// Standard equipment weights (kg).
	EquipmentWeights.Add(FName(TEXT("M67_Frag")),			0.4f);
	EquipmentWeights.Add(FName(TEXT("M18_Smoke")),			0.54f);
	EquipmentWeights.Add(FName(TEXT("M84_Flashbang")),		0.23f);
	EquipmentWeights.Add(FName(TEXT("IFAK")),				0.9f);
	EquipmentWeights.Add(FName(TEXT("C4_Charge")),			0.57f);
	EquipmentWeights.Add(FName(TEXT("Claymore_M18")),		1.6f);
	EquipmentWeights.Add(FName(TEXT("NVG_PVS31")),			0.55f);
	EquipmentWeights.Add(FName(TEXT("Binos_M22")),			1.2f);
	EquipmentWeights.Add(FName(TEXT("Entrenching_Tool")),	1.1f);
	EquipmentWeights.Add(FName(TEXT("AN_PRC152_Radio")),	0.85f);
	EquipmentWeights.Add(FName(TEXT("MRE")),				0.74f);
	EquipmentWeights.Add(FName(TEXT("Water_2L")),			2.0f);
	EquipmentWeights.Add(FName(TEXT("Ammo_556_Mag")),		0.45f);
	EquipmentWeights.Add(FName(TEXT("Ammo_762_Mag")),		0.62f);
	EquipmentWeights.Add(FName(TEXT("Ammo_9mm_Mag")),		0.21f);
}

void USHLoadoutSystem::InitializeDefaultAttachments()
{
	if (AllAttachments.Num() > 0)
	{
		return;
	}

	// Optics
	{
		FSHAttachmentData Att;
		Att.AttachmentId = FName(TEXT("ACOG_TA31"));
		Att.DisplayName = FText::FromString(TEXT("Trijicon ACOG TA31 (4x32)"));
		Att.Slot = ESHAttachmentSlot::Optic;
		Att.RequiredRank = ESHRank::Private;
		Att.WeightKg = 0.30f;
		AllAttachments.Add(Att);
	}
	{
		FSHAttachmentData Att;
		Att.AttachmentId = FName(TEXT("LPVO_1_8x"));
		Att.DisplayName = FText::FromString(TEXT("LPVO 1-8x Variable"));
		Att.Slot = ESHAttachmentSlot::Optic;
		Att.RequiredRank = ESHRank::Corporal;
		Att.WeightKg = 0.45f;
		AllAttachments.Add(Att);
	}
	{
		FSHAttachmentData Att;
		Att.AttachmentId = FName(TEXT("Eotech_EXPS3"));
		Att.DisplayName = FText::FromString(TEXT("EOTech EXPS3 Holographic"));
		Att.Slot = ESHAttachmentSlot::Optic;
		Att.RequiredRank = ESHRank::PFC;
		Att.WeightKg = 0.32f;
		AllAttachments.Add(Att);
	}
	{
		FSHAttachmentData Att;
		Att.AttachmentId = FName(TEXT("Leupold_Mk4"));
		Att.DisplayName = FText::FromString(TEXT("Leupold Mk4 (10x40)"));
		Att.Slot = ESHAttachmentSlot::Optic;
		Att.RequiredRank = ESHRank::Sergeant;
		Att.WeightKg = 0.60f;
		Att.CompatibleWeapons.Add(FName(TEXT("M110_SASS")));
		AllAttachments.Add(Att);
	}

	// Muzzle devices
	{
		FSHAttachmentData Att;
		Att.AttachmentId = FName(TEXT("Suppressor_SOCOM556"));
		Att.DisplayName = FText::FromString(TEXT("SOCOM556 Suppressor"));
		Att.Slot = ESHAttachmentSlot::Muzzle;
		Att.RequiredRank = ESHRank::LanceCorporal;
		Att.WeightKg = 0.48f;
		Att.CompatibleWeapons.Add(FName(TEXT("M27_IAR")));
		Att.CompatibleWeapons.Add(FName(TEXT("M4A1")));
		AllAttachments.Add(Att);
	}
	{
		FSHAttachmentData Att;
		Att.AttachmentId = FName(TEXT("Suppressor_762"));
		Att.DisplayName = FText::FromString(TEXT("7.62 NATO Suppressor"));
		Att.Slot = ESHAttachmentSlot::Muzzle;
		Att.RequiredRank = ESHRank::Sergeant;
		Att.WeightKg = 0.62f;
		Att.CompatibleWeapons.Add(FName(TEXT("M110_SASS")));
		AllAttachments.Add(Att);
	}
	{
		FSHAttachmentData Att;
		Att.AttachmentId = FName(TEXT("FlashHider_A2"));
		Att.DisplayName = FText::FromString(TEXT("A2 Flash Hider"));
		Att.Slot = ESHAttachmentSlot::Muzzle;
		Att.RequiredRank = ESHRank::Private;
		Att.WeightKg = 0.07f;
		AllAttachments.Add(Att);
	}
	{
		FSHAttachmentData Att;
		Att.AttachmentId = FName(TEXT("CompM_Brake"));
		Att.DisplayName = FText::FromString(TEXT("Compensator / Muzzle Brake"));
		Att.Slot = ESHAttachmentSlot::Muzzle;
		Att.RequiredRank = ESHRank::PFC;
		Att.WeightKg = 0.12f;
		AllAttachments.Add(Att);
	}

	// Grips
	{
		FSHAttachmentData Att;
		Att.AttachmentId = FName(TEXT("Grip_Vertical"));
		Att.DisplayName = FText::FromString(TEXT("Vertical Foregrip"));
		Att.Slot = ESHAttachmentSlot::Grip;
		Att.RequiredRank = ESHRank::Private;
		Att.WeightKg = 0.12f;
		AllAttachments.Add(Att);
	}
	{
		FSHAttachmentData Att;
		Att.AttachmentId = FName(TEXT("Grip_Angled"));
		Att.DisplayName = FText::FromString(TEXT("Angled Foregrip"));
		Att.Slot = ESHAttachmentSlot::Grip;
		Att.RequiredRank = ESHRank::LanceCorporal;
		Att.WeightKg = 0.08f;
		AllAttachments.Add(Att);
	}
	{
		FSHAttachmentData Att;
		Att.AttachmentId = FName(TEXT("Grip_Stubby"));
		Att.DisplayName = FText::FromString(TEXT("Stubby Grip"));
		Att.Slot = ESHAttachmentSlot::Grip;
		Att.RequiredRank = ESHRank::PFC;
		Att.WeightKg = 0.06f;
		AllAttachments.Add(Att);
	}

	// Barrel
	{
		FSHAttachmentData Att;
		Att.AttachmentId = FName(TEXT("Barrel_Heavy"));
		Att.DisplayName = FText::FromString(TEXT("Heavy Profile Barrel"));
		Att.Slot = ESHAttachmentSlot::Barrel;
		Att.RequiredRank = ESHRank::Corporal;
		Att.WeightKg = 0.35f;
		Att.CompatibleWeapons.Add(FName(TEXT("M27_IAR")));
		Att.CompatibleWeapons.Add(FName(TEXT("M4A1")));
		AllAttachments.Add(Att);
	}
	{
		FSHAttachmentData Att;
		Att.AttachmentId = FName(TEXT("Barrel_Short"));
		Att.DisplayName = FText::FromString(TEXT("10.3\" CQB Barrel"));
		Att.Slot = ESHAttachmentSlot::Barrel;
		Att.RequiredRank = ESHRank::LanceCorporal;
		Att.WeightKg = -0.15f; // Lighter than standard.
		Att.CompatibleWeapons.Add(FName(TEXT("M4A1")));
		AllAttachments.Add(Att);
	}

	// Magazine
	{
		FSHAttachmentData Att;
		Att.AttachmentId = FName(TEXT("Mag_Extended_40"));
		Att.DisplayName = FText::FromString(TEXT("Extended 40-Round Magazine"));
		Att.Slot = ESHAttachmentSlot::Magazine;
		Att.RequiredRank = ESHRank::Corporal;
		Att.WeightKg = 0.18f;
		Att.CompatibleWeapons.Add(FName(TEXT("M27_IAR")));
		Att.CompatibleWeapons.Add(FName(TEXT("M4A1")));
		AllAttachments.Add(Att);
	}
	{
		FSHAttachmentData Att;
		Att.AttachmentId = FName(TEXT("Mag_Drum_60"));
		Att.DisplayName = FText::FromString(TEXT("60-Round Drum Magazine"));
		Att.Slot = ESHAttachmentSlot::Magazine;
		Att.RequiredRank = ESHRank::StaffSergeant;
		Att.WeightKg = 0.45f;
		Att.CompatibleWeapons.Add(FName(TEXT("M27_IAR")));
		AllAttachments.Add(Att);
	}

	UE_LOG(LogTemp, Log, TEXT("[SHLoadoutSystem] Initialized %d default attachments."), AllAttachments.Num());
}
