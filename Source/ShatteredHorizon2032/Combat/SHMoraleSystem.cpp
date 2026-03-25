// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHMoraleSystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Engine/World.h"

// =====================================================================
//  Constructor
// =====================================================================

USHMoraleSystem::USHMoraleSystem()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PrePhysics;
	PrimaryComponentTick.TickInterval = 0.25f; // Morale doesn't need per-frame ticks.
}

// =====================================================================
//  UActorComponent overrides
// =====================================================================

void USHMoraleSystem::BeginPlay()
{
	Super::BeginPlay();

	MoraleValue = InitialMorale;
	CurrentState = ComputeState(MoraleValue);
	InitDefaultEventMagnitudes();
}

void USHMoraleSystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TickPassiveModifiers(DeltaTime);
	UpdateMoraleState();
}

// =====================================================================
//  Default configuration
// =====================================================================

void USHMoraleSystem::InitDefaultEventMagnitudes()
{
	if (EventMagnitudes.Num() > 0)
	{
		return;
	}

	// Positive events (add to morale).
	EventMagnitudes.Add(ESHMoraleEvent::NearbyFriendlies, 0.02f);
	EventMagnitudes.Add(ESHMoraleEvent::SuccessfulEngagement, 0.08f);
	EventMagnitudes.Add(ESHMoraleEvent::OfficerNearby, 0.03f);
	EventMagnitudes.Add(ESHMoraleEvent::InCover, 0.01f);
	EventMagnitudes.Add(ESHMoraleEvent::Rallied, 0.25f);
	EventMagnitudes.Add(ESHMoraleEvent::ReachedSafePosition, 0.12f);
	EventMagnitudes.Add(ESHMoraleEvent::MedicTreatment, 0.10f);
	EventMagnitudes.Add(ESHMoraleEvent::EnemyKilled, 0.05f);

	// Negative events (subtract from morale).
	EventMagnitudes.Add(ESHMoraleEvent::FriendlyCasualty, -0.12f);
	EventMagnitudes.Add(ESHMoraleEvent::UnderHeavyFire, -0.06f);
	EventMagnitudes.Add(ESHMoraleEvent::Isolated, -0.04f);
	EventMagnitudes.Add(ESHMoraleEvent::Wounded, -0.10f);
	EventMagnitudes.Add(ESHMoraleEvent::LowAmmo, -0.05f);
	EventMagnitudes.Add(ESHMoraleEvent::AllyBroken, -0.08f);
	EventMagnitudes.Add(ESHMoraleEvent::OfficerDown, -0.15f);
	EventMagnitudes.Add(ESHMoraleEvent::SquadmateDown, -0.10f);
	EventMagnitudes.Add(ESHMoraleEvent::Flanked, -0.12f);
}

// =====================================================================
//  Event reporting
// =====================================================================

void USHMoraleSystem::ReportMoraleEvent(ESHMoraleEvent Event)
{
	const float* Magnitude = EventMagnitudes.Find(Event);
	if (Magnitude)
	{
		AdjustMorale(*Magnitude);
	}
}

void USHMoraleSystem::AdjustMorale(float Delta)
{
	MoraleValue = FMath::Clamp(MoraleValue + Delta, 0.f, 1.f);
}

// =====================================================================
//  Recovery / rally
// =====================================================================

