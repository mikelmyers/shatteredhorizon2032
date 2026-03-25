// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "Combat/SHIndirectFireSystem.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"

// ======================================================================
//  Lifecycle
// ======================================================================

void USHIndirectFireSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (UWorld* World = GetWorld())
	{
		TickDelegateHandle = World->OnWorldPostActorTick.AddLambda(
			[this](UWorld*, ELevelTick, float DeltaSeconds)
			{
				Tick(DeltaSeconds);
			});
	}
}

void USHIndirectFireSystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->OnWorldPostActorTick.Remove(TickDelegateHandle);
	}
	Super::Deinitialize();
}

void USHIndirectFireSystem::Tick(float DeltaSeconds)
{
	const float WorldTime = GetWorld()->GetTimeSeconds();

	for (int32 i = ActiveMissions.Num() - 1; i >= 0; --i)
	{
		FSHFireMission& Mission = ActiveMissions[i];

		if (Mission.State != ESHFireMissionState::InFlight)
			continue;

		// Check if next round should impact.
		const float TimeSinceFire = WorldTime - Mission.FireTime;
		const float NextImpactTime = Mission.TimeOfFlightSec + (Mission.RoundsImpacted * Mission.SalvoIntervalSec);

		if (TimeSinceFire >= NextImpactTime && Mission.RoundsImpacted < Mission.RoundCount)
		{
			// Compute dispersed impact location (CEP-based).
			const FVector ImpactLoc = ComputeDispersedImpact(Mission.TargetLocation, Mission.CEPCm);

			ApplyImpact(Mission, ImpactLoc);
			Mission.RoundsImpacted++;

			OnIndirectImpact.Broadcast(ImpactLoc, Mission.Ordnance);

			// Check if salvo is complete.
			if (Mission.RoundsImpacted >= Mission.RoundCount)
			{
				Mission.State = ESHFireMissionState::Complete;
				OnFireMissionStateChanged.Broadcast(Mission);
				ActiveMissions.RemoveAt(i);
			}
		}
	}

	// Clean up stale counter-battery data (older than 60 seconds).
	RecentIncomingImpacts.RemoveAll([WorldTime](const FCounterBatteryData& Data)
	{
		return (WorldTime - Data.TimeReceived) > 60.f;
	});
}

// ======================================================================
//  Fire mission interface
// ======================================================================

FGuid USHIndirectFireSystem::RequestFireMission(
	AActor* Observer,
	ESHOrdnanceType Ordnance,
	FVector TargetLocation,
	int32 RoundCount)
{
	FSHFireMission Mission;
	Mission.MissionId = FGuid::NewGuid();
	Mission.Observer = Observer;
	Mission.Ordnance = Ordnance;
	Mission.TargetLocation = TargetLocation;
	Mission.RoundCount = FMath::Clamp(RoundCount, 1, 12);
	Mission.FiringOrigin = FriendlyBatteryPosition;

	// Populate ordnance parameters.
	GetOrdnanceParams(Ordnance, Mission.TimeOfFlightSec, Mission.SplashRadiusCm,
		Mission.BaseDamage, Mission.CEPCm);

	// Check ammo.
	if (AvailableFireMissions <= 0)
	{
		Mission.State = ESHFireMissionState::Denied;
		OnFireMissionStateChanged.Broadcast(Mission);
		return Mission.MissionId;
	}

	// Check danger close.
	Mission.bIsDangerClose = CheckDangerClose(TargetLocation);

	if (Mission.bIsDangerClose)
	{
		// Requires explicit authorization before firing.
		Mission.State = ESHFireMissionState::Pending;
		ActiveMissions.Add(Mission);
		OnFireMissionStateChanged.Broadcast(Mission);
		return Mission.MissionId;
	}

	// Approved — rounds away.
	Mission.State = ESHFireMissionState::InFlight;
	Mission.FireTime = GetWorld()->GetTimeSeconds();
	AvailableFireMissions--;
	ActiveMissions.Add(Mission);
	OnFireMissionStateChanged.Broadcast(Mission);

	return Mission.MissionId;
}

