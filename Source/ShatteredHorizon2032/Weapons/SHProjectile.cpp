// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHProjectile.h"
#include "SHBallisticsSystem.h"
#include "SHWeaponBase.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Particles/ParticleSystemComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "PhysicalMaterials/PhysicalMaterial.h"

/* -----------------------------------------------------------------------
 *  Construction
 * --------------------------------------------------------------------- */

ASHProjectile::ASHProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// Collision sphere
	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionComp->SetSphereRadius(1.0f); // Bullet-sized
	CollisionComp->SetCollisionProfileName(TEXT("Projectile"));
	CollisionComp->SetGenerateOverlapEvents(false);
	CollisionComp->CanCharacterStepUpOn = ECB_No;
	RootComponent = CollisionComp;

	// Projectile movement — we'll override most of its behavior with custom ballistics,
	// but keep it for sweep-based collision detection.
	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->UpdatedComponent = CollisionComp;
	ProjectileMovement->bRotationFollowsVelocity = true;
	ProjectileMovement->bShouldBounce = false;
	ProjectileMovement->ProjectileGravityScale = 0.0f; // We handle gravity ourselves
	ProjectileMovement->InitialSpeed = 0.0f;
	ProjectileMovement->MaxSpeed = 0.0f; // Will be set during init

	// Tracer VFX — created but inactive until flagged
	TracerVFXComp = CreateDefaultSubobject<UParticleSystemComponent>(TEXT("TracerVFX"));
	TracerVFXComp->SetupAttachment(RootComponent);
	TracerVFXComp->bAutoActivate = false;

	// Collision binding
	CollisionComp->OnComponentHit.AddDynamic(this, &ASHProjectile::OnProjectileHit);

	// Default lifetime
	InitialLifeSpan = 0.0f; // Managed manually
}

/* -----------------------------------------------------------------------
 *  BeginPlay
 * --------------------------------------------------------------------- */

void ASHProjectile::BeginPlay()
{
	Super::BeginPlay();

	SpawnLocation = GetActorLocation();

	// Cache ballistics subsystem
	if (UWorld* World = GetWorld())
	{
		BallisticsSystem = World->GetSubsystem<USHBallisticsSystem>();
	}

	// Lifetime safety
	SetLifeSpan(MaxLifetime);
}

/* -----------------------------------------------------------------------
 *  Initialization
 * --------------------------------------------------------------------- */

void ASHProjectile::InitializeProjectile(
	const FVector& InitialVelocity,
	float InBaseDamage,
	const FSHBallisticCoefficients& InBC,
	float InMaxRangeCm,
	float InDamageFalloffStartCm,
	float InMinDamageMultiplier,
	bool bInIsTracer,
	AActor* InWeaponOwner)
{
	SimulatedVelocity = InitialVelocity;
	BaseDamage = InBaseDamage;
	BallisticCoeffs = InBC;
	MaxRangeCm = InMaxRangeCm;
	DamageFalloffStartCm = InDamageFalloffStartCm;
	MinDamageMultiplier = InMinDamageMultiplier;
	MuzzleVelocityCmS = InitialVelocity.Size();
	bIsTracer = bInIsTracer;
	WeaponOwner = InWeaponOwner;

	// Configure ProjectileMovementComponent for collision sweeps
	ProjectileMovement->MaxSpeed = MuzzleVelocityCmS * 2.0f; // Allow headroom
	ProjectileMovement->Velocity = InitialVelocity;

	// Copy penetration table from weapon data if available
	if (InWeaponOwner)
	{
		if (const ASHWeaponBase* Weapon = Cast<ASHWeaponBase>(InWeaponOwner))
		{
			if (Weapon->WeaponData)
			{
				PenetrationTable = Weapon->WeaponData->PenetrationTable;
			}
		}
	}

	// Ignore the firing weapon and its owner for collision
	if (InWeaponOwner)
	{
		CollisionComp->MoveIgnoreActors.Add(InWeaponOwner);
		if (AActor* WeaponOwnerOwner = InWeaponOwner->GetOwner())
		{
			CollisionComp->MoveIgnoreActors.Add(WeaponOwnerOwner);
		}
	}

	// Activate tracer
	if (bIsTracer && TracerParticle)
	{
		TracerVFXComp->SetTemplate(TracerParticle);
		TracerVFXComp->Activate(true);
	}
}

