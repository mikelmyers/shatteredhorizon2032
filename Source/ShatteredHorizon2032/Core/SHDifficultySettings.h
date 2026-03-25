// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "SHDifficultySettings.generated.h"

class ASHEnemyAIController;
class ASHPlayerCharacter;

// -----------------------------------------------------------------------
//  Difficulty tier enum
// -----------------------------------------------------------------------

/**
 * Difficulty tiers for Shattered Horizon 2032.
 * Each tier progressively enables more Primordia AI subsystems and
 * tightens enemy lethality while increasing pressure on the player.
 */
UENUM(BlueprintType)
enum class ESHDifficultyTier : uint8
{
	/** Thinker & Mnemonic disabled; reactive only. */
	Recruit            UMETA(DisplayName = "Recruit"),

	/** Thinker limited; short-term memory; basic coordination. */
	Regular            UMETA(DisplayName = "Regular"),

	/** Thinker active; full session memory; basic commander AI. */
	Hardened           UMETA(DisplayName = "Hardened"),

	/** Full stack; predictive movement; full commander AI. */
	Veteran            UMETA(DisplayName = "Veteran"),

	/** All subsystems at maximum cognitive depth. */
	PrimordiaUnleashed UMETA(DisplayName = "Primordia Unleashed")
};

// -----------------------------------------------------------------------
//  Per-difficulty configuration struct
// -----------------------------------------------------------------------

