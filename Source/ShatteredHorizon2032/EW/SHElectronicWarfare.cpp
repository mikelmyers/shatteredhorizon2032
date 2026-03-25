// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHElectronicWarfare.h"
#include "ShatteredHorizon2032/ShatteredHorizon2032.h"
#include "ShatteredHorizon2032/Drones/SHDroneBase.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

TWeakObjectPtr<ASHElectronicWarfare> ASHElectronicWarfare::Instance = nullptr;

ASHElectronicWarfare::ASHElectronicWarfare()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	bReplicates = true;

	FriendlyEWSupportCalls = MaxFriendlyEWSupportCalls;
}

void ASHElectronicWarfare::BeginPlay()
{
	Super::BeginPlay();

	Instance = this;
	FriendlyEWSupportCalls = MaxFriendlyEWSupportCalls;
	EscalationLevel = 0;
	EscalationTimer = 0.f;
	GlobalEWStrengthMultiplier = 1.f;

	SH_LOG(LogSH_EW, Log, "Electronic Warfare Manager initialized. MaxEscalation: %d, FriendlySupport: %d",
		MaxEscalationLevel, MaxFriendlyEWSupportCalls);
}

void ASHElectronicWarfare::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	TickZoneDurations(DeltaSeconds);
	TickEscalation(DeltaSeconds);
	TickDroneEWEffects(DeltaSeconds);
	TickActorZoneTracking(DeltaSeconds);
}

// =======================================================================
//  Singleton
// =======================================================================

ASHElectronicWarfare* ASHElectronicWarfare::GetEWManager(const UObject* WorldContextObject)
{
	if (Instance.IsValid())
	{
		return Instance.Get();
	}

	// Fallback: search the world
	if (WorldContextObject)
	{
		if (const UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::ReturnNull))
		{
			for (TActorIterator<ASHElectronicWarfare> It(World); It; ++It)
			{
				Instance = *It;
				return *It;
			}
		}
	}

	return nullptr;
}

// =======================================================================
//  Zone management
// =======================================================================

FName ASHElectronicWarfare::CreateEWZone(const FVector& Center, float Radius, float Strength,
	ESHEWEffectType EffectType, bool bFriendly, float Duration)
{
	FName NewID = GenerateZoneID();

	FSHEWZone Zone;
	Zone.ZoneID = NewID;
	Zone.Center = Center;
	Zone.Radius = Radius;
	Zone.Strength = FMath::Clamp(Strength * GlobalEWStrengthMultiplier, 0.f, 1.f);
	Zone.EffectType = EffectType;
	Zone.bFriendly = bFriendly;
	Zone.bActive = true;
	Zone.Duration = Duration;
	Zone.ElapsedTime = 0.f;

	EWZones.Add(Zone);

	OnEWZoneActivated.Broadcast(NewID, EffectType);

	SH_LOG(LogSH_EW, Log, "EW Zone created: %s — Type: %d, Center: %s, Radius: %.0f, Strength: %.2f, Friendly: %s",
		*NewID.ToString(), static_cast<int32>(EffectType), *Center.ToString(), Radius, Zone.Strength,
		bFriendly ? TEXT("Yes") : TEXT("No"));

	return NewID;
}

void ASHElectronicWarfare::RemoveEWZone(const FName& ZoneID)
{
	for (int32 i = EWZones.Num() - 1; i >= 0; --i)
	{
		if (EWZones[i].ZoneID == ZoneID)
		{
			OnEWZoneDeactivated.Broadcast(ZoneID);
			EWZones.RemoveAt(i);

			SH_LOG(LogSH_EW, Log, "EW Zone removed: %s", *ZoneID.ToString());
			return;
		}
	}
}

void ASHElectronicWarfare::SetZoneActive(const FName& ZoneID, bool bActive)
{
	for (FSHEWZone& Zone : EWZones)
	{
		if (Zone.ZoneID == ZoneID)
		{
			const bool bWasActive = Zone.bActive;
			Zone.bActive = bActive;

			if (bActive && !bWasActive)
			{
				OnEWZoneActivated.Broadcast(ZoneID, Zone.EffectType);
			}
			else if (!bActive && bWasActive)
			{
				OnEWZoneDeactivated.Broadcast(ZoneID);
			}
			return;
		}
	}
}

