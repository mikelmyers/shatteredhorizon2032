// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SHDroneBase.h"
#include "SHISRDrone.generated.h"

class APawn;

// ============================================================================
//  ISR Camera Mode
// ============================================================================

UENUM(BlueprintType)
enum class ESHISRCameraMode : uint8
{
	Daylight	UMETA(DisplayName = "Daylight / Optical"),
	ThermalIR	UMETA(DisplayName = "Thermal / IR"),
	NightVision	UMETA(DisplayName = "Night Vision")
};

// ============================================================================
//  Patrol Pattern
// ============================================================================

UENUM(BlueprintType)
enum class ESHPatrolPattern : uint8
{
	Linear		UMETA(DisplayName = "Linear Waypoint"),
	Orbit		UMETA(DisplayName = "Orbit / Loiter"),
	Racetrack	UMETA(DisplayName = "Racetrack"),
	SearchGrid	UMETA(DisplayName = "Search Grid")
};

// ============================================================================
//  Detection Entry — a detected actor
// ============================================================================

USTRUCT(BlueprintType)
struct FSHDroneDetection
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SH|Drone|Detection")
	TWeakObjectPtr<AActor> DetectedActor;

	/** World location when first detected. */
	UPROPERTY(BlueprintReadOnly, Category = "SH|Drone|Detection")
	FVector DetectionLocation = FVector::ZeroVector;

	/** Server timestamp of initial detection. */
	UPROPERTY(BlueprintReadOnly, Category = "SH|Drone|Detection")
	float DetectionTime = 0.f;

	/** Confidence level 0-1 (affected by distance, movement, camouflage). */
	UPROPERTY(BlueprintReadOnly, Category = "SH|Drone|Detection")
	float Confidence = 0.f;

	/** Whether this detection has been relayed to Primordia AI. */
	UPROPERTY(BlueprintReadOnly, Category = "SH|Drone|Detection")
	bool bRelayedToCommand = false;

	/** Whether the target is still within the detection cone. */
	UPROPERTY(BlueprintReadOnly, Category = "SH|Drone|Detection")
	bool bCurrentlyTracked = false;
};

// ============================================================================
//  Delegate Declarations
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSHOnEnemyDetected, AActor*, DetectedActor, float, Confidence);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHOnEnemyLost, AActor*, LostActor);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHOnDetectionRelayed, const FSHDroneDetection&, Detection);

// ============================================================================
//  ASHISRDrone — Intelligence, Surveillance, Reconnaissance drone
// ============================================================================

/**
 * ASHISRDrone
 *
 * Enemy-operated surveillance drone that detects player and squad members.
 * Flies patrol patterns or orbits areas of interest. Increases enemy tactical
 * awareness when it spots friendly forces. Features daylight and thermal/IR
 * camera modes for day and night operations.
 */
UCLASS(Blueprintable)
class SHATTEREDHORIZON2032_API ASHISRDrone : public ASHDroneBase
{
	GENERATED_BODY()

public:
	ASHISRDrone();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ------------------------------------------------------------------
	//  Detection interface
	// ------------------------------------------------------------------

	/** Get all currently tracked detections. */
	UFUNCTION(BlueprintPure, Category = "SH|Drone|ISR")
	const TArray<FSHDroneDetection>& GetActiveDetections() const { return ActiveDetections; }

	/** Get the number of currently tracked targets. */
	UFUNCTION(BlueprintPure, Category = "SH|Drone|ISR")
	int32 GetTrackedTargetCount() const;

	/** Check if a specific actor is currently detected. */
	UFUNCTION(BlueprintPure, Category = "SH|Drone|ISR")
	bool IsActorDetected(AActor* Actor) const;

	// ------------------------------------------------------------------
	//  Camera control
	// ------------------------------------------------------------------

	/** Set the camera mode (daylight, thermal, night vision). */
	UFUNCTION(BlueprintCallable, Category = "SH|Drone|ISR")
	void SetCameraMode(ESHISRCameraMode NewMode);

	UFUNCTION(BlueprintPure, Category = "SH|Drone|ISR")
	ESHISRCameraMode GetCameraMode() const { return CurrentCameraMode; }

	// ------------------------------------------------------------------
	//  Patrol control
	// ------------------------------------------------------------------

	/** Set patrol pattern type. */
	UFUNCTION(BlueprintCallable, Category = "SH|Drone|ISR")
	void SetPatrolPattern(ESHPatrolPattern Pattern);

	/** Set waypoints for linear/racetrack patrol. */
	UFUNCTION(BlueprintCallable, Category = "SH|Drone|ISR")
	void SetPatrolWaypoints(const TArray<FVector>& Waypoints);

	/** Set the orbit center and radius. */
	UFUNCTION(BlueprintCallable, Category = "SH|Drone|ISR")
	void SetOrbitParameters(const FVector& Center, float RadiusCm);

	/** Override to orbit a specific actor (e.g., spotted player). */
	UFUNCTION(BlueprintCallable, Category = "SH|Drone|ISR")
	void OrbitActor(AActor* TargetActor);

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Drone|ISR|Events")
	FSHOnEnemyDetected OnEnemyDetected;

	UPROPERTY(BlueprintAssignable, Category = "SH|Drone|ISR|Events")
	FSHOnEnemyLost OnEnemyLost;

	UPROPERTY(BlueprintAssignable, Category = "SH|Drone|ISR|Events")
	FSHOnDetectionRelayed OnDetectionRelayed;

protected:
	// ------------------------------------------------------------------
	//  ASHDroneBase overrides
	// ------------------------------------------------------------------
	virtual void TickDroneBehavior(float DeltaSeconds) override;
	virtual void OnStateEntered(ESHDroneState NewState) override;
	virtual void OnStateExited(ESHDroneState OldState) override;