/**
 * FSHDifficultyConfig
 *
 * Comprehensive per-tier configuration that governs AI perception,
 * behavior, accuracy, morale, commander-level systems, Primordia
 * cognitive settings, and player-facing modifiers.
 */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHDifficultyConfig
{
	GENERATED_BODY()

	// ------------------------------------------------------------------
	//  AI Perception
	// ------------------------------------------------------------------

	/** Multiplier on base AI sight range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Perception",
		meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float SightRangeMultiplier = 1.0f;

	/** Multiplier on base AI hearing range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Perception",
		meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float HearingRangeMultiplier = 1.0f;

	/** Multiplier on AI reaction time (higher = slower reactions). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Perception",
		meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float ReactionTimeMultiplier = 1.0f;

	/** Multiplier on how fast the AI accumulates detection on a target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Perception",
		meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float DetectionSpeedMultiplier = 1.0f;

	// ------------------------------------------------------------------
	//  AI Behavior
	// ------------------------------------------------------------------

	/** Enable the Primordia Decision Engine (Thinker). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Behavior")
	bool bEnablePrimordiaDecisionEngine = false;

	/** Enable the Primordia Mnemonic (session memory). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Behavior")
	bool bEnablePrimordiaMnemonic = false;

	/** Enable the Primordia Simulon (predictive player modeling). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Behavior")
	bool bEnablePrimordiaSimulon = false;

	/** Enable squad-level coordination. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Behavior")
	bool bEnableSquadCoordination = false;

	/** Enable flanking maneuvers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Behavior")
	bool bEnableFlankingBehavior = false;

	/** Enable grenade usage by AI. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Behavior")
	bool bEnableGrenadeUsage = false;

	/** Minimum variance added to the AI decision cycle (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Behavior",
		meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float DecisionCycleVarianceMin = 0.0f;

	/** Maximum variance added to the AI decision cycle (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Behavior",
		meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float DecisionCycleVarianceMax = 0.0f;

	// ------------------------------------------------------------------
	//  AI Accuracy
	// ------------------------------------------------------------------

	/** Multiplier on base AI accuracy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Accuracy",
		meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float AccuracyMultiplier = 1.0f;

	/** Multiplier on burst length when AI fires in bursts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Accuracy",
		meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float BurstLengthMultiplier = 1.0f;

	/** Additional accuracy penalty applied when AI is under suppression. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Accuracy",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SuppressionAccuracyPenalty = 0.3f;

	// ------------------------------------------------------------------
	//  AI Morale
	// ------------------------------------------------------------------

	/** Multiplier on how fast AI morale recovers after stress. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Morale",
		meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float MoraleRecoveryMultiplier = 1.0f;

	/** Multiplier on the morale threshold at which AI may surrender. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Morale",
		meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float SurrenderThresholdMultiplier = 1.0f;

	// ------------------------------------------------------------------
	//  AI Commander
	// ------------------------------------------------------------------

	/** Enable commander-level AI (reinforcements, force allocation). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Commander")
	bool bEnableCommanderAI = false;

	/** Multiplier on how frequently the commander dispatches reinforcements. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Commander",
		meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float ReinforcementFrequencyMultiplier = 1.0f;

	/** Quality of force allocation decisions (0 = random, 1 = optimal). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI Commander",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ForceAllocationQuality = 0.0f;

	// ------------------------------------------------------------------
	//  Player
	// ------------------------------------------------------------------

	/** Multiplier on damage received by the player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player",
		meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float DamageReceivedMultiplier = 1.0f;

	/** Multiplier on the player's stamina drain rate. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player",
		meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float StaminaDrainMultiplier = 1.0f;

	/** Multiplier on the player's bleed rate from wounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Player",
		meta = (ClampMin = "0.1", ClampMax = "3.0"))
	float BleedRateMultiplier = 1.0f;

	// ------------------------------------------------------------------
	//  Primordia
	// ------------------------------------------------------------------

	/** Maximum cognitive depth for Primordia subsystems (0 = minimal, 1 = full). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Primordia",
		meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PrimordiaMaxCognitiveDepth = 0.0f;

	/** How long Mnemonic retains session memory (seconds). 0 = disabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Primordia",
		meta = (ClampMin = "0.0"))
	float MnemonicSessionMemoryDuration = 0.0f;
};

// -----------------------------------------------------------------------
//  Difficulty delegate
// -----------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHOnDifficultyChanged, ESHDifficultyTier, NewTier);

// -----------------------------------------------------------------------
//  USHDifficultySettings
// -----------------------------------------------------------------------

/**
 * USHDifficultySettings
 *
 * UDeveloperSettings subclass that exposes per-difficulty-tier
 * configuration in Project Settings (Game > Shattered Horizon Difficulty).
 *
 * Each of the five difficulty tiers maps to an FSHDifficultyConfig that
 * parameterizes every aspect of AI behavior — from perception ranges
 * and accuracy to Primordia cognitive subsystems — as well as player-
 * facing modifiers like incoming damage and stamina drain.
 *
 * Use the static GetDifficultySettings() accessor from anywhere in
 * gameplay code to retrieve the singleton instance.
 */
UCLASS(config = Game, defaultconfig, meta = (DisplayName = "Shattered Horizon Difficulty"))
class SHATTEREDHORIZON2032_API USHDifficultySettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	USHDifficultySettings();

	// ------------------------------------------------------------------
	//  Static accessor
	// ------------------------------------------------------------------

	/** Retrieve the singleton settings object. */
	static USHDifficultySettings* GetDifficultySettings();

	// ------------------------------------------------------------------
	//  UDeveloperSettings overrides
	// ------------------------------------------------------------------

	virtual FName GetContainerName() const override { return TEXT("Project"); }
	virtual FName GetCategoryName() const override  { return TEXT("Game"); }
	virtual FName GetSectionName() const override   { return TEXT("Shattered Horizon Difficulty"); }

#if WITH_EDITOR
	virtual FText GetSectionText() const override;
	virtual FText GetSectionDescription() const override;
#endif

	// ------------------------------------------------------------------
	//  Configuration map
	// ------------------------------------------------------------------

	/** Per-tier difficulty configurations. Populated with defaults on construction. */
	UPROPERTY(EditAnywhere, config, BlueprintReadOnly, Category = "Difficulty",
		meta = (DisplayName = "Difficulty Configurations"))
	TMap<ESHDifficultyTier, FSHDifficultyConfig> DifficultyConfigs;

	// ------------------------------------------------------------------
	//  Active difficulty
	// ------------------------------------------------------------------

	/** Get the config for a specific tier. Falls back to Hardened if tier is missing. */
	UFUNCTION(BlueprintPure, Category = "SH|Difficulty")
	const FSHDifficultyConfig& GetConfigForTier(ESHDifficultyTier Tier) const;

	/** Get the config for the currently active difficulty tier. */
	UFUNCTION(BlueprintPure, Category = "SH|Difficulty")
	const FSHDifficultyConfig& GetCurrentConfig() const;

	/** Set the active difficulty tier. Broadcasts OnDifficultyChanged. */
	UFUNCTION(BlueprintCallable, Category = "SH|Difficulty")
	void SetActiveDifficulty(ESHDifficultyTier NewTier);

	/** Get the currently active difficulty tier. */
	UFUNCTION(BlueprintPure, Category = "SH|Difficulty")
	ESHDifficultyTier GetActiveDifficulty() const { return ActiveDifficulty; }

	// ------------------------------------------------------------------
	//  Application helpers
	// ------------------------------------------------------------------

	/**
	 * Apply the current difficulty settings to an AI controller.
	 * Configures perception, behavior toggles, accuracy, morale,
	 * commander AI, and Primordia subsystems.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Difficulty")
	void ApplyDifficultyToAI(ASHEnemyAIController* AIController) const;

	/**
	 * Apply the current difficulty settings to the player character.
	 * Adjusts damage received multiplier, stamina drain, and bleed rate.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Difficulty")
	void ApplyDifficultyToPlayer(ASHPlayerCharacter* PlayerCharacter) const;

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	/** Fired when the active difficulty tier changes. */
	UPROPERTY(BlueprintAssignable, Category = "SH|Difficulty")
	FSHOnDifficultyChanged OnDifficultyChanged;

private:
	/** Populate DifficultyConfigs with GDD-defined default values. */
	void InitializeDefaultConfigs();

	/** The currently active difficulty tier. */
	UPROPERTY(config)
	ESHDifficultyTier ActiveDifficulty = ESHDifficultyTier::Hardened;

	/** Fallback config returned when a tier lookup fails. */
	static const FSHDifficultyConfig FallbackConfig;
};
