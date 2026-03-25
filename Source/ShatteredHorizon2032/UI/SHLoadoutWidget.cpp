// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHLoadoutWidget.h"
#include "Weapons/SHWeaponData.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"

USHLoadoutWidget::USHLoadoutWidget()
{
}

void USHLoadoutWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// Snapshot the current loadout so the player can revert.
	OriginalLoadout = CurrentLoadout;

	UE_LOG(LogTemp, Log, TEXT("[Loadout] Widget activated — current weight: %.1f / %.1f kg"),
		   GetTotalWeight(), MaxLoadoutWeightKg);
}

void USHLoadoutWidget::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();
}

// ------------------------------------------------------------------
//  Public API
// ------------------------------------------------------------------

void USHLoadoutWidget::SetLoadout(const FSHLoadout& InLoadout)
{
	CurrentLoadout = InLoadout;
	OriginalLoadout = InLoadout;
}

float USHLoadoutWidget::GetTotalWeight() const
{
	float Total = 0.f;

	// Primary weapon weight.
	if (const USHWeaponDataAsset* PrimaryData = FindWeaponData(CurrentLoadout.PrimaryWeaponID))
	{
		Total += PrimaryData->WeightKg;
	}

	// Secondary weapon weight.
	if (const USHWeaponDataAsset* SecondaryData = FindWeaponData(CurrentLoadout.SecondaryWeaponID))
	{
		Total += SecondaryData->WeightKg;
	}

	// Primary attachments.
	for (const FSHAttachmentEntry& Att : CurrentLoadout.PrimaryAttachments)
	{
		Total += Att.WeightKg;
	}

	// Secondary attachments.
	for (const FSHAttachmentEntry& Att : CurrentLoadout.SecondaryAttachments)
	{
		Total += Att.WeightKg;
	}

	// Equipment.
	for (const FSHEquipmentEntry& Equip : CurrentLoadout.Equipment)
	{
		Total += Equip.WeightKg * Equip.Quantity;
	}

	return Total;
}

bool USHLoadoutWidget::IsLoadoutValid() const
{
	// Weight check.
	if (GetTotalWeight() > MaxLoadoutWeightKg)
	{
		return false;
	}

	// Must have at least a primary weapon.
	if (CurrentLoadout.PrimaryWeaponID.IsNone())
	{
		return false;
	}

	return true;
}

FSHWeaponStatsPreview USHLoadoutWidget::GetWeaponStatsPreview(FName WeaponID) const
{
	FSHWeaponStatsPreview Preview;

	const USHWeaponDataAsset* Data = FindWeaponData(WeaponID);
	if (!Data)
	{
		return Preview;
	}

	// Normalize against expected maximums for UI bar display.
	Preview.Damage   = NormalizeStat(Data->BaseDamage, 100.f);
	Preview.Accuracy = NormalizeStat(1.f / FMath::Max(Data->AccuracyModifiers.StandingMOA, 0.1f), 1.f);
	Preview.Range    = NormalizeStat(Data->EffectiveRangeM, 1000.f);
	Preview.FireRate = NormalizeStat(Data->RoundsPerMinute, 1200.f);
	Preview.Mobility = NormalizeStat(1.f - FMath::Clamp(Data->WeightKg / 15.f, 0.f, 1.f), 1.f);
	Preview.Control  = NormalizeStat(1.f / FMath::Max(Data->RecoilPattern.VerticalRecoil, 0.01f), 10.f);

	return Preview;
}

void USHLoadoutWidget::ConfirmLoadout()
{
	if (!IsLoadoutValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("[Loadout] Cannot confirm — loadout is invalid (weight: %.1f / %.1f kg)"),
			   GetTotalWeight(), MaxLoadoutWeightKg);
		return;
	}

	OnLoadoutConfirmed.Broadcast(CurrentLoadout);

	UE_LOG(LogTemp, Log, TEXT("[Loadout] Confirmed — Primary: %s, Secondary: %s, Weight: %.1f kg"),
		   *CurrentLoadout.PrimaryWeaponID.ToString(),
		   *CurrentLoadout.SecondaryWeaponID.ToString(),
		   GetTotalWeight());

	DeactivateWidget();
}

void USHLoadoutWidget::RevertLoadout()
{
	CurrentLoadout = OriginalLoadout;
	UE_LOG(LogTemp, Log, TEXT("[Loadout] Reverted to original loadout"));
}

