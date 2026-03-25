// Copyright Shattered Horizon 2032. All Rights Reserved.

#include "SHSquadAIController.h"
#include "SHSquadMember.h"
#include "SHSquadOrder.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Damage.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/EnvQuery.h"
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "NavigationSystem.h"
#include "Navigation/PathFollowingComponent.h"
#include "Kismet/KismetMathLibrary.h"

// ───────────────────────────────────────────────────────────────
//  Constructor
// ───────────────────────────────────────────────────────────────

ASHSquadAIController::ASHSquadAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.05f; // 20Hz AI tick

	BehaviorTreeComp = CreateDefaultSubobject<UBehaviorTreeComponent>(TEXT("BehaviorTreeComp"));
	BlackboardComp = CreateDefaultSubobject<UBlackboardComponent>(TEXT("BlackboardComp"));
	PerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("PerceptionComp"));

	SetPerceptionComponent(*PerceptionComp);
	SetupPerception();
}

// ───────────────────────────────────────────────────────────────
//  Perception Setup
// ───────────────────────────────────────────────────────────────

void ASHSquadAIController::SetupPerception()
{
	// Sight
	SightConfig = CreateDefaultSubobject<UAISenseConfig_Sight>(TEXT("SightConfig"));
	SightConfig->SightRadius = SightRange;
	SightConfig->LoseSightRadius = SightRange * 1.2f;
	SightConfig->PeripheralVisionAngleDegrees = SightAngle;
	SightConfig->SetMaxAge(LoseSightTime);
	SightConfig->AutoSuccessRangeFromLastSeenLocation = 500.f;
	SightConfig->DetectionByAffiliation.bDetectEnemies = true;
	SightConfig->DetectionByAffiliation.bDetectNeutrals = true;
	SightConfig->DetectionByAffiliation.bDetectFriendlies = false;
	PerceptionComp->ConfigureSense(*SightConfig);

	// Hearing
	HearingConfig = CreateDefaultSubobject<UAISenseConfig_Hearing>(TEXT("HearingConfig"));
	HearingConfig->HearingRange = HearingRange;
	HearingConfig->SetMaxAge(8.f);
	HearingConfig->DetectionByAffiliation.bDetectEnemies = true;
	HearingConfig->DetectionByAffiliation.bDetectNeutrals = true;
	HearingConfig->DetectionByAffiliation.bDetectFriendlies = false;
	PerceptionComp->ConfigureSense(*HearingConfig);

	// Damage
	DamageConfig = CreateDefaultSubobject<UAISenseConfig_Damage>(TEXT("DamageConfig"));
	DamageConfig->SetMaxAge(15.f);
	PerceptionComp->ConfigureSense(*DamageConfig);

	PerceptionComp->SetDominantSense(UAISenseConfig_Sight::StaticClass());
	PerceptionComp->OnTargetPerceptionInfoUpdated.AddDynamic(this, &ASHSquadAIController::OnTargetPerceptionInfoUpdated);
}

// ───────────────────────────────────────────────────────────────
//  Lifecycle
// ───────────────────────────────────────────────────────────────

void ASHSquadAIController::BeginPlay()
{
	Super::BeginPlay();
}

void ASHSquadAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	ControlledMember = Cast<ASHSquadMember>(InPawn);

	if (SquadMemberBT && SquadMemberBT->BlackboardAsset)
	{
		BlackboardComp->InitializeBlackboard(*SquadMemberBT->BlackboardAsset);
		BehaviorTreeComp->StartTree(*SquadMemberBT);
	}

	SyncBlackboard();
}

void ASHSquadAIController::OnUnPossess()
{
	BehaviorTreeComp->StopTree();
	ControlledMember = nullptr;
	Super::OnUnPossess();
}

// ───────────────────────────────────────────────────────────────
//  Tick
// ───────────────────────────────────────────────────────────────

void ASHSquadAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!ControlledMember.IsValid()) return;

	UpdateThreatList(DeltaTime);

	// Cover query cooldown
	CoverQueryCooldown -= DeltaTime;
	if (CoverQueryCooldown <= 0.f && ThreatList.Num() > 0 && !bHasValidCover)
	{
		RequestCoverPosition();
		CoverQueryCooldown = CoverQueryInterval;
	}

	// Autonomous decisions when no explicit order is active
	EvaluateAutonomousActions();

	SyncBlackboard();
}

