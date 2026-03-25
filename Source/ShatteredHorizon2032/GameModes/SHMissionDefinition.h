// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ShatteredHorizon2032/Core/SHGameMode.h"
#include "ShatteredHorizon2032/GameModes/SHObjectiveSystem.h"
#include "ShatteredHorizon2032/World/SHWeatherSystem.h"
#include "ShatteredHorizon2032/AI/SHEnemyCharacter.h"
#include "SHMissionDefinition.generated.h"

class USoundBase;
class UTexture2D;

// -----------------------------------------------------------------------
//  Enums
// -----------------------------------------------------------------------

/** Condition that causes the mission to advance to the next phase. */
UENUM(BlueprintType)
enum class ESHPhaseAdvanceCondition : uint8
{
	AllObjectivesComplete  UMETA(DisplayName = "All Objectives Complete"),
	TimerExpired           UMETA(DisplayName = "Timer Expired"),
	ManualTrigger          UMETA(DisplayName = "Manual Trigger"),
	EnemyBreachesLine      UMETA(DisplayName = "Enemy Breaches Line"),
	FriendlyForcesOverrun  UMETA(DisplayName = "Friendly Forces Overrun")
};

/** Trigger type for scripted events within a phase. */
UENUM(BlueprintType)
enum class ESHScriptedEventTriggerType : uint8
{
	PhaseStart          UMETA(DisplayName = "Phase Start"),
	Delay               UMETA(DisplayName = "Delay"),
	ObjectiveComplete   UMETA(DisplayName = "Objective Complete"),
	ObjectiveFailed     UMETA(DisplayName = "Objective Failed"),
	EnemyCountBelow     UMETA(DisplayName = "Enemy Count Below"),
	PlayerEntersArea    UMETA(DisplayName = "Player Enters Area"),
	Custom              UMETA(DisplayName = "Custom")
};

/** Type of scripted event that fires during a mission phase. */
UENUM(BlueprintType)
enum class ESHScriptedEventType : uint8
{
	ArtilleryBarrage      UMETA(DisplayName = "Artillery Barrage"),
	DroneSwarm            UMETA(DisplayName = "Drone Swarm"),
	EWJammingZone         UMETA(DisplayName = "EW Jamming Zone"),
	AirStrike             UMETA(DisplayName = "Air Strike"),
	Reinforcement         UMETA(DisplayName = "Reinforcement"),
	Dialogue              UMETA(DisplayName = "Dialogue"),
	WeatherChange         UMETA(DisplayName = "Weather Change"),
	Explosion             UMETA(DisplayName = "Explosion"),
	SupplyDrop            UMETA(DisplayName = "Supply Drop"),
	CivilianEvacuation    UMETA(DisplayName = "Civilian Evacuation"),
	CounterBattery        UMETA(DisplayName = "Counter Battery")
};

/** Trigger type for dialogue lines during a phase. */
UENUM(BlueprintType)
enum class ESHDialogueTriggerType : uint8
{
	PhaseStart           UMETA(DisplayName = "Phase Start"),
	Delay                UMETA(DisplayName = "Delay"),
	ObjectiveComplete    UMETA(DisplayName = "Objective Complete"),
	PlayerAction         UMETA(DisplayName = "Player Action"),
	Proximity            UMETA(DisplayName = "Proximity"),
	CasualtyThreshold    UMETA(DisplayName = "Casualty Threshold")
};

/** Squad member combat role (player's team). */
UENUM(BlueprintType)
enum class ESHSquadMemberRole : uint8
{
	Rifleman          UMETA(DisplayName = "Rifleman"),
	Grenadier         UMETA(DisplayName = "Grenadier"),
	AutomaticRifleman UMETA(DisplayName = "Automatic Rifleman"),
	Marksman          UMETA(DisplayName = "Marksman")
};

/** Personality trait that influences squad member dialogue and behavior. */
UENUM(BlueprintType)
enum class ESHPersonalityTrait : uint8
{
	Calm        UMETA(DisplayName = "Calm"),
	Aggressive  UMETA(DisplayName = "Aggressive"),
	Cautious    UMETA(DisplayName = "Cautious"),
	Joker       UMETA(DisplayName = "Joker"),
	Veteran     UMETA(DisplayName = "Veteran")
};

