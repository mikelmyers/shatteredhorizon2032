// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHFPVStrikeDrone.h"
#include "ShatteredHorizon2032/ShatteredHorizon2032.h"
#include "Net/UnrealNetwork.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Components/AudioComponent.h"
#include "GameFramework/DamageType.h"

ASHFPVStrikeDrone::ASHFPVStrikeDrone()
{
	// FPV strike drones: small, fast, fragile, short battery
	MaxFlightSpeed = 4000.f;
	CruiseSpeed = 3000.f;
	MaxBatterySeconds = 300.f; // 5 minutes max
	BaseAudibleRange = 3000.f;
	FlyingNoiseMultiplier = 2.0f;
	AttackNoiseMultiplier = 3.5f; // High-pitched whine is very audible
	DefaultCruiseAltitude = 3000.f;
	MaxHealth = 30.f; // Very fragile — one good hit destroys it
	SmallArmsDamageMultiplier = 1.0f;
	ShotgunDamageMultiplier = 3.0f;
	FlightAcceleration = 2000.f;
	TurnRate = 180.f; // Very agile

	Affiliation = ESHDroneAffiliation::Enemy;

	// Randomize evasive phase offset
	EvasivePhaseOffset = FMath::FRand() * 2.f * PI;
}

void ASHFPVStrikeDrone::BeginPlay()
{
	Super::BeginPlay();

	// Create dive audio component
	DiveAudioComponent = NewObject<UAudioComponent>(this, TEXT("DiveAudio"));
	if (DiveAudioComponent)
	{
		DiveAudioComponent->RegisterComponent();
		DiveAudioComponent->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		DiveAudioComponent->bAutoActivate = false;
		DiveAudioComponent->bIsUISound = false;

		if (DiveWhineSound)
		{
			DiveAudioComponent->SetSound(DiveWhineSound);
		}
		if (DiveWhineAttenuation)
		{
			DiveAudioComponent->AttenuationSettings = DiveWhineAttenuation;
		}
	}

	SH_LOG(LogSH_Drone, Log, "FPV Strike Drone BeginPlay — Swarm: %s, SwarmID: %d",
		bIsSwarmMember ? TEXT("Yes") : TEXT("No"), SwarmID);
}

float ASHFPVStrikeDrone::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (bHasDetonated)
	{
		return 0.f;
	}

	float FinalDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// If destroyed by damage during flight, still detonate (but with reduced effectiveness)
	// unless health reaches zero from far away shot
	if (CurrentHealth <= 0.f && !bHasDetonated)
	{
		// Drone was shot down — detonate at current position with reduced damage
		if (CurrentStrikePhase == ESHStrikePhase::TerminalDive)
		{
			// Close to target — still dangerous
			Detonate();
		}
		else
		{
			// Shot down before terminal dive — just destroy, no detonation
			// (warhead may not arm until terminal phase)
			DestroyDrone(DamageCauser);
		}
	}

	return FinalDamage;
}

// =======================================================================
//  Strike interface
// =======================================================================

void ASHFPVStrikeDrone::AssignTarget(AActor* TargetActor, ESHStrikeTargetType TargetType)
{
	StrikeTarget = TargetActor;
	StrikeTargetType = TargetType;
	bHasTarget = true;

	if (TargetActor)
	{
		StaticTargetPosition = TargetActor->GetActorLocation();
	}

	SH_LOG(LogSH_Drone, Log, "FPV Strike assigned target: %s (Type: %d)",
		TargetActor ? *TargetActor->GetName() : TEXT("None"), static_cast<int32>(TargetType));
}

void ASHFPVStrikeDrone::AssignTargetPosition(const FVector& Position)
{
	StrikeTarget = nullptr;
	StrikeTargetType = ESHStrikeTargetType::Position;
	StaticTargetPosition = Position;
	bHasTarget = true;
}

void ASHFPVStrikeDrone::BeginTerminalAttack()
{
	if (CurrentStrikePhase == ESHStrikePhase::Detonated)
	{
		return;
	}

	SetStrikePhase(ESHStrikePhase::TerminalDive);
	SetDroneState(ESHDroneState::Attacking);

	// Start dive whine audio
	if (DiveAudioComponent && DiveWhineSound)
	{
		DiveAudioComponent->Play();
		DiveAudioComponent->SetPitchMultiplier(1.5f);
		DiveAudioComponent->SetVolumeMultiplier(1.f);
	}

	// Increase flight speed for terminal dive
	if (FloatingMovement)
	{
		FloatingMovement->MaxSpeed = MaxDiveSpeed;
		FloatingMovement->Acceleration = DiveAcceleration;
	}

	SH_LOG(LogSH_Drone, Log, "FPV Strike beginning terminal dive!");
}

