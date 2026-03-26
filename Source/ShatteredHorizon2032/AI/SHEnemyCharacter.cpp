// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "AI/SHEnemyCharacter.h"
#include "AI/SHAIPerceptionConfig.h"
#include "Combat/SHDeathSystem.h"
#include "Combat/SHDamageSystem.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogSHEnemy, Log, All);

// -----------------------------------------------------------------------
ASHEnemyCharacter::ASHEnemyCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f; // Every frame for suppression decay.

	CurrentHealth = MaxHealth;
	bReplicates = true;

	// Death physics system.
	DeathSystemComp = CreateDefaultSubobject<USHDeathSystem>(TEXT("DeathSystem"));

	// Sensible defaults for a military-sim character.
	GetCharacterMovement()->MaxWalkSpeed = 450.f;
	GetCharacterMovement()->MaxWalkSpeedCrouched = 200.f;
	GetCharacterMovement()->bCanCrouch = true;
	GetCharacterMovement()->NavAgentProps.bCanCrouch = true;
}

void ASHEnemyCharacter::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
	Armor.CurrentArmorHP = Armor.ArmorHP;
	Morale = 0.8f;
	MoraleState = ESHMoraleState::Steady;

	// Apply role loadout if preset exists.
	if (const FSHEnemyLoadout* Preset = RoleLoadoutPresets.Find(Role))
	{
		Loadout = *Preset;
	}
}

void ASHEnemyCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (IsDead() || HasSurrendered())
	{
		return;
	}

	// --- Suppression decay ---
	if (SuppressionLevel > 0.f)
	{
		SuppressionLevel = FMath::Max(0.f, SuppressionLevel - SuppressionDecayRate * DeltaSeconds);
	}

	// --- Morale recovery (slow, only when not heavily suppressed) ---
	if (!IsHeavilySuppressed() && Morale < 1.f)
	{
		const float CohesionRecoveryBonus = GetSquadCohesionBonus() - 1.f; // 0..0.3
		Morale = FMath::Min(1.f, Morale + (MoraleRecoveryRate + CohesionRecoveryBonus * 0.01f) * DeltaSeconds);
	}

	// --- Squad cohesion (update periodically, not every frame) ---
	SquadCohesionCheckTimer += DeltaSeconds;
	if (SquadCohesionCheckTimer >= 1.0f)
	{
		SquadCohesionCheckTimer = 0.f;
		UpdateSquadCohesion();
	}

	// --- Voice line cooldown ---
	if (VoiceLineCooldownRemaining > 0.f)
	{
		VoiceLineCooldownRemaining -= DeltaSeconds;
	}

	// --- Evaluate morale state changes ---
	EvaluateMoraleState();
}

// -----------------------------------------------------------------------
//  Damage
// -----------------------------------------------------------------------

float ASHEnemyCharacter::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (IsDead() || HasSurrendered())
	{
		return 0.f;
	}

	const float ModifiedDamage = ApplyArmorDamageReduction(DamageAmount, DamageEvent);
	const float ActualDamage = Super::TakeDamage(ModifiedDamage, DamageEvent, EventInstigator, DamageCauser);

	CurrentHealth = FMath::Max(0.f, CurrentHealth - ActualDamage);

	// Taking damage increases suppression and decreases morale.
	AddSuppression(0.3f);
	ApplyMoraleModifier(-0.15f);

	UE_LOG(LogSHEnemy, Verbose, TEXT("%s took %.1f damage (%.1f after armor). Health: %.1f"),
		*GetName(), DamageAmount, ActualDamage, CurrentHealth);

	if (CurrentHealth <= 0.f)
	{
		Die();
	}
	else if (CurrentHealth < MaxHealth * 0.2f && !bIsWounded)
	{
		// 20% chance to become incapacitated at low health.
		if (FMath::FRand() < 0.2f)
		{
			BecomeWounded();
		}
	}

	return ActualDamage;
}

