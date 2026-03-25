// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SHPlayerController.h"
#include "SHPlayerCharacter.generated.h"

class UCameraComponent;
class USkeletalMeshComponent;
class USpringArmComponent;

// Forward declare the lean enum from the controller.
enum class ESHLeanState : uint8;

/** Player stance. */
UENUM(BlueprintType)
enum class ESHStance : uint8
{
	Standing,
	Crouching,
	Prone
};

/** Limb identifiers for the injury system. */
UENUM(BlueprintType)
enum class ESHLimb : uint8
{
	Head,
	Torso,
	LeftArm,
	RightArm,
	LeftLeg,
	RightLeg
};

/** Per-limb injury state. */
USTRUCT(BlueprintType)
struct FSHLimbState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESHLimb Limb = ESHLimb::Torso;

	/** 0 = destroyed, 1 = healthy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Health = 1.f;

	/** If true, this limb is bleeding and will drain HP over time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsBleeding = false;

	/** If true, a tourniquet/splint has been applied. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsTreated = false;
};

/** Equipment slot descriptor. */
UENUM(BlueprintType)
enum class ESHEquipmentSlot : uint8
{
	PrimaryWeapon,
	Sidearm,
	Grenade,
	Gear1,
	Gear2,
	Max UMETA(Hidden)
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSHOnHealthChanged, float, CurrentHP, float, MaxHP);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHOnStaminaChanged, float, NormalizedStamina);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHOnSuppressionChanged, float, SuppressionLevel);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSHOnLimbDamaged, ESHLimb, Limb, float, RemainingHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSHOnPlayerDeath);

/**
 * ASHPlayerCharacter
 *
 * First-person player character for the squad leader.
 * Features a full milsim movement set (sprint, crouch, prone, vault, slide),
 * stamina/fatigue, non-regenerating health, limb-based injury,
 * weight-dependent mobility, suppression response, and equipment slots.
 */
UCLASS()
class SHATTEREDHORIZON2032_API ASHPlayerCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASHPlayerCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	// ------------------------------------------------------------------
	//  Movement actions (called from controller)
	// ------------------------------------------------------------------

	void StartSprint();
	void StopSprint();
	void ToggleCrouch();
	void ToggleProne();
	void TryVault();
	void TrySlide();
	void SetLeanState(ESHLeanState InLeanState);

	// ------------------------------------------------------------------
	//  Combat actions
	// ------------------------------------------------------------------

	void StartFire();
	void StopFire();
	void StartADS();
	void StopADS();
	void Reload();
	void EquipSlot(int32 SlotIndex);

	// ------------------------------------------------------------------
	//  Health / injury
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|Health")
	float GetCurrentHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "SH|Health")
	float GetMaxHealth() const { return MaxHealth; }

	UFUNCTION(BlueprintPure, Category = "SH|Health")
	bool IsAlive() const { return CurrentHealth > 0.f; }

	/** Apply healing from a medic or aid kit. Does NOT regenerate — external source only. */
	UFUNCTION(BlueprintCallable, Category = "SH|Health")
	void ApplyHealing(float Amount);

	/** Apply treatment to a specific limb (tourniquet, splint). */
	UFUNCTION(BlueprintCallable, Category = "SH|Health")
	void TreatLimb(ESHLimb Limb);

	UFUNCTION(BlueprintPure, Category = "SH|Health")
	const TArray<FSHLimbState>& GetLimbStates() const { return LimbStates; }

	// ------------------------------------------------------------------
	//  Stamina
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|Stamina")
	float GetStaminaNormalized() const { return Stamina / MaxStamina; }

	// ------------------------------------------------------------------
	//  Suppression
	// ------------------------------------------------------------------

	/** Add suppression from incoming fire. Decays over time. */
	UFUNCTION(BlueprintCallable, Category = "SH|Suppression")
	void AddSuppression(float Amount);

	UFUNCTION(BlueprintPure, Category = "SH|Suppression")
	float GetSuppressionLevel() const { return SuppressionLevel; }

	// ------------------------------------------------------------------
	//  Weight system
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|Weight")
	float GetCurrentWeight() const { return CurrentWeight; }

	/** Recalculate weight from equipped items and apply movement penalties. */
	UFUNCTION(BlueprintCallable, Category = "SH|Weight")
	void RecalculateWeight();

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Health")
	FSHOnHealthChanged OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "SH|Stamina")
	FSHOnStaminaChanged OnStaminaChanged;

	UPROPERTY(BlueprintAssignable, Category = "SH|Suppression")
	FSHOnSuppressionChanged OnSuppressionChanged;

	UPROPERTY(BlueprintAssignable, Category = "SH|Health")
	FSHOnLimbDamaged OnLimbDamaged;

	UPROPERTY(BlueprintAssignable, Category = "SH|Health")
	FSHOnPlayerDeath OnPlayerDeath;

	// ------------------------------------------------------------------
	//  Components — public for BP access
	// ------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|Components")
	TObjectPtr<UCameraComponent> FirstPersonCamera;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|Components")
	TObjectPtr<USkeletalMeshComponent> FirstPersonArms;

