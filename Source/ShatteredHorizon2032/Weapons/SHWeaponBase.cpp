// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHWeaponBase.h"
#include "SHProjectile.h"
#include "SHBallisticsSystem.h"
#include "SHWeaponAnimSystem.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

/* -----------------------------------------------------------------------
 *  Construction
 * --------------------------------------------------------------------- */

ASHWeaponBase::ASHWeaponBase()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	WeaponMeshComp = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("WeaponMesh"));
	RootComponent = WeaponMeshComp;
	WeaponMeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	WeaponAnimSystem = CreateDefaultSubobject<USHWeaponAnimSystem>(TEXT("WeaponAnimSystem"));
}

void ASHWeaponBase::BeginPlay()
{
	Super::BeginPlay();

	if (WeaponData)
	{
		CurrentMagAmmo = WeaponData->MagazineCapacity;
		ReserveAmmo    = WeaponData->MaxReserveAmmo;

		if (WeaponData->AvailableFireModes.Num() > 0)
		{
			CurrentFireMode = WeaponData->AvailableFireModes[0];
		}

		// Ensure we can fire immediately
		TimeSinceLastShot = WeaponData->GetSecondsBetweenShots();
	}

	// Wire weapon animation system with weapon data.
	if (WeaponAnimSystem && WeaponData)
	{
		WeaponAnimSystem->SetRecoilPattern(WeaponData->RecoilPattern);
	}
}

/* -----------------------------------------------------------------------
 *  Tick
 * --------------------------------------------------------------------- */

void ASHWeaponBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!WeaponData)
	{
		return;
	}

	TimeSinceLastShot += DeltaTime;

	// Continuous fire logic
	if (bWantsToFire && CanFire())
	{
		const float FireInterval = WeaponData->GetSecondsBetweenShots();

		if (CurrentFireMode == ESHFireMode::Auto)
		{
			while (TimeSinceLastShot >= FireInterval && CanFire())
			{
				FireShot();
				TimeSinceLastShot -= FireInterval;
			}
		}
		else if (CurrentFireMode == ESHFireMode::Burst && BurstShotsRemaining > 0)
		{
			while (TimeSinceLastShot >= FireInterval && BurstShotsRemaining > 0 && CanFire())
			{
				FireShot();
				TimeSinceLastShot -= FireInterval;
				BurstShotsRemaining--;
			}

			if (BurstShotsRemaining <= 0)
			{
				bWantsToFire = false;
			}
		}
	}

	TickRecoilRecovery(DeltaTime);
	TickWeaponSway(DeltaTime);
	TickHeatCooldown(DeltaTime);
	TickReload(DeltaTime);
	TickADSTransition(DeltaTime);

	// Feed movement state to weapon animation system.
	if (WeaponAnimSystem)
	{
		WeaponAnimSystem->SetMovementState(
			GetOwner() ? GetOwner()->GetVelocity().Size2D() : 0.f,
			bIsMoving, CurrentStance);
		WeaponAnimSystem->SetADSAlpha(ADSAlpha);
		WeaponAnimSystem->SetFatigueLevel(FatigueLevel);
	}

	// Malfunction clear timer
	if (WeaponState == ESHWeaponState::Malfunctioned && MalfunctionClearTimer > 0.0f)
	{
		MalfunctionClearTimer -= DeltaTime;
		if (MalfunctionClearTimer <= 0.0f)
		{
			MalfunctionClearTimer = 0.0f;
			SetWeaponState(ESHWeaponState::Idle);
		}
	}
}

/* -----------------------------------------------------------------------
 *  Input Actions
 * --------------------------------------------------------------------- */

