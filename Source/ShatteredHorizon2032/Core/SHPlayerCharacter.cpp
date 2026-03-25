// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHPlayerCharacter.h"
#include "SHCameraSystem.h"
#include "Camera/CameraComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Combat/SHHitFeedback.h"
#include "Combat/SHDeathSystem.h"
#include "Combat/SHDamageSystem.h"
#include "Combat/SHFatigueSystem.h"
#include "Audio/SHReverbZoneManager.h"
#include "Audio/SHAmbientSoundscape.h"

ASHPlayerCharacter::ASHPlayerCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	// First-person camera attached to the head socket area.
	FirstPersonCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCamera->SetupAttachment(GetMesh(), FName(TEXT("head")));
	FirstPersonCamera->bUsePawnControlRotation = true;
	FirstPersonCamera->SetRelativeLocation(FVector(0.f, 0.f, 0.f));

	// First-person arms mesh (visible only to the owning player).
	FirstPersonArms = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FirstPersonArms"));
	FirstPersonArms->SetupAttachment(FirstPersonCamera);
	FirstPersonArms->SetOnlyOwnerSee(true);
	FirstPersonArms->bCastDynamicShadow = false;
	FirstPersonArms->CastShadow = false;

	// Hide the third-person mesh from the owning player.
	GetMesh()->SetOwnerNoSee(true);

	// --- New Round 2-5 systems ---

	// Camera feel system.
	CameraSystem = CreateDefaultSubobject<USHCameraSystem>(TEXT("CameraSystem"));

	// Hit feedback (screen punch, indicators, VFX).
	HitFeedback = CreateDefaultSubobject<USHHitFeedback>(TEXT("HitFeedback"));

	// Death physics (ragdoll, momentum transfer, body persistence).
	DeathSystem = CreateDefaultSubobject<USHDeathSystem>(TEXT("DeathSystem"));

	// Damage model (wounds, armor, bleeding).
	DamageSystemComp = CreateDefaultSubobject<USHDamageSystem>(TEXT("DamageSystem"));

	// Fatigue (stamina, long-term fatigue, breath control).
	FatigueSystem = CreateDefaultSubobject<USHFatigueSystem>(TEXT("FatigueSystem"));

	// Audio: dynamic reverb zones.
	ReverbZoneManager = CreateDefaultSubobject<USHReverbZoneManager>(TEXT("ReverbZoneManager"));

	// Audio: ambient soundscape layers.
	AmbientSoundscape = CreateDefaultSubobject<USHAmbientSoundscape>(TEXT("AmbientSoundscape"));

	// Configure movement defaults.
	UCharacterMovementComponent* CMC = GetCharacterMovement();
	if (CMC)
	{
		CMC->MaxWalkSpeed = BaseWalkSpeed;
		CMC->MaxWalkSpeedCrouched = BaseWalkSpeed * CrouchSpeedMultiplier;
		CMC->NavAgentProps.bCanCrouch = true;
		CMC->SetCrouchedHalfHeight(48.f);
	}

	// Initialize limb states.
	LimbStates.SetNum(6);
	LimbStates[0] = { ESHLimb::Head,     1.f, false, false };
	LimbStates[1] = { ESHLimb::Torso,    1.f, false, false };
	LimbStates[2] = { ESHLimb::LeftArm,  1.f, false, false };
	LimbStates[3] = { ESHLimb::RightArm, 1.f, false, false };
	LimbStates[4] = { ESHLimb::LeftLeg,  1.f, false, false };
	LimbStates[5] = { ESHLimb::RightLeg, 1.f, false, false };
}

void ASHPlayerCharacter::BeginPlay()
{
	Super::BeginPlay();
	CurrentHealth = MaxHealth;
	Stamina = MaxStamina;
	RecalculateWeight();
}

void ASHPlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASHPlayerCharacter, CurrentHealth);
	DOREPLIFETIME(ASHPlayerCharacter, Stamina);
	DOREPLIFETIME(ASHPlayerCharacter, CurrentStance);
	DOREPLIFETIME(ASHPlayerCharacter, LimbStates);
}

// =======================================================================
//  Tick
// =======================================================================

void ASHPlayerCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bIsDead)
	{
		return;
	}

	TickStamina(DeltaSeconds);
	TickSuppression(DeltaSeconds);
	TickBleeding(DeltaSeconds);
	TickLean(DeltaSeconds);

	// --- Feed camera system with current character state ---
	if (CameraSystem)
	{
		FSHCameraContext CamCtx;
		CamCtx.GroundSpeed = GetVelocity().Size2D();
		CamCtx.bIsSprinting = bIsSprinting;
		CamCtx.bIsADS = bIsADS;
		CamCtx.Suppression = SuppressionLevel;
		CamCtx.Fatigue = FatigueSystem ? (1.f - FatigueSystem->GetStaminaPercent()) : 0.f;
		CamCtx.HipFOV = 90.f;
		// TODO: Read ADS FOV from equipped weapon data when weapon system is wired.
		CamCtx.ADSFOV = 55.f;
		CamCtx.ADSTransitionTime = 0.2f;
		CameraSystem->SetCameraContext(CamCtx);
	}

	// Apply dynamic movement speed based on stance, weight, injuries.
	UCharacterMovementComponent* CMC = GetCharacterMovement();
	if (CMC)
	{
		float TargetSpeed = BaseWalkSpeed;

		switch (CurrentStance)
		{
		case ESHStance::Crouching:
			TargetSpeed = BaseWalkSpeed * CrouchSpeedMultiplier;
			break;
		case ESHStance::Prone:
			TargetSpeed = BaseWalkSpeed * ProneSpeedMultiplier;
			break;
		default:
			if (bIsSprinting && Stamina > 0.f)
			{
				TargetSpeed = BaseWalkSpeed * SprintSpeedMultiplier;
			}
			break;
		}

		// Apply weight and injury penalties.
		TargetSpeed *= GetMovementSpeedMultiplier();

		CMC->MaxWalkSpeed = TargetSpeed;
	}
}

// =======================================================================
//  Damage
// =======================================================================

float ASHPlayerCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (bIsDead)
	{
		return 0.f;
	}

	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// Determine hit limb from the damage event (point damage).
	ESHLimb HitLimb = ESHLimb::Torso;
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& PointDamage = static_cast<const FPointDamageEvent&>(DamageEvent);
		const FName BoneName = PointDamage.HitInfo.BoneName;

		// Map bone to limb.
		if (BoneName.ToString().Contains(TEXT("head"), ESearchCase::IgnoreCase))
		{
			HitLimb = ESHLimb::Head;
		}
		else if (BoneName.ToString().Contains(TEXT("l_arm")) || BoneName.ToString().Contains(TEXT("l_hand")))
		{
			HitLimb = ESHLimb::LeftArm;
		}
		else if (BoneName.ToString().Contains(TEXT("r_arm")) || BoneName.ToString().Contains(TEXT("r_hand")))
		{
			HitLimb = ESHLimb::RightArm;
		}
		else if (BoneName.ToString().Contains(TEXT("l_leg")) || BoneName.ToString().Contains(TEXT("l_foot")))
		{
			HitLimb = ESHLimb::LeftLeg;
		}
		else if (BoneName.ToString().Contains(TEXT("r_leg")) || BoneName.ToString().Contains(TEXT("r_foot")))
		{
			HitLimb = ESHLimb::RightLeg;
		}
	}

	// Head shots are lethal multiplier.
	float FinalDamage = ActualDamage;
	if (HitLimb == ESHLimb::Head)
	{
		FinalDamage *= 3.f;
	}

	// Apply limb damage.
	ApplyLimbDamage(HitLimb, FinalDamage / MaxHealth);

	// Apply to global HP.
	CurrentHealth = FMath::Max(0.f, CurrentHealth - FinalDamage);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);

	// Incoming fire always adds suppression.
	AddSuppression(0.15f);

	// --- Wire hit feedback on damage received ---
	if (HitFeedback)
	{
		FSHDamageInfo DmgInfo;
		DmgInfo.BaseDamage = FinalDamage;
		DmgInfo.DamageType = ESHDamageType::Ballistic;
		DmgInfo.HitZone = static_cast<ESHHitZone>(static_cast<uint8>(HitLimb));
		DmgInfo.Instigator = DamageCauser;

		// Compute damage direction from causer.
		if (DamageCauser)
		{
			DmgInfo.DamageDirection = (GetActorLocation() - DamageCauser->GetActorLocation()).GetSafeNormal();
		}

		if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
		{
			const FPointDamageEvent& PointDmg = static_cast<const FPointDamageEvent&>(DamageEvent);
			DmgInfo.ImpactLocation = PointDmg.HitInfo.ImpactPoint;
		}

		FSHDamageResult DmgResult;
		DmgResult.DamageDealt = FinalDamage;
		DmgResult.bIsLethal = CurrentHealth <= 0.f;
		DmgResult.HitZone = DmgInfo.HitZone;

		HitFeedback->OnDamageReceived(DmgInfo, DmgResult);
	}

	if (CurrentHealth <= 0.f)
	{
		Die();
	}

	return FinalDamage;
}

