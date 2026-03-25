// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SHAIPerceptionConfig.generated.h"

// -----------------------------------------------------------------------
//  Awareness states
// -----------------------------------------------------------------------

/** AI awareness level — drives behavior tree transitions and perception tuning. */
UENUM(BlueprintType)
enum class ESHAwarenessState : uint8
{
	/** No knowledge of any threat. Relaxed patrol / idle. */
	Unaware      UMETA(DisplayName = "Unaware"),

	/** Something caught attention but not confirmed. Investigating. */
	Suspicious   UMETA(DisplayName = "Suspicious"),

	/** Confirmed enemy presence in area but no direct contact. High readiness. */
	Alert        UMETA(DisplayName = "Alert"),

	/** Actively engaged in combat with a known target. */
	Combat       UMETA(DisplayName = "Combat"),

	/** Lost contact — actively searching for previously known target. */
	Searching    UMETA(DisplayName = "Searching")
};

// -----------------------------------------------------------------------
//  Environmental conditions
// -----------------------------------------------------------------------

/** Environmental lighting / weather condition affecting perception. */
UENUM(BlueprintType)
enum class ESHVisibilityCondition : uint8
{
	ClearDay     UMETA(DisplayName = "Clear Day"),
	Overcast     UMETA(DisplayName = "Overcast"),
	Dusk         UMETA(DisplayName = "Dusk/Dawn"),
	Night        UMETA(DisplayName = "Night"),
	NightNVG     UMETA(DisplayName = "Night + NVG"),
	Fog          UMETA(DisplayName = "Fog"),
	HeavyRain    UMETA(DisplayName = "Heavy Rain"),
	Smoke        UMETA(DisplayName = "Smoke")
};

/** Sound type classification for hearing range lookup. */
UENUM(BlueprintType)
enum class ESHSoundType : uint8
{
	Footstep_Walk     UMETA(DisplayName = "Footstep (Walk)"),
	Footstep_Run      UMETA(DisplayName = "Footstep (Run)"),
	Footstep_Sprint   UMETA(DisplayName = "Footstep (Sprint)"),
	Voice_Normal      UMETA(DisplayName = "Voice (Normal)"),
	Voice_Shout       UMETA(DisplayName = "Voice (Shout)"),
	GunfireUnsuppressed  UMETA(DisplayName = "Gunfire (Unsuppressed)"),
	GunfireSuppressed    UMETA(DisplayName = "Gunfire (Suppressed)"),
	Explosion         UMETA(DisplayName = "Explosion"),
	VehicleEngine     UMETA(DisplayName = "Vehicle Engine"),
	EquipmentRattle   UMETA(DisplayName = "Equipment Rattle"),
	DoorBreach        UMETA(DisplayName = "Door Breach"),
	GrenadeImpact     UMETA(DisplayName = "Grenade Impact")
};

// -----------------------------------------------------------------------
//  Sight range entry
// -----------------------------------------------------------------------

