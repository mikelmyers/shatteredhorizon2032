// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHPrimordiaSimulon.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"

DEFINE_LOG_CATEGORY_STATIC(LogSH_Simulon, Log, All);

// -----------------------------------------------------------------------
//  Constructor
// -----------------------------------------------------------------------

USHPrimordiaSimulon::USHPrimordiaSimulon()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.2f; // 5 Hz is sufficient for threat modeling
}

// -----------------------------------------------------------------------
//  UActorComponent overrides
// -----------------------------------------------------------------------

void USHPrimordiaSimulon::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogSH_Simulon, Verbose, TEXT("Simulon threat modeler initialized on %s."),
		*GetOwner()->GetName());
}

void USHPrimordiaSimulon::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	// Decay confidence for unobserved targets
	for (FTargetObservation& Obs : Observations)
	{
		const float TimeSinceLastSeen = CurrentTime - Obs.LastObservedTime;
		if (TimeSinceLastSeen > 0.0f)
		{
			Obs.Confidence = FMath::Max(0.0f, Obs.Confidence - ConfidenceDecayRate * DeltaTime);
		}
	}

	PruneExpiredModels();
	RebuildThreatModels();
}

// -----------------------------------------------------------------------
//  Observation
// -----------------------------------------------------------------------

void USHPrimordiaSimulon::UpdateThreatModel(AActor* Target)
{
	if (!Target || !GetWorld())
	{
		return;
	}

	FTargetObservation* Obs = FindOrCreateObservation(Target);
	if (!Obs)
	{
		return;
	}

	const float CurrentTime = GetWorld()->GetTimeSeconds();
	const FVector CurrentPos = Target->GetActorLocation();

	// Add position to history
	Obs->PositionHistory.Add(CurrentPos);
	Obs->TimestampHistory.Add(CurrentTime);

	// Cap history size
	while (Obs->PositionHistory.Num() > PositionHistorySize)
	{
		Obs->PositionHistory.RemoveAt(0);
		Obs->TimestampHistory.RemoveAt(0);
	}

	// Update velocity from recent positions
	Obs->LastVelocity = ComputeVelocity(*Obs);
	Obs->LastObservedTime = CurrentTime;

	// Boost confidence on observation
	Obs->Confidence = FMath::Min(1.0f, Obs->Confidence + 0.3f);

	// Assess threat
	Obs->ThreatLevel = AssessThreatLevel(*Obs);
}

// -----------------------------------------------------------------------
//  Prediction
// -----------------------------------------------------------------------

FVector USHPrimordiaSimulon::PredictTargetPosition(AActor* Target, float TimeAhead) const
{
	if (!Target)
	{
		return FVector::ZeroVector;
	}

	const FTargetObservation* Obs = FindObservation(Target);
	if (!Obs || Obs->PositionHistory.Num() == 0)
	{
		// No data — return current position
		return Target->GetActorLocation();
	}

	const FVector LastKnownPos = Obs->PositionHistory.Last();

	// Velocity-based extrapolation
	const FVector VelocityPrediction = LastKnownPos + Obs->LastVelocity * TimeAhead;

	// Pattern-based prediction (accounts for acceleration trends)
	const FVector PatternPrediction = ComputePatternPrediction(*Obs, TimeAhead);

	// Blend the two predictions
	const FVector Prediction = FMath::Lerp(PatternPrediction, VelocityPrediction, VelocityWeight);

	return Prediction;
}

// -----------------------------------------------------------------------
//  Queries
// -----------------------------------------------------------------------

bool USHPrimordiaSimulon::GetThreatModelForActor(AActor* Target, FSHThreatModel& OutModel) const
{
	for (const FSHThreatModel& Model : ThreatModels)
	{
		if (Model.TrackedActor.Get() == Target)
		{
			OutModel = Model;
			return true;
		}
	}
	return false;
}