void ASHElectronicWarfare::ModifyZone(const FName& ZoneID, float NewStrength, float NewRadius)
{
	for (FSHEWZone& Zone : EWZones)
	{
		if (Zone.ZoneID == ZoneID)
		{
			Zone.Strength = FMath::Clamp(NewStrength, 0.f, 1.f);
			Zone.Radius = FMath::Max(NewRadius, 100.f);
			return;
		}
	}
}

int32 ASHElectronicWarfare::GetActiveEnemyZoneCount() const
{
	int32 Count = 0;
	for (const FSHEWZone& Zone : EWZones)
	{
		if (Zone.bActive && !Zone.bFriendly)
		{
			Count++;
		}
	}
	return Count;
}

// =======================================================================
//  Queries
// =======================================================================

FSHEWQueryResult ASHElectronicWarfare::QueryEWAtPosition(const FVector& Position) const
{
	FSHEWQueryResult Result;

	for (const FSHEWZone& Zone : EWZones)
	{
		if (!Zone.bActive)
		{
			continue;
		}

		const float ZoneStrength = Zone.GetStrengthAtPosition(Position);
		if (ZoneStrength <= 0.f)
		{
			continue;
		}

		Result.bAnyEffectActive = true;
		Result.AffectingZoneIDs.Add(Zone.ZoneID);

		switch (Zone.EffectType)
		{
		case ESHEWEffectType::GPSDenial:
			Result.GPSDenialStrength = FMath::Max(Result.GPSDenialStrength, ZoneStrength);
			break;

		case ESHEWEffectType::CommsJamming:
			Result.CommsJammingStrength = FMath::Max(Result.CommsJammingStrength, ZoneStrength);
			break;

		case ESHEWEffectType::RadarDisruption:
			Result.RadarDisruptionStrength = FMath::Max(Result.RadarDisruptionStrength, ZoneStrength);
			break;

		case ESHEWEffectType::DroneJamming:
			Result.DroneJammingStrength = FMath::Max(Result.DroneJammingStrength, ZoneStrength);
			break;

		case ESHEWEffectType::FullSpectrum:
			Result.GPSDenialStrength = FMath::Max(Result.GPSDenialStrength, ZoneStrength);
			Result.CommsJammingStrength = FMath::Max(Result.CommsJammingStrength, ZoneStrength);
			Result.RadarDisruptionStrength = FMath::Max(Result.RadarDisruptionStrength, ZoneStrength);
			Result.DroneJammingStrength = FMath::Max(Result.DroneJammingStrength, ZoneStrength);
			break;

		default:
			break;
		}
	}

	Result.MaxEffectStrength = FMath::Max({
		Result.GPSDenialStrength,
		Result.CommsJammingStrength,
		Result.RadarDisruptionStrength,
		Result.DroneJammingStrength});

	return Result;
}

float ASHElectronicWarfare::GetGPSDenialAtPosition(const FVector& Position) const
{
	float MaxStrength = 0.f;
	for (const FSHEWZone& Zone : EWZones)
	{
		if (!Zone.bActive) continue;
		if (Zone.EffectType != ESHEWEffectType::GPSDenial && Zone.EffectType != ESHEWEffectType::FullSpectrum) continue;
		MaxStrength = FMath::Max(MaxStrength, Zone.GetStrengthAtPosition(Position));
	}
	return MaxStrength;
}

float ASHElectronicWarfare::GetCommsJammingAtPosition(const FVector& Position) const
{
	float MaxStrength = 0.f;
	for (const FSHEWZone& Zone : EWZones)
	{
		if (!Zone.bActive) continue;
		if (Zone.EffectType != ESHEWEffectType::CommsJamming && Zone.EffectType != ESHEWEffectType::FullSpectrum) continue;
		MaxStrength = FMath::Max(MaxStrength, Zone.GetStrengthAtPosition(Position));
	}
	return MaxStrength;
}

