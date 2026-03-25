// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Components/SphereComponent.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "SHDroneBase.generated.h"

class USHElectronicWarfare;

// ============================================================================
//  Drone State
// ============================================================================

UENUM(BlueprintType)
enum class ESHDroneState : uint8
{
	Inactive	UMETA(DisplayName = "Inactive"),
	Launching	UMETA(DisplayName = "Launching"),
	Flying		UMETA(DisplayName = "Flying"),
	Loitering	UMETA(DisplayName = "Loitering"),
	Attacking	UMETA(DisplayName = "Attacking"),
	Returning	UMETA(DisplayName = "Returning"),
	Destroyed	UMETA(DisplayName = "Destroyed"),
	Jammed		UMETA(DisplayName = "Jammed")
};

// ============================================================================
//  Drone Affiliation
// ============================================================================

UENUM(BlueprintType)
enum class ESHDroneAffiliation : uint8
{
	Friendly	UMETA(DisplayName = "Friendly"),
	Enemy		UMETA(DisplayName = "Enemy"),
	Neutral		UMETA(DisplayName = "Neutral")
};

// ============================================================================
//  Signal Link Quality
// ============================================================================

UENUM(BlueprintType)
enum class ESHSignalLinkQuality : uint8
{
	Excellent	UMETA(DisplayName = "Excellent"),
	Good		UMETA(DisplayName = "Good"),
	Degraded	UMETA(DisplayName = "Degraded"),
	Critical	UMETA(DisplayName = "Critical"),
	Lost		UMETA(DisplayName = "Lost")
};

// ============================================================================
//  Delegate Declarations
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSHOnDroneStateChanged, ESHDroneState, OldState, ESHDroneState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHOnDroneDestroyed, ASHDroneBase*, DestroyedDrone);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSHOnSignalLinkChanged, ESHSignalLinkQuality, OldQuality, ESHSignalLinkQuality, NewQuality);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHOnBatteryDepleted, ASHDroneBase*, Drone);

// ============================================================================
//  ASHDroneBase — Base class for all drones
// ============================================================================

/**
 * ASHDroneBase
 *
 * APawn-based flying entity serving as the foundation for all drone types
 * in Shattered Horizon 2032. Implements battery life, noise profile,
 * vulnerability, signal link, flight model, and destruction effects.
 */
UCLASS(Abstract, Blueprintable)
class SHATTEREDHORIZON2032_API ASHDroneBase : public APawn
{
	GENERATED_BODY()

public:
	ASHDroneBase();

	// ------------------------------------------------------------------
	//  APawn overrides
	// ------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ------------------------------------------------------------------
	//  State management
	// ------------------------------------------------------------------

	/** Transition the drone to a new state. */
	UFUNCTION(BlueprintCallable, Category = "SH|Drone")
	void SetDroneState(ESHDroneState NewState);

	/** Get current drone state. */
	UFUNCTION(BlueprintPure, Category = "SH|Drone")
	ESHDroneState GetDroneState() const { return CurrentState; }

	/** Launch the drone from a given position. */
	UFUNCTION(BlueprintCallable, Category = "SH|Drone")
	void LaunchDrone(const FVector& LaunchPosition, const FRotator& LaunchRotation);

	/** Command the drone to return to its operator. */
	UFUNCTION(BlueprintCallable, Category = "SH|Drone")
	void ReturnToOperator();

	/** Immediately destroy the drone. */
	UFUNCTION(BlueprintCallable, Category = "SH|Drone")
	void DestroyDrone(AActor* DamageInstigator = nullptr);

	// ------------------------------------------------------------------
	//  Battery / fuel
	// ------------------------------------------------------------------

	/** Get remaining battery as a 0-1 fraction. */
	UFUNCTION(BlueprintPure, Category = "SH|Drone|Battery")
	float GetBatteryFraction() const;

	/** Get remaining loiter time in seconds. */
	UFUNCTION(BlueprintPure, Category = "SH|Drone|Battery")
	float GetRemainingLoiterTime() const;

	/** Check if the drone has sufficient battery to operate. */
	UFUNCTION(BlueprintPure, Category = "SH|Drone|Battery")
	bool HasSufficientBattery() const;

	// ------------------------------------------------------------------
	//  Signal link
	// ------------------------------------------------------------------

	/** Get current signal link quality. */
	UFUNCTION(BlueprintPure, Category = "SH|Drone|Signal")
	ESHSignalLinkQuality GetSignalLinkQuality() const { return SignalLinkQuality; }

	/** Get raw signal strength 0-1. */
	UFUNCTION(BlueprintPure, Category = "SH|Drone|Signal")
	float GetSignalStrength() const { return SignalStrength; }

	/** Apply jamming interference to this drone. */
	UFUNCTION(BlueprintCallable, Category = "SH|Drone|Signal")
	void ApplyJammingInterference(float JammingPower, const FVector& JammerLocation);

