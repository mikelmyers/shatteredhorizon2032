// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHThrowableSystem.h"
#include "SHSuppressionSystem.h"
#include "SHBallisticsSystem.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DamageEvents.h"

// =====================================================================
//  ASHThrowableProjectile
// =====================================================================

ASHThrowableProjectile::ASHThrowableProjectile()
{
	PrimaryActorTick.bCanEverTick = true;

	MeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MeshComp"));
	MeshComp->SetSimulatePhysics(false);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	MeshComp->SetCollisionResponseToAllChannels(ECR_Block);
	MeshComp->SetNotifyRigidBodyCollision(true);
	RootComponent = MeshComp;

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = MeshComp;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = true;
	ProjectileMovement->ProjectileGravityScale = 1.0f;
	ProjectileMovement->InitialSpeed = 0.0f;
	ProjectileMovement->MaxSpeed = 5000.0f;

	MeshComp->OnComponentHit.AddDynamic(this, &ASHThrowableProjectile::OnProjectileHit);
}

void ASHThrowableProjectile::BeginPlay()
{
	Super::BeginPlay();
}

void ASHThrowableProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (State == ESHThrowableState::Detonated || State == ESHThrowableState::Dud)
	{
		return;
	}

	// Countdown fuse (whether cooking in hand or in flight/bouncing).
	if (bIsCooking || State == ESHThrowableState::InFlight || State == ESHThrowableState::Bouncing)
	{
		RemainingFuseTime -= DeltaTime;

		if (bIsCooking)
		{
			OnCooked.Broadcast(RemainingFuseTime);
		}

		if (RemainingFuseTime <= 0.0f)
		{
			Detonate();
		}
	}
}

void ASHThrowableProjectile::Throw(const FVector& ThrowVelocity, const FSHThrowableData& InData, AController* InInstigator)
{
	ThrowableData = InData;
	InstigatorController = InInstigator;
	RemainingFuseTime = bIsCooking ? RemainingFuseTime : InData.FuseTime;
	bIsCooking = false;

	State = ESHThrowableState::InFlight;

	// Configure bounce behavior.
	ProjectileMovement->Bounciness = InData.BounceCoefficient;
	ProjectileMovement->Friction = InData.FrictionCoefficient;

	// Set velocity.
	ProjectileMovement->Velocity = ThrowVelocity;
	ProjectileMovement->Activate();

	// C4 doesn't have a fuse timer — requires manual detonation.
	if (InData.Type == ESHThrowableType::C4Charge)
	{
		RemainingFuseTime = 999999.0f; // Effectively infinite until manual trigger.
	}
}

void ASHThrowableProjectile::StartCooking()
{
	if (State != ESHThrowableState::Idle)
	{
		return;
	}

	bIsCooking = true;
	State = ESHThrowableState::Cooking;
	RemainingFuseTime = ThrowableData.FuseTime;

	if (PinPullSound)
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), PinPullSound, GetActorLocation());
	}
}

void ASHThrowableProjectile::ManualDetonate()
{
	if (ThrowableData.Type != ESHThrowableType::C4Charge)
	{
		return;
	}

	if (State == ESHThrowableState::InFlight || State == ESHThrowableState::Bouncing || State == ESHThrowableState::Idle)
	{
		Detonate();
	}
}

void ASHThrowableProjectile::OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (State == ESHThrowableState::Detonated)
	{
		return;
	}

	BounceCount++;

	if (BounceSound)
	{
		const float VolumeScale = FMath::Clamp(NormalImpulse.Size() / 50000.0f, 0.1f, 1.0f);
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), BounceSound, Hit.ImpactPoint, VolumeScale);
	}

	if (State == ESHThrowableState::InFlight)
	{
		State = ESHThrowableState::Bouncing;
	}
}

// =====================================================================
//  Detonation
// =====================================================================

