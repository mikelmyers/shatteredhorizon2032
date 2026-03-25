// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SHIndirectFireSystem.generated.h"

/**
 * Ordnance type for indirect fire missions.
 */
UENUM(BlueprintType)
enum class ESHOrdnanceType : uint8
{
	HE_60mm			UMETA(DisplayName = "60mm HE Mortar"),
	HE_81mm			UMETA(DisplayName = "81mm HE Mortar"),
	HE_120mm		UMETA(DisplayName = "120mm HE Mortar"),
	HE_155mm		UMETA(DisplayName = "155mm Artillery"),
	Smoke_81mm		UMETA(DisplayName = "81mm Smoke"),
	Illumination	UMETA(DisplayName = "Illumination"),
};

/**
 * Fire mission request status.
 */
UENUM(BlueprintType)
enum class ESHFireMissionState : uint8
{
	/** Awaiting observer confirmation. */
	Pending,
	/** Approved, rounds in flight. */
	InFlight,
	/** Rounds have impacted. */
	Complete,
	/** Mission denied (danger close refused, out of ammo, etc.). */
	Denied,
	/** Mission cancelled by observer. */
	Cancelled,
};

/**
 * A single fire mission request.
 */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHFireMission
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FGuid MissionId;

	UPROPERTY(BlueprintReadOnly)
	ESHOrdnanceType Ordnance = ESHOrdnanceType::HE_81mm;

	/** Target center in world space. */
	UPROPERTY(BlueprintReadOnly)
	FVector TargetLocation = FVector::ZeroVector;

	/** Number of rounds to fire. */
	UPROPERTY(BlueprintReadOnly)
	int32 RoundCount = 3;

	/** Time of flight in seconds from fire to impact. */
	UPROPERTY(BlueprintReadOnly)
	float TimeOfFlightSec = 0.f;

	/** Splash radius (cm). */
	UPROPERTY(BlueprintReadOnly)
	float SplashRadiusCm = 0.f;

	/** Base damage at center. */
	UPROPERTY(BlueprintReadOnly)
	float BaseDamage = 0.f;

	/** Seconds between rounds in a salvo (time spread). */
	UPROPERTY(BlueprintReadOnly)
	float SalvoIntervalSec = 1.5f;

	/** CEP — Circular Error Probable (cm). 50% of rounds land within this radius. */
	UPROPERTY(BlueprintReadOnly)
	float CEPCm = 0.f;

	/** Current state. */
	UPROPERTY(BlueprintReadOnly)
	ESHFireMissionState State = ESHFireMissionState::Pending;

	/** World time the mission was approved / rounds left the tube. */
	float FireTime = 0.f;

	/** Number of rounds already impacted. */
	int32 RoundsImpacted = 0;

	/** Whether this is a danger close mission (friendlies within splash). */
	UPROPERTY(BlueprintReadOnly)
	bool bIsDangerClose = false;

	/** Whether danger close was authorized. */
	UPROPERTY(BlueprintReadOnly)
	bool bDangerCloseAuthorized = false;

	/** Actor that called in the fire mission (observer). */
	UPROPERTY(BlueprintReadOnly)
	TWeakObjectPtr<AActor> Observer;

	/** Origin of the firing battery (for counter-battery detection). */
	UPROPERTY(BlueprintReadOnly)
	FVector FiringOrigin = FVector::ZeroVector;
};

// -----------------------------------------------------------------------
//  Delegates
// -----------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFireMissionStateChanged, const FSHFireMission&, Mission);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnIndirectImpact, FVector, ImpactLocation, ESHOrdnanceType, Ordnance);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnCounterBatteryDetected, FVector, DetectedFiringOrigin);

/**
 * USHIndirectFireSystem
 *
 * Manages indirect fire missions (mortars, artillery).
 * Implements doctrine requirements:
 *   - Observer-to-fire coordination (call-for-fire procedure)
 *   - Time of flight delay (8-12 seconds depending on ordnance)
 *   - CEP-based dispersion (not point-target)
 *   - Sound signatures (outgoing and incoming)
 *   - Counter-battery: AI can detect firing positions
 *   - Danger close authorization requirement
 */