// ───────────────────────────────────────────────────────────────
//  Perception Callbacks
// ───────────────────────────────────────────────────────────────

void ASHSquadAIController::OnPerceptionUpdated(const TArray<AActor*>& UpdatedActors)
{
	// Handled via OnTargetPerceptionInfoUpdated instead
}

void ASHSquadAIController::OnTargetPerceptionInfoUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor || !ControlledMember.IsValid()) return;

	// Route by sense type
	if (Stimulus.Type == UAISense::GetSenseID<UAISense_Sight>())
	{
		ProcessSightStimulus(Actor, Stimulus);
	}
	else if (Stimulus.Type == UAISense::GetSenseID<UAISense_Hearing>())
	{
		ProcessHearingStimulus(Actor, Stimulus);
	}
	else if (Stimulus.Type == UAISense::GetSenseID<UAISense_Damage>())
	{
		ProcessDamageStimulus(Actor, Stimulus);
	}
}

void ASHSquadAIController::ProcessSightStimulus(AActor* Actor, const FAIStimulus& Stimulus)
{
	if (!Stimulus.WasSuccessfullySensed())
	{
		// Lost sight — update existing threat entry
		for (auto& Threat : ThreatList)
		{
			if (Threat.ThreatActor.Get() == Actor)
			{
				Threat.bIsCurrentlyVisible = false;
				break;
			}
		}
		return;
	}

	// Check if already tracked
	for (auto& Threat : ThreatList)
	{
		if (Threat.ThreatActor.Get() == Actor)
		{
			Threat.bIsCurrentlyVisible = true;
			Threat.LastKnownLocation = Actor->GetActorLocation();
			Threat.TimeSinceLastSeen = 0.f;
			Threat.DistanceToUs = FVector::Dist(GetPawn()->GetActorLocation(), Actor->GetActorLocation());
			return;
		}
	}

	// New threat
	FSHThreatEntry NewThreat;
	NewThreat.ThreatActor = Actor;
	NewThreat.LastKnownLocation = Actor->GetActorLocation();
	NewThreat.bIsCurrentlyVisible = true;
	NewThreat.TimeSinceLastSeen = 0.f;
	NewThreat.DistanceToUs = FVector::Dist(GetPawn()->GetActorLocation(), Actor->GetActorLocation());
	NewThreat.ThreatScore = CalculateThreatScore(NewThreat);
	ThreatList.Add(NewThreat);

	// Contact report — update combat state
	if (CombatState == ESHAICombatState::Idle || CombatState == ESHAICombatState::Patrolling)
	{
		CombatState = ESHAICombatState::Engaging;
	}
}

void ASHSquadAIController::ProcessHearingStimulus(AActor* Actor, const FAIStimulus& Stimulus)
{
	// Hearing doesn't give precise location — use stimulus location
	bool bAlreadyTracked = false;
	for (auto& Threat : ThreatList)
	{
		if (Threat.ThreatActor.Get() == Actor)
		{
			bAlreadyTracked = true;
			// Update last known if we don't have visual
			if (!Threat.bIsCurrentlyVisible)
			{
				Threat.LastKnownLocation = Stimulus.StimulusLocation;
			}
			break;
		}
	}

	if (!bAlreadyTracked && Stimulus.WasSuccessfullySensed())
	{
		FSHThreatEntry NewThreat;
		NewThreat.ThreatActor = Actor;
		NewThreat.LastKnownLocation = Stimulus.StimulusLocation;
		NewThreat.bIsCurrentlyVisible = false;
		NewThreat.TimeSinceLastSeen = 0.f;
		NewThreat.DistanceToUs = FVector::Dist(GetPawn()->GetActorLocation(), Stimulus.StimulusLocation);
		NewThreat.ThreatScore = CalculateThreatScore(NewThreat);
		ThreatList.Add(NewThreat);
	}
}

