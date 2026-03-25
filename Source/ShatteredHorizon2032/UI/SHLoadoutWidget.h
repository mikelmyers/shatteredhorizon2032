// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "SHLoadoutWidget.generated.h"

class USHWeaponDataAsset;

/** Attachment slot types. */
UENUM(BlueprintType)
enum class ESHAttachmentSlot : uint8
{
	Optic			UMETA(DisplayName = "Optic"),
	Muzzle			UMETA(DisplayName = "Muzzle"),
	Grip			UMETA(DisplayName = "Grip"),
	Underbarrel		UMETA(DisplayName = "Underbarrel"),
	Magazine		UMETA(DisplayName = "Magazine"),
	Laser			UMETA(DisplayName = "Laser / IR Device")
};

/** Equipment slot types. */
UENUM(BlueprintType)
enum class ESHEquipmentSlot : uint8
{
	Grenade			UMETA(DisplayName = "Grenade"),
	Utility			UMETA(DisplayName = "Utility"),
	Medical			UMETA(DisplayName = "Medical"),
	BodyArmor		UMETA(DisplayName = "Body Armor"),
	Drone			UMETA(DisplayName = "Drone")
};

/** Single attachment entry. */
USTRUCT(BlueprintType)
struct FSHAttachmentEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	ESHAttachmentSlot Slot = ESHAttachmentSlot::Optic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FName AttachmentID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	float WeightKg = 0.f;
};

/** Single equipment entry. */
USTRUCT(BlueprintType)
struct FSHEquipmentEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	ESHEquipmentSlot Slot = ESHEquipmentSlot::Grenade;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FName EquipmentID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	float WeightKg = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	int32 Quantity = 1;
};

/** Weapon stats preview used by the UI. */
USTRUCT(BlueprintType)
struct FSHWeaponStatsPreview
{
	GENERATED_BODY()

	/** Normalized 0-1 bars for the UI. */
	UPROPERTY(BlueprintReadOnly, Category = "Loadout")
	float Damage = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Loadout")
	float Accuracy = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Loadout")
	float Range = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Loadout")
	float FireRate = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Loadout")
	float Mobility = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Loadout")
	float Control = 0.f;
};

/** Complete loadout configuration. */
USTRUCT(BlueprintType)
struct FSHLoadout
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FName PrimaryWeaponID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FName SecondaryWeaponID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	TArray<FSHAttachmentEntry> PrimaryAttachments;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	TArray<FSHAttachmentEntry> SecondaryAttachments;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	TArray<FSHEquipmentEntry> Equipment;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FName CamoID;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FText LoadoutName;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLoadoutConfirmed, const FSHLoadout&, Loadout);

/**
 * USHLoadoutWidget
 *
 * Loadout customization screen using CommonUI. Displays primary and
 * secondary weapons, their attachments, equipment, and camo selection.
 *
 * Validates total loadout weight against the doctrine maximum (45 kg)
 * and shows per-weapon stat previews so the player can compare builds.
 *
 * Interfaces with USHLoadoutSystem for persistence and USHProgressionSystem
 * for unlock gating.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class SHATTEREDHORIZON2032_API USHLoadoutWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	USHLoadoutWidget();

	// ------------------------------------------------------------------
	//  Public API
	// ------------------------------------------------------------------

	/** Initialize the widget with an existing loadout. */
	UFUNCTION(BlueprintCallable, Category = "SH|UI|Loadout")
	void SetLoadout(const FSHLoadout& InLoadout);

	/** Calculate the total weight of the current loadout in kg. */
	UFUNCTION(BlueprintPure, Category = "SH|UI|Loadout")
	float GetTotalWeight() const;

	/** Whether the current loadout is within the weight limit. */
	UFUNCTION(BlueprintPure, Category = "SH|UI|Loadout")
	bool IsLoadoutValid() const;

	/** Build a normalized stats preview for a weapon by ID. */
	UFUNCTION(BlueprintCallable, Category = "SH|UI|Loadout")
	FSHWeaponStatsPreview GetWeaponStatsPreview(FName WeaponID) const;

	/** Confirm the loadout and broadcast OnLoadoutConfirmed. */
	UFUNCTION(BlueprintCallable, Category = "SH|UI|Loadout")
	void ConfirmLoadout();

	/** Revert to the loadout state at the time the widget was opened. */
	UFUNCTION(BlueprintCallable, Category = "SH|UI|Loadout")
	void RevertLoadout();

	// ------------------------------------------------------------------
	//  Slot modification
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "SH|UI|Loadout")
	void SetPrimaryWeapon(FName WeaponID);

	UFUNCTION(BlueprintCallable, Category = "SH|UI|Loadout")
	void SetSecondaryWeapon(FName WeaponID);

	UFUNCTION(BlueprintCallable, Category = "SH|UI|Loadout")
	void SetAttachment(bool bPrimary, const FSHAttachmentEntry& Attachment);

	UFUNCTION(BlueprintCallable, Category = "SH|UI|Loadout")
	void RemoveAttachment(bool bPrimary, ESHAttachmentSlot Slot);

	UFUNCTION(BlueprintCallable, Category = "SH|UI|Loadout")
	void SetEquipment(const FSHEquipmentEntry& Entry);

	UFUNCTION(BlueprintCallable, Category = "SH|UI|Loadout")
	void RemoveEquipment(ESHEquipmentSlot Slot);

	UFUNCTION(BlueprintCallable, Category = "SH|UI|Loadout")
	void SetCamo(FName CamoID);

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|UI|Loadout")
	FOnLoadoutConfirmed OnLoadoutConfirmed;

	// ------------------------------------------------------------------
	//  Config
	// ------------------------------------------------------------------

	/** Maximum loadout weight in kilograms (per doctrine). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|UI|Loadout")
	float MaxLoadoutWeightKg = 45.f;

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	/** Current working loadout. */
	UPROPERTY(BlueprintReadOnly, Category = "SH|UI|Loadout")
	FSHLoadout CurrentLoadout;

	/** Snapshot of the loadout when the widget opened, used for revert. */
	UPROPERTY()
	FSHLoadout OriginalLoadout;

private:
	/** Look up a weapon data asset by its ID. Returns nullptr if not found. */
	const USHWeaponDataAsset* FindWeaponData(FName WeaponID) const;

	/** Compute a normalized 0-1 stat from a raw value and expected max. */
	static float NormalizeStat(float Value, float MaxExpected);
};