float ASHEnemyCharacter::ApplyArmorDamageReduction(float RawDamage, const FDamageEvent& DamageEvent)
{
	// Point damage can target specific body areas.
	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& PointDamage = static_cast<const FPointDamageEvent&>(DamageEvent);
		const FName BoneName = PointDamage.HitInfo.BoneName;

		// Head hits.
		if (BoneName == FName("head") || BoneName == FName("neck"))
		{
			if (Armor.bHasHelmet && Armor.CurrentArmorHP > 0.f && FMath::FRand() < Armor.HelmetRicochetChance)
			{
				Armor.CurrentArmorHP -= RawDamage * 0.5f;
				return RawDamage * 0.1f; // Ricochet — minimal damage.
			}
			return RawDamage * 2.0f; // Headshot multiplier.
		}

		// Torso hits with body armor.
		if (BoneName == FName("spine_01") || BoneName == FName("spine_02") || BoneName == FName("spine_03"))
		{
			if (Armor.CurrentArmorHP > 0.f)
			{
				const float Absorbed = RawDamage * (1.f - Armor.TorsoDamageMultiplier);
				Armor.CurrentArmorHP -= Absorbed;
				return RawDamage * Armor.TorsoDamageMultiplier;
			}
		}
	}

	return RawDamage;
}

// -----------------------------------------------------------------------
//  Morale
// -----------------------------------------------------------------------

void ASHEnemyCharacter::ApplyMoraleModifier(float Delta)
{
	if (IsDead() || HasSurrendered()) return;

	// Squad cohesion mitigates negative morale changes.
	if (Delta < 0.f)
	{
		Delta *= (2.f - GetSquadCohesionBonus()); // Higher cohesion = less morale loss.
	}

	Morale = FMath::Clamp(Morale + Delta, 0.f, 1.f);
}

void ASHEnemyCharacter::ForceMoraleState(ESHMoraleState NewState)
{
	if (MoraleState != NewState)
	{
		MoraleState = NewState;
		OnMoraleChanged.Broadcast(this, MoraleState);
	}
}

void ASHEnemyCharacter::EvaluateMoraleState()
{
	if (IsDead() || HasSurrendered()) return;

	ESHMoraleState NewState = MoraleState;

	if (Morale >= 0.7f)
	{
		NewState = ESHMoraleState::Determined;
	}
	else if (Morale >= 0.5f)
	{
		NewState = ESHMoraleState::Steady;
	}
	else if (Morale >= 0.3f)
	{
		NewState = ESHMoraleState::Shaken;
	}
	else if (Morale >= 0.1f)
	{
		NewState = ESHMoraleState::Breaking;
	}
	else
	{
		NewState = ESHMoraleState::Routed;
	}

	if (NewState != MoraleState)
	{
		MoraleState = NewState;
		OnMoraleChanged.Broadcast(this, MoraleState);

		// Auto-voice lines on morale transitions.
		if (MoraleState == ESHMoraleState::Routed)
		{
			PlayVoiceLine(ESHVoiceLineContext::Retreating);
		}
		else if (MoraleState == ESHMoraleState::Breaking)
		{
			PlayVoiceLine(ESHVoiceLineContext::Suppressed);
		}
	}

	// Automatic surrender check when deeply broken.
	if (MoraleState == ESHMoraleState::Routed && IsHeavilySuppressed() && NearbySquadMateCount == 0)
	{
		// Stochastic surrender — 2% chance per second.
		if (FMath::FRand() < 0.02f * GetWorld()->GetDeltaSeconds())
		{
			AttemptSurrender();
		}
	}
}

// -----------------------------------------------------------------------
//  Surrender
// -----------------------------------------------------------------------