	/** Clear all jamming effects. */
	UFUNCTION(BlueprintCallable, Category = "SH|Drone|Signal")
	void ClearJammingInterference();

	// ------------------------------------------------------------------
	//  Noise profile
	// ------------------------------------------------------------------

	/** Get the audible range of this drone in centimeters at current state. */
	UFUNCTION(BlueprintPure, Category = "SH|Drone|Audio")
	float GetAudibleRange() const;

	/** Get the current noise level (0-1). */
	UFUNCTION(BlueprintPure, Category = "SH|Drone|Audio")
	float GetNoiseLevel() const;

	// ------------------------------------------------------------------
	//  Flight
	// ------------------------------------------------------------------

	/** Set the target position for the drone to fly toward. */
	UFUNCTION(BlueprintCallable, Category = "SH|Drone|Flight")
	void SetFlightTarget(const FVector& TargetPosition);

	/** Set the drone's cruise altitude. */
	UFUNCTION(BlueprintCallable, Category = "SH|Drone|Flight")
	void SetCruiseAltitude(float AltitudeCm);

	/** Get the drone's current speed in cm/s. */
	UFUNCTION(BlueprintPure, Category = "SH|Drone|Flight")
	float GetCurrentSpeed() const;

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Drone|Events")
	FSHOnDroneStateChanged OnDroneStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "SH|Drone|Events")
	FSHOnDroneDestroyed OnDroneDestroyed;

	UPROPERTY(BlueprintAssignable, Category = "SH|Drone|Events")
	FSHOnSignalLinkChanged OnSignalLinkChanged;

	UPROPERTY(BlueprintAssignable, Category = "SH|Drone|Events")
	FSHOnBatteryDepleted OnBatteryDepleted;

	// ------------------------------------------------------------------
	//  Properties
	// ------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "SH|Drone|Identity")
	ESHDroneAffiliation Affiliation = ESHDroneAffiliation::Enemy;

	/** Operator actor (AI controller or player). */
	UPROPERTY(BlueprintReadWrite, Category = "SH|Drone|Identity")
	TWeakObjectPtr<AActor> OperatorActor;

	/** Home position the drone returns to when commanded or on low battery. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Drone|Flight")
	FVector HomePosition = FVector::ZeroVector;

protected:
	// ------------------------------------------------------------------
	//  Components
	// ------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|Drone|Components")
	TObjectPtr<USphereComponent> CollisionSphere;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|Drone|Components")
	TObjectPtr<UStaticMeshComponent> BodyMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|Drone|Components")
	TObjectPtr<UStaticMeshComponent> CameraGimbalMesh;

	/** Rotor mesh components (populated by subclasses or blueprint). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|Drone|Components")
	TArray<TObjectPtr<UStaticMeshComponent>> RotorMeshes;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|Drone|Components")
	TObjectPtr<UFloatingPawnMovement> FloatingMovement;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|Drone|Components")
	TObjectPtr<UAudioComponent> MotorAudioComponent;

	/** LED indicator light. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|Drone|Components")
	TObjectPtr<UPointLightComponent> LEDLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|Drone|Components")
	TObjectPtr<UNiagaraComponent> RotorWashVFX;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|Drone|Components")
	TObjectPtr<UNiagaraComponent> TrailVFX;

	// ------------------------------------------------------------------
	//  Configuration — Battery
	// ------------------------------------------------------------------

	/** Maximum battery life in seconds. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Battery")
	float MaxBatterySeconds = 1200.f;

	/** Battery drain multiplier while hovering/loitering. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Battery")
	float LoiterDrainMultiplier = 0.8f;

	/** Battery drain multiplier while flying at speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Battery")
	float FlyingDrainMultiplier = 1.0f;

	/** Battery drain multiplier during attack runs (high throttle). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Battery")
	float AttackDrainMultiplier = 1.5f;

	/** Battery threshold (0-1) at which drone auto-returns. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Battery")
	float LowBatteryThreshold = 0.15f;

	// ------------------------------------------------------------------
	//  Configuration — Flight
	// ------------------------------------------------------------------

	/** Maximum flight speed in cm/s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Flight")
	float MaxFlightSpeed = 2000.f;

	/** Cruise speed in cm/s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Flight")
	float CruiseSpeed = 1200.f;

	/** Acceleration in cm/s^2. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Flight")
	float FlightAcceleration = 800.f;

	/** Default cruise altitude above terrain in cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Flight")
	float DefaultCruiseAltitude = 5000.f;

	/** Launch ascent speed in cm/s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Flight")
	float LaunchAscentSpeed = 500.f;

	/** Height above launch point the drone reaches before transitioning to Flying. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Flight")
	float LaunchTargetHeight = 1500.f;

	/** Turn rate in degrees/second. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Flight")
	float TurnRate = 90.f;

	// ------------------------------------------------------------------
	//  Configuration — Noise
	// ------------------------------------------------------------------

	/** Base audible range in cm when loitering. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Audio")
	float BaseAudibleRange = 5000.f;

	/** Audible range multiplier when flying at speed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Audio")
	float FlyingNoiseMultiplier = 1.5f;

	/** Audible range multiplier during attack dive. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Audio")
	float AttackNoiseMultiplier = 2.5f;

	/** Motor loop sound. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Audio")
	TObjectPtr<USoundBase> MotorLoopSound;

	/** Motor loop sound attenuation. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Audio")
	TObjectPtr<USoundAttenuation> MotorAttenuationSettings;

	// ------------------------------------------------------------------
	//  Configuration — Vulnerability
	// ------------------------------------------------------------------

	/** Maximum health points. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Health")
	float MaxHealth = 100.f;

	/** Damage multiplier from small arms (rifles, pistols). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Health")
	float SmallArmsDamageMultiplier = 1.0f;

	/** Damage multiplier from shotgun pellets (effective counter). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Health")
	float ShotgunDamageMultiplier = 2.0f;

	/** Damage multiplier from explosions/shrapnel. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Health")
	float ExplosiveDamageMultiplier = 3.0f;

	// ------------------------------------------------------------------
	//  Configuration — Signal Link
	// ------------------------------------------------------------------

	/** Maximum operational range from operator in cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Signal")
	float MaxOperationalRange = 200000.f;

	/** Signal strength decay rate per cm of distance (linear falloff). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Signal")
	float SignalDecayPerCm = 0.000005f;

	/** Duration in seconds the drone can fly autonomously after signal loss before crashing. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Signal")
	float AutonomousFlightDuration = 10.f;

	/** Behavior on signal loss. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Signal")
	bool bReturnHomeOnSignalLoss = true;

	// ------------------------------------------------------------------
	//  Configuration — Destruction VFX
	// ------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Destruction")
	TObjectPtr<UNiagaraSystem> DestructionExplosionVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Destruction")
	TObjectPtr<UNiagaraSystem> DebrisFallingVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Destruction")
	TObjectPtr<USoundBase> DestructionSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Destruction")
	TSubclassOf<AActor> DebrisActorClass;

	/** Number of debris pieces spawned on destruction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|Destruction")
	int32 DebrisCount = 4;

	// ------------------------------------------------------------------
	//  Overridable behavior
	// ------------------------------------------------------------------

	/** Called when the drone enters a new state. Override in subclasses. */
	virtual void OnStateEntered(ESHDroneState NewState);

	/** Called when the drone exits a state. Override in subclasses. */
	virtual void OnStateExited(ESHDroneState OldState);

	/** Called each tick to update drone-type-specific behavior. */
	virtual void TickDroneBehavior(float DeltaSeconds);

	/** Called when the drone runs out of battery. */
	virtual void OnBatteryDepleted();

	/** Called when signal link quality changes. */
	virtual void OnSignalLinkQualityChanged(ESHSignalLinkQuality OldQuality, ESHSignalLinkQuality NewQuality);

	// ------------------------------------------------------------------
	//  Internal tick helpers
	// ------------------------------------------------------------------
	void TickBattery(float DeltaSeconds);
	void TickSignalLink(float DeltaSeconds);
	void TickFlight(float DeltaSeconds);
	void TickNoise(float DeltaSeconds);
	void TickLaunchSequence(float DeltaSeconds);
	void TickJammedBehavior(float DeltaSeconds);

	/** Compute signal quality enum from raw signal strength. */
	ESHSignalLinkQuality ComputeSignalQuality(float Strength) const;

	/** Update motor audio based on state and speed. */
	void UpdateMotorAudio();

	/** Spawn destruction effects. */
	void SpawnDestructionEffects();

	/** Spawn debris actors. */
	void SpawnDebris();

	// ------------------------------------------------------------------
	//  Runtime state
	// ------------------------------------------------------------------

	UPROPERTY(ReplicatedUsing = OnRep_DroneState)
	ESHDroneState CurrentState = ESHDroneState::Inactive;

	UFUNCTION()
	void OnRep_DroneState();

	UPROPERTY(Replicated)
	float CurrentHealth;

	UPROPERTY(Replicated)
	float CurrentBatterySeconds;

	float SignalStrength = 1.f;
	ESHSignalLinkQuality SignalLinkQuality = ESHSignalLinkQuality::Excellent;

	FVector FlightTargetPosition = FVector::ZeroVector;
	float CurrentCruiseAltitude;
	FVector LaunchOrigin = FVector::ZeroVector;
	float LaunchElapsedTime = 0.f;

	/** Accumulated jamming power affecting this drone. */
	float AccumulatedJammingPower = 0.f;
	FVector LastJammerLocation = FVector::ZeroVector;

	/** Timer for autonomous flight after signal loss. */
	float AutonomousFlightTimer = 0.f;
	bool bSignalLost = false;

	/** Current noise level 0-1. */
	float CurrentNoiseLevel = 0.f;

	/** Cached rotor spin speed for visual/audio feedback. */
	float RotorSpinSpeed = 0.f;
};
