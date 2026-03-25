// Copyright Shattered Horizon 2032. All Rights Reserved.

#include "SHSquadManager.h"
#include "SHSquadMember.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "NavigationSystem.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"

USHSquadManager::USHSquadManager()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f; // 10 Hz is sufficient for squad-level logic
}

void USHSquadManager::BeginPlay()
{
	Super::BeginPlay();

	SquadMembers.Reserve(MaxSquadSize);
}

void USHSquadManager::TickComponent(float DeltaTime, ELevelTick TickType,
									 FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateCohesion();
	UpdateSquadMorale();
	UpdateCasualties();
	ExpireOldContacts();

	if (bAutoAdaptFormation)
	{
		UpdateFormationAdaptation();
	}
}

/* ────────────────────────────────────────────────────────────── */
/*  Squad Composition                                            */
/* ────────────────────────────────────────────────────────────── */

int32 USHSquadManager::AddSquadMember(ASHSquadMember* NewMember)
{
	if (!NewMember) return -1;
	if (SquadMembers.Num() >= MaxSquadSize) return -1;
	if (SquadMembers.Contains(NewMember)) return NewMember->SquadIndex;

	const int32 Index = SquadMembers.Add(NewMember);
	NewMember->SquadIndex = Index;
	NewMember->OwningSquadManager = this;

	// Auto-assign buddy teams: first two = Alpha, rest = Bravo
	NewMember->BuddyTeam = (Index < 2) ? ESHBuddyTeam::Alpha : ESHBuddyTeam::Bravo;

	// Bind to wound state delegate
	NewMember->OnWoundStateChanged.AddDynamic(this, &USHSquadManager::HandleMemberWoundStateChanged);

	return Index;
}

void USHSquadManager::RemoveSquadMember(ASHSquadMember* Member)
{
	if (!Member) return;

	Member->OnWoundStateChanged.RemoveDynamic(this, &USHSquadManager::HandleMemberWoundStateChanged);

	SquadMembers.Remove(Member);

	// Reindex remaining members
	for (int32 i = 0; i < SquadMembers.Num(); ++i)
	{
		if (SquadMembers[i])
		{
			SquadMembers[i]->SquadIndex = i;
			SquadMembers[i]->BuddyTeam = (i < 2) ? ESHBuddyTeam::Alpha : ESHBuddyTeam::Bravo;
		}
	}
}

TArray<ASHSquadMember*> USHSquadManager::GetAliveMembers() const
{
	TArray<ASHSquadMember*> Alive;
	for (ASHSquadMember* M : SquadMembers)
	{
		if (M && M->IsAlive())
		{
			Alive.Add(M);
		}
	}
	return Alive;
}

TArray<ASHSquadMember*> USHSquadManager::GetCombatEffectiveMembers() const
{
	TArray<ASHSquadMember*> Effective;
	for (ASHSquadMember* M : SquadMembers)
	{
		if (M && M->IsCombatEffective())
		{
			Effective.Add(M);
		}
	}
	return Effective;
}

TArray<ASHSquadMember*> USHSquadManager::GetBuddyTeam(ESHBuddyTeam Team) const
{
	TArray<ASHSquadMember*> Result;
	for (ASHSquadMember* M : SquadMembers)
	{
		if (M && M->BuddyTeam == Team && M->IsAlive())
		{
			Result.Add(M);
		}
	}
	return Result;
}

int32 USHSquadManager::GetEffectiveStrength() const
{
	int32 Count = 0;
	for (const ASHSquadMember* M : SquadMembers)
	{
		if (M && M->IsCombatEffective())
		{
			++Count;
		}
	}
	return Count;
}

/* ────────────────────────────────────────────────────────────── */
/*  Orders                                                       */
/* ────────────────────────────────────────────────────────────── */

void USHSquadManager::IssueSquadOrder(const FSHSquadOrder& Order)
{
	for (ASHSquadMember* M : SquadMembers)
	{
		if (M && M->CanExecuteOrders())
		{
			FSHSquadOrder MemberOrder = Order;
			MemberOrder.Timestamp = GetWorld()->GetTimeSeconds();
			M->ReceiveOrder(MemberOrder);
		}
	}
}

