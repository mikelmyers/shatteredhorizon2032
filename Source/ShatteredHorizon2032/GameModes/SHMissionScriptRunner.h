// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShatteredHorizon2032/Core/SHGameMode.h"
#include "ShatteredHorizon2032/GameModes/SHObjectiveSystem.h"
#include "SHMissionScriptRunner.generated.h"

class USHMissionDefinition;
class USHObjectiveSystem;
class USHDialogueSystem;
class USHWeatherSystem;
class USHCallForFireSystem;
class ASHElectronicWarfare;
class ASHDroneBase;
class ASHGameMode;
class USHGameSessionManager;

struct FSHDialogueLine;

DECLARE_LOG_CATEGORY_EXTERN(LogSH_MissionScript, Log, All);

// =====================================================================
//  Phase advance condition types
// =====================================================================

UENUM(BlueprintType)
enum class ESHPhaseAdvanceCondition : uint8
{
	/** All critical (Primary) objectives in this phase must be completed. */
	AllObjectivesComplete	UMETA(DisplayName = "All Objectives Complete"),

	/** The phase timer exceeds its configured duration. */
	TimerExpired			UMETA(DisplayName = "Timer Expired"),

	/** Enemy count ratio in the defensive zone exceeds a threshold. */
	EnemyBreachesLine		UMETA(DisplayName = "Enemy Breaches Line"),

	/** Friendly casualty ratio exceeds a threshold. */
	FriendlyForcesOverrun	UMETA(DisplayName = "Friendly Forces Overrun")
};

// =====================================================================
//  Scripted event types
// =====================================================================

UENUM(BlueprintType)
enum class ESHScriptedEventType : uint8
{
	ArtilleryBarrage	UMETA(DisplayName = "Artillery Barrage"),
	DroneSwarm			UMETA(DisplayName = "Drone Swarm"),
	EWJammingZone		UMETA(DisplayName = "EW Jamming Zone"),
	WeatherChange		UMETA(DisplayName = "Weather Change"),
	Dialogue			UMETA(DisplayName = "Dialogue"),
	AirStrike			UMETA(DisplayName = "Air Strike")
};

// =====================================================================
//  Data structs — wave, event, dialogue, objective, and phase defs
// =====================================================================

/** Enemy wave definition from a mission script. */
USTRUCT(BlueprintType)
struct FSHWaveDefinition
{
	GENERATED_BODY()

	/** Delay in seconds from the phase start before this wave spawns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	float DelayFromPhaseStart = 0.f;

	/** Infantry headcount. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	int32 InfantryCount = 0;

	/** Armored vehicle count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	int32 ArmorCount = 0;

	/** Amphibious vehicle count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	int32 AmphibiousCount = 0;

	/** Whether this wave includes air support elements. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	bool bIncludesAirSupport = false;

	/** Spawn zone tag identifying where these units appear. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	FName SpawnZoneTag;

	/** Optional squad template name for role assignment. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	FName SquadTemplateName;
};

/** Scripted event definition embedded in a mission phase. */
USTRUCT(BlueprintType)
struct FSHScriptedEventDefinition
{
	GENERATED_BODY()

	/** Unique event identifier within the mission. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	FName EventId;

	/** Type of scripted event. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	ESHScriptedEventType EventType = ESHScriptedEventType::ArtilleryBarrage;

	/** Delay in seconds from phase start before this event triggers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	float DelayFromPhaseStart = 0.f;

	/** World location for the event (barrage target, jamming center, etc.). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	FVector Location = FVector::ZeroVector;

	/** Radius for area-of-effect events (barrage spread, jamming radius). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	float Radius = 5000.f;

	/** Duration of the event in seconds (barrage length, jamming uptime). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	float Duration = 10.f;

	/** Intensity / strength (0-1) for configurable events. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission", meta = (ClampMin = "0", ClampMax = "1"))
	float Intensity = 0.8f;

	/** Number of rounds / drones / strikes for count-based events. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	int32 Count = 6;

	/** Weather type for WeatherChange events. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	uint8 WeatherTypeValue = 0;

	/** Whether the spawned elements are friendly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	bool bFriendly = false;

	/** Drone class for DroneSwarm events. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	TSoftClassPtr<ASHDroneBase> DroneClass;
};

/** Dialogue trigger embedded in a mission phase. */
USTRUCT(BlueprintType)
struct FSHDialogueTrigger
{
	GENERATED_BODY()

