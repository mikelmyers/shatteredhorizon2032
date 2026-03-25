// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShatteredHorizon2032/Weapons/SHBallisticsSystem.h"
#include "SHCallForFireSystem.generated.h"

class USHBallisticsSystem;
class USHSuppressionSystem;
class USoundBase;
class UParticleSystem;

// =====================================================================
//  Enums
// =====================================================================

/** Ordnance types available for indirect fire missions. */
UENUM(BlueprintType)
enum class ESHOrdnanceType : uint8
{
	Mortar60mm		UMETA(DisplayName = "60mm Mortar"),
	Mortar81mm		UMETA(DisplayName = "81mm Mortar"),
	Artillery105mm	UMETA(DisplayName = "105mm Artillery"),
	Artillery155mm	UMETA(DisplayName = "155mm Artillery"),
	SmokeScreen		UMETA(DisplayName = "Smoke Screen"),
	Illumination	UMETA(DisplayName = "Illumination")
};

/** State machine for a fire mission lifecycle. */
UENUM(BlueprintType)
enum class ESHFireMissionState : uint8
{
	Idle		UMETA(DisplayName = "Idle"),
	Requesting	UMETA(DisplayName = "Requesting"),
	Adjusting	UMETA(DisplayName = "Adjusting"),
	Firing		UMETA(DisplayName = "Firing"),
	Complete	UMETA(DisplayName = "Complete"),
	Denied		UMETA(DisplayName = "Denied")
};

// =====================================================================
//  Data structs
// =====================================================================

/** Per-ordnance-type configuration data. */
USTRUCT(BlueprintType)
struct FSHOrdnanceData
{
	GENERATED_BODY()

	/** Base damage at impact center. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ordnance", meta = (ClampMin = "0"))
	float BaseDamage = 200.0f;

	/** Inner blast radius — full damage (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ordnance", meta = (ClampMin = "0"))
	float BlastInnerRadius = 500.0f;

	/** Outer blast radius — damage falloff to zero (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ordnance", meta = (ClampMin = "0"))
	float BlastOuterRadius = 2000.0f;

	/** Fragmentation parameters (reuses FSHFragmentationParams from SHBallisticsSystem). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ordnance")
	FSHFragmentationParams FragParams;

	/** Minimum time of flight in seconds (at short range). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ordnance", meta = (ClampMin = "0"))
	float TimeOfFlightMin = 8.0f;

	/** Maximum time of flight in seconds (at long range). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ordnance", meta = (ClampMin = "0"))
	float TimeOfFlightMax = 12.0f;

	/** Circular error probable — 50% of rounds land within this radius (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ordnance", meta = (ClampMin = "0"))
	float CEP = 1500.0f;

	/** Range at which outgoing/incoming sounds are audible (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ordnance", meta = (ClampMin = "0"))
	float AudibleRange = 50000.0f;

	/** Minimum time between consecutive rounds in a salvo (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ordnance", meta = (ClampMin = "0"))
	float CooldownBetweenRounds = 3.0f;
};

/** Active fire mission descriptor. */
USTRUCT(BlueprintType)
struct FSHFireMission
{
	GENERATED_BODY()

	/** Unique identifier for this mission. */
	UPROPERTY(BlueprintReadOnly, Category = "Fire Mission")
	FGuid MissionId;

	/** Ordnance type requested. */
	UPROPERTY(BlueprintReadOnly, Category = "Fire Mission")
	ESHOrdnanceType OrdnanceType = ESHOrdnanceType::Mortar60mm;

	/** World-space target location. */
	UPROPERTY(BlueprintReadOnly, Category = "Fire Mission")
	FVector TargetLocation = FVector::ZeroVector;

	/** Adjustment offset applied from initial target. */
	UPROPERTY(BlueprintReadOnly, Category = "Fire Mission")
	FVector AdjustOffset = FVector::ZeroVector;

	/** Total rounds originally requested. */
	UPROPERTY(BlueprintReadOnly, Category = "Fire Mission")
	int32 RoundsRequested = 0;

	/** Rounds remaining to fire. */
	UPROPERTY(BlueprintReadOnly, Category = "Fire Mission")
	int32 RoundsRemaining = 0;

	/** Current mission state. */
	UPROPERTY(BlueprintReadOnly, Category = "Fire Mission")
	ESHFireMissionState State = ESHFireMissionState::Idle;

	/** Computed time of flight for current target distance (seconds). */
	UPROPERTY(BlueprintReadOnly, Category = "Fire Mission")
	float TimeOfFlight = 10.0f;

	/** Circular error probable for this mission (cm). */
	UPROPERTY(BlueprintReadOnly, Category = "Fire Mission")
	float CEP = 1500.0f;