bool ASHEnemyCharacter::AttemptSurrender()
{
	if (IsDead() || HasSurrendered()) return false;

	// Must be heavily suppressed and isolated (no nearby squad mates).
	if (!IsHeavilySuppressed() || NearbySquadMateCount > 1)
	{
		return false;
	}

	// Officers and certain roles resist surrender.
	if (Role == ESHEnemyRole::Officer && FMath::FRand() < 0.7f)
	{
		return false;
	}

	MoraleState = ESHMoraleState::Surrendered;
	PlayVoiceLine(ESHVoiceLineContext::Surrendering);
	OnSurrenderEvent.Broadcast(this);

	// Drop weapon, raise hands (controller handles animation).
	GetCharacterMovement()->StopMovementImmediately();

	UE_LOG(LogSHEnemy, Log, TEXT("%s has surrendered."), *GetName());
	return true;
}

// -----------------------------------------------------------------------
//  Suppression
// -----------------------------------------------------------------------

void ASHEnemyCharacter::AddSuppression(float Amount)
{
	if (IsDead() || HasSurrendered()) return;

	SuppressionLevel = FMath::Clamp(SuppressionLevel + Amount, 0.f, 1.f);

	// Suppression also degrades morale.
	ApplyMoraleModifier(-Amount * 0.1f);
}

// -----------------------------------------------------------------------
//  Squad cohesion
// -----------------------------------------------------------------------

void ASHEnemyCharacter::UpdateSquadCohesion()
{
	NearbySquadMateCount = 0;

	if (SquadId == INDEX_NONE) return;

	// Overlap query for nearby characters in same squad.
	TArray<FOverlapResult> Overlaps;
	FCollisionShape Sphere = FCollisionShape::MakeSphere(SquadCohesionRadius);
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	if (GetWorld()->OverlapMultiByChannel(Overlaps, GetActorLocation(), FQuat::Identity,
		ECC_Pawn, Sphere, Params))
	{
		for (const FOverlapResult& Overlap : Overlaps)
		{
			if (ASHEnemyCharacter* Other = Cast<ASHEnemyCharacter>(Overlap.GetActor()))
			{
				if (Other->IsAlive() && Other->GetSquadId() == SquadId)
				{
					NearbySquadMateCount++;
				}
			}
		}
	}
}

float ASHEnemyCharacter::GetSquadCohesionBonus() const
{
	// 0 mates = 1.0x, 4+ mates = 1.3x (max bonus).
	return 1.f + FMath::Clamp(static_cast<float>(NearbySquadMateCount) / 4.f, 0.f, 0.3f);
}

// -----------------------------------------------------------------------
//  Role
// -----------------------------------------------------------------------

void ASHEnemyCharacter::SetRole(ESHEnemyRole NewRole)
{
	Role = NewRole;

	if (const FSHEnemyLoadout* Preset = RoleLoadoutPresets.Find(Role))
	{
		Loadout = *Preset;
	}

	// Adjust movement speed by role.
	switch (Role)
	{
	case ESHEnemyRole::MachineGunner:
		GetCharacterMovement()->MaxWalkSpeed = 380.f;
		break;
	case ESHEnemyRole::RPG:
		GetCharacterMovement()->MaxWalkSpeed = 400.f;
		break;
	case ESHEnemyRole::Engineer:
		GetCharacterMovement()->MaxWalkSpeed = 420.f;
		break;
	default:
		GetCharacterMovement()->MaxWalkSpeed = 450.f;
		break;
	}
}

// -----------------------------------------------------------------------
//  Voice lines
// -----------------------------------------------------------------------

void ASHEnemyCharacter::PlayVoiceLine(ESHVoiceLineContext Context)
{
	if (IsDead() || VoiceLineCooldownRemaining > 0.f) return;

	USoundBase** SoundPtr = VoiceLines.Find(Context);
	if (SoundPtr && *SoundPtr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, *SoundPtr, GetActorLocation(), 1.f, 1.f, 0.f, nullptr, nullptr);
		VoiceLineCooldownRemaining = VoiceLineCooldown;
	}
}

