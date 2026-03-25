// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHEnemyAIController.h"
#include "SHEnemyCharacter.h"
#include "SHAIPerceptionConfig.h"
#include "Primordia/SHPrimordiaDecisionEngine.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Perception/AIPerceptionComponent.h"
#include "Perception/AISenseConfig_Sight.h"
#include "Perception/AISenseConfig_Hearing.h"
#include "Perception/AISenseConfig_Damage.h"
#include "NavigationSystem.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/KismetMathLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogSHEnemyAI, Log, All);

// -----------------------------------------------------------------------
//  Constructor
// -----------------------------------------------------------------------

ASHEnemyAIController::ASHEnemyAIController(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.1f; // 10Hz AI tick for enemies (perf budget)

	BehaviorTreeComp = CreateDefaultSubobject<UBehaviorTreeComponent>(TEXT("BehaviorTreeComp"));
	BlackboardComp = CreateDefaultSubobject<UBlackboardComponent>(TEXT("BlackboardComp"));
	AIPerceptionComp = CreateDefaultSubobject<UAIPerceptionComponent>(TEXT("AIPerceptionComp"));

	SetPerceptionComponent(*AIPerceptionComp);
}

// -----------------------------------------------------------------------
//  Lifecycle
// -----------------------------------------------------------------------

void ASHEnemyAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);

	EnemyCharacter = Cast<ASHEnemyCharacter>(InPawn);
	if (!EnemyCharacter)
	{
		UE_LOG(LogSHEnemyAI, Error, TEXT("OnPossess: pawn is not ASHEnemyCharacter"));
		return;
	}

	// Cache perception config
	PerceptionConfig = EnemyCharacter->FindComponentByClass<USHAIPerceptionConfig>();

	// Setup perception senses
	{
		UAISenseConfig_Sight* SightCfg = NewObject<UAISenseConfig_Sight>(this);
		const float BaseSightRange = PerceptionConfig ? PerceptionConfig->GetSightRange(GetCurrentVisibilityCondition()) : 6000.f;
		SightCfg->SightRadius = BaseSightRange;
		SightCfg->LoseSightRadius = BaseSightRange * 1.25f;
		SightCfg->PeripheralVisionAngleDegrees = 70.f;
		SightCfg->SetMaxAge(8.f);
		SightCfg->AutoSuccessRangeFromLastSeenLocation = 400.f;
		SightCfg->DetectionByAffiliation.bDetectEnemies = true;
		SightCfg->DetectionByAffiliation.bDetectNeutrals = true;
		SightCfg->DetectionByAffiliation.bDetectFriendlies = false;
		AIPerceptionComp->ConfigureSense(*SightCfg);

		UAISenseConfig_Hearing* HearCfg = NewObject<UAISenseConfig_Hearing>(this);
		HearCfg->HearingRange = PerceptionConfig ? PerceptionConfig->GetHearingRange(ESHSoundType::Gunfire) : 8000.f;
		HearCfg->SetMaxAge(10.f);
		HearCfg->DetectionByAffiliation.bDetectEnemies = true;
		HearCfg->DetectionByAffiliation.bDetectNeutrals = true;
		HearCfg->DetectionByAffiliation.bDetectFriendlies = false;
		AIPerceptionComp->ConfigureSense(*HearCfg);

		UAISenseConfig_Damage* DmgCfg = NewObject<UAISenseConfig_Damage>(this);
		DmgCfg->SetMaxAge(20.f);
		AIPerceptionComp->ConfigureSense(*DmgCfg);

		AIPerceptionComp->SetDominantSense(UAISenseConfig_Sight::StaticClass());
	}

	// Bind perception
	AIPerceptionComp->OnTargetPerceptionInfoUpdated.AddDynamic(this, &ASHEnemyAIController::OnPerceptionUpdated);

	// Start behavior tree
	if (BehaviorTreeAsset && BehaviorTreeAsset->BlackboardAsset)
	{
		BlackboardComp->InitializeBlackboard(*BehaviorTreeAsset->BlackboardAsset);
		BehaviorTreeComp->StartTree(*BehaviorTreeAsset);
	}

	AwarenessState = ESHAwarenessState::Unaware;
	CurrentBehavior = ESHCombatBehavior::Patrolling;

	UE_LOG(LogSHEnemyAI, Log, TEXT("OnPossess: %s ready"), *InPawn->GetName());
}

