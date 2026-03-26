// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHMeleeSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

USHMeleeSystem::USHMeleeSystem()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Default punch data.
	PunchData.Type = ESHMeleeType::Punch;
	PunchData.Damage = 35.0f;
	PunchData.Range = 150.0f;
	PunchData.TraceRadius = 25.0f;
	PunchData.WindupTime = 0.12f;
	PunchData.RecoveryTime = 0.35f;
	PunchData.bInstantKillFromBehind = false;
	PunchData.StaminaCost = 0.08f;

	// Default knife data — lethal from behind.
	KnifeData.Type = ESHMeleeType::Knife;
	KnifeData.Damage = 75.0f;
	KnifeData.Range = 120.0f;
	KnifeData.TraceRadius = 15.0f;
	KnifeData.WindupTime = 0.18f;
	KnifeData.RecoveryTime = 0.5f;
	KnifeData.bInstantKillFromBehind = true;
	KnifeData.StaminaCost = 0.12f;

	// Takedown — requires behind approach, always lethal, slow.
	TakedownData.Type = ESHMeleeType::Takedown;
	TakedownData.Damage = 999.0f;
	TakedownData.Range = 100.0f;
	TakedownData.TraceRadius = 30.0f;
	TakedownData.WindupTime = 0.6f;
	TakedownData.RecoveryTime = 1.2f;
	TakedownData.bInstantKillFromBehind = true;
	TakedownData.StaminaCost = 0.2f;

	// Bayonet — longer range, moderate damage.
	BayonetData.Type = ESHMeleeType::Bayonet;
	BayonetData.Damage = 90.0f;
	BayonetData.Range = 200.0f;
	BayonetData.TraceRadius = 20.0f;
	BayonetData.WindupTime = 0.25f;
	BayonetData.RecoveryTime = 0.7f;
	BayonetData.bInstantKillFromBehind = true;
	BayonetData.StaminaCost = 0.15f;
}

void USHMeleeSystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsAttacking)
	{
		return;
	}

	// Windup phase.
	if (!bDamageApplied)
	{
		AttackTimer += DeltaTime;
		if (AttackTimer >= ActiveAttack.WindupTime)
		{
			// Damage frame — execute the trace.
			FSHMeleeResult Result = ExecuteMeleeTrace(ActiveAttack);
			bDamageApplied = true;

			if (Result.bHit)
			{
				OnMeleeHit.Broadcast(Result);
			}
		}
	}
	else
	{
		// Recovery phase.
		RecoveryTimer += DeltaTime;
		if (RecoveryTimer >= ActiveAttack.RecoveryTime)
		{
			bIsAttacking = false;
			bDamageApplied = false;
			AttackTimer = 0.0f;
			RecoveryTimer = 0.0f;
			OnMeleeRecovered.Broadcast();
		}
	}
}

void USHMeleeSystem::StartMeleeAttack(ESHMeleeType Type)
{
	if (!CanAttack())
	{
		return;
	}

	ActiveAttack = GetAttackData(Type);
	bIsAttacking = true;
	bDamageApplied = false;
	AttackTimer = 0.0f;
	RecoveryTimer = 0.0f;

	OnMeleeStarted.Broadcast();
}

void USHMeleeSystem::CancelAttack()
{
	if (bIsAttacking && !bDamageApplied)
	{
		bIsAttacking = false;
		AttackTimer = 0.0f;
	}
}

bool USHMeleeSystem::CanAttack() const
{
	return !bIsAttacking;
}

FSHMeleeResult USHMeleeSystem::ExecuteMeleeTrace(const FSHMeleeAttackData& Data)
{
	FSHMeleeResult Result;

	AActor* Owner = GetOwner();
	if (!Owner) return Result;

	const UWorld* World = GetWorld();
	if (!World) return Result;

	// Trace from the owner's eye location in the forward direction.
	FVector EyeLocation;
	FRotator EyeRotation;
	if (const APawn* Pawn = Cast<APawn>(Owner))
	{
		Pawn->GetActorEyesViewPoint(EyeLocation, EyeRotation);
	}
	else
	{
		EyeLocation = Owner->GetActorLocation();
		EyeRotation = Owner->GetActorRotation();
	}

	const FVector TraceEnd = EyeLocation + EyeRotation.Vector() * Data.Range;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(MeleeTrace), false);
	Params.AddIgnoredActor(Owner);

	const bool bHit = GetWorld()->SweepSingleByChannel(
		Hit, EyeLocation, TraceEnd, FQuat::Identity,
		ECC_Pawn, FCollisionShape::MakeSphere(Data.TraceRadius), Params);

	if (!bHit || !Hit.GetActor())
	{
		return Result;
	}

	Result.bHit = true;
	Result.HitActor = Hit.GetActor();
	Result.HitLocation = Hit.ImpactPoint;
	Result.HitNormal = Hit.ImpactNormal;

	// Determine direction of attack.
	const ESHMeleeDirection Direction = ComputeAttackDirection(Hit.GetActor());
	Result.bFromBehind = (Direction == ESHMeleeDirection::Behind);

	// Calculate damage.
	float FinalDamage = Data.Damage;
	if (Result.bFromBehind && Data.bInstantKillFromBehind)
	{
		FinalDamage = 999.0f; // Instant kill.
		Result.bKill = true;
	}

	Result.DamageDealt = FinalDamage;

	// Apply damage.
	FPointDamageEvent DmgEvent(FinalDamage, Hit, EyeRotation.Vector(), UDamageType::StaticClass());

	AController* InstigatorCtrl = nullptr;
	if (const APawn* OwnerPawn = Cast<APawn>(Owner))
	{
		InstigatorCtrl = OwnerPawn->GetController();
	}

	Hit.GetActor()->TakeDamage(FinalDamage, DmgEvent, InstigatorCtrl, Owner);

	return Result;
}

ESHMeleeDirection USHMeleeSystem::ComputeAttackDirection(const AActor* Target) const
{
	if (!Target || !GetOwner()) return ESHMeleeDirection::Front;

	const FVector ToTarget = (Target->GetActorLocation() - GetOwner()->GetActorLocation()).GetSafeNormal2D();
	const FVector TargetForward = Target->GetActorForwardVector().GetSafeNormal2D();

	// Dot product: positive = attacker is in front of target, negative = behind
	const float Dot = FVector::DotProduct(TargetForward, ToTarget);

	if (Dot > 0.5f)
	{
		// Attacker is behind the target (target is facing away).
		return ESHMeleeDirection::Behind;
	}
	else if (Dot < -0.5f)
	{
		return ESHMeleeDirection::Front;
	}
	else
	{
		// Side attack — check cross product for left vs right.
		const FVector Cross = FVector::CrossProduct(TargetForward, ToTarget);
		return Cross.Z > 0 ? ESHMeleeDirection::Right : ESHMeleeDirection::Left;
	}
}

const FSHMeleeAttackData& USHMeleeSystem::GetAttackData(ESHMeleeType Type) const
{
	switch (Type)
	{
	case ESHMeleeType::Knife:    return KnifeData;
	case ESHMeleeType::Takedown: return TakedownData;
	case ESHMeleeType::Bayonet:  return BayonetData;
	default:                     return PunchData;
	}
}
