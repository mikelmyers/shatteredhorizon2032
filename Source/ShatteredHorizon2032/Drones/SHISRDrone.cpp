// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHISRDrone.h"
#include "ShatteredHorizon2032/ShatteredHorizon2032.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

ASHISRDrone::ASHISRDrone()
{
	// ISR drones are lighter, quieter, higher altitude
	MaxFlightSpeed = 1500.f;
	CruiseSpeed = 1000.f;
	MaxBatterySeconds = 1800.f; // 30 minutes loiter
	BaseAudibleRange = 4000.f;
	FlyingNoiseMultiplier = 1.2f;
	DefaultCruiseAltitude = 8000.f;
	MaxHealth = 60.f; // Fragile
	SmallArmsDamageMultiplier = 1.5f;

	Affiliation = ESHDroneAffiliation::Enemy;
}

void ASHISRDrone::BeginPlay()
{
	Super::BeginPlay();

	// Use ISR-specific buzz sound if set
	if (ISRBuzzSound)
	{
		MotorLoopSound = ISRBuzzSound;
		if (MotorAudioComponent)
		{
			MotorAudioComponent->SetSound(ISRBuzzSound);
		}
	}

	CurrentCruiseAltitude = PatrolAltitude;

	SH_LOG(LogSH_Drone, Log, "ISR Drone BeginPlay — Camera: %d, Pattern: %d",
		static_cast<int32>(CurrentCameraMode), static_cast<int32>(CurrentPatrolPattern));
}

void ASHISRDrone::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Cache night time status (check time-of-day from game state or world)
	// Simple heuristic: use directional light or a game state query
	// For now, check world time
	if (const UWorld* World = GetWorld())
	{
		// Game mode tracks time-of-day; we approximate with a delegate or direct check
		// This would typically query the game state. Using a placeholder for now.
		bIsNightTime = false; // Updated externally or by game state query
	}
}

// =======================================================================
//  Drone behavior overrides
// =======================================================================

void ASHISRDrone::TickDroneBehavior(float DeltaSeconds)
{
	if (GetDroneState() == ESHDroneState::Destroyed ||
		GetDroneState() == ESHDroneState::Inactive ||
		GetDroneState() == ESHDroneState::Launching)
	{
		return;
	}

	TickDetectionCone(DeltaSeconds);
	TickDetectionRelay(DeltaSeconds);
	PruneStaleDetections();

	// Only run patrol movement if flying or loitering
	if (GetDroneState() == ESHDroneState::Flying || GetDroneState() == ESHDroneState::Loitering)
	{
		TickPatrolMovement(DeltaSeconds);
	}
}

void ASHISRDrone::OnStateEntered(ESHDroneState NewState)
{
	Super::OnStateEntered(NewState);

	if (NewState == ESHDroneState::Loitering || NewState == ESHDroneState::Flying)
	{
		// Begin scanning
		ScanAccumulator = 0.f;
	}
}

void ASHISRDrone::OnStateExited(ESHDroneState OldState)
{
	Super::OnStateExited(OldState);
}

// =======================================================================
//  Detection system
// =======================================================================

