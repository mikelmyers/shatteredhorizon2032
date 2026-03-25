// Copyright Shattered Horizon 2032. All Rights Reserved.

#include "SHSquadMember.h"
#include "SHSquadManager.h"
#include "SHSquadAIController.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundCue.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "Engine/World.h"

ASHSquadMember::ASHSquadMember()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f; // Every frame for responsiveness

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = ASHSquadAIController::StaticClass();

	VoiceAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("VoiceAudio"));
	VoiceAudioComponent->SetupAttachment(GetRootComponent());
	VoiceAudioComponent->bAutoActivate = false;
	VoiceAudioComponent->bIsUISound = false;
}

void ASHSquadMember::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
	BleedoutTimeRemaining = BleedoutTimeMax;
	RecalculateEffectiveness();

	// Set ammo based on role
	switch (Role)
	{
	case ESHSquadRole::AutomaticRifleman:
		AmmoState.MagazineCapacity = 200;
		AmmoState.CurrentMagazineRounds = 200;
		AmmoState.TotalReserveMagazines = 3;
		break;
	case ESHSquadRole::Grenadier:
		AmmoState.GrenadeCount = 6;
		AmmoState.MagazineCapacity = 30;
		AmmoState.CurrentMagazineRounds = 30;
		AmmoState.TotalReserveMagazines = 7;
		break;
	case ESHSquadRole::DesignatedMarksman:
		AmmoState.MagazineCapacity = 20;
		AmmoState.CurrentMagazineRounds = 20;
		AmmoState.TotalReserveMagazines = 8;
		break;
	default:
		// Rifleman / TeamLeader defaults are fine
		break;
	}
}

void ASHSquadMember::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (!IsAlive())
	{
		return;
	}

	TimeSinceLastShot += DeltaTime;
	TimeSinceLastIncoming += DeltaTime;
	TimeSinceLastOrder += DeltaTime;

	if (VoiceCooldownRemaining > 0.f)
	{
		VoiceCooldownRemaining -= DeltaTime;
	}

	UpdateSuppression(DeltaTime);
	UpdateMorale(DeltaTime);
	UpdateCombatStress(DeltaTime);
	UpdateAmmoConservation(DeltaTime);
	RecalculateEffectiveness();

	// Bleedout for incapacitated members
	if (WoundState == ESHWoundState::Incapacitated)
	{
		BleedoutTimeRemaining -= DeltaTime;
		if (BleedoutTimeRemaining <= 0.f)
		{
			WoundState = ESHWoundState::KIA;
			CurrentHealth = 0.f;
			OnWoundStateChanged.Broadcast(this, ESHWoundState::KIA);
		}
	}
}

float ASHSquadMember::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
								  AController* EventInstigator, AActor* DamageCauser)
{
	const float ActualDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	if (!IsAlive())
	{
		return ActualDamage;
	}

	ApplyWound(ActualDamage);

	// Near misses also cause suppression
	ApplySuppression(ActualDamage * 0.5f, DamageCauser ? DamageCauser->GetActorLocation() : GetActorLocation());

	TimeSinceLastIncoming = 0.f;

	return ActualDamage;
}

/* ────────────────────────────────────────────────────────────── */
/*  Tick Sub-routines                                            */
/* ────────────────────────────────────────────────────────────── */

void ASHSquadMember::UpdateSuppression(float DeltaTime)
{
	// Decay suppression over time, faster for experienced soldiers
	const float DecayMultiplier = FMath::Lerp(0.7f, 1.4f, Personality.SuppressionResistance);
	SuppressionValue = FMath::Max(0.f, SuppressionValue - (SuppressionDecayRate * DecayMultiplier * DeltaTime));

	// Update suppression level enum
	ESHSuppressionLevel NewLevel;
	if (SuppressionValue >= SuppressionPinnedThreshold)
	{
		NewLevel = ESHSuppressionLevel::Pinned;
	}
	else if (SuppressionValue >= SuppressionHeavyThreshold)
	{
		NewLevel = ESHSuppressionLevel::Heavy;
	}
	else if (SuppressionValue >= SuppressionModerateThreshold)
	{
		NewLevel = ESHSuppressionLevel::Moderate;
	}
	else if (SuppressionValue >= SuppressionLightThreshold)
	{
		NewLevel = ESHSuppressionLevel::Light;
	}
	else
	{
		NewLevel = ESHSuppressionLevel::None;
	}

	if (NewLevel != SuppressionLevel)
	{
		SuppressionLevel = NewLevel;

		// Callout when first suppressed or when pinned
		if (NewLevel == ESHSuppressionLevel::Heavy || NewLevel == ESHSuppressionLevel::Pinned)
		{
			PlayVoiceLine(ESHVoiceLineType::Suppressed);
		}
	}
}