void USHSquadManager::IssueOrderToMember(int32 MemberIndex, const FSHSquadOrder& Order)
{
	if (SquadMembers.IsValidIndex(MemberIndex) && SquadMembers[MemberIndex])
	{
		FSHSquadOrder TimedOrder = Order;
		TimedOrder.Timestamp = GetWorld()->GetTimeSeconds();
		SquadMembers[MemberIndex]->ReceiveOrder(TimedOrder);
	}
}

void USHSquadManager::IssueOrderToBuddyTeam(ESHBuddyTeam Team, const FSHSquadOrder& Order)
{
	TArray<ASHSquadMember*> TeamMembers = GetBuddyTeam(Team);
	for (ASHSquadMember* M : TeamMembers)
	{
		if (M && M->CanExecuteOrders())
		{
			FSHSquadOrder TimedOrder = Order;
			TimedOrder.Timestamp = GetWorld()->GetTimeSeconds();
			M->ReceiveOrder(TimedOrder);
		}
	}
}

void USHSquadManager::OrderMoveTo(const FVector& Location)
{
	FSHSquadOrder Order = FSHSquadOrder::MakeOrder(
		ESHOrderType::MoveToPosition, Location, nullptr,
		ESHOrderPriority::Normal, PlayerLeader.Get());
	IssueSquadOrder(Order);
}

void USHSquadManager::OrderHoldPosition()
{
	FSHSquadOrder Order = FSHSquadOrder::MakeOrder(
		ESHOrderType::HoldPosition, FVector::ZeroVector, nullptr,
		ESHOrderPriority::Normal, PlayerLeader.Get());
	IssueSquadOrder(Order);
}

void USHSquadManager::OrderSuppress(const FVector& TargetLocation, AActor* TargetActor)
{
	FSHSquadOrder Order = FSHSquadOrder::MakeOrder(
		ESHOrderType::Suppress, TargetLocation, TargetActor,
		ESHOrderPriority::High, PlayerLeader.Get());
	IssueSquadOrder(Order);
}

void USHSquadManager::OrderFlank(bool bFlankLeft)
{
	const ESHOrderType FlankType = bFlankLeft ? ESHOrderType::FlankLeft : ESHOrderType::FlankRight;

	// Send one buddy team to flank, the other provides overwatch
	const ESHBuddyTeam FlankTeam = ESHBuddyTeam::Alpha;
	const ESHBuddyTeam OverwatchTeam = ESHBuddyTeam::Bravo;

	// Compute flank direction relative to squad forward
	FVector SquadForward = FVector::ForwardVector;
	if (PlayerLeader.IsValid())
	{
		SquadForward = PlayerLeader->GetActorForwardVector();
	}
	const FVector FlankDir = bFlankLeft
		? FVector::CrossProduct(SquadForward, FVector::UpVector).GetSafeNormal()
		: FVector::CrossProduct(FVector::UpVector, SquadForward).GetSafeNormal();

	// Flank order for Alpha
	FSHSquadOrder FlankOrder = FSHSquadOrder::MakeOrder(
		FlankType, FVector::ZeroVector, nullptr,
		ESHOrderPriority::High, PlayerLeader.Get());
	FlankOrder.SecondaryDirection = FlankDir;
	IssueOrderToBuddyTeam(FlankTeam, FlankOrder);

	// Overwatch order for Bravo
	FSHSquadOrder OverwatchOrder = FSHSquadOrder::MakeOrder(
		ESHOrderType::ProvideOverwatch, FVector::ZeroVector, nullptr,
		ESHOrderPriority::High, PlayerLeader.Get());
	IssueOrderToBuddyTeam(OverwatchTeam, OverwatchOrder);
}

void USHSquadManager::OrderFallBack()
{
	FVector FallBackLocation = FVector::ZeroVector;
	if (PlayerLeader.IsValid())
	{
		// Fall back 20m behind the squad leader
		FallBackLocation = PlayerLeader->GetActorLocation()
			- PlayerLeader->GetActorForwardVector() * 2000.f;
	}

	FSHSquadOrder Order = FSHSquadOrder::MakeOrder(
		ESHOrderType::FallBack, FallBackLocation, nullptr,
		ESHOrderPriority::High, PlayerLeader.Get());
	IssueSquadOrder(Order);
}

