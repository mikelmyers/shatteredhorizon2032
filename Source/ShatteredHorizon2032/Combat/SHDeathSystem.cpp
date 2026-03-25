// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHDeathSystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/World.h"
#include "PhysicsEngine/PhysicsSettings.h"

// =====================================================================
//  Constructor
// =====================================================================

USHDeathSystem::USHDeathSystem()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false; // Only tick after death (during stabilization).
	PrimaryComponentTick.TickGroup = TG_PostPhysics;    // Read physics results after sim step.
}

// =====================================================================
//  UActorComponent overrides
// =====================================================================

void USHDeathSystem::BeginPlay()
{
	Super::BeginPlay();

	InitDefaultZoneImpulseConfigs();

	// Cache the skeletal mesh early.
	CachedMesh = GetOwnerMesh();
}

void USHDeathSystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsDead || bRagdollStabilized)
	{
		return;
	}

	TickStabilizationCheck(DeltaTime);
}

// =====================================================================
//  Default configuration
// =====================================================================

void USHDeathSystem::InitDefaultZoneImpulseConfigs()
{
	if (ZoneImpulseConfigs.Num() > 0)
	{
		return;
	}

	// Head: strong snap-back, slight upward bias for whiplash effect.
	{
		FSHDeathImpulseConfig Config;
		Config.ImpulseScale = 2.5f;
		Config.UpwardBias = 200.f;
		ZoneImpulseConfigs.Add(ESHHitZone::Head, Config);
	}

	// Torso: moderate impulse, center-mass crumple.
	{
		FSHDeathImpulseConfig Config;
		Config.ImpulseScale = 1.5f;
		Config.UpwardBias = 0.f;
		ZoneImpulseConfigs.Add(ESHHitZone::Torso, Config);
	}

	// Arms: low impulse, mostly carried by torso response.
	{
		FSHDeathImpulseConfig ArmConfig;
		ArmConfig.ImpulseScale = 0.8f;
		ArmConfig.UpwardBias = 0.f;
		ZoneImpulseConfigs.Add(ESHHitZone::LeftArm, ArmConfig);
		ZoneImpulseConfigs.Add(ESHHitZone::RightArm, ArmConfig);
	}

	// Legs: moderate impulse, collapse forward.
	{
		FSHDeathImpulseConfig LegConfig;
		LegConfig.ImpulseScale = 1.2f;
		LegConfig.UpwardBias = 0.f;
		ZoneImpulseConfigs.Add(ESHHitZone::LeftLeg, LegConfig);
		ZoneImpulseConfigs.Add(ESHHitZone::RightLeg, LegConfig);
	}
}

// =====================================================================
//  Projectile momentum storage
// =====================================================================

void USHDeathSystem::SetProjectileMomentum(const FVector& Velocity, float MassGrams)
{
	StoredProjectileVelocity = Velocity;
	StoredProjectileMassGrams = FMath::Max(MassGrams, 0.f);
	bHasProjectileMomentum = true;
}

// =====================================================================
//  Death execution
// =====================================================================

void USHDeathSystem::ExecuteDeath(const FSHDamageInfo& KillingBlow)
{
	if (bIsDead)
	{
		return; // Already dead; do not re-execute.
	}

	bIsDead = true;

	// Build the full death context.
	DeathContext = BuildDeathContext(KillingBlow);

	// Broadcast death start.
	OnDeathStarted.Broadcast(DeathContext);

	// Zero-blend-time ragdoll activation.
	ActivateRagdoll();

	// Apply projectile momentum impulse.
	ApplyDeathImpulse();

	// Apply contextual forces (head snap, leg collapse, etc.).
	ApplyContextualForces();

	// Enforce ground contact to prevent floating.
	EnforceGroundContact();

	// Enable tick for stabilization monitoring.
	SetComponentTickEnabled(true);

	// Clear stored momentum for next use.
	bHasProjectileMomentum = false;
	StoredProjectileVelocity = FVector::ZeroVector;
	StoredProjectileMassGrams = 0.f;
}

