// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SHDamageSystem.generated.h"

class USHFatigueSystem;
class USHMoraleSystem;

// ─────────────────────────────────────────────────────────────
//  Enums
// ─────────────────────────────────────────────────────────────

/** Body hit zones. */
UENUM(BlueprintType)
enum class ESHHitZone : uint8
{
	Head	UMETA(DisplayName = "Head"),
	Torso	UMETA(DisplayName = "Torso"),
	LeftArm	UMETA(DisplayName = "Left Arm"),
	RightArm UMETA(DisplayName = "Right Arm"),
	LeftLeg	UMETA(DisplayName = "Left Leg"),
	RightLeg UMETA(DisplayName = "Right Leg")
};

/** Damage categories. */
UENUM(BlueprintType)
enum class ESHDamageType : uint8
{
	Ballistic		UMETA(DisplayName = "Ballistic"),
	Explosive		UMETA(DisplayName = "Explosive"),
	Fragmentation	UMETA(DisplayName = "Fragmentation"),
	Burn			UMETA(DisplayName = "Burn"),
	Concussion		UMETA(DisplayName = "Concussion")
};

/** Wound severity. */
UENUM(BlueprintType)
enum class ESHWoundSeverity : uint8
{
	None		UMETA(DisplayName = "None"),
	Scratch		UMETA(DisplayName = "Scratch"),
	Light		UMETA(DisplayName = "Light"),
	Serious		UMETA(DisplayName = "Serious"),
	Critical	UMETA(DisplayName = "Critical"),
	Fatal		UMETA(DisplayName = "Fatal")
};

/** Character vital status. */
UENUM(BlueprintType)
enum class ESHVitalStatus : uint8
{
	Healthy			UMETA(DisplayName = "Healthy"),
	Wounded			UMETA(DisplayName = "Wounded"),
	Incapacitated	UMETA(DisplayName = "Incapacitated"),
	Dead			UMETA(DisplayName = "Dead")
};

// ─────────────────────────────────────────────────────────────
//  Structs
// ─────────────────────────────────────────────────────────────

/** Individual wound tracked on a character. */
USTRUCT(BlueprintType)
struct FSHWound
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	ESHHitZone HitZone = ESHHitZone::Torso;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	ESHDamageType DamageType = ESHDamageType::Ballistic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	ESHWoundSeverity Severity = ESHWoundSeverity::None;

	/** Whether this wound is actively bleeding. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	bool bIsBleeding = false;

	/** Whether this wound has been treated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	bool bIsTreated = false;

	/** Bleed rate in health points per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float BleedRate = 0.f;

	/** World time the wound was received. */
	UPROPERTY()
	float TimeReceived = 0.f;
};

/** Armor plate slot. */
USTRUCT(BlueprintType)
struct FSHArmorPlate
{
	GENERATED_BODY()

	/** Which body zone this plate protects. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor")
	ESHHitZone CoveredZone = ESHHitZone::Torso;

	/** Current integrity [0..1]. 0 = destroyed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor")
	float Integrity = 1.f;

	/** Max damage this plate can absorb per hit at full integrity. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor")
	float MaxAbsorption = 60.f;

	/** Armor class rating; higher values stop more powerful rounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor")
	int32 ArmorClass = 3;

	/** Does this plate have anti-spall coating (reduces fragmentation). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Armor")
	bool bHasAntiSpall = false;
};

/** Incoming damage descriptor passed to ApplyDamage. */
USTRUCT(BlueprintType)
struct FSHDamageInfo
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	float BaseDamage = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	ESHDamageType DamageType = ESHDamageType::Ballistic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	ESHHitZone HitZone = ESHHitZone::Torso;

	/** Projectile armor penetration rating. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	int32 PenetrationRating = 2;

	/** Direction the damage came from (world space). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	FVector DamageDirection = FVector::ZeroVector;

	/** Impact location in world space. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	FVector ImpactLocation = FVector::ZeroVector;

	/** The actor that dealt the damage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	TWeakObjectPtr<AActor> Instigator = nullptr;

	/** Whether this is friendly fire. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Damage")
	bool bIsFriendlyFire = false;
};

/** Result of a damage application. */
USTRUCT(BlueprintType)
struct FSHDamageResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	float DamageDealt = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	float DamageAbsorbed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	ESHWoundSeverity WoundCreated = ESHWoundSeverity::None;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	bool bArmorHit = false;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	bool bArmorPenetrated = false;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	bool bIsLethal = false;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	bool bIsIncapacitating = false;

	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	ESHHitZone HitZone = ESHHitZone::Torso;