// -----------------------------------------------------------------------
//  Death & Cleanup
// -----------------------------------------------------------------------

void ASHEnemyCharacter::Die()
{
	if (bRagdollActive) return;

	CurrentHealth = 0.f;

	PlayVoiceLine(ESHVoiceLineContext::ManDown);
	SpawnEquipmentDrops();

	// --- Use DeathSystem for physics-driven ragdoll if available ---
	if (DeathSystemComp)
	{
		FSHDamageInfo KillingBlow;
		KillingBlow.DamageType = ESHDamageType::Ballistic;
		KillingBlow.HitZone = ESHHitZone::Torso;
		KillingBlow.DamageDirection = FVector::ForwardVector;
		DeathSystemComp->ExecuteDeath(KillingBlow);
		bRagdollActive = true;
	}
	else
	{
		// Fallback: simple ragdoll.
		EnableRagdoll();
	}

	// Notify squad mates (nearby morale penalty).
	TArray<FOverlapResult> Overlaps;
	FCollisionShape Sphere = FCollisionShape::MakeSphere(SquadCohesionRadius);
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(this);

	if (GetWorld()->OverlapMultiByChannel(Overlaps, GetActorLocation(), FQuat::Identity,
		ECC_Pawn, Sphere, Params))
	{
		for (const FOverlapResult& Overlap : Overlaps)
		{
			if (ASHEnemyCharacter* Other = Cast<ASHEnemyCharacter>(Overlap.GetActor()))
			{
				if (Other->IsAlive() && Other->GetSquadId() == SquadId)
				{
					Other->ApplyMoraleModifier(-0.15f);
					Other->PlayVoiceLine(ESHVoiceLineContext::ManDown);
				}
			}
		}
	}

	// Disable collision for gameplay purposes (keep mesh for ragdoll).
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	OnDeathEvent.Broadcast(this);

	UE_LOG(LogSHEnemy, Log, TEXT("%s killed."), *GetName());
}

void ASHEnemyCharacter::BecomeWounded()
{
	if (bIsWounded || IsDead()) return;

	bIsWounded = true;
	GetCharacterMovement()->MaxWalkSpeed = 100.f;
	GetCharacterMovement()->MaxWalkSpeedCrouched = 50.f;

	PlayVoiceLine(ESHVoiceLineContext::ManDown);

	UE_LOG(LogSHEnemy, Log, TEXT("%s is wounded / incapacitated."), *GetName());
}

void ASHEnemyCharacter::SpawnEquipmentDrops()
{
	UWorld* World = GetWorld();
	if (!World) return;

	for (const TSubclassOf<AActor>& DropClass : Loadout.DropOnDeathClasses)
	{
		if (DropClass)
		{
			const FVector SpawnLoc = GetActorLocation() + FVector(FMath::FRandRange(-50.f, 50.f),
				FMath::FRandRange(-50.f, 50.f), 50.f);
			FActorSpawnParameters SpawnParams;
			SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			World->SpawnActor<AActor>(DropClass, SpawnLoc, FRotator::ZeroRotator, SpawnParams);
		}
	}
}

void ASHEnemyCharacter::EnableRagdoll()
{
	if (bRagdollActive) return;
	bRagdollActive = true;

	USkeletalMeshComponent* MeshComp = GetMesh();
	if (MeshComp)
	{
		MeshComp->SetSimulatePhysics(true);
		MeshComp->SetCollisionProfileName(TEXT("Ragdoll"));
		MeshComp->SetAllBodiesSimulatePhysics(true);
	}

	GetCharacterMovement()->DisableMovement();
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

// -----------------------------------------------------------------------
//  Replication
// -----------------------------------------------------------------------

void ASHEnemyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASHEnemyCharacter, CurrentHealth);
	DOREPLIFETIME(ASHEnemyCharacter, MoraleState);
	DOREPLIFETIME(ASHEnemyCharacter, SquadId);
}