void ASHSquadMember::UpdateMorale(float DeltaTime)
{
	const float PreviousMorale = Morale;

	// Morale slowly recovers when not suppressed and in cover
	if (SuppressionLevel == ESHSuppressionLevel::None && bIsInCover)
	{
		Morale = FMath::Min(100.f, Morale + (5.f * DeltaTime));
	}

	// Morale drops while suppressed
	if (SuppressionLevel >= ESHSuppressionLevel::Heavy)
	{
		Morale = FMath::Max(0.f, Morale - (10.f * DeltaTime));
	}

	// Morale drops faster when wounded
	if (WoundState == ESHWoundState::HeavyWound)
	{
		Morale = FMath::Max(0.f, Morale - (3.f * DeltaTime));
	}

	// Experienced soldiers maintain morale better
	Morale = FMath::Max(Morale, Personality.Experience * 20.f);

	// Broadcast significant changes (> 5 point shift)
	if (FMath::Abs(Morale - PreviousMorale) > 5.f)
	{
		OnMoraleChanged.Broadcast(this, Morale);
	}
}

void ASHSquadMember::UpdateCombatStress(float DeltaTime)
{
	// Determine combat stress from suppression + morale + wound state
	float StressScore = 0.f;
	StressScore += (100.f - Morale) * 0.3f;
	StressScore += SuppressionValue * 0.4f;

	if (WoundState >= ESHWoundState::LightWound) StressScore += 15.f;
	if (WoundState >= ESHWoundState::HeavyWound) StressScore += 30.f;

	// Experience reduces perceived stress
	StressScore *= FMath::Lerp(1.3f, 0.6f, Personality.Experience);

	if (StressScore >= 70.f)
	{
		CombatStress = ESHCombatStress::Panicked;
	}
	else if (StressScore >= 45.f)
	{
		CombatStress = ESHCombatStress::Stressed;
	}
	else if (StressScore >= 20.f)
	{
		CombatStress = ESHCombatStress::Alert;
	}
	else
	{
		CombatStress = ESHCombatStress::Calm;
	}
}

void ASHSquadMember::UpdateAutonomousBehavior(float DeltaTime)
{
	// Handled by behavior tree in the AI controller — this method updates
	// blackboard keys that the behavior tree reads.
}

void ASHSquadMember::UpdateAmmoConservation(float DeltaTime)
{
	bIsConservingAmmo = AmmoState.GetAmmoFraction() < AmmoConservationThreshold;

	if (AmmoState.IsAmmoLow() && VoiceCooldownRemaining <= 0.f)
	{
		// Periodically report low ammo
		if (FMath::FRand() < 0.01f) // ~1% chance per frame to avoid spam
		{
			PlayVoiceLine(ESHVoiceLineType::AmmoLow);
		}
	}
}

/* ────────────────────────────────────────────────────────────── */
/*  Orders                                                       */
/* ────────────────────────────────────────────────────────────── */

void ASHSquadMember::ReceiveOrder(const FSHSquadOrder& InOrder)
{
	if (!CanExecuteOrders())
	{
		return;
	}

	// If we have a current active order, check priority
	if (CurrentOrder.IsValid() && CurrentOrder.AckState == ESHOrderAckState::Executing)
	{
		if (!InOrder.CanOverride(CurrentOrder))
		{
			// Queue the order instead
			OrderQueue.Add(InOrder);
			return;
		}

		// Override current order
		CurrentOrder.AckState = ESHOrderAckState::Overridden;
		OnOrderStateChanged.Broadcast(this, ESHOrderAckState::Overridden);
	}

	CurrentOrder = InOrder;
	CurrentOrder.AckState = ESHOrderAckState::Received;
	TimeSinceLastOrder = 0.f;
	OnOrderStateChanged.Broadcast(this, ESHOrderAckState::Received);

	// Acknowledge verbally
	PlayVoiceLine(ESHVoiceLineType::OrderAcknowledge);

	// Notify the AI controller
	if (ASHSquadAIController* AICon = Cast<ASHSquadAIController>(GetController()))
	{
		AICon->OnOrderReceived(CurrentOrder);
	}
}

