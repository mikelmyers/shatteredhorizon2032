// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SHWeaponData.generated.h"

/* -----------------------------------------------------------------------
 *  Enums
 * --------------------------------------------------------------------- */

UENUM(BlueprintType)
enum class ESHWeaponCategory : uint8
{
	AssaultRifle    UMETA(DisplayName = "Assault Rifle"),
	SAW             UMETA(DisplayName = "Squad Automatic Weapon"),
	DMR             UMETA(DisplayName = "Designated Marksman Rifle"),
	SniperRifle     UMETA(DisplayName = "Sniper Rifle"),
	HeavyMG         UMETA(DisplayName = "Heavy Machine Gun"),
	Sidearm         UMETA(DisplayName = "Sidearm"),
	GrenadeLauncher UMETA(DisplayName = "Grenade Launcher"),
	Shotgun         UMETA(DisplayName = "Shotgun"),
};

UENUM(BlueprintType)
enum class ESHFireMode : uint8
{
	Semi   UMETA(DisplayName = "Semi-Auto"),
	Burst  UMETA(DisplayName = "Burst"),
	Auto   UMETA(DisplayName = "Full-Auto"),
};

UENUM(BlueprintType)
enum class ESHPenetrableMaterial : uint8
{
	Wood      UMETA(DisplayName = "Wood"),
	Drywall   UMETA(DisplayName = "Drywall"),
	Sandbag   UMETA(DisplayName = "Sandbag"),
	Concrete  UMETA(DisplayName = "Concrete"),
	Steel     UMETA(DisplayName = "Steel"),
	Glass     UMETA(DisplayName = "Glass"),
	Dirt      UMETA(DisplayName = "Dirt / Earth"),
	Flesh     UMETA(DisplayName = "Flesh"),
	MAX       UMETA(Hidden),
};

UENUM(BlueprintType)
enum class ESHStance : uint8
{
	Standing  UMETA(DisplayName = "Standing"),
	Crouching UMETA(DisplayName = "Crouching"),
	Prone     UMETA(DisplayName = "Prone"),
};

/* -----------------------------------------------------------------------
 *  Structs
 * --------------------------------------------------------------------- */

USTRUCT(BlueprintType)
struct FSHRecoilPattern
{
	GENERATED_BODY()

