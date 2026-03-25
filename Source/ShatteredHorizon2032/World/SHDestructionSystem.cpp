// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHDestructionSystem.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Components/PrimitiveComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

DEFINE_LOG_CATEGORY(LogSH_Destruction);

// ======================================================================
//  Construction
// ======================================================================

USHDestructionSystem::USHDestructionSystem()
{
}

// ======================================================================
//  USubsystem interface
// ======================================================================

void USHDestructionSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogSH_Destruction, Log, TEXT("DestructionSystem initialised. MaxDebris=%d, MaxCraters=%d"),
		MaxActiveDebris, MaxCraters);
}

void USHDestructionSystem::Deinitialize()
{
	Destructibles.Empty();
	ActorToIDMap.Empty();
	BuildingIntegrityMap.Empty();
	Craters.Empty();
	ActiveDebris.Empty();
	Super::Deinitialize();
}

void USHDestructionSystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Update cached player position
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			if (APawn* Pawn = PC->GetPawn())
			{
				CachedPlayerLocation = Pawn->GetActorLocation();
			}
		}
	}

	TickDebrisCleanup(DeltaTime);

	PerfManagementAccumulator += DeltaTime;
	if (PerfManagementAccumulator >= PerfManagementInterval)
	{
		PerfManagementAccumulator = 0.f;
		TickPerformanceManagement(DeltaTime);
	}
}

TStatId USHDestructionSystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USHDestructionSystem, STATGROUP_Tickables);
}

// ======================================================================
//  Registration
// ======================================================================

int32 USHDestructionSystem::RegisterDestructible(AActor* Actor, ESHDestructibleType Type, float MaxHealth,
												  bool bIsStructuralSupport, int32 BuildingID)
{
	if (!Actor) return -1;

	// Check if already registered
	if (const int32* ExistingID = ActorToIDMap.Find(Actor))
	{
		return *ExistingID;
	}

	int32 ID = NextDestructibleID++;

	FSHDestructibleState State;
	State.DestructibleID = ID;
	State.Actor = Actor;
	State.Type = Type;
	State.MaxHealth = (MaxHealth > 0.f) ? MaxHealth : GetDefaultMaxHealth(Type);
	State.CurrentHealth = State.MaxHealth;
	State.DestructionLevel = ESHDestructionLevel::Pristine;
	State.bIsStructuralSupport = bIsStructuralSupport;
	State.BuildingID = BuildingID;

	Destructibles.Add(ID, State);
	ActorToIDMap.Add(Actor, ID);

	// Register with building integrity tracking
	if (BuildingID >= 0 && bIsStructuralSupport)
	{
		FSHBuildingIntegrity& Building = BuildingIntegrityMap.FindOrAdd(BuildingID);
		Building.BuildingID = BuildingID;
		Building.TotalSupports++;
		Building.RemainingSupports++;
	}

	UE_LOG(LogSH_Destruction, Verbose, TEXT("Registered destructible: ID=%d, Type=%d, HP=%.0f, Building=%d"),
		ID, static_cast<int32>(Type), State.MaxHealth, BuildingID);

	return ID;
}

void USHDestructionSystem::UnregisterDestructible(int32 DestructibleID)
{
	FSHDestructibleState* State = Destructibles.Find(DestructibleID);
	if (!State) return;

	if (State->Actor.IsValid())
	{
		ActorToIDMap.Remove(State->Actor);
	}

	Destructibles.Remove(DestructibleID);
}

// ======================================================================
//  Damage
// ======================================================================