void ASHEnemyAIController::OnUnPossess()
{
	BehaviorTreeComp->StopTree();
	EnemyCharacter = nullptr;
	PerceptionConfig = nullptr;
	KnownThreats.Empty();
	DetectionAccumulation.Empty();
	Super::OnUnPossess();
}

// -----------------------------------------------------------------------
//  Tick
// -----------------------------------------------------------------------

void ASHEnemyAIController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!EnemyCharacter) return;

	UpdateAwarenessState(DeltaSeconds);
	TickCoverLogic(DeltaSeconds);
	TickTacticalOrder(DeltaSeconds);

	// Fire position timer — don't stay in one spot
	if (CurrentBehavior == ESHCombatBehavior::FiringFromCover)
	{
		TimeInFirePosition += DeltaSeconds;
		if (TimeInFirePosition > MaxTimeInFirePosition)
		{
			ChangeFirePosition();
			TimeInFirePosition = 0.f;
		}
	}

	// Evaluate best behavior
	const ESHCombatBehavior NewBehavior = EvaluateBestBehavior();
	if (NewBehavior != CurrentBehavior)
	{
		CurrentBehavior = NewBehavior;
	}

	// Prune dead threats
	KnownThreats.RemoveAll([](const TWeakObjectPtr<AActor>& T) { return !T.IsValid(); });

	UpdateBlackboard();
}

void ASHEnemyAIController::UpdateControlRotation(float DeltaTime, bool bUpdatePawn)
{
	APawn* MyPawn = GetPawn();
	if (!MyPawn) return;

	FRotator CurrentRotation = GetControlRotation();

	// If engaging, smoothly rotate toward target
	if (AwarenessState == ESHAwarenessState::Combat)
	{
		AActor* Target = GetPrimaryTarget();
		if (Target)
		{
			const FVector TargetLoc = Target->GetActorLocation();
			const FRotator DesiredRot = UKismetMathLibrary::FindLookAtRotation(
				MyPawn->GetActorLocation(), TargetLoc);
			const FRotator SmoothedRot = FMath::RInterpTo(CurrentRotation, DesiredRot, DeltaTime, 8.f);
			SetControlRotation(SmoothedRot);
			if (bUpdatePawn) MyPawn->FaceRotation(SmoothedRot, DeltaTime);
			return;
		}
	}

	// If investigating, rotate toward last known location
	if (AwarenessState == ESHAwarenessState::Suspicious || AwarenessState == ESHAwarenessState::Alert)
	{
		if (!LastKnownThreatLocation.IsNearlyZero())
		{
			const FRotator DesiredRot = UKismetMathLibrary::FindLookAtRotation(
				MyPawn->GetActorLocation(), LastKnownThreatLocation);
			const FRotator SmoothedRot = FMath::RInterpTo(CurrentRotation, DesiredRot, DeltaTime, 3.f);
			SetControlRotation(SmoothedRot);
			if (bUpdatePawn) MyPawn->FaceRotation(SmoothedRot, DeltaTime);
			return;
		}
	}

	Super::UpdateControlRotation(DeltaTime, bUpdatePawn);
}

// -----------------------------------------------------------------------
//  Perception
// -----------------------------------------------------------------------