	/** Vertical recoil per shot (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil", meta = (ClampMin = "0"))
	float VerticalRecoil = 0.45f;

	/** Horizontal recoil per shot (degrees). Randomly applied left or right. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil", meta = (ClampMin = "0"))
	float HorizontalRecoil = 0.15f;

	/** Multiplier applied to the very first shot of a burst / full-auto string. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil", meta = (ClampMin = "1.0"))
	float FirstShotMultiplier = 1.5f;

	/** How fast the recoil recovers back to center (degrees / sec). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil", meta = (ClampMin = "0"))
	float RecoveryRate = 5.0f;

	/** Random horizontal deviation pattern. Each element is applied sequentially per shot. Wraps. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil")
	TArray<float> HorizontalPattern;
};

USTRUCT(BlueprintType)
struct FSHBallisticCoefficients
{
	GENERATED_BODY()

	/** G1 ballistic coefficient (higher = less drag). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ballistics", meta = (ClampMin = "0.01"))
	float BallisticCoefficient = 0.310f;

	/** Drag coefficient for air resistance model. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ballistics", meta = (ClampMin = "0"))
	float DragCoefficient = 0.295f;

	/** Bullet mass in grams. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ballistics", meta = (ClampMin = "0.1"))
	float BulletMassGrams = 4.0f;

	/** Cross-sectional area of bullet in cm^2. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ballistics", meta = (ClampMin = "0.01"))
	float CrossSectionCm2 = 0.264f;
};

USTRUCT(BlueprintType)
struct FSHPenetrationEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Penetration")
	ESHPenetrableMaterial Material = ESHPenetrableMaterial::Wood;

	/** Maximum thickness (cm) that can be penetrated at muzzle velocity. Scales with remaining velocity. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Penetration", meta = (ClampMin = "0"))
	float MaxPenetrationCm = 10.0f;

	/** Fraction of damage retained after penetration (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Penetration", meta = (ClampMin = "0", ClampMax = "1"))
	float DamageRetention = 0.6f;

	/** Velocity multiplier after penetration (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Penetration", meta = (ClampMin = "0", ClampMax = "1"))
	float VelocityRetention = 0.5f;
};

USTRUCT(BlueprintType)
struct FSHAccuracyModifiers
{
	GENERATED_BODY()

	/** MOA spread while standing still. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accuracy", meta = (ClampMin = "0"))
	float StandingMOA = 3.0f;

	/** MOA spread while crouching. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accuracy", meta = (ClampMin = "0"))
	float CrouchingMOA = 1.8f;

	/** MOA spread while prone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accuracy", meta = (ClampMin = "0"))
	float ProneMOA = 0.9f;

	/** Additive MOA penalty while moving. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accuracy", meta = (ClampMin = "0"))
	float MovingPenaltyMOA = 4.0f;

	/** Multiplier while ADS (applied on top of stance). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accuracy", meta = (ClampMin = "0", ClampMax = "1"))
	float ADSMultiplier = 0.35f;

	/** Additive MOA from suppression (scaled 0-1 externally). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accuracy", meta = (ClampMin = "0"))
	float SuppressionMaxMOA = 6.0f;

	/** Additive MOA from fatigue (scaled 0-1 externally). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accuracy", meta = (ClampMin = "0"))
	float FatigueMaxMOA = 3.5f;
};

USTRUCT(BlueprintType)
struct FSHSoundProfile
{
	GENERATED_BODY()

	/** Gunshot sound cue. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<USoundBase> FireSound;

	/** Suppressed fire sound. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<USoundBase> FireSoundSuppressed;

	/** Sound attenuation asset for unsuppressed fire. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<USoundAttenuation> FireAttenuation;

	/** Dry-fire / empty chamber click. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<USoundBase> DryFireSound;

	/** Reload sound. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<USoundBase> ReloadSound;

	/** Fire mode switch click. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<USoundBase> FireModeSwitchSound;

	/** Malfunction / jam sound. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	TSoftObjectPtr<USoundBase> MalfunctionSound;

	/** Audible range of unsuppressed fire (cm). Used for AI alerting. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (ClampMin = "0"))
	float AudibleRangeCm = 300000.0f; // ~3 km

	/** Audible range when suppressed (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio", meta = (ClampMin = "0"))
	float SuppressedAudibleRangeCm = 5000.0f; // ~50 m
};

USTRUCT(BlueprintType)
struct FSHWeaponHeatConfig
{
	GENERATED_BODY()

	/** Heat added per shot (0-1 scale). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heat", meta = (ClampMin = "0", ClampMax = "1"))
	float HeatPerShot = 0.015f;

	/** Heat dissipation per second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heat", meta = (ClampMin = "0"))
	float CooldownPerSecond = 0.08f;

	/** Heat threshold at which malfunction chance starts increasing. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heat", meta = (ClampMin = "0", ClampMax = "1"))
	float MalfunctionHeatThreshold = 0.7f;

	/** If true, weapon can overheat and lock out firing until cooled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heat")
	bool bCanOverheat = false;

	/** Heat level that causes a forced cooldown lockout. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heat", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "bCanOverheat"))
	float OverheatThreshold = 1.0f;

	/** Heat level weapon must cool to before firing resumes after overheat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heat", meta = (ClampMin = "0", ClampMax = "1", EditCondition = "bCanOverheat"))
	float CooldownResumeThreshold = 0.5f;
};

/* -----------------------------------------------------------------------
 *  USHWeaponDataAsset
 * --------------------------------------------------------------------- */

UCLASS(BlueprintType)
class SHATTEREDHORIZON2032_API USHWeaponDataAsset : public UDataAsset
{
	GENERATED_BODY()

public:
	/* --- Identity --- */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FName WeaponID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	ESHWeaponCategory Category = ESHWeaponCategory::AssaultRifle;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	TSoftObjectPtr<USkeletalMesh> WeaponMesh;

	/* --- Fire --- */