void ASHWeaponBase::StartFire()
{
	if (!CanFire())
	{
		// Dry fire if magazine is empty
		if (CurrentMagAmmo <= 0 && WeaponState == ESHWeaponState::Idle)
		{
			if (WeaponData)
			{
				if (USoundBase* DryFireSnd = WeaponData->SoundProfile.DryFireSound.LoadSynchronous())
				{
					UGameplayStatics::PlaySoundAtLocation(this, DryFireSnd, GetActorLocation());
				}
			}
		}
		return;
	}

	bWantsToFire = true;

	switch (CurrentFireMode)
	{
	case ESHFireMode::Semi:
		if (TimeSinceLastShot >= WeaponData->GetSecondsBetweenShots())
		{
			FireShot();
			TimeSinceLastShot = 0.0f;
			bWantsToFire = false; // Semi requires re-press
		}
		break;

	case ESHFireMode::Burst:
		if (TimeSinceLastShot >= WeaponData->GetSecondsBetweenShots())
		{
			BurstShotsRemaining = WeaponData->BurstCount;
			FireShot();
			TimeSinceLastShot = 0.0f;
			BurstShotsRemaining--;
		}
		break;

	case ESHFireMode::Auto:
		if (TimeSinceLastShot >= WeaponData->GetSecondsBetweenShots())
		{
			FireShot();
			TimeSinceLastShot = 0.0f;
		}
		break;
	}
}

void ASHWeaponBase::StopFire()
{
	bWantsToFire = false;

	if (BurstShotsRemaining <= 0)
	{
		ShotsFiredInString = 0;
	}
}

void ASHWeaponBase::StartReload()
{
	if (!CanReload())
	{
		return;
	}

	// Cancel fire
	bWantsToFire = false;
	BurstShotsRemaining = 0;

	BeginReloadSequence();
}

void ASHWeaponBase::CancelReload()
{
	if (WeaponState != ESHWeaponState::Reloading)
	{
		return;
	}

	// For single-round reloading, keep any rounds already inserted
	ReloadState = ESHReloadState::Idle;
	ReloadTimer = 0.0f;
	SetWeaponState(ESHWeaponState::Idle);
}

void ASHWeaponBase::CycleFireMode()
{
	if (!WeaponData || WeaponData->AvailableFireModes.Num() <= 1)
	{
		return;
	}

	if (WeaponState == ESHWeaponState::Firing || WeaponState == ESHWeaponState::Reloading)
	{
		return;
	}

	int32 CurrentIndex = WeaponData->AvailableFireModes.IndexOfByKey(CurrentFireMode);
	int32 NextIndex = (CurrentIndex + 1) % WeaponData->AvailableFireModes.Num();
	CurrentFireMode = WeaponData->AvailableFireModes[NextIndex];

	PlayFireModeSwitch();
	OnFireModeChanged.Broadcast(CurrentFireMode);
}

void ASHWeaponBase::StartADS()
{
	bIsADS = true;
}

void ASHWeaponBase::StopADS()
{
	bIsADS = false;
}

void ASHWeaponBase::ClearMalfunction()
{
	if (WeaponState != ESHWeaponState::Malfunctioned || !WeaponData)
	{
		return;
	}

	MalfunctionClearTimer = WeaponData->MalfunctionClearTime;
	PlayAnimMontage(WeaponData->MalfunctionClearMontage_FP);

	if (USoundBase* MalSnd = WeaponData->SoundProfile.MalfunctionSound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(this, MalSnd, GetActorLocation());
	}
}

void ASHWeaponBase::SetAmmo(int32 MagAmmo, int32 InReserve)
{
	if (WeaponData)
	{
		CurrentMagAmmo = FMath::Clamp(MagAmmo, 0, WeaponData->MagazineCapacity);
		ReserveAmmo    = FMath::Clamp(InReserve, 0, WeaponData->MaxReserveAmmo);
		OnAmmoChanged.Broadcast(CurrentMagAmmo, ReserveAmmo);
	}
}

/* -----------------------------------------------------------------------
 *  Fire Logic
 * --------------------------------------------------------------------- */

