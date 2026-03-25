// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SHElectronicWarfare.generated.h"

class ASHDroneBase;

// ============================================================================
//  EW Effect Types
// ============================================================================

UENUM(BlueprintType)
enum class ESHEWEffectType : uint8
{
	None				UMETA(DisplayName = "None"),
	GPSDenial			UMETA(DisplayName = "GPS Denial"),
	CommsJamming		UMETA(DisplayName = "Communications Jamming"),
	RadarDisruption		UMETA(DisplayName = "Radar / Sensor Disruption"),
	DroneJamming		UMETA(DisplayName = "Drone Jamming"),
	FullSpectrum		UMETA(DisplayName = "Full Spectrum Denial")
};

// ============================================================================
//  EW Zone — a region of the battlefield with EW effects
// ============================================================================

USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHEWZone
{
	GENERATED_BODY()

	/** Unique zone identifier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|EW")
	FName ZoneID;

	/** World position of the zone center. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|EW")
	FVector Center = FVector::ZeroVector;

	/** Effective radius of the zone in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|EW")
	float Radius = 10000.f;

	/** EW effect strength at center (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|EW", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Strength = 0.8f;

	/** Type of EW effect in this zone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|EW")
	ESHEWEffectType EffectType = ESHEWEffectType::CommsJamming;

	/** Whether this zone is currently active. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|EW")
	bool bActive = true;

	/** Whether this is a friendly or enemy EW zone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|EW")
	bool bFriendly = false;

	/** Duration this zone remains active (-1 = permanent until removed). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|EW")
	float Duration = -1.f;

	/** Elapsed time since activation (used for duration tracking). */
	float ElapsedTime = 0.f;

	/** Compute the EW effect strength at a given world position. */
	float GetStrengthAtPosition(const FVector& Position) const
	{
		if (!bActive) return 0.f;
		const float Distance = FVector::Dist(Center, Position);
		if (Distance > Radius) return 0.f;
		// Quadratic falloff from center
		const float NormalizedDist = Distance / Radius;
		return Strength * (1.f - NormalizedDist * NormalizedDist);
	}
};

// ============================================================================
//  EW Query Result — what EW effects affect a given position
// ============================================================================

USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHEWQueryResult
{
	GENERATED_BODY()

	/** GPS denial strength (0 = none, 1 = full denial). */
	UPROPERTY(BlueprintReadOnly, Category = "SH|EW")
	float GPSDenialStrength = 0.f;

	/** Communications jamming strength. */
	UPROPERTY(BlueprintReadOnly, Category = "SH|EW")
	float CommsJammingStrength = 0.f;

	/** Radar/sensor disruption strength. */
	UPROPERTY(BlueprintReadOnly, Category = "SH|EW")
	float RadarDisruptionStrength = 0.f;

	/** Drone jamming strength. */
	UPROPERTY(BlueprintReadOnly, Category = "SH|EW")
	float DroneJammingStrength = 0.f;

	/** Whether any EW effect is active at this position. */
	UPROPERTY(BlueprintReadOnly, Category = "SH|EW")
	bool bAnyEffectActive = false;

	/** Strongest single effect strength. */
	UPROPERTY(BlueprintReadOnly, Category = "SH|EW")
	float MaxEffectStrength = 0.f;

	/** Names of zones affecting this position. */
	UPROPERTY(BlueprintReadOnly, Category = "SH|EW")
	TArray<FName> AffectingZoneIDs;

	/** Get the overall EW disruption level (composite of all effects). */
	float GetOverallDisruption() const
	{
		return FMath::Clamp(GPSDenialStrength + CommsJammingStrength + RadarDisruptionStrength + DroneJammingStrength, 0.f, 1.f);
	}
};

// ============================================================================
//  Delegate Declarations
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSHOnEWZoneActivated, const FName&, ZoneID, ESHEWEffectType, EffectType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHOnEWZoneDeactivated, const FName&, ZoneID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSHOnEWEscalation, int32, EscalationLevel, float, GlobalEWStrength);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSHOnActorEnteredEWZone, AActor*, Actor, const FSHEWZone&, Zone);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSHOnActorExitedEWZone, AActor*, Actor, const FName&, ZoneID);

// ============================================================================
//  ASHElectronicWarfare — EW System Manager
// ============================================================================