void ASHSquadAIController::ProcessDamageStimulus(AActor* Actor, const FAIStimulus& Stimulus)
{
	// Damage = immediate high-priority threat
	for (auto& Threat : ThreatList)
	{
		if (Threat.ThreatActor.Get() == Actor)
		{
			Threat.bHasDamagedUs = true;
			Threat.ThreatScore += 50.f; // Major score bump
			Threat.TimeSinceLastSeen = 0.f;
			Threat.LastKnownLocation = Actor->GetActorLocation();
			return;
		}
	}

	// New threat from damage — highest urgency
	FSHThreatEntry NewThreat;
	NewThreat.ThreatActor = Actor;
	NewThreat.LastKnownLocation = Actor->GetActorLocation();
	NewThreat.bIsCurrentlyVisible = false;
	NewThreat.bHasDamagedUs = true;
	NewThreat.TimeSinceLastSeen = 0.f;
	NewThreat.DistanceToUs = FVector::Dist(GetPawn()->GetActorLocation(), Actor->GetActorLocation());
	NewThreat.ThreatScore = CalculateThreatScore(NewThreat) + 50.f;
	ThreatList.Add(NewThreat);

	// Immediate combat state
	CombatState = ESHAICombatState::Engaging;

	// Seek cover if not already
	if (!bHasValidCover)
	{
		TrySeekCover();
	}
}

// ───────────────────────────────────────────────────────────────
//  Threat Management
// ───────────────────────────────────────────────────────────────

void ASHSquadAIController::UpdateThreatList(float DeltaTime)
{
	if (!GetPawn()) return;

	const FVector OurLocation = GetPawn()->GetActorLocation();

	// Update and prune
	for (int32 i = ThreatList.Num() - 1; i >= 0; --i)
	{
		FSHThreatEntry& Threat = ThreatList[i];

		// Remove dead/destroyed actors
		if (!Threat.ThreatActor.IsValid())
		{
			ThreatList.RemoveAt(i);
			continue;
		}

		// Update timers
		if (!Threat.bIsCurrentlyVisible)
		{
			Threat.TimeSinceLastSeen += DeltaTime;
		}

		// Prune stale threats
		if (Threat.TimeSinceLastSeen > ThreatForgetTime)
		{
			ThreatList.RemoveAt(i);
			continue;
		}

		// Refresh distance and score
		Threat.DistanceToUs = FVector::Dist(OurLocation, Threat.ThreatActor->GetActorLocation());
		Threat.ThreatScore = CalculateThreatScore(Threat);
	}

	// Sort by threat score descending
	ThreatList.Sort([](const FSHThreatEntry& A, const FSHThreatEntry& B) {
		return A.ThreatScore > B.ThreatScore;
	});

	// Update primary threat
	PrimaryThreat = ThreatList.Num() > 0 ? ThreatList[0].ThreatActor : nullptr;
}

float ASHSquadAIController::CalculateThreatScore(const FSHThreatEntry& Entry) const
{
	float Score = 0.f;

	// Distance — closer = more threatening (inverse, normalized to 0-40 range)
	const float MaxScoreDistance = 8000.f;
	Score += FMath::Clamp(1.f - (Entry.DistanceToUs / MaxScoreDistance), 0.f, 1.f) * 40.f;

	// Visibility — visible targets are much higher priority
	if (Entry.bIsCurrentlyVisible)
	{
		Score += 30.f;
	}

	// Damage — enemies who have hit us are priority
	if (Entry.bHasDamagedUs)
	{
		Score += 25.f;
	}

	// Recency — recently seen targets are more relevant
	const float RecencyFactor = FMath::Clamp(1.f - (Entry.TimeSinceLastSeen / ThreatForgetTime), 0.f, 1.f);
	Score += RecencyFactor * 15.f;

	return Score;
}

// ───────────────────────────────────────────────────────────────
//  Order Handling
// ───────────────────────────────────────────────────────────────