/* -----------------------------------------------------------------------
 *  Tick — Custom Ballistic Simulation
 * --------------------------------------------------------------------- */

void ASHProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!bIsActive)
	{
		return;
	}

	CustomBallisticTick(DeltaTime);
	CheckSupersonicCrack();
}

void ASHProjectile::CustomBallisticTick(float DeltaTime)
{
	if (!BallisticsSystem)
	{
		// Fallback: simple integration without drag/wind
		const FVector Gravity(0.0f, 0.0f, -981.0f);
		SimulatedVelocity += Gravity * DeltaTime;

		FVector NewPos = GetActorLocation() + SimulatedVelocity * DeltaTime;
		SetActorLocation(NewPos, true); // Sweep for collision
	}
	else
	{
		FVector CurrentPos = GetActorLocation();
		FVector PrevPos = CurrentPos;

		// Let the ballistics system step the simulation
		BallisticsSystem->StepSimulation(CurrentPos, SimulatedVelocity, BallisticCoeffs, DeltaTime);

		// Use sweep move for collision detection
		FHitResult SweepHit;
		const FVector MoveDelta = CurrentPos - PrevPos;

		SetActorLocation(CurrentPos, true, &SweepHit);

		if (SweepHit.bBlockingHit)
		{
			HandleImpact(SweepHit);
			return;
		}
	}

	// Update projectile movement component velocity for rotation following
	ProjectileMovement->Velocity = SimulatedVelocity;

	// Track distance
	DistanceTraveled = FVector::Dist(SpawnLocation, GetActorLocation());

	// Max range check
	if (DistanceTraveled >= MaxRangeCm)
	{
		DeactivateAndDestroy();
	}

	// Minimum velocity check (projectile has effectively stopped)
	if (SimulatedVelocity.SizeSquared() < 100.0f * 100.0f) // < 1 m/s
	{
		DeactivateAndDestroy();
	}
}

/* -----------------------------------------------------------------------
 *  Collision
 * --------------------------------------------------------------------- */

void ASHProjectile::OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
{
	if (!bIsActive)
	{
		return;
	}

	HandleImpact(Hit);
}

void ASHProjectile::HandleImpact(const FHitResult& HitResult)
{
	if (!bIsActive)
	{
		return;
	}

	// Explosive projectiles detonate on any impact
	if (bIsExplosive)
	{
		Detonate(HitResult.ImpactPoint);
		return;
	}

	// Calculate current damage with falloff
	DistanceTraveled = FVector::Dist(SpawnLocation, HitResult.ImpactPoint);
	const float CurrentDamage = USHBallisticsSystem::CalculateDamageAtDistance(
		BaseDamage, DamageFalloffStartCm, MaxRangeCm, MinDamageMultiplier, DistanceTraveled);

	// Apply direct damage to the hit actor
	if (AActor* HitActor = HitResult.GetActor())
	{
		ApplyDamage(HitActor, CurrentDamage, HitResult);
	}

	// Spawn impact effects
	SpawnImpactEffects(HitResult);

	// Check penetration / ricochet via ballistics system
	if (BallisticsSystem)
	{
		// Determine material and find penetration data
		const ESHPenetrableMaterial MatType = BallisticsSystem->ClassifyMaterial(HitResult);

		// Find penetration entry for this material
		const FSHPenetrationEntry* PenData = nullptr;
		for (const FSHPenetrationEntry& Entry : PenetrationTable)
		{
			if (Entry.Material == MatType)
			{
				PenData = &Entry;
				break;
			}
		}

		if (PenData)
		{
			FSHBallisticHitResult BallisticResult;
			const bool bContinues = BallisticsSystem->EvaluateImpact(
				HitResult,
				SimulatedVelocity,
				CurrentDamage,
				*PenData,
				MuzzleVelocityCmS,
				BallisticResult);

			if (bContinues)
			{
				if (BallisticResult.bPenetrated && PenetrationCount < MaxPenetrations)
				{
					ContinueAfterPenetration(BallisticResult, HitResult);
					return;
				}
				else if (BallisticResult.bRicocheted && RicochetCount < MaxRicochets)
				{
					ApplyRicochet(BallisticResult, HitResult);
					return;
				}
			}
		}
	}

	// Round stops here
	DeactivateAndDestroy();
}

