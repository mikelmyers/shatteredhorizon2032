// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SHWeaponData.generated.h"

// ---------------------------------------------------------------------------
// Enums
// ---------------------------------------------------------------------------

UENUM(BlueprintType)
enum class ESHWeaponCategory : uint8
{
	AssaultRifle    UMETA(DisplayName = "Assault Rifle"),
	SAW             UMETA(DisplayName = "Squad Automatic Weapon"),
	DMR             UMETA(DisplayName = "Designated Marksman Rifle"),
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
enum class ESHPenetrationMaterial : uint8
{
	Wood      UMETA(DisplayName = "Wood"),
	Drywall   UMETA(DisplayName = "Drywall"),
	Sandbag   UMETA(DisplayName = "Sandbag"),
	Concrete  UMETA(DisplayName = "Concrete"),
	Steel     UMETA(DisplayName = "Steel"),
	Glass     UMETA(DisplayName = "Glass"),
	Flesh     UMETA(DisplayName = "Flesh"),
	Dirt      UMETA(DisplayName = "Dirt"),
	MAX       UMETA(Hidden),
};

UENUM(BlueprintType)
enum class ESHStance : uint8
{
	Standing  UMETA(DisplayName = "Standing"),
	Crouching UMETA(DisplayName = "Crouching"),
	Prone     UMETA(DisplayName = "Prone"),
	Moving    UMETA(DisplayName = "Moving"),
};

// ---------------------------------------------------------------------------
// Structs
// ---------------------------------------------------------------------------

USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHRecoilPattern
{
	GENERATED_BODY()

	/** Vertical recoil per shot in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil", meta = (ClampMin = "0.0"))
	float VerticalRecoil = 0.45f;

	/** Horizontal recoil per shot in degrees (randomly +/-). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil", meta = (ClampMin = "0.0"))
	float HorizontalRecoil = 0.15f;

	/** Multiplier applied to the first shot after a pause. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil", meta = (ClampMin = "1.0"))
	float FirstShotMultiplier = 1.5f;

	/** How quickly the weapon recovers between shots (degrees/sec). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil", meta = (ClampMin = "0.0"))
	float RecoveryRate = 5.0f;

	/** Maximum accumulated vertical recoil before clamping (degrees). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil", meta = (ClampMin = "0.0"))
	float MaxVerticalRecoil = 8.0f;

	/** Horizontal recoil bias (-1 = left, 0 = none, 1 = right). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
	float HorizontalBias = 0.0f;
};

USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHBallisticCoefficients
{
	GENERATED_BODY()

	/** G1 Ballistic Coefficient for bullet drop calculation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ballistics", meta = (ClampMin = "0.01"))
	float BallisticCoefficient = 0.304f;

	/** Drag coefficient used in air resistance model. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ballistics", meta = (ClampMin = "0.0"))
	float DragCoefficient = 0.295f;

	/** Bullet mass in grains. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ballistics", meta = (ClampMin = "1.0"))
	float BulletMassGrains = 62.0f;

	/** Bullet cross-sectional area in cm^2. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ballistics", meta = (ClampMin = "0.01"))
	float CrossSectionArea = 0.257f;
};

USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHPenetrationEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Penetration")
	ESHPenetrationMaterial Material = ESHPenetrationMaterial::Wood;

	/** Maximum thickness (cm) this round can penetrate through. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Penetration", meta = (ClampMin = "0.0"))
	float MaxPenetrationCm = 10.0f;

	/** Damage retained after full penetration (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Penetration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DamageRetention = 0.6f;

	/** Velocity retained after penetration (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Penetration", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float VelocityRetention = 0.5f;
};

USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHAccuracyModifiers
{
	GENERATED_BODY()

	/** MOA at each stance. Lower = more accurate. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accuracy")
	float StandingMOA = 4.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accuracy")
	float CrouchingMOA = 2.5f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accuracy")
	float ProneMOA = 1.2f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accuracy")
	float MovingMOA = 7.0f;

	/** ADS multiplier applied to the stance MOA (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accuracy", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ADSMultiplier = 0.35f;

	/** Additional spread per point of suppression (0-100 scale). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accuracy", meta = (ClampMin = "0.0"))
	float SuppressionSpreadPerPoint = 0.04f;

	/** Additional spread per unit of fatigue (0-100 scale). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accuracy", meta = (ClampMin = "0.0"))
	float FatigueSpreadPerPoint = 0.02f;
};

USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHSoundProfile
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundBase> FireSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundBase> FireSoundSuppressed = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundBase> DryFireSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundBase> ReloadSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundBase> MalfunctionSound = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	TObjectPtr<USoundAttenuation> AttenuationSettings = nullptr;

	/** Audible range in metres (for AI hearing). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0.0"))
	float AudibleRangeMetres = 800.0f;

	/** Suppressed audible range in metres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound", meta = (ClampMin = "0.0"))
	float SuppressedRangeMetres = 50.0f;
};

// ---------------------------------------------------------------------------
// UDataAsset — the main weapon definition
// ---------------------------------------------------------------------------

UCLASS(BlueprintType)
class SHATTEREDHORIZON2032_API USHWeaponData : public UDataAsset
{
	GENERATED_BODY()

public:
	// ---- Identity ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FName WeaponID;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Identity")
	ESHWeaponCategory Category = ESHWeaponCategory::AssaultRifle;

	// ---- Firing ----

	/** Rounds per minute. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Firing", meta = (ClampMin = "1"))
	float FireRateRPM = 700.0f;

	/** Muzzle velocity in m/s (real-world scale, converted internally). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Firing", meta = (ClampMin = "1.0"))
	float MuzzleVelocityMPS = 940.0f;

	/** Effective range in metres. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Firing", meta = (ClampMin = "1.0"))
	float EffectiveRangeM = 500.0f;

	/** Maximum range in metres before projectile is destroyed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Firing", meta = (ClampMin = "1.0"))
	float MaxRangeM = 3600.0f;

	/** Below this range (metres) fire is resolved as hitscan. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Firing", meta = (ClampMin = "0.0"))
	float HitscanThresholdM = 50.0f;

	/** Available fire modes, ordered for mode-switch cycling. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Firing")
	TArray<ESHFireMode> AvailableFireModes;

	/** Rounds per burst (only used when Burst mode is selected). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Firing", meta = (ClampMin = "2", ClampMax = "5"))
	int32 BurstCount = 3;

	// ---- Damage ----

	/** Base damage at muzzle. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0"))
	float BaseDamage = 34.0f;

	/** Damage at max effective range as a fraction of base (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float DamageFalloffAtRange = 0.55f;

	/** Pellet count — 1 for rifles, >1 for shotguns. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (ClampMin = "1"))
	int32 PelletCount = 1;

	/** If true, this is an explosive round (GL, etc.). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	bool bIsExplosive = false;

	/** Explosive inner radius (full damage). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (EditCondition = "bIsExplosive", ClampMin = "0.0"))
	float ExplosiveInnerRadiusM = 3.0f;

	/** Explosive outer radius (falloff to zero). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (EditCondition = "bIsExplosive", ClampMin = "0.0"))
	float ExplosiveOuterRadiusM = 15.0f;

	/** Number of shrapnel fragments spawned on detonation. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (EditCondition = "bIsExplosive", ClampMin = "0"))
	int32 ShrapnelCount = 0;

	/** Damage per shrapnel fragment. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage", meta = (EditCondition = "bIsExplosive", ClampMin = "0.0"))
	float ShrapnelDamage = 20.0f;

	// ---- Ballistics ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ballistics")
	FSHBallisticCoefficients Ballistics;

	// ---- Penetration ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Penetration")
	TArray<FSHPenetrationEntry> PenetrationTable;

	// ---- Recoil ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Recoil")
	FSHRecoilPattern Recoil;

	// ---- Accuracy ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Accuracy")
	FSHAccuracyModifiers Accuracy;

	// ---- Magazine ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magazine", meta = (ClampMin = "1"))
	int32 MagazineCapacity = 30;

	/** Max reserve ammo a soldier can carry for this weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magazine", meta = (ClampMin = "0"))
	int32 MaxReserveAmmo = 210;

	/** Tactical reload time in seconds (magazine not empty). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magazine", meta = (ClampMin = "0.1"))
	float TacticalReloadTime = 2.1f;

	/** Empty reload time in seconds (must chamber a round). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Magazine", meta = (ClampMin = "0.1"))
	float EmptyReloadTime = 2.8f;

	// ---- Malfunction ----

	/** Base probability of malfunction per shot (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Malfunction", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float BaseMalfunctionChance = 0.0002f;

	/** Extra malfunction chance per consecutive shot of sustained fire. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Malfunction", meta = (ClampMin = "0.0"))
	float SustainedFireMalfunctionRate = 0.00005f;

	/** Extra malfunction chance per unit of dirt accumulation (0-100). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Malfunction", meta = (ClampMin = "0.0"))
	float DirtMalfunctionRate = 0.0001f;

	/** Time to clear a malfunction in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Malfunction", meta = (ClampMin = "0.1"))
	float MalfunctionClearTime = 3.0f;

	// ---- Heat ----

	/** Heat generated per shot (0..1 units toward overheat). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heat", meta = (ClampMin = "0.0"))
	float HeatPerShot = 0.012f;

	/** Passive heat dissipation per second. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heat", meta = (ClampMin = "0.0"))
	float HeatDissipationRate = 0.08f;

	/** Heat threshold (0..1) at which weapon jams from overheating. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Heat", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float OverheatThreshold = 1.0f;

	// ---- ADS ----

	/** Time to transition into ADS in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ADS", meta = (ClampMin = "0.01"))
	float ADSTime = 0.25f;

	/** FOV multiplier when fully ADS (1.0 = no change, 0.5 = 2x zoom). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "ADS", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float ADSFOVMultiplier = 0.75f;

	// ---- Weapon Sway ----

	/** Base sway amplitude in degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sway", meta = (ClampMin = "0.0"))
	float BaseSwayAmplitude = 0.6f;

	/** Sway frequency in Hz. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sway", meta = (ClampMin = "0.1"))
	float SwayFrequency = 1.2f;

	/** Multiplier on sway when stamina is depleted. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sway", meta = (ClampMin = "1.0"))
	float ExhaustedSwayMultiplier = 3.0f;

	// ---- Tracers ----

	/** Fire a visible tracer every N rounds (0 = no tracers). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tracers", meta = (ClampMin = "0"))
	int32 TracerInterval = 0;

	// ---- Sound ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Sound")
	FSHSoundProfile SoundProfile;

	// ---- Visuals / Animation ----

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<USkeletalMesh> WeaponMesh = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<UAnimMontage> FireMontage_1P = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<UAnimMontage> ReloadMontage_1P = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<UAnimMontage> MalfunctionClearMontage_1P = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<UParticleSystem> MuzzleFlashVFX = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<UNiagaraSystem> MuzzleFlashNiagara = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TObjectPtr<UNiagaraSystem> ShellEjectNiagara = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Visuals")
	TSubclassOf<AActor> ProjectileClass;

	// ---- Helpers ----

	/** Seconds between shots derived from RPM. */
	float GetFireInterval() const { return 60.0f / FMath::Max(FireRateRPM, 1.0f); }

	/** Get stance MOA. */
	float GetMOAForStance(ESHStance Stance) const;

	/** Look up penetration data for a material. Returns nullptr if none defined. */
	const FSHPenetrationEntry* FindPenetration(ESHPenetrationMaterial Material) const;
};

// ---------------------------------------------------------------------------
// Default data factory — creates preset data assets at runtime/editor
// ---------------------------------------------------------------------------

UCLASS()
class SHATTEREDHORIZON2032_API USHWeaponDataLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Populate a weapon data asset with real-world-inspired defaults. */
	UFUNCTION(BlueprintCallable, Category = "SH|Weapons")
	static void ApplyPreset_M27IAR(USHWeaponData* Data);