void ASHSquadAIController::OnOrderReceived(const FSHSquadOrder& Order)
{
	if (!BlackboardComp || !ControlledMember.IsValid()) return;

	BlackboardComp->SetValueAsBool(SHBBKeys::HasOrder, true);
	BlackboardComp->SetValueAsEnum(SHBBKeys::OrderType, static_cast<uint8>(Order.OrderType));

	switch (Order.OrderType)
	{
	case ESHOrderType::MoveToPosition:
		CombatState = ESHAICombatState::Moving;
		BlackboardComp->SetValueAsVector(SHBBKeys::MoveToLocation, Order.TargetLocation);
		MoveToTacticalPosition(Order.TargetLocation);
		break;

	case ESHOrderType::HoldPosition:
		CombatState = ESHAICombatState::InCover;
		RequestCoverPosition();
		break;

	case ESHOrderType::Suppress:
		CombatState = ESHAICombatState::Suppressing;
		BlackboardComp->SetValueAsVector(SHBBKeys::TargetLocation, Order.TargetLocation);
		ExecuteSuppressiveFire(Order.TargetLocation);
		break;

	case ESHOrderType::FlankLeft:
	case ESHOrderType::FlankRight:
	{
		CombatState = ESHAICombatState::Flanking;
		const FVector OurPos = GetPawn()->GetActorLocation();
		const FVector ToTarget = (Order.TargetLocation - OurPos).GetSafeNormal();
		const FVector FlankDir = (Order.OrderType == ESHOrderType::FlankLeft)
			? FVector::CrossProduct(ToTarget, FVector::UpVector)
			: FVector::CrossProduct(FVector::UpVector, ToTarget);
		const FVector FlankPos = Order.TargetLocation + FlankDir * 1500.f;
		BlackboardComp->SetValueAsVector(SHBBKeys::MoveToLocation, FlankPos);
		MoveToTacticalPosition(FlankPos);
		break;
	}

	case ESHOrderType::FallBack:
		CombatState = ESHAICombatState::Retreating;
		BlackboardComp->SetValueAsVector(SHBBKeys::MoveToLocation, Order.TargetLocation);
		MoveToLocation(Order.TargetLocation, 100.f);
		break;

	case ESHOrderType::ProvideOverwatch:
		CombatState = ESHAICombatState::Overwatch;
		BlackboardComp->SetValueAsVector(SHBBKeys::TargetLocation, Order.TargetLocation);
		break;

	case ESHOrderType::CoverMe:
		CombatState = ESHAICombatState::Suppressing;
		if (PrimaryThreat.IsValid())
		{
			ExecuteSuppressiveFire(PrimaryThreat->GetActorLocation());
		}
		break;

	case ESHOrderType::BreachAndClear:
		CombatState = ESHAICombatState::Breaching;
		if (Order.TargetActor.IsValid())
		{
			ExecuteBreachProcedure(Order.TargetLocation, Order.TargetActor.Get());
		}
		break;

	case ESHOrderType::Rally:
		CombatState = ESHAICombatState::Moving;
		BlackboardComp->SetValueAsVector(SHBBKeys::MoveToLocation, Order.TargetLocation);
		MoveToLocation(Order.TargetLocation, 150.f);
		break;

	case ESHOrderType::CeaseFire:
		BlackboardComp->SetValueAsBool(SHBBKeys::ShouldFire, false);
		break;

	case ESHOrderType::FreeFire:
		BlackboardComp->SetValueAsBool(SHBBKeys::ShouldFire, true);
		break;

	case ESHOrderType::Medic:
		CombatState = ESHAICombatState::DraggingWounded;
		if (Order.TargetActor.IsValid())
		{
			BlackboardComp->SetValueAsObject(SHBBKeys::WoundedAlly, Order.TargetActor.Get());
			MoveToActor(Order.TargetActor.Get(), 100.f);
		}
		break;

	default:
		break;
	}
}

// ───────────────────────────────────────────────────────────────
//  Cover System
// ───────────────────────────────────────────────────────────────

void ASHSquadAIController::RequestCoverPosition()
{
	if (!FindCoverQuery || !GetPawn()) return;

	UEnvQueryInstanceBlueprintWrapper* QueryInstance = UEnvQueryManager::RunEQSQuery(
		this, FindCoverQuery, GetPawn(), EEnvQueryRunMode::SingleResult, nullptr);

	if (QueryInstance)
	{
		QueryInstance->GetOnQueryFinishedEvent().AddDynamic(this, &ASHSquadAIController::RequestCoverPosition);
		// Note: in production we'd bind to a proper delegate. For now, use polling in Tick.
	}

	// Fallback: manual raycast cover search
	const FVector OurPos = GetPawn()->GetActorLocation();
	FVector BestPos = FVector::ZeroVector;
	float BestScore = -1.f;

	const int32 NumSamples = 16;
	const float SearchRadius = 2000.f;

	for (int32 i = 0; i < NumSamples; ++i)
	{
		const float Angle = (2.f * PI * i) / NumSamples;
		const FVector SampleDir = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);
		const FVector SamplePos = OurPos + SampleDir * FMath::RandRange(300.f, SearchRadius);

		// Check navigability
		UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		if (!NavSys) continue;

		FNavLocation NavLoc;
		if (!NavSys->ProjectPointToNavigation(SamplePos, NavLoc, FVector(200.f, 200.f, 200.f))) continue;

		const float Score = EvaluateCoverQuality(NavLoc.Location);
		if (Score > BestScore)
		{
			BestScore = Score;
			BestPos = NavLoc.Location;
		}
	}

	if (BestScore > 0.f)
	{
		BestCoverPosition = BestPos;
		bHasValidCover = true;
		BlackboardComp->SetValueAsVector(SHBBKeys::CoverLocation, BestCoverPosition);
		BlackboardComp->SetValueAsBool(SHBBKeys::IsInCover, true);
	}
}