void ASHThrowableProjectile::Detonate()
{
	if (State == ESHThrowableState::Detonated)
	{
		return;
	}

	State = ESHThrowableState::Detonated;
	const FVector Origin = GetActorLocation();

	// Stop physics.
	ProjectileMovement->StopMovementImmediately();
	ProjectileMovement->Deactivate();

	// Play detonation VFX and sound.
	if (DetonationVFX)
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), DetonationVFX, Origin, FRotator::ZeroRotator, FVector(1.0f), true);
	}
	if (DetonationSound)
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), DetonationSound, Origin);
	}

	// Type-specific effects.
	switch (ThrowableData.Type)
	{
	case ESHThrowableType::FragGrenade:
		ApplyBlastDamage(Origin);
		ApplyFragmentation(Origin);
		break;

	case ESHThrowableType::SmokeGrenade:
		SpawnSmokeCloud(Origin);
		break;

	case ESHThrowableType::Flashbang:
		ApplyFlashbangEffect(Origin);
		break;

	case ESHThrowableType::Incendiary:
		SpawnFireZone(Origin);
		break;

	case ESHThrowableType::C4Charge:
		// C4 acts like a large fragmentation charge.
		ApplyBlastDamage(Origin);
		ApplyFragmentation(Origin);
		break;

	case ESHThrowableType::Flare:
		// Flare illuminates but no damage. Visual handled by BP.
		break;
	}

	OnDetonated.Broadcast(ThrowableData.Type, Origin);

	// Hide mesh, schedule destruction.
	MeshComp->SetVisibility(false, true);
	MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	SetLifeSpan(10.0f); // Cleanup after VFX fade.
}

void ASHThrowableProjectile::ApplyFragmentation(const FVector& Origin)
{
	UWorld* World = GetWorld();
	if (!World) return;

	// Generate fragments in a sphere. Each fragment ray-casts for hits.
	for (int32 i = 0; i < ThrowableData.FragmentCount; ++i)
	{
		// Random direction on unit sphere.
		const FVector Dir = FMath::VRand();
		const float Range = ThrowableData.CasualtyRadiusCm;

		FHitResult Hit;
		FCollisionQueryParams Params(SCENE_QUERY_STAT(GrenadeFragment), true);
		Params.AddIgnoredActor(this);

		if (World->LineTraceSingleByChannel(Hit, Origin, Origin + Dir * Range, ECC_Visibility, Params))
		{
			AActor* HitActor = Hit.GetActor();
			if (HitActor)
			{
				const float Distance = Hit.Distance;
				// Damage falls off with distance: full at lethal radius, scaling to zero at casualty radius.
				float DamageFraction = 1.0f;
				if (Distance > ThrowableData.LethalRadiusCm)
				{
					DamageFraction = 1.0f - FMath::Clamp(
						(Distance - ThrowableData.LethalRadiusCm) /
						(ThrowableData.CasualtyRadiusCm - ThrowableData.LethalRadiusCm),
						0.0f, 1.0f);
				}

				const float FragDmg = ThrowableData.FragmentDamage * DamageFraction;
				if (FragDmg > 0.0f)
				{
					FPointDamageEvent DmgEvent(FragDmg, Hit, Dir, UDamageType::StaticClass());
					HitActor->TakeDamage(FragDmg, DmgEvent,
						InstigatorController, this);
				}
			}
		}
	}
}

void ASHThrowableProjectile::ApplyBlastDamage(const FVector& Origin)
{
	UWorld* World = GetWorld();
	if (!World) return;

	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(this);

	UGameplayStatics::ApplyRadialDamageWithFalloff(
		World,
		ThrowableData.BlastDamage,
		ThrowableData.BlastDamage * 0.05f,
		Origin,
		ThrowableData.LethalRadiusCm,
		ThrowableData.CasualtyRadiusCm,
		1.0f, // falloff exponent
		UDamageType::StaticClass(),
		IgnoreActors,
		this,
		InstigatorController);
}

void ASHThrowableProjectile::SpawnSmokeCloud(const FVector& Origin)
{
	if (SmokeVFX)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(), SmokeVFX, Origin, FRotator::ZeroRotator,
			FVector(ThrowableData.SmokeRadiusCm / 800.0f), // Scale relative to default
			true);
	}

	// Smoke blocks AI sight. Apply via a detection modifier volume.
	// In production, spawn a collision sphere that reduces sight ranges
	// for any AI with perception overlapping it. For now, log intent.
	UE_LOG(LogTemp, Log, TEXT("[Throwable] Smoke cloud spawned at %s, radius %.0f cm, duration %.0f s"),
		*Origin.ToString(), ThrowableData.SmokeRadiusCm, ThrowableData.SmokeDurationSeconds);
}