/* -----------------------------------------------------------------------
 *  Damage Application
 * --------------------------------------------------------------------- */

void ASHProjectile::ApplyDamage(AActor* HitActor, float Damage, const FHitResult& HitResult)
{
	if (!HitActor || Damage <= 0.0f)
	{
		return;
	}

	AController* InstigatorController = nullptr;
	if (AActor* OwnerActor = WeaponOwner.Get())
	{
		if (APawn* OwnerPawn = Cast<APawn>(OwnerActor->GetOwner()))
		{
			InstigatorController = OwnerPawn->GetController();
		}
	}

	FPointDamageEvent DamageEvent;
	DamageEvent.Damage = Damage;
	DamageEvent.HitInfo = HitResult;
	DamageEvent.ShotDirection = SimulatedVelocity.GetSafeNormal();

	HitActor->TakeDamage(
		Damage,
		FDamageEvent(DamageEvent.GetTypeID()),
		InstigatorController,
		this);
}

/* -----------------------------------------------------------------------
 *  Impact Effects
 * --------------------------------------------------------------------- */

void ASHProjectile::SpawnImpactEffects(const FHitResult& HitResult)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Choose effect based on surface type
	UParticleSystem* EffectToSpawn = DefaultImpactEffect;

	if (const UPhysicalMaterial* PhysMat = HitResult.PhysMaterial.Get())
	{
		if (TObjectPtr<UParticleSystem>* Found = ImpactEffects.Find(PhysMat->SurfaceType))
		{
			if (*Found)
			{
				EffectToSpawn = *Found;
			}
		}
	}

	if (EffectToSpawn)
	{
		const FRotator ImpactRotation = HitResult.ImpactNormal.Rotation();
		UGameplayStatics::SpawnEmitterAtLocation(
			World, EffectToSpawn, HitResult.ImpactPoint, ImpactRotation, true);
	}

	// Impact sound
	if (ImpactSound)
	{
		UGameplayStatics::PlaySoundAtLocation(World, ImpactSound, HitResult.ImpactPoint);
	}
}

/* -----------------------------------------------------------------------
 *  Explosive Detonation
 * --------------------------------------------------------------------- */

void ASHProjectile::Detonate(const FVector& Location)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		DeactivateAndDestroy();
		return;
	}

	// Radial damage
	AController* InstigatorController = nullptr;
	if (AActor* OwnerActor = WeaponOwner.Get())
	{
		if (APawn* OwnerPawn = Cast<APawn>(OwnerActor->GetOwner()))
		{
			InstigatorController = OwnerPawn->GetController();
		}
	}

	TArray<AActor*> IgnoreActors;
	IgnoreActors.Add(this);

	UGameplayStatics::ApplyRadialDamageWithFalloff(
		World,
		ExplosionDamage,
		ExplosionDamage * 0.1f,  // Minimum damage at edge
		Location,
		ExplosionRadiusCm * 0.3f, // Inner radius (full damage)
		ExplosionRadiusCm,         // Outer radius
		2.0f,                      // Damage falloff exponent
		nullptr,                   // DamageTypeClass
		IgnoreActors,
		this,
		InstigatorController);

	// Fragmentation / shrapnel
	if (BallisticsSystem && FragmentationParams.FragmentCount > 0)
	{
		BallisticsSystem->SpawnFragmentation(
			Location, FragmentationParams, InstigatorController, this);
	}

	// VFX
	if (ExplosionVFX)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			World, ExplosionVFX, Location, FRotator::ZeroRotator, true);
	}

	// Sound
	if (ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(World, ExplosionSound, Location);
	}

	DeactivateAndDestroy();
}