void ASHSquadMember::SetOrderAckState(ESHOrderAckState NewState)
{
	CurrentOrder.AckState = NewState;
	OnOrderStateChanged.Broadcast(this, NewState);

	// If completed, dequeue next order
	if (NewState == ESHOrderAckState::Completed || NewState == ESHOrderAckState::Failed)
	{
		if (OrderQueue.Num() > 0)
		{
			FSHSquadOrder NextOrder = OrderQueue[0];
			OrderQueue.RemoveAt(0);
			ReceiveOrder(NextOrder);
		}
		else
		{
			CurrentOrder.Reset();
		}
	}
}

/* ────────────────────────────────────────────────────────────── */
/*  Suppression                                                  */
/* ────────────────────────────────────────────────────────────── */

void ASHSquadMember::ApplySuppression(float Amount, const FVector& SourceLocation)
{
	if (!IsAlive()) return;

	// Distance attenuation — closer fire is more suppressing
	const float Distance = FVector::Dist(GetActorLocation(), SourceLocation);
	const float DistanceFactor = FMath::Clamp(1.f - (Distance / 5000.f), 0.2f, 1.f);

	// Personality modifies suppression received
	const float PersonalityFactor = FMath::Lerp(1.3f, 0.5f, Personality.SuppressionResistance);

	// Cover reduces suppression
	const float CoverFactor = bIsInCover ? 0.5f : 1.f;

	SuppressionValue = FMath::Min(100.f,
		SuppressionValue + (Amount * DistanceFactor * PersonalityFactor * CoverFactor));

	TimeSinceLastIncoming = 0.f;
}

/* ────────────────────────────────────────────────────────────── */
/*  Wound System                                                 */
/* ────────────────────────────────────────────────────────────── */

void ASHSquadMember::ApplyWound(float Damage)
{
	if (!IsAlive()) return;

	CurrentHealth = FMath::Max(0.f, CurrentHealth - Damage);
	UpdateWoundState();

	if (WoundState >= ESHWoundState::LightWound)
	{
		PlayVoiceLine(ESHVoiceLineType::WoundedSelf);
	}
}

bool ASHSquadMember::ApplyBuddyAid(float HealAmount)
{
	if (WoundState == ESHWoundState::KIA) return false;

	if (WoundState == ESHWoundState::Incapacitated)
	{
		// Stabilize — bring back to heavy wound
		CurrentHealth = FMath::Max(CurrentHealth, HeavyWoundThreshold);
		BleedoutTimeRemaining = BleedoutTimeMax;
		UpdateWoundState();
		return true;
	}

	if (WoundState >= ESHWoundState::LightWound)
	{
		CurrentHealth = FMath::Min(MaxHealth, CurrentHealth + HealAmount);
		UpdateWoundState();
		return true;
	}

	return false;
}

void ASHSquadMember::UpdateWoundState()
{
	ESHWoundState PreviousState = WoundState;

	if (CurrentHealth <= 0.f)
	{
		WoundState = ESHWoundState::KIA;
	}
	else if (CurrentHealth <= IncapacitatedThreshold)
	{
		WoundState = ESHWoundState::Incapacitated;
		if (PreviousState != ESHWoundState::Incapacitated)
		{
			BleedoutTimeRemaining = BleedoutTimeMax;
		}
	}
	else if (CurrentHealth <= HeavyWoundThreshold)
	{
		WoundState = ESHWoundState::HeavyWound;
	}
	else if (CurrentHealth <= LightWoundThreshold)
	{
		WoundState = ESHWoundState::LightWound;
	}
	else
	{
		WoundState = ESHWoundState::Healthy;
	}

	if (WoundState != PreviousState)
	{
		OnWoundStateChanged.Broadcast(this, WoundState);
		RecalculateEffectiveness();
	}
}

