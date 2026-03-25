// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHMountedWeapon.h"
#include "ShatteredHorizon2032/Weapons/SHBallisticsSystem.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "ShatteredHorizon2032/ShatteredHorizon2032.h"

/* -----------------------------------------------------------------------
 *  Constructor
 * --------------------------------------------------------------------- */

ASHMountedWeapon::ASHMountedWeapon()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	TurretRoot = CreateDefaultSubobject<USceneComponent>(TEXT("TurretRoot"));
	SetRootComponent(TurretRoot);

	WeaponMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("WeaponMesh"));
	WeaponMesh->SetupAttachment(TurretRoot);
	WeaponMesh->SetCollisionProfileName(TEXT("NoCollision"));

	// Apply sensible defaults for the default weapon type (HeavyMG)
	ApplyWeaponTypeDefaults();
}

/* -----------------------------------------------------------------------
 *  Lifecycle
 * --------------------------------------------------------------------- */

void ASHMountedWeapon::BeginPlay()
{
	Super::BeginPlay();

	BallisticsSystem = GetWorld()->GetSubsystem<USHBallisticsSystem>();

	CurrentMagazineAmmo = MagazineCapacity;
	ReserveAmmo = MaxReserveAmmo;
	CurrentHeat = 0.0f;
	bIsOverheated = false;
	bIsReloading = false;
	bIsFiring = false;
	TimeSinceLastShot = GetSecondsBetweenShots(); // Ready to fire immediately
	TotalRoundsFired = 0;

	// Initialize heat config defaults for weapon types that overheat
	if (WeaponType == ESHMountedWeaponType::HeavyMG)
	{
		HeatConfig.bCanOverheat = true;
		HeatConfig.HeatPerShot = 0.008f;
		HeatConfig.CooldownPerSecond = 0.05f;
		HeatConfig.OverheatThreshold = 1.0f;
		HeatConfig.CooldownResumeThreshold = 0.4f;
	}
}

void ASHMountedWeapon::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TickHeat(DeltaTime);
	TickReload(DeltaTime);
	TickFire(DeltaTime);
}

/* -----------------------------------------------------------------------
 *  Turret Rotation
 * --------------------------------------------------------------------- */

void ASHMountedWeapon::RotateTurret(float DeltaYaw, float DeltaPitch)
{
	// Apply rotation speed limiting
	const float MaxDelta = TurretRotationSpeed * GetWorld()->GetDeltaSeconds();

	const float ClampedYaw = FMath::Clamp(DeltaYaw, -MaxDelta, MaxDelta);
	const float ClampedPitch = FMath::Clamp(DeltaPitch, -MaxDelta, MaxDelta);

	float NewYaw = CurrentTurretRotation.Yaw + ClampedYaw;
	float NewPitch = CurrentTurretRotation.Pitch + ClampedPitch;

	// Apply yaw limits
	if (YawLimit < 360.0f)
	{
		const float HalfYaw = YawLimit * 0.5f;
		NewYaw = FMath::Clamp(NewYaw, -HalfYaw, HalfYaw);
	}
	else
	{
		// Full 360 rotation — normalize
		NewYaw = FMath::UnwindDegrees(NewYaw);
	}

	// Apply pitch limits
	NewPitch = FMath::Clamp(NewPitch, -MinPitch, MaxPitch);

	CurrentTurretRotation.Yaw = NewYaw;
	CurrentTurretRotation.Pitch = NewPitch;
	CurrentTurretRotation.Roll = 0.0f;

	// Apply rotation to the turret root component (relative to parent)
	if (TurretRoot)
	{
		TurretRoot->SetRelativeRotation(CurrentTurretRotation);
	}
}

FVector ASHMountedWeapon::GetMuzzleLocation() const
{
	if (WeaponMesh && WeaponMesh->DoesSocketExist(MuzzleSocket))
	{
		return WeaponMesh->GetSocketLocation(MuzzleSocket);
	}

	// Fallback: use weapon mesh forward offset
	return GetActorLocation() + GetActorForwardVector() * 100.0f;
}

FVector ASHMountedWeapon::GetMuzzleDirection() const
{
	if (WeaponMesh && WeaponMesh->DoesSocketExist(MuzzleSocket))
	{
		return WeaponMesh->GetSocketRotation(MuzzleSocket).Vector();
	}

	return GetActorForwardVector();
}

