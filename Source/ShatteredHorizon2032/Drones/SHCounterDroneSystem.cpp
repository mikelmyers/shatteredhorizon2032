// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHCounterDroneSystem.h"
#include "ShatteredHorizon2032/ShatteredHorizon2032.h"
#include "Components/AudioComponent.h"
#include "NiagaraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"

ASHCounterDroneSystem::ASHCounterDroneSystem()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	// --- Root ---
	RootScene = CreateDefaultSubobject<USceneComponent>(TEXT("RootScene"));
	SetRootComponent(RootScene);

	// --- Detection sphere ---
	DetectionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("DetectionSphere"));
	DetectionSphere->SetupAttachment(RootScene);
	DetectionSphere->InitSphereRadius(DetectionRange);
	DetectionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DetectionSphere->SetHiddenInGame(true);

	// --- Jamming sphere ---
	JammingSphere = CreateDefaultSubobject<USphereComponent>(TEXT("JammingSphere"));
	JammingSphere->SetupAttachment(RootScene);
	JammingSphere->InitSphereRadius(JammingRange);
	JammingSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	JammingSphere->SetHiddenInGame(true);

	// --- Audio ---
	ScanBeepAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("ScanBeepAudio"));
	ScanBeepAudio->SetupAttachment(RootScene);
	ScanBeepAudio->bAutoActivate = false;
	ScanBeepAudio->bIsUISound = true;

	JammingNoiseAudio = CreateDefaultSubobject<UAudioComponent>(TEXT("JammingNoiseAudio"));
	JammingNoiseAudio->SetupAttachment(RootScene);
	JammingNoiseAudio->bAutoActivate = false;

	// --- VFX ---
	JammingFieldVFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("JammingFieldVFX"));
	JammingFieldVFX->SetupAttachment(RootScene);
	JammingFieldVFX->bAutoActivate = false;

	RemainingCharges = MaxCharges;
}

void ASHCounterDroneSystem::BeginPlay()
{
	Super::BeginPlay();

	RemainingCharges = MaxCharges;
	CurrentPower = 1.f;

	// Update sphere radii from config
	DetectionSphere->SetSphereRadius(DetectionRange);
	JammingSphere->SetSphereRadius(JammingRange);

	// Configure audio
	if (JammingActiveSound && JammingNoiseAudio)
	{
		JammingNoiseAudio->SetSound(JammingActiveSound);
	}

	SH_LOG(LogSH_Drone, Log, "Counter-Drone System BeginPlay — DetectionRange: %.0f, JammingRange: %.0f, Charges: %d",
		DetectionRange, JammingRange, MaxCharges);
}

void ASHCounterDroneSystem::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	switch (CurrentMode)
	{
	case ESHCounterDroneMode::Scanning:
		TickScanning(DeltaSeconds);
		break;
	case ESHCounterDroneMode::Jamming:
		TickScanning(DeltaSeconds); // Continue scanning while jamming
		TickJamming(DeltaSeconds);
		break;
	case ESHCounterDroneMode::Cooldown:
		TickScanning(DeltaSeconds); // Continue scanning during cooldown
		TickCooldown(DeltaSeconds);
		break;
	default:
		break;
	}

	if (CurrentMode != ESHCounterDroneMode::Inactive)
	{
		TickAudioFeedback(DeltaSeconds);
	}
}

// =======================================================================
//  Activation
// =======================================================================

void ASHCounterDroneSystem::Activate()
{
	if (CurrentMode != ESHCounterDroneMode::Inactive)
	{
		return;
	}

	SetMode(ESHCounterDroneMode::Scanning);
	SH_LOG(LogSH_Drone, Log, "Counter-Drone System activated.");
}

void ASHCounterDroneSystem::Deactivate()
{
	if (CurrentMode == ESHCounterDroneMode::Jamming)
	{
		StopJamming();
	}

	SetMode(ESHCounterDroneMode::Inactive);
	DetectedDrones.Empty();

	SH_LOG(LogSH_Drone, Log, "Counter-Drone System deactivated.");
}

bool ASHCounterDroneSystem::BeginJamming()
{
	if (!CanJam())
	{
		SH_WARNING(LogSH_Drone, "Cannot begin jamming — not ready (Mode: %d, Charges: %d, Cooldown: %.1f)",
			static_cast<int32>(CurrentMode), RemainingCharges, CooldownTimer);
		return false;
	}

	RemainingCharges--;
	JammingTimer = 0.f;
	SetMode(ESHCounterDroneMode::Jamming);

	// Start jamming audio
	if (JammingNoiseAudio && JammingActiveSound)
	{
		JammingNoiseAudio->Play();
	}

	// Activate jamming VFX
	if (JammingFieldVFX)
	{
		JammingFieldVFX->Activate();
	}

	SH_LOG(LogSH_Drone, Log, "Counter-Drone jamming started. Charges remaining: %d", RemainingCharges);
	return true;
}