/** Sight range for a given visibility condition. */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHSightRangeEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESHVisibilityCondition Condition = ESHVisibilityCondition::ClearDay;

	/** Maximum detection range (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float MaxRange = 8000.f;

	/** Range at which detection is instant (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float InstantDetectionRange = 1500.f;

	/** Peripheral vision half-angle (degrees). Detection is slower outside this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "180"))
	float PeripheralHalfAngleDeg = 70.f;

	/** Central vision half-angle (degrees). Full detection speed inside this. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "90"))
	float CentralHalfAngleDeg = 30.f;
};

// -----------------------------------------------------------------------
//  Hearing range entry
// -----------------------------------------------------------------------

/** Hearing range for a given sound type. */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHHearingRangeEntry
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESHSoundType SoundType = ESHSoundType::GunfireUnsuppressed;

	/** Maximum distance this sound can be heard (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float MaxRange = 10000.f;

	/** Distance at which the sound immediately triggers Suspicious state (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float AlertRange = 3000.f;

	/**
	 * If true, this sound gives the AI a direction to investigate.
	 * (Explosions: yes. Suppressed gunfire: no.)
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bProvidesDirection = true;
};

// -----------------------------------------------------------------------
//  Detection speed curve config
// -----------------------------------------------------------------------

/**
 * Controls how quickly a target accrues "detection" at various ranges.
 * At each tick, detection accrues = DetectionRate * DeltaTime * DistanceMultiplier * PostureMultiplier.
 * When accumulated detection >= 1.0, the target is detected.
 */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHDetectionSpeedConfig
{
	GENERATED_BODY()

	/** Base detection rate (units per second at optimal range and posture). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float BaseDetectionRate = 1.0f;

	/**
	 * Curve mapping normalized distance (0=close, 1=max range) to a
	 * detection speed multiplier.  If null, a linear falloff is used.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UCurveFloat> DistanceCurve = nullptr;

	/** Multiplier when the target is standing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float StandingMultiplier = 1.0f;

	/** Multiplier when the target is crouching. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float CrouchingMultiplier = 0.6f;

	/** Multiplier when the target is prone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float ProneMultiplier = 0.3f;

	/** Multiplier when the target is moving (applied on top of posture). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float MovingMultiplier = 1.5f;

	/** Multiplier when the target is firing a weapon. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float FiringMultiplier = 2.5f;

	/** Detection decay rate when the AI no longer has line of sight (units per second). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float DecayRate = 0.5f;

	/** How long (seconds) AI remembers a fully detected target before starting to lose track. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float MemoryDuration = 10.f;
};

// -----------------------------------------------------------------------
//  Awareness transition config
// -----------------------------------------------------------------------

/** Thresholds and timers for transitioning between awareness states. */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHAwarenessTransitionConfig
{
	GENERATED_BODY()

	/** Detection accumulation needed to go from Unaware -> Suspicious. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "1"))
	float SuspiciousThreshold = 0.3f;

	/** Detection accumulation needed to go from Suspicious -> Alert. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "1"))
	float AlertThreshold = 0.6f;

	/** Detection accumulation needed to go from Alert -> Combat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "1"))
	float CombatThreshold = 1.0f;

	/** Duration (seconds) to stay in Searching before dropping to Alert. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float SearchDuration = 30.f;

	/** Duration (seconds) to stay in Alert before dropping to Suspicious (if nothing found). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float AlertCooldownDuration = 45.f;

	/** Duration (seconds) to stay in Suspicious before dropping to Unaware. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float SuspiciousCooldownDuration = 60.f;

	/** If true, taking damage immediately transitions to Combat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bDamageImmediateCombat = true;

	/** If true, nearby friendly combat state spreads to this AI (squad awareness). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bSquadAwarenessSharing = true;

	/** Range within which squad awareness sharing occurs (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float SquadAwarenessSharingRange = 3000.f;
};

// -----------------------------------------------------------------------
//  Main perception data asset
// -----------------------------------------------------------------------

/**
 * USHAIPerceptionConfig
 *
 * Data asset that defines all perception parameters for enemy AI.
 * Assign different configs for different enemy types (rifleman sees
 * differently than an officer with binoculars, etc.).
 */
UCLASS(BlueprintType)
class SHATTEREDHORIZON2032_API USHAIPerceptionConfig : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	USHAIPerceptionConfig();

	// ------------------------------------------------------------------
	//  Sight
	// ------------------------------------------------------------------

	/** Sight ranges per visibility condition. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perception|Sight")
	TArray<FSHSightRangeEntry> SightRanges;

	/** Get the sight range entry for a specific condition (returns default if not found). */
	UFUNCTION(BlueprintCallable, Category = "Perception|Sight")
	FSHSightRangeEntry GetSightRange(ESHVisibilityCondition Condition) const;

	// ------------------------------------------------------------------
	//  Hearing
	// ------------------------------------------------------------------

	/** Hearing ranges per sound type. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perception|Hearing")
	TArray<FSHHearingRangeEntry> HearingRanges;

	/** Get the hearing range entry for a sound type (returns default if not found). */
	UFUNCTION(BlueprintCallable, Category = "Perception|Hearing")
	FSHHearingRangeEntry GetHearingRange(ESHSoundType SoundType) const;

	/** Multiplier applied to all hearing ranges during heavy rain. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perception|Hearing", meta = (ClampMin = "0", ClampMax = "1"))
	float HeavyRainHearingMultiplier = 0.6f;

	/** Multiplier applied to all hearing ranges during wind. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perception|Hearing", meta = (ClampMin = "0", ClampMax = "1"))
	float WindHearingMultiplier = 0.8f;

	// ------------------------------------------------------------------
	//  Detection speed
	// ------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perception|Detection")
	FSHDetectionSpeedConfig DetectionSpeed;

	// ------------------------------------------------------------------
	//  Awareness transitions
	// ------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Perception|Awareness")
	FSHAwarenessTransitionConfig AwarenessTransitions;
};
