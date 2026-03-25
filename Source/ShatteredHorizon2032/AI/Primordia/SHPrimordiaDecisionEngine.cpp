// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "AI/Primordia/SHPrimordiaDecisionEngine.h"

DEFINE_LOG_CATEGORY_STATIC(LogPrimordiaDecisionEngine, Log, All);

// -----------------------------------------------------------------------
USHPrimordiaDecisionEngine::USHPrimordiaDecisionEngine()
{
}

void USHPrimordiaDecisionEngine::Initialize()
{
	CurrentGameTime = 0.f;
	ActiveOrders.Empty();
	PendingOrders.Empty();
	UE_LOG(LogPrimordiaDecisionEngine, Log, TEXT("Decision Engine initialized."));
}

// -----------------------------------------------------------------------
//  Tick
// -----------------------------------------------------------------------

void USHPrimordiaDecisionEngine::Tick(float DeltaSeconds)
{
	CurrentGameTime += DeltaSeconds;

	// Attempt to assign squads to pending orders.
	for (int32 i = PendingOrders.Num() - 1; i >= 0; --i)
	{
		FSHTacticalOrder& Order = PendingOrders[i];

		// Check if the scheduled time has arrived.
		if (Order.ScheduledTime > CurrentGameTime)
		{
			continue;
		}

		const int32 SquadId = AllocateSquadForOrder(Order);
		if (SquadId != INDEX_NONE)
		{
			IssueOrderToSquad(SquadId, Order);
			ActiveOrders.Add(Order);
			PendingOrders.RemoveAt(i);
		}
	}

	TickOrderLifecycle(DeltaSeconds);
	TickEscalation(DeltaSeconds);
	TickMoraleAndRetreat(DeltaSeconds);
}

// -----------------------------------------------------------------------
//  Directive ingestion
// -----------------------------------------------------------------------

void USHPrimordiaDecisionEngine::IngestDirectiveBatch(const FSHPrimordiaDirectiveBatch& Batch)
{
	UE_LOG(LogPrimordiaDecisionEngine, Log, TEXT("Ingesting directive batch %s (%d directives)"),
		*Batch.BatchId, Batch.Directives.Num());

	// Sort by priority (descending).
	TArray<FSHPrimordiaTacticalDirective> Sorted = Batch.Directives;
	Sorted.Sort([](const FSHPrimordiaTacticalDirective& A, const FSHPrimordiaTacticalDirective& B)
	{
		return A.Priority > B.Priority;
	});

	for (const FSHPrimordiaTacticalDirective& Directive : Sorted)
	{
		DecomposeDirective(Directive);
	}
}

