// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ShatteredHorizon2032/Weapons/SHWeaponData.h"
#include "SHMountedWeapon.generated.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;
class UNiagaraSystem;
class USHBallisticsSystem;
class USoundBase;

/* -----------------------------------------------------------------------
 *  Enums
 * --------------------------------------------------------------------- */

UENUM(BlueprintType)
enum class ESHMountedWeaponType : uint8
{
	HeavyMG           UMETA(DisplayName = "Heavy Machine Gun"),
	Autocannon        UMETA(DisplayName = "Autocannon"),
	GrenadeLauncher   UMETA(DisplayName = "Grenade Launcher"),
	ATGM              UMETA(DisplayName = "Anti-Tank Guided Missile"),
};

/* -----------------------------------------------------------------------
 *  ASHMountedWeapon
 * --------------------------------------------------------------------- */

UCLASS(Blueprintable)
class SHATTEREDHORIZON2032_API ASHMountedWeapon : public AActor
{
	GENERATED_BODY()

public:
	ASHMountedWeapon();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/* --- Turret Control --- */

	/**
	 * Rotate the turret by delta amounts (degrees).
	 * Clamped to configured yaw/pitch limits.
	 */
	UFUNCTION(BlueprintCallable, Category = "MountedWeapon|Turret")
	void RotateTurret(float DeltaYaw, float DeltaPitch);

	/** Get the current turret rotation relative to the vehicle base. */
	UFUNCTION(BlueprintPure, Category = "MountedWeapon|Turret")
	FRotator GetTurretRotation() const { return CurrentTurretRotation; }

	/** Get the world-space muzzle location. */
	UFUNCTION(BlueprintPure, Category = "MountedWeapon|Turret")
	FVector GetMuzzleLocation() const;

	/** Get the world-space muzzle forward direction. */
	UFUNCTION(BlueprintPure, Category = "MountedWeapon|Turret")
	FVector GetMuzzleDirection() const;

	/* --- Fire Control --- */

	UFUNCTION(BlueprintCallable, Category = "MountedWeapon|Fire")
	void StartFire();

	UFUNCTION(BlueprintCallable, Category = "MountedWeapon|Fire")
	void StopFire();

	UFUNCTION(BlueprintCallable, Category = "MountedWeapon|Fire")
	void Reload();

	/** Returns true if the weapon is currently firing. */
	UFUNCTION(BlueprintPure, Category = "MountedWeapon|Fire")
	bool IsFiring() const { return bIsFiring && !bIsOverheated && !bIsReloading; }

	/** Returns true if the weapon is reloading. */
	UFUNCTION(BlueprintPure, Category = "MountedWeapon|Fire")
	bool IsReloading() const { return bIsReloading; }

	/* --- Ammo --- */

	UFUNCTION(BlueprintPure, Category = "MountedWeapon|Ammo")
	int32 GetCurrentMagazineAmmo() const { return CurrentMagazineAmmo; }

	UFUNCTION(BlueprintPure, Category = "MountedWeapon|Ammo")
	int32 GetReserveAmmo() const { return ReserveAmmo; }

	UFUNCTION(BlueprintPure, Category = "MountedWeapon|Ammo")
	int32 GetTotalAmmo() const { return CurrentMagazineAmmo + ReserveAmmo; }

	UFUNCTION(BlueprintPure, Category = "MountedWeapon|Ammo")
	bool HasAmmo() const { return CurrentMagazineAmmo > 0; }

	/* --- Heat --- */

	/** Get current heat level (0.0 = cool, 1.0 = max). */
	UFUNCTION(BlueprintPure, Category = "MountedWeapon|Heat")
	float GetHeatLevel() const { return CurrentHeat; }

	UFUNCTION(BlueprintPure, Category = "MountedWeapon|Heat")
	bool IsOverheated() const { return bIsOverheated; }

	/* --- Identity --- */

	UFUNCTION(BlueprintPure, Category = "MountedWeapon|Identity")
	ESHMountedWeaponType GetWeaponType() const { return WeaponType; }

	/* --- Components --- */

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MountedWeapon|Components")
	TObjectPtr<USceneComponent> TurretRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MountedWeapon|Components")
	TObjectPtr<UStaticMeshComponent> WeaponMesh;

protected:
	/* --- Configuration: Identity --- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|Identity")
	ESHMountedWeaponType WeaponType = ESHMountedWeaponType::HeavyMG;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|Identity")
	FText WeaponDisplayName;

	/* --- Configuration: Weapon Stats --- */

