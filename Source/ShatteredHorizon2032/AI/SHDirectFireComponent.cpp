// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "AI/SHDirectFireComponent.h"
#include "AI/SHEnemyCharacter.h"
#include "AI/SHEnemyAIController.h"
#include "Core/SHPlayerCharacter.h"
#include "Squad/SHSquadMember.h"
#include "Engine/World.h"
#include "AIController.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "EngineUtils.h"

USHDirectFireComponent::USHDirectFireComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f;

	// Sensible content defaults for M01; all overridable via DefaultGame.ini.
	FireSound = TSoftObjectPtr<USoundBase>(FSoftObjectPath(
		TEXT("/Game/FreeGunSounds/Wav/Rifle/WAV_Rifle_shot02.WAV_Rifle_shot02")));
	FireAttenuation = TSoftObjectPtr<USoundAttenuation>(FSoftObjectPath(
		TEXT("/Game/SH/Audio/ATT_Gunfire.ATT_Gunfire")));
	MuzzleFlash = TSoftObjectPtr<UParticleSystem>(FSoftObjectPath(
		TEXT("/Game/Interaction/Effects/MuzzleFlash/P_MF_Handgun.P_MF_Handgun")));
}

void USHDirectFireComponent::BeginPlay()
{
	Super::BeginPlay();
	RoundsLeftInMag = MagazineRounds;
	// Stagger first engagement so squads don't fire in unison.
	NextBurstTime = GetWorld()->GetTimeSeconds() + FMath::FRandRange(1.f, 4.f);
}

bool USHDirectFireComponent::IsOwnerCombatCapable() const
{
	if (const ASHEnemyCharacter* Enemy = Cast<ASHEnemyCharacter>(GetOwner()))
	{
		return Enemy->IsAlive();
	}
	if (const ASHSquadMember* Member = Cast<ASHSquadMember>(GetOwner()))
	{
		return Member->CurrentHealth > 0.f;
	}
	return true;
}

void USHDirectFireComponent::AcquireTarget()
{
	UWorld* World = GetWorld();
	AActor* Best = nullptr;
	float BestDistSq = MaxRangeCm * MaxRangeCm;
	const FVector OurPos = GetOwner()->GetActorLocation();

	if (bTargetsPlayerSide)
	{
		// Player + squad members
		if (ACharacter* Player = UGameplayStatics::GetPlayerCharacter(World, 0))
		{
			if (ASHPlayerCharacter* SHPlayer = Cast<ASHPlayerCharacter>(Player))
			{
				if (SHPlayer->IsAlive())
				{
					const float D = FVector::DistSquared(OurPos, SHPlayer->GetActorLocation());
					if (D < BestDistSq) { BestDistSq = D; Best = SHPlayer; }
				}
			}
		}
		for (TActorIterator<ASHSquadMember> It(World); It; ++It)
		{
			if (It->CurrentHealth <= 0.f) continue;
			const float D = FVector::DistSquared(OurPos, It->GetActorLocation());
			if (D < BestDistSq) { BestDistSq = D; Best = *It; }
		}
	}
	else
	{
		for (TActorIterator<ASHEnemyCharacter> It(World); It; ++It)
		{
			if (!It->IsAlive()) continue;
			const float D = FVector::DistSquared(OurPos, It->GetActorLocation());
			if (D < BestDistSq) { BestDistSq = D; Best = *It; }
		}
	}

	CurrentTarget = Best;
}

bool USHDirectFireComponent::HasLineOfSight(AActor* Target, FVector& OutAimPoint) const
{
	const ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	if (!OwnerChar || !Target) return false;

	const FVector Eye = OwnerChar->GetActorLocation() + FVector(0, 0, 60.f);
	OutAimPoint = Target->GetActorLocation() + FVector(0, 0, 20.f);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(SHDirectFireLOS), true, GetOwner());
	Params.AddIgnoredActor(Target);
	const bool bBlocked = GetWorld()->LineTraceSingleByChannel(
		Hit, Eye, OutAimPoint, ECC_Visibility, Params);
	return !bBlocked;
}

FVector USHDirectFireComponent::GetMuzzleLocation() const
{
	const ACharacter* OwnerChar = Cast<ACharacter>(GetOwner());
	if (!OwnerChar) return GetOwner()->GetActorLocation();

	// Prefer the spawned weapon visual if present.
	TArray<UActorComponent*> Comps;
	GetOwner()->GetComponents(UStaticMeshComponent::StaticClass(), Comps);
	for (UActorComponent* C : Comps)
	{
		if (C->GetName().Contains(TEXT("WeaponVisual")))
		{
			return Cast<UStaticMeshComponent>(C)->GetComponentLocation()
				+ OwnerChar->GetActorForwardVector() * 40.f;
		}
	}
	return OwnerChar->GetActorLocation()
		+ OwnerChar->GetActorForwardVector() * 50.f + FVector(0, 0, 50.f);
}

