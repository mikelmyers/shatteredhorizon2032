// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SHGameplayTypes.h"
#include "GameFramework/Character.h"
#include "SHPlayerCharacter.generated.h"

class UCameraComponent;
class USkeletalMeshComponent;
class USpringArmComponent;
class USHCameraSystem;
class USHHitFeedback;
class USHDeathSystem;
class USHDamageSystem;
class USHFatigueSystem;
class USHReverbZoneManager;
class USHAmbientSoundscape;
class USHCommsDisruption;
class USHFootstepSystem;
class ASHWeaponBase;
class UAnimInstance;

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
class USHLoadoutSystem;
class USHSquadManager;

UCLASS(Config = Game)
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
	virtual void Landed(const FHitResult& Hit) override;

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

	/** Camera feel system — head bob, ADS FOV, suppression FX, screen punch. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|Components")
	TObjectPtr<USHCameraSystem> CameraSystem;

	/** Hit feedback — screen punch, hit indicators, impact VFX. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|Components")
	TObjectPtr<USHHitFeedback> HitFeedback;

	/** Death physics — ragdoll with momentum transfer and body persistence. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|Components")
	TObjectPtr<USHDeathSystem> DeathSystem;

	/** Damage model — wounds, armor, bleeding, vital status. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|Components")
	TObjectPtr<USHDamageSystem> DamageSystemComp;

	/** Fatigue — stamina, long-term fatigue, breath control. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|Components")
	TObjectPtr<USHFatigueSystem> FatigueSystem;

	/** Dynamic reverb classification based on environment geometry. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|Components")
	TObjectPtr<USHReverbZoneManager> ReverbZoneManager;

	/** Layered ambient soundscape (wind, insects, distant battle). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|Components")
	TObjectPtr<USHAmbientSoundscape> AmbientSoundscape;

	/** Comms disruption — EW jamming effects on orders, compass drift, radio static. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|Components")
	TObjectPtr<USHCommsDisruption> CommsDisruption;

	/** Loadout selection/application (weapon registry + auto-equip for direct mission launch). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|Components")
	TObjectPtr<USHLoadoutSystem> LoadoutSystem;

	/** Squad manager — owns the fireteam roster and command interface. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|Components")
	TObjectPtr<USHSquadManager> SquadManager;

	/** Footstep audio + AI-noise (surface-aware). Driven by code stride detection
	 *  here (no anim notifies needed); plays nothing until sounds are imported, but
	 *  the MakeNoise -> AI-hearing path is live regardless. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|Components")
	TObjectPtr<USHFootstepSystem> FootstepSystem;

	/** First-person arms mesh applied at BeginPlay if none set (config-driven). */
	UPROPERTY(Config, EditDefaultsOnly, Category = "SH|FirstPerson")
	TSoftObjectPtr<USkeletalMesh> DefaultArmsMesh;

	/** Animation Blueprint for the first-person arms, applied at BeginPlay. Without a
	 *  posed AnimBP the arms render in bind pose at the camera origin (invisible), which
	 *  makes the weapon appear to float. Point this at an FP arms AnimBP matching the mesh. */
	UPROPERTY(Config, EditDefaultsOnly, Category = "SH|FirstPerson")
	TSoftClassPtr<UAnimInstance> DefaultArmsAnimClass;

	/** Relative placement applied to the equipped weapon when the arms mesh has no
	 *  'WeaponSocket' (keeps the viewmodel believable without authored sockets). */
	UPROPERTY(Config, EditDefaultsOnly, Category = "SH|FirstPerson")
	FVector WeaponViewLocation = FVector(35.f, 16.f, -14.f);

	UPROPERTY(Config, EditDefaultsOnly, Category = "SH|FirstPerson")
	FRotator WeaponViewRotation = FRotator(0.f, 0.f, 0.f);

protected:
	/** Deferred mission-spawn fixup: register pre-placed squad members with the
	 *  squad manager and settle the first-person weapon view transform. */
	void FinalizeMissionSpawn();

	FTimerHandle MissionSpawnTimerHandle;