	// ------------------------------------------------------------------
	//  Detection tick
	// ------------------------------------------------------------------
	void TickDetectionCone(float DeltaSeconds);
	void TickDetectionRelay(float DeltaSeconds);
	void TickPatrolMovement(float DeltaSeconds);

	/** Evaluate whether a given actor is within the detection cone. */
	bool IsInDetectionCone(AActor* Target, float& OutConfidence) const;

	/** Compute detection confidence for a target based on range, visibility, etc. */
	float ComputeDetectionConfidence(AActor* Target, float DistanceCm) const;

	/** Check line-of-sight to target. */
	bool HasLineOfSight(AActor* Target) const;

	/** Relay a detection to the Primordia AI / enemy tactical awareness. */
	void RelayDetectionToCommand(FSHDroneDetection& Detection);

	/** Remove stale detections (targets that left the cone or are destroyed). */
	void PruneStaleDetections();

	/** Advance to the next patrol waypoint. */
	void AdvanceToNextWaypoint();

	/** Compute orbit position at a given angle. */
	FVector ComputeOrbitPosition(float AngleRadians) const;

	// ------------------------------------------------------------------
	//  Configuration — Camera / Detection
	// ------------------------------------------------------------------

	/** Detection cone half-angle in degrees. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|ISR|Camera")
	float DetectionConeHalfAngle = 30.f;

	/** Maximum detection range in cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|ISR|Camera")
	float MaxDetectionRange = 15000.f;

	/** Minimum time a target must be in the cone to be detected (seconds). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|ISR|Camera")
	float DetectionAcquisitionTime = 1.5f;

	/** Detection scan interval in seconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|ISR|Camera")
	float ScanInterval = 0.5f;

	/** Confidence threshold to relay detection to command (0-1). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|ISR|Camera")
	float RelayConfidenceThreshold = 0.6f;

	/** Delay before relaying detection to Primordia (seconds). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|ISR|Camera")
	float RelayDelay = 2.f;

	/** Thermal camera detection range bonus (multiplier). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|ISR|Camera")
	float ThermalRangeMultiplier = 1.3f;

	/** Thermal camera ignores certain concealment (bushes, light cover). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|ISR|Camera")
	bool bThermalIgnoresLightCover = true;

	/** Detection range penalty at night for daylight camera (multiplier). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|ISR|Camera")
	float NightDetectionPenalty = 0.3f;

	// ------------------------------------------------------------------
	//  Configuration — Patrol
	// ------------------------------------------------------------------

	/** Default patrol altitude in cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|ISR|Patrol")
	float PatrolAltitude = 8000.f;

	/** Orbit radius in cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|ISR|Patrol")
	float OrbitRadius = 5000.f;

	/** Orbit angular speed in degrees/second. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|ISR|Patrol")
	float OrbitAngularSpeed = 15.f;

	/** Waypoint arrival threshold in cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|ISR|Patrol")
	float WaypointArrivalDistance = 300.f;

	/** Dwell time at each waypoint (seconds) before advancing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|ISR|Patrol")
	float WaypointDwellTime = 5.f;

	/** Search grid cell size in cm (for grid pattern). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|ISR|Patrol")
	float SearchGridCellSize = 3000.f;

	// ------------------------------------------------------------------
	//  Configuration — Audio
	// ------------------------------------------------------------------

	/** Distinct ISR drone buzz sound (overrides base motor sound). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|ISR|Audio")
	TObjectPtr<USoundBase> ISRBuzzSound;

	// ------------------------------------------------------------------
	//  Configuration — Tactical Awareness
	// ------------------------------------------------------------------

	/** How much enemy tactical awareness increases per relayed detection (0-1). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|ISR|Tactics")
	float AwarenessBoostPerDetection = 0.15f;

	/** Collision channel used for detection line-of-sight checks. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|ISR|Tactics")
	TEnumAsByte<ECollisionChannel> DetectionTraceChannel = ECC_Visibility;

	// ------------------------------------------------------------------
	//  Runtime state
	// ------------------------------------------------------------------

	UPROPERTY()
	ESHISRCameraMode CurrentCameraMode = ESHISRCameraMode::Daylight;

	UPROPERTY()
	ESHPatrolPattern CurrentPatrolPattern = ESHPatrolPattern::Orbit;

	TArray<FSHDroneDetection> ActiveDetections;

	/** Patrol waypoints. */
	TArray<FVector> PatrolWaypoints;
	int32 CurrentWaypointIndex = 0;
	float WaypointDwellTimer = 0.f;
	bool bDwellingAtWaypoint = false;

	/** Orbit state. */
	FVector OrbitCenter = FVector::ZeroVector;
	float CurrentOrbitAngle = 0.f;

	/** Actor to orbit (if set, overrides fixed orbit center). */
	TWeakObjectPtr<AActor> OrbitTargetActor;

	/** Scan accumulator. */
	float ScanAccumulator = 0.f;

	/** Time-in-cone accumulator per potential target. */
	TMap<TWeakObjectPtr<AActor>, float> ConeTimeAccumulator;

	/** Relay timer per detection (indexes into ActiveDetections). */
	TMap<TWeakObjectPtr<AActor>, float> RelayTimerMap;

	/** Whether it's currently nighttime (cached from game state). */
	bool bIsNightTime = false;
};
