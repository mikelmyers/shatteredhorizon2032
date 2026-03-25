// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SHProgressionSystem.h"
#include "SHLoadoutSystem.generated.h"

class ASHPlayerCharacter;
class USHProgressionSystem;

/** Attachment slot type. */
UENUM(BlueprintType)
enum class ESHAttachmentSlot : uint8
{
	Optic		UMETA(DisplayName = "Optic"),
	Barrel		UMETA(DisplayName = "Barrel"),
	Grip		UMETA(DisplayName = "Grip"),
	Magazine	UMETA(DisplayName = "Magazine"),
	Muzzle		UMETA(DisplayName = "Muzzle")
};

/** Descriptor for a weapon attachment. */
USTRUCT(BlueprintType)
struct FSHAttachmentData
{
	GENERATED_BODY()

	/** Unique attachment identifier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Attachment")
	FName AttachmentId;

	/** Human-readable display name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Attachment")
	FText DisplayName;

	/** Which slot this attachment occupies. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Attachment")
	ESHAttachmentSlot Slot = ESHAttachmentSlot::Optic;

	/** Minimum rank required to use this attachment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Attachment")
	ESHRank RequiredRank = ESHRank::Private;

	/** Weight in kilograms. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Attachment", meta = (ClampMin = "0"))
	float WeightKg = 0.2f;

	/** Weapon IDs this attachment is compatible with. Empty means compatible with all. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout|Attachment")
	TArray<FName> CompatibleWeapons;
};

/** Full loadout configuration selected by the player before a mission. */
USTRUCT(BlueprintType)
struct FSHLoadout
{
	GENERATED_BODY()

	/** Primary weapon identifier (e.g. "M27_IAR", "M4A1"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FName PrimaryWeapon;

	/** Attachments on the primary weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	TArray<FName> PrimaryAttachments;

	/** Secondary weapon identifier (e.g. "M17_SIG"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FName SecondaryWeapon;

	/** Equipment items (grenades, medical kits, C4, etc.). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	TArray<FName> Equipment;

	/** Camouflage pattern identifier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Loadout")
	FName CamoPattern;
};

/** Validation result returned by ValidateLoadout(). */
USTRUCT(BlueprintType)
struct FSHLoadoutValidationResult
{
	GENERATED_BODY()

	/** Whether the loadout passed all validation checks. */
	UPROPERTY(BlueprintReadOnly, Category = "Loadout|Validation")
	bool bIsValid = true;

	/** List of human-readable issues found during validation. */
	UPROPERTY(BlueprintReadOnly, Category = "Loadout|Validation")
	TArray<FText> Issues;
};

// Delegate
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnLoadoutChanged, const FSHLoadout&, NewLoadout);

/**
 * USHLoadoutSystem
 *
 * Actor component for pre-mission loadout selection and management.
 * Validates that selected weapons, attachments, and equipment are
 * unlocked in the progression system, compatible with each other,
 * and within the 45 kg doctrinal carry weight limit.
 *
 * Attach to the player controller or player state to manage loadout
 * selection during the pre-mission phase.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHLoadoutSystem : public UActorComponent
{
	GENERATED_BODY()

public:
	USHLoadoutSystem();

protected:
	virtual void BeginPlay() override;

public:
	// ------------------------------------------------------------------
	//  Loadout management
	// ------------------------------------------------------------------

	/** Set the full loadout. Broadcasts OnLoadoutChanged if valid. */
	UFUNCTION(BlueprintCallable, Category = "SH|Loadout")
	void SetLoadout(const FSHLoadout& InLoadout);

	/** Get the current loadout. */
	UFUNCTION(BlueprintPure, Category = "SH|Loadout")
	const FSHLoadout& GetCurrentLoadout() const { return CurrentLoadout; }

	/** Validate the current loadout against progression unlocks and weight limits. */
	UFUNCTION(BlueprintCallable, Category = "SH|Loadout")
	FSHLoadoutValidationResult ValidateLoadout() const;

	/** Validate an arbitrary loadout. */
	UFUNCTION(BlueprintCallable, Category = "SH|Loadout")
	FSHLoadoutValidationResult ValidateLoadoutData(const FSHLoadout& InLoadout) const;

	// ------------------------------------------------------------------
	//  Attachment queries
	// ------------------------------------------------------------------

	/** Get all attachments compatible with a given weapon. */
	UFUNCTION(BlueprintCallable, Category = "SH|Loadout")
	TArray<FSHAttachmentData> GetAvailableAttachments(FName WeaponId) const;

	/** Find attachment data by ID. Returns nullptr if not found. */
	const FSHAttachmentData* FindAttachmentData(FName AttachmentId) const;

	// ------------------------------------------------------------------
	//  Weight calculations
	// ------------------------------------------------------------------

	/** Get the total weight of the current loadout in kilograms. */
	UFUNCTION(BlueprintPure, Category = "SH|Loadout")
	float GetTotalWeight() const;

	/** Calculate total weight for an arbitrary loadout. */
	UFUNCTION(BlueprintPure, Category = "SH|Loadout")
	float CalculateLoadoutWeight(const FSHLoadout& InLoadout) const;

	/** Get the maximum carry weight per doctrinal standard (45 kg). */
	UFUNCTION(BlueprintPure, Category = "SH|Loadout")
	float GetMaxCarryWeight() const { return MaxCarryWeight; }

	// ------------------------------------------------------------------
	//  Character application
	// ------------------------------------------------------------------

	/** Apply the current loadout to the player character's weapon and equipment slots. */
	UFUNCTION(BlueprintCallable, Category = "SH|Loadout")
	void ApplyLoadoutToCharacter();

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Maximum carry weight in kilograms per USMC doctrinal standard. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Loadout")
	float MaxCarryWeight = 45.f;

	/** Master list of all weapon data (ID -> weight in kg). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Loadout")
	TMap<FName, float> WeaponWeights;

	/** Master list of all equipment data (ID -> weight in kg). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Loadout")
	TMap<FName, float> EquipmentWeights;

	/** Master list of all available attachments. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Loadout")
	TArray<FSHAttachmentData> AllAttachments;

	// ------------------------------------------------------------------
	//  Delegate
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Loadout")
	FOnLoadoutChanged OnLoadoutChanged;

private:
	/** Get weapon weight by ID. Returns 0 if not found. */
	float GetWeaponWeight(FName WeaponId) const;

	/** Get equipment item weight by ID. Returns 0 if not found. */
	float GetEquipmentWeight(FName EquipmentId) const;

	/** Get the progression system from the game instance. */
	USHProgressionSystem* GetProgressionSystem() const;

	/** Initialize default weapon and equipment weight tables. */
	void InitializeDefaultWeights();

	/** Initialize the default attachment database. */
	void InitializeDefaultAttachments();

	UPROPERTY()
	FSHLoadout CurrentLoadout;
};