	/** Rounds per minute. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire", meta = (ClampMin = "1"))
	float RoundsPerMinute = 700.0f;

	/** Muzzle velocity in m/s (real-world). Converted to UE units internally. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire", meta = (ClampMin = "1"))
	float MuzzleVelocityMPS = 940.0f;

	/** Effective range in meters. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire", meta = (ClampMin = "1"))
	float EffectiveRangeM = 550.0f;

	/** Maximum range in meters before projectile is despawned. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire", meta = (ClampMin = "1"))
	float MaxRangeM = 3600.0f;

	/** Base damage at muzzle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire", meta = (ClampMin = "0"))
	float BaseDamage = 35.0f;

	/** Distance (m) at which damage starts falling off. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire", meta = (ClampMin = "0"))
	float DamageFalloffStartM = 100.0f;

	/** Minimum damage multiplier at max range. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire", meta = (ClampMin = "0", ClampMax = "1"))
	float MinDamageMultiplier = 0.25f;

	/** Available fire modes. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire")
	TArray<ESHFireMode> AvailableFireModes;

	/** Rounds per burst (only relevant if Burst is an available mode). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire", meta = (ClampMin = "2", ClampMax = "5"))
	int32 BurstCount = 3;

	/** Distance (cm) within which the weapon uses hitscan instead of projectile. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire", meta = (ClampMin = "0"))
	float HitscanRangeCm = 2500.0f; // 25 m

	/** Number of pellets per shot (> 1 for shotguns). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fire", meta = (ClampMin = "1"))
	int32 PelletsPerShot = 1;

	/* --- Magazine --- */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magazine", meta = (ClampMin = "1"))
	int32 MagazineCapacity = 30;

	/** Max reserve ammo the player can carry for this weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magazine", meta = (ClampMin = "0"))
	int32 MaxReserveAmmo = 210;

	/** Reload time (seconds) with round in chamber (tactical reload). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magazine", meta = (ClampMin = "0"))
	float TacticalReloadTime = 2.1f;

	/** Reload time (seconds) from empty (bolt catch / charging handle). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magazine", meta = (ClampMin = "0"))
	float EmptyReloadTime = 2.8f;

	/** If true, reload inserts individual rounds (shotgun pump-style). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magazine")
	bool bSingleRoundReload = false;

	/** Time per individual round insert (only if bSingleRoundReload). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magazine", meta = (ClampMin = "0", EditCondition = "bSingleRoundReload"))
	float SingleRoundInsertTime = 0.55f;

	/* --- Recoil --- */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil")
	FSHRecoilPattern RecoilPattern;

	/* --- Ballistics --- */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ballistics")
	FSHBallisticCoefficients Ballistics;

	/* --- Penetration --- */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Penetration")
	TArray<FSHPenetrationEntry> PenetrationTable;

	/* --- Accuracy --- */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accuracy")
	FSHAccuracyModifiers AccuracyModifiers;

	/* --- ADS --- */

	/** FOV while aiming down sights (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ADS", meta = (ClampMin = "10", ClampMax = "90"))
	float ADSFOV = 55.0f;

	/** Time to transition into / out of ADS (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ADS", meta = (ClampMin = "0"))
	float ADSTransitionTime = 0.2f;

	/* --- Heat --- */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heat")
	FSHWeaponHeatConfig HeatConfig;

	/* --- Malfunction --- */

	/** Base probability of malfunction per shot (before heat modifier). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Malfunction", meta = (ClampMin = "0", ClampMax = "1"))
	float BaseMalfunctionChance = 0.0002f;

	/** Time to clear a malfunction (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Malfunction", meta = (ClampMin = "0"))
	float MalfunctionClearTime = 1.5f;

	/* --- Audio --- */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Audio")
	FSHSoundProfile SoundProfile;

	/* --- Tracers --- */

	/** Fire a visible tracer every N rounds (0 = no tracers). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tracers", meta = (ClampMin = "0"))
	int32 TracerInterval = 0;

	/* --- VFX --- */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	TSoftObjectPtr<UParticleSystem> MuzzleFlashVFX;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "VFX")
	TSoftObjectPtr<UParticleSystem> ShellEjectVFX;

	/* --- Animations --- */

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimMontage> FireMontage_FP;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimMontage> ReloadMontage_FP;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimMontage> ReloadEmptyMontage_FP;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimMontage> ADSInMontage_FP;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Animation")
	TSoftObjectPtr<UAnimMontage> MalfunctionClearMontage_FP;

	/* --- Sway --- */

	/** Max weapon sway amplitude in degrees at full fatigue. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sway", meta = (ClampMin = "0"))
	float MaxSwayDegrees = 2.5f;

	/** Sway frequency (cycles per second). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sway", meta = (ClampMin = "0"))
	float SwayFrequency = 0.7f;

	/* --- Weapon Weight --- */

	/** Weapon weight in kilograms. Affects movement speed and sway. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weight", meta = (ClampMin = "0"))
	float WeightKg = 3.6f;

public:
	/** Helper: seconds between shots from RPM. */
	FORCEINLINE float GetSecondsBetweenShots() const
	{
		return 60.0f / FMath::Max(RoundsPerMinute, 1.0f);
	}