float USHDestructionSystem::ApplyDamage(int32 DestructibleID, float DamageAmount, FVector ImpactPoint,
										 FVector ImpactNormal, AActor* DamageCauser)
{
	FSHDestructibleState* State = Destructibles.Find(DestructibleID);
	if (!State) return -1.f;

	if (State->DestructionLevel == ESHDestructionLevel::Destroyed) return 0.f;

	float PreviousHealth = State->CurrentHealth;
	State->CurrentHealth = FMath::Max(0.f, State->CurrentHealth - DamageAmount);
	State->LastDamageTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	ESHDestructionLevel PreviousLevel = State->DestructionLevel;
	State->DestructionLevel = ComputeDestructionLevel(State->GetHealthFraction());

	// Transition effects
	if (State->DestructionLevel != PreviousLevel)
	{
		UE_LOG(LogSH_Destruction, Log, TEXT("Destructible %d transitioned to damage level %d (HP: %.0f/%.0f)"),
			DestructibleID, static_cast<int32>(State->DestructionLevel), State->CurrentHealth, State->MaxHealth);

		if (State->Actor.IsValid())
		{
			ActivateChaosDestruction(State->Actor.Get(), State->DestructionLevel, ImpactPoint, -ImpactNormal);
		}

		if (State->DestructionLevel == ESHDestructionLevel::Destroyed)
		{
			// Spawn debris
			SpawnDebrisForDestructible(*State, ImpactPoint);

			// Update structural integrity
			if (State->bIsStructuralSupport && State->BuildingID >= 0)
			{
				FSHBuildingIntegrity* Building = BuildingIntegrityMap.Find(State->BuildingID);
				if (Building)
				{
					Building->RemainingSupports = FMath::Max(0, Building->RemainingSupports - 1);
					UpdateStructuralIntegrity();
				}
			}

			OnDestructibleDestroyed.Broadcast(DestructibleID, State->Type);
		}
	}

	return State->CurrentHealth;
}

void USHDestructionSystem::ApplyAreaDamage(FVector Origin, float Radius, float BaseDamage, AActor* DamageCauser)
{
	if (Radius <= 0.f) return;

	float RadiusSq = Radius * Radius;

	for (auto& Pair : Destructibles)
	{
		FSHDestructibleState& State = Pair.Value;
		if (!State.Actor.IsValid()) continue;
		if (State.DestructionLevel == ESHDestructionLevel::Destroyed) continue;

		FVector ActorLoc = State.Actor->GetActorLocation();
		float DistSq = FVector::DistSquared(Origin, ActorLoc);

		if (DistSq < RadiusSq)
		{
			float Dist = FMath::Sqrt(DistSq);
			float NormalizedDist = Dist / Radius;

			// Damage falloff — inverse power law
			float DamageFraction = FMath::Pow(FMath::Max(0.f, 1.f - NormalizedDist), AreaDamageFalloffExponent);
			float Damage = BaseDamage * DamageFraction;

			if (Damage > 0.1f)
			{
				FVector ImpactDir = (ActorLoc - Origin).GetSafeNormal();
				ApplyDamage(Pair.Key, Damage, ActorLoc, -ImpactDir, DamageCauser);
			}
		}
	}

	// Create crater at origin
	float CraterRadius = Radius * 0.3f; // Crater is ~30% of blast radius
	float CraterDepth = CraterRadius * 0.25f;
	CreateCrater(Origin, CraterRadius, CraterDepth);
}

float USHDestructionSystem::ApplyDamageToActor(AActor* Actor, float DamageAmount, FVector ImpactPoint,
												FVector ImpactNormal, AActor* DamageCauser)
{
	if (!Actor) return -1.f;

	const int32* ID = ActorToIDMap.Find(Actor);
	if (!ID) return -1.f;

	return ApplyDamage(*ID, DamageAmount, ImpactPoint, ImpactNormal, DamageCauser);
}

// ======================================================================
//  Craters
// ======================================================================

