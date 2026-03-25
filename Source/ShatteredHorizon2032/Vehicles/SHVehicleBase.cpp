// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHVehicleBase.h"
#include "SHVehicleSeatSystem.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/DamageEvents.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/DamageType.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "ShatteredHorizon2032/ShatteredHorizon2032.h"

/* -----------------------------------------------------------------------
 *  Well-known zone names
 * --------------------------------------------------------------------- */

namespace SHVehicleZones
{
	const FName Engine     = TEXT("Engine");
	const FName Wheels     = TEXT("Wheels");
	const FName Turret     = TEXT("Turret");
	const FName Hull       = TEXT("Hull");
	const FName Fuel       = TEXT("Fuel");
}

/* -----------------------------------------------------------------------
 *  Constructor
 * --------------------------------------------------------------------- */

ASHVehicleBase::ASHVehicleBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PrePhysics;

	VehicleMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("VehicleMesh"));
	SetRootComponent(VehicleMesh);
	VehicleMesh->SetCollisionProfileName(TEXT("Vehicle"));
	VehicleMesh->SetSimulatePhysics(true);
	VehicleMesh->SetGenerateOverlapEvents(false);

	SeatSystem = CreateDefaultSubobject<USHVehicleSeatSystem>(TEXT("SeatSystem"));
}

/* -----------------------------------------------------------------------
 *  Lifecycle
 * --------------------------------------------------------------------- */

void ASHVehicleBase::BeginPlay()
{
	Super::BeginPlay();

	// If no zones were configured in the editor, populate defaults
	if (DamageZones.Num() == 0)
	{
		InitializeDefaultZones();
	}

	// Ensure all zones start at full health
	for (FSHVehicleDamageZone& Zone : DamageZones)
	{
		Zone.CurrentHealth = Zone.MaxHealth;
		Zone.bIsDisabled = false;
	}

	MovementState = ESHVehicleMovementState::Operational;
}

void ASHVehicleBase::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (bFuelFireActive)
	{
		TickFuelFire(DeltaTime);
	}
}

/* -----------------------------------------------------------------------
 *  Damage
 * --------------------------------------------------------------------- */

float ASHVehicleBase::TakeDamage(
	float DamageAmount,
	const FDamageEvent& DamageEvent,
	AController* EventInstigator,
	AActor* DamageCauser)
{
	if (MovementState == ESHVehicleMovementState::Destroyed || DamageAmount <= 0.0f)
	{
		return 0.0f;
	}

	float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// Determine which zone was hit via the damage event hit info
	FName HitZone = SHVehicleZones::Hull; // default fallback

	if (DamageEvent.IsOfType(FPointDamageEvent::ClassID))
	{
		const FPointDamageEvent& PointDmg = static_cast<const FPointDamageEvent&>(DamageEvent);
		HitZone = ResolveZoneFromHit(PointDmg.HitInfo);
	}
	else if (DamageEvent.IsOfType(FRadialDamageEvent::ClassID))
	{
		// Radial damage hits the hull
		HitZone = SHVehicleZones::Hull;
	}

	// Default penetration rating of 0 for generic damage events
	ApplyDamageToZone(HitZone, ActualDamage, /*PenetrationRating=*/ 0);

	return ActualDamage;
}

