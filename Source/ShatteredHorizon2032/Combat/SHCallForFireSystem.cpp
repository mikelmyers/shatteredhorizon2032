// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHCallForFireSystem.h"
#include "ShatteredHorizon2032/ShatteredHorizon2032.h"
#include "ShatteredHorizon2032/Weapons/SHBallisticsSystem.h"
#include "ShatteredHorizon2032/Combat/SHSuppressionSystem.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/DamageEvents.h"
#include "TimerManager.h"
#include "Particles/ParticleSystemComponent.h"

// =====================================================================
//  Constructor
// =====================================================================

USHCallForFireSystem::USHCallForFireSystem()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.0f; // tick every frame
}

// =====================================================================
//  BeginPlay
// =====================================================================

void USHCallForFireSystem::BeginPlay()
{
	Super::BeginPlay();

	// Cache subsystem references.
	if (const UWorld* World = GetWorld())
	{
		CachedBallisticsSystem = World->GetSubsystem<USHBallisticsSystem>();
		if (const UGameInstance* GI = World->GetGameInstance())
		{
			CachedSuppressionSystem = GI->GetSubsystem<USHSuppressionSystem>();
		}
	}

	// Populate defaults if not configured in editor.
	InitDefaultOrdnanceTable();
	InitDefaultAmmunitionPool();

	SH_LOG(LogSH_Combat, Log, "CallForFireSystem initialized on %s", *GetOwner()->GetName());
}

// =====================================================================
//  TickComponent
// =====================================================================

void USHCallForFireSystem::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// The firing cadence is handled via timer callbacks (FireNextRound).
	// Tick is available for future per-frame needs (e.g., UI updates).
}

// =====================================================================
//  Public API — RequestFireMission
// =====================================================================

bool USHCallForFireSystem::RequestFireMission(ESHOrdnanceType Type, FVector Target, int32 Rounds)
{
	// Reject if a mission is already active.
	if (ActiveMission.State == ESHFireMissionState::Requesting ||
		ActiveMission.State == ESHFireMissionState::Adjusting ||
		ActiveMission.State == ESHFireMissionState::Firing)
	{
		SH_WARNING(LogSH_Combat, "RequestFireMission denied — mission already active (State: %d).",
			static_cast<int32>(ActiveMission.State));
		return false;
	}

	// Check cooldown.
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	const float CurrentTime = World->GetTimeSeconds();
	if (CurrentTime - LastMissionCompleteTime < MissionCooldown)
	{
		SH_WARNING(LogSH_Combat, "RequestFireMission denied — on cooldown (%.1fs remaining).",
			MissionCooldown - (CurrentTime - LastMissionCompleteTime));
		return false;
	}

	// Check ammunition.
	const int32* AmmoPtr = AmmunitionPool.Find(Type);
	if (!AmmoPtr || *AmmoPtr <= 0)
	{
		SH_WARNING(LogSH_Combat, "RequestFireMission denied — no ammunition for ordnance type %d.",
			static_cast<int32>(Type));
		return false;
	}

	// Check ordnance data exists.
	const FSHOrdnanceData* OrdData = OrdnanceTable.Find(Type);
	if (!OrdData)
	{
		SH_ERROR(LogSH_Combat, "RequestFireMission denied — no ordnance data for type %d.",
			static_cast<int32>(Type));
		return false;
	}

	// Clamp rounds to available ammo.
	const int32 EffectiveRounds = FMath::Min(Rounds, *AmmoPtr);
	if (EffectiveRounds <= 0)
	{
		return false;
	}

	// Range check.
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return false;
	}
	const float RangeCm = FVector::Dist(Owner->GetActorLocation(), Target);
	if (RangeCm < MinFireRange)
	{
		SH_WARNING(LogSH_Combat, "RequestFireMission denied — target too close (%.0f cm < %.0f cm min).",
			RangeCm, MinFireRange);
		return false;
	}
	if (RangeCm > MaxFireRange)
	{
		SH_WARNING(LogSH_Combat, "RequestFireMission denied — target beyond max range (%.0f cm > %.0f cm max).",
			RangeCm, MaxFireRange);
		return false;
	}

	// Build the mission.
	ActiveMission = FSHFireMission();
	ActiveMission.MissionId = FGuid::NewGuid();
	ActiveMission.OrdnanceType = Type;
	ActiveMission.TargetLocation = Target;
	ActiveMission.AdjustOffset = FVector::ZeroVector;
	ActiveMission.RoundsRequested = EffectiveRounds;
	ActiveMission.RoundsRemaining = EffectiveRounds;
	ActiveMission.State = ESHFireMissionState::Requesting;
	ActiveMission.CEP = OrdData->CEP;
	ActiveMission.TimeOfFlight = ComputeTimeOfFlight(*OrdData, RangeCm);
	ActiveMission.RequestTime = CurrentTime;
	ActiveMission.bIsDangerClose = false;

	// Compute battery position.
	CachedBatteryPosition = ComputeBatteryPosition();

	// Danger-close check.
	if (CheckDangerClose(Target))
	{
		ActiveMission.bIsDangerClose = true;
		bAwaitingDangerCloseConfirmation = true;
		ActiveMission.State = ESHFireMissionState::Requesting;

		SH_WARNING(LogSH_Combat, "Fire mission %s — DANGER CLOSE. Awaiting confirmation.",
			*ActiveMission.MissionId.ToString());

		OnDangerCloseWarning.Broadcast(ActiveMission);
		return true; // Mission accepted but paused until ConfirmDangerClose().
	}

	// No danger-close — proceed directly to firing.
	bAwaitingDangerCloseConfirmation = false;
	BeginFiring();
	return true;
}

