// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/SphereComponent.h"
#include "SHDroneBase.h"
#include "SHCounterDroneSystem.generated.h"

class UAudioComponent;
class UNiagaraComponent;
class UNiagaraSystem;

// ============================================================================
//  Counter-Drone Mode
// ============================================================================

UENUM(BlueprintType)
enum class ESHCounterDroneMode : uint8
{
	Inactive		UMETA(DisplayName = "Inactive"),
	Scanning		UMETA(DisplayName = "Scanning"),
	Jamming			UMETA(DisplayName = "Active Jamming"),
	Cooldown		UMETA(DisplayName = "Cooldown")
};

// ============================================================================
//  Jammer Effect Type — what happens to jammed drones
// ============================================================================

UENUM(BlueprintType)
enum class ESHJammerEffect : uint8
{
	ForceLand		UMETA(DisplayName = "Force Land"),
	ForceReturn		UMETA(DisplayName = "Force Return to Home"),
	LoseControl		UMETA(DisplayName = "Lose Control / Crash"),
	Disorient		UMETA(DisplayName = "Disorient / Drift")
};

// ============================================================================
//  Drone Detection Entry
// ============================================================================

USTRUCT(BlueprintType)
struct FSHDroneDetectionEntry
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SH|CounterDrone")
	TWeakObjectPtr<ASHDroneBase> Drone;

	/** Distance to drone in cm. */
	UPROPERTY(BlueprintReadOnly, Category = "SH|CounterDrone")
	float Distance = 0.f;

	/** Direction from counter-drone system to the drone. */
	UPROPERTY(BlueprintReadOnly, Category = "SH|CounterDrone")
	FVector Direction = FVector::ZeroVector;

	/** Bearing in degrees (0 = North, clockwise). */
	UPROPERTY(BlueprintReadOnly, Category = "SH|CounterDrone")
	float Bearing = 0.f;

	/** Detection confidence 0-1. */
	UPROPERTY(BlueprintReadOnly, Category = "SH|CounterDrone")
	float Confidence = 0.f;

	/** Whether this drone is currently being jammed by this system. */
	UPROPERTY(BlueprintReadOnly, Category = "SH|CounterDrone")
	bool bBeingJammed = false;

	/** Time since first detected (seconds). */
	UPROPERTY(BlueprintReadOnly, Category = "SH|CounterDrone")
	float TimeTracked = 0.f;
};

// ============================================================================
//  Delegate Declarations
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSHOnDroneDetected, ASHDroneBase*, Drone, float, Distance);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHOnDroneLostTracking, ASHDroneBase*, Drone);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSHOnDroneJammed, ASHDroneBase*, Drone, ESHJammerEffect, Effect);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHOnJammerCooldownComplete, ASHCounterDroneSystem*, System);

// ============================================================================
//  ASHCounterDroneSystem
// ============================================================================

/**
 * ASHCounterDroneSystem
 *
 * Counter-drone system that provides both passive detection and active
 * EW jamming against enemy drones. Usable by both player and AI.
 * Detects drones via audio signature and radar, provides HUD indicators,
 * and can jam enemy drones to force them to land, return home, or crash.
 */
UCLASS(Blueprintable)
class SHATTEREDHORIZON2032_API ASHCounterDroneSystem : public AActor
{
	GENERATED_BODY()

public:
	ASHCounterDroneSystem();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ------------------------------------------------------------------
	//  Activation
	// ------------------------------------------------------------------

	/** Activate the counter-drone system (begin scanning). */
	UFUNCTION(BlueprintCallable, Category = "SH|CounterDrone")
	void Activate();

	/** Deactivate the counter-drone system. */
	UFUNCTION(BlueprintCallable, Category = "SH|CounterDrone")
	void Deactivate();

	/** Begin active jamming (targets all drones in range). */
	UFUNCTION(BlueprintCallable, Category = "SH|CounterDrone")
	bool BeginJamming();