void USHSquadManager::OrderBreachAndClear(const FVector& DoorLocation, AActor* DoorActor)
{
	// First, stack up — then breach
	FSHSquadOrder StackOrder = FSHSquadOrder::MakeOrder(
		ESHOrderType::StackUp, DoorLocation, DoorActor,
		ESHOrderPriority::High, PlayerLeader.Get());
	IssueSquadOrder(StackOrder);

	// The breach-and-clear order goes into the queue (will execute after stack-up completes)
	FSHSquadOrder BreachOrder = FSHSquadOrder::MakeOrder(
		ESHOrderType::BreachAndClear, DoorLocation, DoorActor,
		ESHOrderPriority::Critical, PlayerLeader.Get());

	for (ASHSquadMember* M : SquadMembers)
	{
		if (M && M->CanExecuteOrders())
		{
			M->OrderQueue.Add(BreachOrder);
		}
	}
}

void USHSquadManager::OrderRally()
{
	FVector RallyPoint = FVector::ZeroVector;
	if (PlayerLeader.IsValid())
	{
		RallyPoint = PlayerLeader->GetActorLocation();
	}

	FSHSquadOrder Order = FSHSquadOrder::MakeOrder(
		ESHOrderType::Rally, RallyPoint, nullptr,
		ESHOrderPriority::High, PlayerLeader.Get());
	IssueSquadOrder(Order);
}

/* ────────────────────────────────────────────────────────────── */
/*  Formation                                                    */
/* ────────────────────────────────────────────────────────────── */

void USHSquadManager::SetFormation(ESHFormationType NewFormation)
{
	if (CurrentFormation != NewFormation)
	{
		CurrentFormation = NewFormation;
		OnFormationChanged.Broadcast(NewFormation);
	}
}

FVector USHSquadManager::GetFormationPosition(int32 MemberIndex) const
{
	if (!PlayerLeader.IsValid())
	{
		return FVector::ZeroVector;
	}

	const FVector LeaderPos = PlayerLeader->GetActorLocation();
	const FRotator LeaderRot = PlayerLeader->GetActorRotation();
	const FVector Offset = ComputeFormationOffset(MemberIndex, CurrentFormation);

	// Transform local offset to world space relative to leader
	return LeaderPos + LeaderRot.RotateVector(Offset);
}

FVector USHSquadManager::ComputeFormationOffset(int32 Index, ESHFormationType Formation) const
{
	// All offsets in local space: X = forward, Y = right, Z = up
	const float S = FormationSpacing;

	switch (Formation)
	{
	case ESHFormationType::Wedge:
		// Inverted-V behind leader
		switch (Index)
		{
		case 0: return FVector(-S,      -S,       0.f);  // Left rear
		case 1: return FVector(-S,       S,       0.f);  // Right rear
		case 2: return FVector(-S * 2.f, -S * 0.5f, 0.f); // Far left
		case 3: return FVector(-S * 2.f,  S * 0.5f, 0.f); // Far right
		default: return FVector(-S * (Index + 1), 0.f, 0.f);
		}

	case ESHFormationType::Column:
		// Single file behind leader, staggered slightly
		{
			const float Stagger = (Index % 2 == 0) ? -50.f : 50.f;
			return FVector(-S * (Index + 1), Stagger, 0.f);
		}

	case ESHFormationType::Line:
		// Abreast, centered on leader
		{
			const float HalfCount = (SquadMembers.Num() - 1) * 0.5f;
			const float Offset = (Index - HalfCount) * S;
			return FVector(0.f, Offset, 0.f);
		}

	case ESHFormationType::File:
		// Strict single file, no stagger (for narrow passages)
		return FVector(-S * (Index + 1), 0.f, 0.f);

	case ESHFormationType::StaggeredColumn:
		// Two columns offset
		{
			const float Side = (Index % 2 == 0) ? -S * 0.5f : S * 0.5f;
			const int32 Row = (Index / 2) + 1;
			return FVector(-S * Row, Side, 0.f);
		}

	default:
		return FVector(-S * (Index + 1), 0.f, 0.f);
	}
}