// =====================================================================
//  Public API — AdjustFire
// =====================================================================

void USHCallForFireSystem::AdjustFire(FVector NewOffset)
{
	if (ActiveMission.State != ESHFireMissionState::Firing &&
		ActiveMission.State != ESHFireMissionState::Adjusting &&
		ActiveMission.State != ESHFireMissionState::Requesting)
	{
		SH_WARNING(LogSH_Combat, "AdjustFire ignored — no active mission to adjust.");
		return;
	}

	ActiveMission.AdjustOffset += NewOffset;
	ActiveMission.TargetLocation += NewOffset;
	ActiveMission.State = ESHFireMissionState::Adjusting;

	// Recompute time of flight for adjusted target.
	const AActor* Owner = GetOwner();
	if (Owner)
	{
		const float RangeCm = FVector::Dist(Owner->GetActorLocation(), ActiveMission.TargetLocation);
		const FSHOrdnanceData* OrdData = OrdnanceTable.Find(ActiveMission.OrdnanceType);
		if (OrdData)
		{
			ActiveMission.TimeOfFlight = ComputeTimeOfFlight(*OrdData, RangeCm);
		}
	}

	// Re-check danger close for new position.
	ActiveMission.bIsDangerClose = CheckDangerClose(ActiveMission.TargetLocation);
	if (ActiveMission.bIsDangerClose)
	{
		bAwaitingDangerCloseConfirmation = true;
		SH_WARNING(LogSH_Combat, "Adjusted target is DANGER CLOSE. Awaiting confirmation.");
		OnDangerCloseWarning.Broadcast(ActiveMission);
		return;
	}

	// If rounds remain, continue firing at new target.
	if (ActiveMission.RoundsRemaining > 0)
	{
		BeginFiring();
	}

	SH_LOG(LogSH_Combat, Log, "Fire adjusted by offset (%s). New target: %s",
		*NewOffset.ToString(), *ActiveMission.TargetLocation.ToString());
}

// =====================================================================
//  Public API — RepeatFire
// =====================================================================

bool USHCallForFireSystem::RepeatFire()
{
	if (ActiveMission.State == ESHFireMissionState::Idle)
	{
		SH_WARNING(LogSH_Combat, "RepeatFire denied — no previous mission to repeat.");
		return false;
	}

	// Repeat uses the same ordnance type, target location, and round count.
	const ESHOrdnanceType Type = ActiveMission.OrdnanceType;
	const FVector Target = ActiveMission.TargetLocation;
	const int32 Rounds = ActiveMission.RoundsRequested;

	// Reset active mission state so RequestFireMission accepts.
	ActiveMission.State = ESHFireMissionState::Idle;

	return RequestFireMission(Type, Target, Rounds);
}