void ASHEnemyAIController::OnPerceptionUpdated(AActor* Actor, FAIStimulus Stimulus)
{
	if (!Actor) return;

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

void ASHEnemyAIController::ProcessSightStimulus(AActor* Actor, const FAIStimulus& Stimulus)
{
	if (!Stimulus.WasSuccessfullySensed())
	{
		// Lost sight — don't remove from threats, just mark last known
		LastKnownThreatLocation = Stimulus.StimulusLocation;
		return;
	}

	// Gradual detection accumulation (not instant)
	float& Accumulation = DetectionAccumulation.FindOrAdd(Actor, 0.f);

	const float Distance = FVector::Dist(GetPawn()->GetActorLocation(), Actor->GetActorLocation());
	const float MaxSight = AIPerceptionComp->GetMaxSightRange();
	const float DistanceFactor = FMath::Clamp(1.f - (Distance / FMath::Max(MaxSight, 1.f)), 0.1f, 1.f);

	// Visibility conditions affect detection speed
	float ConditionMultiplier = 1.f;
	const ESHVisibilityCondition Condition = GetCurrentVisibilityCondition();
	switch (Condition)
	{
	case ESHVisibilityCondition::Night:       ConditionMultiplier = 0.3f; break;
	case ESHVisibilityCondition::Fog:         ConditionMultiplier = 0.4f; break;
	case ESHVisibilityCondition::Smoke:       ConditionMultiplier = 0.2f; break;
	case ESHVisibilityCondition::Rain:        ConditionMultiplier = 0.6f; break;
	case ESHVisibilityCondition::DawnDusk:    ConditionMultiplier = 0.7f; break;
	default:                                  ConditionMultiplier = 1.f;  break;
	}

	Accumulation += PrimaryActorTick.TickInterval * DistanceFactor * ConditionMultiplier;

	// Threshold to confirm detection
	const float DetectionThreshold = 0.5f; // Half second at optimal conditions
	if (Accumulation >= DetectionThreshold)
	{
		if (!KnownThreats.Contains(Actor))
		{
			KnownThreats.Add(Actor);

			// Cap tracked threats
			if (KnownThreats.Num() > MaxTrackedTargets)
			{
				KnownThreats.RemoveAt(0);
			}
		}

		LastKnownThreatLocation = Actor->GetActorLocation();

		// Escalate awareness
		if (AwarenessState < ESHAwarenessState::Combat)
		{
			AwarenessState = ESHAwarenessState::Combat;
		}
	}
	else if (Accumulation > DetectionThreshold * 0.3f)
	{
		// Partial detection — become suspicious
		if (AwarenessState < ESHAwarenessState::Suspicious)
		{
			AwarenessState = ESHAwarenessState::Suspicious;
			LastKnownThreatLocation = Stimulus.StimulusLocation;
		}
	}
}

void ASHEnemyAIController::ProcessHearingStimulus(AActor* Actor, const FAIStimulus& Stimulus)
{
	if (!Stimulus.WasSuccessfullySensed()) return;

	LastKnownThreatLocation = Stimulus.StimulusLocation;

	// Hearing immediately raises suspicion, but not full combat
	if (AwarenessState < ESHAwarenessState::Alert)
	{
		AwarenessState = ESHAwarenessState::Alert;
	}
}

void ASHEnemyAIController::ProcessDamageStimulus(AActor* Actor, const FAIStimulus& Stimulus)
{
	// Damage = instant combat awareness
	AwarenessState = ESHAwarenessState::Combat;
	AwarenessCooldownTimer = 0.f;

	if (Actor && !KnownThreats.Contains(Actor))
	{
		KnownThreats.Add(Actor);
	}

	if (Actor)
	{
		LastKnownThreatLocation = Actor->GetActorLocation();
		// Attacker becomes primary target
		SetPrimaryTarget(Actor);
	}

	// Immediate response: seek cover
	if (GetPawn())
	{
		FindAndMoveToCover(LastKnownThreatLocation);
	}
}

// -----------------------------------------------------------------------
//  Awareness State
// -----------------------------------------------------------------------

void ASHEnemyAIController::UpdateAwarenessState(float DeltaSeconds)
{
	// Decay awareness over time if no stimuli
	if (KnownThreats.Num() == 0)
	{
		AwarenessCooldownTimer += DeltaSeconds;

		if (AwarenessState == ESHAwarenessState::Combat && AwarenessCooldownTimer > 10.f)
		{
			AwarenessState = ESHAwarenessState::Searching;
			AwarenessCooldownTimer = 0.f;
		}
		else if (AwarenessState == ESHAwarenessState::Searching && AwarenessCooldownTimer > SearchDuration)
		{
			AwarenessState = ESHAwarenessState::Alert;
			AwarenessCooldownTimer = 0.f;
		}
		else if (AwarenessState == ESHAwarenessState::Alert && AwarenessCooldownTimer > 20.f)
		{
			AwarenessState = ESHAwarenessState::Suspicious;
			AwarenessCooldownTimer = 0.f;
		}
		else if (AwarenessState == ESHAwarenessState::Suspicious && AwarenessCooldownTimer > 30.f)
		{
			AwarenessState = ESHAwarenessState::Unaware;
			AwarenessCooldownTimer = 0.f;
			DetectionAccumulation.Empty();
		}
	}
	else
	{
		AwarenessCooldownTimer = 0.f;
	}

	// Decay detection accumulation for actors we haven't seen recently
	TArray<TWeakObjectPtr<AActor>> ToRemove;
	for (auto& Pair : DetectionAccumulation)
	{
		if (!Pair.Key.IsValid())
		{
			ToRemove.Add(Pair.Key);
			continue;
		}

		// If not in known threats and accumulation partial, decay
		if (!KnownThreats.Contains(Pair.Key.Get()))
		{
			Pair.Value -= DeltaSeconds * 0.2f;
			if (Pair.Value <= 0.f)
			{
				ToRemove.Add(Pair.Key);
			}
		}
	}
	for (const auto& Key : ToRemove)
	{
		DetectionAccumulation.Remove(Key);
	}
}

ESHVisibilityCondition ASHEnemyAIController::GetCurrentVisibilityCondition() const
{
	// Query from game state weather/time if available
	// Fallback to clear daylight
	return ESHVisibilityCondition::Daylight;
}

// -----------------------------------------------------------------------
//  Target Management
// -----------------------------------------------------------------------

AActor* ASHEnemyAIController::GetPrimaryTarget() const
{
	if (BlackboardComp)
	{
		return Cast<AActor>(BlackboardComp->GetValueAsObject(SHBBKeys::TargetActor));
	}
	return nullptr;
}

void ASHEnemyAIController::SetPrimaryTarget(AActor* NewTarget)
{
	if (BlackboardComp)
	{
		BlackboardComp->SetValueAsObject(SHBBKeys::TargetActor, NewTarget);
		if (NewTarget)
		{
			BlackboardComp->SetValueAsVector(SHBBKeys::TargetLocation, NewTarget->GetActorLocation());
		}
	}
}

AActor* ASHEnemyAIController::SelectBestTarget() const
{
	AActor* Best = nullptr;
	float BestScore = -1.f;

	for (const auto& Threat : KnownThreats)
	{
		if (!Threat.IsValid()) continue;

		const float Score = ScoreTarget(Threat.Get());
		if (Score > BestScore)
		{
			BestScore = Score;
			Best = Threat.Get();
		}
	}

	return Best;
}

float ASHEnemyAIController::ScoreTarget(AActor* Target) const
{
	if (!Target || !GetPawn()) return 0.f;

	float Score = 0.f;
	const float Dist = FVector::Dist(GetPawn()->GetActorLocation(), Target->GetActorLocation());

	// Distance — closer is higher priority
	Score += FMath::Clamp(1.f - (Dist / 10000.f), 0.f, 1.f) * 40.f;

	// Line of sight check
	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetPawn());
	if (!GetWorld()->LineTraceSingleByChannel(Hit, GetPawn()->GetActorLocation() + FVector(0, 0, 60),
		Target->GetActorLocation() + FVector(0, 0, 60), ECC_Visibility, Params))
	{
		Score += 30.f; // Can see target
	}

	// Threat level — targets that damaged us are higher priority
	// Check if this target is in our damage history
	if (DetectionAccumulation.Contains(Target))
	{
		Score += 15.f;
	}

	return Score;
}

