// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SHWeaponData.h"
#include "SHWeaponAttachmentSystem.generated.h"

// -----------------------------------------------------------------------
//  Attachment slot types
// -----------------------------------------------------------------------

UENUM(BlueprintType)
enum class ESHAttachmentSlot : uint8
{
	Optic           UMETA(DisplayName = "Optic / Sight"),
	Muzzle          UMETA(DisplayName = "Muzzle Device"),
	Underbarrel     UMETA(DisplayName = "Underbarrel"),
	Magazine        UMETA(DisplayName = "Magazine"),
	Stock           UMETA(DisplayName = "Stock"),
	Rail            UMETA(DisplayName = "Rail Accessory"),
	Bipod           UMETA(DisplayName = "Bipod")
};

// -----------------------------------------------------------------------
//  Attachment definition
// -----------------------------------------------------------------------

/**
 * Defines a single weapon attachment with stat modifiers.
 * Attachments stack multiplicatively for multipliers, additively for flat mods.
 */
USTRUCT(BlueprintType)
struct FSHWeaponAttachment
{
	GENERATED_BODY()

	/** Unique ID for this attachment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment")
	FName AttachmentID;

	/** Human-readable name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment")
	FText DisplayName;

	/** Which slot this occupies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment")
	ESHAttachmentSlot Slot = ESHAttachmentSlot::Optic;

	/** Weight added to the weapon (kg). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment|Stats", meta = (ClampMin = "0"))
	float WeightKg = 0.0f;

	// --- Accuracy modifiers ---

	/** Multiplier to base MOA (< 1.0 = more accurate). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment|Stats", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float MOAMultiplier = 1.0f;

	/** Multiplier to ADS accuracy (< 1.0 = tighter ADS). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment|Stats", meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float ADSAccuracyMultiplier = 1.0f;

	// --- Recoil modifiers ---

	/** Multiplier to vertical recoil (< 1.0 = less kick). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment|Stats", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float VerticalRecoilMultiplier = 1.0f;

	/** Multiplier to horizontal recoil. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment|Stats", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float HorizontalRecoilMultiplier = 1.0f;

	// --- ADS speed ---

	/** Multiplier to ADS transition time (> 1.0 = slower ADS). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment|Stats", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float ADSSpeedMultiplier = 1.0f;

	// --- Sound ---

	/** Multiplier to fire sound volume (suppressor = 0.15). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment|Stats", meta = (ClampMin = "0.0", ClampMax = "1.5"))
	float SoundVolumeMultiplier = 1.0f;

	/** Multiplier to AI detection range of fire sound. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment|Stats", meta = (ClampMin = "0.0", ClampMax = "1.5"))
	float SoundDetectionRangeMultiplier = 1.0f;

	// --- Muzzle velocity (suppressors reduce slightly) ---

	/** Flat change to muzzle velocity (m/s). Suppressors: -15 to -30 m/s. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment|Stats")
	float MuzzleVelocityDelta = 0.0f;

	// --- Optic specific ---

	/** Magnification (optics only). 0 = not an optic. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment|Optic", meta = (ClampMin = "0"))
	float Magnification = 0.0f;

	/** FOV when using this optic (degrees). 0 = use weapon default. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment|Optic", meta = (ClampMin = "0"))
	float OpticFOV = 0.0f;

	// --- Visual ---

	/** Static mesh for this attachment (attached to weapon socket). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment|Visual")
	TSoftObjectPtr<UStaticMesh> AttachmentMesh;

	/** Socket name on the weapon where this attachment mounts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment|Visual")
	FName MountSocket;

	// --- Compatibility ---

	/** Weapon IDs this attachment is compatible with. Empty = all weapons. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Attachment|Compat")
	TArray<FName> CompatibleWeapons;
};

// -----------------------------------------------------------------------
//  Computed modifier aggregate
// -----------------------------------------------------------------------

/** Aggregated stat modifiers from all equipped attachments. */
USTRUCT(BlueprintType)
struct FSHAttachmentModifiers
{
	GENERATED_BODY()