void ASHVehicleBase::ApplyDamageToZone(FName Zone, float Damage, int32 PenetrationRating)
{
	if (MovementState == ESHVehicleMovementState::Destroyed || Damage <= 0.0f)
	{
		return;
	}

	FSHVehicleDamageZone* TargetZone = FindZone(Zone);
	if (!TargetZone)
	{
		SH_WARNING(LogSH_Vehicle, "ApplyDamageToZone: Zone '%s' not found on vehicle '%s'",
			*Zone.ToString(), *GetName());
		// Fall through to Hull
		TargetZone = FindZone(SHVehicleZones::Hull);
		if (!TargetZone)
		{
			return;
		}
	}

	if (TargetZone->bIsDisabled)
	{
		// Zone already destroyed — redistribute to hull
		if (Zone != SHVehicleZones::Hull)
		{
			FSHVehicleDamageZone* HullZone = FindZone(SHVehicleZones::Hull);
			if (HullZone && !HullZone->bIsDisabled)
			{
				TargetZone = HullZone;
			}
			else
			{
				return; // Everything is gone
			}
		}
		else
		{
			return;
		}
	}

	// Armor penetration check
	float EffectiveDamage = Damage * TargetZone->DamageMultiplier;

	if (PenetrationRating < TargetZone->ArmorRating)
	{
		// Under-penetrating round: severe damage reduction
		const float ArmorDiff = static_cast<float>(TargetZone->ArmorRating - PenetrationRating);
		const float Reduction = FMath::Clamp(ArmorDiff / static_cast<float>(TargetZone->ArmorRating), 0.0f, 0.95f);
		EffectiveDamage *= (1.0f - Reduction);
	}

	TargetZone->CurrentHealth = FMath::Max(TargetZone->CurrentHealth - EffectiveDamage, 0.0f);

	SH_LOG(LogSH_Vehicle, Verbose, "Vehicle '%s' zone '%s' took %.1f damage (%.1f remaining)",
		*GetName(), *TargetZone->ZoneName.ToString(), EffectiveDamage, TargetZone->CurrentHealth);

	if (TargetZone->CurrentHealth <= 0.0f && !TargetZone->bIsDisabled)
	{
		TargetZone->bIsDisabled = true;
		HandleZoneDisabled(TargetZone->ZoneName);
	}

	RecalculateMovementState();
}

/* -----------------------------------------------------------------------
 *  Queries
 * --------------------------------------------------------------------- */

float ASHVehicleBase::GetZoneHealth(FName Zone) const
{
	const FSHVehicleDamageZone* Z = FindZone(Zone);
	return Z ? Z->CurrentHealth : 0.0f;
}

float ASHVehicleBase::GetZoneHealthPercent(FName Zone) const
{
	const FSHVehicleDamageZone* Z = FindZone(Zone);
	if (!Z || Z->MaxHealth <= 0.0f)
	{
		return 0.0f;
	}
	return FMath::Clamp(Z->CurrentHealth / Z->MaxHealth, 0.0f, 1.0f);
}

float ASHVehicleBase::GetOverallHealthPercent() const
{
	if (DamageZones.Num() == 0)
	{
		return 0.0f;
	}

	float TotalCurrent = 0.0f;
	float TotalMax = 0.0f;

	for (const FSHVehicleDamageZone& Zone : DamageZones)
	{
		TotalCurrent += Zone.CurrentHealth;
		TotalMax += Zone.MaxHealth;
	}

	return (TotalMax > 0.0f) ? FMath::Clamp(TotalCurrent / TotalMax, 0.0f, 1.0f) : 0.0f;
}

/* -----------------------------------------------------------------------
 *  Zone resolution
 * --------------------------------------------------------------------- */

FName ASHVehicleBase::ResolveZoneFromHit(const FHitResult& HitResult) const
{
	// Use the bone name from the hit result to determine which damage zone was struck.
	// Each zone has an AttachedBone that maps to a skeletal mesh bone or socket.
	if (HitResult.BoneName != NAME_None)
	{
		for (const FSHVehicleDamageZone& Zone : DamageZones)
		{
			if (Zone.AttachedBone == HitResult.BoneName)
			{
				return Zone.ZoneName;
			}
		}

		// Bone hit but not mapped — check if it contains a known zone keyword
		const FString BoneStr = HitResult.BoneName.ToString().ToLower();
		if (BoneStr.Contains(TEXT("engine")))   return SHVehicleZones::Engine;
		if (BoneStr.Contains(TEXT("wheel")) ||
			BoneStr.Contains(TEXT("track")))     return SHVehicleZones::Wheels;
		if (BoneStr.Contains(TEXT("turret")))    return SHVehicleZones::Turret;
		if (BoneStr.Contains(TEXT("fuel")))      return SHVehicleZones::Fuel;
	}

	// If nothing matched, do a closest-bone proximity check
	if (VehicleMesh && VehicleMesh->GetSkeletalMeshAsset())
	{
		const FVector ImpactPoint = HitResult.ImpactPoint;
		FName ClosestZone = SHVehicleZones::Hull;
		float ClosestDistSq = MAX_FLT;

		for (const FSHVehicleDamageZone& Zone : DamageZones)
		{
			if (Zone.AttachedBone == NAME_None)
			{
				continue;
			}

			const FVector BoneLoc = VehicleMesh->GetSocketLocation(Zone.AttachedBone);
			const float DistSq = FVector::DistSquared(ImpactPoint, BoneLoc);
			if (DistSq < ClosestDistSq)
			{
				ClosestDistSq = DistSq;
				ClosestZone = Zone.ZoneName;
			}
		}

		return ClosestZone;
	}

	return SHVehicleZones::Hull;
}

