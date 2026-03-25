// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "SHDifficultyConfig.generated.h"

/**
 * Difficulty tier — scales AI cognitive depth, NOT health/damage.
 * Doctrine: "Difficulty scales AI intelligence, not aim accuracy or health pools."
 */
UENUM(BlueprintType)
enum class ESHDifficultyTier : uint8
{
	/** Thinker & Mnemonic disabled. Reactive only. */
	Recruit				UMETA(DisplayName = "Recruit"),

	/** Thinker limited. Short-term memory. Basic coordination. */
	Regular				UMETA(DisplayName = "Regular"),

	/** Thinker active. Full session memory. Basic commander AI. */
	Hardened			UMETA(DisplayName = "Hardened"),

	/** Full stack. Predictive movement. Full commander AI. */
	Veteran				UMETA(DisplayName = "Veteran"),

	/** All subsystems at maximum cognitive depth. */
	PrimordiaUnleashed	UMETA(DisplayName = "Primordia Unleashed"),
};

/**
 * Per-difficulty AI configuration.
 * Controls how much of the AI stack is active at each tier.
 */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHDifficultyParams
{
	GENERATED_BODY()

	// ------------------------------------------------------------------
	//  Perception
	// ------------------------------------------------------------------

	/** Multiplier on AI sight detection speed. Lower = slower to detect player. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception")
	float DetectionSpeedMultiplier = 1.0f;

	/** Multiplier on AI sight range. Lower = shorter detection range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception")
	float SightRangeMultiplier = 1.0f;

	/** Multiplier on awareness state transition speed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Perception")
	float AwarenessTransitionSpeed = 1.0f;

	// ------------------------------------------------------------------
	//  Decision quality
	// ------------------------------------------------------------------

	/** Whether tactical Thinker AI is enabled (commander-level decisions). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	bool bThinkerEnabled = true;

	/** Whether session memory (Mnemonic) tracks player patterns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	bool bMnemonicEnabled = true;

	/** Memory duration multiplier. 0 = no memory, 1 = full retention. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI", meta = (ClampMin = "0", ClampMax = "1"))
	float MemoryRetentionMultiplier = 1.0f;

	/** Whether AI uses predictive movement (Simulon anticipates player paths). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	bool bPredictiveMovementEnabled = true;

	/** AI decision cycle variance (seconds). Higher = more exploitable patterns. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AI")
	float DecisionCycleVariance = 0.5f;

	// ------------------------------------------------------------------
	//  Coordination
	// ------------------------------------------------------------------

	/** Whether AI squads coordinate flanking maneuvers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coordination")
	bool bFlankingEnabled = true;

	/** Whether AI uses suppressive fire coordination. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coordination")
	bool bSuppressCoordinationEnabled = true;

	/** Whether commander AI issues multi-axis assault orders. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coordination")
	bool bCommanderAIEnabled = true;

	/** Max squads that can coordinate on a single objective simultaneously. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Coordination")
	int32 MaxCoordinatedSquads = 4;

	// ------------------------------------------------------------------
	//  Engagement
	// ------------------------------------------------------------------

	/** Accuracy modifier. 1.0 = realistic, <1 = worse aim (but never "stormtrooper aim"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Engagement", meta = (ClampMin = "0.3", ClampMax = "1.0"))
	float AccuracyModifier = 1.0f;

	/** Reaction time multiplier. >1 = slower reactions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Engagement", meta = (ClampMin = "0.5", ClampMax = "3.0"))
	float ReactionTimeMultiplier = 1.0f;

	/** Whether AI pre-throws grenades based on predicted player position. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Engagement")
	bool bPredictiveGrenadeUsage = true;

	// ------------------------------------------------------------------
	//  Morale
	// ------------------------------------------------------------------

	/** Morale recovery rate multiplier. Higher = AI recovers morale faster. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Morale")
	float MoraleRecoveryMultiplier = 1.0f;

	/** Suppression resistance multiplier. Higher = harder to suppress. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Morale")
	float SuppressionResistanceMultiplier = 1.0f;
};

/**
 * USHDifficultyConfig
 *
 * Data asset containing the full configuration for all 5 difficulty tiers.
 * These are NOT about making the AI cheat — they control cognitive depth.
 */
UCLASS(BlueprintType)
class SHATTEREDHORIZON2032_API USHDifficultyConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	USHDifficultyConfig();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Difficulty")
	TMap<ESHDifficultyTier, FSHDifficultyParams> TierParams;

	/** Get params for a difficulty tier. Returns default if not found. */
	UFUNCTION(BlueprintCallable, Category = "Difficulty")
	FSHDifficultyParams GetParamsForTier(ESHDifficultyTier Tier) const;
};
