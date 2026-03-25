// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SHWeaponData.h"
#include "SHWeaponBase.generated.h"

class USHBallisticsSystem;
class ASHProjectile;
class USkeletalMeshComponent;
class UNiagaraComponent;

// ---------------------------------------------------------------------------
// Weapon state enums
// ---------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ESHWeaponState : uint8
{
	Idle       UMETA(DisplayName = "Idle"),
	Firing     UMETA(DisplayName = "Firing"),
	Reloading  UMETA(DisplayName = "Reloading"),
	Switching  UMETA(DisplayName = "Switching Fire Mode"),
	Malfunction UMETA(DisplayName = "Malfunction"),
	ClearingJam UMETA(DisplayName = "Clearing Jam"),
	Equipping  UMETA(DisplayName = "Equipping"),
	Unequipping UMETA(DisplayName = "Unequipping"),
};

UENUM(BlueprintType)
enum class ESHReloadPhase : uint8
{
	None       UMETA(DisplayName = "None"),
	Start      UMETA(DisplayName = "Start"),
	RemoveMag  UMETA(DisplayName = "Remove Magazine"),
	InsertMag  UMETA(DisplayName = "Insert Magazine"),
	Chamber    UMETA(DisplayName = "Chamber Round"),
	Ready      UMETA(DisplayName = "Ready"),
};

// ---------------------------------------------------------------------------
// Delegates
// ---------------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHOnWeaponFired, int32, RemainingAmmo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSHOnWeaponReloadStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSHOnWeaponReloadFinished);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSHOnWeaponMalfunction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHOnFireModeChanged, ESHFireMode, NewMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSHOnWeaponOverheated);

// ---------------------------------------------------------------------------
// ASHWeaponBase
// ---------------------------------------------------------------------------

UCLASS(Abstract)
class SHATTEREDHORIZON2032_API ASHWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	ASHWeaponBase();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	// -------------------------------------------------------------------
	// Weapon data
	// -------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<USHWeaponData> WeaponData;

	// -------------------------------------------------------------------
	// Input actions (called by owning character)
	// -------------------------------------------------------------------

	/** Begin firing (trigger pull). */
	UFUNCTION(BlueprintCallable, Category = "SH|Weapon")
	void StartFire();

	/** Stop firing (trigger release). */
	UFUNCTION(BlueprintCallable, Category = "SH|Weapon")
	void StopFire();

	/** Initiate reload. */
	UFUNCTION(BlueprintCallable, Category = "SH|Weapon")
	void StartReload();

	/** Cancel reload (e.g., sprint interrupt). */
	UFUNCTION(BlueprintCallable, Category = "SH|Weapon")
	void CancelReload();

	/** Cycle to the next available fire mode. */
	UFUNCTION(BlueprintCallable, Category = "SH|Weapon")
	void CycleFireMode();

	/** Begin clearing a weapon malfunction. */
	UFUNCTION(BlueprintCallable, Category = "SH|Weapon")
	void StartClearMalfunction();

	/** Enter ADS. */
	UFUNCTION(BlueprintCallable, Category = "SH|Weapon")
	void StartADS();

	/** Exit ADS. */
	UFUNCTION(BlueprintCallable, Category = "SH|Weapon")
	void StopADS();

	// -------------------------------------------------------------------
	// Queries
	// -------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|Weapon")
	int32 GetCurrentAmmo() const { return CurrentMagAmmo; }

	UFUNCTION(BlueprintPure, Category = "SH|Weapon")
	int32 GetReserveAmmo() const { return ReserveAmmo; }

	UFUNCTION(BlueprintPure, Category = "SH|Weapon")
	ESHWeaponState GetWeaponState() const { return WeaponState; }

	UFUNCTION(BlueprintPure, Category = "SH|Weapon")
	ESHFireMode GetCurrentFireMode() const { return CurrentFireMode; }

	UFUNCTION(BlueprintPure, Category = "SH|Weapon")
	bool IsAiming() const { return bIsADS; }

	UFUNCTION(BlueprintPure, Category = "SH|Weapon")
	float GetHeatLevel() const { return CurrentHeat; }

	UFUNCTION(BlueprintPure, Category = "SH|Weapon")
	float GetADSAlpha() const { return ADSAlpha; }

	UFUNCTION(BlueprintPure, Category = "SH|Weapon")
	bool CanFire() const;

	UFUNCTION(BlueprintPure, Category = "SH|Weapon")
	bool CanReload() const;

	// -------------------------------------------------------------------
	// External state inputs (set by owning character each frame)
	// -------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "SH|Weapon")
	void SetOwnerStance(ESHStance InStance) { CurrentStance = InStance; }

	UFUNCTION(BlueprintCallable, Category = "SH|Weapon")
	void SetOwnerMovementSpeed(float SpeedCmS) { OwnerSpeed = SpeedCmS; }

	UFUNCTION(BlueprintCallable, Category = "SH|Weapon")
	void SetSuppressionLevel(float Level) { SuppressionLevel = FMath::Clamp(Level, 0.0f, 100.0f); }

	UFUNCTION(BlueprintCallable, Category = "SH|Weapon")
	void SetFatigueLevel(float Level) { FatigueLevel = FMath::Clamp(Level, 0.0f, 100.0f); }

	UFUNCTION(BlueprintCallable, Category = "SH|Weapon")
	void SetDirtLevel(float Level) { DirtLevel = FMath::Clamp(Level, 0.0f, 100.0f); }

	UFUNCTION(BlueprintCallable, Category = "SH|Weapon")
	void SetStamina(float Stamina01) { StaminaNormalized = FMath::Clamp(Stamina01, 0.0f, 1.0f); }

	// -------------------------------------------------------------------
	// Delegates
	// -------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Weapon|Events")
	FSHOnWeaponFired OnWeaponFired;

	UPROPERTY(BlueprintAssignable, Category = "SH|Weapon|Events")
	FSHOnWeaponReloadStarted OnReloadStarted;

	UPROPERTY(BlueprintAssignable, Category = "SH|Weapon|Events")
	FSHOnWeaponReloadFinished OnReloadFinished;

	UPROPERTY(BlueprintAssignable, Category = "SH|Weapon|Events")
	FSHOnWeaponMalfunction OnMalfunction;

	UPROPERTY(BlueprintAssignable, Category = "SH|Weapon|Events")
	FSHOnFireModeChanged OnFireModeChanged;

	UPROPERTY(BlueprintAssignable, Category = "SH|Weapon|Events")
	FSHOnWeaponOverheated OnOverheated;

	// -------------------------------------------------------------------
	// Components
	// -------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> WeaponMeshComp;

