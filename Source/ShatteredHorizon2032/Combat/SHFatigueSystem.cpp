// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHFatigueSystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

// =====================================================================
//  Constructor
// =====================================================================

USHFatigueSystem::USHFatigueSystem()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
}

// =====================================================================
//  UActorComponent overrides
// =====================================================================

void USHFatigueSystem::BeginPlay()
{
	Super::BeginPlay();

	CurrentStamina = MaxStamina;
	CurrentFatigueLevel = ESHFatigueLevel::Fresh;
	InitDefaultDrainRates();
}

void USHFatigueSystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (ActiveDrains.Num() > 0 || bIsHoldingBreath)
	{
		TickStaminaDrain(DeltaTime);
		TickLongTermFatigue(DeltaTime);
	}
	else
	{
		TickStaminaRecovery(DeltaTime);
	}

	UpdateFatigueLevel();
}

// =====================================================================
//  Default configuration
// =====================================================================

void USHFatigueSystem::InitDefaultDrainRates()
{
	// Continuous drain rates (per second).
	if (DrainRates.Num() == 0)
	{
		DrainRates.Add(ESHStaminaDrainType::Sprint, 12.f);
		DrainRates.Add(ESHStaminaDrainType::HoldBreath, 15.f);
		DrainRates.Add(ESHStaminaDrainType::Swim, 18.f);
		DrainRates.Add(ESHStaminaDrainType::Climb, 14.f);
	}

	// One-shot burst costs.
	if (BurstCosts.Num() == 0)
	{
		BurstCosts.Add(ESHStaminaDrainType::Vault, 15.f);
		BurstCosts.Add(ESHStaminaDrainType::Melee, 20.f);
	}

	// Minimum stamina to initiate activity.
	if (MinimumStaminaRequired.Num() == 0)
	{
		MinimumStaminaRequired.Add(ESHStaminaDrainType::Sprint, 10.f);
		MinimumStaminaRequired.Add(ESHStaminaDrainType::Vault, 15.f);
		MinimumStaminaRequired.Add(ESHStaminaDrainType::Melee, 20.f);
		MinimumStaminaRequired.Add(ESHStaminaDrainType::HoldBreath, 5.f);
		MinimumStaminaRequired.Add(ESHStaminaDrainType::Swim, 10.f);
		MinimumStaminaRequired.Add(ESHStaminaDrainType::Climb, 10.f);
	}
}

// =====================================================================
//  Stamina management
// =====================================================================

void USHFatigueSystem::BeginStaminaDrain(ESHStaminaDrainType Activity)
{
	if (!CanPerformActivity(Activity))
	{
		return;
	}
	ActiveDrains.Add(Activity);
}

void USHFatigueSystem::EndStaminaDrain(ESHStaminaDrainType Activity)
{
	ActiveDrains.Remove(Activity);
}

void USHFatigueSystem::ConsumeStamina(float Amount)
{
	// Weight increases stamina cost.
	const float WeightMultiplier = (CarriedWeightKg > WeightPenaltyThreshold)
		? 1.f + (CarriedWeightKg - WeightPenaltyThreshold) * 0.02f
		: 1.f;

	// Injury compounds costs.
	const float InjuryMultiplier = 1.f + InjuryPenalty * 0.5f;

	const float FinalCost = Amount * WeightMultiplier * InjuryMultiplier;

	CurrentStamina = FMath::Max(CurrentStamina - FinalCost, 0.f);

	if (CurrentStamina <= 0.f && !bWasDepleted)
	{
		bWasDepleted = true;
		TimeSinceDepleted = 0.f;
		OnStaminaDepleted.Broadcast(GetOwner());
	}
}

bool USHFatigueSystem::CanPerformActivity(ESHStaminaDrainType Activity) const
{
	const float* MinRequired = MinimumStaminaRequired.Find(Activity);
	if (MinRequired)
	{
		return CurrentStamina >= *MinRequired;
	}
	return CurrentStamina > 0.f;
}

// =====================================================================
//  Breath control
// =====================================================================

void USHFatigueSystem::BeginHoldBreath()
{
	if (bIsHoldingBreath || !CanPerformActivity(ESHStaminaDrainType::HoldBreath))
	{
		return;
	}
	bIsHoldingBreath = true;
	BreathHoldElapsed = 0.f;
}

void USHFatigueSystem::EndHoldBreath()
{
	if (!bIsHoldingBreath)
	{
		return;
	}
	bIsHoldingBreath = false;
	OnBreathHoldReleased.Broadcast(CurrentStamina);
}

// =====================================================================
//  Injury integration
// =====================================================================

void USHFatigueSystem::NotifyInjury(float Severity)
{
	InjuryPenalty = FMath::Clamp(InjuryPenalty + Severity, 0.f, 1.f);
}

void USHFatigueSystem::ClearInjuryPenalty()
{
	InjuryPenalty = 0.f;
}

// =====================================================================
//  Carried weight
// =====================================================================

void USHFatigueSystem::SetCarriedWeight(float WeightKg)
{
	CarriedWeightKg = FMath::Max(WeightKg, 0.f);
}

// =====================================================================
//  Tick helpers
// =====================================================================