void ASHCounterDroneSystem::StopJamming()
{
	if (CurrentMode != ESHCounterDroneMode::Jamming)
	{
		return;
	}

	// Clear jamming on all affected drones
	for (FSHDroneDetectionEntry& Entry : DetectedDrones)
	{
		if (Entry.bBeingJammed && Entry.Drone.IsValid())
		{
			Entry.Drone->ClearJammingInterference();
			Entry.bBeingJammed = false;
		}
	}

	// Stop jamming audio and VFX
	if (JammingNoiseAudio)
	{
		JammingNoiseAudio->Stop();
	}
	if (JammingFieldVFX)
	{
		JammingFieldVFX->Deactivate();
	}

	// Enter cooldown
	CooldownTimer = JammingCooldown;
	SetMode(ESHCounterDroneMode::Cooldown);

	SH_LOG(LogSH_Drone, Log, "Counter-Drone jamming stopped. Entering cooldown: %.1fs", JammingCooldown);
}

bool ASHCounterDroneSystem::CanJam() const
{
	return (CurrentMode == ESHCounterDroneMode::Scanning)
		&& (RemainingCharges > 0)
		&& (CooldownTimer <= 0.f);
}

// =======================================================================
//  Detection queries
// =======================================================================

bool ASHCounterDroneSystem::GetClosestDrone(FSHDroneDetectionEntry& OutEntry) const
{
	if (DetectedDrones.Num() == 0)
	{
		return false;
	}

	float ClosestDist = MAX_FLT;
	int32 ClosestIdx = -1;

	for (int32 i = 0; i < DetectedDrones.Num(); ++i)
	{
		if (DetectedDrones[i].Drone.IsValid() && DetectedDrones[i].Distance < ClosestDist)
		{
			ClosestDist = DetectedDrones[i].Distance;
			ClosestIdx = i;
		}
	}

	if (ClosestIdx >= 0)
	{
		OutEntry = DetectedDrones[ClosestIdx];
		return true;
	}

	return false;
}

bool ASHCounterDroneSystem::HasDronesInRange(float RangeCm) const
{
	for (const FSHDroneDetectionEntry& Entry : DetectedDrones)
	{
		if (Entry.Drone.IsValid() && Entry.Distance <= RangeCm)
		{
			return true;
		}
	}
	return false;
}

float ASHCounterDroneSystem::GetCooldownFraction() const
{
	return (JammingCooldown > 0.f) ? FMath::Clamp(CooldownTimer / JammingCooldown, 0.f, 1.f) : 0.f;
}

float ASHCounterDroneSystem::GetPowerLevel() const
{
	return CurrentPower;
}

bool ASHCounterDroneSystem::GetNearestDroneIndicator(FVector& OutDirection, float& OutDistance) const
{
	FSHDroneDetectionEntry Closest;
	if (GetClosestDrone(Closest))
	{
		OutDirection = Closest.Direction;
		OutDistance = Closest.Distance;
		return true;
	}
	return false;
}

bool ASHCounterDroneSystem::ShouldPlayDroneAudioCue() const
{
	return HasDronesInRange(AudioDetectionRange) && CurrentMode != ESHCounterDroneMode::Inactive;
}

// =======================================================================
//  Tick helpers
// =======================================================================

void ASHCounterDroneSystem::TickScanning(float DeltaSeconds)
{
	ScanAccumulator += DeltaSeconds;
	if (ScanAccumulator >= ScanInterval)
	{
		ScanAccumulator -= ScanInterval;
		PerformDroneScan();
	}
}

void ASHCounterDroneSystem::TickJamming(float DeltaSeconds)
{
	JammingTimer += DeltaSeconds;

	// Drain power
	CurrentPower -= PowerDrainRate * DeltaSeconds;

	if (JammingTimer >= MaxJammingDuration || CurrentPower <= 0.f)
	{
		CurrentPower = FMath::Max(CurrentPower, 0.f);
		StopJamming();
		return;
	}

	// Apply jamming to all drones in range
	ApplyJammingToTargets();
}