FSHDeathContext USHDeathSystem::BuildDeathContext(const FSHDamageInfo& KillingBlow) const
{
	FSHDeathContext Context;
	Context.KillingBlowZone = KillingBlow.HitZone;
	Context.KillingBlowDamageType = KillingBlow.DamageType;
	Context.DamageDirection = KillingBlow.DamageDirection.GetSafeNormal();
	Context.ImpactLocation = KillingBlow.ImpactLocation;
	Context.BaseDamage = KillingBlow.BaseDamage;

	if (bHasProjectileMomentum)
	{
		Context.ProjectileVelocity = StoredProjectileVelocity;
		Context.ProjectileMassGrams = StoredProjectileMassGrams;
	}

	// Determine the response type. Explosive/fragmentation damage overrides zone-based logic.
	if (KillingBlow.DamageType == ESHDamageType::Explosive ||
		KillingBlow.DamageType == ESHDamageType::Fragmentation)
	{
		Context.ResponseType = ESHDeathResponseType::ExplosiveThrow;
	}
	else
	{
		Context.ResponseType = GetDeathResponseType(KillingBlow.HitZone);
	}

	if (const UWorld* World = GetWorld())
	{
		Context.DeathTime = World->GetTimeSeconds();
	}

	return Context;
}

// =====================================================================
//  Death response type
// =====================================================================

ESHDeathResponseType USHDeathSystem::GetDeathResponseType(ESHHitZone Zone) const
{
	switch (Zone)
	{
	case ESHHitZone::Head:
		return ESHDeathResponseType::HeadSnap;

	case ESHHitZone::Torso:
		return ESHDeathResponseType::TorsoSlump;

	case ESHHitZone::LeftLeg:
	case ESHHitZone::RightLeg:
		return ESHDeathResponseType::LegCollapse;

	case ESHHitZone::LeftArm:
	case ESHHitZone::RightArm:
		// Arm hits produce a torso slump since the arm alone doesn't drive a distinctive response.
		return ESHDeathResponseType::TorsoSlump;

	default:
		return ESHDeathResponseType::GenericFall;
	}
}

// =====================================================================
//  Ragdoll activation
// =====================================================================

void USHDeathSystem::ActivateRagdoll()
{
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter)
	{
		return;
	}

	// Disable character movement immediately.
	if (UCharacterMovementComponent* Movement = OwnerCharacter->GetCharacterMovement())
	{
		Movement->DisableMovement();
		Movement->StopMovementImmediately();
	}

	// Disable capsule collision to let ragdoll physics take over.
	if (UCapsuleComponent* Capsule = GetOwnerCapsule())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	}

	// Get the mesh component.
	USkeletalMeshComponent* Mesh = GetOwnerMesh();
	if (!Mesh)
	{
		return;
	}

	// Stop all active montages immediately (zero blend time).
	if (UAnimInstance* AnimInstance = Mesh->GetAnimInstance())
	{
		AnimInstance->StopAllMontages(0.f);
	}

	// Zero-blend-time ragdoll activation.
	// Disable animation evaluation entirely to prevent any blend artifacts.
	Mesh->SetAnimationMode(EAnimationMode::AnimationCustomMode);
	Mesh->bNoSkeletonUpdate = false;
	Mesh->bPauseAnims = true;

	// Enable physics simulation on all bodies.
	Mesh->SetAllBodiesSimulatePhysics(true);
	Mesh->SetSimulatePhysics(true);
	Mesh->WakeAllRigidBodies();

	// Set collision to ragdoll profile: query and physics, overlap with world.
	Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Mesh->SetCollisionObjectType(ECC_PhysicsBody);
	Mesh->SetCollisionResponseToAllChannels(ECR_Block);
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);

	// Enable gravity.
	Mesh->SetEnableGravity(true);

	// Detach from any parent attachment to avoid constraint issues.
	OwnerCharacter->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);

	CachedMesh = Mesh;
}

// =====================================================================
//  Death impulse (momentum transfer)
// =====================================================================