float ASHSquadAIController::EvaluateCoverQuality(const FVector& CoverPos) const
{
	if (!GetPawn()) return 0.f;

	float Score = 0.f;
	const FVector OurPos = GetPawn()->GetActorLocation();

	// Not too far from current position
	const float Dist = FVector::Dist(OurPos, CoverPos);
	Score += FMath::Clamp(1.f - (Dist / 2000.f), 0.f, 1.f) * 20.f;

	// Check if position blocks line of sight from threats
	UWorld* World = GetWorld();
	if (World && PrimaryThreat.IsValid())
	{
		const FVector ThreatPos = PrimaryThreat->GetActorLocation();

		// Trace from threat to cover position at crouch height
		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(GetPawn());

		const FVector CoverCrouchPos = CoverPos + FVector(0.f, 0.f, 60.f);
		const FVector CoverStandPos = CoverPos + FVector(0.f, 0.f, 170.f);

		// Good cover blocks at crouch height
		if (World->LineTraceSingleByChannel(Hit, ThreatPos, CoverCrouchPos, ECC_Visibility, Params))
		{
			if (Hit.Distance < FVector::Dist(ThreatPos, CoverCrouchPos) * 0.9f)
			{
				Score += 40.f; // Blocked at crouch = excellent cover
			}
		}

		// Bonus if we can still peek (standing exposes us)
		if (!World->LineTraceSingleByChannel(Hit, ThreatPos, CoverStandPos, ECC_Visibility, Params))
		{
			Score += 15.f; // Can peek from standing = good firing position
		}

		// Not too close to threat
		const float DistToThreat = FVector::Dist(CoverPos, ThreatPos);
		if (DistToThreat > 500.f)
		{
			Score += FMath::Clamp(DistToThreat / 3000.f, 0.f, 1.f) * 10.f;
		}
	}

	return Score;
}

bool ASHSquadAIController::IsPositionInCover(const FVector& Position) const
{
	if (!GetWorld() || !PrimaryThreat.IsValid()) return false;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetPawn());

	const FVector CrouchPos = Position + FVector(0.f, 0.f, 60.f);
	return GetWorld()->LineTraceSingleByChannel(
		Hit, PrimaryThreat->GetActorLocation(), CrouchPos, ECC_Visibility, Params);
}

// ───────────────────────────────────────────────────────────────
//  Tactical Movement
// ───────────────────────────────────────────────────────────────

bool ASHSquadAIController::MoveToTacticalPosition(const FVector& Destination)
{
	if (!GetPawn()) return false;

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys) return false;

	FNavLocation NavLoc;
	if (!NavSys->ProjectPointToNavigation(Destination, NavLoc, FVector(500.f, 500.f, 500.f)))
	{
		return false;
	}

	MoveToLocation(NavLoc.Location, 50.f, true, true, true, true);
	return true;
}

void ASHSquadAIController::ExecuteBoundingOverwatch(const FVector& Destination)
{
	// Bounding overwatch: move in short bounds while buddy covers
	if (!ControlledMember.IsValid() || !GetPawn()) return;

	const FVector OurPos = GetPawn()->GetActorLocation();
	const FVector ToDestination = Destination - OurPos;
	const float TotalDist = ToDestination.Size();

	// Bound distance: ~50m (5000cm) at a time
	const float BoundDist = FMath::Min(5000.f, TotalDist);
	const FVector BoundTarget = OurPos + ToDestination.GetSafeNormal() * BoundDist;

	CombatState = ESHAICombatState::Moving;
	MoveToTacticalPosition(BoundTarget);
}