	UFUNCTION(BlueprintCallable, Category = "SH|Weapons")
	static void ApplyPreset_M4A1(USHWeaponData* Data);

	UFUNCTION(BlueprintCallable, Category = "SH|Weapons")
	static void ApplyPreset_M249SAW(USHWeaponData* Data);

	UFUNCTION(BlueprintCallable, Category = "SH|Weapons")
	static void ApplyPreset_M110SASS(USHWeaponData* Data);

	UFUNCTION(BlueprintCallable, Category = "SH|Weapons")
	static void ApplyPreset_M17SIG(USHWeaponData* Data);

	UFUNCTION(BlueprintCallable, Category = "SH|Weapons")
	static void ApplyPreset_M320GL(USHWeaponData* Data);

	UFUNCTION(BlueprintCallable, Category = "SH|Weapons")
	static void ApplyPreset_Mossberg590(USHWeaponData* Data);

private:
	/** Helper: builds a standard 5.56 NATO penetration table. */
	static TArray<FSHPenetrationEntry> MakeStandard556PenTable();

	/** Helper: builds a 7.62 NATO penetration table. */
	static TArray<FSHPenetrationEntry> MakeStandard762PenTable();

	/** Helper: builds a 9mm penetration table. */
	static TArray<FSHPenetrationEntry> MakeStandard9mmPenTable();

	/** Helper: builds a 12-gauge penetration table. */
	static TArray<FSHPenetrationEntry> MakeStandard12GaugePenTable();

	/** Helper: builds a 40mm HE penetration table. */
	static TArray<FSHPenetrationEntry> MakeStandard40mmPenTable();
};