bool USHPrimordiaSimulon::GetHighestThreat(FSHThreatModel& OutModel) const
{
	if (ThreatModels.Num() == 0)
	{
		return false;
	}

	const FSHThreatModel* Best = &ThreatModels[0];
	for (const FSHThreatModel& Model : ThreatModels)
	{
		if (Model.ThreatLevel > Best->ThreatLevel)
		{
			Best = &Model;
		}
	}

	OutModel = *Best;
	return true;
}

// -----------------------------------------------------------------------
//  Internal helpers
// -----------------------------------------------------------------------

USHPrimordiaSimulon::FTargetObservation* USHPrimordiaSimulon::FindOrCreateObservation(AActor* Target)
{
	// Find existing
	for (FTargetObservation& Obs : Observations)
	{
		if (Obs.Actor.Get() == Target)
		{
			return &Obs;
		}
	}

	// Cap total tracked threats
	if (Observations.Num() >= MaxTrackedThreats)
	{
		// Remove lowest confidence observation
		int32 LowestIdx = 0;
		float LowestConf = Observations[0].Confidence;
		for (int32 i = 1; i < Observations.Num(); ++i)
		{
			if (Observations[i].Confidence < LowestConf)
			{
				LowestConf = Observations[i].Confidence;
				LowestIdx = i;
			}
		}
		Observations.RemoveAt(LowestIdx);
	}

	// Create new
	FTargetObservation NewObs;
	NewObs.Actor = Target;
	NewObs.LastObservedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	NewObs.Confidence = 0.5f;
	Observations.Add(MoveTemp(NewObs));
	return &Observations.Last();
}

const USHPrimordiaSimulon::FTargetObservation* USHPrimordiaSimulon::FindObservation(AActor* Target) const
{
	for (const FTargetObservation& Obs : Observations)
	{
		if (Obs.Actor.Get() == Target)
		{
			return &Obs;
		}
	}
	return nullptr;
}

FVector USHPrimordiaSimulon::ComputeVelocity(const FTargetObservation& Obs) const
{
	if (Obs.PositionHistory.Num() < 2)
	{
		return FVector::ZeroVector;
	}

	// Use the last two positions to compute instantaneous velocity
	const int32 Last = Obs.PositionHistory.Num() - 1;
	const int32 Prev = Last - 1;

	const float DeltaT = Obs.TimestampHistory[Last] - Obs.TimestampHistory[Prev];
	if (DeltaT <= KINDA_SMALL_NUMBER)
	{
		return FVector::ZeroVector;
	}

	return (Obs.PositionHistory[Last] - Obs.PositionHistory[Prev]) / DeltaT;
}

FVector USHPrimordiaSimulon::ComputePatternPrediction(const FTargetObservation& Obs, float TimeAhead) const
{
	if (Obs.PositionHistory.Num() < 3)
	{
		// Not enough data for pattern — fallback to linear extrapolation
		if (Obs.PositionHistory.Num() > 0)
		{
			return Obs.PositionHistory.Last() + Obs.LastVelocity * TimeAhead;
		}
		return FVector::ZeroVector;
	}

	// Compute average acceleration from the position history to detect trends
	// (e.g., the target consistently turns left, accelerates, etc.)
	FVector AccumulatedAcceleration = FVector::ZeroVector;
	int32 AccelSamples = 0;

	for (int32 i = 2; i < Obs.PositionHistory.Num(); ++i)
	{
		const float Dt1 = Obs.TimestampHistory[i - 1] - Obs.TimestampHistory[i - 2];
		const float Dt2 = Obs.TimestampHistory[i] - Obs.TimestampHistory[i - 1];

		if (Dt1 > KINDA_SMALL_NUMBER && Dt2 > KINDA_SMALL_NUMBER)
		{
			const FVector V1 = (Obs.PositionHistory[i - 1] - Obs.PositionHistory[i - 2]) / Dt1;
			const FVector V2 = (Obs.PositionHistory[i] - Obs.PositionHistory[i - 1]) / Dt2;
			const FVector Accel = (V2 - V1) / ((Dt1 + Dt2) * 0.5f);

			AccumulatedAcceleration += Accel;
			AccelSamples++;
		}
	}

	const FVector AvgAcceleration = AccelSamples > 0
		? AccumulatedAcceleration / static_cast<float>(AccelSamples)
		: FVector::ZeroVector;

	// Prediction: position + velocity * t + 0.5 * acceleration * t^2
	const FVector LastPos = Obs.PositionHistory.Last();
	return LastPos + Obs.LastVelocity * TimeAhead + 0.5f * AvgAcceleration * TimeAhead * TimeAhead;
}