FSHCrater USHDestructionSystem::CreateCrater(FVector Location, float Radius, float Depth)
{
	// Enforce limit — remove oldest
	while (Craters.Num() >= MaxCraters)
	{
		Craters.RemoveAt(0);
	}

	FSHCrater Crater;
	Crater.CraterID = NextCraterID++;
	Crater.Location = Location;
	Crater.Radius = Radius;
	Crater.Depth = Depth;
	Crater.CreationTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	Crater.bProvidesCover = (Radius >= MinCoverCraterRadius && Depth >= 30.f);

	Craters.Add(Crater);

	UE_LOG(LogSH_Destruction, Log, TEXT("Crater created: ID=%d, R=%.0f, D=%.0f at %s (cover=%s)"),
		Crater.CraterID, Radius, Depth, *Location.ToString(), Crater.bProvidesCover ? TEXT("yes") : TEXT("no"));

	// In production: deform terrain mesh, apply crater decal, spawn dust VFX
	OnCraterCreated.Broadcast(Crater);

	return Crater;
}

TArray<FSHCrater> USHDestructionSystem::QueryCratersInRadius(FVector Location, float Radius) const
{
	TArray<FSHCrater> Result;
	float RadiusSq = Radius * Radius;

	for (const FSHCrater& Crater : Craters)
	{
		if (FVector::DistSquared(Location, Crater.Location) < RadiusSq)
		{
			Result.Add(Crater);
		}
	}

	return Result;
}

// ======================================================================
//  Queries
// ======================================================================

bool USHDestructionSystem::GetDestructibleState(int32 DestructibleID, FSHDestructibleState& OutState) const
{
	const FSHDestructibleState* State = Destructibles.Find(DestructibleID);
	if (!State) return false;

	OutState = *State;
	return true;
}

float USHDestructionSystem::GetBuildingIntegrity(int32 BuildingID) const
{
	const FSHBuildingIntegrity* Building = BuildingIntegrityMap.Find(BuildingID);
	if (!Building) return 1.f;
	return Building->GetIntegrity();
}

// ======================================================================
//  Structural integrity
// ======================================================================

void USHDestructionSystem::UpdateStructuralIntegrity()
{
	for (auto& Pair : BuildingIntegrityMap)
	{
		FSHBuildingIntegrity& Building = Pair.Value;
		if (Building.bCollapseTriggered) continue;

		if (Building.GetIntegrity() <= Building.CollapseThreshold)
		{
			TriggerBuildingCollapse(Building.BuildingID);
		}
	}
}

void USHDestructionSystem::TriggerBuildingCollapse(int32 BuildingID)
{
	FSHBuildingIntegrity* Building = BuildingIntegrityMap.Find(BuildingID);
	if (!Building || Building->bCollapseTriggered) return;

	Building->bCollapseTriggered = true;

	UE_LOG(LogSH_Destruction, Warning, TEXT("Building %d COLLAPSE triggered! Integrity=%.2f"),
		BuildingID, Building->GetIntegrity());

	// Destroy all non-destroyed elements of this building
	for (auto& Pair : Destructibles)
	{
		FSHDestructibleState& State = Pair.Value;
		if (State.BuildingID != BuildingID) continue;
		if (State.DestructionLevel == ESHDestructionLevel::Destroyed) continue;

		State.CurrentHealth = 0.f;
		State.DestructionLevel = ESHDestructionLevel::Destroyed;

		if (State.Actor.IsValid())
		{
			FVector Loc = State.Actor->GetActorLocation();
			ActivateChaosDestruction(State.Actor.Get(), ESHDestructionLevel::Destroyed, Loc, FVector::DownVector);
			SpawnDebrisForDestructible(State, Loc);
		}

		OnDestructibleDestroyed.Broadcast(Pair.Key, State.Type);
	}

	OnBuildingCollapse.Broadcast(BuildingID);
}

// ======================================================================
//  Chaos destruction activation
// ======================================================================

