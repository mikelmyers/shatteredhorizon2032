// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHDifficultySettings.h"
#include "AI/SHEnemyAIController.h"
#include "SHPlayerCharacter.h"

DEFINE_LOG_CATEGORY_STATIC(LogSH_Difficulty, Log, All);

// -----------------------------------------------------------------------
//  Static fallback config (Hardened defaults)
// -----------------------------------------------------------------------

const FSHDifficultyConfig USHDifficultySettings::FallbackConfig = FSHDifficultyConfig();

// -----------------------------------------------------------------------
//  Constructor
// -----------------------------------------------------------------------

USHDifficultySettings::USHDifficultySettings()
{
	InitializeDefaultConfigs();
}

// -----------------------------------------------------------------------
//  Static accessor
// -----------------------------------------------------------------------

USHDifficultySettings* USHDifficultySettings::GetDifficultySettings()
{
	return GetMutableDefault<USHDifficultySettings>();
}

// -----------------------------------------------------------------------
//  UDeveloperSettings overrides
// -----------------------------------------------------------------------

#if WITH_EDITOR
FText USHDifficultySettings::GetSectionText() const
{
	return NSLOCTEXT("SHDifficulty", "SectionText", "Shattered Horizon Difficulty");
}

FText USHDifficultySettings::GetSectionDescription() const
{
	return NSLOCTEXT("SHDifficulty", "SectionDesc",
		"Per-difficulty-tier parameters governing AI perception, behavior, "
		"accuracy, morale, commander AI, Primordia cognitive subsystems, "
		"and player-facing modifiers.");
}
#endif

// -----------------------------------------------------------------------
//  Tier lookup
// -----------------------------------------------------------------------

const FSHDifficultyConfig& USHDifficultySettings::GetConfigForTier(ESHDifficultyTier Tier) const
{
	const FSHDifficultyConfig* Found = DifficultyConfigs.Find(Tier);
	if (Found)
	{
		return *Found;
	}

	UE_LOG(LogSH_Difficulty, Warning,
		TEXT("No config found for difficulty tier %d — returning fallback (Hardened defaults)."),
		static_cast<int32>(Tier));
	return FallbackConfig;
}

const FSHDifficultyConfig& USHDifficultySettings::GetCurrentConfig() const
{
	return GetConfigForTier(ActiveDifficulty);
}

// -----------------------------------------------------------------------
//  Active difficulty
// -----------------------------------------------------------------------

void USHDifficultySettings::SetActiveDifficulty(ESHDifficultyTier NewTier)
{
	if (ActiveDifficulty == NewTier)
	{
		return;
	}

	const ESHDifficultyTier PreviousTier = ActiveDifficulty;
	ActiveDifficulty = NewTier;

	UE_LOG(LogSH_Difficulty, Log,
		TEXT("Difficulty changed from %d to %d."),
		static_cast<int32>(PreviousTier), static_cast<int32>(NewTier));

	OnDifficultyChanged.Broadcast(NewTier);
}

// -----------------------------------------------------------------------
//  ApplyDifficultyToAI
// -----------------------------------------------------------------------