// =====================================================================
//  Public API — CancelMission
// =====================================================================

void USHCallForFireSystem::CancelMission()
{
	if (ActiveMission.State == ESHFireMissionState::Idle ||
		ActiveMission.State == ESHFireMissionState::Complete)
	{
		return;
	}

	SH_LOG(LogSH_Combat, Log, "Fire mission %s cancelled. %d rounds remaining.",
		*ActiveMission.MissionId.ToString(), ActiveMission.RoundsRemaining);

	ClearPendingTimers();
	ActiveMission.State = ESHFireMissionState::Complete;
	ActiveMission.RoundsRemaining = 0;
	LastMissionCompleteTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	OnFireMissionComplete.Broadcast(ActiveMission);
}

// =====================================================================
//  Public API — ConfirmDangerClose
// =====================================================================

void USHCallForFireSystem::ConfirmDangerClose()
{
	if (!bAwaitingDangerCloseConfirmation)
	{
		SH_WARNING(LogSH_Combat, "ConfirmDangerClose called but no confirmation pending.");
		return;
	}

	SH_LOG(LogSH_Combat, Log, "DANGER CLOSE confirmed for mission %s.",
		*ActiveMission.MissionId.ToString());

	bAwaitingDangerCloseConfirmation = false;
	BeginFiring();
}

// =====================================================================
//  Queries
// =====================================================================

bool USHCallForFireSystem::IsFireAvailable(ESHOrdnanceType Type) const
{
	// Check ammunition.
	const int32* AmmoPtr = AmmunitionPool.Find(Type);
	if (!AmmoPtr || *AmmoPtr <= 0)
	{
		return false;
	}

	// Check cooldown.
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	const float CurrentTime = World->GetTimeSeconds();
	if (CurrentTime - LastMissionCompleteTime < MissionCooldown)
	{
		return false;
	}

	// Check that we're not mid-mission.
	if (ActiveMission.State == ESHFireMissionState::Requesting ||
		ActiveMission.State == ESHFireMissionState::Adjusting ||
		ActiveMission.State == ESHFireMissionState::Firing)
	{
		return false;
	}

	return true;
}

int32 USHCallForFireSystem::GetRemainingAmmo(ESHOrdnanceType Type) const
{
	const int32* AmmoPtr = AmmunitionPool.Find(Type);
	return AmmoPtr ? *AmmoPtr : 0;
}

// =====================================================================
//  Internal — Default ordnance table
// =====================================================================