void ASHWeaponBase::FireShot()
{
	if (!WeaponData || CurrentMagAmmo <= 0)
	{
		return;
	}

	// Malfunction check
	if (RollForMalfunction())
	{
		TriggerMalfunction();
		return;
	}

	SetWeaponState(ESHWeaponState::Firing);

	CurrentMagAmmo--;
	TotalRoundsFired++;

	const FTransform MuzzleTM = GetMuzzleTransform();
	const FVector MuzzleLoc   = MuzzleTM.GetLocation();
	const FVector MuzzleFwd   = MuzzleTM.GetRotation().GetForwardVector();

	// Fire pellets (1 for rifles, >1 for shotguns)
	const int32 Pellets = FMath::Max(WeaponData->PelletsPerShot, 1);
	for (int32 i = 0; i < Pellets; ++i)
	{
		const FVector ShotDir = ApplySpread(MuzzleFwd);

		// Hitscan for close range, projectile for distance
		if (WeaponData->HitscanRangeCm > 0.0f)
		{
			// Do a short hitscan trace first
			FHitResult HitResult;
			const FVector TraceEnd = MuzzleLoc + ShotDir * WeaponData->HitscanRangeCm;

			FCollisionQueryParams QueryParams;
			QueryParams.AddIgnoredActor(this);
			QueryParams.AddIgnoredActor(GetOwner());
			QueryParams.bReturnPhysicalMaterial = true;

			if (GetWorld()->LineTraceSingleByChannel(HitResult, MuzzleLoc, TraceEnd,
				ECC_GameTraceChannel1, QueryParams))
			{
				// Hit something within hitscan range — apply damage directly
				ExecuteHitscan(MuzzleLoc, ShotDir);
			}
			else
			{
				// Nothing hit in hitscan range — spawn projectile from hitscan endpoint
				SpawnProjectile(MuzzleLoc, ShotDir);
			}
		}
		else
		{
			// Always projectile (e.g., grenade launcher)
			SpawnProjectile(MuzzleLoc, ShotDir);
		}
	}

	// Recoil
	ApplyRecoil();

	// Feed the animation system.
	if (WeaponAnimSystem)
	{
		WeaponAnimSystem->OnShotFired(ShotsFiredInString);
	}

	// Heat
	AddHeat();

	// VFX & Audio
	PlayMuzzleFlash();
	SpawnShellCasing();
	PlayFireSound();

	// Animation
	PlayAnimMontage(WeaponData->FireMontage_FP);

	// Events
	OnAmmoChanged.Broadcast(CurrentMagAmmo, ReserveAmmo);
	OnWeaponFired.Broadcast();

	ShotsFiredInString++;

	// Return to idle after single shot (state will be re-set on next shot)
	if (CurrentFireMode == ESHFireMode::Semi)
	{
		SetWeaponState(ESHWeaponState::Idle);
	}
}

void ASHWeaponBase::ExecuteHitscan(const FVector& MuzzleLocation, const FVector& ShotDirection)
{
	if (!WeaponData)
	{
		return;
	}

	FHitResult HitResult;
	const FVector TraceEnd = MuzzleLocation + ShotDirection * WeaponData->HitscanRangeCm;

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(GetOwner());
	QueryParams.bReturnPhysicalMaterial = true;

	if (GetWorld()->LineTraceSingleByChannel(HitResult, MuzzleLocation, TraceEnd,
		ECC_GameTraceChannel1, QueryParams))
	{
		// Calculate damage (no real falloff at hitscan range, but compute anyway)
		const float DistanceM = HitResult.Distance / 100.0f;
		float DamageMultiplier = 1.0f;

		if (DistanceM > WeaponData->DamageFalloffStartM)
		{
			const float FalloffRange = WeaponData->MaxRangeM - WeaponData->DamageFalloffStartM;
			if (FalloffRange > 0.0f)
			{
				const float FalloffAlpha = FMath::Clamp(
					(DistanceM - WeaponData->DamageFalloffStartM) / FalloffRange, 0.0f, 1.0f);
				DamageMultiplier = FMath::Lerp(1.0f, WeaponData->MinDamageMultiplier, FalloffAlpha);
			}
		}

		const float FinalDamage = WeaponData->BaseDamage * DamageMultiplier;

		// Apply damage
		if (AActor* HitActor = HitResult.GetActor())
		{
			FPointDamageEvent DamageEvent;
			DamageEvent.Damage = FinalDamage;
			DamageEvent.HitInfo = HitResult;
			DamageEvent.ShotDirection = ShotDirection;

			HitActor->TakeDamage(FinalDamage, FDamageEvent(DamageEvent.GetTypeID()),
				GetInstigatorController(), this);
		}
	}
}