	/** True if this was a skull graze (glancing headshot). Triggers stagger, not death. */
	UPROPERTY(BlueprintReadOnly, Category = "Damage")
	bool bIsHeadGraze = false;
};

// ─────────────────────────────────────────────────────────────
//  Delegates
// ─────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnDamageReceived,
	const FSHDamageInfo&, DamageInfo,
	const FSHDamageResult&, Result);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnVitalStatusChanged,
	ESHVitalStatus, NewStatus);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnWoundAdded,
	const FSHWound&, Wound);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCharacterDied);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnCharacterIncapacitated);

// ─────────────────────────────────────────────────────────────
//  Component
// ─────────────────────────────────────────────────────────────

/**
 * USHDamageSystem
 *
 * Actor component that manages health, wounds, armor, and vital status
 * for any combatant character.
 *
 * Key design points:
 *  - No health regeneration; all healing requires first aid
 *  - Headshots are instantly lethal (no helmet stops rifle rounds)
 *  - Armor degrades per hit and can be defeated by sufficient penetration
 *  - Bleeding is a persistent state requiring treatment
 *  - Incapacitated characters can be stabilized before death
 *  - Friendly fire is always enabled
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHDamageSystem : public UActorComponent
{
	GENERATED_BODY()

public:
	USHDamageSystem();

	// ------------------------------------------------------------------
	//  UActorComponent overrides
	// ------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ------------------------------------------------------------------
	//  Damage application
	// ------------------------------------------------------------------

	/** Apply damage to this character. Returns the result of damage processing. */
	UFUNCTION(BlueprintCallable, Category = "SH|Damage")
	FSHDamageResult ApplyDamage(const FSHDamageInfo& DamageInfo);

	// ------------------------------------------------------------------
	//  Medical treatment
	// ------------------------------------------------------------------

	/**
	 * Apply a self-bandage to the most severe bleeding wound.
	 * Only works on Light wounds.
	 * @return true if a wound was treated.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Damage")
	bool ApplySelfBandage();

	/**
	 * Medic treatment: treats the most severe untreated wound.
	 * Can treat Serious and Critical wounds.
	 * @return true if a wound was treated.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Damage")
	bool ApplyMedicTreatment();

	/**
	 * Attempt to stabilize an incapacitated character.
	 * Stops bleeding and prevents death, but does not restore to combat-ready.
	 * @return true if stabilization succeeded.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Damage")
	bool Stabilize();

	/**
	 * Full revival from incapacitated to wounded state (medic only).
	 * @return true if the character was revived.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Damage")
	bool Revive();

	// ------------------------------------------------------------------
	//  Armor management
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "SH|Damage")
	void EquipArmorPlate(const FSHArmorPlate& Plate);

	UFUNCTION(BlueprintCallable, Category = "SH|Damage")
	void RemoveArmorPlate(ESHHitZone Zone);

	UFUNCTION(BlueprintPure, Category = "SH|Damage")
	bool HasArmorForZone(ESHHitZone Zone) const;

	UFUNCTION(BlueprintPure, Category = "SH|Damage")
	float GetArmorIntegrity(ESHHitZone Zone) const;

	// ------------------------------------------------------------------
	//  Queries
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|Damage")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "SH|Damage")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "SH|Damage")
	float GetHealthPercent() const { return MaxHealth > 0.f ? CurrentHealth / MaxHealth : 0.f; }

	UFUNCTION(BlueprintPure, Category = "SH|Damage")
	ESHVitalStatus GetVitalStatus() const { return VitalStatus; }

	UFUNCTION(BlueprintPure, Category = "SH|Damage")
	bool IsAlive() const { return VitalStatus != ESHVitalStatus::Dead; }

	UFUNCTION(BlueprintPure, Category = "SH|Damage")
	bool IsIncapacitated() const { return VitalStatus == ESHVitalStatus::Incapacitated; }

	UFUNCTION(BlueprintPure, Category = "SH|Damage")
	bool IsBleeding() const;

	UFUNCTION(BlueprintPure, Category = "SH|Damage")
	float GetTotalBleedRate() const;

	UFUNCTION(BlueprintPure, Category = "SH|Damage")
	const TArray<FSHWound>& GetWounds() const { return ActiveWounds; }

	/** Get the worst wound severity on a specific body zone. */
	UFUNCTION(BlueprintPure, Category = "SH|Damage")
	ESHWoundSeverity GetWorstWoundOnZone(ESHHitZone Zone) const;

	/** Aim penalty from arm wounds (1.0 = no penalty). */
	UFUNCTION(BlueprintPure, Category = "SH|Damage")
	float GetAimPenaltyMultiplier() const;

	/** Movement penalty from leg wounds (1.0 = no penalty). */
	UFUNCTION(BlueprintPure, Category = "SH|Damage")
	float GetMovementPenaltyMultiplier() const;

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Damage")
	FOnDamageReceived OnDamageReceived;

	UPROPERTY(BlueprintAssignable, Category = "SH|Damage")
	FOnVitalStatusChanged OnVitalStatusChanged;

	UPROPERTY(BlueprintAssignable, Category = "SH|Damage")
	FOnWoundAdded OnWoundAdded;

	UPROPERTY(BlueprintAssignable, Category = "SH|Damage")
	FOnCharacterDied OnCharacterDied;

	UPROPERTY(BlueprintAssignable, Category = "SH|Damage")
	FOnCharacterIncapacitated OnCharacterIncapacitated;