void USHCallForFireSystem::InitDefaultOrdnanceTable()
{
	if (OrdnanceTable.Num() > 0)
	{
		return;
	}

	// 60mm Mortar — light, fast, short range.
	{
		FSHOrdnanceData Data;
		Data.BaseDamage = 150.0f;
		Data.BlastInnerRadius = 300.0f;
		Data.BlastOuterRadius = 1200.0f;
		Data.FragParams.FragmentCount = 40;
		Data.FragParams.InnerRadiusCm = 300.0f;
		Data.FragParams.OuterRadiusCm = 1200.0f;
		Data.FragParams.FragmentDamage = 35.0f;
		Data.FragParams.FragmentSpeedCmS = 120000.0f;
		Data.TimeOfFlightMin = 8.0f;
		Data.TimeOfFlightMax = 10.0f;
		Data.CEP = 1000.0f;
		Data.AudibleRange = 30000.0f;
		Data.CooldownBetweenRounds = 2.0f;
		OrdnanceTable.Add(ESHOrdnanceType::Mortar60mm, Data);
	}

	// 81mm Mortar — medium.
	{
		FSHOrdnanceData Data;
		Data.BaseDamage = 250.0f;
		Data.BlastInnerRadius = 500.0f;
		Data.BlastOuterRadius = 2000.0f;
		Data.FragParams.FragmentCount = 60;
		Data.FragParams.InnerRadiusCm = 500.0f;
		Data.FragParams.OuterRadiusCm = 2000.0f;
		Data.FragParams.FragmentDamage = 45.0f;
		Data.FragParams.FragmentSpeedCmS = 140000.0f;
		Data.TimeOfFlightMin = 8.0f;
		Data.TimeOfFlightMax = 11.0f;
		Data.CEP = 1200.0f;
		Data.AudibleRange = 40000.0f;
		Data.CooldownBetweenRounds = 3.0f;
		OrdnanceTable.Add(ESHOrdnanceType::Mortar81mm, Data);
	}

	// 105mm Artillery — heavy, long range.
	{
		FSHOrdnanceData Data;
		Data.BaseDamage = 400.0f;
		Data.BlastInnerRadius = 800.0f;
		Data.BlastOuterRadius = 3500.0f;
		Data.FragParams.FragmentCount = 100;
		Data.FragParams.InnerRadiusCm = 800.0f;
		Data.FragParams.OuterRadiusCm = 3500.0f;
		Data.FragParams.FragmentDamage = 55.0f;
		Data.FragParams.FragmentSpeedCmS = 160000.0f;
		Data.TimeOfFlightMin = 9.0f;
		Data.TimeOfFlightMax = 12.0f;
		Data.CEP = 1500.0f;
		Data.AudibleRange = 60000.0f;
		Data.CooldownBetweenRounds = 4.0f;
		OrdnanceTable.Add(ESHOrdnanceType::Artillery105mm, Data);
	}

	// 155mm Artillery — devastating.
	{
		FSHOrdnanceData Data;
		Data.BaseDamage = 600.0f;
		Data.BlastInnerRadius = 1200.0f;
		Data.BlastOuterRadius = 5000.0f;
		Data.FragParams.FragmentCount = 150;
		Data.FragParams.InnerRadiusCm = 1200.0f;
		Data.FragParams.OuterRadiusCm = 5000.0f;
		Data.FragParams.FragmentDamage = 65.0f;
		Data.FragParams.FragmentSpeedCmS = 180000.0f;
		Data.TimeOfFlightMin = 10.0f;
		Data.TimeOfFlightMax = 12.0f;
		Data.CEP = 2000.0f;
		Data.AudibleRange = 80000.0f;
		Data.CooldownBetweenRounds = 5.0f;
		OrdnanceTable.Add(ESHOrdnanceType::Artillery155mm, Data);
	}

	// Smoke screen — no damage, area denial.
	{
		FSHOrdnanceData Data;
		Data.BaseDamage = 0.0f;
		Data.BlastInnerRadius = 0.0f;
		Data.BlastOuterRadius = 0.0f;
		Data.FragParams.FragmentCount = 0;
		Data.FragParams.FragmentDamage = 0.0f;
		Data.TimeOfFlightMin = 8.0f;
		Data.TimeOfFlightMax = 10.0f;
		Data.CEP = 800.0f;
		Data.AudibleRange = 20000.0f;
		Data.CooldownBetweenRounds = 2.0f;
		OrdnanceTable.Add(ESHOrdnanceType::SmokeScreen, Data);
	}

	// Illumination — no damage, battlefield illumination.
	{
		FSHOrdnanceData Data;
		Data.BaseDamage = 0.0f;
		Data.BlastInnerRadius = 0.0f;
		Data.BlastOuterRadius = 0.0f;
		Data.FragParams.FragmentCount = 0;
		Data.FragParams.FragmentDamage = 0.0f;
		Data.TimeOfFlightMin = 8.0f;
		Data.TimeOfFlightMax = 11.0f;
		Data.CEP = 1000.0f;
		Data.AudibleRange = 25000.0f;
		Data.CooldownBetweenRounds = 3.0f;
		OrdnanceTable.Add(ESHOrdnanceType::Illumination, Data);
	}
}

// =====================================================================
//  Internal — Default ammunition pool
// =====================================================================