float ASHElectronicWarfare::GetRadarDisruptionAtPosition(const FVector& Position) const
{
	float MaxStrength = 0.f;
	for (const FSHEWZone& Zone : EWZones)
	{
		if (!Zone.bActive) continue;
		if (Zone.EffectType != ESHEWEffectType::RadarDisruption && Zone.EffectType != ESHEWEffectType::FullSpectrum) continue;
		MaxStrength = FMath::Max(MaxStrength, Zone.GetStrengthAtPosition(Position));
	}
	return MaxStrength;
}

float ASHElectronicWarfare::GetDroneJammingAtPosition(const FVector& Position) const
{
	float MaxStrength = 0.f;
	for (const FSHEWZone& Zone : EWZones)
	{
		if (!Zone.bActive) continue;
		if (Zone.EffectType != ESHEWEffectType::DroneJamming && Zone.EffectType != ESHEWEffectType::FullSpectrum) continue;
		MaxStrength = FMath::Max(MaxStrength, Zone.GetStrengthAtPosition(Position));
	}
	return MaxStrength;
}

bool ASHElectronicWarfare::IsPositionInEWZone(const FVector& Position) const
{
	for (const FSHEWZone& Zone : EWZones)
	{
		if (Zone.bActive && Zone.GetStrengthAtPosition(Position) > 0.f)
		{
			return true;
		}
	}
	return false;
}

bool ASHElectronicWarfare::IsPositionInFriendlyEW(const FVector& Position) const
{
	for (const FSHEWZone& Zone : EWZones)
	{
		if (Zone.bActive && Zone.bFriendly && Zone.GetStrengthAtPosition(Position) > 0.f)
		{
			return true;
		}
	}
	return false;
}

// =======================================================================
//  Friendly EW Support
// =======================================================================

bool ASHElectronicWarfare::RequestFriendlyEWSupport(const FVector& Position, float Radius,
	ESHEWEffectType EffectType, float Duration)
{
	if (FriendlyEWSupportCalls <= 0)
	{
		SH_WARNING(LogSH_EW, "Friendly EW support denied — no remaining calls.");
		return false;
	}

	FriendlyEWSupportCalls--;

	const float EffectiveRadius = (Radius > 0.f) ? Radius : FriendlyEWSupportRadius;
	const float EffectiveDuration = (Duration > 0.f) ? Duration : FriendlyEWSupportDuration;

	CreateEWZone(Position, EffectiveRadius, 0.9f, EffectType, true, EffectiveDuration);

	SH_LOG(LogSH_EW, Log, "Friendly EW support deployed at %s. Remaining calls: %d",
		*Position.ToString(), FriendlyEWSupportCalls);

	return true;
}

// =======================================================================
//  Escalation
// =======================================================================

void ASHElectronicWarfare::ForceEscalation(int32 NewLevel)
{
	NewLevel = FMath::Clamp(NewLevel, 0, MaxEscalationLevel);

	// Spawn zones for levels we're skipping
	const int32 LevelsToAdd = NewLevel - EscalationLevel;
	for (int32 i = 0; i < LevelsToAdd; ++i)
	{
		for (int32 z = 0; z < ZonesPerEscalation; ++z)
		{
			SpawnEscalationZone();
		}
	}

	EscalationLevel = NewLevel;
	GlobalEWStrengthMultiplier = 1.f + (EscalationLevel * StrengthPerEscalation);

	// Update existing enemy zone strengths
	for (FSHEWZone& Zone : EWZones)
	{
		if (!Zone.bFriendly)
		{
			Zone.Strength = FMath::Min(Zone.Strength * (1.f + StrengthPerEscalation), 1.f);
		}
	}

	OnEWEscalation.Broadcast(EscalationLevel, GlobalEWStrengthMultiplier);

	SH_LOG(LogSH_EW, Log, "EW Escalation forced to level %d. Global strength: %.2f",
		EscalationLevel, GlobalEWStrengthMultiplier);
}