void ASHISRDrone::TickDetectionCone(float DeltaSeconds)
{
	ScanAccumulator += DeltaSeconds;
	if (ScanAccumulator < ScanInterval)
	{
		return;
	}
	ScanAccumulator -= ScanInterval;

	// Gather all potential targets (pawns)
	TArray<AActor*> PotentialTargets;
	UGameplayStatics::GetAllActorsOfClass(GetWorld(), APawn::StaticClass(), PotentialTargets);

	for (AActor* Target : PotentialTargets)
	{
		if (Target == this || Target == nullptr)
		{
			continue;
		}

		// Only detect pawns that are player-controlled or friendly AI
		// (ISR drone is enemy, so it detects non-enemy pawns)
		const APawn* TargetPawn = Cast<APawn>(Target);
		if (!TargetPawn)
		{
			continue;
		}

		// Skip other enemy drones/AI
		if (const ASHDroneBase* OtherDrone = Cast<ASHDroneBase>(Target))
		{
			if (OtherDrone->Affiliation == ESHDroneAffiliation::Enemy)
			{
				continue;
			}
		}

		float Confidence = 0.f;
		if (IsInDetectionCone(Target, Confidence))
		{
			// Accumulate time in cone
			TWeakObjectPtr<AActor> WeakTarget(Target);
			float& TimeInCone = ConeTimeAccumulator.FindOrAdd(WeakTarget, 0.f);
			TimeInCone += ScanInterval;

			// Check if acquisition time met
			if (TimeInCone >= DetectionAcquisitionTime)
			{
				// Check if already tracking
				bool bAlreadyTracking = false;
				for (FSHDroneDetection& Detection : ActiveDetections)
				{
					if (Detection.DetectedActor == Target)
					{
						Detection.Confidence = Confidence;
						Detection.DetectionLocation = Target->GetActorLocation();
						Detection.bCurrentlyTracked = true;
						bAlreadyTracking = true;
						break;
					}
				}

				if (!bAlreadyTracking)
				{
					// New detection
					FSHDroneDetection NewDetection;
					NewDetection.DetectedActor = Target;
					NewDetection.DetectionLocation = Target->GetActorLocation();
					NewDetection.DetectionTime = GetWorld()->GetTimeSeconds();
					NewDetection.Confidence = Confidence;
					NewDetection.bCurrentlyTracked = true;
					NewDetection.bRelayedToCommand = false;
					ActiveDetections.Add(NewDetection);

					OnEnemyDetected.Broadcast(Target, Confidence);

					// Start relay timer
					RelayTimerMap.Add(WeakTarget, 0.f);

					SH_LOG(LogSH_Drone, Log, "ISR Drone detected target: %s (Confidence: %.2f)",
						*Target->GetName(), Confidence);
				}
			}
		}
		else
		{
			// Target left the cone — reset time accumulator
			TWeakObjectPtr<AActor> WeakTarget(Target);
			ConeTimeAccumulator.Remove(WeakTarget);
		}
	}
}

void ASHISRDrone::TickDetectionRelay(float DeltaSeconds)
{
	for (FSHDroneDetection& Detection : ActiveDetections)
	{
		if (Detection.bRelayedToCommand || !Detection.bCurrentlyTracked)
		{
			continue;
		}

		if (Detection.Confidence < RelayConfidenceThreshold)
		{
			continue;
		}

		TWeakObjectPtr<AActor> WeakActor = Detection.DetectedActor;
		float* Timer = RelayTimerMap.Find(WeakActor);
		if (Timer)
		{
			*Timer += DeltaSeconds;
			if (*Timer >= RelayDelay)
			{
				RelayDetectionToCommand(Detection);
			}
		}
	}
}

bool ASHISRDrone::IsInDetectionCone(AActor* Target, float& OutConfidence) const
{
	if (!Target)
	{
		return false;
	}

	const FVector DroneLocation = GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	const float Distance = FVector::Dist(DroneLocation, TargetLocation);

	// Get effective detection range
	float EffectiveRange = MaxDetectionRange;
	if (CurrentCameraMode == ESHISRCameraMode::ThermalIR)
	{
		EffectiveRange *= ThermalRangeMultiplier;
	}
	else if (CurrentCameraMode == ESHISRCameraMode::Daylight && bIsNightTime)
	{
		EffectiveRange *= NightDetectionPenalty;
	}

	if (Distance > EffectiveRange)
	{
		return false;
	}

	// Check cone angle — camera points downward from drone
	const FVector ToTarget = (TargetLocation - DroneLocation).GetSafeNormal();
	const FVector CameraDirection = GetActorForwardVector().RotateAngleAxis(-45.f, GetActorRightVector());

	const float DotProduct = FVector::DotProduct(CameraDirection, ToTarget);
	const float ConeThreshold = FMath::Cos(FMath::DegreesToRadians(DetectionConeHalfAngle));

	if (DotProduct < ConeThreshold)
	{
		return false;
	}

	// Line-of-sight check
	if (!HasLineOfSight(Target))
	{
		return false;
	}

	OutConfidence = ComputeDetectionConfidence(Target, Distance);
	return true;
}