void USHCallForFireSystem::InitDefaultAmmunitionPool()
{
	if (AmmunitionPool.Num() > 0)
	{
		return;
	}

	AmmunitionPool.Add(ESHOrdnanceType::Mortar60mm, 20);
	AmmunitionPool.Add(ESHOrdnanceType::Mortar81mm, 12);
	AmmunitionPool.Add(ESHOrdnanceType::Artillery105mm, 8);
	AmmunitionPool.Add(ESHOrdnanceType::Artillery155mm, 4);
	AmmunitionPool.Add(ESHOrdnanceType::SmokeScreen, 10);
	AmmunitionPool.Add(ESHOrdnanceType::Illumination, 6);
}

// =====================================================================
//  Internal — Danger-close check
// =====================================================================

bool USHCallForFireSystem::CheckDangerClose(const FVector& Position) const
{
	TArray<FVector> FriendlyPositions;
	GatherFriendlyPositions(FriendlyPositions);

	for (const FVector& FriendlyPos : FriendlyPositions)
	{
		const float DistCm = FVector::Dist(FriendlyPos, Position);
		if (DistCm <= DangerCloseRadius)
		{
			return true;
		}
	}

	return false;
}

void USHCallForFireSystem::GatherFriendlyPositions(TArray<FVector>& OutPositions) const
{
	OutPositions.Reset();

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Gather all player-controlled pawns and AI characters on the same team.
	// We iterate all player controllers and use their pawns as friendlies.
	// In a full implementation this would query the team/faction system.
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* PC = It->Get();
		if (PC && PC->GetPawn())
		{
			// Exclude the owning player from danger-close check against self.
			if (PC->GetPawn() == GetOwner())
			{
				continue;
			}
			OutPositions.Add(PC->GetPawn()->GetActorLocation());
		}
	}

	// Also gather AI characters. Use APawn iterator to find all non-player pawns
	// that could be friendlies. In production this would use the team subsystem.
	for (TActorIterator<ACharacter> CharIt(World); CharIt; ++CharIt)
	{
		ACharacter* Character = *CharIt;
		if (!Character || Character == GetOwner())
		{
			continue;
		}

		// Skip player-controlled characters (already gathered above).
		if (Character->IsPlayerControlled())
		{
			continue;
		}

		// Heuristic: AI characters without a player controller on the same side.
		// In production, check IGenericTeamAgentInterface or the game's faction system.
		// For now, include all AI pawns as potential friendlies for safety.
		OutPositions.Add(Character->GetActorLocation());
	}
}

// =====================================================================
//  Internal — Battery position
// =====================================================================

FVector USHCallForFireSystem::ComputeBatteryPosition() const
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FVector::ZeroVector;
	}

	// Battery position is behind the player relative to their facing direction.
	const FRotator OwnerRotation = Owner->GetActorRotation();
	const FVector ForwardDir = OwnerRotation.Vector();
	const FVector RightDir = FRotationMatrix(OwnerRotation).GetScaledAxis(EAxis::Y);

	// Offset: primarily behind the player, slightly randomized laterally.
	const FVector BatteryPos = Owner->GetActorLocation()
		+ ForwardDir * BatteryPositionOffset.X
		+ RightDir * BatteryPositionOffset.Y
		+ FVector(0.0f, 0.0f, BatteryPositionOffset.Z);

	return BatteryPos;
}

// =====================================================================
//  Internal — Time of flight computation
// =====================================================================

float USHCallForFireSystem::ComputeTimeOfFlight(const FSHOrdnanceData& Data, float RangeCm) const
{
	// Interpolate time of flight between min and max based on range fraction.
	const float RangeFraction = FMath::Clamp(RangeCm / MaxFireRange, 0.0f, 1.0f);
	return FMath::Lerp(Data.TimeOfFlightMin, Data.TimeOfFlightMax, RangeFraction);
}

// =====================================================================
//  Internal — CEP dispersion
// =====================================================================

