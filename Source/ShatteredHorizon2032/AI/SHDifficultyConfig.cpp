// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "AI/SHDifficultyConfig.h"

USHDifficultyConfig::USHDifficultyConfig()
{
	// ---------------------------------------------------------------
	//  Recruit — Thinker & Mnemonic disabled, reactive only
	// ---------------------------------------------------------------
	{
		FSHDifficultyParams P;
		P.DetectionSpeedMultiplier     = 0.5f;
		P.SightRangeMultiplier         = 0.6f;
		P.AwarenessTransitionSpeed     = 0.5f;
		P.bThinkerEnabled              = false;
		P.bMnemonicEnabled             = false;
		P.MemoryRetentionMultiplier    = 0.0f;
		P.bPredictiveMovementEnabled   = false;
		P.DecisionCycleVariance        = 2.0f;  // Very exploitable
		P.bFlankingEnabled             = false;
		P.bSuppressCoordinationEnabled = false;
		P.bCommanderAIEnabled          = false;
		P.MaxCoordinatedSquads         = 1;
		P.AccuracyModifier             = 0.6f;
		P.ReactionTimeMultiplier       = 2.0f;
		P.bPredictiveGrenadeUsage      = false;
		P.MoraleRecoveryMultiplier     = 0.5f;
		P.SuppressionResistanceMultiplier = 0.5f;
		TierParams.Add(ESHDifficultyTier::Recruit, P);
	}

	// ---------------------------------------------------------------
	//  Regular — Thinker limited, short-term memory, basic coordination
	// ---------------------------------------------------------------
	{
		FSHDifficultyParams P;
		P.DetectionSpeedMultiplier     = 0.7f;
		P.SightRangeMultiplier         = 0.8f;
		P.AwarenessTransitionSpeed     = 0.7f;
		P.bThinkerEnabled              = true;  // Limited
		P.bMnemonicEnabled             = true;
		P.MemoryRetentionMultiplier    = 0.3f;  // Short-term
		P.bPredictiveMovementEnabled   = false;
		P.DecisionCycleVariance        = 1.5f;
		P.bFlankingEnabled             = true;
		P.bSuppressCoordinationEnabled = false;
		P.bCommanderAIEnabled          = false;
		P.MaxCoordinatedSquads         = 2;
		P.AccuracyModifier             = 0.75f;
		P.ReactionTimeMultiplier       = 1.5f;
		P.bPredictiveGrenadeUsage      = false;
		P.MoraleRecoveryMultiplier     = 0.7f;
		P.SuppressionResistanceMultiplier = 0.7f;
		TierParams.Add(ESHDifficultyTier::Regular, P);
	}

	// ---------------------------------------------------------------
	//  Hardened — Thinker active, full session memory, basic commander AI
	// ---------------------------------------------------------------
	{
		FSHDifficultyParams P;
		P.DetectionSpeedMultiplier     = 0.9f;
		P.SightRangeMultiplier         = 0.9f;
		P.AwarenessTransitionSpeed     = 0.9f;
		P.bThinkerEnabled              = true;
		P.bMnemonicEnabled             = true;
		P.MemoryRetentionMultiplier    = 0.7f;
		P.bPredictiveMovementEnabled   = false;
		P.DecisionCycleVariance        = 1.0f;
		P.bFlankingEnabled             = true;
		P.bSuppressCoordinationEnabled = true;
		P.bCommanderAIEnabled          = true;
		P.MaxCoordinatedSquads         = 3;
		P.AccuracyModifier             = 0.85f;
		P.ReactionTimeMultiplier       = 1.2f;
		P.bPredictiveGrenadeUsage      = true;
		P.MoraleRecoveryMultiplier     = 0.9f;
		P.SuppressionResistanceMultiplier = 0.9f;
		TierParams.Add(ESHDifficultyTier::Hardened, P);
	}

	// ---------------------------------------------------------------
	//  Veteran — Full stack, predictive movement, full commander AI
	// ---------------------------------------------------------------
	{
		FSHDifficultyParams P;
		P.DetectionSpeedMultiplier     = 1.0f;
		P.SightRangeMultiplier         = 1.0f;
		P.AwarenessTransitionSpeed     = 1.0f;
		P.bThinkerEnabled              = true;
		P.bMnemonicEnabled             = true;
		P.MemoryRetentionMultiplier    = 1.0f;
		P.bPredictiveMovementEnabled   = true;
		P.DecisionCycleVariance        = 0.5f;
		P.bFlankingEnabled             = true;
		P.bSuppressCoordinationEnabled = true;
		P.bCommanderAIEnabled          = true;
		P.MaxCoordinatedSquads         = 4;
		P.AccuracyModifier             = 0.95f;
		P.ReactionTimeMultiplier       = 1.0f;
		P.bPredictiveGrenadeUsage      = true;
		P.MoraleRecoveryMultiplier     = 1.0f;
		P.SuppressionResistanceMultiplier = 1.0f;
		TierParams.Add(ESHDifficultyTier::Veteran, P);
	}

	// ---------------------------------------------------------------
	//  Primordia Unleashed — All subsystems maximum cognitive depth
	// ---------------------------------------------------------------
	{
		FSHDifficultyParams P;
		P.DetectionSpeedMultiplier     = 1.2f;  // Slightly faster — vigilant
		P.SightRangeMultiplier         = 1.1f;
		P.AwarenessTransitionSpeed     = 1.3f;
		P.bThinkerEnabled              = true;
		P.bMnemonicEnabled             = true;
		P.MemoryRetentionMultiplier    = 1.0f;  // Perfect recall
		P.bPredictiveMovementEnabled   = true;
		P.DecisionCycleVariance        = 0.2f;  // Near-instant adaptation
		P.bFlankingEnabled             = true;
		P.bSuppressCoordinationEnabled = true;
		P.bCommanderAIEnabled          = true;
		P.MaxCoordinatedSquads         = 6;     // Multi-axis simultaneous
		P.AccuracyModifier             = 1.0f;  // Realistic — NOT aimbot
		P.ReactionTimeMultiplier       = 0.8f;  // Elite reaction times
		P.bPredictiveGrenadeUsage      = true;
		P.MoraleRecoveryMultiplier     = 1.3f;  // Disciplined troops
		P.SuppressionResistanceMultiplier = 1.2f; // Hardened fighters
		TierParams.Add(ESHDifficultyTier::PrimordiaUnleashed, P);
	}
}

FSHDifficultyParams USHDifficultyConfig::GetParamsForTier(ESHDifficultyTier Tier) const
{
	const FSHDifficultyParams* Found = TierParams.Find(Tier);
	if (Found)
	{
		return *Found;
	}

	// Default to Regular if not found.
	const FSHDifficultyParams* Regular = TierParams.Find(ESHDifficultyTier::Regular);
	return Regular ? *Regular : FSHDifficultyParams();
}