void USHDestructionSystem::ActivateChaosDestruction(AActor* Actor, ESHDestructionLevel Level,
													 FVector ImpactPoint, FVector ImpactDirection)
{
	if (!Actor) return;

	// Find Geometry Collection component for Chaos destruction
	UGeometryCollectionComponent* GeoComp = Actor->FindComponentByClass<UGeometryCollectionComponent>();

	if (GeoComp)
	{
		// Apply strain to trigger fracture at damage level thresholds
		float StrainValue = 0.f;
		switch (Level)
		{
		case ESHDestructionLevel::LightDamage:		StrainValue = 100.f; break;
		case ESHDestructionLevel::ModerateDamage:	StrainValue = 500.f; break;
		case ESHDestructionLevel::HeavyDamage:		StrainValue = 2000.f; break;
		case ESHDestructionLevel::Destroyed:		StrainValue = 10000.f; break;
		default: break;
		}

		if (StrainValue > 0.f)
		{
			// Apply internal strain — Chaos will fracture based on pre-authored break patterns
			FVector Impulse = ImpactDirection * StrainValue;
			GeoComp->ApplyExternalStrain(0, FVector::ZeroVector, StrainValue, 0, 0.f, Impulse);

			UE_LOG(LogSH_Destruction, Verbose, TEXT("Chaos strain %.0f applied to %s at %s"),
				StrainValue, *Actor->GetName(), *ImpactPoint.ToString());
		}
	}
	else
	{
		// Fallback for non-Chaos actors — scale or hide mesh to simulate damage
		UStaticMeshComponent* MeshComp = Actor->FindComponentByClass<UStaticMeshComponent>();
		if (MeshComp && Level == ESHDestructionLevel::Destroyed)
		{
			MeshComp->SetVisibility(false);
			MeshComp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}
	}

	// Spawn VFX
	ESHDestructibleType Type = ESHDestructibleType::Generic;
	if (const int32* ID = ActorToIDMap.Find(Actor))
	{
		if (const FSHDestructibleState* State = Destructibles.Find(*ID))
		{
			Type = State->Type;
		}
	}

	if (const TSoftObjectPtr<UNiagaraSystem>* VFXRef = DestructionVFX.Find(Type))
	{
		if (VFXRef->IsValid() || VFXRef->LoadSynchronous())
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				GetWorld(), VFXRef->Get(), ImpactPoint,
				ImpactDirection.Rotation(), FVector::OneVector, true);
		}
	}

	// Play destruction sound
	if (const TSoftObjectPtr<USoundBase>* SoundRef = DestructionSounds.Find(Type))
	{
		if (SoundRef->IsValid() || SoundRef->LoadSynchronous())
		{
			UGameplayStatics::PlaySoundAtLocation(GetWorld(), SoundRef->Get(), ImpactPoint);
		}
	}
}

// ======================================================================
//  Debris management
// ======================================================================

void USHDestructionSystem::SpawnDebrisForDestructible(const FSHDestructibleState& State, FVector ImpactPoint)
{
	// In production, Chaos already generates debris fragments from the Geometry Collection.
	// We register those fragments for tracking/cleanup.

	FSHDebrisInfo Debris;
	Debris.DebrisID = NextDebrisID++;
	Debris.Location = ImpactPoint;
	Debris.CreationTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	Debris.bPhysicsActive = true;

	ActiveDebris.Add(Debris);

	UE_LOG(LogSH_Destruction, Verbose, TEXT("Debris spawned for destructible %d at %s (total: %d)"),
		State.DestructibleID, *ImpactPoint.ToString(), ActiveDebris.Num());
}