FVector USHCallForFireSystem::ComputeDispersedImpactPoint(const FVector& AimPoint, float CEPRadius) const
{
	// CEP = radius in which 50% of rounds land.
	// Model as a Rayleigh distribution: sigma = CEP / 1.1774 for 50% containment.
	// We sample a 2D Gaussian and apply it to the XY plane.
	const float Sigma = CEPRadius / 1.1774f;

	// Box-Muller transform for two independent normal samples.
	const float U1 = FMath::Max(FMath::FRand(), SMALL_NUMBER); // avoid log(0)
	const float U2 = FMath::FRand();
	const float Magnitude = Sigma * FMath::Sqrt(-2.0f * FMath::Loge(U1));
	const float Angle = 2.0f * PI * U2;

	const float OffsetX = Magnitude * FMath::Cos(Angle);
	const float OffsetY = Magnitude * FMath::Sin(Angle);

	return AimPoint + FVector(OffsetX, OffsetY, 0.0f);
}

// =====================================================================
//  Internal — Begin firing salvo
// =====================================================================

void USHCallForFireSystem::BeginFiring()
{
	ActiveMission.State = ESHFireMissionState::Firing;

	SH_LOG(LogSH_Combat, Log, "Fire mission %s — FIRING. Ordnance: %d, Rounds: %d, ToF: %.1fs, Target: %s",
		*ActiveMission.MissionId.ToString(),
		static_cast<int32>(ActiveMission.OrdnanceType),
		ActiveMission.RoundsRemaining,
		ActiveMission.TimeOfFlight,
		*ActiveMission.TargetLocation.ToString());

	OnFireMissionStarted.Broadcast(ActiveMission);

	// Expose battery position for counter-battery fire.
	BroadcastCounterBatteryExposure(CachedBatteryPosition);

	// Fire the first round immediately.
	FireNextRound();
}

// =====================================================================
//  Internal — Fire next round
// =====================================================================

void USHCallForFireSystem::FireNextRound()
{
	if (ActiveMission.State != ESHFireMissionState::Firing)
	{
		return;
	}

	if (ActiveMission.RoundsRemaining <= 0)
	{
		CompleteMission();
		return;
	}

	const FSHOrdnanceData* OrdData = OrdnanceTable.Find(ActiveMission.OrdnanceType);
	if (!OrdData)
	{
		SH_ERROR(LogSH_Combat, "FireNextRound — missing ordnance data. Aborting mission.");
		CompleteMission();
		return;
	}

	// Consume ammunition.
	int32& Ammo = AmmunitionPool.FindOrAdd(ActiveMission.OrdnanceType, 0);
	if (Ammo <= 0)
	{
		SH_WARNING(LogSH_Combat, "FireNextRound — out of ammunition. Completing mission early.");
		CompleteMission();
		return;
	}
	Ammo--;
	ActiveMission.RoundsRemaining--;

	// Compute dispersed impact point.
	const FVector ImpactPoint = ComputeDispersedImpactPoint(ActiveMission.TargetLocation, OrdData->CEP);

	// Play outgoing round sound at battery position.
	if (OutgoingRoundSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			GetWorld(), OutgoingRoundSound, CachedBatteryPosition,
			1.0f, 1.0f, 0.0f,
			nullptr, nullptr);
	}

	SH_LOG(LogSH_Combat, Log, "Round fired — impact at %s in %.1fs. Remaining: %d",
		*ImpactPoint.ToString(), ActiveMission.TimeOfFlight, ActiveMission.RoundsRemaining);

	// Schedule incoming whistle and impact.
	ScheduleRoundImpact(ImpactPoint, ActiveMission.TimeOfFlight);

	// Schedule the next round after the inter-round cooldown.
	if (ActiveMission.RoundsRemaining > 0)
	{
		FTimerHandle NextRoundTimer;
		FTimerDelegate NextRoundDelegate;
		NextRoundDelegate.BindUObject(this, &USHCallForFireSystem::FireNextRound);

		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				NextRoundTimer,
				NextRoundDelegate,
				OrdData->CooldownBetweenRounds,
				false);
			PendingRoundTimers.Add(NextRoundTimer);
		}
	}
}

// =====================================================================
//  Internal — Schedule round impact
// =====================================================================