/* -----------------------------------------------------------------------
 *  Penetration
 * --------------------------------------------------------------------- */

void ASHProjectile::ContinueAfterPenetration(
	const FSHBallisticHitResult& BallisticResult,
	const FHitResult& OriginalHit)
{
	PenetrationCount++;

	// Move to exit point and continue with reduced velocity
	const FVector ExitOffset = BallisticResult.PostPenetrationVelocity.GetSafeNormal() * 5.0f; // 5cm past the surface
	SetActorLocation(OriginalHit.ImpactPoint + ExitOffset, false);

	SimulatedVelocity = BallisticResult.PostPenetrationVelocity;
	BaseDamage = BallisticResult.PostPenetrationDamage;

	// Update rotation to match new velocity
	if (!SimulatedVelocity.IsNearlyZero())
	{
		SetActorRotation(SimulatedVelocity.Rotation());
	}

	ProjectileMovement->Velocity = SimulatedVelocity;
}

/* -----------------------------------------------------------------------
 *  Ricochet
 * --------------------------------------------------------------------- */

void ASHProjectile::ApplyRicochet(
	const FSHBallisticHitResult& BallisticResult,
	const FHitResult& OriginalHit)
{
	RicochetCount++;

	// Move slightly off the surface along ricochet direction
	const FVector RicochetStart = OriginalHit.ImpactPoint +
		BallisticResult.RicochetDirection * 5.0f; // 5cm off surface

	SetActorLocation(RicochetStart, false);

	SimulatedVelocity = BallisticResult.RicochetDirection * BallisticResult.RicochetSpeed;
	BaseDamage = BallisticResult.RicochetDamage;

	if (!SimulatedVelocity.IsNearlyZero())
	{
		SetActorRotation(SimulatedVelocity.Rotation());
	}

	ProjectileMovement->Velocity = SimulatedVelocity;

	// Ricochet sound
	if (RicochetSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			GetWorld(), RicochetSound, OriginalHit.ImpactPoint);
	}
}

/* -----------------------------------------------------------------------
 *  Supersonic Crack
 * --------------------------------------------------------------------- */

void ASHProjectile::CheckSupersonicCrack()
{
	if (!SupersonicCrackSound)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector ProjectilePos = GetActorLocation();
	const float NearMissRadius = 300.0f; // 3 meters

	// Check all player pawns
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC)
		{
			continue;
		}

		APawn* ListenerPawn = PC->GetPawn();
		if (!ListenerPawn)
		{
			continue;
		}

		// Skip the shooter
		if (WeaponOwner.IsValid())
		{
			if (AActor* OwnerActor = WeaponOwner->GetOwner())
			{
				if (OwnerActor == ListenerPawn)
				{
					continue;
				}
			}
		}

		// Already notified?
		if (SupersonicCrackNotifiedActors.Contains(ListenerPawn))
		{
			continue;
		}

		const FVector ListenerPos = ListenerPawn->GetActorLocation();

		if (USHBallisticsSystem::ShouldTriggerSupersonicCrack(
			ProjectilePos, SimulatedVelocity, ListenerPos, NearMissRadius))
		{
			SupersonicCrackNotifiedActors.Add(ListenerPawn);

			// Play the supersonic crack at the listener's location (it's a local phenomenon)
			UGameplayStatics::PlaySoundAtLocation(
				World, SupersonicCrackSound, ListenerPos, 1.0f, 1.0f, 0.0f, nullptr);
		}
	}
}

/* -----------------------------------------------------------------------
 *  Cleanup
 * --------------------------------------------------------------------- */

void ASHProjectile::DeactivateAndDestroy()
{
	bIsActive = false;

	// Disable collision
	if (CollisionComp)
	{
		CollisionComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Stop movement
	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->SetActive(false);
	}

	// Deactivate tracer
	if (TracerVFXComp && TracerVFXComp->IsActive())
	{
		TracerVFXComp->Deactivate();
	}

	// Destroy after a short delay (let particles finish)
	SetLifeSpan(2.0f);
}
