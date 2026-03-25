// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SHDroneBase.h"
#include "SHFPVStrikeDrone.generated.h"

// ============================================================================
//  Strike Drone Attack Phase
// ============================================================================

UENUM(BlueprintType)
enum class ESHStrikePhase : uint8
{
	Transit			UMETA(DisplayName = "Transit"),
	Seeking			UMETA(DisplayName = "Seeking"),
	TerminalDive	UMETA(DisplayName = "Terminal Dive"),
	Detonated		UMETA(DisplayName = "Detonated")
};

// ============================================================================
//  Strike Target Type
// ============================================================================

UENUM(BlueprintType)
enum class ESHStrikeTargetType : uint8
{
	Personnel	UMETA(DisplayName = "Personnel"),
	Vehicle		UMETA(DisplayName = "Vehicle"),
	Position	UMETA(DisplayName = "Static Position")
};

// ============================================================================
//  Delegate Declarations
// ============================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSHOnStrikeDetonation, ASHFPVStrikeDrone*, Drone, const FVector&, ImpactLocation);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSHOnStrikePhaseChanged, ESHStrikePhase, OldPhase, ESHStrikePhase, NewPhase);

// ============================================================================
//  ASHFPVStrikeDrone — First-Person View kamikaze attack drone
// ============================================================================

/**
 * ASHFPVStrikeDrone
 *
 * Small, fast, single-use kamikaze drone carrying an explosive payload.
 * Enemy AI-operated with a high-speed terminal approach. Distinctive
 * high-pitched whine. Supports swarm mode where multiple drones attack
 * simultaneously. Can target personnel, vehicles, and static positions.
 */
UCLASS(Blueprintable)
class SHATTEREDHORIZON2032_API ASHFPVStrikeDrone : public ASHDroneBase
{
	GENERATED_BODY()

public:
	ASHFPVStrikeDrone();

	virtual void BeginPlay() override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ------------------------------------------------------------------
	//  Strike interface
	// ------------------------------------------------------------------

	/** Assign a target actor for the strike. */
	UFUNCTION(BlueprintCallable, Category = "SH|Drone|FPV")
	void AssignTarget(AActor* TargetActor, ESHStrikeTargetType TargetType);

	/** Assign a target position for the strike (static position attack). */
	UFUNCTION(BlueprintCallable, Category = "SH|Drone|FPV")
	void AssignTargetPosition(const FVector& Position);

	/** Begin the terminal dive / attack run. */
	UFUNCTION(BlueprintCallable, Category = "SH|Drone|FPV")
	void BeginTerminalAttack();

	/** Detonate the warhead (called on impact or manually). */
	UFUNCTION(BlueprintCallable, Category = "SH|Drone|FPV")
	void Detonate();

	/** Get current strike phase. */
	UFUNCTION(BlueprintPure, Category = "SH|Drone|FPV")
	ESHStrikePhase GetStrikePhase() const { return CurrentStrikePhase; }

	/** Check if this drone is part of a swarm. */
	UFUNCTION(BlueprintPure, Category = "SH|Drone|FPV")
	bool IsSwarmMember() const { return bIsSwarmMember; }

	// ------------------------------------------------------------------
	//  Swarm interface
	// ------------------------------------------------------------------

	/** Register this drone as part of a swarm. */
	UFUNCTION(BlueprintCallable, Category = "SH|Drone|FPV|Swarm")
	void JoinSwarm(int32 InSwarmID, int32 InSwarmIndex, int32 InSwarmSize);

	/** Get swarm ID (0 if not in swarm). */
	UFUNCTION(BlueprintPure, Category = "SH|Drone|FPV|Swarm")
	int32 GetSwarmID() const { return SwarmID; }

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Drone|FPV|Events")
	FSHOnStrikeDetonation OnStrikeDetonation;

	UPROPERTY(BlueprintAssignable, Category = "SH|Drone|FPV|Events")
	FSHOnStrikePhaseChanged OnStrikePhaseChanged;

protected:
	// ------------------------------------------------------------------
	//  ASHDroneBase overrides
	// ------------------------------------------------------------------
	virtual void TickDroneBehavior(float DeltaSeconds) override;
	virtual void OnStateEntered(ESHDroneState NewState) override;

	// ------------------------------------------------------------------
	//  Attack tick helpers
	// ------------------------------------------------------------------
	void TickTransit(float DeltaSeconds);
	void TickSeeking(float DeltaSeconds);
	void TickTerminalDive(float DeltaSeconds);

	/** Compute the lead-intercept position for a moving target. */
	FVector ComputeInterceptPosition() const;

	/** Apply evasive maneuvers (slight weaving to make shooting harder). */
	FVector ComputeEvasiveOffset(float DeltaSeconds) const;

	/** Check for impact with ground or target. */
	bool CheckImpact() const;

	/** Apply explosive damage at detonation point. */
	void ApplyExplosiveDamage(const FVector& DetonationPoint);