// =======================================================================
//  Visual / Audio indicators (static helpers)
// =======================================================================

float ASHElectronicWarfare::ComputeCompassDrift(float GPSDenialStrength)
{
	// Compass drift: 0 at no denial, up to +-45 degrees at full denial
	// Random but biased by strength
	const float MaxDrift = 45.f;
	return GPSDenialStrength * MaxDrift;
}

float ASHElectronicWarfare::ComputeRadioStaticIntensity(float CommsJammingStrength)
{
	// Radio static: linear with jamming strength
	// 0 = clear, 0.3 = light static, 0.7 = heavy static, 1.0 = total whiteout
	return FMath::Clamp(CommsJammingStrength, 0.f, 1.f);
}

// =======================================================================
//  Tick helpers
// =======================================================================

void ASHElectronicWarfare::TickZoneDurations(float DeltaSeconds)
{
	for (int32 i = EWZones.Num() - 1; i >= 0; --i)
	{
		FSHEWZone& Zone = EWZones[i];
		if (!Zone.bActive || Zone.Duration < 0.f)
		{
			continue; // Permanent zone
		}

		Zone.ElapsedTime += DeltaSeconds;
		if (Zone.ElapsedTime >= Zone.Duration)
		{
			SH_LOG(LogSH_EW, Log, "EW Zone expired: %s", *Zone.ZoneID.ToString());
			OnEWZoneDeactivated.Broadcast(Zone.ZoneID);
			EWZones.RemoveAt(i);
		}
	}
}

void ASHElectronicWarfare::TickEscalation(float DeltaSeconds)
{
	if (EscalationLevel >= MaxEscalationLevel)
	{
		return;
	}

	EscalationTimer += DeltaSeconds;
	if (EscalationTimer >= EscalationInterval)
	{
		EscalationTimer -= EscalationInterval;
		EscalationLevel++;
		GlobalEWStrengthMultiplier = 1.f + (EscalationLevel * StrengthPerEscalation);

		// Spawn new enemy EW zones
		for (int32 i = 0; i < ZonesPerEscalation; ++i)
		{
			SpawnEscalationZone();
		}

		// Boost existing enemy zones
		for (FSHEWZone& Zone : EWZones)
		{
			if (!Zone.bFriendly)
			{
				Zone.Strength = FMath::Min(Zone.Strength + StrengthPerEscalation * 0.5f, 1.f);
			}
		}

		OnEWEscalation.Broadcast(EscalationLevel, GlobalEWStrengthMultiplier);

		SH_LOG(LogSH_EW, Log, "EW Escalation to level %d. Global strength: %.2f. Active enemy zones: %d",
			EscalationLevel, GlobalEWStrengthMultiplier, GetActiveEnemyZoneCount());
	}
}

void ASHElectronicWarfare::TickDroneEWEffects(float DeltaSeconds)
{
	DroneEWTickAccumulator += DeltaSeconds;
	if (DroneEWTickAccumulator < DroneEWTickInterval)
	{
		return;
	}
	DroneEWTickAccumulator -= DroneEWTickInterval;

	ApplyEWToDrones();
}

void ASHElectronicWarfare::TickActorZoneTracking(float DeltaSeconds)
{
	// Track enter/exit events at a lower frequency
	ActorTrackingAccumulator += DeltaSeconds;
	if (ActorTrackingAccumulator < 0.5f)
	{
		return;
	}
	ActorTrackingAccumulator -= 0.5f;

	// Track all player pawns
	TArray<AActor*> PlayerPawns;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APawn::StaticClass(), PlayerPawns);

	for (AActor* Actor : PlayerPawns)
	{
		if (!Actor) continue;

		const APawn* Pawn = Cast<APawn>(Actor);
		if (!Pawn || !Pawn->IsPlayerControlled()) continue;

		TWeakObjectPtr<AActor> WeakActor(Actor);
		TSet<FName>& CurrentZones = ActorZoneMap.FindOrAdd(WeakActor);
		TSet<FName> NewZones;

		// Check which zones this actor is in
		for (const FSHEWZone& Zone : EWZones)
		{
			if (!Zone.bActive) continue;
			if (Zone.GetStrengthAtPosition(Actor->GetActorLocation()) > 0.f)
			{
				NewZones.Add(Zone.ZoneID);

				// Check for enter event
				if (!CurrentZones.Contains(Zone.ZoneID))
				{
					OnActorEnteredEWZone.Broadcast(Actor, Zone);
				}
			}
		}

		// Check for exit events
		for (const FName& OldZone : CurrentZones)
		{
			if (!NewZones.Contains(OldZone))
			{
				OnActorExitedEWZone.Broadcast(Actor, OldZone);
			}
		}

		CurrentZones = NewZones;
	}

	// Clean up stale actor entries
	for (auto It = ActorZoneMap.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid())
		{
			It.RemoveCurrent();
		}
	}
}