// -----------------------------------------------------------------------
//  Structs — building blocks (leaf types first)
// -----------------------------------------------------------------------

/** Definition of a single squad member on the player's team. */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHSquadMemberDef
{
	GENERATED_BODY()

	/** Soldier's name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SquadMember")
	FText Name;

	/** Radio callsign (e.g. "Hammer 2-1"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SquadMember")
	FText Callsign;

	/** Military rank. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SquadMember")
	FText Rank;

	/** Combat role. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SquadMember")
	ESHSquadMemberRole Role = ESHSquadMemberRole::Rifleman;

	/** Personality trait that colors dialogue and reactions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SquadMember")
	ESHPersonalityTrait PersonalityTrait = ESHPersonalityTrait::Calm;

	/** Short backstory snippet shown in the briefing screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SquadMember", meta = (MultiLine = true))
	FText BackstorySnippet;
};

/** Composition of an enemy squad — how many soldiers of each role. */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHSquadTemplate
{
	GENERATED_BODY()

	/** Total soldiers in the squad (typically 4-8). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad", meta = (ClampMin = "4", ClampMax = "8"))
	int32 SquadSize = 4;

	/** Role distribution: how many soldiers fill each combat role. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad")
	TMap<ESHEnemyRole, int32> Roles;

	/** Whether this squad has an officer leading it. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad")
	bool bHasOfficer = false;

	/** Starting morale level (0 = demoralized, 1 = fresh). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MoraleLevel = 1.0f;
};

/** Definition of a single reinforcement wave within a phase. */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHWaveDefinition
{
	GENERATED_BODY()

	/** Display name for debug and editor use (e.g. "First Landing Wave"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	FText WaveName;

	/** Seconds after phase start before this wave spawns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0.0"))
	float DelayFromPhaseStart = 0.f;

	/** Spawn zone tag consumed by the spawning subsystem. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	FName SpawnZoneTag;

	/** Total infantry units in the wave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0"))
	int32 TotalInfantry = 0;

	/** Squad templates that compose this wave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	TArray<FSHSquadTemplate> SquadComposition;

	/** Whether the wave includes armored or transport vehicles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	bool bIncludesVehicles = false;

	/** Number of vehicles in the wave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave", meta = (ClampMin = "0", EditCondition = "bIncludesVehicles"))
	int32 VehicleCount = 0;

	/** Whether the wave has air support elements. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	bool bIncludesAirSupport = false;

	/** World-space direction the wave approaches from (normalized). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave")
	FVector ApproachDirection = FVector::ForwardVector;
};

/** A phase objective — a specific goal active during a mission phase. */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHPhaseObjective
{
	GENERATED_BODY()

	/** Unique identifier for cross-referencing with scripted events. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	FGuid ObjectiveId;

	/** Short display name shown in the HUD. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	FText DisplayName;

	/** Longer description for the mission log. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective", meta = (MultiLine = true))
	FText Description;

	/** Objective type (Capture, Destroy, Defend, etc.). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	ESHObjectiveType Type = ESHObjectiveType::Capture;

	/** Priority tier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	ESHObjectivePriority Priority = ESHObjectivePriority::Primary;

	/** World location of the objective marker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective", meta = (MakeEditWidget = true))
	FVector WorldLocation = FVector::ZeroVector;

	/** Radius in centimeters for area-based objectives. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective", meta = (ClampMin = "0.0"))
	float AreaRadius = 2000.f;

	/** Duration in seconds the area must be held for Defend objectives. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective", meta = (ClampMin = "0.0"))
	float DefendDuration = 0.f;

	/** Time limit in seconds (0 = no limit). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective", meta = (ClampMin = "0.0"))
	float TimeLimit = 0.f;

	/** If true, failing this objective means mission failure. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	bool bIsCritical = false;
};

/** A scripted event that fires at a specific moment within a phase. */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHScriptedEvent
{
	GENERATED_BODY()

	/** Unique identifier for this event. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScriptedEvent")
	FName EventId;

	/** What triggers this event. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScriptedEvent")
	ESHScriptedEventTriggerType TriggerType = ESHScriptedEventTriggerType::PhaseStart;

	/** Additional delay after the trigger condition is met (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScriptedEvent", meta = (ClampMin = "0.0"))
	float TriggerDelay = 0.f;

	/** For EnemyCountBelow: fraction of enemies remaining (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScriptedEvent", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float TriggerThreshold = 0.f;

	/** For objective-based triggers: which objective must complete/fail. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScriptedEvent")
	FGuid TriggerObjectiveId;

	/** For PlayerEntersArea: name of the trigger volume in the level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScriptedEvent")
	FName TriggerVolumeName;

	/** What kind of event fires. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScriptedEvent")
	ESHScriptedEventType EventType = ESHScriptedEventType::ArtilleryBarrage;

	/** World-space center of the event effect. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScriptedEvent", meta = (MakeEditWidget = true))
	FVector EventLocation = FVector::ZeroVector;

	/** Area of effect radius in centimeters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScriptedEvent", meta = (ClampMin = "0.0"))
	float EventRadius = 0.f;

	/** Duration of the event in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScriptedEvent", meta = (ClampMin = "0.0"))
	float EventDuration = 0.f;

	/** Intensity scalar (0 = minimal, 1 = maximum). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScriptedEvent", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EventIntensity = 0.5f;

	/** Human-readable description for the editor and debug views. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ScriptedEvent", meta = (MultiLine = true))
	FText EventDescription;
};

/** A dialogue line that plays at a specific moment in a phase. */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHDialogueTrigger
{
	GENERATED_BODY()

	/** What triggers this dialogue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	ESHDialogueTriggerType TriggerType = ESHDialogueTriggerType::PhaseStart;

	/** Delay in seconds after the trigger condition is met. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (ClampMin = "0.0"))
	float Delay = 0.f;

	/** Speaker callsign or name (e.g. "CPT Torres", "Overlord"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	FText Speaker;

	/** The dialogue text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue", meta = (MultiLine = true))
	FText DialogueText;

	/** Audio cue for the voice line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	TSoftObjectPtr<USoundBase> AudioCue;

	/** Whether this is a radio transmission (applies crackle filter). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	bool bIsRadioComm = true;

	/** Playback priority (higher value = higher priority). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Dialogue")
	int32 Priority = 0;
};

/** Mission briefing data presented to the player before the mission starts. */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHMissionBriefing
{
	GENERATED_BODY()

	/** Mission display name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Briefing")
	FText MissionName;

	/** Operational callsign (e.g. "Operation Breakwater"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Briefing")
	FText MissionCallsign;

	/** Full briefing narration text read to the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Briefing", meta = (MultiLine = true))
	FText BriefingNarration;

	/** Voice-over audio for the briefing narration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Briefing")
	TSoftObjectPtr<USoundBase> NarrationAudio;

	/** Bullet-point objective summaries shown in the briefing screen. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Briefing")
	TArray<FText> ObjectiveSummaries;

	/** Intelligence briefing notes. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Briefing")
	TArray<FText> IntelNotes;

	/** Map overlay texture displayed during the briefing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Briefing")
	TSoftObjectPtr<UTexture2D> MapOverlayTexture;

	/** Context string: time, date, and location (e.g. "0530 Local, 14 June 2032, Taoyuan Coast"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Briefing", meta = (MultiLine = true))
	FText SettingDescription;
};

/** Definition of a single mission phase — narrative context, objectives, waves, and events. */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHPhaseDefinition
{
	GENERATED_BODY()

	/** The mission phase this definition corresponds to. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase")
	ESHMissionPhase Phase = ESHMissionPhase::PreInvasion;

	/** Display name for this phase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase")
	FText PhaseName;

	/** Narrative description of what is happening in this phase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase", meta = (MultiLine = true))
	FText PhaseDescription;

	/** Phase duration in seconds (0 = event-driven, no fixed timer). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase", meta = (ClampMin = "0.0"))
	float Duration = 0.f;

	/** Objectives active during this phase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase")
	TArray<FSHPhaseObjective> Objectives;

	/** Reinforcement waves scheduled during this phase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase")
	TArray<FSHWaveDefinition> ReinforcementWaves;

	/** Scripted events that fire during this phase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase")
	TArray<FSHScriptedEvent> ScriptedEvents;

	/** Dialogue triggers for this phase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase")
	TArray<FSHDialogueTrigger> DialogueTriggers;

	/** Condition that must be met to advance to the next phase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase")
	ESHPhaseAdvanceCondition AdvanceCondition = ESHPhaseAdvanceCondition::AllObjectivesComplete;

	/** Threshold for condition-based advance (e.g. 0.6 = 60% enemy strength for EnemyBreachesLine). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AdvanceThreshold = 0.f;

	/** Ambient weather type during this phase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase")
	ESHWeatherType AmbientWeather = ESHWeatherType::Clear;

	/** Time of day at the start of this phase (24-hour float, e.g. 5.5 = 05:30). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Phase", meta = (ClampMin = "0.0", ClampMax = "24.0"))
	float TimeOfDayStart = 6.f;
};

// -----------------------------------------------------------------------
//  USHMissionDefinition — Primary Data Asset
// -----------------------------------------------------------------------

/**
 * USHMissionDefinition
 *
 * Data-driven definition of a complete playable mission. Authored in the
 * editor as a Primary Data Asset — no code changes needed to create new
 * missions. Contains the briefing, phase structure, reinforcement waves,
 * scripted events, dialogue triggers, squad roster, and difficulty scaling.
 */
UCLASS(BlueprintType, Blueprintable)
class SHATTEREDHORIZON2032_API USHMissionDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	USHMissionDefinition();

	// ------------------------------------------------------------------
	//  UPrimaryDataAsset interface
	// ------------------------------------------------------------------

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

	// ------------------------------------------------------------------
	//  Mission identity
	// ------------------------------------------------------------------

	/** Unique identifier for this mission (used by save system and asset manager). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FName MissionId;

	/** Pre-mission briefing data. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	FSHMissionBriefing Briefing;

	/** Ordered list of mission phases. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	TArray<FSHPhaseDefinition> Phases;

	/** The player's squad roster for this mission. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	TArray<FSHSquadMemberDef> SquadRoster;

	/** Soft path to the map/level asset to load for this mission. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission", meta = (AllowedClasses = "/Script/Engine.World"))
	FSoftObjectPath MapAssetPath;

	/** Per-difficulty-tier enemy count multiplier (key = difficulty tier, value = multiplier). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission")
	TMap<int32, float> DifficultyScaling;

	/** Estimated play time in minutes (displayed on mission select). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mission", meta = (ClampMin = "0.0"))
	float EstimatedPlayTime = 30.f;

	// ------------------------------------------------------------------
	//  Queries
	// ------------------------------------------------------------------

	/** Find the phase definition for a given mission phase. Returns nullptr if not found. */
	UFUNCTION(BlueprintPure, Category = "SH|Mission")
	const FSHPhaseDefinition* GetPhaseDefinition(ESHMissionPhase Phase) const;

	/** Get the mission briefing data. */
	UFUNCTION(BlueprintPure, Category = "SH|Mission")
	const FSHMissionBriefing& GetBriefing() const { return Briefing; }

	/** Get the player's squad roster. */
	UFUNCTION(BlueprintPure, Category = "SH|Mission")
	const TArray<FSHSquadMemberDef>& GetSquadRoster() const { return SquadRoster; }

	// ------------------------------------------------------------------
	//  Validation
	// ------------------------------------------------------------------

	/**
	 * Validate the mission definition for completeness and consistency.
	 * Checks for missing data, orphaned objective references in scripted
	 * events, empty phases, and other authoring errors.
	 *
	 * @param OutErrors  Populated with human-readable error messages.
	 * @return True if the definition passes all checks.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Mission")
	bool Validate(TArray<FText>& OutErrors) const;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};
