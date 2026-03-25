// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SHElectronicWarfare.h"
#include "SHCommsDisruption.generated.h"

class UAudioComponent;
class USoundBase;
class USoundAttenuation;

// ============================================================================
//  Comms Status
// ============================================================================

UENUM(BlueprintType)
enum class ESHCommsStatus : uint8
{
	Clear			UMETA(DisplayName = "Clear"),
	LightStatic		UMETA(DisplayName = "Light Static"),
	HeavyStatic		UMETA(DisplayName = "Heavy Static"),
	Garbled			UMETA(DisplayName = "Garbled"),
	Blackout		UMETA(DisplayName = "Total Blackout")
};

// ============================================================================
//  Squad Order Disruption Info — how an order is degraded
// ============================================================================

USTRUCT(BlueprintType)
struct FSHOrderDisruptionInfo
{
	GENERATED_BODY()

	/** Whether the order was delayed. */
	UPROPERTY(BlueprintReadOnly, Category = "SH|Comms")
	bool bDelayed = false;

	/** Delay duration in seconds. */
	UPROPERTY(BlueprintReadOnly, Category = "SH|Comms")
	float DelaySeconds = 0.f;

	/** Whether the order content was garbled (may be misunderstood). */
	UPROPERTY(BlueprintReadOnly, Category = "SH|Comms")
	bool bGarbled = false;

	/** Whether the order was completely lost. */
	UPROPERTY(BlueprintReadOnly, Category = "SH|Comms")
	bool bLost = false;

	/** Whether acknowledgment can be received back. */
	UPROPERTY(BlueprintReadOnly, Category = "SH|Comms")
	bool bAckPossible = true;

	/** Probability that acknowledgment arrives (0-1). */
	UPROPERTY(BlueprintReadOnly, Category = "SH|Comms")
	float AckProbability = 1.f;
};

// ============================================================================
//  HUD Degradation Info — how HUD elements are affected
// ============================================================================

USTRUCT(BlueprintType)
struct FSHHUDDegradationInfo
{
	GENERATED_BODY()

	/** Minimap visibility/clarity (0 = invisible, 1 = clear). */
	UPROPERTY(BlueprintReadOnly, Category = "SH|Comms|HUD")
	float MinimapClarity = 1.f;

	/** Compass accuracy (0 = wildly wrong, 1 = accurate). */
	UPROPERTY(BlueprintReadOnly, Category = "SH|Comms|HUD")
	float CompassAccuracy = 1.f;

	/** Compass drift in degrees (applied as noise). */
	UPROPERTY(BlueprintReadOnly, Category = "SH|Comms|HUD")
	float CompassDriftDegrees = 0.f;

	/** Squad marker visibility (0 = invisible, 1 = visible). */
	UPROPERTY(BlueprintReadOnly, Category = "SH|Comms|HUD")
	float SquadMarkerVisibility = 1.f;

	/** Waypoint marker visibility. */
	UPROPERTY(BlueprintReadOnly, Category = "SH|Comms|HUD")
	float WaypointVisibility = 1.f;

	/** Objective marker visibility. */
	UPROPERTY(BlueprintReadOnly, Category = "SH|Comms|HUD")
	float ObjectiveMarkerVisibility = 1.f;

	/** Whether GPS coordinates should display noise/errors. */
	UPROPERTY(BlueprintReadOnly, Category = "SH|Comms|HUD")
	bool bGPSNoisy = false;

	/** GPS position error in cm (for displaying incorrect coordinates). */
	UPROPERTY(BlueprintReadOnly, Category = "SH|Comms|HUD")
	float GPSErrorCm = 0.f;

	/** Radio static overlay intensity (0-1). */
	UPROPERTY(BlueprintReadOnly, Category = "SH|Comms|HUD")
	float RadioStaticOverlay = 0.f;
};

// ============================================================================
//  Delegate Declarations
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSHOnCommsStatusChanged, ESHCommsStatus, OldStatus, ESHCommsStatus, NewStatus);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHOnOrderDisrupted, const FSHOrderDisruptionInfo&, DisruptionInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHOnHUDDegradationChanged, const FSHHUDDegradationInfo&, DegradationInfo);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSHOnCommsRestored);

// ============================================================================
//  USHCommsDisruption — Communications disruption component
// ============================================================================