UCLASS()
class SHATTEREDHORIZON2032_API USHIndirectFireSystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	void Tick(float DeltaSeconds);

	// ------------------------------------------------------------------
	//  Fire mission interface
	// ------------------------------------------------------------------

	/**
	 * Request a fire mission. Returns mission ID.
	 * Caller must be a valid observer with LOS to target area.
	 * If friendlies are within splash radius, mission requires danger close auth.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|IndirectFire")
	FGuid RequestFireMission(
		AActor* Observer,
		ESHOrdnanceType Ordnance,
		FVector TargetLocation,
		int32 RoundCount = 3);

	/** Authorize a danger close fire mission. */
	UFUNCTION(BlueprintCallable, Category = "SH|IndirectFire")
	bool AuthorizeDangerClose(const FGuid& MissionId);

	/** Cancel an active or pending fire mission. */
	UFUNCTION(BlueprintCallable, Category = "SH|IndirectFire")
	void CancelFireMission(const FGuid& MissionId);

	/** Query a fire mission by ID. */
	UFUNCTION(BlueprintCallable, Category = "SH|IndirectFire")
	bool GetFireMission(const FGuid& MissionId, FSHFireMission& OutMission) const;

	// ------------------------------------------------------------------
	//  Counter-battery
	// ------------------------------------------------------------------

	/**
	 * Report an incoming indirect fire impact for counter-battery analysis.
	 * AI systems call this when they detect impacts to attempt to triangulate
	 * the firing origin.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|IndirectFire|CounterBattery")
	void ReportIncomingImpact(FVector ImpactLocation, float EstimatedTOF);

	/** Enemy battery positions detected by counter-battery analysis. */
	UFUNCTION(BlueprintPure, Category = "SH|IndirectFire|CounterBattery")
	TArray<FVector> GetDetectedBatteryPositions() const { return DetectedBatteries; }

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Friendly detection radius for danger close check (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|IndirectFire|Config")
	float DangerCloseRadiusCm = 60000.f; // 600m

	/** Available fire missions remaining (limited ammo). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|IndirectFire|Config")
	int32 AvailableFireMissions = 5;

	/** Firing battery position (friendly). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|IndirectFire|Config")
	FVector FriendlyBatteryPosition = FVector(-500000.f, 0.f, 0.f);

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|IndirectFire")
	FOnFireMissionStateChanged OnFireMissionStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "SH|IndirectFire")
	FOnIndirectImpact OnIndirectImpact;

	UPROPERTY(BlueprintAssignable, Category = "SH|IndirectFire")
	FOnCounterBatteryDetected OnCounterBatteryDetected;

private:
	// ------------------------------------------------------------------
	//  Internal
	// ------------------------------------------------------------------

	/** Populate ordnance parameters (TOF, splash, damage, CEP). */
	void GetOrdnanceParams(ESHOrdnanceType Type, float& OutTOF, float& OutSplashCm, float& OutDamage, float& OutCEP) const;

	/** Check if any friendlies are within danger close radius of target. */
	bool CheckDangerClose(FVector TargetLocation) const;

	/** Apply a single round impact at location. */
	void ApplyImpact(const FSHFireMission& Mission, FVector ImpactLocation);

	/** Compute a CEP-dispersed impact point. */
	FVector ComputeDispersedImpact(FVector Center, float CEP) const;

	/** World tick delegate handle. */
	FDelegateHandle TickDelegateHandle;

	/** Active fire missions. */
	TArray<FSHFireMission> ActiveMissions;

	/** Detected enemy battery positions (counter-battery). */
	TArray<FVector> DetectedBatteries;

	/** Recent incoming impact data for counter-battery triangulation. */
	struct FCounterBatteryData
	{
		FVector ImpactLocation;
		float EstimatedTOF;
		float TimeReceived;
	};
	TArray<FCounterBatteryData> RecentIncomingImpacts;
};
