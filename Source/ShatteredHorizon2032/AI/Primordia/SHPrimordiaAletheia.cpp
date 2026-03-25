// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHPrimordiaAletheia.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogSH_Aletheia, Log, All);

// -----------------------------------------------------------------------
//  Constructor
// -----------------------------------------------------------------------

USHPrimordiaAletheia::USHPrimordiaAletheia()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// -----------------------------------------------------------------------
//  UActorComponent overrides
// -----------------------------------------------------------------------

void USHPrimordiaAletheia::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogSH_Aletheia, Verbose, TEXT("Aletheia decision validator initialized on %s."),
		*GetOwner()->GetName());
}

// -----------------------------------------------------------------------
//  Validation
// -----------------------------------------------------------------------

bool USHPrimordiaAletheia::ValidateDecision(const FSHAIDecision& Decision, FSHAIDecisionValidation& OutValidation)
{
	TotalValidations++;
	OutValidation.bValid = true;
	OutValidation.RejectionReason.Empty();

	FString Reason;

	// Run through all validation rules — first failure rejects
	if (!CheckConfidence(Decision, Reason))
	{
		OutValidation.bValid = false;
		OutValidation.RejectionReason = Reason;
	}
	else if (!CheckKillZone(Decision, Reason))
	{
		OutValidation.bValid = false;
		OutValidation.RejectionReason = Reason;
	}
	else if (!CheckMorale(Decision, Reason))
	{
		OutValidation.bValid = false;
		OutValidation.RejectionReason = Reason;
	}
	else if (!CheckFlankingRetreat(Decision, Reason))
	{
		OutValidation.bValid = false;
		OutValidation.RejectionReason = Reason;
	}
	else if (!CheckSuppressionAdvance(Decision, Reason))
	{
		OutValidation.bValid = false;
		OutValidation.RejectionReason = Reason;
	}
	else if (!CheckRepetitivePattern(Decision, Reason))
	{
		OutValidation.bValid = false;
		OutValidation.RejectionReason = Reason;
	}

	// Record in history
	DecisionHistory.Add(Decision);
	if (DecisionHistory.Num() > MaxHistorySize)
	{
		DecisionHistory.RemoveAt(0, DecisionHistory.Num() - MaxHistorySize);
	}

	if (!OutValidation.bValid)
	{
		TotalRejections++;
		UE_LOG(LogSH_Aletheia, Log, TEXT("[%s] Decision REJECTED (%s): %s"),
			*GetOwner()->GetName(),
			*UEnum::GetValueAsString(Decision.DecisionType),
			*OutValidation.RejectionReason);
	}

	return OutValidation.bValid;
}

bool USHPrimordiaAletheia::IsDecisionValid(const FSHAIDecision& Decision)
{
	FSHAIDecisionValidation Validation;
	return ValidateDecision(Decision, Validation);
}

// -----------------------------------------------------------------------
//  Context setters
// -----------------------------------------------------------------------

void USHPrimordiaAletheia::ReportKillZone(FVector Location, float Radius)
{
	FKillZone Zone;
	Zone.Location = Location;
	Zone.Radius = Radius;
	Zone.ReportedTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	KnownKillZones.Add(Zone);

	// Cap kill zone memory to prevent unbounded growth
	if (KnownKillZones.Num() > 50)
	{
		KnownKillZones.RemoveAt(0);
	}
}

void USHPrimordiaAletheia::SetCurrentMorale(float Morale)
{
	CurrentMorale = FMath::Clamp(Morale, 0.0f, 1.0f);
}

void USHPrimordiaAletheia::SetFlankingAdvantage(bool bHasAdvantage)
{
	bHasFlankingAdvantage = bHasAdvantage;
}

void USHPrimordiaAletheia::SetNearbyFriendlyCount(int32 Count)
{
	NearbyFriendlyCount = FMath::Max(Count, 0);
}

void USHPrimordiaAletheia::SetSuppressed(bool bSuppressed)
{
	bIsSuppressed = bSuppressed;
}

// -----------------------------------------------------------------------
//  Diagnostics
// -----------------------------------------------------------------------

float USHPrimordiaAletheia::GetRejectionRate() const
{
	if (TotalValidations <= 0)
	{
		return 0.0f;
	}
	return static_cast<float>(TotalRejections) / static_cast<float>(TotalValidations);
}

