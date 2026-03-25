// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHGameState.h"
#include "Net/UnrealNetwork.h"

ASHGameState::ASHGameState()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f; // 10 Hz is sufficient for state decay

	// Initialize defensive lines.
	DefensiveLines.SetNum(3);
	DefensiveLines[0].Line = ESHDefensiveLine::Primary;
	DefensiveLines[0].Integrity = 1.f;
	DefensiveLines[0].DefenderCount = 60;
	DefensiveLines[0].FortificationCount = 24;

	DefensiveLines[1].Line = ESHDefensiveLine::Secondary;
	DefensiveLines[1].Integrity = 1.f;
	DefensiveLines[1].DefenderCount = 36;
	DefensiveLines[1].FortificationCount = 16;

	DefensiveLines[2].Line = ESHDefensiveLine::Final;
	DefensiveLines[2].Integrity = 1.f;
	DefensiveLines[2].DefenderCount = 24;
	DefensiveLines[2].FortificationCount = 12;

	// Initial friendly force composition.
	FriendlyForces.InfantryCount = 120;
	FriendlyForces.ArmorCount = 4;
}

void ASHGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASHGameState, MissionPhase);
	DOREPLIFETIME(ASHGameState, ActiveObjectives);
	DOREPLIFETIME(ASHGameState, FriendlyForces);
	DOREPLIFETIME(ASHGameState, EnemyForces);
	DOREPLIFETIME(ASHGameState, DefensiveLines);
	DOREPLIFETIME(ASHGameState, BattleIntensity);
	DOREPLIFETIME(ASHGameState, GlobalSuppressionModifier);
	DOREPLIFETIME(ASHGameState, GlobalMoraleModifier);
	DOREPLIFETIME(ASHGameState, AIAggressionModifier);
	DOREPLIFETIME(ASHGameState, Environment);
}

// =======================================================================
//  Phase / objectives
// =======================================================================

void ASHGameState::SetMissionPhase(ESHMissionPhase Phase)
{
	MissionPhase = Phase;
}

void ASHGameState::SetActiveObjectives(const TArray<FSHDynamicObjective>& Objectives)
{
	ActiveObjectives = Objectives;
}

// =======================================================================
//  Force disposition
// =======================================================================

float ASHGameState::GetEnemyForceStrengthNormalized() const
{
	if (ExpectedPeakEnemyStrength <= 0)
	{
		return 0.f;
	}
	return FMath::Clamp(
		static_cast<float>(EnemyForces.GetActiveStrength()) / static_cast<float>(ExpectedPeakEnemyStrength),
		0.f, 1.f);
}

float ASHGameState::GetFriendlyCasualtyRate() const
{
	if (InitialFriendlyCount <= 0)
	{
		return 0.f;
	}
	return FMath::Clamp(
		static_cast<float>(FriendlyForces.Casualties) / static_cast<float>(InitialFriendlyCount),
		0.f, 1.f);
}

void ASHGameState::AddEnemyForces(int32 Infantry, int32 Armor, int32 Amphibious)
{
	EnemyForces.InfantryCount += Infantry;
	EnemyForces.ArmorCount += Armor;
	EnemyForces.AmphibiousCount += Amphibious;
}

void ASHGameState::AddFriendlyCasualties(int32 Count)
{
	FriendlyForces.Casualties += Count;

	// Casualties reduce morale.
	const float CasualtyRate = GetFriendlyCasualtyRate();
	GlobalMoraleModifier = FMath::Clamp(1.f - (CasualtyRate * 0.8f), 0.2f, 1.f);
}

// =======================================================================
//  Defensive lines
// =======================================================================

float ASHGameState::GetDefensiveLineIntegrity(ESHDefensiveLine Line) const
{
	for (const FSHDefensiveLineStatus& Status : DefensiveLines)
	{
		if (Status.Line == Line)
		{
			return Status.Integrity;
		}
	}
	return 0.f;
}

void ASHGameState::SetDefensiveLineIntegrity(ESHDefensiveLine Line, float Integrity)
{
	for (FSHDefensiveLineStatus& Status : DefensiveLines)
	{
		if (Status.Line == Line)
		{
			Status.Integrity = FMath::Clamp(Integrity, 0.f, 1.f);
			return;
		}
	}
}