void USHFatigueSystem::TickStaminaDrain(float DeltaTime)
{
	float TotalDrain = 0.f;

	for (const ESHStaminaDrainType Activity : ActiveDrains)
	{
		const float* Rate = DrainRates.Find(Activity);
		if (Rate)
		{
			TotalDrain += *Rate;
		}
	}

	// Hold breath drain.
	if (bIsHoldingBreath)
	{
		const float* BreathRate = DrainRates.Find(ESHStaminaDrainType::HoldBreath);
		if (BreathRate)
		{
			TotalDrain += *BreathRate;
		}
		BreathHoldElapsed += DeltaTime;

		// Forced release after max duration.
		if (BreathHoldElapsed >= MaxHoldBreathDuration)
		{
			EndHoldBreath();
		}
	}

	if (TotalDrain > 0.f)
	{
		ConsumeStamina(TotalDrain * DeltaTime);
	}

	// Force-end activities that require minimum stamina.
	if (CurrentStamina <= 0.f)
	{
		if (bIsHoldingBreath)
		{
			EndHoldBreath();
		}

		// End sprint if we can't sustain it.
		if (ActiveDrains.Contains(ESHStaminaDrainType::Sprint))
		{
			EndStaminaDrain(ESHStaminaDrainType::Sprint);
		}
	}
}

void USHFatigueSystem::TickStaminaRecovery(float DeltaTime)
{
	if (CurrentStamina >= MaxStamina)
	{
		bWasDepleted = false;
		return;
	}

	// Delay recovery briefly after full depletion.
	if (bWasDepleted)
	{
		TimeSinceDepleted += DeltaTime;
		if (TimeSinceDepleted < 2.f)
		{
			return;
		}
		bWasDepleted = false;
	}

	const float Rate = ComputeRecoveryRate();
	CurrentStamina = FMath::Min(CurrentStamina + Rate * DeltaTime, MaxStamina);
}

void USHFatigueSystem::TickLongTermFatigue(float DeltaTime)
{
	// Long-term fatigue only accumulates during physical exertion.
	if (ActiveDrains.Num() == 0 && !bIsHoldingBreath)
	{
		return;
	}

	// Injury makes fatigue accumulate faster.
	const float InjuryMultiplier = 1.f + InjuryPenalty;
	LongTermFatigue = FMath::Clamp(
		LongTermFatigue + LongTermFatigueRate * InjuryMultiplier * DeltaTime,
		0.f, 1.f);
}

float USHFatigueSystem::ComputeRecoveryRate() const
{
	float Rate = BaseRecoveryRate;

	// Long-term fatigue reduces recovery.
	Rate *= (1.f - LongTermFatigue * 0.6f);

	// Weight penalty.
	if (CarriedWeightKg > WeightPenaltyThreshold)
	{
		const float OverWeight = CarriedWeightKg - WeightPenaltyThreshold;
		Rate -= OverWeight * RecoveryPenaltyPerKg;
	}

	// Injury reduces recovery.
	Rate *= (1.f - InjuryPenalty * 0.4f);

	return FMath::Max(Rate, 0.5f); // Always recover at least a trickle.
}

void USHFatigueSystem::UpdateFatigueLevel()
{
	// Composite fatigue score from stamina percentage and long-term fatigue.
	const float StaminaFactor = 1.f - GetStaminaPercent(); // 0 = full, 1 = empty
	const float CompositeFatigue = FMath::Clamp(
		StaminaFactor * 0.6f + LongTermFatigue * 0.4f + InjuryPenalty * 0.3f,
		0.f, 1.f);

	ESHFatigueLevel NewLevel;
	if (CompositeFatigue >= 0.9f)
		NewLevel = ESHFatigueLevel::Collapsed;
	else if (CompositeFatigue >= 0.7f)
		NewLevel = ESHFatigueLevel::Exhausted;
	else if (CompositeFatigue >= 0.45f)
		NewLevel = ESHFatigueLevel::Tired;
	else if (CompositeFatigue >= 0.2f)
		NewLevel = ESHFatigueLevel::Winded;
	else
		NewLevel = ESHFatigueLevel::Fresh;

	if (NewLevel != CurrentFatigueLevel)
	{
		const ESHFatigueLevel OldLevel = CurrentFatigueLevel;
		CurrentFatigueLevel = NewLevel;
		OnFatigueLevelChanged.Broadcast(OldLevel, NewLevel);
	}
}

// =====================================================================
//  Queries (effect multipliers)
// =====================================================================

float USHFatigueSystem::GetWeaponSwayMultiplier() const
{
	// Fresh = 1.0, Collapsed = 3.5.
	const float StaminaFactor = 1.f - GetStaminaPercent();
	const float Sway = 1.f + StaminaFactor * 1.5f + LongTermFatigue * 1.0f + InjuryPenalty * 0.5f;

	// When holding breath, reduce sway (the whole point of holding breath).
	if (bIsHoldingBreath)
	{
		// Effectiveness degrades as hold time increases.
		const float HoldEfficiency = 1.f - FMath::Clamp(BreathHoldElapsed / MaxHoldBreathDuration, 0.f, 1.f);
		return FMath::Lerp(Sway, 0.2f, HoldEfficiency * 0.8f);
	}

	return Sway;
}

float USHFatigueSystem::GetSprintSpeedMultiplier() const
{
	// Fresh = 1.0, gradually slows.
	const float StaminaFactor = GetStaminaPercent();
	const float FatigueReduction = LongTermFatigue * 0.3f + InjuryPenalty * 0.2f;

	float Multiplier;
	if (StaminaFactor > 0.5f)
	{
		Multiplier = 1.f;
	}
	else
	{
		// Below 50% stamina, speed drops linearly to 0.6.
		Multiplier = FMath::Lerp(0.6f, 1.f, StaminaFactor / 0.5f);
	}

	return FMath::Max(Multiplier - FatigueReduction, 0.3f);
}

float USHFatigueSystem::GetReloadSpeedMultiplier() const
{
	// Fresh = 1.0, Exhausted/Collapsed = as low as 0.5.
	const float StaminaFactor = 1.f - GetStaminaPercent();
	const float Penalty = StaminaFactor * 0.2f + LongTermFatigue * 0.15f + InjuryPenalty * 0.15f;
	return FMath::Max(1.f - Penalty, 0.5f);
}
