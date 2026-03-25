// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SHWeaponData.h"
#include "SHWeaponBase.generated.h"

class USHWeaponDataAsset;
class ASHProjectile;
class USHBallisticsSystem;
class USHWeaponAnimSystem;

/* -----------------------------------------------------------------------
 *  Reload state machine
 * --------------------------------------------------------------------- */

UENUM(BlueprintType)
enum class ESHReloadState : uint8
{
	Idle        UMETA(DisplayName = "Idle"),
	Starting    UMETA(DisplayName = "Starting"),
	Inserting   UMETA(DisplayName = "Inserting Magazine / Shell"),
	Chambering  UMETA(DisplayName = "Chambering"),
	Finishing   UMETA(DisplayName = "Finishing"),
};

UENUM(BlueprintType)
enum class ESHWeaponState : uint8
{
	Idle            UMETA(DisplayName = "Idle"),
	Firing          UMETA(DisplayName = "Firing"),
	Reloading       UMETA(DisplayName = "Reloading"),
	Switching       UMETA(DisplayName = "Switching"),
	Malfunctioned   UMETA(DisplayName = "Malfunctioned"),
	Overheated      UMETA(DisplayName = "Overheated"),
	Equipping       UMETA(DisplayName = "Equipping"),
};

/* -----------------------------------------------------------------------
 *  Delegates
 * --------------------------------------------------------------------- */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSHOnAmmoChanged, int32, CurrentMag, int32, Reserve);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHOnFireModeChanged, ESHFireMode, NewMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHOnWeaponStateChanged, ESHWeaponState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSHOnWeaponFired);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSHOnMalfunction);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHOnHeatChanged, float, NormalizedHeat);

/* -----------------------------------------------------------------------
 *  ASHWeaponBase
 * --------------------------------------------------------------------- */

UCLASS(Abstract, Blueprintable)
class SHATTEREDHORIZON2032_API ASHWeaponBase : public AActor
{
	GENERATED_BODY()

public:
	ASHWeaponBase();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/* --- Configuration --- */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon|Data")
	TObjectPtr<USHWeaponDataAsset> WeaponData;

	/* --- Components --- */

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	TObjectPtr<USkeletalMeshComponent> WeaponMeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Weapon|Components")
	TObjectPtr<USHWeaponAnimSystem> WeaponAnimSystem;

	/* --- Input Actions --- */

	/** Call to begin firing (press). */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Actions")
	void StartFire();

	/** Call to stop firing (release). */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Actions")
	void StopFire();

	/** Begin reload. */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Actions")
	void StartReload();

	/** Cancel an in-progress reload (e.g., single-round reload interrupted by fire). */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Actions")
	void CancelReload();

	/** Cycle to the next available fire mode. */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Actions")
	void CycleFireMode();

	/** Begin aiming down sights. */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Actions")
	void StartADS();

	/** Release ADS. */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Actions")
	void StopADS();

	/** Attempt to clear a malfunction. */
	UFUNCTION(BlueprintCallable, Category = "Weapon|Actions")
	void ClearMalfunction();

	/* --- Queries --- */

	UFUNCTION(BlueprintPure, Category = "Weapon|State")
	int32 GetCurrentMagAmmo() const { return CurrentMagAmmo; }

	UFUNCTION(BlueprintPure, Category = "Weapon|State")
	int32 GetReserveAmmo() const { return ReserveAmmo; }

	UFUNCTION(BlueprintPure, Category = "Weapon|State")
	ESHFireMode GetCurrentFireMode() const { return CurrentFireMode; }

	UFUNCTION(BlueprintPure, Category = "Weapon|State")
	ESHWeaponState GetWeaponState() const { return WeaponState; }

	UFUNCTION(BlueprintPure, Category = "Weapon|State")
	bool IsADS() const { return bIsADS; }

	UFUNCTION(BlueprintPure, Category = "Weapon|State")
	float GetNormalizedHeat() const { return CurrentHeat; }

	UFUNCTION(BlueprintPure, Category = "Weapon|State")
	bool IsMalfunctioned() const { return WeaponState == ESHWeaponState::Malfunctioned; }

	/* --- External state setters (driven by character / player controller) --- */

	/** Set current stance for accuracy calculations. */
	UFUNCTION(BlueprintCallable, Category = "Weapon|State")
	void SetStance(ESHStance InStance) { CurrentStance = InStance; }

	/** Set whether the character is moving. */
	UFUNCTION(BlueprintCallable, Category = "Weapon|State")
	void SetIsMoving(bool bMoving) { bIsMoving = bMoving; }

	/** Set suppression level 0-1. */
	UFUNCTION(BlueprintCallable, Category = "Weapon|State")
	void SetSuppressionLevel(float Level) { SuppressionLevel = FMath::Clamp(Level, 0.0f, 1.0f); }

	/** Set fatigue level 0-1 (from stamina system). */
	UFUNCTION(BlueprintCallable, Category = "Weapon|State")
	void SetFatigueLevel(float Level) { FatigueLevel = FMath::Clamp(Level, 0.0f, 1.0f); }