	/** Apply fragmentation damage (shrapnel). */
	void ApplyFragmentationDamage(const FVector& DetonationPoint);

	/** Set the strike phase with notification. */
	void SetStrikePhase(ESHStrikePhase NewPhase);

	/** Compute swarm offset for formation flying. */
	FVector ComputeSwarmOffset() const;

	// ------------------------------------------------------------------
	//  Configuration — Attack
	// ------------------------------------------------------------------

	/** Maximum dive speed in cm/s. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|FPV|Attack")
	float MaxDiveSpeed = 5000.f;

	/** Dive acceleration in cm/s^2. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|FPV|Attack")
	float DiveAcceleration = 3000.f;

	/** Distance at which the drone begins terminal dive (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|FPV|Attack")
	float TerminalDiveRange = 8000.f;

	/** Distance at which the drone begins seeking phase from transit (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|FPV|Attack")
	float SeekingRange = 15000.f;

	/** Dive angle in degrees (0 = horizontal, 90 = vertical). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|FPV|Attack")
	float TerminalDiveAngle = 45.f;

	// ------------------------------------------------------------------
	//  Configuration — Warhead
	// ------------------------------------------------------------------

	/** Direct impact damage. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|FPV|Warhead")
	float DirectDamage = 150.f;

	/** Explosion inner radius (full damage) in cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|FPV|Warhead")
	float ExplosionInnerRadius = 200.f;

	/** Explosion outer radius (falloff) in cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|FPV|Warhead")
	float ExplosionOuterRadius = 600.f;

	/** Explosion damage (for radial damage, separate from direct). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|FPV|Warhead")
	float ExplosionDamage = 100.f;

	/** Number of fragmentation shrapnel traces. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|FPV|Warhead")
	int32 FragmentCount = 32;

	/** Maximum range of fragmentation in cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|FPV|Warhead")
	float FragmentRange = 1500.f;

	/** Damage per fragment. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|FPV|Warhead")
	float FragmentDamage = 25.f;

	/** Anti-vehicle damage multiplier (warheads are shaped-charge effective). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|FPV|Warhead")
	float AntiVehicleDamageMultiplier = 2.5f;

	/** Damage type class for the explosion. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|FPV|Warhead")
	TSubclassOf<UDamageType> ExplosionDamageType;

	// ------------------------------------------------------------------
	//  Configuration — VFX / Audio
	// ------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|FPV|VFX")
	TObjectPtr<UNiagaraSystem> ExplosionVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|FPV|VFX")
	TObjectPtr<UNiagaraSystem> FragmentationVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|FPV|Audio")
	TObjectPtr<USoundBase> DiveWhineSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|FPV|Audio")
	TObjectPtr<USoundBase> ExplosionSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|FPV|Audio")
	TObjectPtr<USoundAttenuation> DiveWhineAttenuation;

	// ------------------------------------------------------------------
	//  Configuration — Evasion
	// ------------------------------------------------------------------

	/** Amplitude of evasive weaving in cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|FPV|Evasion")
	float EvasiveAmplitude = 150.f;

	/** Frequency of evasive weaving in Hz. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|FPV|Evasion")
	float EvasiveFrequency = 3.f;

	/** Whether to use evasive maneuvers during approach. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|FPV|Evasion")
	bool bUseEvasiveManeuvers = true;

	// ------------------------------------------------------------------
	//  Configuration — Swarm
	// ------------------------------------------------------------------

	/** Spacing between swarm members in cm. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|FPV|Swarm")
	float SwarmSpacing = 500.f;

	/** Time offset between swarm member attacks (staggered arrival). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Drone|FPV|Swarm")
	float SwarmStaggerSeconds = 0.8f;

	// ------------------------------------------------------------------
	//  Runtime state
	// ------------------------------------------------------------------

	UPROPERTY(Replicated)
	ESHStrikePhase CurrentStrikePhase = ESHStrikePhase::Transit;

	/** The actor being targeted. */
	UPROPERTY()
	TWeakObjectPtr<AActor> StrikeTarget;

	/** Target type. */
	ESHStrikeTargetType StrikeTargetType = ESHStrikeTargetType::Personnel;

	/** Static target position (when targeting a position). */
	FVector StaticTargetPosition = FVector::ZeroVector;

	/** Whether a specific target has been assigned. */
	bool bHasTarget = false;

	/** Timer for evasive maneuver phase offset. */
	float EvasivePhaseOffset = 0.f;

	/** Dive audio component (separate from motor). */
	UPROPERTY()
	TObjectPtr<UAudioComponent> DiveAudioComponent;

	// Swarm state
	bool bIsSwarmMember = false;
	int32 SwarmID = 0;
	int32 SwarmIndex = 0;
	int32 SwarmSize = 1;
	float SwarmAttackDelay = 0.f;

	/** Whether the warhead has detonated. */
	bool bHasDetonated = false;
};