protected:
	// -------------------------------------------------------------------
	// State
	// -------------------------------------------------------------------

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	ESHWeaponState WeaponState = ESHWeaponState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	ESHFireMode CurrentFireMode = ESHFireMode::Semi;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	ESHReloadPhase ReloadPhase = ESHReloadPhase::None;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	int32 CurrentMagAmmo = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	int32 ReserveAmmo = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	float CurrentHeat = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	bool bIsADS = false;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	float ADSAlpha = 0.0f;

	// --- Owner state ---
	ESHStance CurrentStance = ESHStance::Standing;
	float OwnerSpeed = 0.0f;
	float SuppressionLevel = 0.0f;
	float FatigueLevel = 0.0f;
	float DirtLevel = 0.0f;
	float StaminaNormalized = 1.0f;

	// --- Fire timing ---
	bool bWantsToFire = false;
	float TimeSinceLastShot = 0.0f;
	int32 ConsecutiveShotCount = 0;
	int32 BurstShotsRemaining = 0;
	float ShotCooldownTimer = 0.0f;
	int32 TotalShotsFired = 0; // for tracer interval

	// --- Recoil state ---
	float AccumulatedVerticalRecoil = 0.0f;
	float AccumulatedHorizontalRecoil = 0.0f;
	float RecoilRecoveryTimer = 0.0f;

	// --- Weapon sway ---
	float SwayTime = 0.0f;

	// --- Reload timer ---
	float ReloadTimer = 0.0f;
	float ReloadDuration = 0.0f;
	bool bWasEmptyReload = false;

	// --- Malfunction ---
	float MalfunctionClearTimer = 0.0f;

	// --- Cached subsystem ---
	UPROPERTY(Transient)
	TObjectPtr<USHBallisticsSystem> BallisticsSystem;

	// -------------------------------------------------------------------
	// Internal methods
	// -------------------------------------------------------------------

	/** Attempt to fire a single round. */
	void FireShot();

	/** Determine spread cone half-angle in degrees. */
	float CalculateSpreadDegrees() const;

	/** Apply recoil to the owning controller. */
	void ApplyRecoil();

	/** Tick recoil recovery. */
	void TickRecoilRecovery(float DeltaTime);

	/** Get the muzzle world transform. */
	FTransform GetMuzzleTransform() const;

	/** Spawn a projectile. */
	void SpawnProjectile(const FVector& Origin, const FVector& Direction);

	/** Perform hitscan fire. */
	void PerformHitscanFire(const FVector& Origin, const FVector& Direction);

	/** Calculate final fire direction with spread applied. */
	FVector ApplySpreadToDirection(const FVector& BaseDirection) const;

	/** Roll for weapon malfunction. Returns true if weapon jammed. */
	bool RollForMalfunction();

	/** Tick the reload state machine. */
	void TickReload(float DeltaTime);

	/** Tick heat dissipation. */
	void TickHeat(float DeltaTime);

	/** Tick ADS interpolation. */
	void TickADS(float DeltaTime);

	/** Tick weapon sway. */
	FRotator CalculateWeaponSway(float DeltaTime);

	/** Tick fire cooldown and burst logic. */
	void TickFiring(float DeltaTime);

	/** Play fire VFX (muzzle flash, shell eject). */
	void PlayFireEffects();

	/** Play fire sound. */
	void PlayFireSound();

	/** Play dry fire sound. */
	void PlayDryFireSound();

	/** Play first-person animation montage. */
	void PlayWeaponMontage(UAnimMontage* Montage);

	/** Get socket name for muzzle. */
	static FName GetMuzzleSocketName() { return FName(TEXT("Muzzle")); }

	/** Get socket name for shell ejection. */
	static FName GetShellEjectSocketName() { return FName(TEXT("ShellEject")); }
};