void USHPrimordiaDecisionEngine::DecomposeDirective(const FSHPrimordiaTacticalDirective& Directive)
{
	const FString& Type = Directive.OrderType;

	if (Type == TEXT("mass_assault") || Type == TEXT("attack"))
	{
		// Single focused attack order using the requested force ratio.
		FSHTacticalOrder Order;
		Order.OrderId = FGuid::NewGuid();
		Order.SourceDirectiveId = Directive.DirectiveId;
		Order.OrderType = TEXT("attack");
		Order.TargetLocation = Directive.TargetLocation;
		Order.ScheduledTime = Directive.ExecutionTime > 0.f ? Directive.ExecutionTime : CurrentGameTime;
		Order.Parameters = Directive.Parameters;

		// Determine how many squads to commit.
		const int32 TotalAvailable = GetAvailableSquadCount(false);
		const int32 DesiredCount = FMath::Max(1, FMath::RoundToInt32(TotalAvailable * Directive.ForceAllocationRatio));

		// Create one order per squad to commit.
		for (int32 i = 0; i < DesiredCount; ++i)
		{
			FSHTacticalOrder SquadOrder = Order;
			SquadOrder.OrderId = FGuid::NewGuid();

			if (Directive.AssignedSquadIds.IsValidIndex(i))
			{
				SquadOrder.SquadId = Directive.AssignedSquadIds[i];
			}
			PendingOrders.Add(MoveTemp(SquadOrder));
		}
	}
	else if (Type == TEXT("feint"))
	{
		// Phase 1: feint attack at secondary location with small force.
		{
			FSHTacticalOrder Feint;
			Feint.OrderId = FGuid::NewGuid();
			Feint.SourceDirectiveId = Directive.DirectiveId;
			Feint.OrderType = TEXT("feint");
			Feint.TargetLocation = Directive.SecondaryLocation;
			Feint.ScheduledTime = Directive.ExecutionTime > 0.f ? Directive.ExecutionTime : CurrentGameTime;
			Feint.TimeoutSeconds = 45.f; // Short — feints are brief.
			Feint.Parameters = Directive.Parameters;
			PendingOrders.Add(MoveTemp(Feint));
		}

		// Phase 2: real push at primary location, delayed.
		{
			FSHTacticalOrder Push;
			Push.OrderId = FGuid::NewGuid();
			Push.SourceDirectiveId = Directive.DirectiveId;
			Push.OrderType = TEXT("attack");
			Push.TargetLocation = Directive.TargetLocation;
			Push.ScheduledTime = (Directive.ExecutionTime > 0.f ? Directive.ExecutionTime : CurrentGameTime) + 20.f;
			Push.Parameters = Directive.Parameters;

			const int32 TotalAvailable = GetAvailableSquadCount(false);
			const int32 DesiredCount = FMath::Max(1, FMath::RoundToInt32(TotalAvailable * Directive.ForceAllocationRatio));
			for (int32 i = 0; i < DesiredCount; ++i)
			{
				FSHTacticalOrder Copy = Push;
				Copy.OrderId = FGuid::NewGuid();
				PendingOrders.Add(MoveTemp(Copy));
			}
		}
	}
	else if (Type == TEXT("probe"))
	{
		// Lightweight recon-in-force — single squad.
		FSHTacticalOrder Probe;
		Probe.OrderId = FGuid::NewGuid();
		Probe.SourceDirectiveId = Directive.DirectiveId;
		Probe.OrderType = TEXT("probe");
		Probe.TargetLocation = Directive.TargetLocation;
		Probe.ScheduledTime = Directive.ExecutionTime > 0.f ? Directive.ExecutionTime : CurrentGameTime;
		Probe.TimeoutSeconds = 60.f;
		Probe.Parameters = Directive.Parameters;
		PendingOrders.Add(MoveTemp(Probe));
	}
	else if (Type == TEXT("retreat") || Type == TEXT("regroup"))
	{
		// Issue retreat to specified squads or all engaged squads.
		if (Directive.AssignedSquadIds.Num() > 0)
		{
			for (int32 SqId : Directive.AssignedSquadIds)
			{
				FSHTacticalOrder Retreat;
				Retreat.OrderId = FGuid::NewGuid();
				Retreat.SourceDirectiveId = Directive.DirectiveId;
				Retreat.OrderType = TEXT("retreat");
				Retreat.TargetLocation = Directive.TargetLocation; // Rally point.
				Retreat.SquadId = SqId;
				Retreat.ScheduledTime = CurrentGameTime;
				PendingOrders.Add(MoveTemp(Retreat));
			}
		}
		else
		{
			// Retreat all currently engaged.
			for (FSHTacticalOrder& Active : ActiveOrders)
			{
				Active.State = ESHTacticalOrderState::Cancelled;
			}
		}
	}
	else if (Type == TEXT("hold"))
	{
		// Defensive hold at location.
		FSHTacticalOrder Hold;
		Hold.OrderId = FGuid::NewGuid();
		Hold.SourceDirectiveId = Directive.DirectiveId;
		Hold.OrderType = TEXT("hold");
		Hold.TargetLocation = Directive.TargetLocation;
		Hold.ScheduledTime = CurrentGameTime;
		Hold.TimeoutSeconds = 300.f; // Holds can be long.
		Hold.Parameters = Directive.Parameters;
		PendingOrders.Add(MoveTemp(Hold));
	}
	else if (Type == TEXT("suppress"))
	{
		FSHTacticalOrder Suppress;
		Suppress.OrderId = FGuid::NewGuid();
		Suppress.SourceDirectiveId = Directive.DirectiveId;
		Suppress.OrderType = TEXT("suppress");
		Suppress.TargetLocation = Directive.TargetLocation;
		Suppress.ScheduledTime = CurrentGameTime;
		Suppress.TimeoutSeconds = 60.f;
		Suppress.Parameters = Directive.Parameters;
		PendingOrders.Add(MoveTemp(Suppress));
	}
	else
	{
		UE_LOG(LogPrimordiaDecisionEngine, Warning, TEXT("Unknown directive type '%s' — creating generic order."), *Type);
		FSHTacticalOrder Generic;
		Generic.OrderId = FGuid::NewGuid();
		Generic.SourceDirectiveId = Directive.DirectiveId;
		Generic.OrderType = Type;
		Generic.TargetLocation = Directive.TargetLocation;
		Generic.ScheduledTime = CurrentGameTime;
		Generic.Parameters = Directive.Parameters;
		PendingOrders.Add(MoveTemp(Generic));
	}
}