/**
 * USHCommsDisruption
 *
 * ActorComponent attached to player characters that handles communications
 * disruption effects when in EW jammed zones. Manages order delay/garbling,
 * radio static audio, squad autonomy, and HUD degradation (minimap, compass,
 * markers). Can be countered by moving to non-jammed areas or using backup comms.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHCommsDisruption : public UActorComponent
{
	GENERATED_BODY()

public:
	USHCommsDisruption();

	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ------------------------------------------------------------------
	//  Comms status
	// ------------------------------------------------------------------

	/** Get current communications status. */
	UFUNCTION(BlueprintPure, Category = "SH|Comms")
	ESHCommsStatus GetCommsStatus() const { return CurrentCommsStatus; }

	/** Get current comms jamming strength (0-1). */
	UFUNCTION(BlueprintPure, Category = "SH|Comms")
	float GetCommsJammingStrength() const { return CurrentCommsJamming; }

	/** Check if comms are degraded in any way. */
	UFUNCTION(BlueprintPure, Category = "SH|Comms")
	bool AreCommsDegraded() const { return CurrentCommsStatus != ESHCommsStatus::Clear; }

	/** Check if comms are in total blackout. */
	UFUNCTION(BlueprintPure, Category = "SH|Comms")
	bool IsCommsBlackout() const { return CurrentCommsStatus == ESHCommsStatus::Blackout; }

	// ------------------------------------------------------------------
	//  Order disruption
	// ------------------------------------------------------------------

	/** Evaluate how an outgoing order is disrupted by current comms status. */
	UFUNCTION(BlueprintPure, Category = "SH|Comms")
	FSHOrderDisruptionInfo EvaluateOrderDisruption() const;

	/** Check if an incoming acknowledgment should be received. */
	UFUNCTION(BlueprintPure, Category = "SH|Comms")
	bool ShouldReceiveAcknowledgment() const;

	/** Get the autonomy multiplier for squad AI (higher = more autonomous). */
	UFUNCTION(BlueprintPure, Category = "SH|Comms")
	float GetSquadAutonomyMultiplier() const;

	// ------------------------------------------------------------------
	//  HUD degradation
	// ------------------------------------------------------------------

	/** Get the current HUD degradation state. */
	UFUNCTION(BlueprintPure, Category = "SH|Comms")
	const FSHHUDDegradationInfo& GetHUDDegradation() const { return CurrentHUDDegradation; }

	/** Get the current compass reading with drift applied. */
	UFUNCTION(BlueprintPure, Category = "SH|Comms")
	float GetDriftedCompassBearing(float TrueBearing) const;

	/** Get minimap static/noise intensity (0-1). */
	UFUNCTION(BlueprintPure, Category = "SH|Comms")
	float GetMinimapStaticIntensity() const;

	// ------------------------------------------------------------------
	//  Backup comms
	// ------------------------------------------------------------------

	/** Activate backup communications (short-range, limited duration). */
	UFUNCTION(BlueprintCallable, Category = "SH|Comms")
	bool ActivateBackupComms();

	/** Check if backup comms are active. */
	UFUNCTION(BlueprintPure, Category = "SH|Comms")
	bool AreBackupCommsActive() const { return bBackupCommsActive; }

	/** Get remaining backup comms duration. */
	UFUNCTION(BlueprintPure, Category = "SH|Comms")
	float GetBackupCommsDuration() const { return BackupCommsTimer; }

	// ------------------------------------------------------------------
	//  Audio feedback
	// ------------------------------------------------------------------

	/** Get radio static volume (0-1) for audio mixing. */
	UFUNCTION(BlueprintPure, Category = "SH|Comms")
	float GetRadioStaticVolume() const;

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Comms|Events")
	FSHOnCommsStatusChanged OnCommsStatusChanged;

	UPROPERTY(BlueprintAssignable, Category = "SH|Comms|Events")
	FSHOnOrderDisrupted OnOrderDisrupted;

	UPROPERTY(BlueprintAssignable, Category = "SH|Comms|Events")
	FSHOnHUDDegradationChanged OnHUDDegradationChanged;

	UPROPERTY(BlueprintAssignable, Category = "SH|Comms|Events")
	FSHOnCommsRestored OnCommsRestored;