void ASHThrowableProjectile::ApplyFlashbangEffect(const FVector& Origin)
{
	UWorld* World = GetWorld();
	if (!World) return;

	if (FlashVFX)
	{
		UGameplayStatics::SpawnEmitterAtLocation(World, FlashVFX, Origin, FRotator::ZeroRotator, FVector(1.0f), true);
	}

	// Find all characters within flash radius.
	TArray<FOverlapResult> Overlaps;
	FCollisionShape Sphere = FCollisionShape::MakeSphere(ThrowableData.FlashRadiusCm);
	World->OverlapMultiByChannel(Overlaps, Origin, FQuat::Identity, ECC_Pawn, Sphere);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* HitActor = Overlap.GetActor();
		if (!HitActor) continue;

		const float Distance = FVector::Dist(Origin, HitActor->GetActorLocation());
		const float Effectiveness = 1.0f - FMath::Clamp(Distance / ThrowableData.FlashRadiusCm, 0.0f, 1.0f);

		// Check line-of-sight — flashbang requires visual exposure.
		FHitResult LOSHit;
		FCollisionQueryParams LOSParams;
		LOSParams.AddIgnoredActor(this);
		LOSParams.AddIgnoredActor(HitActor);
		const bool bBlocked = World->LineTraceSingleByChannel(
			LOSHit, Origin, HitActor->GetActorLocation(), ECC_Visibility, LOSParams);

		if (bBlocked)
		{
			continue; // Flashbang blocked by cover — no effect.
		}

		// Apply blind and deaf durations scaled by distance.
		const float BlindTime = ThrowableData.FlashBlindDuration * Effectiveness;
		const float DeafTime = ThrowableData.FlashDeafDuration * Effectiveness;

		// Deliver as damage event so character systems can respond.
		// The actual blind/deaf implementation happens in the character's TakeDamage.
		FRadialDamageEvent FlashDmg;
		FlashDmg.Params.BaseDamage = 1.0f; // Non-lethal marker damage
		FlashDmg.Params.OuterRadius = ThrowableData.FlashRadiusCm;
		FlashDmg.Origin = Origin;

		HitActor->TakeDamage(1.0f, FlashDmg, InstigatorController, this);

		UE_LOG(LogTemp, Log, TEXT("[Flashbang] %s: blind %.1fs, deaf %.1fs (effectiveness %.0f%%)"),
			*HitActor->GetName(), BlindTime, DeafTime, Effectiveness * 100.f);
	}
}

void ASHThrowableProjectile::SpawnFireZone(const FVector& Origin)
{
	if (FireVFX)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			GetWorld(), FireVFX, Origin, FRotator::ZeroRotator,
			FVector(ThrowableData.FireRadiusCm / 400.0f),
			true);
	}

	UE_LOG(LogTemp, Log, TEXT("[Incendiary] Fire zone at %s, radius %.0f cm, burn %.0f DPS for %.0f s"),
		*Origin.ToString(), ThrowableData.FireRadiusCm,
		ThrowableData.BurnDamagePerSecond, ThrowableData.BurnDurationSeconds);
}

// =====================================================================
//  Throwable Data Factory — Doctrine-accurate specifications
// =====================================================================

FSHThrowableData USHThrowableDataFactory::CreateM67FragGrenade()
{
	FSHThrowableData D;
	D.Type = ESHThrowableType::FragGrenade;
	D.FuseTime = 4.0f;                  // M67 fuse: 4-5 seconds
	D.MaxThrowSpeed = 2000.0f;          // ~20 m/s throw
	D.UnderhandSpeedFraction = 0.4f;
	D.MassGrams = 400.0f;              // M67: 400g
	D.BounceCoefficient = 0.25f;        // Steel body, low bounce
	D.FrictionCoefficient = 0.5f;
	D.LethalRadiusCm = 500.0f;          // M67: 5m lethal
	D.CasualtyRadiusCm = 1500.0f;       // M67: 15m casualty
	D.FragmentCount = 200;              // ~200 fragments
	D.FragmentDamage = 40.0f;
	D.BlastDamage = 200.0f;
	return D;
}