	/** Whether the target is within danger-close distance of friendlies. */
	UPROPERTY(BlueprintReadOnly, Category = "Fire Mission")
	bool bIsDangerClose = false;

	/** World time at which this mission was requested. */
	UPROPERTY(BlueprintReadOnly, Category = "Fire Mission")
	float RequestTime = 0.0f;

	/** World time at which the next round is scheduled to fire. */
	float NextRoundTime = 0.0f;
};

// =====================================================================
//  Delegates
// =====================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnFireMissionStarted,
	const FSHFireMission&, Mission);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnRoundImpact,
	const FSHFireMission&, Mission,
	const FVector&, ImpactLocation);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnFireMissionComplete,
	const FSHFireMission&, Mission);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnDangerCloseWarning,
	const FSHFireMission&, Mission);

// =====================================================================
//  USHCallForFireSystem — Actor Component
// =====================================================================

/**
 * USHCallForFireSystem
 *
 * Player-facing actor component that manages the call-for-fire procedure
 * for indirect fire weapons (mortars and artillery).
 *
 * Simulates the full observer-to-fire coordination loop:
 *  1. Player requests a fire mission with ordnance type, target, and rounds.
 *  2. System checks danger-close proximity and pauses for confirmation if needed.
 *  3. Each round is simulated as a high-arc parabolic trajectory with CEP dispersion.
 *  4. On impact: blast damage, fragmentation, suppression, and VFX/SFX.
 *  5. Sound cues for outgoing rounds (battery position) and incoming whistle (target).
 *  6. Battery position is broadcast for enemy counter-battery fire.
 *  7. Ammunition tracking and cooldowns per ordnance type.
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHCallForFireSystem : public UActorComponent
{
	GENERATED_BODY()

public:
	USHCallForFireSystem();

	// -----------------------------------------------------------------
	//  UActorComponent overrides
	// -----------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// -----------------------------------------------------------------
	//  Public API — Fire mission control
	// -----------------------------------------------------------------

	/**
	 * Initiate a call-for-fire request.
	 * @param Type    Ordnance type to employ.
	 * @param Target  World-space target location.
	 * @param Rounds  Number of rounds requested.
	 * @return True if the request was accepted (may still require danger-close confirmation).
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|CallForFire")
	bool RequestFireMission(ESHOrdnanceType Type, FVector Target, int32 Rounds);

	/**
	 * Shift fire from the previous impact point.
	 * @param NewOffset  World-space offset to apply to current target.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|CallForFire")
	void AdjustFire(FVector NewOffset);

	/** Repeat the previous fire mission with identical parameters. */
	UFUNCTION(BlueprintCallable, Category = "SH|CallForFire")
	bool RepeatFire();

	/** Cancel the active fire mission. */
	UFUNCTION(BlueprintCallable, Category = "SH|CallForFire")
	void CancelMission();

	/** Confirm a danger-close fire mission that is awaiting authorization. */
	UFUNCTION(BlueprintCallable, Category = "SH|CallForFire")
	void ConfirmDangerClose();

	// -----------------------------------------------------------------
	//  Queries
	// -----------------------------------------------------------------

	/** Get a copy of the current active fire mission. */
	UFUNCTION(BlueprintPure, Category = "SH|CallForFire")
	FSHFireMission GetActiveMission() const { return ActiveMission; }

	/** Check if a given ordnance type is available (has ammo and is off cooldown). */
	UFUNCTION(BlueprintPure, Category = "SH|CallForFire")
	bool IsFireAvailable(ESHOrdnanceType Type) const;

	/** Get remaining rounds for a given ordnance type. */
	UFUNCTION(BlueprintPure, Category = "SH|CallForFire")
	int32 GetRemainingAmmo(ESHOrdnanceType Type) const;

	// -----------------------------------------------------------------
	//  Delegates
	// -----------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|CallForFire")
	FOnFireMissionStarted OnFireMissionStarted;

	UPROPERTY(BlueprintAssignable, Category = "SH|CallForFire")
	FOnRoundImpact OnRoundImpact;

	UPROPERTY(BlueprintAssignable, Category = "SH|CallForFire")
	FOnFireMissionComplete OnFireMissionComplete;

	UPROPERTY(BlueprintAssignable, Category = "SH|CallForFire")
	FOnDangerCloseWarning OnDangerCloseWarning;