void USHDeathSystem::ApplyDeathImpulse()
{
	USkeletalMeshComponent* Mesh = GetOwnerMesh();
	if (!Mesh || !Mesh->IsSimulatingPhysics())
	{
		return;
	}

	// Compute base impulse from projectile momentum (p = m * v).
	FVector Impulse = FVector::ZeroVector;

	if (bHasProjectileMomentum && StoredProjectileMassGrams > 0.f)
	{
		// Momentum in g*cm/s, scaled to Unreal impulse.
		Impulse = StoredProjectileVelocity * StoredProjectileMassGrams * MomentumToImpulseScale;
	}
	else if (!DeathContext.DamageDirection.IsNearlyZero())
	{
		// Fallback: use damage direction with a baseline impulse proportional to damage.
		Impulse = DeathContext.DamageDirection * DeathContext.BaseDamage * 50.f;
	}

	if (Impulse.IsNearlyZero())
	{
		return;
	}

	// Apply zone-specific scaling.
	const FSHDeathImpulseConfig* ZoneConfig = ZoneImpulseConfigs.Find(DeathContext.KillingBlowZone);
	float ZoneScale = ZoneConfig ? ZoneConfig->ImpulseScale : 1.f;
	float UpwardBias = ZoneConfig ? ZoneConfig->UpwardBias : 0.f;

	Impulse *= ZoneScale * GlobalImpulseScale;

	// Add upward bias.
	Impulse.Z += UpwardBias;

	// Clamp impulse magnitude.
	const float Magnitude = Impulse.Size();
	if (Magnitude > MaxImpulseMagnitude)
	{
		Impulse = Impulse.GetSafeNormal() * MaxImpulseMagnitude;
	}

	// Determine target bone for impulse application.
	const FName TargetBone = GetBoneForZone(DeathContext.KillingBlowZone);

	// Apply impulse at the impact location on the target bone.
	if (!DeathContext.ImpactLocation.IsNearlyZero())
	{
		Mesh->AddImpulseAtLocation(Impulse, DeathContext.ImpactLocation, TargetBone);
	}
	else
	{
		Mesh->AddImpulse(Impulse, TargetBone, false);
	}
}

// =====================================================================
//  Contextual death forces
// =====================================================================

void USHDeathSystem::ApplyContextualForces()
{
	USkeletalMeshComponent* Mesh = GetOwnerMesh();
	if (!Mesh || !Mesh->IsSimulatingPhysics())
	{
		return;
	}

	switch (DeathContext.ResponseType)
	{
	case ESHDeathResponseType::HeadSnap:
	{
		// Apply a sharp impulse to the head bone opposite to the damage direction
		// to simulate the snap-back whiplash effect.
		const FVector SnapDirection = -DeathContext.DamageDirection.GetSafeNormal();
		const FVector HeadImpulse = (SnapDirection + FVector(0.f, 0.f, 0.3f)) * 3000.f;
		Mesh->AddImpulse(HeadImpulse, HeadBoneName, false);
		break;
	}

	case ESHDeathResponseType::TorsoSlump:
	{
		// Crumple: weaken upper body, let gravity pull the torso forward/down.
		// Apply a forward-downward impulse to spine.
		const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
		FVector ForwardDir = OwnerCharacter
			? OwnerCharacter->GetActorForwardVector()
			: FVector::ForwardVector;

		const FVector SlumpImpulse = (ForwardDir * 0.3f + FVector(0.f, 0.f, -1.f)) * 1500.f;
		Mesh->AddImpulse(SlumpImpulse, SpineBoneName, false);

		// Slight knee buckle via pelvis.
		Mesh->AddImpulse(FVector(0.f, 0.f, -800.f), PelvisBoneName, false);
		break;
	}

	case ESHDeathResponseType::LegCollapse:
	{
		// Collapse forward: remove support from the hit leg, push torso forward.
		const FName HitLegBone = (DeathContext.KillingBlowZone == ESHHitZone::LeftLeg)
			? LeftThighBoneName
			: RightThighBoneName;

		// Buckle the hit leg backward.
		const FVector LegBuckle = FVector(0.f, 0.f, -1200.f);
		Mesh->AddImpulse(LegBuckle, HitLegBone, false);

		// Push the torso forward to create a collapsing motion.
		const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
		FVector ForwardDir = OwnerCharacter
			? OwnerCharacter->GetActorForwardVector()
			: FVector::ForwardVector;

		Mesh->AddImpulse(ForwardDir * 2000.f + FVector(0.f, 0.f, -500.f), SpineBoneName, false);
		break;
	}

	case ESHDeathResponseType::ExplosiveThrow:
	{
		// Explosive: strong radial throw from the impact location.
		// The main impulse is already applied by ApplyDeathImpulse.
		// Add additional lift and spread to all major bones.
		const FVector UpAndOut = FVector(0.f, 0.f, 1.f) * 4000.f;
		Mesh->AddImpulse(UpAndOut, PelvisBoneName, false);

		// Spin the torso for a tumbling effect.
		const FVector SpinTorque = FVector(
			FMath::FRandRange(-2000.f, 2000.f),
			FMath::FRandRange(-2000.f, 2000.f),
			FMath::FRandRange(-1000.f, 1000.f));
		Mesh->AddAngularImpulseInDegrees(SpinTorque, SpineBoneName, false);
		break;
	}

	case ESHDeathResponseType::GenericFall:
	default:
	{
		// Simple forward/sideways fall with slight randomization.
		const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
		FVector ForwardDir = OwnerCharacter
			? OwnerCharacter->GetActorForwardVector()
			: FVector::ForwardVector;
		FVector RightDir = OwnerCharacter
			? OwnerCharacter->GetActorRightVector()
			: FVector::RightVector;

		const float LateralBias = FMath::FRandRange(-0.4f, 0.4f);
		const FVector FallImpulse = (ForwardDir * 0.5f + RightDir * LateralBias + FVector(0.f, 0.f, -0.3f)) * 1500.f;
		Mesh->AddImpulse(FallImpulse, PelvisBoneName, false);
		break;
	}
	}
}