void USHCallForFireSystem::ScheduleRoundImpact(const FVector& ImpactPoint, float TimeOfFlight)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FTimerManager& TimerMgr = World->GetTimerManager();

	// Schedule incoming whistle at ~75% of time-of-flight (last quarter of trajectory).
	const float WhistleLeadTime = TimeOfFlight * 0.75f;
	{
		FTimerHandle WhistleTimer;
		FTimerDelegate WhistleDelegate;
		WhistleDelegate.BindUObject(this, &USHCallForFireSystem::PlayIncomingWhistle, ImpactPoint);
		TimerMgr.SetTimer(WhistleTimer, WhistleDelegate, WhistleLeadTime, false);
		PendingRoundTimers.Add(WhistleTimer);
	}

	// Schedule impact at full time-of-flight.
	{
		FTimerHandle ImpactTimer;
		FTimerDelegate ImpactDelegate;
		ImpactDelegate.BindUObject(this, &USHCallForFireSystem::ProcessRoundImpact, ImpactPoint);
		TimerMgr.SetTimer(ImpactTimer, ImpactDelegate, TimeOfFlight, false);
		PendingRoundTimers.Add(ImpactTimer);
	}
}

// =====================================================================
//  Internal — Incoming whistle
// =====================================================================

void USHCallForFireSystem::PlayIncomingWhistle(FVector ImpactPoint)
{
	if (IncomingWhistleSound)
	{
		// Play at a point above the impact to simulate descending trajectory.
		const FVector WhistleOrigin = ImpactPoint + FVector(0.0f, 0.0f, 5000.0f);
		UGameplayStatics::PlaySoundAtLocation(
			GetWorld(), IncomingWhistleSound, WhistleOrigin,
			1.0f, 1.0f, 0.0f,
			nullptr, nullptr);
	}
}

// =====================================================================
//  Internal — Round impact
// =====================================================================

void USHCallForFireSystem::ProcessRoundImpact(FVector ImpactPoint)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FSHOrdnanceData* OrdData = OrdnanceTable.Find(ActiveMission.OrdnanceType);
	if (!OrdData)
	{
		return;
	}

	// Trace downward to find actual ground position.
	FHitResult GroundHit;
	const FVector TraceStart = ImpactPoint + FVector(0.0f, 0.0f, 10000.0f);
	const FVector TraceEnd = ImpactPoint - FVector(0.0f, 0.0f, 20000.0f);
	FCollisionQueryParams TraceParams(SCENE_QUERY_STAT(IndirectFireImpact), true);
	TraceParams.AddIgnoredActor(GetOwner());

	FVector FinalImpactPoint = ImpactPoint;
	if (World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, TraceParams))
	{
		FinalImpactPoint = GroundHit.ImpactPoint;
	}

	SH_LOG(LogSH_Combat, Log, "Round impact at %s", *FinalImpactPoint.ToString());

	// Play explosion sound.
	if (ImpactExplosionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(
			World, ImpactExplosionSound, FinalImpactPoint,
			1.0f, 1.0f, 0.0f,
			nullptr, nullptr);
	}

	// Spawn explosion VFX.
	if (ImpactExplosionVFX)
	{
		UGameplayStatics::SpawnEmitterAtLocation(
			World, ImpactExplosionVFX, FinalImpactPoint,
			FRotator::ZeroRotator, FVector(1.0f), true);
	}

	// Apply blast damage (only for lethal ordnance types).
	if (OrdData->BaseDamage > 0.0f)
	{
		ApplyBlastDamage(FinalImpactPoint, *OrdData);
	}

	// Report suppression via the suppression system.
	if (CachedSuppressionSystem && OrdData->BlastOuterRadius > 0.0f)
	{
		CachedSuppressionSystem->ReportExplosion(
			FinalImpactPoint, OrdData->BlastOuterRadius, OrdData->BlastInnerRadius);
	}

	// Spawn fragmentation via the ballistics system.
	if (CachedBallisticsSystem && OrdData->FragParams.FragmentCount > 0)
	{
		AController* InstigatorController = nullptr;
		if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
		{
			InstigatorController = OwnerPawn->GetController();
		}

		CachedBallisticsSystem->SpawnFragmentation(
			FinalImpactPoint,
			OrdData->FragParams,
			InstigatorController,
			GetOwner());
	}

	// Broadcast delegate.
	OnRoundImpact.Broadcast(ActiveMission, FinalImpactPoint);

	// If all rounds have been fired and this was the last impact, complete the mission.
	// We check if there are no more pending impact timers scheduled after this one.
	if (ActiveMission.RoundsRemaining <= 0)
	{
		// Defer completion slightly to allow any remaining impact timers to fire.
		// The mission will be completed when the last impact processes.
		// Check if this is the last scheduled impact by counting remaining timers.
		bool bHasRemainingImpacts = false;
		FTimerManager& TimerMgr = World->GetTimerManager();
		for (const FTimerHandle& Handle : PendingRoundTimers)
		{
			if (TimerMgr.IsTimerActive(Handle))
			{
				bHasRemainingImpacts = true;
				break;
			}
		}

		if (!bHasRemainingImpacts)
		{
			CompleteMission();
		}
	}
}