// -----------------------------------------------------------------------
//  Squad registration
// -----------------------------------------------------------------------

void USHPrimordiaDecisionEngine::RegisterSquad(int32 SquadId, int32 MemberCount, bool bReserve)
{
	// Avoid duplicates.
	if (FindSquadAllocation(SquadId) != nullptr)
	{
		UE_LOG(LogPrimordiaDecisionEngine, Warning, TEXT("Squad %d already registered."), SquadId);
		return;
	}

	FSHSquadAllocation Alloc;
	Alloc.SquadId = SquadId;
	Alloc.ActiveMemberCount = MemberCount;
	Alloc.MaxMemberCount = MemberCount;
	Alloc.bIsReserve = bReserve;
	SquadAllocations.Add(Alloc);

	UE_LOG(LogPrimordiaDecisionEngine, Log, TEXT("Registered squad %d (%d members, reserve=%d)"), SquadId, MemberCount, bReserve);
}

void USHPrimordiaDecisionEngine::UnregisterSquad(int32 SquadId)
{
	SquadAllocations.RemoveAll([SquadId](const FSHSquadAllocation& A) { return A.SquadId == SquadId; });

	// Cancel any active orders for this squad.
	for (FSHTacticalOrder& Order : ActiveOrders)
	{
		if (Order.SquadId == SquadId && Order.State == ESHTacticalOrderState::InProgress)
		{
			Order.State = ESHTacticalOrderState::Failed;
			OnSquadOrderCompleted.Broadcast(SquadId, ESHTacticalOrderState::Failed);
		}
	}
}

void USHPrimordiaDecisionEngine::UpdateSquadStrength(int32 SquadId, int32 AliveMemberCount, float AverageMorale)
{
	FSHSquadAllocation* Alloc = FindSquadAllocation(SquadId);
	if (Alloc)
	{
		Alloc->ActiveMemberCount = AliveMemberCount;
		Alloc->AverageMorale = AverageMorale;
	}
}

// -----------------------------------------------------------------------
//  Order queries
// -----------------------------------------------------------------------

bool USHPrimordiaDecisionEngine::GetActiveOrderForSquad(int32 SquadId, FSHTacticalOrder& OutOrder) const
{
	for (const FSHTacticalOrder& Order : ActiveOrders)
	{
		if (Order.SquadId == SquadId && Order.State == ESHTacticalOrderState::InProgress)
		{
			OutOrder = Order;
			return true;
		}
	}
	return false;
}