// -----------------------------------------------------------------------
//  Tactical Orders (Primordia)
// -----------------------------------------------------------------------

void ASHEnemyAIController::ReceiveTacticalOrder(const FSHTacticalOrder& Order)
{
	bHasTacticalOrder = true;
	CurrentOrderType = Order.OrderType;
	CurrentOrderTarget = Order.TargetLocation;
	TacticalOrderElapsed = 0.f;

	CurrentBehavior = ESHCombatBehavior::ExecutingOrder;

	if (BlackboardComp)
	{
		BlackboardComp->SetValueAsBool(SHBBKeys::HasTacticalOrder, true);
		BlackboardComp->SetValueAsString(SHBBKeys::TacticalOrderType, CurrentOrderType);
		BlackboardComp->SetValueAsVector(SHBBKeys::TacticalOrderTarget, CurrentOrderTarget);
	}

	UE_LOG(LogSHEnemyAI, Log, TEXT("%s received tactical order: %s"),
		*GetPawn()->GetName(), *CurrentOrderType);

	// Execute based on order type
	if (CurrentOrderType == TEXT("Assault"))
	{
		MoveToLocation(CurrentOrderTarget, 200.f, true, true, true);
	}
	else if (CurrentOrderType == TEXT("Flank"))
	{
		AttemptFlank(CurrentOrderTarget);
	}
	else if (CurrentOrderType == TEXT("Suppress"))
	{
		OrderSuppressiveFire(CurrentOrderTarget, 10.f);
	}
	else if (CurrentOrderType == TEXT("Retreat"))
	{
		BeginRetreat(CurrentOrderTarget);
	}
	else if (CurrentOrderType == TEXT("Hold"))
	{
		FindAndMoveToCover(CurrentOrderTarget);
	}
}