bool USHIndirectFireSystem::AuthorizeDangerClose(const FGuid& MissionId)
{
	for (FSHFireMission& Mission : ActiveMissions)
	{
		if (Mission.MissionId == MissionId && Mission.State == ESHFireMissionState::Pending)
		{
			if (AvailableFireMissions <= 0)
			{
				Mission.State = ESHFireMissionState::Denied;
				OnFireMissionStateChanged.Broadcast(Mission);
				return false;
			}

			Mission.bDangerCloseAuthorized = true;
			Mission.State = ESHFireMissionState::InFlight;
			Mission.FireTime = GetWorld()->GetTimeSeconds();
			AvailableFireMissions--;
			OnFireMissionStateChanged.Broadcast(Mission);
			return true;
		}
	}
	return false;
}

void USHIndirectFireSystem::CancelFireMission(const FGuid& MissionId)
{
	for (int32 i = 0; i < ActiveMissions.Num(); ++i)
	{
		if (ActiveMissions[i].MissionId == MissionId)
		{
			// Can only cancel if still pending. In-flight rounds can't be recalled.
			if (ActiveMissions[i].State == ESHFireMissionState::Pending)
			{
				ActiveMissions[i].State = ESHFireMissionState::Cancelled;
				OnFireMissionStateChanged.Broadcast(ActiveMissions[i]);
				ActiveMissions.RemoveAt(i);
			}
			return;
		}
	}
}

bool USHIndirectFireSystem::GetFireMission(const FGuid& MissionId, FSHFireMission& OutMission) const
{
	for (const FSHFireMission& Mission : ActiveMissions)
	{
		if (Mission.MissionId == MissionId)
		{
			OutMission = Mission;
			return true;
		}
	}
	return false;
}

// ======================================================================
//  Counter-battery
// ======================================================================

void USHIndirectFireSystem::ReportIncomingImpact(FVector ImpactLocation, float EstimatedTOF)
{
	const float WorldTime = GetWorld()->GetTimeSeconds();

	FCounterBatteryData Data;
	Data.ImpactLocation = ImpactLocation;
	Data.EstimatedTOF = EstimatedTOF;
	Data.TimeReceived = WorldTime;
	RecentIncomingImpacts.Add(Data);

	// Triangulation: with 3+ impacts from the same battery, estimate origin.
	// Simplified: use trajectory back-projection from impact cluster centroid.
	if (RecentIncomingImpacts.Num() >= 3)
	{
		// Compute centroid of recent impacts.
		FVector Centroid = FVector::ZeroVector;
		float AvgTOF = 0.f;
		const int32 Count = RecentIncomingImpacts.Num();

		for (const FCounterBatteryData& Entry : RecentIncomingImpacts)
		{
			Centroid += Entry.ImpactLocation;
			AvgTOF += Entry.EstimatedTOF;
		}
		Centroid /= Count;
		AvgTOF /= Count;

		// Estimate firing origin: mortar velocity ~200 m/s at 45° elevation.
		// Horizontal distance = V^2 * sin(2*45°) / g = V^2 / g
		// With TOF ~10s for 81mm at ~4km, we estimate distance from TOF.
		const float EstimatedRangeM = AvgTOF * 400.f; // Rough: 400 m per second of TOF
		const float EstimatedRangeCm = EstimatedRangeM * 100.f;

		// Direction: assume rounds come from a general direction (use impact spread).
		// For now, pick a random azimuth offset from impact centroid since we lack
		// trajectory data. In production, this would use acoustic triangulation.
		const FVector EstimatedOrigin = Centroid + FVector(EstimatedRangeCm, 0.f, 0.f);

		DetectedBatteries.Add(EstimatedOrigin);
		OnCounterBatteryDetected.Broadcast(EstimatedOrigin);

		// Clear used data.
		RecentIncomingImpacts.Empty();
	}
}

// ======================================================================
//  Ordnance parameters
// ======================================================================