TArray<FSHTacticalOrder> USHPrimordiaDecisionEngine::GetAllActiveOrders() const
{
	TArray<FSHTacticalOrder> Result;
	for (const FSHTacticalOrder& Order : ActiveOrders)
	{
		if (Order.State == ESHTacticalOrderState::InProgress || Order.State == ESHTacticalOrderState::Stalled)
		{
			Result.Add(Order);
		}
	}
	return Result;
}

void USHPrimordiaDecisionEngine::CancelOrder(const FGuid& OrderId)
{
	for (FSHTacticalOrder& Order : ActiveOrders)
	{
		if (Order.OrderId == OrderId)
		{
			Order.State = ESHTacticalOrderState::Cancelled;
			if (FSHSquadAllocation* Alloc = FindSquadAllocation(Order.SquadId))
			{
				Alloc->bAssigned = false;
				Alloc->ActiveOrderId.Invalidate();
			}
			OnSquadOrderCompleted.Broadcast(Order.SquadId, ESHTacticalOrderState::Cancelled);
			return;
		}
	}
}

// -----------------------------------------------------------------------
//  Fallback
// -----------------------------------------------------------------------

void USHPrimordiaDecisionEngine::GenerateLocalFallbackOrders()
{
	UE_LOG(LogPrimordiaDecisionEngine, Log, TEXT("Generating local fallback orders for %d squads."), SquadAllocations.Num());

	for (FSHSquadAllocation& Alloc : SquadAllocations)
	{
		if (Alloc.bAssigned || Alloc.ActiveMemberCount <= 0)
		{
			continue;
		}

		// Default: hold current position.
		FSHTacticalOrder Hold;
		Hold.OrderId = FGuid::NewGuid();
		Hold.SourceDirectiveId = TEXT("local_fallback");
		Hold.OrderType = TEXT("hold");
		Hold.TargetLocation = FVector::ZeroVector; // Controller will use current position.
		Hold.SquadId = Alloc.SquadId;
		Hold.ScheduledTime = CurrentGameTime;
		Hold.TimeoutSeconds = 600.f;

		IssueOrderToSquad(Alloc.SquadId, Hold);
		ActiveOrders.Add(Hold);
	}
}

// -----------------------------------------------------------------------
//  Wave spawning interface
// -----------------------------------------------------------------------

void USHPrimordiaDecisionEngine::OnReinforcementWaveSpawned(const TArray<int32>& NewSquadIds)
{
	// Assign new squads to the most urgent pending orders.
	for (int32 SqId : NewSquadIds)
	{
		if (PendingOrders.Num() == 0)
		{
			break;
		}

		// Pick the highest-priority pending order.
		int32 BestIdx = 0;
		for (int32 i = 1; i < PendingOrders.Num(); ++i)
		{
			if (PendingOrders[i].ScheduledTime <= CurrentGameTime)
			{
				BestIdx = i;
				break;
			}
		}

		FSHTacticalOrder Order = PendingOrders[BestIdx];
		PendingOrders.RemoveAt(BestIdx);

		IssueOrderToSquad(SqId, Order);
		ActiveOrders.Add(Order);
	}
}

// -----------------------------------------------------------------------
//  Internal — Squad allocation
// -----------------------------------------------------------------------

int32 USHPrimordiaDecisionEngine::AllocateSquadForOrder(const FSHTacticalOrder& Order)
{
	// If the order already specifies a squad, use it.
	if (Order.SquadId != INDEX_NONE)
	{
		FSHSquadAllocation* Alloc = FindSquadAllocation(Order.SquadId);
		if (Alloc && !Alloc->bAssigned && Alloc->ActiveMemberCount > 0)
		{
			return Order.SquadId;
		}
	}

	return FindNearestAvailableSquad(Order.TargetLocation, false);
}