void ASHWeaponBase::SpawnProjectile(const FVector& MuzzleLocation, const FVector& ShotDirection)
{
	if (!ProjectileClass || !WeaponData)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = GetOwner();
	SpawnParams.Instigator = GetInstigator();
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FRotator SpawnRotation = ShotDirection.Rotation();

	ASHProjectile* Projectile = World->SpawnActor<ASHProjectile>(
		ProjectileClass, MuzzleLocation, SpawnRotation, SpawnParams);

	if (Projectile)
	{
		// Determine if this round should be a tracer
		bool bIsTracer = false;
		if (WeaponData->TracerInterval > 0)
		{
			bIsTracer = (TotalRoundsFired % WeaponData->TracerInterval) == 0;
		}

		Projectile->InitializeProjectile(
			ShotDirection * WeaponData->GetMuzzleVelocityCmPerSec(),
			WeaponData->BaseDamage,
			WeaponData->Ballistics,
			WeaponData->MaxRangeM * 100.0f,
			WeaponData->DamageFalloffStartM * 100.0f,
			WeaponData->MinDamageMultiplier,
			bIsTracer,
			this
		);
	}
}

/* -----------------------------------------------------------------------
 *  Spread / Accuracy
 * --------------------------------------------------------------------- */

float ASHWeaponBase::CalculateCurrentMOA() const
{
	if (!WeaponData)
	{
		return 10.0f;
	}

	const FSHAccuracyModifiers& Acc = WeaponData->AccuracyModifiers;

	// Base MOA from stance
	float MOA = 0.0f;
	switch (CurrentStance)
	{
	case ESHStance::Standing:  MOA = Acc.StandingMOA;  break;
	case ESHStance::Crouching: MOA = Acc.CrouchingMOA; break;
	case ESHStance::Prone:     MOA = Acc.ProneMOA;     break;
	}

	// Movement penalty
	if (bIsMoving)
	{
		MOA += Acc.MovingPenaltyMOA;
	}

	// Suppression & fatigue
	MOA += Acc.SuppressionMaxMOA * SuppressionLevel;
	MOA += Acc.FatigueMaxMOA * FatigueLevel;

	// ADS bonus
	if (bIsADS)
	{
		MOA *= FMath::Lerp(1.0f, Acc.ADSMultiplier, ADSAlpha);
	}

	return FMath::Max(MOA, 0.05f); // Absolute minimum spread
}

FVector ASHWeaponBase::ApplySpread(const FVector& BaseDirection) const
{
	const float MOA = CalculateCurrentMOA();

	// Convert MOA to radians. 1 MOA ~ 0.000290888 radians.
	const float SpreadRadians = MOA * 0.000290888f;

	// Random cone
	const float HalfAngle = SpreadRadians * 0.5f;
	const FVector SpreadDir = FMath::VRandCone(BaseDirection, HalfAngle);

	return SpreadDir.GetSafeNormal();
}

FTransform ASHWeaponBase::GetMuzzleTransform() const
{
	if (WeaponMeshComp && WeaponMeshComp->DoesSocketExist(MuzzleSocketName))
	{
		return WeaponMeshComp->GetSocketTransform(MuzzleSocketName);
	}

	// Fallback to actor transform
	return GetActorTransform();
}

/* -----------------------------------------------------------------------
 *  Recoil
 * --------------------------------------------------------------------- */