/* ────────────────────────────────────────────────────────────── */
/*  Cohesion                                                     */
/* ────────────────────────────────────────────────────────────── */

void USHSquadManager::UpdateCohesion()
{
	if (!PlayerLeader.IsValid() || SquadMembers.Num() == 0)
	{
		Cohesion.CohesionScore = 1.f;
		return;
	}

	const FVector LeaderPos = PlayerLeader->GetActorLocation();
	const UWorld* World = GetWorld();

	float TotalDist = 0.f;
	float MaxDist = 0.f;
	int32 LOSCount = 0;
	int32 AliveCount = 0;

	for (const ASHSquadMember* M : SquadMembers)
	{
		if (!M || !M->IsAlive()) continue;

		++AliveCount;
		const float Dist = FVector::Dist(M->GetActorLocation(), LeaderPos);
		TotalDist += Dist;
		MaxDist = FMath::Max(MaxDist, Dist);

		// Line-of-sight check to leader
		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(M);
		Params.AddIgnoredActor(PlayerLeader.Get());
		const bool bHasLOS = !World->LineTraceSingleByChannel(
			Hit, M->GetActorLocation() + FVector(0, 0, 100),
			LeaderPos + FVector(0, 0, 100),
			ECC_Visibility, Params);

		if (bHasLOS)
		{
			++LOSCount;
		}
	}

	if (AliveCount > 0)
	{
		Cohesion.AverageSpread = TotalDist / AliveCount;
		Cohesion.MaxMemberDistance = MaxDist;
		Cohesion.MembersWithLOSToLeader = LOSCount;

		// Score: 1.0 at spacing, falling off beyond MaxFormationSpread
		const float SpreadRatio = FMath::Clamp(Cohesion.AverageSpread / MaxFormationSpread, 0.f, 2.f);
		const float LOSRatio = static_cast<float>(LOSCount) / AliveCount;
		Cohesion.CohesionScore = FMath::Clamp((1.f - SpreadRatio * 0.5f) * (0.5f + LOSRatio * 0.5f), 0.f, 1.f);
	}
}

/* ────────────────────────────────────────────────────────────── */
/*  Squad Morale                                                 */
/* ────────────────────────────────────────────────────────────── */

void USHSquadManager::UpdateSquadMorale()
{
	float Total = 0.f;
	int32 Count = 0;

	for (const ASHSquadMember* M : SquadMembers)
	{
		if (M && M->IsAlive())
		{
			Total += M->Morale;
			++Count;
		}
	}

	const float PreviousMorale = SquadMorale;

	if (Count > 0)
	{
		SquadMorale = Total / Count;
	}

	// Casualties drag morale down further
	SquadMorale -= KIACount * 10.f;
	SquadMorale -= WoundedCount * 5.f;

	// Cohesion bonus
	SquadMorale += Cohesion.CohesionScore * 5.f;

	SquadMorale = FMath::Clamp(SquadMorale, 0.f, 100.f);

	if (FMath::Abs(SquadMorale - PreviousMorale) > 3.f)
	{
		OnSquadMoraleChanged.Broadcast(SquadMorale);
	}
}

/* ────────────────────────────────────────────────────────────── */
/*  Casualties                                                   */
/* ────────────────────────────────────────────────────────────── */

void USHSquadManager::UpdateCasualties()
{
	WoundedCount = 0;
	KIACount = 0;

	for (const ASHSquadMember* M : SquadMembers)
	{
		if (!M) continue;

		switch (M->WoundState)
		{
		case ESHWoundState::LightWound:
		case ESHWoundState::HeavyWound:
		case ESHWoundState::Incapacitated:
			++WoundedCount;
			break;
		case ESHWoundState::KIA:
			++KIACount;
			break;
		default:
			break;
		}
	}
}