/* ────────────────────────────────────────────────────────────── */
/*  Dragging Wounded                                             */
/* ────────────────────────────────────────────────────────────── */

void ASHSquadMember::StartDragging(ASHSquadMember* WoundedMember)
{
	if (!WoundedMember || WoundedMember->WoundState != ESHWoundState::Incapacitated)
	{
		return;
	}

	DraggingTarget = WoundedMember;
	WoundedMember->bIsBeingDragged = true;
	WoundedMember->DraggedBy = this;

	// Attach wounded member for movement
	WoundedMember->AttachToActor(this, FAttachmentTransformRules::KeepWorldTransform);

	PlayVoiceLine(ESHVoiceLineType::CoverMe);
}

void ASHSquadMember::StopDragging()
{
	if (DraggingTarget.IsValid())
	{
		DraggingTarget->bIsBeingDragged = false;
		DraggingTarget->DraggedBy = nullptr;
		DraggingTarget->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
		DraggingTarget = nullptr;
	}
}

/* ────────────────────────────────────────────────────────────── */
/*  Weapon Usage                                                 */
/* ────────────────────────────────────────────────────────────── */

bool ASHSquadMember::FireWeapon()
{
	if (!IsCombatEffective() || bIsReloading) return false;

	if (!AmmoState.ConsumeRound())
	{
		// Click — empty magazine
		if (AmmoState.HasReserveAmmo())
		{
			BeginReload();
		}
		else
		{
			PlayVoiceLine(ESHVoiceLineType::AmmoLow);
		}
		return false;
	}

	bIsFiring = true;
	TimeSinceLastShot = 0.f;

	// Auto-reload on empty
	if (AmmoState.IsCurrentMagazineEmpty() && AmmoState.HasReserveAmmo())
	{
		BeginReload();
	}

	return true;
}

void ASHSquadMember::BeginReload()
{
	if (bIsReloading || !AmmoState.HasReserveAmmo()) return;

	bIsReloading = true;
	bIsFiring = false;

	PlayVoiceLine(ESHVoiceLineType::Reloading);

	// Reload duration varies by role
	float ReloadTime = 2.5f;
	switch (Role)
	{
	case ESHSquadRole::AutomaticRifleman: ReloadTime = 5.f; break;
	case ESHSquadRole::DesignatedMarksman: ReloadTime = 2.f; break;
	default: ReloadTime = 2.5f; break;
	}

	// Stress makes you fumble
	if (CombatStress >= ESHCombatStress::Stressed)
	{
		ReloadTime *= 1.3f;
	}
	if (CombatStress >= ESHCombatStress::Panicked)
	{
		ReloadTime *= 1.5f;
	}

	GetWorldTimerManager().SetTimer(ReloadTimerHandle, this, &ASHSquadMember::FinishReload,
									ReloadTime, false);
}

void ASHSquadMember::FinishReload()
{
	AmmoState.Reload();
	bIsReloading = false;
}

/* ────────────────────────────────────────────────────────────── */
/*  Voice / Callouts                                             */
/* ────────────────────────────────────────────────────────────── */

void ASHSquadMember::PlayVoiceLine(ESHVoiceLineType LineType)
{
	if (VoiceCooldownRemaining > 0.f || !IsAlive()) return;

	// Select variant based on combat stress
	USoundCue* SelectedCue = nullptr;

	if (CombatStress >= ESHCombatStress::Stressed)
	{
		if (USoundCue** Found = VoiceLinesStressedVariant.Find(LineType))
		{
			SelectedCue = *Found;
		}
	}
	else if (CombatStress == ESHCombatStress::Calm)
	{
		if (USoundCue** Found = VoiceLinesCalmVariant.Find(LineType))
		{
			SelectedCue = *Found;
		}
	}

	// Fallback to default
	if (!SelectedCue)
	{
		if (USoundCue** Found = VoiceLines.Find(LineType))
		{
			SelectedCue = *Found;
		}
	}

	if (SelectedCue && VoiceAudioComponent)
	{
		VoiceAudioComponent->SetSound(SelectedCue);
		VoiceAudioComponent->Play();
		VoiceCooldownRemaining = VoiceCooldownDuration;
	}
}