void ASHWeaponBase::ApplyRecoil()
{
	if (!WeaponData)
	{
		return;
	}

	const FSHRecoilPattern& RP = WeaponData->RecoilPattern;

	float VerticalKick = RP.VerticalRecoil;
	float HorizontalKick = RP.HorizontalRecoil;

	// First-shot multiplier
	if (ShotsFiredInString == 0)
	{
		VerticalKick *= RP.FirstShotMultiplier;
		HorizontalKick *= RP.FirstShotMultiplier;
	}

	// Horizontal pattern (deterministic component)
	if (RP.HorizontalPattern.Num() > 0)
	{
		const int32 PatternIndex = ShotsFiredInString % RP.HorizontalPattern.Num();
		HorizontalKick += RP.HorizontalPattern[PatternIndex];
	}

	// Random horizontal sign if no pattern
	if (RP.HorizontalPattern.Num() == 0)
	{
		HorizontalKick *= (FMath::RandBool() ? 1.0f : -1.0f);
	}

	// Add some random variation (+/- 15%)
	VerticalKick *= FMath::FRandRange(0.85f, 1.15f);
	HorizontalKick *= FMath::FRandRange(0.85f, 1.15f);

	// ADS reduces felt recoil slightly
	if (bIsADS)
	{
		const float ADSRecoilReduction = FMath::Lerp(1.0f, 0.75f, ADSAlpha);
		VerticalKick *= ADSRecoilReduction;
		HorizontalKick *= ADSRecoilReduction;
	}

	AccumulatedVerticalRecoil += VerticalKick;
	AccumulatedHorizontalRecoil += HorizontalKick;

	// Apply to player controller view rotation
	if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController()))
		{
			PC->AddPitchInput(-VerticalKick);
			PC->AddYawInput(HorizontalKick);
		}
	}
}

void ASHWeaponBase::TickRecoilRecovery(float DeltaTime)
{
	if (!WeaponData)
	{
		return;
	}

	// Only recover when not actively firing
	if (bWantsToFire && WeaponState == ESHWeaponState::Firing)
	{
		return;
	}

	const float RecoveryRate = WeaponData->RecoilPattern.RecoveryRate;

	if (FMath::Abs(AccumulatedVerticalRecoil) > KINDA_SMALL_NUMBER)
	{
		const float RecoverAmount = RecoveryRate * DeltaTime;
		const float RecoverVert = FMath::Min(RecoverAmount, FMath::Abs(AccumulatedVerticalRecoil));

		if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
		{
			if (APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController()))
			{
				PC->AddPitchInput(RecoverVert); // Opposite direction (pitch up to recover down-recoil)
			}
		}

		AccumulatedVerticalRecoil -= RecoverVert;
	}

	if (FMath::Abs(AccumulatedHorizontalRecoil) > KINDA_SMALL_NUMBER)
	{
		const float RecoverAmount = RecoveryRate * DeltaTime * 0.5f; // Horizontal recovers slower
		const float RecoverHoriz = FMath::Min(RecoverAmount, FMath::Abs(AccumulatedHorizontalRecoil));
		const float Sign = FMath::Sign(AccumulatedHorizontalRecoil);

		if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
		{
			if (APlayerController* PC = Cast<APlayerController>(OwnerPawn->GetController()))
			{
				PC->AddYawInput(-Sign * RecoverHoriz);
			}
		}

		AccumulatedHorizontalRecoil -= Sign * RecoverHoriz;
	}
}

/* -----------------------------------------------------------------------
 *  Weapon Sway
 * --------------------------------------------------------------------- */

void ASHWeaponBase::TickWeaponSway(float DeltaTime)
{
	if (!WeaponData)
	{
		return;
	}

	SwayTime += DeltaTime;

	// Sway amplitude scales with fatigue and suppression
	const float SwayScale = FMath::Lerp(0.2f, 1.0f, FMath::Max(FatigueLevel, SuppressionLevel * 0.5f));
	const float MaxSway = WeaponData->MaxSwayDegrees * SwayScale;
	const float Freq = WeaponData->SwayFrequency;

	// ADS reduces sway
	const float ADSSway = FMath::Lerp(1.0f, 0.4f, ADSAlpha);

	// Figure-8 pattern using Lissajous curves
	CurrentSwayOffset.X = FMath::Sin(SwayTime * Freq * PI * 2.0f) * MaxSway * ADSSway;
	CurrentSwayOffset.Y = FMath::Sin(SwayTime * Freq * PI * 2.0f * 0.7f + PI * 0.25f) * MaxSway * 0.6f * ADSSway;
}

/* -----------------------------------------------------------------------
 *  Heat
 * --------------------------------------------------------------------- */