void ASHEnemyAIController::ClearTacticalOrder()
{
	bHasTacticalOrder = false;
	CurrentOrderType = TEXT("");
	TacticalOrderElapsed = 0.f;

	if (BlackboardComp)
	{
		BlackboardComp->SetValueAsBool(SHBBKeys::HasTacticalOrder, false);
	}
}

void ASHEnemyAIController::TickTacticalOrder(float DeltaSeconds)
{
	if (!bHasTacticalOrder) return;

	TacticalOrderElapsed += DeltaSeconds;

	// Orders timeout after 60 seconds
	if (TacticalOrderElapsed > 60.f)
	{
		UE_LOG(LogSHEnemyAI, Log, TEXT("%s: tactical order timed out"), *GetPawn()->GetName());
		ClearTacticalOrder();
	}
}

// -----------------------------------------------------------------------
//  Combat Behaviors
// -----------------------------------------------------------------------

ESHCombatBehavior ASHEnemyAIController::EvaluateBestBehavior() const
{
	// Primordia order overrides local decision
	if (bHasTacticalOrder)
	{
		return ESHCombatBehavior::ExecutingOrder;
	}

	// Check morale — broken units retreat
	// (would query morale component on EnemyCharacter)

	// No threats — patrol or investigate
	if (KnownThreats.Num() == 0)
	{
		switch (AwarenessState)
		{
		case ESHAwarenessState::Unaware:    return ESHCombatBehavior::Patrolling;
		case ESHAwarenessState::Suspicious: return ESHCombatBehavior::Investigating;
		case ESHAwarenessState::Alert:      return ESHCombatBehavior::Investigating;
		case ESHAwarenessState::Searching:  return ESHCombatBehavior::Searching;
		default:                            return ESHCombatBehavior::Patrolling;
		}
	}

	// In combat — evaluate situation

	// Check suppression from blackboard
	const float Suppression = BlackboardComp ?
		BlackboardComp->GetValueAsFloat(SHBBKeys::SuppressionLevel) : 0.f;

	// Heavily suppressed — seek cover
	if (Suppression > 0.7f)
	{
		return ESHCombatBehavior::SeekingCover;
	}

	// Grenade opportunity
	if (EvaluateGrenadeUsage())
	{
		return ESHCombatBehavior::ThrowingGrenade;
	}

	// Default — fire from cover
	return ESHCombatBehavior::FiringFromCover;
}