void USHPrimordiaDecisionEngine::IssueOrderToSquad(int32 SquadId, FSHTacticalOrder& Order)
{
	Order.SquadId = SquadId;
	Order.State = ESHTacticalOrderState::InProgress;

	FSHSquadAllocation* Alloc = FindSquadAllocation(SquadId);
	if (Alloc)
	{
		Alloc->bAssigned = true;
		Alloc->bIsReserve = false; // No longer reserve once committed.
		Alloc->ActiveOrderId = Order.OrderId;
	}

	UE_LOG(LogPrimordiaDecisionEngine, Log, TEXT("Issued order [%s] '%s' to squad %d -> %s"),
		*Order.OrderId.ToString(EGuidFormats::Short), *Order.OrderType, SquadId, *Order.TargetLocation.ToString());

	OnSquadOrderIssued.Broadcast(SquadId, Order);
}

// -----------------------------------------------------------------------
//  Internal — Tick sub-systems
// -----------------------------------------------------------------------

void USHPrimordiaDecisionEngine::TickOrderLifecycle(float DeltaSeconds)
{
	for (int32 i = ActiveOrders.Num() - 1; i >= 0; --i)
	{
		FSHTacticalOrder& Order = ActiveOrders[i];

		if (Order.State != ESHTacticalOrderState::InProgress && Order.State != ESHTacticalOrderState::Stalled)
		{
			// Remove completed/cancelled/failed orders after a brief grace period.
			ActiveOrders.RemoveAt(i);
			continue;
		}

		Order.ElapsedTime += DeltaSeconds;

		// Timeout check.
		if (Order.ElapsedTime > Order.TimeoutSeconds)
		{
			UE_LOG(LogPrimordiaDecisionEngine, Log, TEXT("Order %s timed out for squad %d."), *Order.OrderType, Order.SquadId);
			Order.State = ESHTacticalOrderState::Failed;

			if (FSHSquadAllocation* Alloc = FindSquadAllocation(Order.SquadId))
			{
				Alloc->bAssigned = false;
				Alloc->ActiveOrderId.Invalidate();
			}
			OnSquadOrderCompleted.Broadcast(Order.SquadId, ESHTacticalOrderState::Failed);
		}
	}
}

void USHPrimordiaDecisionEngine::TickEscalation(float DeltaSeconds)
{
	for (FSHTacticalOrder& Order : ActiveOrders)
	{
		if (Order.State != ESHTacticalOrderState::InProgress)
		{
			continue;
		}

		// Mark as stalled if it has been running too long without progress.
		if (Order.ElapsedTime > StallThresholdSeconds && Order.State == ESHTacticalOrderState::InProgress)
		{
			// Check squad strength — if the squad is losing people, the attack is stalling.
			const FSHSquadAllocation* Alloc = FindSquadAllocation(Order.SquadId);
			if (Alloc && Alloc->GetStrengthRatio() < 0.7f)
			{
				Order.State = ESHTacticalOrderState::Stalled;
				UE_LOG(LogPrimordiaDecisionEngine, Log, TEXT("Order for squad %d STALLED — attempting escalation."), Order.SquadId);

				// Try to commit a reserve.
				const int32 ReserveId = FindNearestAvailableSquad(Order.TargetLocation, true);
				if (ReserveId != INDEX_NONE)
				{
					CommitReserve(ReserveId, Order);
				}
			}
		}
	}
}