// ───────────────────────────────────────────────────────────────
//  Engagement
// ───────────────────────────────────────────────────────────────

AActor* ASHSquadAIController::SelectBestTarget()
{
	if (ThreatList.Num() == 0) return nullptr;

	// Already sorted by threat score in UpdateThreatList
	// Prefer visible targets
	for (const auto& Threat : ThreatList)
	{
		if (Threat.bIsCurrentlyVisible && Threat.ThreatActor.IsValid())
		{
			PrimaryThreat = Threat.ThreatActor;
			return Threat.ThreatActor.Get();
		}
	}

	// Fall back to highest scored threat
	if (ThreatList[0].ThreatActor.IsValid())
	{
		PrimaryThreat = ThreatList[0].ThreatActor;
		return ThreatList[0].ThreatActor.Get();
	}

	return nullptr;
}

bool ASHSquadAIController::ShouldEngageTarget(AActor* Target) const
{
	if (!Target || !GetPawn()) return false;

	const float Dist = FVector::Dist(GetPawn()->GetActorLocation(), Target->GetActorLocation());

	// Don't engage beyond effective range (~600m for rifles)
	if (Dist > 60000.f) return false;

	// Don't engage if heavily suppressed (self-preservation)
	if (ShouldBreakForCover()) return false;

	return true;
}

void ASHSquadAIController::ExecuteSuppressiveFire(const FVector& TargetLocation)
{
	CombatState = ESHAICombatState::Suppressing;

	if (BlackboardComp)
	{
		BlackboardComp->SetValueAsVector(SHBBKeys::TargetLocation, TargetLocation);
		BlackboardComp->SetValueAsBool(SHBBKeys::ShouldFire, true);
	}

	// Set burst fire parameters for suppression (longer bursts, less accurate)
	BurstRoundsRemaining = FMath::RandRange(5, 12);
	BurstFireRate = 0.1f;
}

void ASHSquadAIController::ExecuteBreachProcedure(const FVector& EntryPoint, AActor* DoorActor)
{
	CombatState = ESHAICombatState::Breaching;

	if (BlackboardComp)
	{
		BlackboardComp->SetValueAsVector(SHBBKeys::MoveToLocation, EntryPoint);
	}

	// Move to stack-up position offset from entry
	const FVector ApproachDir = (EntryPoint - GetPawn()->GetActorLocation()).GetSafeNormal();
	const FVector StackUpPos = EntryPoint - ApproachDir * 200.f;
	MoveToLocation(StackUpPos, 50.f);
}

// ───────────────────────────────────────────────────────────────
//  Self-Preservation
// ───────────────────────────────────────────────────────────────

float ASHSquadAIController::GetSelfPreservationWeight() const
{
	float Weight = 0.f;

	if (!ControlledMember.IsValid()) return 0.5f;

	// Health factor — lower health = higher self-preservation
	// ASHSquadMember should expose health. Use a reasonable estimate.
	Weight += 0.3f; // Base self-preservation

	// Suppression factor
	const bool bIsSuppressed = BlackboardComp ?
		BlackboardComp->GetValueAsBool(SHBBKeys::IsSuppressed) : false;
	if (bIsSuppressed)
	{
		Weight += 0.3f;
	}

	// Ammo factor
	const bool bAmmoLow = BlackboardComp ?
		BlackboardComp->GetValueAsBool(SHBBKeys::AmmoLow) : false;
	if (bAmmoLow)
	{
		Weight += 0.15f;
	}

	// Wounded factor
	const bool bWounded = BlackboardComp ?
		BlackboardComp->GetValueAsBool(SHBBKeys::IsWounded) : false;
	if (bWounded)
	{
		Weight += 0.2f;
	}

	return FMath::Clamp(Weight, 0.f, 1.f);
}

bool ASHSquadAIController::ShouldBreakForCover() const
{
	return GetSelfPreservationWeight() > 0.7f && !bHasValidCover;
}

bool ASHSquadAIController::ShouldReloadNow() const
{
	if (!BlackboardComp) return false;

	const bool bNeedsReload = BlackboardComp->GetValueAsBool(SHBBKeys::NeedsReload);
	if (!bNeedsReload) return false;

	// Prefer reloading in cover
	if (bHasValidCover && IsPositionInCover(GetPawn()->GetActorLocation()))
	{
		return true;
	}

	// Reload anyway if no threats visible
	bool bAnyVisible = false;
	for (const auto& Threat : ThreatList)
	{
		if (Threat.bIsCurrentlyVisible)
		{
			bAnyVisible = true;
			break;
		}
	}

	return !bAnyVisible;
}