void ASHFPVStrikeDrone::Detonate()
{
	if (bHasDetonated)
	{
		return;
	}

	bHasDetonated = true;
	SetStrikePhase(ESHStrikePhase::Detonated);

	const FVector DetonationPoint = GetActorLocation();

	// Apply explosive damage
	ApplyExplosiveDamage(DetonationPoint);

	// Apply fragmentation
	ApplyFragmentationDamage(DetonationPoint);

	// Spawn VFX
	if (ExplosionVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), ExplosionVFX, DetonationPoint, GetActorRotation());
	}

	if (FragmentationVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), FragmentationVFX, DetonationPoint, GetActorRotation());
	}

	// Play explosion sound
	if (ExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), ExplosionSound, DetonationPoint);
	}

	// Stop dive whine
	if (DiveAudioComponent && DiveAudioComponent->IsPlaying())
	{
		DiveAudioComponent->Stop();
	}

	OnStrikeDetonation.Broadcast(this, DetonationPoint);

	SH_LOG(LogSH_Drone, Log, "FPV Strike detonated at %s", *DetonationPoint.ToString());

	// Destroy the drone actor
	DestroyDrone(nullptr);
}

// =======================================================================
//  Swarm interface
// =======================================================================

void ASHFPVStrikeDrone::JoinSwarm(int32 InSwarmID, int32 InSwarmIndex, int32 InSwarmSize)
{
	bIsSwarmMember = true;
	SwarmID = InSwarmID;
	SwarmIndex = InSwarmIndex;
	SwarmSize = InSwarmSize;
	SwarmAttackDelay = InSwarmIndex * SwarmStaggerSeconds;

	SH_LOG(LogSH_Drone, Log, "FPV Strike joined swarm %d (Index: %d/%d, Delay: %.1fs)",
		SwarmID, SwarmIndex, SwarmSize, SwarmAttackDelay);
}

// =======================================================================
//  Behavior tick
// =======================================================================

void ASHFPVStrikeDrone::TickDroneBehavior(float DeltaSeconds)
{
	if (bHasDetonated || GetDroneState() == ESHDroneState::Destroyed || GetDroneState() == ESHDroneState::Inactive)
	{
		return;
	}

	if (!bHasTarget)
	{
		return;
	}

	switch (CurrentStrikePhase)
	{
	case ESHStrikePhase::Transit:
		TickTransit(DeltaSeconds);
		break;
	case ESHStrikePhase::Seeking:
		TickSeeking(DeltaSeconds);
		break;
	case ESHStrikePhase::TerminalDive:
		TickTerminalDive(DeltaSeconds);
		break;
	default:
		break;
	}
}

void ASHFPVStrikeDrone::OnStateEntered(ESHDroneState NewState)
{
	Super::OnStateEntered(NewState);

	if (NewState == ESHDroneState::Attacking)
	{
		// Increase LED blink rate or color to red
		if (LEDLight)
		{
			LEDLight->SetLightColor(FLinearColor::Red);
			LEDLight->SetIntensity(1000.f);
		}
	}
}

void ASHFPVStrikeDrone::TickTransit(float DeltaSeconds)
{
	// Fly toward the general target area at cruise altitude
	FVector TargetPos = StrikeTarget.IsValid() ? StrikeTarget->GetActorLocation() : StaticTargetPosition;
	TargetPos.Z = CurrentCruiseAltitude;

	// Apply swarm offset during transit
	if (bIsSwarmMember)
	{
		TargetPos += ComputeSwarmOffset();
	}

	SetFlightTarget(TargetPos);

	const float DistToTarget = FVector::Dist(GetActorLocation(), TargetPos);

	// Transition to seeking when close enough
	if (DistToTarget < SeekingRange)
	{
		SetStrikePhase(ESHStrikePhase::Seeking);
	}
}

void ASHFPVStrikeDrone::TickSeeking(float DeltaSeconds)
{
	// Update target position (tracking a moving target)
	FVector TargetPos = StrikeTarget.IsValid() ? StrikeTarget->GetActorLocation() : StaticTargetPosition;

	// Apply evasive maneuvers during approach
	FVector EvasiveOffset = FVector::ZeroVector;
	if (bUseEvasiveManeuvers)
	{
		EvasiveOffset = ComputeEvasiveOffset(DeltaSeconds);
	}

	FVector SeekTarget = TargetPos;
	SeekTarget.Z = FMath::Max(TargetPos.Z + 1000.f, CurrentCruiseAltitude * 0.5f); // Descend somewhat
	SeekTarget += EvasiveOffset;

	SetFlightTarget(SeekTarget);

	// Check distance for terminal dive
	const float DistToTarget = FVector::Dist(GetActorLocation(), TargetPos);

	// Swarm members stagger their attack
	if (bIsSwarmMember && SwarmAttackDelay > 0.f)
	{
		SwarmAttackDelay -= DeltaSeconds;
		return; // Wait for stagger timer
	}

	if (DistToTarget < TerminalDiveRange)
	{
		BeginTerminalAttack();
	}
}