// ------------------------------------------------------------------
//  Slot modification
// ------------------------------------------------------------------

void USHLoadoutWidget::SetPrimaryWeapon(FName WeaponID)
{
	CurrentLoadout.PrimaryWeaponID = WeaponID;

	// Clear primary attachments when switching weapons — incompatible
	// attachments will need to be re-selected.
	CurrentLoadout.PrimaryAttachments.Empty();

	UE_LOG(LogTemp, Log, TEXT("[Loadout] Primary weapon set to: %s"), *WeaponID.ToString());
}

void USHLoadoutWidget::SetSecondaryWeapon(FName WeaponID)
{
	CurrentLoadout.SecondaryWeaponID = WeaponID;
	CurrentLoadout.SecondaryAttachments.Empty();

	UE_LOG(LogTemp, Log, TEXT("[Loadout] Secondary weapon set to: %s"), *WeaponID.ToString());
}

void USHLoadoutWidget::SetAttachment(bool bPrimary, const FSHAttachmentEntry& Attachment)
{
	TArray<FSHAttachmentEntry>& Attachments = bPrimary
		? CurrentLoadout.PrimaryAttachments
		: CurrentLoadout.SecondaryAttachments;

	// Replace existing attachment in the same slot, or add new.
	bool bReplaced = false;
	for (FSHAttachmentEntry& Existing : Attachments)
	{
		if (Existing.Slot == Attachment.Slot)
		{
			Existing = Attachment;
			bReplaced = true;
			break;
		}
	}

	if (!bReplaced)
	{
		Attachments.Add(Attachment);
	}
}

void USHLoadoutWidget::RemoveAttachment(bool bPrimary, ESHAttachmentSlot Slot)
{
	TArray<FSHAttachmentEntry>& Attachments = bPrimary
		? CurrentLoadout.PrimaryAttachments
		: CurrentLoadout.SecondaryAttachments;

	Attachments.RemoveAll([Slot](const FSHAttachmentEntry& Entry)
	{
		return Entry.Slot == Slot;
	});
}

void USHLoadoutWidget::SetEquipment(const FSHEquipmentEntry& Entry)
{
	// Replace existing equipment in the same slot, or add new.
	bool bReplaced = false;
	for (FSHEquipmentEntry& Existing : CurrentLoadout.Equipment)
	{
		if (Existing.Slot == Entry.Slot)
		{
			Existing = Entry;
			bReplaced = true;
			break;
		}
	}

	if (!bReplaced)
	{
		CurrentLoadout.Equipment.Add(Entry);
	}
}

void USHLoadoutWidget::RemoveEquipment(ESHEquipmentSlot Slot)
{
	CurrentLoadout.Equipment.RemoveAll([Slot](const FSHEquipmentEntry& Entry)
	{
		return Entry.Slot == Slot;
	});
}

void USHLoadoutWidget::SetCamo(FName CamoID)
{
	CurrentLoadout.CamoID = CamoID;
}

// ------------------------------------------------------------------
//  Private helpers
// ------------------------------------------------------------------

const USHWeaponDataAsset* USHLoadoutWidget::FindWeaponData(FName WeaponID) const
{
	if (WeaponID.IsNone())
	{
		return nullptr;
	}

	// Query the Asset Manager for USHWeaponDataAsset with a matching WeaponID.
	// In production the loadout system would maintain a cached registry; this
	// performs a synchronous scan as a fallback.
	UAssetManager& AssetManager = UAssetManager::Get();

	TArray<FAssetData> AssetList;
	AssetManager.GetPrimaryAssetDataList(FPrimaryAssetType(TEXT("SHWeaponData")), AssetList);

	for (const FAssetData& AssetData : AssetList)
	{
		if (const USHWeaponDataAsset* Data = Cast<USHWeaponDataAsset>(AssetData.GetAsset()))
		{
			if (Data->WeaponID == WeaponID)
			{
				return Data;
			}
		}
	}

	UE_LOG(LogTemp, Warning, TEXT("[Loadout] Weapon data not found for ID: %s"), *WeaponID.ToString());
	return nullptr;
}

float USHLoadoutWidget::NormalizeStat(float Value, float MaxExpected)
{
	return FMath::Clamp(Value / FMath::Max(MaxExpected, KINDA_SMALL_NUMBER), 0.f, 1.f);
}