void ASHSquadMember::ReportContact(AActor* SpottedEnemy, const FVector& EnemyLocation)
{
	if (!SpottedEnemy || !IsAlive()) return;

	// Build contact report
	FSHContactReport Report;
	Report.Reporter = this;
	Report.ContactLocation = EnemyLocation;
	Report.Timestamp = GetWorld()->GetTimeSeconds();

	// Calculate distance
	const float DistCm = FVector::Dist(GetActorLocation(), EnemyLocation);
	Report.DistanceMeters = DistCm / 100.f; // UE units (cm) to meters

	// Calculate bearing from squad forward direction
	const FVector ToEnemy = (EnemyLocation - GetActorLocation()).GetSafeNormal2D();
	const FVector Forward = GetActorForwardVector().GetSafeNormal2D();

	float Bearing = FMath::Atan2(ToEnemy.Y, ToEnemy.X) - FMath::Atan2(Forward.Y, Forward.X);
	Bearing = FMath::RadiansToDegrees(Bearing);
	if (Bearing < 0.f) Bearing += 360.f;
	Report.BearingDegrees = Bearing;

	// Determine relative direction
	if (Bearing >= 315.f || Bearing < 45.f)
	{
		Report.Direction = ESHContactDirection::Front;
	}
	else if (Bearing >= 45.f && Bearing < 135.f)
	{
		Report.Direction = ESHContactDirection::Right;
	}
	else if (Bearing >= 135.f && Bearing < 225.f)
	{
		Report.Direction = ESHContactDirection::Rear;
	}
	else
	{
		Report.Direction = ESHContactDirection::Left;
	}

	OnContactSpotted.Broadcast(this, Report);
	PlayVoiceLine(ESHVoiceLineType::ContactReport);

	// Forward to squad manager
	if (OwningSquadManager.IsValid())
	{
		OwningSquadManager->OnContactReported(this, Report);
	}
}

/* ────────────────────────────────────────────────────────────── */
/*  Stat Queries                                                 */
/* ────────────────────────────────────────────────────────────── */

float ASHSquadMember::GetMovementSpeedModifier() const
{
	float Modifier = 1.f;

	// Wounds reduce speed
	switch (WoundState)
	{
	case ESHWoundState::LightWound:   Modifier *= 0.85f; break;
	case ESHWoundState::HeavyWound:   Modifier *= 0.5f;  break;
	case ESHWoundState::Incapacitated:
	case ESHWoundState::KIA:          return 0.f;
	default: break;
	}

	// Dragging a wounded teammate halves speed
	if (DraggingTarget.IsValid())
	{
		Modifier *= 0.4f;
	}

	// Heavy suppression makes movement sluggish (hunkering)
	if (SuppressionLevel >= ESHSuppressionLevel::Heavy)
	{
		Modifier *= 0.6f;
	}

	return Modifier;
}

float ASHSquadMember::GetAccuracyModifier() const
{
	float Modifier = 1.f;

	// Base accuracy from experience
	Modifier *= FMath::Lerp(0.6f, 1.1f, Personality.Experience);

	// Suppression degrades accuracy
	switch (SuppressionLevel)
	{
	case ESHSuppressionLevel::Light:    Modifier *= 0.85f; break;
	case ESHSuppressionLevel::Moderate: Modifier *= 0.65f; break;
	case ESHSuppressionLevel::Heavy:    Modifier *= 0.4f;  break;
	case ESHSuppressionLevel::Pinned:   Modifier *= 0.15f; break;
	default: break;
	}

	// Wounds degrade accuracy
	switch (WoundState)
	{
	case ESHWoundState::LightWound: Modifier *= 0.8f; break;
	case ESHWoundState::HeavyWound: Modifier *= 0.5f; break;
	default: break;
	}

	// Stress degrades accuracy
	switch (CombatStress)
	{
	case ESHCombatStress::Stressed: Modifier *= 0.8f; break;
	case ESHCombatStress::Panicked: Modifier *= 0.5f; break;
	default: break;
	}

	// Cover improves accuracy (stable shooting position)
	if (bIsInCover)
	{
		Modifier *= 1.15f;
	}

	return FMath::Clamp(Modifier, 0.05f, 1.5f);
}