void ASHFPVStrikeDrone::TickTerminalDive(float DeltaSeconds)
{
	if (bHasDetonated)
	{
		return;
	}

	// Compute intercept position
	FVector InterceptPos = ComputeInterceptPosition();

	// Apply dive angle
	const FVector CurrentPos = GetActorLocation();
	FVector DiveDirection = (InterceptPos - CurrentPos).GetSafeNormal();

	// Bias the direction downward based on dive angle
	const float DiveRadians = FMath::DegreesToRadians(TerminalDiveAngle);
	DiveDirection.Z = FMath::Min(DiveDirection.Z, -FMath::Sin(DiveRadians));
	DiveDirection.Normalize();

	// Minor evasive weaving even during dive (reduced amplitude)
	if (bUseEvasiveManeuvers)
	{
		FVector Evasion = ComputeEvasiveOffset(DeltaSeconds) * 0.3f;
		DiveDirection = (DiveDirection * 10.f + Evasion.GetSafeNormal()).GetSafeNormal();
	}

	SetFlightTarget(CurrentPos + DiveDirection * MaxDiveSpeed);

	// Increase dive audio pitch as we get closer
	if (DiveAudioComponent && DiveAudioComponent->IsPlaying())
	{
		const float DistToTarget = FVector::Dist(CurrentPos, InterceptPos);
		const float PitchScale = FMath::Lerp(2.0f, 1.2f, FMath::Clamp(DistToTarget / TerminalDiveRange, 0.f, 1.f));
		DiveAudioComponent->SetPitchMultiplier(PitchScale);
	}

	// Check for impact
	if (CheckImpact())
	{
		Detonate();
	}
}

FVector ASHFPVStrikeDrone::ComputeInterceptPosition() const
{
	if (!StrikeTarget.IsValid())
	{
		return StaticTargetPosition;
	}

	const FVector TargetPos = StrikeTarget->GetActorLocation();
	const FVector TargetVel = StrikeTarget->GetVelocity();

	if (TargetVel.IsNearlyZero())
	{
		return TargetPos;
	}

	// Simple lead prediction
	const FVector DronePos = GetActorLocation();
	const float Distance = FVector::Dist(DronePos, TargetPos);
	const float TimeToTarget = Distance / FMath::Max(GetCurrentSpeed(), 1.f);

	return TargetPos + TargetVel * TimeToTarget * 0.8f; // Slight under-lead for realism
}

FVector ASHFPVStrikeDrone::ComputeEvasiveOffset(float DeltaSeconds) const
{
	const float Time = GetWorld()->GetTimeSeconds() + EvasivePhaseOffset;

	FVector Offset;
	Offset.X = FMath::Sin(Time * EvasiveFrequency * 2.f * PI) * EvasiveAmplitude;
	Offset.Y = FMath::Cos(Time * EvasiveFrequency * 2.f * PI * 0.7f) * EvasiveAmplitude;
	Offset.Z = FMath::Sin(Time * EvasiveFrequency * 2.f * PI * 1.3f) * EvasiveAmplitude * 0.3f;

	return Offset;
}

bool ASHFPVStrikeDrone::CheckImpact() const
{
	const FVector CurrentPos = GetActorLocation();
	const FVector Velocity = GetVelocity();

	if (Velocity.IsNearlyZero())
	{
		return false;
	}

	// Sweep check along velocity vector
	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.bTraceComplex = false;

	const FVector TraceEnd = CurrentPos + Velocity.GetSafeNormal() * 100.f;

	if (GetWorld()->LineTraceSingleByChannel(HitResult, CurrentPos, TraceEnd, ECC_WorldDynamic, QueryParams))
	{
		return true;
	}

	// Also check proximity to target
	if (StrikeTarget.IsValid())
	{
		const float DistToTarget = FVector::Dist(CurrentPos, StrikeTarget->GetActorLocation());
		if (DistToTarget < 150.f)
		{
			return true;
		}
	}

	// Check ground proximity
	FHitResult GroundHit;
	if (GetWorld()->LineTraceSingleByChannel(GroundHit, CurrentPos,
		CurrentPos - FVector(0.f, 0.f, 80.f), ECC_WorldStatic, QueryParams))
	{
		return true;
	}

	return false;
}