	/** Helper: muzzle velocity in UE centimeters/sec (MPS * 100). */
	FORCEINLINE float GetMuzzleVelocityCmPerSec() const
	{
		return MuzzleVelocityMPS * 100.0f;
	}

	/** Lookup penetration data for a material. Returns nullptr if not found. */
	const FSHPenetrationEntry* FindPenetrationData(ESHPenetrableMaterial Material) const
	{
		for (const FSHPenetrationEntry& Entry : PenetrationTable)
		{
			if (Entry.Material == Material)
			{
				return &Entry;
			}
		}
		return nullptr;
	}
};

/* -----------------------------------------------------------------------
 *  Default weapon data factory (code-driven defaults for prototyping)
 * --------------------------------------------------------------------- */

UCLASS()
class SHATTEREDHORIZON2032_API USHWeaponDataFactory : public UObject
{
	GENERATED_BODY()

public:
	/** Populate a weapon data asset with M27 IAR defaults. */
	UFUNCTION(BlueprintCallable, Category = "Weapons|Defaults")
	static void ApplyDefaults_M27_IAR(USHWeaponDataAsset* Data);

	UFUNCTION(BlueprintCallable, Category = "Weapons|Defaults")
	static void ApplyDefaults_M4A1(USHWeaponDataAsset* Data);

	UFUNCTION(BlueprintCallable, Category = "Weapons|Defaults")
	static void ApplyDefaults_M249_SAW(USHWeaponDataAsset* Data);

	UFUNCTION(BlueprintCallable, Category = "Weapons|Defaults")
	static void ApplyDefaults_M110_SASS(USHWeaponDataAsset* Data);

	UFUNCTION(BlueprintCallable, Category = "Weapons|Defaults")
	static void ApplyDefaults_M17_SIG(USHWeaponDataAsset* Data);

	UFUNCTION(BlueprintCallable, Category = "Weapons|Defaults")
	static void ApplyDefaults_M320_GL(USHWeaponDataAsset* Data);

	UFUNCTION(BlueprintCallable, Category = "Weapons|Defaults")
	static void ApplyDefaults_Mossberg590(USHWeaponDataAsset* Data);

	/** .338 Lapua Magnum sniper rifle. */
	UFUNCTION(BlueprintCallable, Category = "Weapons|Defaults")
	static void ApplyDefaults_SniperLapua(USHWeaponDataAsset* Data);

	/** M2 Browning .50 BMG heavy machine gun. */
	UFUNCTION(BlueprintCallable, Category = "Weapons|Defaults")
	static void ApplyDefaults_M2_Browning(USHWeaponDataAsset* Data);

	/** QBZ-95 (PLA standard-issue 5.8×42mm, analogous role to 7.62×39mm AK-platform). */
	UFUNCTION(BlueprintCallable, Category = "Weapons|Defaults")
	static void ApplyDefaults_QBZ95(USHWeaponDataAsset* Data);

	/** Type 56 (7.62×39mm AK-variant, PLA militia / reserve). */
	UFUNCTION(BlueprintCallable, Category = "Weapons|Defaults")
	static void ApplyDefaults_Type56(USHWeaponDataAsset* Data);

private:
	/** Populate the standard 5.56 penetration table. */
	static void ApplyStandardPenetration_556(USHWeaponDataAsset* Data);

	/** Populate the 7.62 NATO penetration table. */
	static void ApplyStandardPenetration_762(USHWeaponDataAsset* Data);

	/** Populate pistol 9mm penetration table. */
	static void ApplyStandardPenetration_9mm(USHWeaponDataAsset* Data);

	/** Populate 12-gauge penetration table. */
	static void ApplyStandardPenetration_12ga(USHWeaponDataAsset* Data);

	/** Populate 40mm grenade penetration table. */
	static void ApplyStandardPenetration_40mm(USHWeaponDataAsset* Data);

	/** Populate .338 Lapua Magnum penetration table. */
	static void ApplyStandardPenetration_338Lapua(USHWeaponDataAsset* Data);

	/** Populate .50 BMG penetration table. */
	static void ApplyStandardPenetration_50BMG(USHWeaponDataAsset* Data);

	/** Populate 7.62x39mm penetration table. */
	static void ApplyStandardPenetration_762x39(USHWeaponDataAsset* Data);
};