	/** Rounds per minute. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|Stats", meta = (ClampMin = "1"))
	float RoundsPerMinute = 550.0f;

	/** Muzzle velocity in m/s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|Stats", meta = (ClampMin = "1"))
	float MuzzleVelocityMPS = 890.0f;

	/** Base damage per round. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|Stats", meta = (ClampMin = "0"))
	float BaseDamage = 80.0f;

	/** Damage falloff start distance in meters. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|Stats", meta = (ClampMin = "0"))
	float DamageFalloffStartM = 200.0f;

	/** Maximum range in meters. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|Stats", meta = (ClampMin = "1"))
	float MaxRangeM = 2000.0f;

	/** Minimum damage multiplier at max range. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|Stats", meta = (ClampMin = "0", ClampMax = "1"))
	float MinDamageMultiplier = 0.3f;

	/** Penetration rating for armor checks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|Stats", meta = (ClampMin = "0"))
	int32 PenetrationRating = 5;

	/** Ballistic coefficients for projectile simulation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|Stats")
	FSHBallisticCoefficients BallisticCoeffs;

	/** Spread in MOA (minute of angle). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|Stats", meta = (ClampMin = "0"))
	float SpreadMOA = 4.0f;

	/** Fire a tracer every N rounds (0 = no tracers). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|Stats", meta = (ClampMin = "0"))
	int32 TracerInterval = 5;

	/* --- Configuration: Ammo --- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|Ammo", meta = (ClampMin = "1"))
	int32 MagazineCapacity = 100;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|Ammo", meta = (ClampMin = "0"))
	int32 MaxReserveAmmo = 400;

	/** Reload time in seconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|Ammo", meta = (ClampMin = "0"))
	float ReloadTime = 5.0f;

	/* --- Configuration: Turret --- */

	/** Maximum yaw rotation in degrees from center (symmetric). 0 = no limit. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|Turret", meta = (ClampMin = "0", ClampMax = "360"))
	float YawLimit = 360.0f;

	/** Maximum upward pitch in degrees. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|Turret", meta = (ClampMin = "0", ClampMax = "90"))
	float MaxPitch = 60.0f;

	/** Maximum downward pitch in degrees (stored positive, applied as negative). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|Turret", meta = (ClampMin = "0", ClampMax = "90"))
	float MinPitch = 10.0f;

	/** Turret rotation speed in degrees per second. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|Turret", meta = (ClampMin = "1"))
	float TurretRotationSpeed = 90.0f;

	/* --- Configuration: Heat --- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|Heat")
	FSHWeaponHeatConfig HeatConfig;

	/* --- Configuration: VFX / Audio --- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|VFX")
	TSoftObjectPtr<UNiagaraSystem> MuzzleFlashVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|VFX")
	TSoftObjectPtr<UNiagaraSystem> ShellEjectVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|Audio")
	TSoftObjectPtr<USoundBase> FireSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|Audio")
	TSoftObjectPtr<USoundBase> FireStopSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|Audio")
	TSoftObjectPtr<USoundBase> ReloadSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|Audio")
	TSoftObjectPtr<USoundBase> OverheatSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|Audio")
	TSoftObjectPtr<USoundBase> DryFireSound;

	/** Muzzle socket name on the weapon mesh. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|Sockets")
	FName MuzzleSocket = TEXT("muzzle");

	/** Shell ejection socket name. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "MountedWeapon|Sockets")
	FName ShellEjectSocket = TEXT("shell_eject");

	/* --- Runtime State --- */

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "MountedWeapon|State")
	FRotator CurrentTurretRotation = FRotator::ZeroRotator;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "MountedWeapon|State")
	int32 CurrentMagazineAmmo = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "MountedWeapon|State")
	int32 ReserveAmmo = 0;

	float CurrentHeat = 0.0f;
	bool bIsFiring = false;
	bool bIsOverheated = false;
	bool bIsReloading = false;
	float TimeSinceLastShot = 0.0f;
	float ReloadElapsed = 0.0f;
	int32 TotalRoundsFired = 0;

	/** Cached ref to the world ballistics subsystem. */
	UPROPERTY()
	TObjectPtr<USHBallisticsSystem> BallisticsSystem;

	/* --- Internal --- */

	/** Fire a single round. */
	void FireRound();

	/** Spawn the projectile via the ballistics system. */
	void SpawnProjectile(const FVector& MuzzleLoc, const FVector& Direction);

	/** Apply spread to a direction vector. */
	FVector ApplySpread(const FVector& BaseDirection) const;

	/** Tick the heat system. */
	void TickHeat(float DeltaTime);

	/** Tick the reload process. */
	void TickReload(float DeltaTime);

	/** Tick auto-fire (continuous fire while trigger is held). */
	void TickFire(float DeltaTime);

	/** Get seconds between shots from RPM. */
	FORCEINLINE float GetSecondsBetweenShots() const
	{
		return 60.0f / FMath::Max(RoundsPerMinute, 1.0f);
	}

	/** Play a sound at the weapon location. */
	void PlayWeaponSound(const TSoftObjectPtr<USoundBase>& Sound);

	/** Apply per-type default stats. Called from constructor. */
	void ApplyWeaponTypeDefaults();
};
