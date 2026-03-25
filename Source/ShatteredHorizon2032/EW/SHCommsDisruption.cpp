// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHCommsDisruption.h"
#include "ShatteredHorizon2032/ShatteredHorizon2032.h"
#include "SHElectronicWarfare.h"
#include "Components/AudioComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

USHCommsDisruption::USHCommsDisruption()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.f;

	RemainingBackupCommsUses = MaxBackupCommsUses;
}

void USHCommsDisruption::BeginPlay()
{
	Super::BeginPlay();

	RemainingBackupCommsUses = MaxBackupCommsUses;

	// Find the EW manager
	EWManager = ASHElectronicWarfare::GetEWManager(this);

	// Create radio static audio component
	if (AActor* Owner = GetOwner())
	{
		RadioStaticAudioComponent = NewObject<UAudioComponent>(Owner, TEXT("RadioStaticAudio"));
		if (RadioStaticAudioComponent)
		{
			RadioStaticAudioComponent->RegisterComponent();
			RadioStaticAudioComponent->AttachToComponent(Owner->GetRootComponent(),
				FAttachmentTransformRules::KeepRelativeTransform);
			RadioStaticAudioComponent->bAutoActivate = false;
			RadioStaticAudioComponent->bIsUISound = true; // Player always hears this
			RadioStaticAudioComponent->SetVolumeMultiplier(0.f);

			if (RadioStaticSound)
			{
				RadioStaticAudioComponent->SetSound(RadioStaticSound);
			}
		}
	}

	// Initialize crackle timing
	NextCrackleTime = FMath::FRandRange(2.f, 5.f);

	SH_LOG(LogSH_EW, Log, "CommsDisruption component initialized on %s",
		GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
}

void USHCommsDisruption::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	TickCommsStatus(DeltaTime);
	TickHUDDegradation(DeltaTime);
	TickAudioFeedback(DeltaTime);
	TickBackupComms(DeltaTime);
}

// =======================================================================
//  Comms status
// =======================================================================

void USHCommsDisruption::TickCommsStatus(float DeltaSeconds)
{
	EWQueryAccumulator += DeltaSeconds;
	if (EWQueryAccumulator < EWQueryInterval)
	{
		return;
	}
	EWQueryAccumulator -= EWQueryInterval;

	// Query EW manager for current effects at our position
	if (!EWManager.IsValid())
	{
		EWManager = ASHElectronicWarfare::GetEWManager(this);
	}

	if (!EWManager.IsValid() || !GetOwner())
	{
		return;
	}

	const FVector Position = GetOwner()->GetActorLocation();
	const FSHEWQueryResult EWResult = EWManager->QueryEWAtPosition(Position);

	// Store raw values
	float EffectiveCommsJamming = EWResult.CommsJammingStrength;
	CurrentGPSDenial = EWResult.GPSDenialStrength;
	CurrentRadarDisruption = EWResult.RadarDisruptionStrength;

	// Apply backup comms reduction
	if (bBackupCommsActive)
	{
		EffectiveCommsJamming *= (1.f - BackupCommsEffectiveness);
	}

	// Smooth the comms jamming value to avoid sudden jumps
	CurrentCommsJamming = FMath::FInterpTo(CurrentCommsJamming, EffectiveCommsJamming, EWQueryInterval, 3.f);

	// Compute new comms status
	const ESHCommsStatus OldStatus = CurrentCommsStatus;
	const ESHCommsStatus NewStatus = ComputeCommsStatus(CurrentCommsJamming);

	if (NewStatus != OldStatus)
	{
		CurrentCommsStatus = NewStatus;
		OnCommsStatusChanged.Broadcast(OldStatus, NewStatus);
		PlayCommsTransitionAudio(OldStatus, NewStatus);

		if (NewStatus == ESHCommsStatus::Clear && OldStatus != ESHCommsStatus::Clear)
		{
			OnCommsRestored.Broadcast();
		}

		SH_LOG(LogSH_EW, Log, "Comms status changed: %d -> %d (Jamming: %.2f)",
			static_cast<int32>(OldStatus), static_cast<int32>(NewStatus), CurrentCommsJamming);
	}

	// Update HUD degradation from EW values
	UpdateHUDDegradation(CurrentGPSDenial, CurrentCommsJamming, CurrentRadarDisruption);
}