bool ASHEnemyAIController::FindAndMoveToCover(FVector ThreatLocation)
{
	FVector CoverPoint;
	if (FindCoverPoint(ThreatLocation, CoverPoint))
	{
		MoveToLocation(CoverPoint, 50.f, true, true, true);
		CurrentBehavior = ESHCombatBehavior::SeekingCover;

		if (BlackboardComp)
		{
			BlackboardComp->SetValueAsVector(SHBBKeys::CoverLocation, CoverPoint);
		}
		return true;
	}
	return false;
}

bool ASHEnemyAIController::FindCoverPoint(FVector ThreatLocation, FVector& OutCoverPoint) const
{
	if (!GetPawn() || !GetWorld()) return false;

	const FVector OurPos = GetPawn()->GetActorLocation();
	const FVector ThreatDir = (ThreatLocation - OurPos).GetSafeNormal();

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys) return false;

	float BestScore = -1.f;
	FVector BestPos = FVector::ZeroVector;

	// Sample positions in a hemisphere away from threat
	const int32 NumSamples = 24;
	for (int32 i = 0; i < NumSamples; ++i)
	{
		const float Angle = (2.f * PI * i) / NumSamples;
		const FVector SampleDir = FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f);

		// Bias toward positions away from threat
		const float DotWithThreat = FVector::DotProduct(SampleDir, -ThreatDir);
		if (DotWithThreat < -0.3f) continue; // Skip positions toward threat

		const float SampleDist = FMath::RandRange(CoverSearchRadius * 0.3f, CoverSearchRadius);
		const FVector SamplePos = OurPos + SampleDir * SampleDist;

		FNavLocation NavLoc;
		if (!NavSys->ProjectPointToNavigation(SamplePos, NavLoc, FVector(200.f))) continue;

		// Must be far enough from threat
		if (FVector::Dist(NavLoc.Location, ThreatLocation) < MinCoverDistFromThreat) continue;

		// Evaluate cover quality via raycasts
		float Score = 0.f;

		FHitResult Hit;
		FCollisionQueryParams Params;
		Params.AddIgnoredActor(GetPawn());

		// Check if position is blocked from threat at crouch height
		const FVector TestPos = NavLoc.Location + FVector(0, 0, 60.f);
		if (GetWorld()->LineTraceSingleByChannel(Hit, ThreatLocation, TestPos, ECC_Visibility, Params))
		{
			Score += 50.f; // Blocked from threat = good cover
		}

		// Closer to us is better (less exposure during movement)
		const float DistToUs = FVector::Dist(OurPos, NavLoc.Location);
		Score += FMath::Clamp(1.f - (DistToUs / CoverSearchRadius), 0.f, 1.f) * 20.f;

		// Lateral displacement from threat axis is good (harder for enemy to track)
		const float LateralDot = FMath::Abs(FVector::DotProduct(
			(NavLoc.Location - OurPos).GetSafeNormal(),
			FVector::CrossProduct(ThreatDir, FVector::UpVector)));
		Score += LateralDot * 15.f;

		if (Score > BestScore)
		{
			BestScore = Score;
			BestPos = NavLoc.Location;
		}
	}

	if (BestScore > 0.f)
	{
		OutCoverPoint = BestPos;
		return true;
	}
	return false;
}

bool ASHEnemyAIController::AttemptFlank(FVector EnemyLocation)
{
	const FVector FlankPos = ComputeFlankingPosition(EnemyLocation);

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!NavSys) return false;

	FNavLocation NavLoc;
	if (!NavSys->ProjectPointToNavigation(FlankPos, NavLoc, FVector(500.f))) return false;

	CurrentBehavior = ESHCombatBehavior::Flanking;
	MoveToLocation(NavLoc.Location, 100.f, true, true, true);

	if (BlackboardComp)
	{
		BlackboardComp->SetValueAsVector(SHBBKeys::FlankingTarget, NavLoc.Location);
	}

	return true;
}