void ASHCounterDroneSystem::TickCooldown(float DeltaSeconds)
{
	CooldownTimer -= DeltaSeconds;

	// Recharge power during cooldown
	CurrentPower = FMath::Min(CurrentPower + PowerDrainRate * 0.5f * DeltaSeconds, 1.f);

	if (CooldownTimer <= 0.f)
	{
		CooldownTimer = 0.f;
		SetMode(ESHCounterDroneMode::Scanning);
		OnJammerCooldownComplete.Broadcast(this);
		SH_LOG(LogSH_Drone, Log, "Counter-Drone cooldown complete. Ready to jam.");
	}
}

void ASHCounterDroneSystem::TickAudioFeedback(float DeltaSeconds)
{
	// Play periodic beeps based on detected drones
	if (DetectedDrones.Num() == 0)
	{
		bCloseWarningPlaying = false;
		return;
	}

	// Close warning takes priority
	bool bHasCloseTarget = HasDronesInRange(CloseWarningRange);

	if (bHasCloseTarget && !bCloseWarningPlaying)
	{
		if (DroneCloseWarning)
		{
			UGameplayStatics::PlaySoundAtLocation(GetWorld(), DroneCloseWarning, GetActorLocation());
		}
		bCloseWarningPlaying = true;
	}
	else if (!bHasCloseTarget)
	{
		bCloseWarningPlaying = false;
	}

	// Regular detection beeps
	if (!bCloseWarningPlaying)
	{
		// Beep interval inversely proportional to nearest drone distance
		FSHDroneDetectionEntry Nearest;
		if (GetClosestDrone(Nearest))
		{
			float NormalizedDist = FMath::Clamp(Nearest.Distance / DetectionRange, 0.f, 1.f);
			float BeepInterval = FMath::Lerp(0.3f, 2.0f, NormalizedDist);

			AudioCueAccumulator += DeltaSeconds;
			if (AudioCueAccumulator >= BeepInterval)
			{
				AudioCueAccumulator -= BeepInterval;
				if (DroneDetectedBeep && ScanBeepAudio)
				{
					ScanBeepAudio->SetSound(DroneDetectedBeep);
					ScanBeepAudio->Play();
				}
			}
		}
	}
}

void ASHCounterDroneSystem::PerformDroneScan()
{
	const FVector SystemLocation = GetActorLocation();

	// Track which drones we still see this scan
	TSet<TWeakObjectPtr<ASHDroneBase>> CurrentlyVisible;

	// Iterate all drones in the world
	for (TActorIterator<ASHDroneBase> It(GetWorld()); It; ++It)
	{
		ASHDroneBase* Drone = *It;
		if (!Drone || Drone->GetDroneState() == ESHDroneState::Inactive ||
			Drone->GetDroneState() == ESHDroneState::Destroyed)
		{
			continue;
		}

		const float Distance = FVector::Dist(SystemLocation, Drone->GetActorLocation());

		// Check if in detection range
		if (Distance > DetectionRange)
		{
			continue;
		}

		// Check noise level (audio detection aspect)
		const float DroneNoise = Drone->GetNoiseLevel();
		bool bDetectedByNoise = (DroneNoise >= MinDetectableNoise) && (Distance <= AudioDetectionRange);
		bool bDetectedByRadar = (Distance <= DetectionRange);

		if (!bDetectedByNoise && !bDetectedByRadar)
		{
			continue;
		}

		CurrentlyVisible.Add(TWeakObjectPtr<ASHDroneBase>(Drone));

		// Compute detection info
		const FVector DirectionToDrone = (Drone->GetActorLocation() - SystemLocation).GetSafeNormal();
		const float Bearing = ComputeBearing(Drone->GetActorLocation());

		// Detection confidence
		float Confidence = 1.f - FMath::Clamp(Distance / DetectionRange, 0.f, 1.f);
		if (bDetectedByNoise)
		{
			Confidence = FMath::Max(Confidence, DroneNoise);
		}

		// Update or add detection entry
		bool bExisting = false;
		for (FSHDroneDetectionEntry& Entry : DetectedDrones)
		{
			if (Entry.Drone == Drone)
			{
				Entry.Distance = Distance;
				Entry.Direction = DirectionToDrone;
				Entry.Bearing = Bearing;
				Entry.Confidence = Confidence;
				Entry.TimeTracked += ScanInterval;
				bExisting = true;
				break;
			}
		}

		if (!bExisting)
		{
			FSHDroneDetectionEntry NewEntry;
			NewEntry.Drone = Drone;
			NewEntry.Distance = Distance;
			NewEntry.Direction = DirectionToDrone;
			NewEntry.Bearing = Bearing;
			NewEntry.Confidence = Confidence;
			NewEntry.TimeTracked = 0.f;
			NewEntry.bBeingJammed = false;
			DetectedDrones.Add(NewEntry);

			OnDroneDetectedDelegate.Broadcast(Drone, Distance);

			SH_LOG(LogSH_Drone, Verbose, "Counter-Drone detected: %s at %.0fm, Bearing: %.0f",
				*Drone->GetName(), Distance / 100.f, Bearing);
		}
	}

	// Remove drones that are no longer visible
	for (int32 i = DetectedDrones.Num() - 1; i >= 0; --i)
	{
		if (!DetectedDrones[i].Drone.IsValid() || !CurrentlyVisible.Contains(DetectedDrones[i].Drone))
		{
			if (DetectedDrones[i].Drone.IsValid())
			{
				// Clear jamming if we were jamming this drone
				if (DetectedDrones[i].bBeingJammed)
				{
					DetectedDrones[i].Drone->ClearJammingInterference();
				}
				OnDroneLostTracking.Broadcast(DetectedDrones[i].Drone.Get());
			}
			DetectedDrones.RemoveAt(i);
		}
	}
}

