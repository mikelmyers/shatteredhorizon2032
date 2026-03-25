// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SHPrimordiaSimulon.generated.h"

/* -----------------------------------------------------------------------
 *  Structs
 * --------------------------------------------------------------------- */

USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHThreatModel
{
	GENERATED_BODY()

	/** World-space location of the threat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Threat")
	FVector ThreatLocation = FVector::ZeroVector;

	/** Predicted movement direction and magnitude. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Threat")
	FVector PredictedMovement = FVector::ZeroVector;

	/** Prediction confidence (0 = no data, 1 = high confidence). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Threat", meta = (ClampMin = "0", ClampMax = "1"))
	float Confidence = 0.0f;

	/** Game-time when this target was last observed. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Threat")
	float LastObserved = 0.0f;

	/** Overall threat level assessment (0 = negligible, 1 = critical). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Threat", meta = (ClampMin = "0", ClampMax = "1"))
	float ThreatLevel = 0.0f;

	/** Tracked actor (may become null if destroyed). */
	UPROPERTY()
	TWeakObjectPtr<AActor> TrackedActor = nullptr;
};

/* -----------------------------------------------------------------------
 *  USHPrimordiaSimulon
 * --------------------------------------------------------------------- */

/**
 * USHPrimordiaSimulon
 *
 * Threat modeling component for AI units. Builds predictive movement
 * models for observed targets based on velocity extrapolation and
 * behavioral pattern matching. AI systems use these predictions to
 * lead shots, set up ambushes, and anticipate flanking maneuvers.
 */
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHPrimordiaSimulon : public UActorComponent
{
	GENERATED_BODY()

public:
	USHPrimordiaSimulon();

	// ------------------------------------------------------------------
	//  UActorComponent overrides
	// ------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ------------------------------------------------------------------
	//  Observation
	// ------------------------------------------------------------------

	/**
	 * Feed an observation of a target into the threat model.
	 * Call this whenever the AI detects or re-observes a target.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Simulon")
	void UpdateThreatModel(AActor* Target);

	// ------------------------------------------------------------------
	//  Prediction
	// ------------------------------------------------------------------

	/**
	 * Predict where a target will be in the future.
	 * Uses velocity extrapolation plus behavioral pattern weighting.
	 *
	 * @param Target     The actor to predict.
	 * @param TimeAhead  Seconds into the future.
	 * @return           Predicted world location.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Simulon")
	FVector PredictTargetPosition(AActor* Target, float TimeAhead) const;

	// ------------------------------------------------------------------
	//  Queries
	// ------------------------------------------------------------------

	/** Get all active threat models. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Simulon")
	TArray<FSHThreatModel> GetThreatModels() const { return ThreatModels; }

	/** Get the threat model for a specific actor. Returns false if not tracked. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Simulon")
	bool GetThreatModelForActor(AActor* Target, FSHThreatModel& OutModel) const;

	/** Get the highest-threat model currently tracked. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Simulon")
	bool GetHighestThreat(FSHThreatModel& OutModel) const;

protected:
	/** Time (seconds) after which an unobserved threat model is pruned. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Primordia|Simulon|Config", meta = (ClampMin = "1"))
	float ThreatExpirationTime = 30.0f;

	/** Max number of threat models tracked simultaneously. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Primordia|Simulon|Config", meta = (ClampMin = "1"))
	int32 MaxTrackedThreats = 20;

	/** Confidence decay rate per second for unobserved targets. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Primordia|Simulon|Config", meta = (ClampMin = "0"))
	float ConfidenceDecayRate = 0.1f;

	/** Number of historical positions to store per target for pattern matching. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Primordia|Simulon|Config", meta = (ClampMin = "2"))
	int32 PositionHistorySize = 10;

	/** Weight given to velocity extrapolation vs pattern matching (0 = all pattern, 1 = all velocity). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Primordia|Simulon|Config", meta = (ClampMin = "0", ClampMax = "1"))
	float VelocityWeight = 0.7f;

private:
	/** Internal observation data per tracked target. */
	struct FTargetObservation
	{
		TWeakObjectPtr<AActor> Actor;
		TArray<FVector> PositionHistory;
		TArray<float> TimestampHistory;
		FVector LastVelocity = FVector::ZeroVector;
		float LastObservedTime = 0.0f;
		float ThreatLevel = 0.0f;
		float Confidence = 0.0f;
	};

	/** Find or create observation data for a target. */
	FTargetObservation* FindOrCreateObservation(AActor* Target);
	const FTargetObservation* FindObservation(AActor* Target) const;

	/** Compute velocity from position history. */
	FVector ComputeVelocity(const FTargetObservation& Obs) const;

	/** Compute behavioral pattern prediction (acceleration trend). */
	FVector ComputePatternPrediction(const FTargetObservation& Obs, float TimeAhead) const;

	/** Assess threat level based on distance, velocity toward us, etc. */
	float AssessThreatLevel(const FTargetObservation& Obs) const;

	/** Prune expired threat models. */
	void PruneExpiredModels();

	/** Rebuild the public ThreatModels array from internal data. */
	void RebuildThreatModels();

	/** Internal observation data. */
	TArray<FTargetObservation> Observations;

	/** Public-facing threat models (rebuilt each tick). */
	TArray<FSHThreatModel> ThreatModels;
};