void USHDestructionSystem::TickDebrisCleanup(float DeltaTime)
{
	float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	for (int32 i = ActiveDebris.Num() - 1; i >= 0; --i)
	{
		FSHDebrisInfo& Debris = ActiveDebris[i];

		// Update distance
		Debris.DistanceFromPlayer = FVector::Dist(Debris.Location, CachedPlayerLocation);

		// Put distant debris to sleep
		if (Debris.bPhysicsActive && Debris.DistanceFromPlayer > DebrisPhysicsSleepDistance)
		{
			Debris.bPhysicsActive = false;
			if (Debris.Actor.IsValid())
			{
				UPrimitiveComponent* PrimComp = Debris.Actor->FindComponentByClass<UPrimitiveComponent>();
				if (PrimComp)
				{
					PrimComp->PutRigidBodyToSleep();
				}
			}
		}

		// Settle check — disable physics after settle time
		float Age = CurrentTime - Debris.CreationTime;
		if (Debris.bPhysicsActive && Age > DebrisSettleTime)
		{
			Debris.bPhysicsActive = false;
			if (Debris.Actor.IsValid())
			{
				UPrimitiveComponent* PrimComp = Debris.Actor->FindComponentByClass<UPrimitiveComponent>();
				if (PrimComp)
				{
					PrimComp->SetSimulatePhysics(false);
				}
			}
		}

		// Remove old debris that's far from the player
		if (Age > MaxDebrisAge && Debris.DistanceFromPlayer > DebrisPhysicsSleepDistance)
		{
			if (Debris.Actor.IsValid())
			{
				Debris.Actor->Destroy();
			}
			ActiveDebris.RemoveAt(i);
			continue;
		}
	}

	// Hard limit cleanup — remove oldest when over budget
	while (ActiveDebris.Num() > MaxActiveDebris)
	{
		// Remove the oldest debris farthest from player
		int32 BestIdx = 0;
		float BestScore = -1.f;

		for (int32 i = 0; i < ActiveDebris.Num(); ++i)
		{
			// Score: higher = more eligible for removal (old + far away)
			float Age = CurrentTime - ActiveDebris[i].CreationTime;
			float Score = Age * ActiveDebris[i].DistanceFromPlayer;
			if (Score > BestScore)
			{
				BestScore = Score;
				BestIdx = i;
			}
		}

		if (ActiveDebris[BestIdx].Actor.IsValid())
		{
			ActiveDebris[BestIdx].Actor->Destroy();
		}
		ActiveDebris.RemoveAt(BestIdx);
	}
}

// ======================================================================
//  Performance management
// ======================================================================

void USHDestructionSystem::TickPerformanceManagement(float DeltaTime)
{
	// Update distances for all destructibles
	for (auto& Pair : Destructibles)
	{
		FSHDestructibleState& State = Pair.Value;
		if (State.Actor.IsValid())
		{
			State.DistanceFromPlayer = FVector::Dist(State.Actor->GetActorLocation(), CachedPlayerLocation);
		}
	}
}

// ======================================================================
//  Helpers
// ======================================================================

ESHDestructionLevel USHDestructionSystem::ComputeDestructionLevel(float HealthFraction) const
{
	if (HealthFraction <= 0.f)		return ESHDestructionLevel::Destroyed;
	if (HealthFraction <= 0.25f)	return ESHDestructionLevel::HeavyDamage;
	if (HealthFraction <= 0.55f)	return ESHDestructionLevel::ModerateDamage;
	if (HealthFraction <= 0.85f)	return ESHDestructionLevel::LightDamage;
	return ESHDestructionLevel::Pristine;
}

float USHDestructionSystem::GetDefaultMaxHealth(ESHDestructibleType Type) const
{
	switch (Type)
	{
	case ESHDestructibleType::Wall:				return 500.f;
	case ESHDestructibleType::Roof:				return 400.f;
	case ESHDestructibleType::Floor:			return 600.f;
	case ESHDestructibleType::SupportColumn:	return 800.f;
	case ESHDestructibleType::Window:			return 20.f;
	case ESHDestructibleType::Door:				return 100.f;
	case ESHDestructibleType::Sandbag:			return 150.f;
	case ESHDestructibleType::Fortification:	return 350.f;
	case ESHDestructibleType::VehicleWreck:		return 1000.f;
	case ESHDestructibleType::Tree:				return 200.f;
	case ESHDestructibleType::Vegetation:		return 30.f;
	case ESHDestructibleType::Furniture:		return 50.f;
	case ESHDestructibleType::Debris:			return 50.f;
	case ESHDestructibleType::Generic:			return 100.f;
	}

	return 100.f;
}