// -----------------------------------------------------------------------
//  Validation rules
// -----------------------------------------------------------------------

bool USHPrimordiaAletheia::CheckKillZone(const FSHAIDecision& Decision, FString& OutReason) const
{
	// Don't advance or attack into a confirmed kill zone
	if (Decision.DecisionType != ESHAIDecisionType::Advance &&
		Decision.DecisionType != ESHAIDecisionType::Attack &&
		Decision.DecisionType != ESHAIDecisionType::Flank)
	{
		return true;
	}

	for (const FKillZone& Zone : KnownKillZones)
	{
		const float Distance = FVector::Dist(Decision.TargetLocation, Zone.Location);
		if (Distance < Zone.Radius + KillZoneSafetyMargin)
		{
			OutReason = FString::Printf(
				TEXT("Target location is within confirmed kill zone (dist=%.0f, zone_radius=%.0f)."),
				Distance, Zone.Radius);
			return false;
		}
	}
	return true;
}

bool USHPrimordiaAletheia::CheckMorale(const FSHAIDecision& Decision, FString& OutReason) const
{
	// Don't attack with broken morale
	if (Decision.DecisionType == ESHAIDecisionType::Attack ||
		Decision.DecisionType == ESHAIDecisionType::Advance ||
		Decision.DecisionType == ESHAIDecisionType::Flank)
	{
		if (CurrentMorale < MoraleAttackThreshold)
		{
			OutReason = FString::Printf(
				TEXT("Morale too low for offensive action (morale=%.2f, threshold=%.2f)."),
				CurrentMorale, MoraleAttackThreshold);
			return false;
		}
	}
	return true;
}

bool USHPrimordiaAletheia::CheckFlankingRetreat(const FSHAIDecision& Decision, FString& OutReason) const
{
	// Don't retreat when a flanking advantage exists
	if (Decision.DecisionType == ESHAIDecisionType::Retreat && bHasFlankingAdvantage)
	{
		OutReason = TEXT("Retreat rejected — flanking advantage detected. Exploit position instead.");
		return false;
	}
	return true;
}

bool USHPrimordiaAletheia::CheckConfidence(const FSHAIDecision& Decision, FString& OutReason) const
{
	if (Decision.Confidence < MinConfidenceThreshold)
	{
		OutReason = FString::Printf(
			TEXT("Decision confidence too low (confidence=%.2f, min=%.2f)."),
			Decision.Confidence, MinConfidenceThreshold);
		return false;
	}
	return true;
}

bool USHPrimordiaAletheia::CheckRepetitivePattern(const FSHAIDecision& Decision, FString& OutReason) const
{
	// Detect repetitive decision patterns that indicate the AI is stuck
	if (DecisionHistory.Num() < RepetitionThreshold)
	{
		return true;
	}

	int32 ConsecutiveCount = 0;
	for (int32 i = DecisionHistory.Num() - 1; i >= 0 && ConsecutiveCount < RepetitionThreshold; --i)
	{
		if (DecisionHistory[i].DecisionType == Decision.DecisionType)
		{
			// Check if target locations are similar (within 200cm)
			if (FVector::Dist(DecisionHistory[i].TargetLocation, Decision.TargetLocation) < 200.0f)
			{
				ConsecutiveCount++;
			}
			else
			{
				break;
			}
		}
		else
		{
			break;
		}
	}

	if (ConsecutiveCount >= RepetitionThreshold)
	{
		OutReason = FString::Printf(
			TEXT("Repetitive decision pattern detected (%d identical consecutive decisions). Try an alternative approach."),
			ConsecutiveCount);
		return false;
	}
	return true;
}

bool USHPrimordiaAletheia::CheckSuppressionAdvance(const FSHAIDecision& Decision, FString& OutReason) const
{
	// Don't advance or attack while actively suppressed (unless flanking)
	if (bIsSuppressed &&
		(Decision.DecisionType == ESHAIDecisionType::Advance ||
		 Decision.DecisionType == ESHAIDecisionType::Attack))
	{
		OutReason = TEXT("Cannot advance while under active suppression. Seek cover or suppress first.");
		return false;
	}
	return true;
}