void ASHWeaponBase::AddHeat()
{
	if (!WeaponData)
	{
		return;
	}

	CurrentHeat = FMath::Clamp(CurrentHeat + WeaponData->HeatConfig.HeatPerShot, 0.0f, 1.0f);
	OnHeatChanged.Broadcast(CurrentHeat);

	// Check overheat
	if (WeaponData->HeatConfig.bCanOverheat && CurrentHeat >= WeaponData->HeatConfig.OverheatThreshold)
	{
		bIsOverheated = true;
		bWantsToFire = false;
		SetWeaponState(ESHWeaponState::Overheated);
	}
}

void ASHWeaponBase::TickHeatCooldown(float DeltaTime)
{
	if (!WeaponData || CurrentHeat <= 0.0f)
	{
		return;
	}

	CurrentHeat = FMath::Max(0.0f, CurrentHeat - WeaponData->HeatConfig.CooldownPerSecond * DeltaTime);
	OnHeatChanged.Broadcast(CurrentHeat);

	// Check if we've cooled down enough to resume firing after overheat
	if (bIsOverheated && CurrentHeat <= WeaponData->HeatConfig.CooldownResumeThreshold)
	{
		bIsOverheated = false;
		if (WeaponState == ESHWeaponState::Overheated)
		{
			SetWeaponState(ESHWeaponState::Idle);
		}
	}
}

/* -----------------------------------------------------------------------
 *  Reload State Machine
 * --------------------------------------------------------------------- */

void ASHWeaponBase::BeginReloadSequence()
{
	SetWeaponState(ESHWeaponState::Reloading);
	ReloadState = ESHReloadState::Starting;

	const bool bIsEmpty = (CurrentMagAmmo == 0);

	if (WeaponData->bSingleRoundReload)
	{
		// For single-round reloading, calculate how many rounds to insert
		RoundsToInsert = WeaponData->MagazineCapacity - CurrentMagAmmo;
		ReloadStageDuration = 0.3f; // Brief start animation
		ReloadTimer = ReloadStageDuration;

		PlayAnimMontage(WeaponData->ReloadMontage_FP);
	}
	else
	{
		// Magazine reload
		ReloadStageDuration = bIsEmpty ? WeaponData->EmptyReloadTime : WeaponData->TacticalReloadTime;
		ReloadTimer = ReloadStageDuration;

		if (bIsEmpty)
		{
			PlayAnimMontage(WeaponData->ReloadEmptyMontage_FP);
		}
		else
		{
			PlayAnimMontage(WeaponData->ReloadMontage_FP);
		}
	}

	// Play reload sound
	if (USoundBase* ReloadSnd = WeaponData->SoundProfile.ReloadSound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(this, ReloadSnd, GetActorLocation());
	}

	if (WeaponAnimSystem) { WeaponAnimSystem->OnReloadStarted(); }
}

void ASHWeaponBase::TickReload(float DeltaTime)
{
	if (WeaponState != ESHWeaponState::Reloading)
	{
		return;
	}

	ReloadTimer -= DeltaTime;

	if (ReloadTimer <= 0.0f)
	{
		AdvanceReloadState();
	}
}

void ASHWeaponBase::AdvanceReloadState()
{
	if (!WeaponData)
	{
		return;
	}

	switch (ReloadState)
	{
	case ESHReloadState::Starting:
		if (WeaponData->bSingleRoundReload)
		{
			// Transition to inserting individual rounds
			ReloadState = ESHReloadState::Inserting;
			ReloadStageDuration = WeaponData->SingleRoundInsertTime;
			ReloadTimer = ReloadStageDuration;
		}
		else
		{
			// Magazine swap — go straight to chambering if empty, or finish
			if (CurrentMagAmmo == 0)
			{
				ReloadState = ESHReloadState::Chambering;
				ReloadStageDuration = 0.4f; // Chambering time baked into total
				ReloadTimer = ReloadStageDuration;
			}
			else
			{
				CompleteReload();
			}
		}
		break;

	case ESHReloadState::Inserting:
		// Insert one round
		if (RoundsToInsert > 0 && ReserveAmmo > 0)
		{
			CurrentMagAmmo++;
			ReserveAmmo--;
			RoundsToInsert--;
			OnAmmoChanged.Broadcast(CurrentMagAmmo, ReserveAmmo);
		}

		if (RoundsToInsert > 0 && ReserveAmmo > 0)
		{
			// Continue inserting
			ReloadTimer = WeaponData->SingleRoundInsertTime;
		}
		else
		{
			// Done inserting
			ReloadState = ESHReloadState::Finishing;
			ReloadStageDuration = 0.3f;
			ReloadTimer = ReloadStageDuration;
		}
		break;

	case ESHReloadState::Chambering:
		CompleteReload();
		break;

	case ESHReloadState::Finishing:
		CompleteReload();
		break;

	default:
		CompleteReload();
		break;
	}
}

