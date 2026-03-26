// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "SHGameMode.generated.h"

class ASHGameState;
class ASHPlayerController;
class ASHPlayerCharacter;
class ASHEnemyCharacter;
class USHPrimordiaDecisionEngine;

/** Mission phases representing the arc of the Taoyuan Beach defense. */
UENUM(BlueprintType)
enum class ESHMissionPhase : uint8
{
	PreInvasion     UMETA(DisplayName = "Pre-Invasion"),
	BeachAssault    UMETA(DisplayName = "Beach Assault"),
	UrbanFallback   UMETA(DisplayName = "Urban Fallback"),
	Counterattack   UMETA(DisplayName = "Counterattack"),
	Collapse        UMETA(DisplayName = "Collapse"),           // Defensive line breaks, fighting withdrawal
	GuerrillaHoldout UMETA(DisplayName = "Guerrilla Holdout")  // Urban survival until relief arrives
};

/** Descriptor for an enemy reinforcement wave. */
USTRUCT(BlueprintType)
struct FSHReinforcementWave
{
	GENERATED_BODY()

	/** Delay in seconds after phase start before this wave spawns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DelayFromPhaseStart = 0.f;

	/** Number of infantry units in the wave. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 InfantryCount = 0;

	/** Number of armored vehicles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ArmorCount = 0;

	/** Number of amphibious assault vehicles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 AmphibiousCount = 0;

	/** If true, this wave includes air support elements. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIncludesAirSupport = false;

	/** Spawn zone tag used by the spawning subsystem. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName SpawnZoneTag;
};

/** Dynamic objective generated during gameplay. */
USTRUCT(BlueprintType)
struct FSHDynamicObjective
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FGuid ObjectiveId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector WorldLocation = FVector::ZeroVector;

	/** 0..1 completion fraction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Progress = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsComplete = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bIsFailed = false;

	/** Phase during which this objective is valid. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESHMissionPhase RelevantPhase = ESHMissionPhase::BeachAssault;
};

/**
 * ASHGameMode
 *
 * Authoritative game mode for the Taoyuan Beach defense scenario.
 * Manages mission phase progression, enemy reinforcement scheduling,
 * dynamic objective generation, and interfaces with the Primordia AI
 * director to drive enemy grand strategy.
 */
UCLASS()
class SHATTEREDHORIZON2032_API ASHGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	ASHGameMode();

	// ------------------------------------------------------------------
	//  AGameModeBase overrides
	// ------------------------------------------------------------------
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;

	// ------------------------------------------------------------------
	//  Phase management
	// ------------------------------------------------------------------

	/** Force-advance to a specific mission phase (authority only). */
	UFUNCTION(BlueprintCallable, Category = "SH|GameMode")
	void AdvanceToPhase(ESHMissionPhase NewPhase);

	/** Query current phase. */
	UFUNCTION(BlueprintPure, Category = "SH|GameMode")
	ESHMissionPhase GetCurrentPhase() const { return CurrentPhase; }

	// ------------------------------------------------------------------
	//  Reinforcements
	// ------------------------------------------------------------------

	/** Manually trigger a reinforcement wave (debug / scripted events). */
	UFUNCTION(BlueprintCallable, Category = "SH|GameMode")
	void TriggerReinforcementWave(const FSHReinforcementWave& Wave);

	// ------------------------------------------------------------------
	//  Dynamic objectives
	// ------------------------------------------------------------------

	/** Generate a new dynamic objective and replicate it via game state. */
	UFUNCTION(BlueprintCallable, Category = "SH|GameMode")
	FGuid CreateDynamicObjective(const FText& Name, const FText& Desc, FVector Location, ESHMissionPhase Phase);

	/** Mark a dynamic objective as complete. */
	UFUNCTION(BlueprintCallable, Category = "SH|GameMode")
	void CompleteObjective(const FGuid& ObjectiveId);

	/** Mark a dynamic objective as failed. */
	UFUNCTION(BlueprintCallable, Category = "SH|GameMode")
	void FailObjective(const FGuid& ObjectiveId);

	// ------------------------------------------------------------------
	//  Primordia AI interface
	// ------------------------------------------------------------------