protected:
	// ------------------------------------------------------------------
	//  Tick helpers
	// ------------------------------------------------------------------
	void TickStamina(float DeltaSeconds);
	void TickSuppression(float DeltaSeconds);
	void TickBleeding(float DeltaSeconds);
	void TickLean(float DeltaSeconds);

	/** Perform a vault trace and execute if geometry permits. */
	bool CanVault() const;
	void ExecuteVault();

	/** Internal death handling. */
	void Die();

	/** Apply limb-specific damage and compute injury effects. */
	void ApplyLimbDamage(ESHLimb Limb, float Damage);

	/** Get movement speed multiplier based on injuries and weight. */
	float GetMovementSpeedMultiplier() const;

	/** Get aim sway multiplier based on injuries, suppression, and fatigue. */
	float GetAimSwayMultiplier() const;

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, Category = "SH|Health")
	float MaxHealth = 100.f;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Stamina")
	float MaxStamina = 100.f;

	/** Stamina drain per second while sprinting. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Stamina")
	float SprintStaminaDrain = 15.f;

	/** Stamina recovery per second while not sprinting. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Stamina")
	float StaminaRecoveryRate = 8.f;

	/** Suppression decay per second. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Suppression")
	float SuppressionDecayRate = 0.25f;

	/** Max suppression value. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Suppression")
	float MaxSuppression = 1.f;

	/** Bleeding damage per second per bleeding limb. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Health")
	float BleedDamagePerSecond = 2.f;

	/** Base walk speed (cm/s). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Movement")
	float BaseWalkSpeed = 350.f;

	/** Sprint speed multiplier. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Movement")
	float SprintSpeedMultiplier = 1.8f;

	/** Crouch speed multiplier. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Movement")
	float CrouchSpeedMultiplier = 0.5f;

	/** Prone speed multiplier. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Movement")
	float ProneSpeedMultiplier = 0.25f;

	/** Maximum carry weight (kg) before movement is severely impaired. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Weight")
	float MaxCarryWeight = 45.f;

	/** Vault trace distance (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Movement")
	float VaultTraceDistance = 150.f;

	/** Vault max height (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Movement")
	float VaultMaxHeight = 120.f;

	/** Lean offset distance (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Movement")
	float LeanOffsetDistance = 35.f;

	/** Lean rotation angle (degrees). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Movement")
	float LeanAngleDeg = 15.f;

	/** Lean interpolation speed. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Movement")
	float LeanInterpSpeed = 10.f;

private:
	// ------------------------------------------------------------------
	//  Runtime state
	// ------------------------------------------------------------------

	UPROPERTY(Replicated)
	float CurrentHealth = 100.f;

	UPROPERTY(Replicated)
	float Stamina = 100.f;

	UPROPERTY(Replicated)
	ESHStance CurrentStance = ESHStance::Standing;

	UPROPERTY(Replicated)
	TArray<FSHLimbState> LimbStates;

	float SuppressionLevel = 0.f;
	float CurrentWeight = 10.f; // base weight of the character's gear in kg

	bool bIsSprinting = false;
	bool bIsFiring = false;
	bool bIsADS = false;
	bool bIsDead = false;
	bool bIsVaulting = false;
	bool bIsSliding = false;

	ESHLeanState ActiveLeanState = ESHLeanState::None;
	float CurrentLeanAlpha = 0.f; // -1 left, 0 center, +1 right

	int32 ActiveEquipmentSlot = 0;
};