void ASHWeaponBase::CompleteReload()
{
	if (!WeaponData)
	{
		return;
	}

	if (!WeaponData->bSingleRoundReload)
	{
		// Magazine reload: swap the magazine
		const int32 RoundsNeeded = WeaponData->MagazineCapacity - CurrentMagAmmo;
		const int32 RoundsAvailable = FMath::Min(RoundsNeeded, ReserveAmmo);

		CurrentMagAmmo += RoundsAvailable;
		ReserveAmmo -= RoundsAvailable;
	}

	ReloadState = ESHReloadState::Idle;
	ReloadTimer = 0.0f;
	SetWeaponState(ESHWeaponState::Idle);

	OnAmmoChanged.Broadcast(CurrentMagAmmo, ReserveAmmo);

	if (WeaponAnimSystem) { WeaponAnimSystem->OnReloadFinished(); }
}

/* -----------------------------------------------------------------------
 *  Malfunction
 * --------------------------------------------------------------------- */

bool ASHWeaponBase::RollForMalfunction()
{
	if (!WeaponData)
	{
		return false;
	}

	float MalfunctionChance = WeaponData->BaseMalfunctionChance;

	// Heat increases malfunction probability
	if (CurrentHeat > WeaponData->HeatConfig.MalfunctionHeatThreshold)
	{
		const float HeatExcess = CurrentHeat - WeaponData->HeatConfig.MalfunctionHeatThreshold;
		const float HeatRange = 1.0f - WeaponData->HeatConfig.MalfunctionHeatThreshold;
		if (HeatRange > 0.0f)
		{
			// Up to 50x base chance at max heat
			MalfunctionChance *= (1.0f + 49.0f * (HeatExcess / HeatRange));
		}
	}

	// Dirt increases malfunction probability (up to 10x at max dirt)
	MalfunctionChance *= (1.0f + 9.0f * DirtLevel);

	return FMath::FRand() < MalfunctionChance;
}

void ASHWeaponBase::TriggerMalfunction()
{
	SetWeaponState(ESHWeaponState::Malfunctioned);
	bWantsToFire = false;
	BurstShotsRemaining = 0;
	MalfunctionClearTimer = -1.0f; // Negative means waiting for player input

	OnMalfunction.Broadcast();

	// Play malfunction sound
	if (WeaponData)
	{
		if (USoundBase* MalSnd = WeaponData->SoundProfile.MalfunctionSound.LoadSynchronous())
		{
			UGameplayStatics::PlaySoundAtLocation(this, MalSnd, GetActorLocation());
		}
	}
}

/* -----------------------------------------------------------------------
 *  ADS
 * --------------------------------------------------------------------- */

void ASHWeaponBase::TickADSTransition(float DeltaTime)
{
	if (!WeaponData)
	{
		return;
	}

	const float TransitionTime = FMath::Max(WeaponData->ADSTransitionTime, 0.01f);
	const float TransitionSpeed = 1.0f / TransitionTime;

	if (bIsADS)
	{
		ADSAlpha = FMath::Min(1.0f, ADSAlpha + TransitionSpeed * DeltaTime);
	}
	else
	{
		ADSAlpha = FMath::Max(0.0f, ADSAlpha - TransitionSpeed * DeltaTime);
	}
}

/* -----------------------------------------------------------------------
 *  State Management
 * --------------------------------------------------------------------- */

void ASHWeaponBase::SetWeaponState(ESHWeaponState NewState)
{
	if (WeaponState != NewState)
	{
		WeaponState = NewState;
		OnWeaponStateChanged.Broadcast(NewState);
	}
}