ESHCommsStatus USHCommsDisruption::ComputeCommsStatus(float JammingStrength) const
{
	if (JammingStrength >= BlackoutThreshold) return ESHCommsStatus::Blackout;
	if (JammingStrength >= GarbledThreshold) return ESHCommsStatus::Garbled;
	if (JammingStrength >= HeavyStaticThreshold) return ESHCommsStatus::HeavyStatic;
	if (JammingStrength >= LightStaticThreshold) return ESHCommsStatus::LightStatic;
	return ESHCommsStatus::Clear;
}

// =======================================================================
//  Order disruption
// =======================================================================

FSHOrderDisruptionInfo USHCommsDisruption::EvaluateOrderDisruption() const
{
	FSHOrderDisruptionInfo Info;

	switch (CurrentCommsStatus)
	{
	case ESHCommsStatus::Clear:
		// No disruption
		Info.bDelayed = false;
		Info.bGarbled = false;
		Info.bLost = false;
		Info.bAckPossible = true;
		Info.AckProbability = 1.f;
		break;

	case ESHCommsStatus::LightStatic:
		// Minor delay, full acknowledgment
		Info.bDelayed = true;
		Info.DelaySeconds = BaseOrderDelay * 0.5f;
		Info.bGarbled = false;
		Info.bLost = false;
		Info.bAckPossible = true;
		Info.AckProbability = 0.95f;
		break;

	case ESHCommsStatus::HeavyStatic:
		// Noticeable delay, possible garbling
		Info.bDelayed = true;
		Info.DelaySeconds = BaseOrderDelay + FMath::FRandRange(0.f, MaxRandomDelay * 0.5f);
		Info.bGarbled = (FMath::FRand() < GarbleProbability * 0.5f);
		Info.bLost = false;
		Info.bAckPossible = true;
		Info.AckProbability = 0.7f;
		break;

	case ESHCommsStatus::Garbled:
		// Significant delay, high garble chance, may lose order
		Info.bDelayed = true;
		Info.DelaySeconds = BaseOrderDelay + FMath::FRandRange(0.f, MaxRandomDelay);
		Info.bGarbled = (FMath::FRand() < GarbleProbability);
		Info.bLost = (FMath::FRand() < OrderLossProbability * 0.3f);
		Info.bAckPossible = !Info.bLost;
		Info.AckProbability = 0.35f;
		break;

	case ESHCommsStatus::Blackout:
		// Orders almost certainly lost
		Info.bDelayed = true;
		Info.DelaySeconds = MaxRandomDelay;
		Info.bGarbled = true;
		Info.bLost = (FMath::FRand() < OrderLossProbability);
		Info.bAckPossible = false;
		Info.AckProbability = 0.05f;
		break;
	}

	return Info;
}

bool USHCommsDisruption::ShouldReceiveAcknowledgment() const
{
	const FSHOrderDisruptionInfo Info = EvaluateOrderDisruption();
	if (!Info.bAckPossible)
	{
		return false;
	}
	return FMath::FRand() < Info.AckProbability;
}

float USHCommsDisruption::GetSquadAutonomyMultiplier() const
{
	// Squad acts more autonomously when comms are degraded
	const float JammingFactor = FMath::Clamp(CurrentCommsJamming, 0.f, 1.f);
	return FMath::Lerp(BaseAutonomyMultiplier, MaxAutonomyMultiplier, JammingFactor);
}

// =======================================================================
//  HUD degradation
// =======================================================================

void USHCommsDisruption::TickHUDDegradation(float DeltaSeconds)
{
	// Compass drift oscillation
	CompassDriftPhase += DeltaSeconds * CompassDriftFrequency * 2.f * PI;
	if (CompassDriftPhase > 2.f * PI)
	{
		CompassDriftPhase -= 2.f * PI;
	}

	// Target drift based on GPS denial
	TargetCompassDrift = MaxCompassDrift * CurrentGPSDenial * FMath::Sin(CompassDriftPhase);

	// Add random jitter at high denial levels
	if (CurrentGPSDenial > 0.5f)
	{
		TargetCompassDrift += FMath::FRandRange(-5.f, 5.f) * CurrentGPSDenial;
	}

	CurrentCompassDrift = FMath::FInterpTo(CurrentCompassDrift, TargetCompassDrift, DeltaSeconds, 2.f);

	// GPS error drift
	if (CurrentGPSDenial > 0.1f)
	{
		// Slowly drift the GPS error
		if (FMath::FRand() < DeltaSeconds * 0.5f)
		{
			TargetGPSError = FVector(
				FMath::FRandRange(-1.f, 1.f),
				FMath::FRandRange(-1.f, 1.f),
				0.f) * MaxGPSError * CurrentGPSDenial;
		}
		GPSErrorOffset = FMath::VInterpTo(GPSErrorOffset, TargetGPSError, DeltaSeconds, 0.5f);
	}
	else
	{
		GPSErrorOffset = FMath::VInterpTo(GPSErrorOffset, FVector::ZeroVector, DeltaSeconds, 2.f);
	}
}