/**
 * ASHElectronicWarfare
 *
 * Singleton-pattern battlefield manager for all electronic warfare effects.
 * Manages GPS denial zones, communications jamming, radar disruption,
 * and drone interference across the map. Provides an EW strength map
 * and supports escalation over time as the enemy deploys more EW assets.
 */
UCLASS(Blueprintable)
class SHATTEREDHORIZON2032_API ASHElectronicWarfare : public AActor
{
	GENERATED_BODY()

public:
	ASHElectronicWarfare();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ------------------------------------------------------------------
	//  Singleton access
	// ------------------------------------------------------------------

	/** Get the EW manager instance in the current world. */
	UFUNCTION(BlueprintPure, Category = "SH|EW", meta = (WorldContext = "WorldContextObject"))
	static ASHElectronicWarfare* GetEWManager(const UObject* WorldContextObject);

	// ------------------------------------------------------------------
	//  Zone management
	// ------------------------------------------------------------------

	/** Create and activate a new EW zone. Returns the zone ID. */
	UFUNCTION(BlueprintCallable, Category = "SH|EW")
	FName CreateEWZone(const FVector& Center, float Radius, float Strength,
		ESHEWEffectType EffectType, bool bFriendly, float Duration = -1.f);

	/** Remove an EW zone by ID. */
	UFUNCTION(BlueprintCallable, Category = "SH|EW")
	void RemoveEWZone(const FName& ZoneID);

	/** Activate/deactivate an existing zone. */
	UFUNCTION(BlueprintCallable, Category = "SH|EW")
	void SetZoneActive(const FName& ZoneID, bool bActive);

	/** Modify an existing zone's parameters. */
	UFUNCTION(BlueprintCallable, Category = "SH|EW")
	void ModifyZone(const FName& ZoneID, float NewStrength, float NewRadius);

	/** Get all active EW zones. */
	UFUNCTION(BlueprintPure, Category = "SH|EW")
	const TArray<FSHEWZone>& GetAllZones() const { return EWZones; }

	/** Get number of active enemy EW zones. */
	UFUNCTION(BlueprintPure, Category = "SH|EW")
	int32 GetActiveEnemyZoneCount() const;

	// ------------------------------------------------------------------
	//  Queries
	// ------------------------------------------------------------------

	/** Query all EW effects at a given world position. */
	UFUNCTION(BlueprintPure, Category = "SH|EW")
	FSHEWQueryResult QueryEWAtPosition(const FVector& Position) const;

	/** Get GPS denial strength at a position (0-1). */
	UFUNCTION(BlueprintPure, Category = "SH|EW")
	float GetGPSDenialAtPosition(const FVector& Position) const;

	/** Get communications jamming strength at a position (0-1). */
	UFUNCTION(BlueprintPure, Category = "SH|EW")
	float GetCommsJammingAtPosition(const FVector& Position) const;

	/** Get radar disruption strength at a position (0-1). */
	UFUNCTION(BlueprintPure, Category = "SH|EW")
	float GetRadarDisruptionAtPosition(const FVector& Position) const;

	/** Get drone jamming strength at a position (0-1). */
	UFUNCTION(BlueprintPure, Category = "SH|EW")
	float GetDroneJammingAtPosition(const FVector& Position) const;

	/** Check if a position is in any EW zone. */
	UFUNCTION(BlueprintPure, Category = "SH|EW")
	bool IsPositionInEWZone(const FVector& Position) const;

	/** Check if a position is in a friendly EW zone. */
	UFUNCTION(BlueprintPure, Category = "SH|EW")
	bool IsPositionInFriendlyEW(const FVector& Position) const;

	// ------------------------------------------------------------------
	//  EW Support (friendly)
	// ------------------------------------------------------------------

	/** Request friendly EW support at a position (called by player/squad leader). */
	UFUNCTION(BlueprintCallable, Category = "SH|EW")
	bool RequestFriendlyEWSupport(const FVector& Position, float Radius, ESHEWEffectType EffectType, float Duration);

	/** Get remaining friendly EW support calls. */
	UFUNCTION(BlueprintPure, Category = "SH|EW")
	int32 GetRemainingFriendlyEWSupportCalls() const { return FriendlyEWSupportCalls; }

	// ------------------------------------------------------------------
	//  Escalation
	// ------------------------------------------------------------------

	/** Get current enemy EW escalation level (0+). */
	UFUNCTION(BlueprintPure, Category = "SH|EW")
	int32 GetEscalationLevel() const { return EscalationLevel; }