bool ASHSquadMember::IsCombatEffective() const
{
	return IsAlive()
		&& WoundState != ESHWoundState::Incapacitated
		&& WoundState != ESHWoundState::KIA;
}

bool ASHSquadMember::IsAlive() const
{
	return WoundState != ESHWoundState::KIA;
}

bool ASHSquadMember::CanExecuteOrders() const
{
	return IsCombatEffective()
		&& SuppressionLevel != ESHSuppressionLevel::Pinned;
}

float ASHSquadMember::GetOptimalEngagementRange() const
{
	// In centimeters (UE units)
	switch (Role)
	{
	case ESHSquadRole::AutomaticRifleman:   return 30000.f;  // 300m — area suppression
	case ESHSquadRole::DesignatedMarksman:  return 50000.f;  // 500m — precision
	case ESHSquadRole::Grenadier:           return 25000.f;  // 250m — GL effective range
	case ESHSquadRole::Rifleman:
	case ESHSquadRole::TeamLeader:
	default:                                return 20000.f;  // 200m — standard rifle
	}
}

float ASHSquadMember::GetMaxEngagementRange() const
{
	switch (Role)
	{
	case ESHSquadRole::AutomaticRifleman:   return 80000.f;  // 800m
	case ESHSquadRole::DesignatedMarksman:  return 80000.f;  // 800m
	case ESHSquadRole::Grenadier:           return 40000.f;  // 400m
	case ESHSquadRole::Rifleman:
	case ESHSquadRole::TeamLeader:
	default:                                return 55000.f;  // 550m
	}
}

int32 ASHSquadMember::GetDesiredBurstLength() const
{
	if (bIsConservingAmmo)
	{
		return (Role == ESHSquadRole::AutomaticRifleman) ? 5 : 1;
	}

	switch (Role)
	{
	case ESHSquadRole::AutomaticRifleman:   return 10; // Sustained bursts for suppression
	case ESHSquadRole::DesignatedMarksman:  return 1;  // Single aimed shots
	case ESHSquadRole::Grenadier:           return 3;
	case ESHSquadRole::Rifleman:
	case ESHSquadRole::TeamLeader:
	default:                                return 3;  // 3-round bursts
	}
}

ASHSquadMember* ASHSquadMember::GetBuddyTeamPartner() const
{
	if (!OwningSquadManager.IsValid()) return nullptr;

	const TArray<ASHSquadMember*>& Members = OwningSquadManager->GetSquadMembers();
	for (ASHSquadMember* Member : Members)
	{
		if (Member && Member != this && Member->BuddyTeam == BuddyTeam && Member->IsAlive())
		{
			return Member;
		}
	}
	return nullptr;
}

/* ────────────────────────────────────────────────────────────── */
/*  Effectiveness                                                */
/* ────────────────────────────────────────────────────────────── */

void ASHSquadMember::RecalculateEffectiveness()
{
	float Eff = 1.f;

	// Wounds
	switch (WoundState)
	{
	case ESHWoundState::LightWound:    Eff *= 0.8f;  break;
	case ESHWoundState::HeavyWound:    Eff *= 0.45f; break;
	case ESHWoundState::Incapacitated: Eff = 0.f;    break;
	case ESHWoundState::KIA:           Eff = 0.f;    break;
	default: break;
	}

	// Morale
	Eff *= FMath::Lerp(0.3f, 1.f, Morale / 100.f);

	// Suppression
	switch (SuppressionLevel)
	{
	case ESHSuppressionLevel::Light:    Eff *= 0.9f;  break;
	case ESHSuppressionLevel::Moderate: Eff *= 0.7f;  break;
	case ESHSuppressionLevel::Heavy:    Eff *= 0.4f;  break;
	case ESHSuppressionLevel::Pinned:   Eff *= 0.1f;  break;
	default: break;
	}

	// Ammo
	if (AmmoState.IsAmmoOut()) Eff *= 0.2f;
	else if (bIsConservingAmmo) Eff *= 0.7f;

	EffectivenessModifier = FMath::Clamp(Eff, 0.f, 1.f);
}