FSHVehicleDamageZone* ASHVehicleBase::FindZone(FName ZoneName)
{
	for (FSHVehicleDamageZone& Zone : DamageZones)
	{
		if (Zone.ZoneName == ZoneName)
		{
			return &Zone;
		}
	}
	return nullptr;
}

const FSHVehicleDamageZone* ASHVehicleBase::FindZone(FName ZoneName) const
{
	for (const FSHVehicleDamageZone& Zone : DamageZones)
	{
		if (Zone.ZoneName == ZoneName)
		{
			return &Zone;
		}
	}
	return nullptr;
}

/* -----------------------------------------------------------------------
 *  State Management
 * --------------------------------------------------------------------- */

void ASHVehicleBase::RecalculateMovementState()
{
	if (MovementState == ESHVehicleMovementState::Destroyed)
	{
		return;
	}

	// Check for total destruction: hull disabled
	const FSHVehicleDamageZone* HullZone = FindZone(SHVehicleZones::Hull);
	if (HullZone && HullZone->bIsDisabled)
	{
		BeginDestructionSequence();
		return;
	}

	// Check for immobilization: engine or wheels/tracks disabled
	const FSHVehicleDamageZone* EngineZone = FindZone(SHVehicleZones::Engine);
	const FSHVehicleDamageZone* WheelsZone = FindZone(SHVehicleZones::Wheels);

	const bool bEngineDisabled = EngineZone && EngineZone->bIsDisabled;
	const bool bWheelsDisabled = WheelsZone && WheelsZone->bIsDisabled;

	if (bEngineDisabled || bWheelsDisabled)
	{
		if (MovementState != ESHVehicleMovementState::Immobilized)
		{
			MovementState = ESHVehicleMovementState::Immobilized;
			SH_LOG(LogSH_Vehicle, Log, "Vehicle '%s' is now IMMOBILIZED", *GetName());
		}
		return;
	}

	// Check for damaged state based on overall health threshold
	const float OverallPercent = GetOverallHealthPercent();
	if (OverallPercent <= DamagedThresholdPercent)
	{
		if (MovementState != ESHVehicleMovementState::Damaged)
		{
			MovementState = ESHVehicleMovementState::Damaged;
			SH_LOG(LogSH_Vehicle, Log, "Vehicle '%s' is now DAMAGED (%.0f%% health)", *GetName(), OverallPercent * 100.0f);
		}
		return;
	}

	// Otherwise operational
	if (MovementState != ESHVehicleMovementState::Operational)
	{
		MovementState = ESHVehicleMovementState::Operational;
	}
}

void ASHVehicleBase::HandleZoneDisabled(FName ZoneName)
{
	SH_LOG(LogSH_Vehicle, Warning, "Vehicle '%s' zone '%s' DISABLED", *GetName(), *ZoneName.ToString());

	OnZoneDisabled.Broadcast(this, ZoneName);

	if (ZoneName == SHVehicleZones::Fuel)
	{
		StartFuelFire();
	}
}

/* -----------------------------------------------------------------------
 *  Destruction
 * --------------------------------------------------------------------- */