float USHPrimordiaSimulon::AssessThreatLevel(const FTargetObservation& Obs) const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Obs.Actor.IsValid())
	{
		return 0.0f;
	}

	const FVector OwnerLoc = Owner->GetActorLocation();
	const FVector ThreatLoc = Obs.PositionHistory.Num() > 0 ? Obs.PositionHistory.Last() : FVector::ZeroVector;

	// Distance factor (closer = more threatening)
	const float Distance = FVector::Dist(OwnerLoc, ThreatLoc);
	const float DistanceFactor = FMath::GetMappedRangeValueClamped(
		FVector2D(500.0f, 10000.0f), FVector2D(1.0f, 0.1f), Distance);

	// Approach factor (moving toward us = more threatening)
	float ApproachFactor = 0.5f; // neutral
	if (Obs.LastVelocity.SizeSquared() > 100.0f)
	{
		const FVector ToOwner = (OwnerLoc - ThreatLoc).GetSafeNormal();
		const FVector VelDir = Obs.LastVelocity.GetSafeNormal();
		const float Dot = FVector::DotProduct(ToOwner, VelDir);
		ApproachFactor = FMath::GetMappedRangeValueClamped(
			FVector2D(-1.0f, 1.0f), FVector2D(0.1f, 1.0f), Dot);
	}

	// Speed factor (faster = more threatening in milsim context)
	const float Speed = Obs.LastVelocity.Size();
	const float SpeedFactor = FMath::GetMappedRangeValueClamped(
		FVector2D(0.0f, 600.0f), FVector2D(0.3f, 0.8f), Speed);

	const float RawThreat = DistanceFactor * 0.4f + ApproachFactor * 0.4f + SpeedFactor * 0.2f;
	return FMath::Clamp(RawThreat, 0.0f, 1.0f);
}

void USHPrimordiaSimulon::PruneExpiredModels()
{
	const float CurrentTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	Observations.RemoveAll([this, CurrentTime](const FTargetObservation& Obs)
	{
		// Remove if actor is gone
		if (!Obs.Actor.IsValid())
		{
			return true;
		}

		// Remove if expired
		const float TimeSinceLastSeen = CurrentTime - Obs.LastObservedTime;
		if (TimeSinceLastSeen > ThreatExpirationTime)
		{
			return true;
		}

		// Remove if confidence is zero
		if (Obs.Confidence <= 0.0f)
		{
			return true;
		}

		return false;
	});
}

void USHPrimordiaSimulon::RebuildThreatModels()
{
	ThreatModels.Empty(Observations.Num());

	for (const FTargetObservation& Obs : Observations)
	{
		FSHThreatModel Model;
		Model.TrackedActor = Obs.Actor;
		Model.ThreatLocation = Obs.PositionHistory.Num() > 0 ? Obs.PositionHistory.Last() : FVector::ZeroVector;
		Model.PredictedMovement = Obs.LastVelocity;
		Model.Confidence = Obs.Confidence;
		Model.LastObserved = Obs.LastObservedTime;
		Model.ThreatLevel = Obs.ThreatLevel;
		ThreatModels.Add(MoveTemp(Model));
	}
}