	/** Stop active jamming. */
	UFUNCTION(BlueprintCallable, Category = "SH|CounterDrone")
	void StopJamming();

	/** Get current operating mode. */
	UFUNCTION(BlueprintPure, Category = "SH|CounterDrone")
	ESHCounterDroneMode GetCurrentMode() const { return CurrentMode; }

	/** Check if the system is ready to jam (not on cooldown, has charges). */
	UFUNCTION(BlueprintPure, Category = "SH|CounterDrone")
	bool CanJam() const;

	// ------------------------------------------------------------------
	//  Detection queries
	// ------------------------------------------------------------------

	/** Get all currently detected drones. */
	UFUNCTION(BlueprintPure, Category = "SH|CounterDrone")
	const TArray<FSHDroneDetectionEntry>& GetDetectedDrones() const { return DetectedDrones; }

	/** Get the number of detected drones. */
	UFUNCTION(BlueprintPure, Category = "SH|CounterDrone")
	int32 GetDetectedDroneCount() const { return DetectedDrones.Num(); }

	/** Get the closest detected drone. */
	UFUNCTION(BlueprintPure, Category = "SH|CounterDrone")
	bool GetClosestDrone(FSHDroneDetectionEntry& OutEntry) const;

	/** Check if any drones are detected within a given range. */
	UFUNCTION(BlueprintPure, Category = "SH|CounterDrone")
	bool HasDronesInRange(float RangeCm) const;

	// ------------------------------------------------------------------
	//  Charges / resource management
	// ------------------------------------------------------------------

	/** Get remaining jamming charges. */
	UFUNCTION(BlueprintPure, Category = "SH|CounterDrone")
	int32 GetRemainingCharges() const { return RemainingCharges; }

	/** Get remaining cooldown time in seconds. */
	UFUNCTION(BlueprintPure, Category = "SH|CounterDrone")
	float GetCooldownRemaining() const { return CooldownTimer; }

	/** Get cooldown fraction (0 = ready, 1 = full cooldown). */
	UFUNCTION(BlueprintPure, Category = "SH|CounterDrone")
	float GetCooldownFraction() const;

	/** Get current power drain fraction 0-1 while jamming. */
	UFUNCTION(BlueprintPure, Category = "SH|CounterDrone")
	float GetPowerLevel() const;

	// ------------------------------------------------------------------
	//  HUD / feedback
	// ------------------------------------------------------------------

	/** Get the direction to the nearest drone for HUD indicator. */
	UFUNCTION(BlueprintPure, Category = "SH|CounterDrone")
	bool GetNearestDroneIndicator(FVector& OutDirection, float& OutDistance) const;

	/** Check if audio detection cue should play. */
	UFUNCTION(BlueprintPure, Category = "SH|CounterDrone")
	bool ShouldPlayDroneAudioCue() const;

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|CounterDrone|Events")
	FSHOnDroneDetected OnDroneDetectedDelegate;

	UPROPERTY(BlueprintAssignable, Category = "SH|CounterDrone|Events")
	FSHOnDroneLostTracking OnDroneLostTracking;

	UPROPERTY(BlueprintAssignable, Category = "SH|CounterDrone|Events")
	FSHOnDroneJammed OnDroneJammed;

	UPROPERTY(BlueprintAssignable, Category = "SH|CounterDrone|Events")
	FSHOnJammerCooldownComplete OnJammerCooldownComplete;

protected:
	// ------------------------------------------------------------------
	//  Components
	// ------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|CounterDrone|Components")
	TObjectPtr<USceneComponent> RootScene;