float ASHISRDrone::ComputeDetectionConfidence(AActor* Target, float DistanceCm) const
{
	// Base confidence from distance (closer = more confident)
	float EffectiveRange = MaxDetectionRange;
	if (CurrentCameraMode == ESHISRCameraMode::ThermalIR)
	{
		EffectiveRange *= ThermalRangeMultiplier;
	}

	float DistanceFactor = 1.f - FMath::Clamp(DistanceCm / EffectiveRange, 0.f, 1.f);

	// Movement bonus — moving targets are easier to spot
	float MovementBonus = 0.f;
	if (Target)
	{
		const float TargetSpeed = Target->GetVelocity().Size();
		MovementBonus = FMath::Clamp(TargetSpeed / 600.f, 0.f, 0.3f);
	}

	// Thermal bonus at night
	float ThermalBonus = 0.f;
	if (CurrentCameraMode == ESHISRCameraMode::ThermalIR)
	{
		ThermalBonus = 0.2f;
		if (bIsNightTime)
		{
			ThermalBonus = 0.4f; // Thermal is excellent at night
		}
	}

	// Night penalty for daylight camera
	float NightPenalty = 0.f;
	if (CurrentCameraMode == ESHISRCameraMode::Daylight && bIsNightTime)
	{
		NightPenalty = 0.4f;
	}

	// Signal degradation penalty
	float SignalPenalty = 0.f;
	if (GetSignalLinkQuality() == ESHSignalLinkQuality::Degraded)
	{
		SignalPenalty = 0.1f;
	}
	else if (GetSignalLinkQuality() == ESHSignalLinkQuality::Critical)
	{
		SignalPenalty = 0.3f;
	}

	float Confidence = DistanceFactor + MovementBonus + ThermalBonus - NightPenalty - SignalPenalty;
	return FMath::Clamp(Confidence, 0.f, 1.f);
}

bool ASHISRDrone::HasLineOfSight(AActor* Target) const
{
	if (!Target)
	{
		return false;
	}

	FHitResult HitResult;
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(this);
	QueryParams.AddIgnoredActor(Target);
	QueryParams.bTraceComplex = false;

	const FVector Start = GetActorLocation();
	const FVector End = Target->GetActorLocation();

	bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, DetectionTraceChannel, QueryParams);

	// If thermal, ignore certain materials / light cover
	if (bHit && CurrentCameraMode == ESHISRCameraMode::ThermalIR && bThermalIgnoresLightCover)
	{
		// Check if the blocking object is "light cover" (tagged or via physical material)
		if (HitResult.GetActor() && HitResult.GetActor()->ActorHasTag(TEXT("LightCover")))
		{
			return true; // Thermal sees through light cover
		}
	}

	return !bHit;
}

void ASHISRDrone::RelayDetectionToCommand(FSHDroneDetection& Detection)
{
	Detection.bRelayedToCommand = true;

	OnDetectionRelayed.Broadcast(Detection);

	SH_LOG(LogSH_Drone, Log, "ISR Drone relayed detection to command: %s at %s (Confidence: %.2f)",
		Detection.DetectedActor.IsValid() ? *Detection.DetectedActor->GetName() : TEXT("Unknown"),
		*Detection.DetectionLocation.ToString(), Detection.Confidence);

	// In a full implementation, this would interface with the Primordia AI system
	// to increase enemy tactical awareness and potentially trigger tactical responses
	// (flanking, mortar fire, reinforcement repositioning, etc.)
}