protected:
	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, Category = "SH|Damage|Config")
	float MaxHealth = 100.f;

	/** Zone damage multipliers (e.g. Head = 10.0 for guaranteed kill). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Damage|Config")
	TMap<ESHHitZone, float> ZoneDamageMultipliers;

	/** Bleed-out time in seconds when incapacitated and not stabilized. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Damage|Config")
	float IncapacitatedBleedOutTime = 120.f;

	/** Health threshold below which the character becomes incapacitated instead of dying. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Damage|Config")
	float IncapacitationThreshold = 15.f;

	/** Health restored on successful revive. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Damage|Config")
	float ReviveHealthAmount = 25.f;

private:
	// ------------------------------------------------------------------
	//  Internal
	// ------------------------------------------------------------------
	FSHDamageResult ProcessDamage(const FSHDamageInfo& DamageInfo);
	float ApplyArmorReduction(float IncomingDamage, ESHHitZone Zone, int32 PenetrationRating, bool& bOutArmorHit, bool& bOutPenetrated);
	ESHWoundSeverity DetermineWoundSeverity(float DamageDealt, ESHHitZone Zone) const;
	FSHWound CreateWound(const FSHDamageInfo& DamageInfo, ESHWoundSeverity Severity);
	void TickBleeding(float DeltaTime);
	void TickIncapacitated(float DeltaTime);
	void UpdateVitalStatus();
	void NotifyLinkedSystems(const FSHDamageInfo& DamageInfo, const FSHDamageResult& Result);
	void InitDefaultZoneMultipliers();

	UPROPERTY()
	float CurrentHealth = 100.f;

	UPROPERTY()
	ESHVitalStatus VitalStatus = ESHVitalStatus::Healthy;

	UPROPERTY()
	TArray<FSHWound> ActiveWounds;

	UPROPERTY()
	TArray<FSHArmorPlate> EquippedArmor;

	/** Time remaining before an incapacitated character dies. */
	float IncapacitatedTimer = 0.f;

	/** Whether the character has been stabilized while incapacitated. */
	bool bIsStabilized = false;

	/** Cached references to sibling components. */
	UPROPERTY()
	TObjectPtr<USHFatigueSystem> CachedFatigueSystem = nullptr;

	UPROPERTY()
	TObjectPtr<USHMoraleSystem> CachedMoraleSystem = nullptr;
};