/* -----------------------------------------------------------------------
 *  Fire Control
 * --------------------------------------------------------------------- */

void ASHMountedWeapon::StartFire()
{
	if (bIsReloading || bIsOverheated)
	{
		return;
	}

	if (CurrentMagazineAmmo <= 0)
	{
		PlayWeaponSound(DryFireSound);

		// Auto-reload if we have reserve ammo
		if (ReserveAmmo > 0)
		{
			Reload();
		}
		return;
	}

	bIsFiring = true;
}

void ASHMountedWeapon::StopFire()
{
	if (bIsFiring)
	{
		bIsFiring = false;
		PlayWeaponSound(FireStopSound);
	}
}

void ASHMountedWeapon::Reload()
{
	if (bIsReloading)
	{
		return;
	}

	if (ReserveAmmo <= 0)
	{
		SH_LOG(LogSH_Vehicle, Verbose, "MountedWeapon '%s': no reserve ammo to reload", *GetName());
		return;
	}

	if (CurrentMagazineAmmo >= MagazineCapacity)
	{
		return; // Already full
	}

	// Stop firing during reload
	bIsFiring = false;
	bIsReloading = true;
	ReloadElapsed = 0.0f;

	PlayWeaponSound(ReloadSound);

	SH_LOG(LogSH_Vehicle, Verbose, "MountedWeapon '%s': reload started (%.1fs)", *GetName(), ReloadTime);
}

/* -----------------------------------------------------------------------
 *  Tick: Fire
 * --------------------------------------------------------------------- */

void ASHMountedWeapon::TickFire(float DeltaTime)
{
	TimeSinceLastShot += DeltaTime;

	if (!bIsFiring || bIsReloading || bIsOverheated)
	{
		return;
	}

	if (CurrentMagazineAmmo <= 0)
	{
		StopFire();
		if (ReserveAmmo > 0)
		{
			Reload();
		}
		return;
	}

	const float FireInterval = GetSecondsBetweenShots();

	while (TimeSinceLastShot >= FireInterval && bIsFiring && CurrentMagazineAmmo > 0 && !bIsOverheated)
	{
		FireRound();
		TimeSinceLastShot -= FireInterval;
	}
}

void ASHMountedWeapon::FireRound()
{
	if (CurrentMagazineAmmo <= 0)
	{
		return;
	}

	--CurrentMagazineAmmo;
	++TotalRoundsFired;

	const FVector MuzzleLoc = GetMuzzleLocation();
	const FVector BaseDir = GetMuzzleDirection();
	const FVector SpreadDir = ApplySpread(BaseDir);

	// Spawn projectile through the ballistics system
	SpawnProjectile(MuzzleLoc, SpreadDir);

	// Add heat
	CurrentHeat = FMath::Clamp(CurrentHeat + HeatConfig.HeatPerShot, 0.0f, 1.0f);

	// Check overheat
	if (HeatConfig.bCanOverheat && CurrentHeat >= HeatConfig.OverheatThreshold)
	{
		bIsOverheated = true;
		bIsFiring = false;
		PlayWeaponSound(OverheatSound);

		SH_LOG(LogSH_Vehicle, Log, "MountedWeapon '%s': OVERHEATED", *GetName());
	}

	// Fire sound
	PlayWeaponSound(FireSound);

	// Muzzle flash VFX
	if (UNiagaraSystem* MuzzleNS = MuzzleFlashVFX.LoadSynchronous())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), MuzzleNS, MuzzleLoc,
			GetMuzzleDirection().Rotation());
	}

	// Shell eject VFX
	if (UNiagaraSystem* ShellNS = ShellEjectVFX.LoadSynchronous())
	{
		FVector EjectLoc = GetActorLocation();
		if (WeaponMesh && WeaponMesh->DoesSocketExist(ShellEjectSocket))
		{
			EjectLoc = WeaponMesh->GetSocketLocation(ShellEjectSocket);
		}

		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), ShellNS, EjectLoc, GetActorRotation());
	}
}

/* -----------------------------------------------------------------------
 *  Projectile Spawning
 * --------------------------------------------------------------------- */

