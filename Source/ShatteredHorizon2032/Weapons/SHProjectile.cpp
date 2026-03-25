// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "Weapons/SHProjectile.h"
#include "Weapons/SHBallisticsSystem.h"
#include "Weapons/SHWeaponData.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/DamageType.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/World.h"

// ---------------------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------------------

ASHProjectile::ASHProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	// Collision sphere — small radius for bullet
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->SetSphereRadius(0.5f);
	CollisionSphere->SetCollisionProfileName(TEXT("Projectile"));
	CollisionSphere->SetGenerateOverlapEvents(false);
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision); // We do manual traces
	SetRootComponent(CollisionSphere);

	// Projectile movement — we override its physics in Tick, but keep the
	// component for network replication and interop.
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.0f; // We handle gravity ourselves
	ProjectileMovement->InitialSpeed = 0.0f;
	ProjectileMovement->MaxSpeed = 0.0f; // Uncapped by setting to 0
	ProjectileMovement->bSimulationEnabled = false; // We drive manually

	// Tracer VFX component — activated only for tracer rounds
	TracerVFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TracerVFX"));
	TracerVFX->SetupAttachment(RootComponent);
	TracerVFX->SetAutoActivate(false);

	// Defaults
	CurrentVelocity = FVector::ZeroVector;
	CurrentDamage = 0.0f;
	DistanceTravelled = 0.0f;
	bIsTracer = false;
	PenetrationCount = 0;
	RicochetCount = 0;
	LifetimeElapsed = 0.0f;
	PreviousPosition = FVector::ZeroVector;

	// No replication for now (server-authoritative projectiles would need it)
	bReplicates = false;
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

void ASHProjectile::InitProjectile(
	const USHWeaponData* InWeaponData,
	const FVector& InMuzzleVelocity,
	float InDamage,
	bool bInIsTracer,
	APawn* InInstigator)
{
	WeaponData = InWeaponData;
	CurrentVelocity = InMuzzleVelocity;
	CurrentDamage = InDamage;
	bIsTracer = bInIsTracer;
	InstigatorPawn = InInstigator;
	PreviousPosition = GetActorLocation();
	PenetrationCount = 0;
	RicochetCount = 0;
	DistanceTravelled = 0.0f;
	LifetimeElapsed = 0.0f;

	if (InInstigator)
	{
		SetInstigator(InInstigator);
	}

	SetTracerVisible(bIsTracer);
}

// ---------------------------------------------------------------------------
// BeginPlay
// ---------------------------------------------------------------------------

void ASHProjectile::BeginPlay()
{
	Super::BeginPlay();

	if (UWorld* World = GetWorld())
	{
		BallisticsSystem = World->GetSubsystem<USHBallisticsSystem>();
	}

	PreviousPosition = GetActorLocation();
}

// ---------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------

void ASHProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	LifetimeElapsed += DeltaTime;

	// Lifetime cutoff
	if (LifetimeElapsed >= MaxLifetime || HasExceededMaxRange())
	{
		Destroy();
		return;
	}

	TickBallistics(DeltaTime);
	CheckSupersonicCracks();
}

// ---------------------------------------------------------------------------
// Ballistic tick
// ---------------------------------------------------------------------------

void ASHProjectile::TickBallistics(float DeltaTime)
{
	if (!BallisticsSystem || !WeaponData)
	{
		// Fallback: simple linear motion
		const FVector NewPos = GetActorLocation() + CurrentVelocity * DeltaTime;
		SetActorLocation(NewPos);
		return;
	}

	PreviousPosition = GetActorLocation();

	// Use the ballistics subsystem to compute new position / velocity
	FVector NewPosition, NewVelocity;
	BallisticsSystem->StepProjectile(
		PreviousPosition,
		CurrentVelocity,
		DeltaTime,
		WeaponData,
		NewPosition,
		NewVelocity);

	// Sweep trace for collision between old and new position
	FHitResult Hit;
	if (PerformSweepTrace(PreviousPosition, NewPosition, Hit))
	{
		// Move to impact point
		SetActorLocation(Hit.ImpactPoint);
		ProcessHit(Hit);
		// If we're still alive after processing (penetration/ricochet), the
		// position and velocity have been updated in ProcessHit.
	}
	else
	{
		// No hit — update position and velocity
		const float StepDistance = FVector::Dist(PreviousPosition, NewPosition);
		DistanceTravelled += StepDistance;

		SetActorLocation(NewPosition);
		CurrentVelocity = NewVelocity;

		// Orient along velocity
		if (!CurrentVelocity.IsNearlyZero())
		{
			SetActorRotation(CurrentVelocity.Rotation());
		}
	}
}

// ---------------------------------------------------------------------------
// Sweep trace
// ---------------------------------------------------------------------------

bool ASHProjectile::PerformSweepTrace(const FVector& Start, const FVector& End, FHitResult& OutHit) const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(SHProjectile), true, this);
	Params.bReturnPhysicalMaterial = true;

	if (InstigatorPawn.IsValid())
	{
		Params.AddIgnoredActor(InstigatorPawn.Get());
	}

	return World->LineTraceSingleByChannel(OutHit, Start, End, ECC_GameTraceChannel1, Params);
}

// ---------------------------------------------------------------------------
// Hit processing
// ---------------------------------------------------------------------------