void ASHISRDrone::PruneStaleDetections()
{
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	const float StaleTimeout = 10.f; // Seconds before a non-tracked detection is pruned

	for (int32 i = ActiveDetections.Num() - 1; i >= 0; --i)
	{
		FSHDroneDetection& Detection = ActiveDetections[i];

		// Remove if the actor is invalid (destroyed)
		if (!Detection.DetectedActor.IsValid())
		{
			ActiveDetections.RemoveAt(i);
			continue;
		}

		// Mark as not tracked (will be re-set in TickDetectionCone if still visible)
		if (Detection.bCurrentlyTracked)
		{
			// Check if still in cone
			float DummyConfidence;
			if (!IsInDetectionCone(Detection.DetectedActor.Get(), DummyConfidence))
			{
				Detection.bCurrentlyTracked = false;
				OnEnemyLost.Broadcast(Detection.DetectedActor.Get());

				SH_LOG(LogSH_Drone, Verbose, "ISR Drone lost track of: %s",
					*Detection.DetectedActor->GetName());
			}
		}

		// Prune stale entries
		if (!Detection.bCurrentlyTracked && (CurrentTime - Detection.DetectionTime) > StaleTimeout)
		{
			TWeakObjectPtr<AActor> WeakActor = Detection.DetectedActor;
			ConeTimeAccumulator.Remove(WeakActor);
			RelayTimerMap.Remove(WeakActor);
			ActiveDetections.RemoveAt(i);
		}
	}
}

// =======================================================================
//  Camera control
// =======================================================================

void ASHISRDrone::SetCameraMode(ESHISRCameraMode NewMode)
{
	if (NewMode == CurrentCameraMode)
	{
		return;
	}

	CurrentCameraMode = NewMode;

	SH_LOG(LogSH_Drone, Log, "ISR Drone camera mode changed to: %d", static_cast<int32>(NewMode));
}

// =======================================================================
//  Patrol movement
// =======================================================================

void ASHISRDrone::SetPatrolPattern(ESHPatrolPattern Pattern)
{
	CurrentPatrolPattern = Pattern;
	CurrentWaypointIndex = 0;
	WaypointDwellTimer = 0.f;
	bDwellingAtWaypoint = false;
	CurrentOrbitAngle = 0.f;
}

void ASHISRDrone::SetPatrolWaypoints(const TArray<FVector>& Waypoints)
{
	PatrolWaypoints = Waypoints;
	CurrentWaypointIndex = 0;
}

void ASHISRDrone::SetOrbitParameters(const FVector& Center, float RadiusCm)
{
	OrbitCenter = Center;
	OrbitRadius = RadiusCm;
	OrbitTargetActor = nullptr;
}

void ASHISRDrone::OrbitActor(AActor* TargetActor)
{
	OrbitTargetActor = TargetActor;
}