	/** Request the AI director to evaluate the current tactical situation. */
	UFUNCTION(BlueprintCallable, Category = "SH|AI")
	void RequestPrimordiaEvaluation();

	// ------------------------------------------------------------------
	//  Time-of-day
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|Environment")
	float GetTimeOfDayHours() const { return TimeOfDayHours; }

	UFUNCTION(BlueprintCallable, Category = "SH|Environment")
	void SetTimeOfDayHours(float Hours);

	UFUNCTION(BlueprintCallable, Category = "SH|Environment")
	void SetTimeScale(float Scale);

protected:
	// ------------------------------------------------------------------
	//  Internal tick helpers
	// ------------------------------------------------------------------
	void TickPhaseLogic(float DeltaSeconds);
	void TickReinforcementScheduler(float DeltaSeconds);
	void TickTimeOfDay(float DeltaSeconds);
	void TickPrimordiaAI(float DeltaSeconds);

	/** Evaluate whether conditions are met to auto-advance the phase. */
	bool ShouldAdvancePhase() const;

	/** Build the reinforcement schedule for a given phase. */
	void BuildReinforcementSchedule(ESHMissionPhase Phase);

	/** Spawn units for a reinforcement wave. */
	void ExecuteWaveSpawn(const FSHReinforcementWave& Wave);

	/** Push updated objectives to the game state for replication. */
	void ReplicateObjectivesToGameState();

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** How long the pre-invasion phase lasts before the assault begins (seconds). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Phase")
	float PreInvasionDuration = 180.f;

	/** Enemy force strength threshold to trigger urban fallback. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Phase")
	float FallbackEnemyStrengthThreshold = 0.6f;

	/** Defensive line integrity threshold that triggers counterattack eligibility. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Phase")
	float CounterattackIntegrityThreshold = 0.4f;

	/** Reinforcement wave definitions per phase. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Reinforcements")
	TMap<ESHMissionPhase, TArray<FSHReinforcementWave>> PhaseReinforcementTemplates;

	/** Primordia AI tick interval (seconds). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|AI")
	float PrimordiaTickInterval = 5.f;

	/** In-game time at mission start (24 h format, e.g. 5.5 = 05:30). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Environment")
	float StartingTimeOfDay = 5.0f;

	/** Real-seconds per in-game hour. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Environment")
	float SecondsPerGameHour = 300.f;

private:
	// Runtime state -------------------------------------------------------

	UPROPERTY()
	ESHMissionPhase CurrentPhase = ESHMissionPhase::PreInvasion;

	/** Elapsed time within the current phase. */
	float PhaseElapsedTime = 0.f;

	/** Time-of-day in 24 h float. */
	float TimeOfDayHours = 5.f;

	/** Active time scale multiplier. */
	float TimeScaleMultiplier = 1.f;

	/** Reinforcement waves pending for the current phase. */
	TArray<FSHReinforcementWave> PendingWaves;

	/** Indices of waves already dispatched. */
	TSet<int32> DispatchedWaveIndices;

	/** Active dynamic objectives keyed by GUID. */
	TMap<FGuid, FSHDynamicObjective> ActiveObjectives;

	/** Accumulator for Primordia AI tick. */
	float PrimordiaTickAccumulator = 0.f;

	/** Cached pointer to the authoritative game state. */
	UPROPERTY()
	TObjectPtr<ASHGameState> SHGameState = nullptr;

	/** Enemy character class to spawn for infantry. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Spawning")
	TSubclassOf<AActor> EnemyCharacterClass;

	/** Armored vehicle class to spawn (e.g., ZBD-05, Type 08). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Spawning")
	TSubclassOf<AActor> ArmorVehicleClass;

	/** Amphibious assault vehicle class to spawn. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Spawning")
	TSubclassOf<AActor> AmphibiousVehicleClass;

	/** Primordia decision engine instance for tactical order assignment. */
	UPROPERTY()
	TObjectPtr<USHPrimordiaDecisionEngine> PrimordiaDecisionEngine = nullptr;

	/** Auto-incrementing squad ID for Primordia registration. */
	int32 NextSquadId = 1;
};