bool USHMoraleSystem::AttemptRally(AActor* RallySource)
{
	if (!IsValid(RallySource))
	{
		return false;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Check cooldown.
	const float WorldTime = World->GetTimeSeconds();
	if (WorldTime - LastRallyTime < RallyCooldown)
	{
		return false;
	}

	// Rally is only meaningful if the character is Shaken or worse.
	if (CurrentState == ESHMoraleState::Determined || CurrentState == ESHMoraleState::Steady)
	{
		return false;
	}

	// Routed units are too far gone to rally easily; require them to be at
	// least Breaking (or a rally can bring Breaking back but not Routed).
	if (CurrentState == ESHMoraleState::Routed)
	{
		// Routed can only be rallied by an officer, with reduced effect.
		AdjustMorale(RallyRestorationAmount * 0.5f);
	}
	else
	{
		AdjustMorale(RallyRestorationAmount);
	}

	LastRallyTime = WorldTime;
	bBrokenNotificationSent = false;
	OnMoraleRallied.Broadcast(GetOwner());
	ReportMoraleEvent(ESHMoraleEvent::Rallied);

	return true;
}

void USHMoraleSystem::NotifySafePosition()
{
	ReportMoraleEvent(ESHMoraleEvent::ReachedSafePosition);
}

void USHMoraleSystem::NotifyMedicTreatment()
{
	ReportMoraleEvent(ESHMoraleEvent::MedicTreatment);
}

// =====================================================================
//  Squad integration
// =====================================================================

void USHMoraleSystem::NotifySquadmateBroken(AActor* BrokenSquadmate)
{
	AdjustMorale(-CascadePenalty);
	ReportMoraleEvent(ESHMoraleEvent::AllyBroken);
}

void USHMoraleSystem::NotifySquadmateCasualty(AActor* Casualty)
{
	ReportMoraleEvent(ESHMoraleEvent::SquadmateDown);
}

void USHMoraleSystem::SetNearbyFriendlyCount(int32 Count)
{
	NearbyFriendlyCount = FMath::Max(Count, 0);
}

void USHMoraleSystem::SetOfficerNearby(bool bNearby)
{
	bOfficerNearby = bNearby;
}

// =====================================================================
//  Passive tick modifiers
// =====================================================================

void USHMoraleSystem::TickPassiveModifiers(float DeltaTime)
{
	// Passive recovery when above Shaken threshold.
	if (CurrentState == ESHMoraleState::Determined || CurrentState == ESHMoraleState::Steady)
	{
		float Recovery = PassiveRecoveryRate;
		if (bOfficerNearby)
		{
			Recovery += OfficerRecoveryBonus;
		}
		AdjustMorale(Recovery * DeltaTime);
	}

	// Officer presence bonus (small continuous boost).
	if (bOfficerNearby && CurrentState == ESHMoraleState::Shaken)
	{
		AdjustMorale(OfficerRecoveryBonus * DeltaTime);
	}

	// Isolation penalty.
	if (NearbyFriendlyCount == 0)
	{
		AdjustMorale(-IsolationPenaltyRate * DeltaTime);
	}
}

// =====================================================================
//  State computation
// =====================================================================

void USHMoraleSystem::UpdateMoraleState()
{
	const ESHMoraleState NewState = ComputeState(MoraleValue);

	if (NewState != CurrentState)
	{
		const ESHMoraleState OldState = CurrentState;
		CurrentState = NewState;
		OnMoraleStateChanged.Broadcast(GetOwner(), OldState, NewState);

		// Fire break event when transitioning into Breaking or Routed.
		if ((NewState == ESHMoraleState::Breaking || NewState == ESHMoraleState::Routed) && !bBrokenNotificationSent)
		{
			bBrokenNotificationSent = true;
			const ESHMoraleBreakBehavior Behavior = DetermineBreakBehavior();
			OnMoraleBroken.Broadcast(GetOwner(), Behavior);
		}

		// Clear notification flag when recovering from broken state.
		if (NewState != ESHMoraleState::Breaking && NewState != ESHMoraleState::Routed)
		{
			bBrokenNotificationSent = false;
		}
	}
}

ESHMoraleState USHMoraleSystem::ComputeState(float Value) const
{
	if (Value >= 0.8f) return ESHMoraleState::Determined;
	if (Value >= 0.55f) return ESHMoraleState::Steady;
	if (Value >= 0.35f) return ESHMoraleState::Shaken;
	if (Value >= 0.15f) return ESHMoraleState::Breaking;
	return ESHMoraleState::Routed;
}

ESHMoraleBreakBehavior USHMoraleSystem::DetermineBreakBehavior() const
{
	// AI behavior when morale breaks depends on circumstances.
	if (CurrentState == ESHMoraleState::Routed)
	{
		// Routed characters have a chance to surrender or flee.
		// For deterministic behavior in the system, we use a simple heuristic.
		if (NearbyFriendlyCount == 0)
		{
			return ESHMoraleBreakBehavior::Surrender;
		}
		return ESHMoraleBreakBehavior::Retreat;
	}

	// Breaking state.
	if (NearbyFriendlyCount == 0)
	{
		return ESHMoraleBreakBehavior::Freeze;
	}

	return ESHMoraleBreakBehavior::RefuseOrders;
}

// =====================================================================
//  Queries
// =====================================================================

bool USHMoraleSystem::IsPlayerControlled() const
{
	if (const AActor* Owner = GetOwner())
	{
		if (const APawn* Pawn = Cast<APawn>(Owner))
		{
			return Pawn->IsPlayerControlled();
		}
	}
	return false;
}

float USHMoraleSystem::GetMoraleDesaturation() const
{
	// Only players get visual morale effects.
	if (!IsPlayerControlled())
	{
		return 0.f;
	}

	// Desaturation increases as morale drops below Steady.
	if (MoraleValue > 0.55f)
	{
		return 0.f;
	}
	return FMath::GetMappedRangeValueClamped(
		FVector2D(0.f, 0.55f), FVector2D(0.6f, 0.f), MoraleValue);
}

float USHMoraleSystem::GetMoraleVignette() const
{
	if (!IsPlayerControlled())
	{
		return 0.f;
	}

	if (MoraleValue > 0.55f)
	{
		return 0.f;
	}
	return FMath::GetMappedRangeValueClamped(
		FVector2D(0.f, 0.55f), FVector2D(0.8f, 0.f), MoraleValue);
}