// =====================================================================
//  Internal — Blast damage
// =====================================================================

void USHCallForFireSystem::ApplyBlastDamage(const FVector& Origin, const FSHOrdnanceData& Data)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Determine the instigator controller.
	AController* InstigatorController = nullptr;
	if (const APawn* OwnerPawn = Cast<APawn>(GetOwner()))
	{
		InstigatorController = OwnerPawn->GetController();
	}

	// Use UE5's radial damage with falloff.
	TArray<AActor*> IgnoreActors;
	UGameplayStatics::ApplyRadialDamageWithFalloff(
		World,
		Data.BaseDamage,
		Data.BaseDamage * 0.1f, // MinimumDamage at outer edge
		Origin,
		Data.BlastInnerRadius,
		Data.BlastOuterRadius,
		1.0f, // DamageFalloff exponent
		IndirectFireDamageType ? IndirectFireDamageType.Get() : UDamageType::StaticClass(),
		IgnoreActors,
		GetOwner(),
		InstigatorController);
}

// =====================================================================
//  Internal — Counter-battery exposure
// =====================================================================

void USHCallForFireSystem::BroadcastCounterBatteryExposure(const FVector& BatteryPos)
{
	// Notify enemy AI of the battery firing position so they can initiate
	// counter-battery fire. This iterates all AI controllers in the world
	// and sends a stimulus if they are within detection range.
	//
	// In production, this would use the AI perception system (UAISense_Hearing)
	// or a dedicated threat-reporting subsystem. For now, we report via the
	// noise event system which AI perception can pick up.

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Make noise at the battery position that enemy AI can detect.
	AActor* Owner = GetOwner();
	if (Owner)
	{
		// MakeNoise with a loud volume — artillery fire is extremely audible.
		Owner->MakeNoise(1.0f, Cast<APawn>(Owner), BatteryPos);
	}

	SH_LOG(LogSH_Combat, Log, "Counter-battery exposure broadcast at %s", *BatteryPos.ToString());
}

// =====================================================================
//  Internal — Complete mission
// =====================================================================

void USHCallForFireSystem::CompleteMission()
{
	ActiveMission.State = ESHFireMissionState::Complete;
	LastMissionCompleteTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;

	ClearPendingTimers();

	SH_LOG(LogSH_Combat, Log, "Fire mission %s COMPLETE. %d/%d rounds delivered.",
		*ActiveMission.MissionId.ToString(),
		ActiveMission.RoundsRequested - ActiveMission.RoundsRemaining,
		ActiveMission.RoundsRequested);

	OnFireMissionComplete.Broadcast(ActiveMission);
}

// =====================================================================
//  Internal — Timer cleanup
// =====================================================================

void USHCallForFireSystem::ClearPendingTimers()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		PendingRoundTimers.Empty();
		return;
	}

	FTimerManager& TimerMgr = World->GetTimerManager();
	for (FTimerHandle& Handle : PendingRoundTimers)
	{
		TimerMgr.ClearTimer(Handle);
	}
	PendingRoundTimers.Empty();
}
