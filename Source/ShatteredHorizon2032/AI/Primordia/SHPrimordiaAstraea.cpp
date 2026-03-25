// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHPrimordiaAstraea.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogSH_Astraea, Log, All);

// -----------------------------------------------------------------------
//  Constructor
// -----------------------------------------------------------------------

USHPrimordiaAstraea::USHPrimordiaAstraea()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f; // 10 Hz
}

// -----------------------------------------------------------------------
//  UActorComponent overrides
// -----------------------------------------------------------------------

void USHPrimordiaAstraea::BeginPlay()
{
	Super::BeginPlay();

	// Initialize cognitive state
	CognitiveState.StressLevel = 0.0f;
	CognitiveState.DecisionQuality = 1.0f;
	CognitiveState.Alertness = 0.8f;
	CognitiveState.CombatEffectiveness = 1.0f;

	UE_LOG(LogSH_Astraea, Verbose, TEXT("Astraea cognitive state component initialized on %s."),
		*GetOwner()->GetName());
}

void USHPrimordiaAstraea::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Track time since last fire
	if (bUnderFire)
	{
		TimeSinceLastFire = 0.0f;
	}
	else
	{
		TimeSinceLastFire += DeltaTime;
	}

	// Alertness slowly decays over time (fatigue) but is clamped
	CognitiveState.Alertness = FMath::Max(0.1f,
		CognitiveState.Alertness - AlertnessDecayRate * DeltaTime);

	TickStressRecovery(DeltaTime);
	UpdateDerivedState();
}

// -----------------------------------------------------------------------
//  Stress input
// -----------------------------------------------------------------------

void USHPrimordiaAstraea::ApplyStressor(float Amount, FName Source)
{
	const float ClampedAmount = FMath::Max(Amount, 0.0f);
	CognitiveState.StressLevel = FMath::Min(1.0f, CognitiveState.StressLevel + ClampedAmount);

	// Adrenaline spike from stress — temporarily boosts alertness
	CognitiveState.Alertness = FMath::Min(1.0f,
		CognitiveState.Alertness + StressorAlertnessBoost);

	// Track cumulative stress per source
	float& Cumulative = StressorHistory.FindOrAdd(Source);
	Cumulative += ClampedAmount;

	UpdateDerivedState();

	UE_LOG(LogSH_Astraea, Verbose,
		TEXT("[%s] Stressor '%s' applied (amount=%.3f, total_stress=%.3f)."),
		*GetOwner()->GetName(), *Source.ToString(),
		ClampedAmount, CognitiveState.StressLevel);

	OnCognitiveStateChanged.Broadcast(GetOwner(), CognitiveState);
}

// -----------------------------------------------------------------------
//  Recovery context
// -----------------------------------------------------------------------

void USHPrimordiaAstraea::SetInCover(bool bCover)
{
	bInCover = bCover;
}

void USHPrimordiaAstraea::SetSquadProximityCount(int32 Count)
{
	SquadProximityCount = FMath::Max(Count, 0);
}

void USHPrimordiaAstraea::SetOfficerPresent(bool bPresent)
{
	bOfficerPresent = bPresent;
}

void USHPrimordiaAstraea::SetUnderFire(bool bFire)
{
	bUnderFire = bFire;
	if (bFire)
	{
		TimeSinceLastFire = 0.0f;
	}
}

// -----------------------------------------------------------------------
//  Internal
// -----------------------------------------------------------------------

void USHPrimordiaAstraea::TickStressRecovery(float DeltaTime)
{
	float RecoveryRate = BaseRecoveryRate;

	// Cover bonus
	if (bInCover)
	{
		RecoveryRate += CoverRecoveryBonus;
	}

	// Squad proximity bonus
	RecoveryRate += SquadProximityCount * SquadProximityRecoveryPerMember;

	// Officer presence bonus
	if (bOfficerPresent)
	{
		RecoveryRate += OfficerRecoveryBonus;
	}

	// No-contact bonus (when not under fire for a sustained period)
	if (TimeSinceLastFire >= NoContactThreshold)
	{
		RecoveryRate += NoContactRecoveryBonus;
	}

	// Recovery is halved while under active fire
	if (bUnderFire)
	{
		RecoveryRate *= 0.5f;
	}

	CognitiveState.StressLevel = FMath::Max(0.0f,
		CognitiveState.StressLevel - RecoveryRate * DeltaTime);
}

void USHPrimordiaAstraea::UpdateDerivedState()
{
	// Map stress to decision quality
	if (CognitiveState.StressLevel <= StressQualityDegradationStart)
	{
		CognitiveState.DecisionQuality = 1.0f;
	}
	else
	{
		// Linear degradation from 1.0 to MinDecisionQuality as stress goes from threshold to 1.0
		const float StressRange = 1.0f - StressQualityDegradationStart;
		const float StressProgress = (CognitiveState.StressLevel - StressQualityDegradationStart) / StressRange;
		CognitiveState.DecisionQuality = FMath::Lerp(1.0f, MinDecisionQuality, StressProgress);
	}

	// Combat effectiveness is a blend of all factors
	CognitiveState.CombatEffectiveness =
		CognitiveState.DecisionQuality * 0.4f +
		CognitiveState.Alertness * 0.3f +
		(1.0f - CognitiveState.StressLevel) * 0.3f;

	CognitiveState.CombatEffectiveness = FMath::Clamp(CognitiveState.CombatEffectiveness, 0.0f, 1.0f);
}