	/** Set dirt/fouling level 0-1 (increases malfunction chance). */
	UFUNCTION(BlueprintCallable, Category = "Weapon|State")
	void SetDirtLevel(float Level) { DirtLevel = FMath::Clamp(Level, 0.0f, 1.0f); }

	/** Set ammo directly (e.g., on pickup). */
	UFUNCTION(BlueprintCallable, Category = "Weapon|State")
	void SetAmmo(int32 MagAmmo, int32 InReserve);

	/* --- Delegates --- */

	UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
	FSHOnAmmoChanged OnAmmoChanged;

	UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
	FSHOnFireModeChanged OnFireModeChanged;

	UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
	FSHOnWeaponStateChanged OnWeaponStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
	FSHOnWeaponFired OnWeaponFired;

	UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
	FSHOnMalfunction OnMalfunction;

	UPROPERTY(BlueprintAssignable, Category = "Weapon|Events")
	FSHOnHeatChanged OnHeatChanged;

protected:
	/* --- Internal Fire Logic --- */

	/** Execute a single shot (hitscan or projectile). */
	virtual void FireShot();

	/** Perform a hitscan trace for close-range fire. */
	void ExecuteHitscan(const FVector& MuzzleLocation, const FVector& ShotDirection);

	/** Spawn a projectile actor for distance fire. */
	void SpawnProjectile(const FVector& MuzzleLocation, const FVector& ShotDirection);

	/** Calculate spread-affected direction from base direction. */
	FVector ApplySpread(const FVector& BaseDirection) const;

	/** Get the current MOA spread value accounting for all modifiers. */
	float CalculateCurrentMOA() const;

	/** Get the muzzle world transform from weapon mesh socket. */
	FTransform GetMuzzleTransform() const;

	/* --- Recoil --- */

	void ApplyRecoil();
	void TickRecoilRecovery(float DeltaTime);

	/* --- Weapon Sway --- */

	void TickWeaponSway(float DeltaTime);

	/* --- Heat --- */

	void AddHeat();
	void TickHeatCooldown(float DeltaTime);

	/* --- Reload State Machine --- */

	void TickReload(float DeltaTime);
	void BeginReloadSequence();
	void AdvanceReloadState();
	void CompleteReload();

	/* --- Malfunction --- */

	bool RollForMalfunction();
	void TriggerMalfunction();

	/* --- ADS --- */

	void TickADSTransition(float DeltaTime);

	/* --- State Management --- */

	void SetWeaponState(ESHWeaponState NewState);
	bool CanFire() const;
	bool CanReload() const;

	/* --- VFX / Audio Helpers --- */

	void PlayMuzzleFlash();
	void SpawnShellCasing();
	void PlayFireSound();
	void PlayFireModeSwitch();

	/* --- Animation --- */

	void PlayAnimMontage(const TSoftObjectPtr<UAnimMontage>& Montage);

	/* --- Internal State --- */

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon|State")
	int32 CurrentMagAmmo = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon|State")
	int32 ReserveAmmo = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon|State")
	ESHFireMode CurrentFireMode = ESHFireMode::Semi;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Weapon|State")
	ESHWeaponState WeaponState = ESHWeaponState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	ESHReloadState ReloadState = ESHReloadState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	ESHStance CurrentStance = ESHStance::Standing;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	float CurrentHeat = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	float SuppressionLevel = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	float FatigueLevel = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Weapon|State")
	float DirtLevel = 0.0f;

	/* --- Fire control --- */

	bool bWantsToFire = false;
	bool bIsADS = false;
	float ADSAlpha = 0.0f; // 0 = hip, 1 = fully ADS

	/** Time accumulator for fire rate control. */
	float TimeSinceLastShot = 0.0f;

	/** Consecutive shots in current burst/string (for recoil pattern indexing). */
	int32 ShotsFiredInString = 0;

	/** Remaining rounds in the current burst. */
	int32 BurstShotsRemaining = 0;

	/** Total rounds fired since last full cooldown (for tracer interval). */
	int32 TotalRoundsFired = 0;

	/* --- Recoil state --- */

	float AccumulatedVerticalRecoil = 0.0f;
	float AccumulatedHorizontalRecoil = 0.0f;

	/* --- Sway state --- */

	float SwayTime = 0.0f;
	FVector2D CurrentSwayOffset = FVector2D::ZeroVector;

	/* --- Reload timer --- */

	float ReloadTimer = 0.0f;
	float ReloadStageDuration = 0.0f;
	int32 RoundsToInsert = 0; // For single-round reload tracking

	/* --- Malfunction --- */

	float MalfunctionClearTimer = 0.0f;

	/* --- Overheat lockout --- */

	bool bIsOverheated = false;

	/* --- Replication --- */

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/* --- Projectile class --- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Projectile")
	TSubclassOf<ASHProjectile> ProjectileClass;

	/* --- Socket names --- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Sockets")
	FName MuzzleSocketName = FName(TEXT("Muzzle"));

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Sockets")
	FName ShellEjectSocketName = FName(TEXT("ShellEject"));

	bool bIsMoving = false;
};