	/** Delay in seconds from phase start. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	float DelayFromPhaseStart = 0.f;

	/** Speaker name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	FText Speaker;

	/** Dialogue text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	FText Line;

	/** Soft reference to the audio cue. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	TSoftObjectPtr<USoundBase> AudioCue;

	/** Display / playback duration in seconds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	float Duration = 4.f;

	/** Dialogue priority value. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	int32 Priority = 50;
};

/** Objective definition embedded in a mission phase. */
USTRUCT(BlueprintType)
struct FSHPhaseObjectiveDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	ESHObjectiveType Type = ESHObjectiveType::Defend;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	ESHObjectivePriority Priority = ESHObjectivePriority::Primary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	FVector WorldLocation = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	float AreaRadius = 2000.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	float DefendDuration = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	float TimeLimit = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	int32 RequiredPresence = 1;
};

/** Phase advance condition entry. */
USTRUCT(BlueprintType)
struct FSHPhaseAdvanceRule
{
	GENERATED_BODY()

	/** Condition type. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	ESHPhaseAdvanceCondition Condition = ESHPhaseAdvanceCondition::AllObjectivesComplete;

	/** Threshold value for ratio-based conditions (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission", meta = (ClampMin = "0", ClampMax = "1"))
	float Threshold = 0.5f;

	/** Zone tag for spatial conditions (EnemyBreachesLine). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	FName ZoneTag;
};

/** Complete definition for a single mission phase. */
USTRUCT(BlueprintType)
struct FSHPhaseDefinition
{
	GENERATED_BODY()