void ASHElectronicWarfare::SpawnEscalationZone()
{
	// Generate a random position within the play area
	// Bias toward the player side of the battlefield for maximum effect
	const FVector RandomPos = FVector(
		FMath::FRandRange(-30000.f, 30000.f),
		FMath::FRandRange(-20000.f, 20000.f),
		0.f);

	const float Radius = FMath::FRandRange(EscalationZoneRadiusRange.X, EscalationZoneRadiusRange.Y);

	// Randomly select effect type (weighted toward comms and GPS)
	ESHEWEffectType EffectType;
	const float Roll = FMath::FRand();
	if (Roll < 0.3f)
	{
		EffectType = ESHEWEffectType::GPSDenial;
	}
	else if (Roll < 0.6f)
	{
		EffectType = ESHEWEffectType::CommsJamming;
	}
	else if (Roll < 0.8f)
	{
		EffectType = ESHEWEffectType::DroneJamming;
	}
	else if (Roll < 0.9f)
	{
		EffectType = ESHEWEffectType::RadarDisruption;
	}
	else
	{
		EffectType = ESHEWEffectType::FullSpectrum;
	}

	const float BaseStrength = 0.4f + (EscalationLevel * 0.1f);

	CreateEWZone(RandomPos, Radius, FMath::Min(BaseStrength, 1.f), EffectType, false, -1.f);
}

FName ASHElectronicWarfare::GenerateZoneID()
{
	ZoneIDCounter++;
	return FName(*FString::Printf(TEXT("EW_Zone_%d"), ZoneIDCounter));
}

void ASHElectronicWarfare::ApplyEWToDrones()
{
	for (TActorIterator<ASHDroneBase> It(GetWorld()); It; ++It)
	{
		ASHDroneBase* Drone = *It;
		if (!Drone || Drone->GetDroneState() == ESHDroneState::Inactive ||
			Drone->GetDroneState() == ESHDroneState::Destroyed)
		{
			continue;
		}

		const float DroneJamming = GetDroneJammingAtPosition(Drone->GetActorLocation());

		if (DroneJamming > 0.f)
		{
			// Only jam enemy drones with friendly EW, and friendly drones with enemy EW
			bool bShouldJam = false;

			// Check if any affecting zone is hostile to this drone
			for (const FSHEWZone& Zone : EWZones)
			{
				if (!Zone.bActive) continue;
				if (Zone.GetStrengthAtPosition(Drone->GetActorLocation()) <= 0.f) continue;
				if (Zone.EffectType != ESHEWEffectType::DroneJamming &&
					Zone.EffectType != ESHEWEffectType::FullSpectrum) continue;

				// Friendly EW jams enemy drones, enemy EW jams friendly drones
				if (Zone.bFriendly && Drone->Affiliation == ESHDroneAffiliation::Enemy)
				{
					bShouldJam = true;
					break;
				}
				if (!Zone.bFriendly && Drone->Affiliation == ESHDroneAffiliation::Friendly)
				{
					bShouldJam = true;
					break;
				}
			}

			if (bShouldJam)
			{
				Drone->ApplyJammingInterference(DroneJamming, Drone->GetActorLocation());
			}
		}
	}
}