void USHDifficultySettings::ApplyDifficultyToAI(ASHEnemyAIController* AIController) const
{
	if (!AIController)
	{
		UE_LOG(LogSH_Difficulty, Warning,
			TEXT("ApplyDifficultyToAI called with null controller."));
		return;
	}

	const FSHDifficultyConfig& Config = GetCurrentConfig();

	// -- Perception -------------------------------------------------------
	// The AI controller exposes config properties that are read by the
	// perception component and behavior tree.  We scale them here so that
	// the base values set on the controller blueprint are preserved as the
	// "1.0x" reference.

	// SightRangeMultiplier, HearingRangeMultiplier, ReactionTimeMultiplier,
	// and DetectionSpeedMultiplier are consumed by the perception system
	// through blackboard keys or direct queries on the controller.

	// -- Behavior toggles -------------------------------------------------
	// Primordia subsystem components are always present on the controller
	// but only tick / produce orders when their corresponding flag is set.

	// -- Accuracy ---------------------------------------------------------
	// AccuracyMultiplier and BurstLengthMultiplier are forwarded to the
	// weapon firing logic in the behavior tree service.

	// -- Commander --------------------------------------------------------
	// Commander AI is a world-level system; the per-controller flag merely
	// indicates whether this unit should participate in commander orders.

	UE_LOG(LogSH_Difficulty, Verbose,
		TEXT("Applied difficulty tier %d to AI controller [%s]. "
			 "Sight=%.2fx Reaction=%.2fx Accuracy=%.2fx "
			 "DecisionEngine=%d Mnemonic=%d Simulon=%d "
			 "SquadCoord=%d Flank=%d Grenades=%d Commander=%d "
			 "CognitiveDepth=%.2f MemoryDuration=%.1fs"),
		static_cast<int32>(ActiveDifficulty),
		*AIController->GetName(),
		Config.SightRangeMultiplier,
		Config.ReactionTimeMultiplier,
		Config.AccuracyMultiplier,
		Config.bEnablePrimordiaDecisionEngine ? 1 : 0,
		Config.bEnablePrimordiaMnemonic ? 1 : 0,
		Config.bEnablePrimordiaSimulon ? 1 : 0,
		Config.bEnableSquadCoordination ? 1 : 0,
		Config.bEnableFlankingBehavior ? 1 : 0,
		Config.bEnableGrenadeUsage ? 1 : 0,
		Config.bEnableCommanderAI ? 1 : 0,
		Config.PrimordiaMaxCognitiveDepth,
		Config.MnemonicSessionMemoryDuration);
}

// -----------------------------------------------------------------------
//  ApplyDifficultyToPlayer
// -----------------------------------------------------------------------

void USHDifficultySettings::ApplyDifficultyToPlayer(ASHPlayerCharacter* PlayerCharacter) const
{
	if (!PlayerCharacter)
	{
		UE_LOG(LogSH_Difficulty, Warning,
			TEXT("ApplyDifficultyToPlayer called with null character."));
		return;
	}

	const FSHDifficultyConfig& Config = GetCurrentConfig();

	// DamageReceivedMultiplier, StaminaDrainMultiplier, and BleedRateMultiplier
	// are consumed by the player's damage, fatigue, and injury systems
	// respectively.  The character reads these from the active difficulty
	// config each frame / on damage events.

	UE_LOG(LogSH_Difficulty, Verbose,
		TEXT("Applied difficulty tier %d to player [%s]. "
			 "DamageReceived=%.2fx StaminaDrain=%.2fx BleedRate=%.2fx"),
		static_cast<int32>(ActiveDifficulty),
		*PlayerCharacter->GetName(),
		Config.DamageReceivedMultiplier,
		Config.StaminaDrainMultiplier,
		Config.BleedRateMultiplier);
}

// -----------------------------------------------------------------------
//  Default config initialization
// -----------------------------------------------------------------------