FVector ASHEnemyAIController::ComputeFlankingPosition(FVector EnemyLocation) const
{
	if (!GetPawn()) return EnemyLocation;

	const FVector OurPos = GetPawn()->GetActorLocation();
	const FVector ToEnemy = (EnemyLocation - OurPos).GetSafeNormal();

	// 90 degrees perpendicular, randomly left or right
	const FVector Perpendicular = FMath::RandBool()
		? FVector::CrossProduct(ToEnemy, FVector::UpVector)
		: FVector::CrossProduct(FVector::UpVector, ToEnemy);

	// Flank position: perpendicular + forward toward enemy
	const float FlankDistance = 2000.f;
	return OurPos + Perpendicular * FlankDistance + ToEnemy * FlankDistance * 0.5f;
}

void ASHEnemyAIController::OrderSuppressiveFire(FVector TargetLocation, float Duration)
{
	CurrentBehavior = ESHCombatBehavior::Suppressing;

	if (BlackboardComp)
	{
		BlackboardComp->SetValueAsVector(SHBBKeys::TargetLocation, TargetLocation);
	}

	// Duration managed by behavior tree
}

bool ASHEnemyAIController::EvaluateGrenadeUsage()
{
	if (!GetPawn()) return false;

	AActor* Target = GetPrimaryTarget();
	if (!Target) return false;

	const float Dist = FVector::Dist(GetPawn()->GetActorLocation(), Target->GetActorLocation());

	// Must be in throw range
	if (Dist > GrenadeThrowRange || Dist < 500.f) return false;

	// Don't use grenades too frequently
	// (would check a grenade cooldown timer)

	// Evaluate if smoke would be better
	if (ShouldUseSmokeGrenade())
	{
		// Use smoke to break contact or advance
		if (BlackboardComp)
		{
			BlackboardComp->SetValueAsBool(SHBBKeys::ShouldUseGrenade, true);
			BlackboardComp->SetValueAsVector(SHBBKeys::GrenadeTarget, Target->GetActorLocation());
		}
		return true;
	}

	// Frag grenade — enemy must be in a static position (behind cover)
	// Check if target hasn't moved much recently
	if (BlackboardComp)
	{
		BlackboardComp->SetValueAsBool(SHBBKeys::ShouldUseGrenade, true);
		BlackboardComp->SetValueAsVector(SHBBKeys::GrenadeTarget, Target->GetActorLocation());
	}
	return true;
}

bool ASHEnemyAIController::ShouldUseSmokeGrenade() const
{
	// Use smoke when heavily suppressed and need to break contact or advance
	const float Suppression = BlackboardComp ?
		BlackboardComp->GetValueAsFloat(SHBBKeys::SuppressionLevel) : 0.f;
	return Suppression > 0.6f;
}

void ASHEnemyAIController::BeginRetreat(FVector RallyPoint)
{
	CurrentBehavior = ESHCombatBehavior::Retreating;
	AwarenessState = ESHAwarenessState::Combat; // Still aware, just falling back

	MoveToLocation(RallyPoint, 200.f, true, true, true);
	UE_LOG(LogSHEnemyAI, Log, TEXT("%s retreating to rally point"), *GetPawn()->GetName());
}

bool ASHEnemyAIController::ChangeFirePosition()
{
	if (!GetPawn()) return false;

	// Find a new cover position different from current
	FVector NewCover;
	if (FindCoverPoint(LastKnownThreatLocation, NewCover))
	{
		MoveToLocation(NewCover, 50.f, true, true, true);
		CurrentBehavior = ESHCombatBehavior::SeekingCover;

		if (BlackboardComp)
		{
			BlackboardComp->SetValueAsVector(SHBBKeys::CoverLocation, NewCover);
		}
		return true;
	}
	return false;
}