	/** Get the global enemy EW strength multiplier. */
	UFUNCTION(BlueprintPure, Category = "SH|EW")
	float GetGlobalEWStrength() const { return GlobalEWStrengthMultiplier; }

	/** Force an escalation (debug / scripted). */
	UFUNCTION(BlueprintCallable, Category = "SH|EW")
	void ForceEscalation(int32 NewLevel);

	// ------------------------------------------------------------------
	//  Visual / Audio indicators
	// ------------------------------------------------------------------

	/** Get compass drift amount in degrees for a given GPS denial strength. */
	UFUNCTION(BlueprintPure, Category = "SH|EW")
	static float ComputeCompassDrift(float GPSDenialStrength);

	/** Get radio static intensity for a given comms jamming strength. */
	UFUNCTION(BlueprintPure, Category = "SH|EW")
	static float ComputeRadioStaticIntensity(float CommsJammingStrength);

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|EW|Events")
	FSHOnEWZoneActivated OnEWZoneActivated;

	UPROPERTY(BlueprintAssignable, Category = "SH|EW|Events")
	FSHOnEWZoneDeactivated OnEWZoneDeactivated;

	UPROPERTY(BlueprintAssignable, Category = "SH|EW|Events")
	FSHOnEWEscalation OnEWEscalation;

	UPROPERTY(BlueprintAssignable, Category = "SH|EW|Events")
	FSHOnActorEnteredEWZone OnActorEnteredEWZone;

	UPROPERTY(BlueprintAssignable, Category = "SH|EW|Events")
	FSHOnActorExitedEWZone OnActorExitedEWZone;

protected:
	// ------------------------------------------------------------------
	//  Configuration — Escalation
	// ------------------------------------------------------------------

	/** Time between escalation levels (seconds). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|EW|Escalation")
	float EscalationInterval = 300.f;

	/** Maximum escalation level. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|EW|Escalation")
	int32 MaxEscalationLevel = 5;

	/** EW strength multiplier increase per escalation level. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|EW|Escalation")
	float StrengthPerEscalation = 0.15f;

	/** Number of new enemy EW zones spawned per escalation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|EW|Escalation")
	int32 ZonesPerEscalation = 1;

	/** Radius range for auto-generated escalation zones. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|EW|Escalation")
	FVector2D EscalationZoneRadiusRange = FVector2D(8000.f, 20000.f);

	// ------------------------------------------------------------------
	//  Configuration — Friendly EW Support
	// ------------------------------------------------------------------

	/** Total friendly EW support calls available per mission. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|EW|FriendlySupport")
	int32 MaxFriendlyEWSupportCalls = 3;

	/** Default duration for friendly EW support (seconds). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|EW|FriendlySupport")
	float FriendlyEWSupportDuration = 60.f;

	/** Default radius for friendly EW support (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|EW|FriendlySupport")
	float FriendlyEWSupportRadius = 10000.f;

	// ------------------------------------------------------------------
	//  Configuration — Drone interaction
	// ------------------------------------------------------------------

	/** Tick interval for applying EW effects to drones in zones. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|EW|Drones")
	float DroneEWTickInterval = 1.f;

	// ------------------------------------------------------------------
	//  Internal
	// ------------------------------------------------------------------
	void TickZoneDurations(float DeltaSeconds);
	void TickEscalation(float DeltaSeconds);
	void TickDroneEWEffects(float DeltaSeconds);
	void TickActorZoneTracking(float DeltaSeconds);

	/** Generate a new enemy EW zone during escalation. */
	void SpawnEscalationZone();

	/** Generate a unique zone ID. */
	FName GenerateZoneID();

	/** Apply EW effects to all drones in affected zones. */
	void ApplyEWToDrones();

	// ------------------------------------------------------------------
	//  Runtime state
	// ------------------------------------------------------------------

	UPROPERTY()
	TArray<FSHEWZone> EWZones;

	int32 EscalationLevel = 0;
	float EscalationTimer = 0.f;
	float GlobalEWStrengthMultiplier = 1.f;
	int32 FriendlyEWSupportCalls;
	int32 ZoneIDCounter = 0;

	float DroneEWTickAccumulator = 0.f;
	float ActorTrackingAccumulator = 0.f;

	/** Track which actors are in which zones for enter/exit events. */
	TMap<TWeakObjectPtr<AActor>, TSet<FName>> ActorZoneMap;

	/** Cached singleton instance. */
	static TWeakObjectPtr<ASHElectronicWarfare> Instance;
};