void ASHISRDrone::TickPatrolMovement(float DeltaSeconds)
{
	switch (CurrentPatrolPattern)
	{
	case ESHPatrolPattern::Linear:
	case ESHPatrolPattern::Racetrack:
	{
		if (PatrolWaypoints.Num() == 0)
		{
			return;
		}

		if (bDwellingAtWaypoint)
		{
			WaypointDwellTimer += DeltaSeconds;
			if (WaypointDwellTimer >= WaypointDwellTime)
			{
				bDwellingAtWaypoint = false;
				WaypointDwellTimer = 0.f;
				AdvanceToNextWaypoint();
			}
			return;
		}

		// Fly toward current waypoint
		FVector TargetWP = PatrolWaypoints[CurrentWaypointIndex];
		TargetWP.Z = PatrolAltitude;
		SetFlightTarget(TargetWP);

		const float DistToWP = FVector::DistXY(GetActorLocation(), TargetWP);
		if (DistToWP < WaypointArrivalDistance)
		{
			bDwellingAtWaypoint = true;
			WaypointDwellTimer = 0.f;

			if (GetDroneState() != ESHDroneState::Loitering)
			{
				SetDroneState(ESHDroneState::Loitering);
			}
		}
		else if (GetDroneState() != ESHDroneState::Flying)
		{
			SetDroneState(ESHDroneState::Flying);
		}
		break;
	}

	case ESHPatrolPattern::Orbit:
	{
		// Update orbit center if tracking an actor
		if (OrbitTargetActor.IsValid())
		{
			OrbitCenter = OrbitTargetActor->GetActorLocation();
		}

		CurrentOrbitAngle += FMath::DegreesToRadians(OrbitAngularSpeed) * DeltaSeconds;
		if (CurrentOrbitAngle > 2.f * PI)
		{
			CurrentOrbitAngle -= 2.f * PI;
		}

		const FVector OrbitPos = ComputeOrbitPosition(CurrentOrbitAngle);
		SetFlightTarget(OrbitPos);

		if (GetDroneState() != ESHDroneState::Loitering)
		{
			SetDroneState(ESHDroneState::Loitering);
		}
		break;
	}

	case ESHPatrolPattern::SearchGrid:
	{
		// Generate grid waypoints on-the-fly if not set
		if (PatrolWaypoints.Num() == 0)
		{
			const FVector Center = OrbitCenter.IsNearlyZero() ? GetActorLocation() : OrbitCenter;
			const int32 GridSize = 3;
			const float HalfGrid = (GridSize - 1) * SearchGridCellSize * 0.5f;

			for (int32 Y = 0; Y < GridSize; ++Y)
			{
				for (int32 X = 0; X < GridSize; ++X)
				{
					// Serpentine pattern
					int32 EffectiveX = (Y % 2 == 0) ? X : (GridSize - 1 - X);
					FVector WP = Center + FVector(
						(EffectiveX * SearchGridCellSize) - HalfGrid,
						(Y * SearchGridCellSize) - HalfGrid,
						PatrolAltitude);
					PatrolWaypoints.Add(WP);
				}
			}
		}

		// Reuse linear movement logic
		if (PatrolWaypoints.Num() > 0)
		{
			if (bDwellingAtWaypoint)
			{
				WaypointDwellTimer += DeltaSeconds;
				if (WaypointDwellTimer >= WaypointDwellTime * 0.5f) // Shorter dwell for grid
				{
					bDwellingAtWaypoint = false;
					WaypointDwellTimer = 0.f;
					AdvanceToNextWaypoint();
				}
				return;
			}

			FVector TargetWP = PatrolWaypoints[CurrentWaypointIndex];
			TargetWP.Z = PatrolAltitude;
			SetFlightTarget(TargetWP);

			const float DistToWP = FVector::DistXY(GetActorLocation(), TargetWP);
			if (DistToWP < WaypointArrivalDistance)
			{
				bDwellingAtWaypoint = true;
				WaypointDwellTimer = 0.f;
			}
		}
		break;
	}
	}
}

void ASHISRDrone::AdvanceToNextWaypoint()
{
	if (PatrolWaypoints.Num() == 0)
	{
		return;
	}

	if (CurrentPatrolPattern == ESHPatrolPattern::Racetrack)
	{
		// Racetrack: go back and forth
		static bool bForward = true;
		if (bForward)
		{
			CurrentWaypointIndex++;
			if (CurrentWaypointIndex >= PatrolWaypoints.Num())
			{
				CurrentWaypointIndex = PatrolWaypoints.Num() - 2;
				bForward = false;
			}
		}
		else
		{
			CurrentWaypointIndex--;
			if (CurrentWaypointIndex < 0)
			{
				CurrentWaypointIndex = 1;
				bForward = true;
			}
		}
	}
	else
	{
		// Linear / Grid: loop around
		CurrentWaypointIndex = (CurrentWaypointIndex + 1) % PatrolWaypoints.Num();
	}
}

FVector ASHISRDrone::ComputeOrbitPosition(float AngleRadians) const
{
	return FVector(
		OrbitCenter.X + OrbitRadius * FMath::Cos(AngleRadians),
		OrbitCenter.Y + OrbitRadius * FMath::Sin(AngleRadians),
		PatrolAltitude);
}

// =======================================================================
//  Queries
// =======================================================================

int32 ASHISRDrone::GetTrackedTargetCount() const
{
	int32 Count = 0;
	for (const FSHDroneDetection& Detection : ActiveDetections)
	{
		if (Detection.bCurrentlyTracked)
		{
			Count++;
		}
	}
	return Count;
}

bool ASHISRDrone::IsActorDetected(AActor* Actor) const
{
	for (const FSHDroneDetection& Detection : ActiveDetections)
	{
		if (Detection.DetectedActor == Actor)
		{
			return true;
		}
	}
	return false;
}