void ASHMountedWeapon::SpawnProjectile(const FVector& MuzzleLoc, const FVector& Direction)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Determine if this is a tracer round
	const bool bIsTracer = USHBallisticsSystem::IsTracerRound(TotalRoundsFired, TracerInterval);

	// Compute initial velocity in cm/s
	const FVector InitialVelocity = Direction * (MuzzleVelocityMPS * 100.0f);

	// Spawn projectile actor
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner() ? GetOwner() : this;
	SpawnParams.Instigator = GetInstigator();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// Use the default projectile class from the project
	// In production, this would be a configurable TSubclassOf<ASHProjectile>
	const FTransform SpawnTransform(Direction.Rotation(), MuzzleLoc);

	ASHProjectile* Projectile = World->SpawnActorDeferred<ASHProjectile>(
		ASHProjectile::StaticClass(), SpawnTransform, SpawnParams.Owner,
		SpawnParams.Instigator, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	if (Projectile)
	{
		Projectile->InitializeProjectile(
			InitialVelocity,
			BaseDamage,
			BallisticCoeffs,
			MaxRangeM * 100.0f,         // Convert to cm
			DamageFalloffStartM * 100.0f, // Convert to cm
			MinDamageMultiplier,
			bIsTracer,
			this);

		Projectile->FinishSpawning(SpawnTransform);
	}
}

FVector ASHMountedWeapon::ApplySpread(const FVector& BaseDirection) const
{
	if (SpreadMOA <= 0.0f)
	{
		return BaseDirection;
	}

	// Convert MOA to radians: 1 MOA = 1/60 degree
	const float SpreadDeg = SpreadMOA / 60.0f;
	const float SpreadRad = FMath::DegreesToRadians(SpreadDeg);

	// Random cone spread
	return FMath::VRandCone(BaseDirection, SpreadRad);
}

/* -----------------------------------------------------------------------
 *  Tick: Heat
 * --------------------------------------------------------------------- */

void ASHMountedWeapon::TickHeat(float DeltaTime)
{
	if (CurrentHeat <= 0.0f && !bIsOverheated)
	{
		return;
	}

	// Cool down
	CurrentHeat = FMath::Max(CurrentHeat - HeatConfig.CooldownPerSecond * DeltaTime, 0.0f);

	// Check if we've cooled enough to resume firing after overheat
	if (bIsOverheated && CurrentHeat <= HeatConfig.CooldownResumeThreshold)
	{
		bIsOverheated = false;
		SH_LOG(LogSH_Vehicle, Verbose, "MountedWeapon '%s': cooled down, ready to fire", *GetName());
	}
}

/* -----------------------------------------------------------------------
 *  Tick: Reload
 * --------------------------------------------------------------------- */

void ASHMountedWeapon::TickReload(float DeltaTime)
{
	if (!bIsReloading)
	{
		return;
	}

	ReloadElapsed += DeltaTime;

	if (ReloadElapsed >= ReloadTime)
	{
		// Reload complete
		const int32 AmmoNeeded = MagazineCapacity - CurrentMagazineAmmo;
		const int32 AmmoToLoad = FMath::Min(AmmoNeeded, ReserveAmmo);

		CurrentMagazineAmmo += AmmoToLoad;
		ReserveAmmo -= AmmoToLoad;

		bIsReloading = false;
		ReloadElapsed = 0.0f;

		SH_LOG(LogSH_Vehicle, Verbose, "MountedWeapon '%s': reload complete (%d/%d, %d reserve)",
			*GetName(), CurrentMagazineAmmo, MagazineCapacity, ReserveAmmo);
	}
}

/* -----------------------------------------------------------------------
 *  Audio Helper
 * --------------------------------------------------------------------- */

void ASHMountedWeapon::PlayWeaponSound(const TSoftObjectPtr<USoundBase>& Sound)
{
	if (USoundBase* LoadedSound = Sound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(this, LoadedSound, GetMuzzleLocation());
	}
}

/* -----------------------------------------------------------------------
 *  Per-type defaults
 * --------------------------------------------------------------------- */