	/** Detection sphere for passive scanning. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|CounterDrone|Components")
	TObjectPtr<USphereComponent> DetectionSphere;

	/** Jamming effect sphere (active jamming range). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|CounterDrone|Components")
	TObjectPtr<USphereComponent> JammingSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|CounterDrone|Components")
	TObjectPtr<UAudioComponent> ScanBeepAudio;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|CounterDrone|Components")
	TObjectPtr<UAudioComponent> JammingNoiseAudio;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|CounterDrone|Components")
	TObjectPtr<UNiagaraComponent> JammingFieldVFX;

	// ------------------------------------------------------------------
	//  Configuration — Detection
	// ------------------------------------------------------------------

	/** Passive detection range in cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|CounterDrone|Detection")
	float DetectionRange = 30000.f;

	/** Audio detection range (can hear drones at this distance). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|CounterDrone|Detection")
	float AudioDetectionRange = 15000.f;

	/** Scan update interval in seconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|CounterDrone|Detection")
	float ScanInterval = 0.25f;

	/** Minimum drone noise level to detect (0-1). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|CounterDrone|Detection")
	float MinDetectableNoise = 0.15f;

	// ------------------------------------------------------------------
	//  Configuration — Jamming
	// ------------------------------------------------------------------

	/** Active jamming effective range in cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|CounterDrone|Jamming")
	float JammingRange = 15000.f;

	/** Jamming power output (0-1). Higher = more effective against drones. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|CounterDrone|Jamming")
	float JammingPower = 0.9f;

	/** Maximum jamming duration per charge (seconds). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|CounterDrone|Jamming")
	float MaxJammingDuration = 30.f;

	/** Cooldown time between jamming uses (seconds). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|CounterDrone|Jamming")
	float JammingCooldown = 45.f;

	/** Total number of jamming charges available. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|CounterDrone|Jamming")
	int32 MaxCharges = 3;

	/** Effect applied to jammed drones. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|CounterDrone|Jamming")
	ESHJammerEffect JammerEffect = ESHJammerEffect::LoseControl;

	/** Power drain per second while jamming (0-1 per second). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|CounterDrone|Jamming")
	float PowerDrainRate = 0.033f;

	// ------------------------------------------------------------------
	//  Configuration — Audio
	// ------------------------------------------------------------------

	/** Beep sound when a drone is detected. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|CounterDrone|Audio")
	TObjectPtr<USoundBase> DroneDetectedBeep;

	/** Warning sound when drone is within close range. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|CounterDrone|Audio")
	TObjectPtr<USoundBase> DroneCloseWarning;

	/** Jamming active sound (continuous). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|CounterDrone|Audio")
	TObjectPtr<USoundBase> JammingActiveSound;

	/** Close warning threshold in cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|CounterDrone|Audio")
	float CloseWarningRange = 5000.f;

	// ------------------------------------------------------------------
	//  Internal tick helpers
	// ------------------------------------------------------------------
	void TickScanning(float DeltaSeconds);
	void TickJamming(float DeltaSeconds);
	void TickCooldown(float DeltaSeconds);
	void TickAudioFeedback(float DeltaSeconds);

	/** Scan for drones in detection range. */
	void PerformDroneScan();

	/** Apply jamming to all drones in jamming range. */
	void ApplyJammingToTargets();

	/** Apply the configured jammer effect to a specific drone. */
	void ApplyJammerEffectToDrone(ASHDroneBase* Drone);

	/** Set operating mode. */
	void SetMode(ESHCounterDroneMode NewMode);

	/** Compute bearing from system to a world position. */
	float ComputeBearing(const FVector& TargetPos) const;

	// ------------------------------------------------------------------
	//  Runtime state
	// ------------------------------------------------------------------

	ESHCounterDroneMode CurrentMode = ESHCounterDroneMode::Inactive;
	TArray<FSHDroneDetectionEntry> DetectedDrones;

	float ScanAccumulator = 0.f;
	float JammingTimer = 0.f;
	float CooldownTimer = 0.f;
	int32 RemainingCharges;
	float CurrentPower = 1.f; // 0-1 power level

	/** Audio feedback state. */
	float AudioCueAccumulator = 0.f;
	bool bCloseWarningPlaying = false;
};