bool ASHWeaponBase::CanFire() const
{
	if (!WeaponData)
	{
		return false;
	}

	if (CurrentMagAmmo <= 0)
	{
		return false;
	}

	if (bIsOverheated)
	{
		return false;
	}

	switch (WeaponState)
	{
	case ESHWeaponState::Idle:
	case ESHWeaponState::Firing:
		return true;
	default:
		return false;
	}
}

bool ASHWeaponBase::CanReload() const
{
	if (!WeaponData)
	{
		return false;
	}

	if (WeaponState == ESHWeaponState::Reloading ||
		WeaponState == ESHWeaponState::Malfunctioned ||
		WeaponState == ESHWeaponState::Equipping)
	{
		return false;
	}

	if (ReserveAmmo <= 0)
	{
		return false;
	}

	if (CurrentMagAmmo >= WeaponData->MagazineCapacity)
	{
		return false;
	}

	return true;
}

/* -----------------------------------------------------------------------
 *  VFX / Audio
 * --------------------------------------------------------------------- */

void ASHWeaponBase::PlayMuzzleFlash()
{
	if (!WeaponData)
	{
		return;
	}

	if (UParticleSystem* MuzzleVFX = WeaponData->MuzzleFlashVFX.LoadSynchronous())
	{
		const FTransform MuzzleTM = GetMuzzleTransform();
		UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(), MuzzleVFX, MuzzleTM.GetLocation(), MuzzleTM.Rotator(), true);
	}
}

void ASHWeaponBase::SpawnShellCasing()
{
	if (!WeaponData)
	{
		return;
	}

	if (UParticleSystem* ShellVFX = WeaponData->ShellEjectVFX.LoadSynchronous())
	{
		FTransform EjectTM;
		if (WeaponMeshComp && WeaponMeshComp->DoesSocketExist(ShellEjectSocketName))
		{
			EjectTM = WeaponMeshComp->GetSocketTransform(ShellEjectSocketName);
		}
		else
		{
			EjectTM = GetActorTransform();
		}

		UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(), ShellVFX, EjectTM.GetLocation(), EjectTM.Rotator(), true);
	}
}

void ASHWeaponBase::PlayFireSound()
{
	if (!WeaponData)
	{
		return;
	}

	USoundBase* SoundToPlay = WeaponData->SoundProfile.FireSound.LoadSynchronous();
	USoundAttenuation* Attenuation = WeaponData->SoundProfile.FireAttenuation.LoadSynchronous();

	if (SoundToPlay)
	{
		UGameplayStatics::PlaySoundAtLocation(
			this, SoundToPlay, GetActorLocation(), 1.0f, 1.0f, 0.0f,
			Attenuation);
	}
}

void ASHWeaponBase::PlayFireModeSwitch()
{
	if (!WeaponData)
	{
		return;
	}

	if (USoundBase* SwitchSnd = WeaponData->SoundProfile.FireModeSwitchSound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(this, SwitchSnd, GetActorLocation());
	}
}

void ASHWeaponBase::PlayAnimMontage(const TSoftObjectPtr<UAnimMontage>& Montage)
{
	if (UAnimMontage* LoadedMontage = Montage.LoadSynchronous())
	{
		if (APawn* OwnerPawn = Cast<APawn>(GetOwner()))
		{
			if (USkeletalMeshComponent* PawnMesh = OwnerPawn->FindComponentByClass<USkeletalMeshComponent>())
			{
				if (UAnimInstance* AnimInstance = PawnMesh->GetAnimInstance())
				{
					AnimInstance->Montage_Play(LoadedMontage);
				}
			}
		}
	}
}

/* -----------------------------------------------------------------------
 *  Replication
 * --------------------------------------------------------------------- */

void ASHWeaponBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASHWeaponBase, CurrentMagAmmo);
	DOREPLIFETIME(ASHWeaponBase, ReserveAmmo);
	DOREPLIFETIME(ASHWeaponBase, CurrentFireMode);
	DOREPLIFETIME(ASHWeaponBase, WeaponState);
}