// =====================================================================
//  Ground enforcement
// =====================================================================

void USHDeathSystem::EnforceGroundContact()
{
	USkeletalMeshComponent* Mesh = GetOwnerMesh();
	if (!Mesh || !Mesh->IsSimulatingPhysics())
	{
		return;
	}

	// Apply a downward impulse to the pelvis to ensure the body doesn't float.
	const FVector DownwardImpulse = FVector(0.f, 0.f, -AntiFloatDownwardImpulse);
	Mesh->AddImpulse(DownwardImpulse, PelvisBoneName, false);
}

// =====================================================================
//  Stabilization
// =====================================================================

void USHDeathSystem::TickStabilizationCheck(float DeltaTime)
{
	TimeSinceDeath += DeltaTime;

	// Force stabilization after timeout.
	if (TimeSinceDeath >= StabilizationTimeout)
	{
		StabilizeRagdoll();
		return;
	}

	USkeletalMeshComponent* Mesh = GetOwnerMesh();
	if (!Mesh)
	{
		StabilizeRagdoll();
		return;
	}

	// Sample velocity across all physics bodies.
	float MaxBodyVelocity = 0.f;
	const TArray<FBodyInstance*>& Bodies = Mesh->Bodies;

	for (const FBodyInstance* Body : Bodies)
	{
		if (Body && Body->IsInstanceSimulatingPhysics())
		{
			const float BodySpeed = Body->GetUnrealWorldVelocity().Size();
			MaxBodyVelocity = FMath::Max(MaxBodyVelocity, BodySpeed);
		}
	}

	if (MaxBodyVelocity < SettleVelocityThreshold)
	{
		SettleTimer += DeltaTime;

		if (SettleTimer >= SettleDurationRequired)
		{
			ValidateBodyState();
			StabilizeRagdoll();
		}
	}
	else
	{
		// Reset settle timer if any body is still moving.
		SettleTimer = 0.f;
	}
}