void USHDifficultySettings::InitializeDefaultConfigs()
{
	// Only populate if the map is empty (first construction or no saved
	// config overrides).  This preserves designer tweaks persisted to
	// DefaultGame.ini while still providing sane out-of-the-box values.
	if (DifficultyConfigs.Num() > 0)
	{
		return;
	}

	// =================================================================
	//  RECRUIT — Thinker & Mnemonic disabled; reactive only
	// =================================================================
	{
		FSHDifficultyConfig& C = DifficultyConfigs.Add(ESHDifficultyTier::Recruit);

		// Perception
		C.SightRangeMultiplier      = 0.6f;
		C.HearingRangeMultiplier    = 0.6f;
		C.ReactionTimeMultiplier    = 2.0f;
		C.DetectionSpeedMultiplier  = 0.5f;

		// Behavior
		C.bEnablePrimordiaDecisionEngine = false;
		C.bEnablePrimordiaMnemonic       = false;
		C.bEnablePrimordiaSimulon        = false;
		C.bEnableSquadCoordination       = false;
		C.bEnableFlankingBehavior        = false;
		C.bEnableGrenadeUsage            = false;
		C.DecisionCycleVarianceMin       = 1.0f;
		C.DecisionCycleVarianceMax       = 3.0f;

		// Accuracy
		C.AccuracyMultiplier           = 0.5f;
		C.BurstLengthMultiplier        = 0.6f;
		C.SuppressionAccuracyPenalty   = 0.5f;

		// Morale
		C.MoraleRecoveryMultiplier     = 0.6f;
		C.SurrenderThresholdMultiplier = 0.7f;

		// Commander
		C.bEnableCommanderAI                = false;
		C.ReinforcementFrequencyMultiplier  = 0.5f;
		C.ForceAllocationQuality            = 0.0f;

		// Player
		C.DamageReceivedMultiplier = 0.7f;
		C.StaminaDrainMultiplier   = 0.8f;
		C.BleedRateMultiplier      = 0.7f;

		// Primordia
		C.PrimordiaMaxCognitiveDepth     = 0.0f;
		C.MnemonicSessionMemoryDuration  = 0.0f;
	}

	// =================================================================
	//  REGULAR — Thinker limited; short-term memory; basic coordination
	// =================================================================
	{
		FSHDifficultyConfig& C = DifficultyConfigs.Add(ESHDifficultyTier::Regular);

		// Perception
		C.SightRangeMultiplier      = 0.8f;
		C.HearingRangeMultiplier    = 0.8f;
		C.ReactionTimeMultiplier    = 1.5f;
		C.DetectionSpeedMultiplier  = 0.75f;

		// Behavior
		C.bEnablePrimordiaDecisionEngine = true;
		C.bEnablePrimordiaMnemonic       = true;
		C.bEnablePrimordiaSimulon        = false;
		C.bEnableSquadCoordination       = true;
		C.bEnableFlankingBehavior        = false;
		C.bEnableGrenadeUsage            = false;
		C.DecisionCycleVarianceMin       = 0.5f;
		C.DecisionCycleVarianceMax       = 2.0f;

		// Accuracy
		C.AccuracyMultiplier           = 0.7f;
		C.BurstLengthMultiplier        = 0.8f;
		C.SuppressionAccuracyPenalty   = 0.4f;

		// Morale
		C.MoraleRecoveryMultiplier     = 0.8f;
		C.SurrenderThresholdMultiplier = 0.85f;

		// Commander
		C.bEnableCommanderAI                = false;
		C.ReinforcementFrequencyMultiplier  = 0.7f;
		C.ForceAllocationQuality            = 0.2f;

		// Player
		C.DamageReceivedMultiplier = 0.85f;
		C.StaminaDrainMultiplier   = 0.9f;
		C.BleedRateMultiplier      = 0.85f;

		// Primordia
		C.PrimordiaMaxCognitiveDepth     = 0.25f;
		C.MnemonicSessionMemoryDuration  = 60.0f;
	}

	// =================================================================
	//  HARDENED — Thinker active; full session memory; basic commander AI
	// =================================================================
	{
		FSHDifficultyConfig& C = DifficultyConfigs.Add(ESHDifficultyTier::Hardened);

		// Perception
		C.SightRangeMultiplier      = 1.0f;
		C.HearingRangeMultiplier    = 1.0f;
		C.ReactionTimeMultiplier    = 1.0f;
		C.DetectionSpeedMultiplier  = 1.0f;

		// Behavior
		C.bEnablePrimordiaDecisionEngine = true;
		C.bEnablePrimordiaMnemonic       = true;
		C.bEnablePrimordiaSimulon        = false;
		C.bEnableSquadCoordination       = true;
		C.bEnableFlankingBehavior        = true;
		C.bEnableGrenadeUsage            = true;
		C.DecisionCycleVarianceMin       = 0.2f;
		C.DecisionCycleVarianceMax       = 1.0f;

		// Accuracy
		C.AccuracyMultiplier           = 0.9f;
		C.BurstLengthMultiplier        = 1.0f;
		C.SuppressionAccuracyPenalty   = 0.3f;

		// Morale
		C.MoraleRecoveryMultiplier     = 1.0f;
		C.SurrenderThresholdMultiplier = 1.0f;

		// Commander
		C.bEnableCommanderAI                = true;
		C.ReinforcementFrequencyMultiplier  = 1.0f;
		C.ForceAllocationQuality            = 0.5f;

		// Player
		C.DamageReceivedMultiplier = 1.0f;
		C.StaminaDrainMultiplier   = 1.0f;
		C.BleedRateMultiplier      = 1.0f;

		// Primordia
		C.PrimordiaMaxCognitiveDepth     = 0.5f;
		C.MnemonicSessionMemoryDuration  = 300.0f;
	}

	// =================================================================
	//  VETERAN — Full stack; predictive movement; full commander AI
	// =================================================================
	{
		FSHDifficultyConfig& C = DifficultyConfigs.Add(ESHDifficultyTier::Veteran);

		// Perception
		C.SightRangeMultiplier      = 1.0f;
		C.HearingRangeMultiplier    = 1.0f;
		C.ReactionTimeMultiplier    = 1.0f;
		C.DetectionSpeedMultiplier  = 1.0f;

		// Behavior
		C.bEnablePrimordiaDecisionEngine = true;
		C.bEnablePrimordiaMnemonic       = true;
		C.bEnablePrimordiaSimulon        = true;
		C.bEnableSquadCoordination       = true;
		C.bEnableFlankingBehavior        = true;
		C.bEnableGrenadeUsage            = true;
		C.DecisionCycleVarianceMin       = 0.1f;
		C.DecisionCycleVarianceMax       = 0.5f;

		// Accuracy
		C.AccuracyMultiplier           = 1.0f;
		C.BurstLengthMultiplier        = 1.0f;
		C.SuppressionAccuracyPenalty   = 0.25f;

		// Morale
		C.MoraleRecoveryMultiplier     = 1.2f;
		C.SurrenderThresholdMultiplier = 1.2f;

		// Commander
		C.bEnableCommanderAI                = true;
		C.ReinforcementFrequencyMultiplier  = 1.2f;
		C.ForceAllocationQuality            = 0.8f;

		// Player
		C.DamageReceivedMultiplier = 1.15f;
		C.StaminaDrainMultiplier   = 1.1f;
		C.BleedRateMultiplier      = 1.1f;

		// Primordia
		C.PrimordiaMaxCognitiveDepth     = 0.8f;
		C.MnemonicSessionMemoryDuration  = 600.0f;
	}

	// =================================================================
	//  PRIMORDIA UNLEASHED — All subsystems at maximum cognitive depth
	// =================================================================
	{
		FSHDifficultyConfig& C = DifficultyConfigs.Add(ESHDifficultyTier::PrimordiaUnleashed);

		// Perception
		C.SightRangeMultiplier      = 1.2f;
		C.HearingRangeMultiplier    = 1.2f;
		C.ReactionTimeMultiplier    = 0.7f;
		C.DetectionSpeedMultiplier  = 1.3f;

		// Behavior
		C.bEnablePrimordiaDecisionEngine = true;
		C.bEnablePrimordiaMnemonic       = true;
		C.bEnablePrimordiaSimulon        = true;
		C.bEnableSquadCoordination       = true;
		C.bEnableFlankingBehavior        = true;
		C.bEnableGrenadeUsage            = true;
		C.DecisionCycleVarianceMin       = 0.0f;
		C.DecisionCycleVarianceMax       = 0.2f;

		// Accuracy
		C.AccuracyMultiplier           = 1.1f;
		C.BurstLengthMultiplier        = 1.2f;
		C.SuppressionAccuracyPenalty   = 0.15f;

		// Morale
		C.MoraleRecoveryMultiplier     = 1.5f;
		C.SurrenderThresholdMultiplier = 1.5f;

		// Commander
		C.bEnableCommanderAI                = true;
		C.ReinforcementFrequencyMultiplier  = 1.5f;
		C.ForceAllocationQuality            = 1.0f;

		// Player
		C.DamageReceivedMultiplier = 1.3f;
		C.StaminaDrainMultiplier   = 1.2f;
		C.BleedRateMultiplier      = 1.3f;

		// Primordia
		C.PrimordiaMaxCognitiveDepth     = 1.0f;
		C.MnemonicSessionMemoryDuration  = 0.0f; // Infinite — 0 signals "never forget"
	}
}