void USHCommsDisruption::UpdateHUDDegradation(float GPSDenial, float CommsJamming, float RadarDisruption)
{
	FSHHUDDegradationInfo OldInfo = CurrentHUDDegradation;

	// Minimap clarity: affected by comms jamming and radar disruption
	float MinimapDegradation = FMath::Max(CommsJamming, RadarDisruption);
	CurrentHUDDegradation.MinimapClarity = 1.f - FMath::Clamp(MinimapDegradation * 1.2f, 0.f, 1.f);

	// Compass accuracy
	CurrentHUDDegradation.CompassAccuracy = 1.f - FMath::Clamp(GPSDenial, 0.f, 1.f);
	CurrentHUDDegradation.CompassDriftDegrees = CurrentCompassDrift;

	// Squad markers: degrade with comms jamming
	CurrentHUDDegradation.SquadMarkerVisibility = 1.f - FMath::Clamp(CommsJamming * 0.8f, 0.f, 1.f);

	// Waypoint visibility: affected by GPS denial
	CurrentHUDDegradation.WaypointVisibility = 1.f - FMath::Clamp(GPSDenial * 0.9f, 0.f, 1.f);

	// Objective markers: affected by both comms and GPS
	float ObjDegradation = FMath::Max(CommsJamming * 0.5f, GPSDenial * 0.7f);
	CurrentHUDDegradation.ObjectiveMarkerVisibility = 1.f - FMath::Clamp(ObjDegradation, 0.f, 1.f);

	// GPS noise
	CurrentHUDDegradation.bGPSNoisy = GPSDenial > 0.1f;
	CurrentHUDDegradation.GPSErrorCm = GPSErrorOffset.Size();

	// Radio static overlay
	CurrentHUDDegradation.RadioStaticOverlay = ASHElectronicWarfare::ComputeRadioStaticIntensity(CommsJamming);

	// Broadcast if anything materially changed
	bool bChanged = false;
	bChanged |= FMath::Abs(OldInfo.MinimapClarity - CurrentHUDDegradation.MinimapClarity) > 0.05f;
	bChanged |= FMath::Abs(OldInfo.CompassAccuracy - CurrentHUDDegradation.CompassAccuracy) > 0.05f;
	bChanged |= FMath::Abs(OldInfo.RadioStaticOverlay - CurrentHUDDegradation.RadioStaticOverlay) > 0.05f;

	if (bChanged)
	{
		OnHUDDegradationChanged.Broadcast(CurrentHUDDegradation);
	}
}

float USHCommsDisruption::GetDriftedCompassBearing(float TrueBearing) const
{
	float Drifted = TrueBearing + CurrentCompassDrift;
	// Wrap to 0-360
	while (Drifted < 0.f) Drifted += 360.f;
	while (Drifted >= 360.f) Drifted -= 360.f;
	return Drifted;
}

float USHCommsDisruption::GetMinimapStaticIntensity() const
{
	return 1.f - CurrentHUDDegradation.MinimapClarity;
}

// =======================================================================
//  Backup comms
// =======================================================================

bool USHCommsDisruption::ActivateBackupComms()
{
	if (bBackupCommsActive)
	{
		SH_WARNING(LogSH_EW, "Backup comms already active.");
		return false;
	}

	if (RemainingBackupCommsUses <= 0)
	{
		SH_WARNING(LogSH_EW, "No backup comms uses remaining.");
		return false;
	}

	RemainingBackupCommsUses--;
	bBackupCommsActive = true;
	BackupCommsTimer = BackupCommsDuration;

	SH_LOG(LogSH_EW, Log, "Backup comms activated. Duration: %.0fs, Remaining uses: %d",
		BackupCommsDuration, RemainingBackupCommsUses);

	return true;
}