	float TotalWeightKg = 0.0f;
	float MOAMultiplier = 1.0f;
	float ADSAccuracyMultiplier = 1.0f;
	float VerticalRecoilMultiplier = 1.0f;
	float HorizontalRecoilMultiplier = 1.0f;
	float ADSSpeedMultiplier = 1.0f;
	float SoundVolumeMultiplier = 1.0f;
	float SoundDetectionRangeMultiplier = 1.0f;
	float MuzzleVelocityDelta = 0.0f;
	float Magnification = 0.0f;
	float OpticFOV = 0.0f;
};

// -----------------------------------------------------------------------
//  Delegates
// -----------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnAttachmentChanged, ESHAttachmentSlot, Slot, FName, AttachmentID);

// -----------------------------------------------------------------------
//  USHWeaponAttachmentSystem
// -----------------------------------------------------------------------

/**
 * USHWeaponAttachmentSystem
 *
 * Manages weapon attachments: optics, muzzle devices, grips, stocks, bipods.
 * Each attachment modifies weapon stats (accuracy, recoil, ADS speed, sound,
 * velocity) and attaches a visual mesh to the weapon model.
 *
 * Stat modifiers stack multiplicatively. Weight stacks additively.
 */
UCLASS(ClassGroup = (SH), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHWeaponAttachmentSystem : public UActorComponent
{
	GENERATED_BODY()

public:
	USHWeaponAttachmentSystem();

	// ------------------------------------------------------------------
	//  Attachment management
	// ------------------------------------------------------------------

	/** Equip an attachment to its designated slot. Replaces any existing. */
	UFUNCTION(BlueprintCallable, Category = "SH|Weapon|Attachments")
	bool EquipAttachment(const FSHWeaponAttachment& Attachment);

	/** Remove the attachment from a slot. */
	UFUNCTION(BlueprintCallable, Category = "SH|Weapon|Attachments")
	bool RemoveAttachment(ESHAttachmentSlot Slot);

	/** Remove all attachments. */
	UFUNCTION(BlueprintCallable, Category = "SH|Weapon|Attachments")
	void RemoveAllAttachments();

	/** Check if a slot has an attachment. */
	UFUNCTION(BlueprintPure, Category = "SH|Weapon|Attachments")
	bool HasAttachment(ESHAttachmentSlot Slot) const;

	/** Get the attachment in a slot (returns false if empty). */
	UFUNCTION(BlueprintPure, Category = "SH|Weapon|Attachments")
	bool GetAttachment(ESHAttachmentSlot Slot, FSHWeaponAttachment& OutAttachment) const;

	// ------------------------------------------------------------------
	//  Modifier queries (used by weapon systems)
	// ------------------------------------------------------------------

	/** Get the aggregated stat modifiers from all equipped attachments. */
	UFUNCTION(BlueprintPure, Category = "SH|Weapon|Attachments")
	FSHAttachmentModifiers GetAggregateModifiers() const;

	/** Check if the weapon has a suppressor equipped. */
	UFUNCTION(BlueprintPure, Category = "SH|Weapon|Attachments")
	bool IsSuppressed() const;

	/** Check if the weapon has a bipod equipped. */
	UFUNCTION(BlueprintPure, Category = "SH|Weapon|Attachments")
	bool HasBipod() const;

	/** Get the equipped optic magnification (0 = no optic). */
	UFUNCTION(BlueprintPure, Category = "SH|Weapon|Attachments")
	float GetOpticMagnification() const;

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Weapon|Attachments")
	FOnAttachmentChanged OnAttachmentChanged;

private:
	/** Recalculate aggregate modifiers. Called after any attachment change. */
	void RecalculateModifiers();

	/** Equipped attachments by slot. */
	UPROPERTY()
	TMap<ESHAttachmentSlot, FSHWeaponAttachment> EquippedAttachments;

	/** Cached aggregate modifiers. */
	FSHAttachmentModifiers CachedModifiers;
};

// -----------------------------------------------------------------------
//  Default attachment database
// -----------------------------------------------------------------------

UCLASS()
class SHATTEREDHORIZON2032_API USHAttachmentDatabase : public UObject
{
	GENERATED_BODY()

public:
	// --- Optics ---
	static FSHWeaponAttachment Create_ACOG_4x();
	static FSHWeaponAttachment Create_LPVO_1_8x();
	static FSHWeaponAttachment Create_Leupold_10x();
	static FSHWeaponAttachment Create_RedDot_Aimpoint();
	static FSHWeaponAttachment Create_Holographic_EOTech();

	// --- Muzzle ---
	static FSHWeaponAttachment Create_Suppressor_556();
	static FSHWeaponAttachment Create_Suppressor_762();
	static FSHWeaponAttachment Create_FlashHider();
	static FSHWeaponAttachment Create_MuzzleBrake();

	// --- Underbarrel ---
	static FSHWeaponAttachment Create_VerticalGrip();
	static FSHWeaponAttachment Create_AngledGrip();
	static FSHWeaponAttachment Create_Bipod();

	// --- Accessories ---
	static FSHWeaponAttachment Create_PEQ15_LaserIR();
	static FSHWeaponAttachment Create_Flashlight();
};