void USHSquadManager::HandleMemberWoundStateChanged(ASHSquadMember* Member, ESHWoundState NewState)
{
	OnSquadMemberStatusChanged.Broadcast(Member, NewState);

	// If a member goes down, nearby squad mates call it out
	if (NewState == ESHWoundState::Incapacitated || NewState == ESHWoundState::KIA)
	{
		const ESHVoiceLineType CallType =
			(NewState == ESHWoundState::KIA) ? ESHVoiceLineType::ManDown : ESHVoiceLineType::WoundedFriendly;

		for (ASHSquadMember* M : SquadMembers)
		{
			if (M && M != Member && M->IsCombatEffective())
			{
				const float Dist = FVector::Dist(M->GetActorLocation(), Member->GetActorLocation());
				if (Dist < 3000.f) // Within 30m
				{
					M->PlayVoiceLine(CallType);
					break; // Only one member calls it out
				}
			}
		}
	}
}

/* ────────────────────────────────────────────────────────────── */
/*  Formation Adaptation                                         */
/* ────────────────────────────────────────────────────────────── */

void USHSquadManager::UpdateFormationAdaptation()
{
	const ESHFormationType Suggested = EvaluateTerrainFormation();
	if (Suggested != CurrentFormation)
	{
		SetFormation(Suggested);
	}
}

ESHFormationType USHSquadManager::EvaluateTerrainFormation() const
{
	if (!PlayerLeader.IsValid()) return CurrentFormation;

	const UWorld* World = GetWorld();
	const FVector LeaderPos = PlayerLeader->GetActorLocation();
	const FVector Forward = PlayerLeader->GetActorForwardVector();
	const FVector Right = FVector::CrossProduct(Forward, FVector::UpVector).GetSafeNormal();

	// Probe laterally to determine passage width
	FHitResult HitLeft, HitRight;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(PlayerLeader.Get());

	const float ProbeDistance = 600.f; // 6m
	const FVector EyeOffset(0, 0, 100);

	const bool bBlockedLeft = World->LineTraceSingleByChannel(
		HitLeft, LeaderPos + EyeOffset, LeaderPos + EyeOffset - Right * ProbeDistance,
		ECC_WorldStatic, Params);

	const bool bBlockedRight = World->LineTraceSingleByChannel(
		HitRight, LeaderPos + EyeOffset, LeaderPos + EyeOffset + Right * ProbeDistance,
		ECC_WorldStatic, Params);

	// Narrow passage — switch to file
	if (bBlockedLeft && bBlockedRight)
	{
		const float Width = 0.f
			+ (bBlockedLeft  ? HitLeft.Distance  : ProbeDistance)
			+ (bBlockedRight ? HitRight.Distance  : ProbeDistance);

		if (Width < 300.f) // < 3m
		{
			return ESHFormationType::File;
		}
		if (Width < 500.f) // < 5m
		{
			return ESHFormationType::Column;
		}
	}

	// Open terrain — wedge is default combat formation
	// If the squad is actively in contact, line abreast may be more useful,
	// but we leave that decision to player commands.
	return ESHFormationType::Wedge;
}

/* ────────────────────────────────────────────────────────────── */
/*  Contact Reports                                              */
/* ────────────────────────────────────────────────────────────── */

void USHSquadManager::OnContactReported(ASHSquadMember* Reporter, const FSHContactReport& Report)
{
	// Avoid duplicate reports for the same location within a short time
	for (const FSHContactReport& Existing : RecentContacts)
	{
		if (FVector::DistSquared(Existing.ContactLocation, Report.ContactLocation) < 10000.f // 1m
			&& (Report.Timestamp - Existing.Timestamp) < 5.f)
		{
			return; // Duplicate
		}
	}

	if (RecentContacts.Num() >= MaxContactReports)
	{
		RecentContacts.RemoveAt(0); // Evict oldest
	}
	RecentContacts.Add(Report);

	OnContactReportedDelegate.Broadcast(Report);
}

void USHSquadManager::ExpireOldContacts()
{
	const float CurrentTime = GetWorld()->GetTimeSeconds();
	RecentContacts.RemoveAll([CurrentTime, this](const FSHContactReport& R)
	{
		return (CurrentTime - R.Timestamp) > ContactReportExpiry;
	});
}