protected:
	// -----------------------------------------------------------------
	//  Configuration — Ordnance data table
	// -----------------------------------------------------------------

	/** Per-ordnance-type configuration. Populated with defaults on BeginPlay if empty. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|CallForFire|Config")
	TMap<ESHOrdnanceType, FSHOrdnanceData> OrdnanceTable;

	/** Available ammunition per ordnance type. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|CallForFire|Config")
	TMap<ESHOrdnanceType, int32> AmmunitionPool;

	/** Cooldown (seconds) between successive fire missions of any type. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|CallForFire|Config", meta = (ClampMin = "0"))
	float MissionCooldown = 15.0f;

	/** Danger-close distance threshold from any friendly (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|CallForFire|Config", meta = (ClampMin = "0"))
	float DangerCloseRadius = 20000.0f; // 200 meters

	/** Simulated battery position offset behind the owning player (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|CallForFire|Config")
	FVector BatteryPositionOffset = FVector(-100000.0f, 0.0f, 0.0f); // 1 km behind

	/** Maximum range for indirect fire from the player's position (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|CallForFire|Config", meta = (ClampMin = "0"))
	float MaxFireRange = 500000.0f; // 5 km

	/** Minimum range to avoid hitting own position (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|CallForFire|Config", meta = (ClampMin = "0"))
	float MinFireRange = 10000.0f; // 100 m

	// -----------------------------------------------------------------
	//  Configuration — Audio
	// -----------------------------------------------------------------

	/** Sound played at the battery position when a round is fired. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|CallForFire|Audio")
	TObjectPtr<USoundBase> OutgoingRoundSound;

	/** Incoming whistle sound played at the target area. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|CallForFire|Audio")
	TObjectPtr<USoundBase> IncomingWhistleSound;

	/** Explosion sound at impact. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|CallForFire|Audio")
	TObjectPtr<USoundBase> ImpactExplosionSound;

	// -----------------------------------------------------------------
	//  Configuration — VFX
	// -----------------------------------------------------------------

	/** Explosion VFX spawned at impact. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|CallForFire|VFX")
	TObjectPtr<UParticleSystem> ImpactExplosionVFX;

	// -----------------------------------------------------------------
	//  Configuration — Damage
	// -----------------------------------------------------------------

	/** Damage type class used for radial blast damage. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|CallForFire|Damage")
	TSubclassOf<UDamageType> IndirectFireDamageType;

private:
	// -----------------------------------------------------------------
	//  Internal state
	// -----------------------------------------------------------------

	/** The currently active fire mission. */
	FSHFireMission ActiveMission;

	/** World time at which the last mission completed (for cooldown). */
	float LastMissionCompleteTime = -100.0f;

	/** Whether we are waiting for danger-close confirmation. */
	bool bAwaitingDangerCloseConfirmation = false;

	/** Cached battery position in world space (updated per mission). */
	FVector CachedBatteryPosition = FVector::ZeroVector;

	/** Timer handles for scheduled incoming-whistle and impact events. */
	TArray<FTimerHandle> PendingRoundTimers;

	/** Cached subsystem pointers. */
	UPROPERTY()
	TObjectPtr<USHBallisticsSystem> CachedBallisticsSystem = nullptr;

	UPROPERTY()
	TObjectPtr<USHSuppressionSystem> CachedSuppressionSystem = nullptr;

	// -----------------------------------------------------------------
	//  Internal methods
	// -----------------------------------------------------------------

	/** Populate default ordnance table if not configured in editor. */
	void InitDefaultOrdnanceTable();

	/** Populate default ammunition pool if not configured. */
	void InitDefaultAmmunitionPool();

	/** Check if any friendly character is within DangerCloseRadius of Position. */
	bool CheckDangerClose(const FVector& Position) const;

	/** Compute battery position in world space based on owner facing. */
	FVector ComputeBatteryPosition() const;

	/** Calculate time of flight based on range and ordnance data. */
	float ComputeTimeOfFlight(const FSHOrdnanceData& Data, float RangeCm) const;

	/** Compute the effective target for a round, applying CEP dispersion. */
	FVector ComputeDispersedImpactPoint(const FVector& AimPoint, float CEPRadius) const;

	/** Begin firing the salvo — schedules rounds one at a time. */
	void BeginFiring();

	/** Fire the next round in the active mission. */
	void FireNextRound();

	/** Schedule the incoming whistle and impact for a single round. */
	void ScheduleRoundImpact(const FVector& ImpactPoint, float TimeOfFlight);

	/** Called when a round's incoming whistle should play. */
	void PlayIncomingWhistle(FVector ImpactPoint);

	/** Called when a round impacts the ground. */
	void ProcessRoundImpact(FVector ImpactPoint);

	/** Apply radial blast damage at an impact location. */
	void ApplyBlastDamage(const FVector& Origin, const FSHOrdnanceData& Data);

	/** Broadcast battery position to enemy AI for counter-battery fire. */
	void BroadcastCounterBatteryExposure(const FVector& BatteryPos);

	/** Complete the active fire mission. */
	void CompleteMission();

	/** Clean up any pending timer handles. */
	void ClearPendingTimers();

	/** Get all friendly pawns in the world for danger-close checks. */
	void GatherFriendlyPositions(TArray<FVector>& OutPositions) const;
};