	/** Which mission phase this definition represents. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	ESHMissionPhase Phase = ESHMissionPhase::PreInvasion;

	/** Maximum phase duration in seconds (0 = unlimited). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	float PhaseDuration = 0.f;

	/** Objectives activated when this phase begins. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	TArray<FSHPhaseObjectiveDefinition> Objectives;

	/** Enemy waves spawned during this phase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	TArray<FSHWaveDefinition> Waves;

	/** Scripted events triggered during this phase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	TArray<FSHScriptedEventDefinition> ScriptedEvents;

	/** Dialogue lines triggered during this phase. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	TArray<FSHDialogueTrigger> DialogueTriggers;

	/** Conditions under which this phase advances to the next. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	TArray<FSHPhaseAdvanceRule> AdvanceConditions;

	/** Weather type to set at the start of this phase (255 = no change). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	uint8 InitialWeatherType = 255;

	/** Weather transition duration in seconds (if InitialWeatherType is set). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	float WeatherTransitionDuration = 30.f;
};

// =====================================================================
//  Mission Definition — UDataAsset
// =====================================================================

/**
 * USHMissionDefinition
 *
 * Data asset describing the complete mission script for a Shattered
 * Horizon scenario. Contains all phases, waves, events, objectives,
 * and dialogue needed for the MissionScriptRunner to orchestrate
 * the mission experience.
 */
UCLASS(BlueprintType)
class SHATTEREDHORIZON2032_API USHMissionDefinition : public UDataAsset
{
	GENERATED_BODY()

public:
	/** Human-readable mission name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	FText MissionName;

	/** Mission identifier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	FName MissionId;

	/** Ordered list of phase definitions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	TArray<FSHPhaseDefinition> Phases;

	/** Brief description shown in briefing screens. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	FText Description;

	/** Map name this mission is designed for. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	FString MapName;

	/** Expected total mission duration in seconds (for UI). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	float EstimatedDuration = 1800.f;
};

// =====================================================================
//  Delegates
// =====================================================================

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMissionPhaseAdvanced, ESHMissionPhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnScriptedEventTriggered, FName, EventId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMissionComplete, bool, bVictory);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMissionFailed, FText, Reason);

// =====================================================================
//  USHMissionScriptRunner — Actor Component
// =====================================================================

/**
 * USHMissionScriptRunner
 *
 * UActorComponent designed to be added to the GameMode actor. Takes a
 * USHMissionDefinition data asset and drives the entire mission
 * experience: phase progression, objective activation, enemy wave
 * spawning, scripted event execution, dialogue playback, and weather
 * transitions.
 *
 * Tick-driven: each frame it checks advance conditions, fires timed
 * events, and spawns scheduled waves.
 */
UCLASS(ClassGroup = (GameModes), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHMissionScriptRunner : public UActorComponent
{
	GENERATED_BODY()

public:
	USHMissionScriptRunner();

	// ------------------------------------------------------------------
	//  UActorComponent overrides
	// ------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// ------------------------------------------------------------------
	//  Mission lifecycle
	// ------------------------------------------------------------------

	/**
	 * Load a mission definition and prepare internal state.
	 * Must be called before StartMission().
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|MissionScript")
	void LoadMission(USHMissionDefinition* Definition);

	/** Begin the mission from phase 0. LoadMission() must have been called. */
	UFUNCTION(BlueprintCallable, Category = "SH|MissionScript")
	void StartMission();

	/** Manually advance to the next phase. */
	UFUNCTION(BlueprintCallable, Category = "SH|MissionScript")
	void AdvancePhase();

	// ------------------------------------------------------------------
	//  Queries
	// ------------------------------------------------------------------

	/** Get the currently active mission phase enum. */
	UFUNCTION(BlueprintPure, Category = "SH|MissionScript")
	ESHMissionPhase GetCurrentPhase() const;

	/** Get the 0-based index of the current phase. */
	UFUNCTION(BlueprintPure, Category = "SH|MissionScript")
	int32 GetCurrentPhaseIndex() const { return CurrentPhaseIndex; }

	/** Total elapsed time since mission start (seconds). */
	UFUNCTION(BlueprintPure, Category = "SH|MissionScript")
	float GetMissionElapsedTime() const { return MissionElapsedTime; }

	/** Elapsed time within the current phase (seconds). */
	UFUNCTION(BlueprintPure, Category = "SH|MissionScript")
	float GetPhaseElapsedTime() const { return PhaseElapsedTime; }

	/** Return the loaded mission definition, or nullptr. */
	UFUNCTION(BlueprintPure, Category = "SH|MissionScript")
	USHMissionDefinition* GetActiveMission() const { return ActiveMission; }

	/** Whether a mission is currently running. */
	UFUNCTION(BlueprintPure, Category = "SH|MissionScript")
	bool IsMissionRunning() const { return bMissionRunning; }

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|MissionScript")
	FOnMissionPhaseAdvanced OnPhaseAdvanced;

	UPROPERTY(BlueprintAssignable, Category = "SH|MissionScript")
	FOnScriptedEventTriggered OnScriptedEventTriggered;

	UPROPERTY(BlueprintAssignable, Category = "SH|MissionScript")
	FOnMissionComplete OnMissionComplete;

	UPROPERTY(BlueprintAssignable, Category = "SH|MissionScript")
	FOnMissionFailed OnMissionFailed;

protected:
	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Defensive zone tag used for EnemyBreachesLine condition checks. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|MissionScript|Config")
	FName DefaultDefensiveZoneTag = TEXT("DefenseZone");

	/** Radius for defensive zone spatial queries (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|MissionScript|Config")
	float DefensiveZoneRadius = 20000.f;

	/** Center of the defensive zone (overridden per-phase if ZoneTag is set). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|MissionScript|Config")
	FVector DefensiveZoneCenter = FVector::ZeroVector;

	/** Total friendly force count at mission start (for casualty ratio). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|MissionScript|Config")
	int32 InitialFriendlyCount = 24;

private:
	// ------------------------------------------------------------------
	//  Phase management
	// ------------------------------------------------------------------

	/** Enter the phase at CurrentPhaseIndex — activate objectives, schedule events. */
	void EnterCurrentPhase();

	/** Deactivate current phase objectives and clean up pending queues. */
	void ExitCurrentPhase();

	// ------------------------------------------------------------------
	//  Tick sub-routines
	// ------------------------------------------------------------------

	/** Check all phase advance conditions; advance if any are met. */
	void TickAdvanceConditions(float DeltaTime);

	/** Spawn any waves whose delay has been reached. */
	void TickPendingWaves(float DeltaTime);

	/** Trigger any scripted events whose delay has been reached. */
	void TickPendingEvents(float DeltaTime);

	/** Play any dialogue lines whose delay has been reached. */
	void TickPendingDialogue(float DeltaTime);

	/** Monitor active objectives for critical failures. */
	void TickObjectiveMonitoring();

	// ------------------------------------------------------------------
	//  Advance condition evaluators
	// ------------------------------------------------------------------

	bool EvaluateAllObjectivesComplete() const;
	bool EvaluateTimerExpired() const;
	bool EvaluateEnemyBreachesLine(const FSHPhaseAdvanceRule& Rule) const;
	bool EvaluateFriendlyForcesOverrun(const FSHPhaseAdvanceRule& Rule) const;

	// ------------------------------------------------------------------
	//  Event execution
	// ------------------------------------------------------------------

	void ExecuteScriptedEvent(const FSHScriptedEventDefinition& Event);
	void ExecuteArtilleryBarrage(const FSHScriptedEventDefinition& Event);
	void ExecuteDroneSwarm(const FSHScriptedEventDefinition& Event);
	void ExecuteEWJammingZone(const FSHScriptedEventDefinition& Event);
	void ExecuteWeatherChange(const FSHScriptedEventDefinition& Event);
	void ExecuteDialogue(const FSHScriptedEventDefinition& Event);
	void ExecuteAirStrike(const FSHScriptedEventDefinition& Event);

	// ------------------------------------------------------------------
	//  Wave spawning
	// ------------------------------------------------------------------

	/** Convert FSHWaveDefinition to FSHReinforcementWave and feed to game mode. */
	void SpawnWave(const FSHWaveDefinition& WaveDef);

	// ------------------------------------------------------------------
	//  Objective management
	// ------------------------------------------------------------------

	/** Create objectives from phase definitions via the ObjectiveSystem. */
	void ActivatePhaseObjectives(const FSHPhaseDefinition& PhaseDef);

	/** Deactivate all objectives tracked in ActiveObjectiveIds. */
	void DeactivatePhaseObjectives();

	// ------------------------------------------------------------------
	//  Completion
	// ------------------------------------------------------------------

	void CompleteMission(bool bVictory);
	void FailMission(const FText& Reason);

	// ------------------------------------------------------------------
	//  System caching
	// ------------------------------------------------------------------

	void CacheSubsystems();

	USHObjectiveSystem* GetObjectiveSystem() const;
	USHDialogueSystem* GetDialogueSystem() const;
	USHWeatherSystem* GetWeatherSystem() const;
	ASHElectronicWarfare* GetEWManager() const;
	ASHGameMode* GetSHGameMode() const;
	USHGameSessionManager* GetSessionManager() const;

	// ------------------------------------------------------------------
	//  Helper — count actors in a radius
	// ------------------------------------------------------------------

	int32 CountEnemiesInRadius(const FVector& Center, float Radius) const;
	int32 CountFriendliesAlive() const;

	// ------------------------------------------------------------------
	//  Runtime state
	// ------------------------------------------------------------------

	/** The loaded mission definition. */
	UPROPERTY()
	TObjectPtr<USHMissionDefinition> ActiveMission = nullptr;

	/** Whether the mission is actively running. */
	bool bMissionRunning = false;

	/** Whether the mission has ended (prevent re-entry). */
	bool bMissionEnded = false;

	/** Current phase index into ActiveMission->Phases. */
	int32 CurrentPhaseIndex = 0;

	/** Elapsed seconds within the current phase. */
	float PhaseElapsedTime = 0.f;

	/** Total elapsed seconds since StartMission(). */
	float MissionElapsedTime = 0.f;

	/** Waves not yet spawned for the current phase, sorted by delay ascending. */
	TArray<FSHWaveDefinition> PendingWaves;

	/** Scripted events not yet triggered for the current phase. */
	TArray<FSHScriptedEventDefinition> PendingEvents;

	/** Dialogue triggers not yet played for the current phase. */
	TArray<FSHDialogueTrigger> PendingDialogue;

	/** GUIDs of objectives created for the current phase. */
	TArray<FGuid> ActiveObjectiveIds;

	// --- Cached subsystem pointers (refreshed on BeginPlay / phase entry) ---

	UPROPERTY()
	TObjectPtr<USHObjectiveSystem> CachedObjectiveSystem = nullptr;

	UPROPERTY()
	TObjectPtr<USHDialogueSystem> CachedDialogueSystem = nullptr;

	UPROPERTY()
	TObjectPtr<USHWeatherSystem> CachedWeatherSystem = nullptr;

	UPROPERTY()
	TWeakObjectPtr<ASHElectronicWarfare> CachedEWManager = nullptr;

	UPROPERTY()
	TWeakObjectPtr<ASHGameMode> CachedGameMode = nullptr;
};