void ASHVehicleBase::BeginDestructionSequence()
{
	if (MovementState == ESHVehicleMovementState::Destroyed)
	{
		return;
	}

	MovementState = ESHVehicleMovementState::Destroyed;

	SH_LOG(LogSH_Vehicle, Warning, "Vehicle '%s' DESTROYED", *GetName());

	// Stop fuel fire if active (we're blowing up now)
	bFuelFireActive = false;

	// Spawn explosion VFX
	if (UNiagaraSystem* ExplosionNS = DestructionExplosionVFX.LoadSynchronous())
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), ExplosionNS, GetActorLocation(), GetActorRotation());
	}

	// Play explosion sound
	if (USoundBase* ExpSound = DestructionExplosionSound.LoadSynchronous())
	{
		UGameplayStatics::PlaySoundAtLocation(this, ExpSound, GetActorLocation());
	}

	// Spawn persistent burning fire on the wreck
	if (UNiagaraSystem* FireNS = BurningFireVFX.LoadSynchronous())
	{
		ActiveFireVFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
			FireNS, VehicleMesh, NAME_None,
			FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset, true);
	}

	// Play looping burn sound
	if (USoundBase* BurnSound = BurningLoopSound.LoadSynchronous())
	{
		UGameplayStatics::SpawnSoundAtLocation(this, BurnSound, GetActorLocation());
	}

	// Eject all occupants
	if (SeatSystem)
	{
		// Iterate over all seat indices and eject occupants
		TArray<ACharacter*> Occupants;
		for (int32 i = 0; i < 6; ++i)
		{
			ACharacter* Occupant = SeatSystem->GetOccupant(i);
			if (Occupant)
			{
				Occupants.Add(Occupant);
			}
		}
		for (ACharacter* Occupant : Occupants)
		{
			SeatSystem->ExitVehicle(Occupant);
		}
	}

	// Disable physics-driven movement
	if (VehicleMesh)
	{
		VehicleMesh->SetSimulatePhysics(false);
	}

	// Swap to wreck mesh if available
	if (UStaticMesh* Wreck = WreckMesh.LoadSynchronous())
	{
		// Hide the skeletal mesh and spawn a static mesh actor or component
		VehicleMesh->SetVisibility(false);
		VehicleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

		UStaticMeshComponent* WreckComp = NewObject<UStaticMeshComponent>(this, TEXT("WreckMeshComp"));
		if (WreckComp)
		{
			WreckComp->SetStaticMesh(Wreck);
			WreckComp->SetWorldTransform(VehicleMesh->GetComponentTransform());
			WreckComp->SetCollisionProfileName(TEXT("BlockAll"));
			WreckComp->RegisterComponent();
			WreckComp->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
		}
	}

	OnVehicleDestroyed.Broadcast(this);
}

/* -----------------------------------------------------------------------
 *  Fuel Fire
 * --------------------------------------------------------------------- */

void ASHVehicleBase::StartFuelFire()
{
	if (bFuelFireActive || MovementState == ESHVehicleMovementState::Destroyed)
	{
		return;
	}

	bFuelFireActive = true;
	FuelFireElapsed = 0.0f;

	SH_LOG(LogSH_Vehicle, Warning, "Vehicle '%s' FUEL FIRE started — destruction in %.0fs", *GetName(), FuelFireDestructionTime);

	// Spawn fire VFX attached to the vehicle
	if (UNiagaraSystem* FireNS = BurningFireVFX.LoadSynchronous())
	{
		ActiveFireVFX = UNiagaraFunctionLibrary::SpawnSystemAttached(
			FireNS, VehicleMesh, NAME_None,
			FVector::ZeroVector, FRotator::ZeroRotator,
			EAttachLocation::KeepRelativeOffset, true);
	}

	// Play burning sound
	if (USoundBase* BurnSound = BurningLoopSound.LoadSynchronous())
	{
		UGameplayStatics::SpawnSoundAtLocation(this, BurnSound, GetActorLocation());
	}
}

void ASHVehicleBase::TickFuelFire(float DeltaTime)
{
	if (!bFuelFireActive || MovementState == ESHVehicleMovementState::Destroyed)
	{
		return;
	}

	FuelFireElapsed += DeltaTime;

	// Apply tick damage to hull zone (fire eating through the vehicle)
	const float FireDPS = 10.0f; // damage per second from fire
	FSHVehicleDamageZone* HullZone = FindZone(SHVehicleZones::Hull);
	if (HullZone && !HullZone->bIsDisabled)
	{
		HullZone->CurrentHealth = FMath::Max(HullZone->CurrentHealth - FireDPS * DeltaTime, 0.0f);
		if (HullZone->CurrentHealth <= 0.0f)
		{
			HullZone->bIsDisabled = true;
			BeginDestructionSequence();
			return;
		}
	}

	// Forced destruction after timeout regardless of remaining hull health
	if (FuelFireElapsed >= FuelFireDestructionTime)
	{
		BeginDestructionSequence();
	}
}