void ASHCounterDroneSystem::ApplyJammingToTargets()
{
	const FVector SystemLocation = GetActorLocation();

	for (FSHDroneDetectionEntry& Entry : DetectedDrones)
	{
		if (!Entry.Drone.IsValid())
		{
			continue;
		}

		const float Distance = Entry.Distance;
		if (Distance > JammingRange)
		{
			// Out of jamming range — clear if previously jammed
			if (Entry.bBeingJammed)
			{
				Entry.Drone->ClearJammingInterference();
				Entry.bBeingJammed = false;
			}
			continue;
		}

		// Jamming power decreases with distance
		const float EffectivePower = JammingPower * (1.f - FMath::Clamp(Distance / JammingRange, 0.f, 1.f));

		Entry.Drone->ApplyJammingInterference(EffectivePower, SystemLocation);
		Entry.bBeingJammed = true;

		// Apply specific jammer effect when power is sufficient
		if (EffectivePower > 0.5f && Entry.Drone->GetDroneState() != ESHDroneState::Jammed &&
			Entry.Drone->GetDroneState() != ESHDroneState::Destroyed)
		{
			ApplyJammerEffectToDrone(Entry.Drone.Get());
			OnDroneJammed.Broadcast(Entry.Drone.Get(), JammerEffect);
		}
	}
}

void ASHCounterDroneSystem::ApplyJammerEffectToDrone(ASHDroneBase* Drone)
{
	if (!Drone)
	{
		return;
	}

	switch (JammerEffect)
	{
	case ESHJammerEffect::ForceLand:
		// Force the drone to descend and land
		Drone->SetDroneState(ESHDroneState::Jammed);
		Drone->SetFlightTarget(FVector(
			Drone->GetActorLocation().X,
			Drone->GetActorLocation().Y,
			0.f));
		SH_LOG(LogSH_Drone, Log, "Jammer forcing drone to land: %s", *Drone->GetName());
		break;

	case ESHJammerEffect::ForceReturn:
		// Force RTH
		Drone->ReturnToOperator();
		SH_LOG(LogSH_Drone, Log, "Jammer forcing drone RTH: %s", *Drone->GetName());
		break;

	case ESHJammerEffect::LoseControl:
		// Drone enters jammed/erratic state
		Drone->SetDroneState(ESHDroneState::Jammed);
		SH_LOG(LogSH_Drone, Log, "Jammer causing drone to lose control: %s", *Drone->GetName());
		break;

	case ESHJammerEffect::Disorient:
		// Drone drifts randomly (handled by drone's jammed tick)
		Drone->SetDroneState(ESHDroneState::Jammed);
		SH_LOG(LogSH_Drone, Log, "Jammer disorienting drone: %s", *Drone->GetName());
		break;
	}
}

void ASHCounterDroneSystem::SetMode(ESHCounterDroneMode NewMode)
{
	if (NewMode == CurrentMode)
	{
		return;
	}

	CurrentMode = NewMode;
	SH_LOG(LogSH_Drone, Log, "Counter-Drone mode: %d", static_cast<int32>(NewMode));
}

float ASHCounterDroneSystem::ComputeBearing(const FVector& TargetPos) const
{
	const FVector SystemPos = GetActorLocation();
	const FVector Delta = TargetPos - SystemPos;

	// Compute bearing: 0 = North (+Y in UE), clockwise
	float BearingRad = FMath::Atan2(Delta.X, Delta.Y);
	float BearingDeg = FMath::RadiansToDegrees(BearingRad);

	if (BearingDeg < 0.f)
	{
		BearingDeg += 360.f;
	}

	return BearingDeg;
}