void ASHProjectile::ProcessHit(const FHitResult& HitResult)
{
	if (!BallisticsSystem || !WeaponData)
	{
		Destroy();
		return;
	}

	// Determine material
	const ESHPenetrationMaterial Material =
		USHBallisticsSystem::PhysMaterialToSHMaterial(HitResult.PhysMaterial.Get());

	// Calculate damage at this distance
	CurrentDamage = BallisticsSystem->CalcDamageAtDistance(WeaponData, DistanceTravelled);

	// Spawn impact VFX
	SpawnImpactEffect(HitResult, Material);

	// --- Explosive rounds detonate on impact ---
	if (WeaponData->bIsExplosive)
	{
		BallisticsSystem->ProcessExplosion(
			HitResult.ImpactPoint,
			WeaponData,
			InstigatorPawn.Get(),
			this);
		Destroy();
		return;
	}

	// --- Apply damage to hit actor ---
	ApplyDamageToActor(HitResult, CurrentDamage);

	// --- Try penetration ---
	const FVector InDir = CurrentVelocity.GetSafeNormal();
	const float InSpeed = CurrentVelocity.Size();

	if (PenetrationCount < MaxPenetrations)
	{
		FSHPenetrationResult PenResult = BallisticsSystem->TryPenetrate(
			HitResult, InDir, InSpeed, CurrentDamage, WeaponData, Material);

		if (PenResult.bPenetrated)
		{
			PenetrationCount++;
			CurrentDamage = PenResult.DamageAfterPenetration;
			CurrentVelocity = PenResult.ExitDirection * PenResult.VelocityAfterPenetration;
			SetActorLocation(PenResult.ExitPoint);
			SetActorRotation(PenResult.ExitDirection.Rotation());
			return; // Continue flight
		}
	}

	// --- Try ricochet ---
	if (RicochetCount < MaxRicochets)
	{
		FSHRicochetResult RicResult = BallisticsSystem->TryRicochet(
			HitResult, InDir, InSpeed, CurrentDamage, Material);

		if (RicResult.bRicocheted)
		{
			RicochetCount++;
			CurrentDamage = RicResult.RicochetDamage;
			CurrentVelocity = RicResult.RicochetDirection * RicResult.RicochetSpeed;
			SetActorLocation(RicResult.RicochetOrigin);
			SetActorRotation(RicResult.RicochetDirection.Rotation());
			return; // Continue flight
		}
	}

	// --- Neither penetrated nor ricocheted — bullet stops ---
	Destroy();
}

// ---------------------------------------------------------------------------
// Damage application
// ---------------------------------------------------------------------------

void ASHProjectile::ApplyDamageToActor(const FHitResult& HitResult, float Damage)
{
	AActor* HitActor = HitResult.GetActor();
	if (!HitActor || Damage <= 0.0f)
	{
		return;
	}

	AController* InstigatorController = nullptr;
	if (InstigatorPawn.IsValid())
	{
		InstigatorController = InstigatorPawn->GetController();
	}

	FPointDamageEvent DamageEvent;
	DamageEvent.Damage = Damage;
	DamageEvent.HitInfo = HitResult;
	DamageEvent.ShotDirection = CurrentVelocity.GetSafeNormal();

	HitActor->TakeDamage(Damage, DamageEvent, InstigatorController, this);
}

// ---------------------------------------------------------------------------
// Impact VFX
// ---------------------------------------------------------------------------

void ASHProjectile::SpawnImpactEffect(const FHitResult& HitResult, ESHPenetrationMaterial Material)
{
	UNiagaraSystem* EffectToSpawn = nullptr;

	if (const TObjectPtr<UNiagaraSystem>* Found = ImpactEffects.Find(Material))
	{
		EffectToSpawn = *Found;
	}

	if (!EffectToSpawn)
	{
		EffectToSpawn = DefaultImpactEffect;
	}

	if (EffectToSpawn)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(),
			EffectToSpawn,
			HitResult.ImpactPoint,
			HitResult.ImpactNormal.Rotation(),
			FVector::OneVector,
			true,  // bAutoDestroy
			true,  // bAutoActivate
			ENCPoolMethod::AutoRelease);
	}
}

// ---------------------------------------------------------------------------
// Supersonic crack
// ---------------------------------------------------------------------------

void ASHProjectile::CheckSupersonicCracks()
{
	if (!BallisticsSystem || !SupersonicCrackSound)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}

		TWeakObjectPtr<APlayerController> WeakPC(PC);
		if (CrackTriggeredForControllers.Contains(WeakPC))
		{
			continue;
		}

		APawn* ListenerPawn = PC->GetPawn();
		if (!ListenerPawn)
		{
			continue;
		}

		FVector CrackLocation;
		if (BallisticsSystem->ShouldTriggerSupersonicCrack(
				GetActorLocation(),
				CurrentVelocity,
				ListenerPawn->GetActorLocation(),
				CrackLocation))
		{
			CrackTriggeredForControllers.Add(WeakPC);

			UGameplayStatics::PlaySoundAtLocation(
				World,
				SupersonicCrackSound,
				CrackLocation,
				FRotator::ZeroRotator,
				1.0f,
				1.0f,
				0.0f,
				CrackAttenuation);
		}
	}
}

// ---------------------------------------------------------------------------
// Tracer
// ---------------------------------------------------------------------------

void ASHProjectile::SetTracerVisible(bool bVisible)
{
	if (TracerVFX)
	{
		if (bVisible && TracerNiagaraAsset)
		{
			TracerVFX->SetAsset(TracerNiagaraAsset);
			TracerVFX->SetColorParameter(FName(TEXT("TracerColor")), TracerColor);
			TracerVFX->SetFloatParameter(FName(TEXT("Width")), TracerWidth);
			TracerVFX->Activate(true);
		}
		else
		{
			TracerVFX->Deactivate();
		}
	}
}

// ---------------------------------------------------------------------------
// Range check
// ---------------------------------------------------------------------------

bool ASHProjectile::HasExceededMaxRange() const
{
	if (!WeaponData)
	{
		return false;
	}

	const float MaxRangeCm = WeaponData->MaxRangeM * 100.0f;
	return DistanceTravelled >= MaxRangeCm;
}