public:

	/** Currently equipped weapon actor. Set via EquipWeapon(). */
	UPROPERTY(BlueprintReadOnly, Category = "SH|Weapon")
	TObjectPtr<ASHWeaponBase> EquippedWeapon;

	/** Equip a weapon — attaches to first-person arms, sets up delegates. */
	UFUNCTION(BlueprintCallable, Category = "SH|Weapon")
	void EquipWeapon(ASHWeaponBase* Weapon);

	/** Returns the currently equipped weapon (may be null). */
	UFUNCTION(BlueprintPure, Category = "SH|Weapon")
	ASHWeaponBase* GetEquippedWeapon() const { return EquippedWeapon; }

protected:
	// ------------------------------------------------------------------
	//  Tick helpers
	// ------------------------------------------------------------------
	void TickStamina(float DeltaSeconds);
	void TickSuppression(float DeltaSeconds);
	void TickBleeding(float DeltaSeconds);
	void TickLean(float DeltaSeconds);

	/** Code-driven stride detection: emits footsteps (sound + AI noise) at a
	 *  stance/speed-scaled stride length while grounded and moving. */
	void TickFootsteps(float DeltaSeconds);

	/** Distance accumulated since the last footstep (cm). */
	float StrideAccumCm = 0.f;

	/** Alternating foot for the next step. */
	bool bNextFootRight = false;

	/** Apply movement-feel tuning to the CharacterMovementComponent. Called from
	 *  BeginPlay (Live-Coding-proof) and the constructor (packaged builds). */
	void ApplyMovementTuning();

	/** Dev-only automated playtest driver: walks, then faces the nearest enemy and
	 *  fires bursts so the combat-feel systems (recoil, fire kick, hit markers, ADS)
	 *  can be exercised and screenshotted without a human at the controls. Config-gated
	 *  by bAutoPlaytest; never runs in normal play. */
	void TickAutoPlaytest(float DeltaSeconds);

	/** Find the nearest living enemy to the player (for the auto-playtest driver). */
	AActor* FindNearestEnemy() const;

	/** Enable the dev auto-playtest driver (set via -ini:Game override for QA runs). */
	UPROPERTY(Config, EditDefaultsOnly, Category = "SH|Dev")
	bool bAutoPlaytest = false;

	/** Seconds elapsed since the auto-playtest began. */
	float AutoPlaytestTime = 0.f;

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

	// ------------------------------------------------------------------
	//  Movement "feel" tuning (applied to the CharacterMovementComponent
	//  in BeginPlay so changes survive Live Coding iteration, which never
	//  re-runs the constructor). Engine defaults read as floaty/icy for an
	//  FPS — these give a grounded, intentional, gear-laden soldier feel.
	// ------------------------------------------------------------------

	/** Ground acceleration (cm/s^2). Higher = crisper start. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Movement|Feel")
	float MoveMaxAcceleration = 1800.f;

	/** Walking braking deceleration (cm/s^2). Lower = more momentum/settle on stop. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Movement|Feel")
	float MoveBrakingDeceleration = 1500.f;

	/** Ground friction while steering. Higher = grippier turns. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Movement|Feel")
	float MoveGroundFriction = 8.f;

	/** Separate braking friction (used when stopping). Slightly below ground
	 *  friction gives a believable weight-shift settle rather than a dead stop. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Movement|Feel")
	float MoveBrakingFriction = 6.f;

	/** Mid-air directional control (0 = none/floaty-locked, 1 = full). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Movement|Feel")
	float MoveAirControl = 0.4f;

	/** Jump launch velocity (cm/s). Modest for a loaded soldier. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Movement|Feel")
	float MoveJumpZVelocity = 450.f;

	/** Step height (cm) — how tall a ledge/curb can be auto-stepped. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Movement|Feel")
	float MoveMaxStepHeight = 50.f;

	/** Walkable floor angle (deg) — steepest slope treated as ground. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Movement|Feel")
	float MoveWalkableFloorAngle = 50.f;

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

	/** Most recent downward velocity while falling (cm/s), used to scale the
	 *  landing camera dip — captured each tick because Landed() arrives after
	 *  the movement component has zeroed vertical velocity. */
	float LastFallingZSpeed = 0.f;
	bool bIsFiring = false;
	bool bIsADS = false;
	bool bIsDead = false;
	bool bIsVaulting = false;
	bool bIsSliding = false;

	ESHLeanState ActiveLeanState = ESHLeanState::None;
	float CurrentLeanAlpha = 0.f; // -1 left, 0 center, +1 right

	int32 ActiveEquipmentSlot = 0;
};