// ───────────────────────────────────────────────────────────────
//  Autonomous Actions
// ───────────────────────────────────────────────────────────────

void ASHSquadAIController::EvaluateAutonomousActions()
{
	if (!ControlledMember.IsValid()) return;

	const bool bHasOrder = BlackboardComp ?
		BlackboardComp->GetValueAsBool(SHBBKeys::HasOrder) : false;

	// If we have an explicit order, mostly follow it
	// But override for self-preservation
	if (ShouldBreakForCover())
	{
		TrySeekCover();
		return;
	}

	if (ShouldReloadNow())
	{
		CombatState = ESHAICombatState::InCover;
		BlackboardComp->SetValueAsBool(SHBBKeys::NeedsReload, true);
		return;
	}

	// If no orders, act autonomously
	if (!bHasOrder)
	{
		if (ThreatList.Num() > 0)
		{
			// Under threat — find cover and return fire
			if (!bHasValidCover)
			{
				TrySeekCover();
			}
			TryReturnFire();
		}
	}
}

void ASHSquadAIController::TryReturnFire()
{
	AActor* Target = SelectBestTarget();
	if (Target && ShouldEngageTarget(Target))
	{
		CombatState = ESHAICombatState::Engaging;
		if (BlackboardComp)
		{
			BlackboardComp->SetValueAsObject(SHBBKeys::CurrentTarget, Target);
			BlackboardComp->SetValueAsBool(SHBBKeys::ShouldFire, true);
			BlackboardComp->SetValueAsBool(SHBBKeys::CanSeeEnemy, true);
		}
	}
}

void ASHSquadAIController::TrySeekCover()
{
	if (bHasValidCover)
	{
		// Move to known cover
		CombatState = ESHAICombatState::Moving;
		MoveToTacticalPosition(BestCoverPosition);
	}
	else
	{
		// Request new cover search
		RequestCoverPosition();
	}
}

// ───────────────────────────────────────────────────────────────
//  EQS Callback
// ───────────────────────────────────────────────────────────────

void ASHSquadAIController::OnCoverQueryFinished(TSharedPtr<FEnvQueryResult> Result)
{
	if (!Result.IsValid() || !Result->IsSuccessful()) return;

	const FVector ResultLocation = Result->GetItemAsLocation(0);
	BestCoverPosition = ResultLocation;
	bHasValidCover = true;

	if (BlackboardComp)
	{
		BlackboardComp->SetValueAsVector(SHBBKeys::CoverLocation, BestCoverPosition);
	}
}

// ───────────────────────────────────────────────────────────────
//  Blackboard Sync
// ───────────────────────────────────────────────────────────────

void ASHSquadAIController::SyncBlackboard()
{
	if (!BlackboardComp) return;

	BlackboardComp->SetValueAsEnum(SHBBKeys::CombatState, static_cast<uint8>(CombatState));
	BlackboardComp->SetValueAsInt(SHBBKeys::ThreatCount, ThreatList.Num());
	BlackboardComp->SetValueAsBool(SHBBKeys::IsInCover,
		bHasValidCover && IsPositionInCover(GetPawn() ? GetPawn()->GetActorLocation() : FVector::ZeroVector));

	if (PrimaryThreat.IsValid())
	{
		BlackboardComp->SetValueAsObject(SHBBKeys::CurrentTarget, PrimaryThreat.Get());
		BlackboardComp->SetValueAsVector(SHBBKeys::TargetLocation, PrimaryThreat->GetActorLocation());
		BlackboardComp->SetValueAsBool(SHBBKeys::CanSeeEnemy, ThreatList.Num() > 0 && ThreatList[0].bIsCurrentlyVisible);
	}
	else
	{
		BlackboardComp->ClearValue(SHBBKeys::CurrentTarget);
		BlackboardComp->SetValueAsBool(SHBBKeys::CanSeeEnemy, false);
	}

	if (bHasValidCover)
	{
		BlackboardComp->SetValueAsVector(SHBBKeys::CoverLocation, BestCoverPosition);
	}
}