void ASHMountedWeapon::ApplyWeaponTypeDefaults()
{
	switch (WeaponType)
	{
	case ESHMountedWeaponType::HeavyMG:
		RoundsPerMinute = 550.0f;
		MuzzleVelocityMPS = 890.0f;
		BaseDamage = 80.0f;
		DamageFalloffStartM = 200.0f;
		MaxRangeM = 1800.0f;
		MinDamageMultiplier = 0.3f;
		PenetrationRating = 5;
		MagazineCapacity = 100;
		MaxReserveAmmo = 400;
		ReloadTime = 5.0f;
		SpreadMOA = 4.0f;
		TracerInterval = 5;
		YawLimit = 360.0f;
		MaxPitch = 60.0f;
		MinPitch = 10.0f;
		TurretRotationSpeed = 90.0f;
		HeatConfig.bCanOverheat = true;
		HeatConfig.HeatPerShot = 0.008f;
		HeatConfig.CooldownPerSecond = 0.05f;
		HeatConfig.OverheatThreshold = 1.0f;
		HeatConfig.CooldownResumeThreshold = 0.4f;
		BallisticCoeffs.BallisticCoefficient = 0.670f;
		BallisticCoeffs.DragCoefficient = 0.310f;
		BallisticCoeffs.BulletMassGrams = 42.0f;
		BallisticCoeffs.CrossSectionCm2 = 1.26f;
		break;

	case ESHMountedWeaponType::Autocannon:
		RoundsPerMinute = 200.0f;
		MuzzleVelocityMPS = 1100.0f;
		BaseDamage = 250.0f;
		DamageFalloffStartM = 500.0f;
		MaxRangeM = 3000.0f;
		MinDamageMultiplier = 0.4f;
		PenetrationRating = 10;
		MagazineCapacity = 40;
		MaxReserveAmmo = 160;
		ReloadTime = 8.0f;
		SpreadMOA = 2.0f;
		TracerInterval = 3;
		YawLimit = 360.0f;
		MaxPitch = 70.0f;
		MinPitch = 8.0f;
		TurretRotationSpeed = 60.0f;
		HeatConfig.bCanOverheat = false;
		BallisticCoeffs.BallisticCoefficient = 0.350f;
		BallisticCoeffs.DragCoefficient = 0.280f;
		BallisticCoeffs.BulletMassGrams = 185.0f;
		BallisticCoeffs.CrossSectionCm2 = 4.91f;
		break;

	case ESHMountedWeaponType::GrenadeLauncher:
		RoundsPerMinute = 60.0f;
		MuzzleVelocityMPS = 240.0f;
		BaseDamage = 150.0f;
		DamageFalloffStartM = 50.0f;
		MaxRangeM = 1500.0f;
		MinDamageMultiplier = 0.8f;
		PenetrationRating = 2;
		MagazineCapacity = 6;
		MaxReserveAmmo = 24;
		ReloadTime = 4.0f;
		SpreadMOA = 6.0f;
		TracerInterval = 0;
		YawLimit = 360.0f;
		MaxPitch = 75.0f;
		MinPitch = 5.0f;
		TurretRotationSpeed = 45.0f;
		HeatConfig.bCanOverheat = false;
		BallisticCoeffs.BallisticCoefficient = 0.035f;
		BallisticCoeffs.DragCoefficient = 0.450f;
		BallisticCoeffs.BulletMassGrams = 230.0f;
		BallisticCoeffs.CrossSectionCm2 = 12.56f;
		break;

	case ESHMountedWeaponType::ATGM:
		RoundsPerMinute = 6.0f;
		MuzzleVelocityMPS = 300.0f;
		BaseDamage = 800.0f;
		DamageFalloffStartM = 100.0f;
		MaxRangeM = 4000.0f;
		MinDamageMultiplier = 0.9f;
		PenetrationRating = 15;
		MagazineCapacity = 1;
		MaxReserveAmmo = 3;
		ReloadTime = 12.0f;
		SpreadMOA = 0.5f;
		TracerInterval = 0;
		YawLimit = 270.0f;
		MaxPitch = 30.0f;
		MinPitch = 10.0f;
		TurretRotationSpeed = 30.0f;
		HeatConfig.bCanOverheat = false;
		BallisticCoeffs.BallisticCoefficient = 0.100f;
		BallisticCoeffs.DragCoefficient = 0.400f;
		BallisticCoeffs.BulletMassGrams = 14500.0f;
		BallisticCoeffs.CrossSectionCm2 = 78.54f;
		break;
	}
}