void USHCommsDisruption::TickBackupComms(float DeltaSeconds)
{
	if (!bBackupCommsActive)
	{
		return;
	}

	BackupCommsTimer -= DeltaSeconds;
	if (BackupCommsTimer <= 0.f)
	{
		bBackupCommsActive = false;
		BackupCommsTimer = 0.f;

		SH_LOG(LogSH_EW, Log, "Backup comms expired.");
	}
}

// =======================================================================
//  Audio feedback
// =======================================================================

float USHCommsDisruption::GetRadioStaticVolume() const
{
	if (CurrentCommsStatus == ESHCommsStatus::Clear)
	{
		return 0.f;
	}

	float Volume = 0.f;
	switch (CurrentCommsStatus)
	{
	case ESHCommsStatus::LightStatic:
		Volume = 0.15f;
		break;
	case ESHCommsStatus::HeavyStatic:
		Volume = 0.4f;
		break;
	case ESHCommsStatus::Garbled:
		Volume = 0.65f;
		break;
	case ESHCommsStatus::Blackout:
		Volume = 0.85f;
		break;
	default:
		break;
	}

	// Backup comms reduce static
	if (bBackupCommsActive)
	{
		Volume *= (1.f - BackupCommsEffectiveness);
	}

	return Volume;
}

void USHCommsDisruption::TickAudioFeedback(float DeltaSeconds)
{
	const float TargetVolume = GetRadioStaticVolume();

	// Update radio static loop
	if (RadioStaticAudioComponent)
	{
		if (TargetVolume > 0.01f)
		{
			if (!RadioStaticAudioComponent->IsPlaying() && RadioStaticSound)
			{
				RadioStaticAudioComponent->Play();
			}

			// Smooth volume transition
			const float CurrentVolume = RadioStaticAudioComponent->VolumeMultiplier;
			const float NewVolume = FMath::FInterpTo(CurrentVolume, TargetVolume, DeltaSeconds, 4.f);
			RadioStaticAudioComponent->SetVolumeMultiplier(NewVolume);

			// Slight pitch variation for realism
			float PitchVariation = 1.f + FMath::Sin(GetWorld()->GetTimeSeconds() * 3.f) * 0.05f;
			RadioStaticAudioComponent->SetPitchMultiplier(PitchVariation);
		}
		else
		{
			if (RadioStaticAudioComponent->IsPlaying())
			{
				// Fade out
				const float CurrentVolume = RadioStaticAudioComponent->VolumeMultiplier;
				if (CurrentVolume > 0.01f)
				{
					RadioStaticAudioComponent->SetVolumeMultiplier(
						FMath::FInterpTo(CurrentVolume, 0.f, DeltaSeconds, 6.f));
				}
				else
				{
					RadioStaticAudioComponent->Stop();
				}
			}
		}
	}

	// Intermittent crackle sounds at moderate jamming levels
	if (CurrentCommsJamming > LightStaticThreshold && CurrentCommsJamming < BlackoutThreshold)
	{
		CrackleAccumulator += DeltaSeconds;
		if (CrackleAccumulator >= NextCrackleTime)
		{
			CrackleAccumulator = 0.f;

			// Higher jamming = more frequent crackles
			float CrackleFrequency = FMath::Lerp(4.f, 0.5f, CurrentCommsJamming);
			NextCrackleTime = FMath::FRandRange(CrackleFrequency * 0.5f, CrackleFrequency * 1.5f);

			if (RadioCrackleSound && GetOwner())
			{
				UGameplayStatics::PlaySoundAtLocation(GetWorld(), RadioCrackleSound,
					GetOwner()->GetActorLocation(), CurrentCommsJamming);
			}
		}
	}
}

void USHCommsDisruption::PlayCommsTransitionAudio(ESHCommsStatus OldStatus, ESHCommsStatus NewStatus)
{
	if (!GetOwner())
	{
		return;
	}

	const FVector Location = GetOwner()->GetActorLocation();

	// Entering jamming
	if (OldStatus == ESHCommsStatus::Clear && NewStatus != ESHCommsStatus::Clear)
	{
		if (CommsJammedSound)
		{
			UGameplayStatics::PlaySoundAtLocation(GetWorld(), CommsJammedSound, Location);
		}
	}
	// Comms restored
	else if (OldStatus != ESHCommsStatus::Clear && NewStatus == ESHCommsStatus::Clear)
	{
		if (CommsRestoredSound)
		{
			UGameplayStatics::PlaySoundAtLocation(GetWorld(), CommsRestoredSound, Location);
		}
	}
}