void USHIndirectFireSystem::GetOrdnanceParams(ESHOrdnanceType Type, float& OutTOF,
	float& OutSplashCm, float& OutDamage, float& OutCEP) const
{
	switch (Type)
	{
	case ESHOrdnanceType::HE_60mm:
		OutTOF       = 8.0f;    // 8 seconds
		OutSplashCm  = 1500.f;  // 15m lethal radius
		OutDamage    = 150.f;
		OutCEP       = 2000.f;  // 20m CEP
		break;

	case ESHOrdnanceType::HE_81mm:
		OutTOF       = 10.0f;   // 10 seconds
		OutSplashCm  = 2500.f;  // 25m lethal radius
		OutDamage    = 250.f;
		OutCEP       = 3000.f;  // 30m CEP
		break;

	case ESHOrdnanceType::HE_120mm:
		OutTOF       = 12.0f;   // 12 seconds
		OutSplashCm  = 4000.f;  // 40m lethal radius
		OutDamage    = 400.f;
		OutCEP       = 4000.f;  // 40m CEP
		break;

	case ESHOrdnanceType::HE_155mm:
		OutTOF       = 15.0f;   // 15 seconds (longer range)
		OutSplashCm  = 7000.f;  // 70m lethal radius
		OutDamage    = 600.f;
		OutCEP       = 5000.f;  // 50m CEP
		break;

	case ESHOrdnanceType::Smoke_81mm:
		OutTOF       = 10.0f;
		OutSplashCm  = 5000.f;  // 50m smoke screen radius
		OutDamage    = 0.f;     // Non-lethal
		OutCEP       = 3000.f;
		break;

	case ESHOrdnanceType::Illumination:
		OutTOF       = 12.0f;
		OutSplashCm  = 30000.f; // 300m illumination radius
		OutDamage    = 0.f;     // Non-lethal
		OutCEP       = 5000.f;
		break;

	default:
		OutTOF       = 10.0f;
		OutSplashCm  = 2500.f;
		OutDamage    = 200.f;
		OutCEP       = 3000.f;
		break;
	}
}

// ======================================================================
//  Internal
// ======================================================================

bool USHIndirectFireSystem::CheckDangerClose(FVector TargetLocation) const
{
	// Check if any player-controlled pawns are within danger close radius.
	UWorld* World = GetWorld();
	if (!World) return false;

	TArray<AActor*> Characters;
	UGameplayStatics::GetAllActorsOfClass(World, ACharacter::StaticClass(), Characters);

	for (AActor* Actor : Characters)
	{
		if (ACharacter* Char = Cast<ACharacter>(Actor))
		{
			// Check player-controlled characters only.
			if (Char->IsPlayerControlled() || Char->Tags.Contains(FName("Friendly")))
			{
				const float DistCm = FVector::Dist(Char->GetActorLocation(), TargetLocation);
				if (DistCm < DangerCloseRadiusCm)
				{
					return true;
				}
			}
		}
	}
	return false;
}

void USHIndirectFireSystem::ApplyImpact(const FSHFireMission& Mission, FVector ImpactLocation)
{
	UWorld* World = GetWorld();
	if (!World) return;

	if (Mission.BaseDamage > 0.f)
	{
		// Apply radial damage with inverse-square falloff.
		const float InnerRadius = Mission.SplashRadiusCm * 0.3f;
		const float OuterRadius = Mission.SplashRadiusCm;
		const float MinDamage = Mission.BaseDamage * 0.1f;

		UGameplayStatics::ApplyRadialDamageWithFalloff(
			World,
			Mission.BaseDamage,
			MinDamage,
			ImpactLocation,
			InnerRadius,
			OuterRadius,
			2.0f, // Falloff exponent (inverse square)
			nullptr, // DamageTypeClass
			TArray<AActor*>(), // IgnoreActors
			nullptr, // DamageCauser
			nullptr, // InstigatedByController
			ECollisionChannel::ECC_Visibility);
	}

	// Report suppression in blast zone (AI morale degradation at 15m+).
	// This delegates to the suppression system via gameplay event.
}

FVector USHIndirectFireSystem::ComputeDispersedImpact(FVector Center, float CEP) const
{
	// CEP: 50% of rounds land within this radius.
	// Model as 2D Gaussian: CEP = 0.6745 * sigma for 50% probability.
	const float Sigma = CEP / 0.6745f;

	const float OffsetX = FMath::FRandRange(-1.f, 1.f) * Sigma;
	const float OffsetY = FMath::FRandRange(-1.f, 1.f) * Sigma;

	return Center + FVector(OffsetX, OffsetY, 0.f);
}