/* -----------------------------------------------------------------------
 *  Default Zone Initialization
 * --------------------------------------------------------------------- */

void ASHVehicleBase::InitializeDefaultZones()
{
	DamageZones.Empty();

	// Engine zone
	{
		FSHVehicleDamageZone Zone;
		Zone.ZoneName = SHVehicleZones::Engine;
		Zone.MaxHealth = 150.0f;
		Zone.CurrentHealth = 150.0f;
		Zone.ArmorRating = 2;
		Zone.DamageMultiplier = 1.5f;
		Zone.AttachedBone = TEXT("bone_engine");
		DamageZones.Add(Zone);
	}

	// Wheels / Tracks
	{
		FSHVehicleDamageZone Zone;
		Zone.ZoneName = SHVehicleZones::Wheels;
		Zone.MaxHealth = 80.0f;
		Zone.CurrentHealth = 80.0f;
		Zone.ArmorRating = 0;
		Zone.DamageMultiplier = 1.0f;
		Zone.AttachedBone = TEXT("bone_wheel_fl");
		DamageZones.Add(Zone);
	}

	// Turret
	{
		FSHVehicleDamageZone Zone;
		Zone.ZoneName = SHVehicleZones::Turret;
		Zone.MaxHealth = 200.0f;
		Zone.CurrentHealth = 200.0f;
		Zone.ArmorRating = 4;
		Zone.DamageMultiplier = 1.0f;
		Zone.AttachedBone = TEXT("bone_turret");
		DamageZones.Add(Zone);
	}

	// Hull (primary structural zone)
	{
		FSHVehicleDamageZone Zone;
		Zone.ZoneName = SHVehicleZones::Hull;
		Zone.MaxHealth = 500.0f;
		Zone.CurrentHealth = 500.0f;
		Zone.ArmorRating = 3;
		Zone.DamageMultiplier = 1.0f;
		Zone.AttachedBone = TEXT("bone_hull");
		DamageZones.Add(Zone);
	}

	// Fuel
	{
		FSHVehicleDamageZone Zone;
		Zone.ZoneName = SHVehicleZones::Fuel;
		Zone.MaxHealth = 60.0f;
		Zone.CurrentHealth = 60.0f;
		Zone.ArmorRating = 1;
		Zone.DamageMultiplier = 2.0f;
		Zone.AttachedBone = TEXT("bone_fuel");
		DamageZones.Add(Zone);
	}

	// Adjust defaults per vehicle type
	switch (VehicleType)
	{
	case ESHVehicleType::APC:
		if (FSHVehicleDamageZone* Hull = FindZone(SHVehicleZones::Hull))
		{
			Hull->MaxHealth = 800.0f;
			Hull->CurrentHealth = 800.0f;
			Hull->ArmorRating = 6;
		}
		if (FSHVehicleDamageZone* Engine = FindZone(SHVehicleZones::Engine))
		{
			Engine->ArmorRating = 4;
		}
		break;

	case ESHVehicleType::MRAP:
		if (FSHVehicleDamageZone* Hull = FindZone(SHVehicleZones::Hull))
		{
			Hull->MaxHealth = 650.0f;
			Hull->CurrentHealth = 650.0f;
			Hull->ArmorRating = 5;
		}
		break;

	case ESHVehicleType::Technical:
		if (FSHVehicleDamageZone* Hull = FindZone(SHVehicleZones::Hull))
		{
			Hull->MaxHealth = 300.0f;
			Hull->CurrentHealth = 300.0f;
			Hull->ArmorRating = 1;
		}
		break;

	case ESHVehicleType::StaticEmplacement:
		// Remove wheels/engine — static emplacements don't move
		DamageZones.RemoveAll([](const FSHVehicleDamageZone& Z)
		{
			return Z.ZoneName == SHVehicleZones::Engine || Z.ZoneName == SHVehicleZones::Wheels;
		});
		break;

	case ESHVehicleType::BoatLanding:
		// Rename Wheels to Hull_Lower for watercraft
		if (FSHVehicleDamageZone* Wheels = FindZone(SHVehicleZones::Wheels))
		{
			Wheels->ZoneName = TEXT("Hull_Lower");
			Wheels->MaxHealth = 120.0f;
			Wheels->CurrentHealth = 120.0f;
		}
		break;
	}
}