void ASHFPVStrikeDrone::ApplyExplosiveDamage(const FVector& DetonationPoint)
{
	TArray<AActor*> IgnoredActors;
	IgnoredActors.Add(this);

	// Radial damage
	UGameplayStatics::ApplyRadialDamageWithFalloff(
		GetWorld(),
		ExplosionDamage,
		ExplosionDamage * 0.2f, // Minimum damage at outer radius
		DetonationPoint,
		ExplosionInnerRadius,
		ExplosionOuterRadius,
		1.f, // Damage falloff exponent
		ExplosionDamageType ? ExplosionDamageType.Get() : UDamageType::StaticClass(),
		IgnoredActors,
		this, // Damage causer
		nullptr, // Instigator controller
		ECC_Visibility
	);

	// Apply bonus damage to the direct target if it's a vehicle
	if (StrikeTarget.IsValid() && StrikeTargetType == ESHStrikeTargetType::Vehicle)
	{
		const float DistToTarget = FVector::Dist(DetonationPoint, StrikeTarget->GetActorLocation());
		if (DistToTarget < ExplosionInnerRadius)
		{
			FDamageEvent DirectDamageEvent;
			StrikeTarget->TakeDamage(
				DirectDamage * AntiVehicleDamageMultiplier,
				DirectDamageEvent,
				nullptr,
				this);

			SH_LOG(LogSH_Drone, Log, "FPV Strike applied anti-vehicle bonus damage: %.1f",
				DirectDamage * AntiVehicleDamageMultiplier);
		}
	}
}

void ASHFPVStrikeDrone::ApplyFragmentationDamage(const FVector& DetonationPoint)
{
	if (FragmentCount <= 0)
	{
		return;
	}

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.bTraceComplex = false;
	QueryParams.bReturnPhysicalMaterial = true;

	for (int32 i = 0; i < FragmentCount; ++i)
	{
		// Generate random direction (biased slightly outward/downward like a real fragmentation pattern)
		FVector FragDir = FMath::VRand();
		FragDir.Z = FMath::Abs(FragDir.Z) * -0.3f + FragDir.Z * 0.7f; // Slight downward bias

		const FVector TraceEnd = DetonationPoint + FragDir * FragmentRange;

		FHitResult HitResult;
		if (GetWorld()->LineTraceSingleByChannel(HitResult, DetonationPoint, TraceEnd, ECC_Visibility, QueryParams))
		{
			AActor* HitActor = HitResult.GetActor();
			if (HitActor && HitActor != this)
			{
				// Distance-based falloff for fragments
				const float FragDist = HitResult.Distance;
				const float Falloff = 1.f - FMath::Clamp(FragDist / FragmentRange, 0.f, 1.f);
				const float FinalFragDamage = FragmentDamage * Falloff;

				FPointDamageEvent FragDamageEvent;
				FragDamageEvent.Damage = FinalFragDamage;
				FragDamageEvent.HitInfo = HitResult;
				FragDamageEvent.ShotDirection = FragDir;

				HitActor->TakeDamage(FinalFragDamage, FragDamageEvent, nullptr, this);
			}
		}
	}
}

void ASHFPVStrikeDrone::SetStrikePhase(ESHStrikePhase NewPhase)
{
	if (NewPhase == CurrentStrikePhase)
	{
		return;
	}

	const ESHStrikePhase OldPhase = CurrentStrikePhase;
	CurrentStrikePhase = NewPhase;
	OnStrikePhaseChanged.Broadcast(OldPhase, NewPhase);

	SH_LOG(LogSH_Drone, Log, "FPV Strike phase: %d -> %d", static_cast<int32>(OldPhase), static_cast<int32>(NewPhase));
}

FVector ASHFPVStrikeDrone::ComputeSwarmOffset() const
{
	if (!bIsSwarmMember || SwarmSize <= 1)
	{
		return FVector::ZeroVector;
	}

	// Distribute swarm members in a circle around the primary flight path
	const float AngleStep = 2.f * PI / static_cast<float>(SwarmSize);
	const float Angle = AngleStep * SwarmIndex;

	return FVector(
		FMath::Cos(Angle) * SwarmSpacing,
		FMath::Sin(Angle) * SwarmSpacing,
		(SwarmIndex % 2 == 0 ? 1.f : -1.f) * SwarmSpacing * 0.3f
	);
}