void ASHPlayerCharacter::ApplyLimbDamage(ESHLimb Limb, float Damage)
{
	for (FSHLimbState& State : LimbStates)
	{
		if (State.Limb == Limb)
		{
			State.Health = FMath::Clamp(State.Health - Damage, 0.f, 1.f);

			// Significant damage causes bleeding.
			if (Damage > 0.15f && !State.bIsTreated)
			{
				State.bIsBleeding = true;
			}

			OnLimbDamaged.Broadcast(Limb, State.Health);
			return;
		}
	}
}

void ASHPlayerCharacter::Die()
{
	if (bIsDead)
	{
		return;
	}

	bIsDead = true;
	OnPlayerDeath.Broadcast();

	// Disable input.
	if (AController* PC = GetController())
	{
		DisableInput(Cast<APlayerController>(PC));
	}

	// --- Use DeathSystem for physics-driven ragdoll ---
	if (DeathSystem)
	{
		FSHDamageInfo KillingBlow;
		KillingBlow.DamageDirection = FVector::ForwardVector; // overridden by last hit
		KillingBlow.HitZone = ESHHitZone::Torso;
		KillingBlow.DamageType = ESHDamageType::Ballistic;
		DeathSystem->ExecuteDeath(KillingBlow);
	}
	else
	{
		// Fallback: simple ragdoll if DeathSystem is missing.
		GetMesh()->SetSimulatePhysics(true);
		GetMesh()->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	UE_LOG(LogTemp, Log, TEXT("[SHPlayerCharacter] Player killed"));
}

// =======================================================================
//  Movement
// =======================================================================

void ASHPlayerCharacter::StartSprint()
{
	if (CurrentStance != ESHStance::Standing || Stamina <= 0.f)
	{
		return;
	}

	bIsSprinting = true;

	// Cannot ADS while sprinting.
	if (bIsADS)
	{
		StopADS();
	}
}

void ASHPlayerCharacter::StopSprint()
{
	bIsSprinting = false;
}

void ASHPlayerCharacter::ToggleCrouch()
{
	if (CurrentStance == ESHStance::Crouching)
	{
		UnCrouch();
		CurrentStance = ESHStance::Standing;
	}
	else if (CurrentStance == ESHStance::Prone)
	{
		// Go from prone to crouch.
		CurrentStance = ESHStance::Crouching;
		Crouch();
	}
	else
	{
		Crouch();
		CurrentStance = ESHStance::Crouching;
	}

	bIsSprinting = false;
}

void ASHPlayerCharacter::ToggleProne()
{
	if (CurrentStance == ESHStance::Prone)
	{
		CurrentStance = ESHStance::Crouching;
		Crouch();
	}
	else
	{
		CurrentStance = ESHStance::Prone;
		Crouch(); // Use engine crouch as a base, reduce further in tick.
		bIsSprinting = false;
	}
}

void ASHPlayerCharacter::TryVault()
{
	if (bIsVaulting || CurrentStance == ESHStance::Prone)
	{
		return;
	}

	if (CanVault())
	{
		ExecuteVault();
	}
	else
	{
		// Fall back to standard jump if no vault geometry.
		Jump();
	}
}

bool ASHPlayerCharacter::CanVault() const
{
	// Forward trace to detect vaultable geometry.
	const FVector Start = GetActorLocation();
	const FVector Forward = GetActorForwardVector();
	const FVector End = Start + Forward * VaultTraceDistance;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	if (GetWorld()->LineTraceSingleByChannel(Hit, Start, End, ECC_WorldStatic, Params))
	{
		// Check height of the obstacle.
		const FVector WallTop = Hit.ImpactPoint + FVector(0.f, 0.f, VaultMaxHeight);
		FHitResult TopHit;
		if (!GetWorld()->LineTraceSingleByChannel(TopHit, WallTop, WallTop - FVector(0.f, 0.f, VaultMaxHeight * 2.f),
			ECC_WorldStatic, Params))
		{
			return false;
		}

		const float ObstacleHeight = TopHit.ImpactPoint.Z - GetActorLocation().Z;
		return ObstacleHeight > 30.f && ObstacleHeight <= VaultMaxHeight;
	}

	return false;
}

void ASHPlayerCharacter::ExecuteVault()
{
	bIsVaulting = true;

	// Launch the character upward and forward.
	const FVector LaunchVelocity = GetActorForwardVector() * 300.f + FVector(0.f, 0.f, 400.f);
	LaunchCharacter(LaunchVelocity, true, true);

	// Reset vaulting state after a short delay.
	FTimerHandle VaultTimerHandle;
	GetWorldTimerManager().SetTimer(VaultTimerHandle, [this]()
	{
		bIsVaulting = false;
	}, 0.6f, false);

	// Drain stamina for vault.
	Stamina = FMath::Max(0.f, Stamina - 10.f);
}

void ASHPlayerCharacter::TrySlide()
{
	if (bIsSprinting && CurrentStance == ESHStance::Standing && !bIsSliding)
	{
		bIsSliding = true;
		CurrentStance = ESHStance::Crouching;
		Crouch();

		// Apply forward boost.
		const FVector SlideBoost = GetActorForwardVector() * 600.f;
		LaunchCharacter(SlideBoost, true, false);

		// End slide after duration.
		FTimerHandle SlideTimerHandle;
		GetWorldTimerManager().SetTimer(SlideTimerHandle, [this]()
		{
			bIsSliding = false;
		}, 0.8f, false);

		Stamina = FMath::Max(0.f, Stamina - 15.f);
	}
}

void ASHPlayerCharacter::SetLeanState(ESHLeanState InLeanState)
{
	ActiveLeanState = InLeanState;
}

// =======================================================================
//  Combat
// =======================================================================

void ASHPlayerCharacter::StartFire()
{
	if (bIsSprinting || bIsDead)
	{
		return;
	}

	bIsFiring = true;
	// TODO: Interface with weapon component to fire.
}

void ASHPlayerCharacter::StopFire()
{
	bIsFiring = false;
}

void ASHPlayerCharacter::StartADS()
{
	if (bIsSprinting || bIsDead)
	{
		return;
	}

	bIsADS = true;
	// TODO: Adjust camera FOV and apply weapon ADS offset.
}

void ASHPlayerCharacter::StopADS()
{
	bIsADS = false;
}

void ASHPlayerCharacter::Reload()
{
	if (bIsDead)
	{
		return;
	}

	// TODO: Interface with weapon component to reload.
	UE_LOG(LogTemp, Log, TEXT("[SHPlayerCharacter] Reload requested"));
}

void ASHPlayerCharacter::EquipSlot(int32 SlotIndex)
{
	if (bIsDead || SlotIndex < 0 || SlotIndex >= static_cast<int32>(ESHEquipmentSlot::Max))
	{
		return;
	}

	ActiveEquipmentSlot = SlotIndex;
	RecalculateWeight();

	UE_LOG(LogTemp, Log, TEXT("[SHPlayerCharacter] Equipped slot %d"), SlotIndex);
}

// =======================================================================
//  Health / healing
// =======================================================================

void ASHPlayerCharacter::ApplyHealing(float Amount)
{
	if (bIsDead || Amount <= 0.f)
	{
		return;
	}

	CurrentHealth = FMath::Min(MaxHealth, CurrentHealth + Amount);
	OnHealthChanged.Broadcast(CurrentHealth, MaxHealth);
}

void ASHPlayerCharacter::TreatLimb(ESHLimb Limb)
{
	for (FSHLimbState& State : LimbStates)
	{
		if (State.Limb == Limb)
		{
			State.bIsBleeding = false;
			State.bIsTreated = true;
			// Treatment restores some limb function but not full.
			State.Health = FMath::Min(1.f, State.Health + 0.25f);
			return;
		}
	}
}

// =======================================================================
//  Suppression
// =======================================================================

void ASHPlayerCharacter::AddSuppression(float Amount)
{
	SuppressionLevel = FMath::Clamp(SuppressionLevel + Amount, 0.f, MaxSuppression);
	OnSuppressionChanged.Broadcast(SuppressionLevel);
}

// =======================================================================
//  Weight
// =======================================================================

void ASHPlayerCharacter::RecalculateWeight()
{
	// Base weight: body armor + helmet + basic gear.
	CurrentWeight = 12.f;

	// TODO: Sum actual equipped item weights from the inventory system.
	// Placeholder estimates per slot:
	// Primary weapon: ~4-5 kg, Sidearm: ~1 kg, Grenades: ~0.5 kg each,
	// Gear varies. For now, use a fixed estimate.
	CurrentWeight += 4.5f; // primary
	CurrentWeight += 1.2f; // sidearm
	CurrentWeight += 1.5f; // grenades (3x)
	CurrentWeight += 3.0f; // other gear (radio, NVG, etc.)
}

// =======================================================================
//  Tick helpers
// =======================================================================

void ASHPlayerCharacter::TickStamina(float DeltaSeconds)
{
	const float OldNormalized = Stamina / MaxStamina;

	if (bIsSprinting && CurrentStance == ESHStance::Standing)
	{
		// Weight penalty on stamina drain.
		const float WeightPenalty = FMath::GetMappedRangeValueClamped(
			FVector2D(10.f, MaxCarryWeight), FVector2D(1.f, 1.8f), CurrentWeight);

		Stamina = FMath::Max(0.f, Stamina - SprintStaminaDrain * WeightPenalty * DeltaSeconds);

		if (Stamina <= 0.f)
		{
			StopSprint();
		}
	}
	else
	{
		// Recovery is slower when heavily loaded.
		const float RecoveryPenalty = FMath::GetMappedRangeValueClamped(
			FVector2D(10.f, MaxCarryWeight), FVector2D(1.f, 0.5f), CurrentWeight);

		Stamina = FMath::Min(MaxStamina, Stamina + StaminaRecoveryRate * RecoveryPenalty * DeltaSeconds);
	}

	const float NewNormalized = Stamina / MaxStamina;
	if (!FMath::IsNearlyEqual(OldNormalized, NewNormalized, 0.01f))
	{
		OnStaminaChanged.Broadcast(NewNormalized);
	}
}

void ASHPlayerCharacter::TickSuppression(float DeltaSeconds)
{
	if (SuppressionLevel > 0.f)
	{
		SuppressionLevel = FMath::Max(0.f, SuppressionLevel - SuppressionDecayRate * DeltaSeconds);

		// Apply camera shake / screen effects proportional to suppression.
		// TODO: Interface with post-process volume for suppression VFX.
	}
}

void ASHPlayerCharacter::TickBleeding(float DeltaSeconds)
{
	for (const FSHLimbState& State : LimbStates)
	{
		if (State.bIsBleeding && !State.bIsTreated)
		{
			CurrentHealth = FMath::Max(0.f, CurrentHealth - BleedDamagePerSecond * DeltaSeconds);

			if (CurrentHealth <= 0.f)
			{
				Die();
				return;
			}
		}
	}
}

void ASHPlayerCharacter::TickLean(float DeltaSeconds)
{
	float TargetAlpha = 0.f;
	if (ActiveLeanState == ESHLeanState::Left)
	{
		TargetAlpha = -1.f;
	}
	else if (ActiveLeanState == ESHLeanState::Right)
	{
		TargetAlpha = 1.f;
	}

	CurrentLeanAlpha = FMath::FInterpTo(CurrentLeanAlpha, TargetAlpha, DeltaSeconds, LeanInterpSpeed);

	if (FirstPersonCamera && !FMath::IsNearlyZero(CurrentLeanAlpha, 0.01f))
	{
		// Apply lean offset and rotation to the camera.
		const FVector LeanOffset = FVector(0.f, CurrentLeanAlpha * LeanOffsetDistance, 0.f);
		FirstPersonCamera->SetRelativeLocation(LeanOffset);

		const FRotator LeanRotation = FRotator(0.f, 0.f, CurrentLeanAlpha * LeanAngleDeg);
		FirstPersonCamera->SetRelativeRotation(LeanRotation);
	}
	else if (FirstPersonCamera)
	{
		FirstPersonCamera->SetRelativeLocation(FVector::ZeroVector);
		FirstPersonCamera->SetRelativeRotation(FRotator::ZeroRotator);
	}
}

float ASHPlayerCharacter::GetMovementSpeedMultiplier() const
{
	float Multiplier = 1.f;

	// Weight penalty: linear reduction from 1.0 at 10kg to 0.6 at max weight.
	Multiplier *= FMath::GetMappedRangeValueClamped(
		FVector2D(10.f, MaxCarryWeight), FVector2D(1.f, 0.6f), CurrentWeight);

	// Leg injury penalty.
	for (const FSHLimbState& State : LimbStates)
	{
		if (State.Limb == ESHLimb::LeftLeg || State.Limb == ESHLimb::RightLeg)
		{
			if (State.Health < 0.5f)
			{
				Multiplier *= FMath::Lerp(0.3f, 1.f, State.Health * 2.f);
			}
		}
	}

	// Suppression causes involuntary slowing.
	Multiplier *= FMath::Lerp(1.f, 0.7f, SuppressionLevel);

	return FMath::Max(Multiplier, 0.15f);
}

float ASHPlayerCharacter::GetAimSwayMultiplier() const
{
	float Sway = 1.f;

	// Fatigue increases sway.
	const float FatigueNormalized = 1.f - (Stamina / MaxStamina);
	Sway += FatigueNormalized * 0.8f;

	// Suppression increases sway heavily.
	Sway += SuppressionLevel * 1.5f;

	// Arm injuries increase sway.
	for (const FSHLimbState& State : LimbStates)
	{
		if (State.Limb == ESHLimb::LeftArm || State.Limb == ESHLimb::RightArm)
		{
			if (State.Health < 0.8f)
			{
				Sway += (1.f - State.Health) * 1.2f;
			}
		}
	}

	return Sway;
}