void ASHGameState::DamageDefensiveLine(ESHDefensiveLine Line, float Damage)
{
	for (FSHDefensiveLineStatus& Status : DefensiveLines)
	{
		if (Status.Line == Line)
		{
			Status.Integrity = FMath::Clamp(Status.Integrity - Damage, 0.f, 1.f);

			// Fortifications absorb some damage.
			if (Status.FortificationCount > 0 && Damage > 0.05f)
			{
				Status.FortificationCount = FMath::Max(0, Status.FortificationCount - 1);
			}

			// Increase global suppression when lines are being hit.
			GlobalSuppressionModifier = FMath::Clamp(GlobalSuppressionModifier + Damage * 0.5f, 0.f, 1.f);
			return;
		}
	}
}

// =======================================================================
//  Battle intensity & modifiers
// =======================================================================

void ASHGameState::AddBattleIntensity(float Delta)
{
	BattleIntensity = FMath::Clamp(BattleIntensity + Delta, 0.f, 1.f);
}

void ASHGameState::SetBattleIntensity(float Value)
{
	BattleIntensity = FMath::Clamp(Value, 0.f, 1.f);
}

void ASHGameState::SetGlobalSuppressionModifier(float Value)
{
	GlobalSuppressionModifier = FMath::Clamp(Value, 0.f, 1.f);
}

void ASHGameState::SetGlobalMoraleModifier(float Value)
{
	GlobalMoraleModifier = FMath::Clamp(Value, 0.f, 1.f);
}

void ASHGameState::SetAIAggressionModifier(float Value)
{
	AIAggressionModifier = FMath::Clamp(Value, 0.f, 1.f);
}

// =======================================================================
//  Environment
// =======================================================================

void ASHGameState::SetTimeOfDay(float Hours)
{
	Environment.TimeOfDayHours = FMath::Fmod(Hours, 24.f);
}

void ASHGameState::SetVisibility(float InVisibility)
{
	Environment.Visibility = FMath::Clamp(InVisibility, 0.f, 1.f);
}

void ASHGameState::SetWeather(ESHWeatherCondition InWeather)
{
	Environment.Weather = InWeather;

	// Weather affects visibility.
	switch (InWeather)
	{
	case ESHWeatherCondition::Clear:
	case ESHWeatherCondition::Overcast:
		break; // no additional penalty
	case ESHWeatherCondition::Rain:
		Environment.Visibility = FMath::Min(Environment.Visibility, 0.7f);
		break;
	case ESHWeatherCondition::HeavyRain:
		Environment.Visibility = FMath::Min(Environment.Visibility, 0.4f);
		break;
	case ESHWeatherCondition::Fog:
		Environment.Visibility = FMath::Min(Environment.Visibility, 0.25f);
		break;
	case ESHWeatherCondition::Typhoon:
		Environment.Visibility = FMath::Min(Environment.Visibility, 0.15f);
		Environment.WindSpeed = FMath::Max(Environment.WindSpeed, 30.f);
		break;
	}
}

void ASHGameState::SetWind(float Speed, float DirectionDeg)
{
	Environment.WindSpeed = FMath::Max(Speed, 0.f);
	Environment.WindDirectionDeg = FMath::Fmod(DirectionDeg, 360.f);
}

// =======================================================================
//  Tick
// =======================================================================

void ASHGameState::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (HasAuthority())
	{
		DecayBattleIntensity(DeltaSeconds);

		// Decay suppression over time.
		if (GlobalSuppressionModifier > 0.f)
		{
			GlobalSuppressionModifier = FMath::Max(0.f, GlobalSuppressionModifier - 0.03f * DeltaSeconds);
		}
	}
}

void ASHGameState::DecayBattleIntensity(float DeltaSeconds)
{
	const float Baseline = (MissionPhase == ESHMissionPhase::BeachAssault ||
	                         MissionPhase == ESHMissionPhase::Counterattack)
		? MinAssaultIntensity
		: 0.f;

	if (BattleIntensity > Baseline)
	{
		BattleIntensity = FMath::Max(Baseline, BattleIntensity - BattleIntensityDecayRate * DeltaSeconds);
	}
}