// -----------------------------------------------------------------------
//  Squad Coordination
// -----------------------------------------------------------------------

void ASHEnemyAIController::ShareTargetWithSquad(AActor* Target)
{
	if (!Target || !GetPawn()) return;

	const float ShareRange = 5000.f;

	// Find nearby enemy AI controllers
	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		ASHEnemyAIController* Other = Cast<ASHEnemyAIController>(It->Get());
		if (!Other || Other == this || !Other->GetPawn()) continue;

		const float Dist = FVector::Dist(GetPawn()->GetActorLocation(), Other->GetPawn()->GetActorLocation());
		if (Dist <= ShareRange)
		{
			Other->SetPrimaryTarget(Target);
		}
	}
}

bool ASHEnemyAIController::IsTargetAlreadyCoveredBySquad(AActor* Target) const
{
	if (!Target || !GetPawn()) return false;

	int32 SquadMatesTargeting = 0;
	const float SquadRange = 5000.f;

	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		const ASHEnemyAIController* Other = Cast<ASHEnemyAIController>(It->Get());
		if (!Other || Other == this || !Other->GetPawn()) continue;

		const float Dist = FVector::Dist(GetPawn()->GetActorLocation(), Other->GetPawn()->GetActorLocation());
		if (Dist > SquadRange) continue;

		if (Other->GetPrimaryTarget() == Target)
		{
			SquadMatesTargeting++;
		}
	}

	return SquadMatesTargeting >= 2;
}

FVector ASHEnemyAIController::GetSquadCentroid() const
{
	if (!GetPawn()) return FVector::ZeroVector;

	FVector Sum = GetPawn()->GetActorLocation();
	int32 Count = 1;
	const float SquadRange = 5000.f;

	for (FConstControllerIterator It = GetWorld()->GetControllerIterator(); It; ++It)
	{
		const ASHEnemyAIController* Other = Cast<ASHEnemyAIController>(It->Get());
		if (!Other || Other == this || !Other->GetPawn()) continue;

		const float Dist = FVector::Dist(GetPawn()->GetActorLocation(), Other->GetPawn()->GetActorLocation());
		if (Dist <= SquadRange)
		{
			Sum += Other->GetPawn()->GetActorLocation();
			Count++;
		}
	}

	return Sum / Count;
}

// -----------------------------------------------------------------------
//  Cover Logic
// -----------------------------------------------------------------------

void ASHEnemyAIController::TickCoverLogic(float DeltaSeconds)
{
	if (AwarenessState != ESHAwarenessState::Combat) return;

	CoverReevalTimer += DeltaSeconds;
	if (CoverReevalTimer >= CoverReevaluationInterval)
	{
		CoverReevalTimer = 0.f;

		// Re-evaluate cover against current threat
		if (!LastKnownThreatLocation.IsNearlyZero())
		{
			FindAndMoveToCover(LastKnownThreatLocation);
		}
	}
}

// -----------------------------------------------------------------------
//  Blackboard
// -----------------------------------------------------------------------

void ASHEnemyAIController::UpdateBlackboard()
{
	if (!BlackboardComp) return;

	BlackboardComp->SetValueAsEnum(SHBBKeys::AwarenessState, static_cast<uint8>(AwarenessState));
	BlackboardComp->SetValueAsEnum(SHBBKeys::CombatBehavior, static_cast<uint8>(CurrentBehavior));
	BlackboardComp->SetValueAsInt(SHBBKeys::NearbySquadMates, 0); // TODO: count nearby friendlies

	if (!LastKnownThreatLocation.IsNearlyZero())
	{
		BlackboardComp->SetValueAsVector(SHBBKeys::TargetLocation, LastKnownThreatLocation);
	}

	// Select and set best target
	AActor* BestTarget = SelectBestTarget();
	if (BestTarget)
	{
		SetPrimaryTarget(BestTarget);
	}
}