void USHPrimordiaDecisionEngine::TickMoraleAndRetreat(float DeltaSeconds)
{
	for (FSHSquadAllocation& Alloc : SquadAllocations)
	{
		if (!Alloc.bAssigned || Alloc.ActiveMemberCount <= 0)
		{
			continue;
		}

		// Heavy casualties — force retreat.
		if (Alloc.GetStrengthRatio() < CasualtyRetreatThreshold || Alloc.AverageMorale < MoraleRetreatThreshold)
		{
			UE_LOG(LogPrimordiaDecisionEngine, Log, TEXT("Squad %d broken (strength=%.2f, morale=%.2f) — issuing retreat."),
				Alloc.SquadId, Alloc.GetStrengthRatio(), Alloc.AverageMorale);

			// Cancel current order.
			for (FSHTacticalOrder& Order : ActiveOrders)
			{
				if (Order.SquadId == Alloc.SquadId && Order.State == ESHTacticalOrderState::InProgress)
				{
					Order.State = ESHTacticalOrderState::Failed;
					OnSquadOrderCompleted.Broadcast(Alloc.SquadId, ESHTacticalOrderState::Failed);
				}
			}

			// Issue retreat.
			FSHTacticalOrder Retreat;
			Retreat.OrderId = FGuid::NewGuid();
			Retreat.SourceDirectiveId = TEXT("morale_break");
			Retreat.OrderType = TEXT("retreat");
			Retreat.TargetLocation = FVector::ZeroVector; // Controller determines rally point.
			Retreat.ScheduledTime = CurrentGameTime;

			IssueOrderToSquad(Alloc.SquadId, Retreat);
			ActiveOrders.Add(Retreat);
		}
	}
}

void USHPrimordiaDecisionEngine::CommitReserve(int32 ReserveSquadId, const FSHTacticalOrder& StalledOrder)
{
	FSHTacticalOrder Reinforce;
	Reinforce.OrderId = FGuid::NewGuid();
	Reinforce.SourceDirectiveId = StalledOrder.SourceDirectiveId;
	Reinforce.OrderType = TEXT("attack"); // Reinforcing with an attack order on the same target.
	Reinforce.TargetLocation = StalledOrder.TargetLocation;
	Reinforce.ScheduledTime = CurrentGameTime;
	Reinforce.Parameters = StalledOrder.Parameters;

	IssueOrderToSquad(ReserveSquadId, Reinforce);
	ActiveOrders.Add(Reinforce);

	UE_LOG(LogPrimordiaDecisionEngine, Log, TEXT("Committed reserve squad %d to reinforce stalled attack at %s"),
		ReserveSquadId, *StalledOrder.TargetLocation.ToString());

	OnReservesCommitted.Broadcast(ReserveSquadId);
}

int32 USHPrimordiaDecisionEngine::FindNearestAvailableSquad(FVector Location, bool bReserveOnly) const
{
	int32 BestId = INDEX_NONE;
	float BestScore = TNumericLimits<float>::Max();

	for (const FSHSquadAllocation& Alloc : SquadAllocations)
	{
		if (Alloc.bAssigned || Alloc.ActiveMemberCount <= 0)
		{
			continue;
		}
		if (bReserveOnly && !Alloc.bIsReserve)
		{
			continue;
		}

		// Score by strength (prefer full-strength squads) — distance scoring would require
		// world position data which we don't cache here; the controller layer handles positioning.
		const float StrengthScore = 1.f - Alloc.GetStrengthRatio();
		if (StrengthScore < BestScore)
		{
			BestScore = StrengthScore;
			BestId = Alloc.SquadId;
		}
	}

	return BestId;
}

FSHSquadAllocation* USHPrimordiaDecisionEngine::FindSquadAllocation(int32 SquadId)
{
	return SquadAllocations.FindByPredicate([SquadId](const FSHSquadAllocation& A) { return A.SquadId == SquadId; });
}

const FSHSquadAllocation* USHPrimordiaDecisionEngine::FindSquadAllocation(int32 SquadId) const
{
	return SquadAllocations.FindByPredicate([SquadId](const FSHSquadAllocation& A) { return A.SquadId == SquadId; });
}

// Helper used by DecomposeDirective — count unassigned non-reserve squads.
int32 USHPrimordiaDecisionEngine::GetAvailableSquadCount(bool bIncludeReserves) const
{
	int32 Count = 0;
	for (const FSHSquadAllocation& Alloc : SquadAllocations)
	{
		if (!Alloc.bAssigned && Alloc.ActiveMemberCount > 0)
		{
			if (bIncludeReserves || !Alloc.bIsReserve)
			{
				++Count;
			}
		}
	}
	return Count;
}