void USHDirectFireComponent::FireRound()
{
	AActor* Target = CurrentTarget.Get();
	if (!Target) return;

	const FVector Muzzle = GetMuzzleLocation();
	const FVector TargetPos = Target->GetActorLocation();
	const float Dist = FVector::Dist(Muzzle, TargetPos);

	// Hit probability: falls off with range.
	const float RangeFactor = FMath::GetMappedRangeValueClamped(
		FVector2D(2000.f, MaxRangeCm), FVector2D(1.f, 0.18f), Dist);

	// Cognitive state degrades accuracy: a stressed/suppressed soldier (low
	// Astraea combat effectiveness) misses more. Only enemy AI carries an
	// Astraea model — friendly squad falls back to 1.0 (unmodified).
	float Effectiveness = 1.f;
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		if (const ASHEnemyAIController* AICon = Cast<ASHEnemyAIController>(OwnerPawn->GetController()))
		{
			Effectiveness = AICon->GetCombatEffectiveness();
		}
	}

	const bool bHit = FMath::FRand() < BaseHitChance * RangeFactor * Effectiveness;

	if (bHit)
	{
		const FVector ShotDir = (TargetPos - Muzzle).GetSafeNormal();
		FHitResult HitInfo(1.f);
		HitInfo.ImpactPoint = TargetPos;
		HitInfo.Location = TargetPos;
		AController* InstigatorController = nullptr;
		if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
		{
			InstigatorController = OwnerPawn->GetController();
		}
		UGameplayStatics::ApplyPointDamage(Target, DamagePerRound, ShotDir, HitInfo,
			InstigatorController, GetOwner(), nullptr);
	}
	else if (ASHPlayerCharacter* Player = Cast<ASHPlayerCharacter>(Target))
	{
		Player->AddSuppression(SuppressionPerNearMiss);
	}

	// Feedback
	if (USoundBase* Sound = FireSound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), Sound, Muzzle, 1.f, 1.f, 0.f,
			FireAttenuation.LoadSynchronous());
	}
	if (UParticleSystem* Flash = MuzzleFlash.LoadSynchronous())
	{
		UGameplayStatics::SpawnEmitterAtLocation(GetWorld(), Flash, Muzzle,
			(CurrentTarget.IsValid()
				? (TargetPos - Muzzle).Rotation() : GetOwner()->GetActorRotation()),
			FVector(1.2f));
	}
}

void USHDirectFireComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bEnabled || !GetOwner() || !IsOwnerCombatCapable())
	{
		return;
	}

	UWorld* World = GetWorld();
	const float Now = World->GetTimeSeconds();

	if (Now >= NextTargetScanTime)
	{
		NextTargetScanTime = Now + 0.8f;
		AcquireTarget();
	}

	AActor* Target = CurrentTarget.Get();
	if (!Target) return;

	FVector AimPoint;
	if (!HasLineOfSight(Target, AimPoint))
	{
		RoundsLeftInBurst = 0;
		// No LOS — close the distance (fallback assault movement when no
		// behavior tree / director drives this pawn).
		if (bAdvanceWhenNoLOS && Now >= NextRepathTime)
		{
			NextRepathTime = Now + AdvanceRepathInterval * FMath::FRandRange(0.8f, 1.3f);
			if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
			{
				if (AAIController* AI = Cast<AAIController>(OwnerPawn->GetController()))
				{
					const float Dist = FVector::Dist(OwnerPawn->GetActorLocation(),
						Target->GetActorLocation());
					if (Dist > AdvanceAcceptanceRadius)
					{
						AI->MoveToLocation(Target->GetActorLocation(),
							AdvanceAcceptanceRadius * 0.8f, true, true, false, true);
					}
				}
			}
		}
		return;
	}

	// Face the target while engaging (yaw only).
	if (ACharacter* OwnerChar = Cast<ACharacter>(GetOwner()))
	{
		FRotator Face = (Target->GetActorLocation()
			- OwnerChar->GetActorLocation()).Rotation();
		Face.Pitch = 0.f;
		Face.Roll = 0.f;
		OwnerChar->SetActorRotation(
			FMath::RInterpTo(OwnerChar->GetActorRotation(), Face, DeltaTime, 6.f));
	}

	if (RoundsLeftInBurst > 0)
	{
		if (Now >= NextRoundTime)
		{
			FireRound();
			--RoundsLeftInBurst;
			--RoundsLeftInMag;
			NextRoundTime = Now + RoundInterval;

			if (RoundsLeftInMag <= 0)
			{
				RoundsLeftInBurst = 0;
				RoundsLeftInMag = MagazineRounds;
				NextBurstTime = Now + ReloadTime;
			}
		}
		return;
	}

	if (Now >= NextBurstTime)
	{
		RoundsLeftInBurst = FMath::Max(1, BurstRounds + FMath::RandRange(-1, 2));
		NextRoundTime = Now;
		NextBurstTime = Now + BurstCooldown * FMath::FRandRange(0.6f, 1.4f);

		if (!bHasLoggedFirstBurst)
		{
			bHasLoggedFirstBurst = true;
			UE_LOG(LogTemp, Log, TEXT("[DirectFire] %s opened fire on %s at %.0f m"),
				*GetOwner()->GetName(), *Target->GetName(),
				FVector::Dist(GetOwner()->GetActorLocation(),
					Target->GetActorLocation()) / 100.f);
		}
	}
}