protected:
	// ------------------------------------------------------------------
	//  Configuration — Disruption thresholds
	// ------------------------------------------------------------------

	/** Jamming strength threshold for light static. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Comms|Thresholds")
	float LightStaticThreshold = 0.15f;

	/** Jamming strength threshold for heavy static. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Comms|Thresholds")
	float HeavyStaticThreshold = 0.4f;

	/** Jamming strength threshold for garbled comms. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Comms|Thresholds")
	float GarbledThreshold = 0.65f;

	/** Jamming strength threshold for total blackout. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Comms|Thresholds")
	float BlackoutThreshold = 0.85f;

	// ------------------------------------------------------------------
	//  Configuration — Order disruption
	// ------------------------------------------------------------------

	/** Base delay applied to orders when comms are degraded (seconds). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Comms|Orders")
	float BaseOrderDelay = 0.5f;

	/** Maximum additional random delay (seconds). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Comms|Orders")
	float MaxRandomDelay = 3.f;

	/** Probability of order being garbled at garbled comms threshold. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Comms|Orders")
	float GarbleProbability = 0.3f;

	/** Probability of order being completely lost at blackout. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Comms|Orders")
	float OrderLossProbability = 0.7f;

	// ------------------------------------------------------------------
	//  Configuration — HUD degradation
	// ------------------------------------------------------------------

	/** Maximum compass drift in degrees at full GPS denial. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Comms|HUD")
	float MaxCompassDrift = 45.f;

	/** Compass drift oscillation speed (Hz). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Comms|HUD")
	float CompassDriftFrequency = 0.3f;

	/** Maximum GPS position error in cm at full denial. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Comms|HUD")
	float MaxGPSError = 50000.f;

	// ------------------------------------------------------------------
	//  Configuration — Squad autonomy
	// ------------------------------------------------------------------

	/** Base autonomy multiplier when comms are clear. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Comms|Squad")
	float BaseAutonomyMultiplier = 1.f;

	/** Maximum autonomy multiplier at total blackout. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Comms|Squad")
	float MaxAutonomyMultiplier = 3.f;

	// ------------------------------------------------------------------
	//  Configuration — Backup comms
	// ------------------------------------------------------------------

	/** Duration of backup comms in seconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Comms|Backup")
	float BackupCommsDuration = 30.f;

	/** Number of backup comms uses available. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Comms|Backup")
	int32 MaxBackupCommsUses = 2;

	/** Effectiveness of backup comms (how much it reduces jamming, 0-1). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Comms|Backup")
	float BackupCommsEffectiveness = 0.7f;

	// ------------------------------------------------------------------
	//  Configuration — Audio
	// ------------------------------------------------------------------

	/** Radio static loop sound. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Comms|Audio")
	TObjectPtr<USoundBase> RadioStaticSound;

	/** Radio crackle/pop sound (played intermittently). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Comms|Audio")
	TObjectPtr<USoundBase> RadioCrackleSound;

	/** Sound when comms are restored. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Comms|Audio")
	TObjectPtr<USoundBase> CommsRestoredSound;

	/** Sound when entering a jammed zone. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Comms|Audio")
	TObjectPtr<USoundBase> CommsJammedSound;

	// ------------------------------------------------------------------
	//  Internal
	// ------------------------------------------------------------------
	void TickCommsStatus(float DeltaSeconds);
	void TickHUDDegradation(float DeltaSeconds);
	void TickAudioFeedback(float DeltaSeconds);
	void TickBackupComms(float DeltaSeconds);

	/** Compute comms status from current jamming strength. */
	ESHCommsStatus ComputeCommsStatus(float JammingStrength) const;

	/** Update the HUD degradation struct based on current EW state. */
	void UpdateHUDDegradation(float GPSDenial, float CommsJamming, float RadarDisruption);

	/** Play comms state transition audio. */
	void PlayCommsTransitionAudio(ESHCommsStatus OldStatus, ESHCommsStatus NewStatus);

	// ------------------------------------------------------------------
	//  Runtime state
	// ------------------------------------------------------------------

	ESHCommsStatus CurrentCommsStatus = ESHCommsStatus::Clear;
	float CurrentCommsJamming = 0.f;
	float CurrentGPSDenial = 0.f;
	float CurrentRadarDisruption = 0.f;

	FSHHUDDegradationInfo CurrentHUDDegradation;

	/** Cached EW manager reference. */
	TWeakObjectPtr<ASHElectronicWarfare> EWManager;

	/** EW query update interval. */
	float EWQueryInterval = 0.25f;
	float EWQueryAccumulator = 0.f;

	/** Radio static audio component. */
	UPROPERTY()
	TObjectPtr<UAudioComponent> RadioStaticAudioComponent;

	/** Audio crackle accumulator. */
	float CrackleAccumulator = 0.f;
	float NextCrackleTime = 0.f;

	/** Compass drift state (smoothly interpolated). */
	float CurrentCompassDrift = 0.f;
	float TargetCompassDrift = 0.f;
	float CompassDriftPhase = 0.f;

	/** Backup comms state. */
	bool bBackupCommsActive = false;
	float BackupCommsTimer = 0.f;
	int32 RemainingBackupCommsUses;

	/** GPS error offset (smoothly drifts). */
	FVector GPSErrorOffset = FVector::ZeroVector;
	FVector TargetGPSError = FVector::ZeroVector;
};