void USHDeathSystem::StabilizeRagdoll()
{
	if (bRagdollStabilized)
	{
		return;
	}

	bRagdollStabilized = true;

	USkeletalMeshComponent* Mesh = GetOwnerMesh();
	if (Mesh)
	{
		// Freeze all physics bodies to prevent jitter.
		Mesh->PutAllRigidBodiesToSleep();
		Mesh->SetAllBodiesSimulatePhysics(false);

		// Set collision to block-all for persistent world interaction.
		// Bodies should remain as static collision for other actors.
		Mesh->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		Mesh->SetCollisionObjectType(ECC_WorldStatic);
		Mesh->SetCollisionResponseToAllChannels(ECR_Block);
		Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Block);
	}

	// Disable tick — no further processing needed.
	SetComponentTickEnabled(false);

	// Broadcast completion.
	OnDeathComplete.Broadcast(DeathContext);
}

void USHDeathSystem::ValidateBodyState()
{
	USkeletalMeshComponent* Mesh = GetOwnerMesh();
	if (!Mesh)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Check pelvis height against ground to detect floating bodies.
	const FVector PelvisLocation = Mesh->GetBoneLocation(PelvisBoneName);

	FHitResult GroundHit;
	const FVector TraceStart = PelvisLocation;
	const FVector TraceEnd = PelvisLocation - FVector(0.f, 0.f, GroundCheckDistance);

	FCollisionQueryParams TraceParams;
	TraceParams.AddIgnoredActor(GetOwner());
	TraceParams.bTraceComplex = false;

	const bool bHitGround = World->LineTraceSingleByChannel(
		GroundHit,
		TraceStart,
		TraceEnd,
		ECC_Visibility,
		TraceParams);

	if (!bHitGround)
	{
		// Body is floating — apply strong downward force to bring it to ground.
		// Re-enable physics briefly, apply force, then let stabilization re-trigger.
		Mesh->SetAllBodiesSimulatePhysics(true);
		Mesh->WakeAllRigidBodies();
		Mesh->AddImpulse(FVector(0.f, 0.f, -5000.f), PelvisBoneName, false);

		// Reset settle timer so we re-check after the correction.
		SettleTimer = 0.f;
		bRagdollStabilized = false;
		return;
	}

	// Check for wall-leaning: if the pelvis is significantly higher than
	// the nearest ground contact, the body may be propped against a wall.
	const float HeightAboveGround = PelvisLocation.Z - GroundHit.ImpactPoint.Z;

	// A standing-height pelvis (~90cm) suggests the body is propped up.
	// Typical ragdoll pelvis should be well below 60cm from ground.
	constexpr float MaxAcceptablePelvisHeight = 70.f;

	if (HeightAboveGround > MaxAcceptablePelvisHeight)
	{
		// Push the body away from whatever it's leaning against.
		// Use the ground normal to determine a push direction.
		const FVector PushDirection = (GroundHit.ImpactNormal * 0.3f + FVector(0.f, 0.f, -1.f)).GetSafeNormal();
		Mesh->SetAllBodiesSimulatePhysics(true);
		Mesh->WakeAllRigidBodies();
		Mesh->AddImpulse(PushDirection * 3000.f, PelvisBoneName, false);

		SettleTimer = 0.f;
		bRagdollStabilized = false;
	}
}

// =====================================================================
//  Helpers
// =====================================================================

USkeletalMeshComponent* USHDeathSystem::GetOwnerMesh() const
{
	if (CachedMesh.Get())
	{
		return CachedMesh.Get();
	}

	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (OwnerCharacter)
	{
		return OwnerCharacter->GetMesh();
	}

	return nullptr;
}

UCapsuleComponent* USHDeathSystem::GetOwnerCapsule() const
{
	const ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (OwnerCharacter)
	{
		return OwnerCharacter->GetCapsuleComponent();
	}

	return nullptr;
}

FName USHDeathSystem::GetBoneForZone(ESHHitZone Zone) const
{
	switch (Zone)
	{
	case ESHHitZone::Head:
		return HeadBoneName;

	case ESHHitZone::Torso:
		return SpineBoneName;

	case ESHHitZone::LeftArm:
		return FName(TEXT("upperarm_l"));

	case ESHHitZone::RightArm:
		return FName(TEXT("upperarm_r"));

	case ESHHitZone::LeftLeg:
		return LeftThighBoneName;

	case ESHHitZone::RightLeg:
		return RightThighBoneName;

	default:
		return PelvisBoneName;
	}
}