FSHThrowableData USHThrowableDataFactory::CreateM18SmokeGrenade()
{
	FSHThrowableData D;
	D.Type = ESHThrowableType::SmokeGrenade;
	D.FuseTime = 2.0f;                  // M18: 1-2 second delay
	D.MaxThrowSpeed = 1800.0f;
	D.MassGrams = 539.0f;              // M18: 539g
	D.BounceCoefficient = 0.2f;
	D.SmokeRadiusCm = 800.0f;          // ~8m radius cloud
	D.SmokeDurationSeconds = 60.0f;     // M18: 50-90 seconds
	D.LethalRadiusCm = 0.0f;
	D.BlastDamage = 0.0f;
	D.FragmentCount = 0;
	return D;
}

FSHThrowableData USHThrowableDataFactory::CreateM84Flashbang()
{
	FSHThrowableData D;
	D.Type = ESHThrowableType::Flashbang;
	D.FuseTime = 1.5f;                  // M84: 1-2 second delay
	D.MaxThrowSpeed = 1800.0f;
	D.MassGrams = 500.0f;              // M84: ~500g
	D.BounceCoefficient = 0.3f;
	D.FlashBlindDuration = 5.0f;        // 3-5 seconds blind
	D.FlashDeafDuration = 8.0f;         // 5-10 seconds deaf
	D.FlashRadiusCm = 600.0f;          // Effective in enclosed spaces
	D.LethalRadiusCm = 0.0f;
	D.BlastDamage = 0.0f;
	D.FragmentCount = 0;
	return D;
}

FSHThrowableData USHThrowableDataFactory::CreateANM14Incendiary()
{
	FSHThrowableData D;
	D.Type = ESHThrowableType::Incendiary;
	D.FuseTime = 2.0f;
	D.MaxThrowSpeed = 1500.0f;
	D.MassGrams = 907.0f;              // AN-M14: ~2 lbs
	D.BounceCoefficient = 0.15f;        // Heavy, minimal bounce
	D.FireRadiusCm = 400.0f;
	D.BurnDurationSeconds = 15.0f;      // Thermite burns for ~15 seconds
	D.BurnDamagePerSecond = 25.0f;      // Lethal in fire zone
	D.LethalRadiusCm = 0.0f;
	D.BlastDamage = 0.0f;
	D.FragmentCount = 0;
	return D;
}

FSHThrowableData USHThrowableDataFactory::CreateM112C4()
{
	FSHThrowableData D;
	D.Type = ESHThrowableType::C4Charge;
	D.FuseTime = 999999.0f;             // Remote detonation only
	D.MaxThrowSpeed = 1200.0f;          // Tossed, not thrown far
	D.MassGrams = 567.0f;              // M112: 1.25 lb block
	D.BounceCoefficient = 0.1f;         // Sticks on impact (plastic)
	D.LethalRadiusCm = 800.0f;          // Larger blast than grenade
	D.CasualtyRadiusCm = 2500.0f;
	D.FragmentCount = 50;               // Some debris, not designed for fragmentation
	D.FragmentDamage = 30.0f;
	D.BlastDamage = 500.0f;             // C4 is primarily blast
	return D;
}

FSHThrowableData USHThrowableDataFactory::CreateType82FragGrenade()
{
	FSHThrowableData D;
	D.Type = ESHThrowableType::FragGrenade;
	D.FuseTime = 3.5f;                  // Type 82: ~3.5 second fuse
	D.MaxThrowSpeed = 1900.0f;
	D.MassGrams = 260.0f;              // Type 82: lighter than M67
	D.BounceCoefficient = 0.3f;
	D.LethalRadiusCm = 600.0f;          // Slightly larger lethal radius
	D.CasualtyRadiusCm = 1200.0f;
	D.FragmentCount = 150;
	D.FragmentDamage = 35.0f;
	D.BlastDamage = 180.0f;
	return D;
}
