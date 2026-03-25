// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHDroneBase.h"
#include "ShatteredHorizon2032/ShatteredHorizon2032.h"
#include "Net/UnrealNetwork.h"
#include "Components/SphereComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "NiagaraFunctionLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

ASHDroneBase::ASHDroneBase()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
	bReplicates = true;
	SetReplicatingMovement(true);

	// --- Collision root ---
	CollisionSphere = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionSphere"));
	CollisionSphere->InitSphereRadius(30.f);
	CollisionSphere->SetCollisionProfileName(TEXT("Pawn"));
	CollisionSphere->SetCollisionObjectType(SH_COLLISION_DRONE);
	CollisionSphere->SetGenerateOverlapEvents(true);
	SetRootComponent(CollisionSphere);

	// --- Body mesh ---
	BodyMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMesh"));
	BodyMesh->SetupAttachment(CollisionSphere);
	BodyMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// --- Camera gimbal mesh ---
	CameraGimbalMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CameraGimbalMesh"));
	CameraGimbalMesh->SetupAttachment(BodyMesh);
	CameraGimbalMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CameraGimbalMesh->SetRelativeLocation(FVector(0.f, 0.f, -10.f));

	// --- Floating pawn movement ---
	FloatingMovement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("FloatingMovement"));
	FloatingMovement->MaxSpeed = MaxFlightSpeed;
	FloatingMovement->Acceleration = FlightAcceleration;
	FloatingMovement->Deceleration = FlightAcceleration * 0.5f;

	// --- Motor audio ---
	MotorAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("MotorAudio"));
	MotorAudioComponent->SetupAttachment(CollisionSphere);
	MotorAudioComponent->bAutoActivate = false;
	MotorAudioComponent->bIsUISound = false;

	// --- LED light ---
	LEDLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("LEDLight"));
	LEDLight->SetupAttachment(BodyMesh);
	LEDLight->SetRelativeLocation(FVector(0.f, 0.f, 5.f));
	LEDLight->SetIntensity(500.f);
	LEDLight->SetAttenuationRadius(200.f);
	LEDLight->SetLightColor(FLinearColor::Green);
	LEDLight->SetVisibility(false);

	// --- Niagara VFX ---
	RotorWashVFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("RotorWashVFX"));
	RotorWashVFX->SetupAttachment(CollisionSphere);
	RotorWashVFX->bAutoActivate = false;

	TrailVFX = CreateDefaultSubobject<UNiagaraComponent>(TEXT("TrailVFX"));
	TrailVFX->SetupAttachment(CollisionSphere);
	TrailVFX->bAutoActivate = false;

	// --- Defaults ---
	CurrentHealth = MaxHealth;
	CurrentBatterySeconds = MaxBatterySeconds;
	CurrentCruiseAltitude = DefaultCruiseAltitude;
}

// =======================================================================
//  Lifecycle
// =======================================================================

void ASHDroneBase::BeginPlay()
{
	Super::BeginPlay();

	CurrentHealth = MaxHealth;
	CurrentBatterySeconds = MaxBatterySeconds;
	CurrentCruiseAltitude = DefaultCruiseAltitude;

	// Configure motor audio
	if (MotorLoopSound)
	{
		MotorAudioComponent->SetSound(MotorLoopSound);
		if (MotorAttenuationSettings)
		{
			MotorAudioComponent->AttenuationSettings = MotorAttenuationSettings;
		}
	}

	// Configure movement component
	FloatingMovement->MaxSpeed = MaxFlightSpeed;
	FloatingMovement->Acceleration = FlightAcceleration;
	FloatingMovement->Deceleration = FlightAcceleration * 0.5f;

	SH_LOG(LogSH_Drone, Log, "DroneBase BeginPlay — Affiliation: %d, MaxBattery: %.0fs",
		static_cast<int32>(Affiliation), MaxBatterySeconds);
}

void ASHDroneBase::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (CurrentState == ESHDroneState::Inactive || CurrentState == ESHDroneState::Destroyed)
	{
		return;
	}

	TickBattery(DeltaSeconds);
	TickSignalLink(DeltaSeconds);
	TickNoise(DeltaSeconds);
	UpdateMotorAudio();

	switch (CurrentState)
	{
	case ESHDroneState::Launching:
		TickLaunchSequence(DeltaSeconds);
		break;

	case ESHDroneState::Flying:
	case ESHDroneState::Loitering:
	case ESHDroneState::Attacking:
	case ESHDroneState::Returning:
		TickFlight(DeltaSeconds);
		break;

	case ESHDroneState::Jammed:
		TickJammedBehavior(DeltaSeconds);
		break;

	default:
		break;
	}

	// Subclass-specific behavior
	TickDroneBehavior(DeltaSeconds);
}

float ASHDroneBase::TakeDamage(float DamageAmount, FDamageEvent const& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	if (CurrentState == ESHDroneState::Destroyed || CurrentState == ESHDroneState::Inactive)
	{
		return 0.f;
	}

	float FinalDamage = Super::TakeDamage(DamageAmount, DamageEvent, EventInstigator, DamageCauser);

	// Apply damage type multipliers based on damage event
	// Subclasses and the damage system can tag the damage causer appropriately.
	// For now, we apply the base damage directly.
	CurrentHealth = FMath::Max(CurrentHealth - FinalDamage, 0.f);

	SH_LOG(LogSH_Drone, Verbose, "Drone took %.1f damage — Health: %.1f/%.1f",
		FinalDamage, CurrentHealth, MaxHealth);

	if (CurrentHealth <= 0.f)
	{
		DestroyDrone(DamageCauser);
	}

	return FinalDamage;
}

void ASHDroneBase::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASHDroneBase, CurrentState);
	DOREPLIFETIME(ASHDroneBase, CurrentHealth);
	DOREPLIFETIME(ASHDroneBase, CurrentBatterySeconds);
}

void ASHDroneBase::OnRep_DroneState()
{
	// Client-side state change handling — update visuals/audio
	UpdateMotorAudio();

	if (CurrentState == ESHDroneState::Destroyed)
	{
		SpawnDestructionEffects();
	}
}

// =======================================================================
//  State management
// =======================================================================

void ASHDroneBase::SetDroneState(ESHDroneState NewState)
{
	if (NewState == CurrentState)
	{
		return;
	}

	const ESHDroneState OldState = CurrentState;
	OnStateExited(OldState);

	CurrentState = NewState;
	OnStateEntered(NewState);

	OnDroneStateChanged.Broadcast(OldState, NewState);

	SH_LOG(LogSH_Drone, Log, "Drone state changed: %d -> %d", static_cast<int32>(OldState), static_cast<int32>(NewState));
}

void ASHDroneBase::LaunchDrone(const FVector& LaunchPosition, const FRotator& LaunchRotation)
{
	if (CurrentState != ESHDroneState::Inactive)
	{
		SH_WARNING(LogSH_Drone, "Attempted to launch drone that is not inactive (state: %d)",
			static_cast<int32>(CurrentState));
		return;
	}

	SetActorLocationAndRotation(LaunchPosition, LaunchRotation);
	LaunchOrigin = LaunchPosition;
	LaunchElapsedTime = 0.f;
	HomePosition = LaunchPosition;

	SetDroneState(ESHDroneState::Launching);

	// Activate motor audio
	if (MotorAudioComponent && MotorLoopSound)
	{
		MotorAudioComponent->Play();
	}

	// Activate LED
	if (LEDLight)
	{
		LEDLight->SetVisibility(true);
	}

	// Activate rotor wash VFX
	if (RotorWashVFX)
	{
		RotorWashVFX->Activate();
	}

	SH_LOG(LogSH_Drone, Log, "Drone launched from %s", *LaunchPosition.ToString());
}

void ASHDroneBase::ReturnToOperator()
{
	if (CurrentState == ESHDroneState::Destroyed || CurrentState == ESHDroneState::Inactive)
	{
		return;
	}

	SetFlightTarget(HomePosition);
	SetDroneState(ESHDroneState::Returning);
}

void ASHDroneBase::DestroyDrone(AActor* DamageInstigator)
{
	if (CurrentState == ESHDroneState::Destroyed)
	{
		return;
	}

	SetDroneState(ESHDroneState::Destroyed);
	OnDroneDestroyed.Broadcast(this);

	SpawnDestructionEffects();
	SpawnDebris();

	// Stop motor audio
	if (MotorAudioComponent)
	{
		MotorAudioComponent->Stop();
	}

	// Disable LED
	if (LEDLight)
	{
		LEDLight->SetVisibility(false);
	}

	// Deactivate VFX
	if (RotorWashVFX)
	{
		RotorWashVFX->Deactivate();
	}
	if (TrailVFX)
	{
		TrailVFX->Deactivate();
	}

	// Disable collision after a short delay so debris can scatter
	CollisionSphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	// Schedule actor destruction
	SetLifeSpan(5.f);

	SH_LOG(LogSH_Drone, Log, "Drone destroyed by %s", DamageInstigator ? *DamageInstigator->GetName() : TEXT("Unknown"));
}

// =======================================================================
//  Battery / fuel
// =======================================================================

float ASHDroneBase::GetBatteryFraction() const
{
	return (MaxBatterySeconds > 0.f) ? FMath::Clamp(CurrentBatterySeconds / MaxBatterySeconds, 0.f, 1.f) : 0.f;
}

float ASHDroneBase::GetRemainingLoiterTime() const
{
	const float DrainRate = LoiterDrainMultiplier;
	return (DrainRate > 0.f) ? CurrentBatterySeconds / DrainRate : 0.f;
}

bool ASHDroneBase::HasSufficientBattery() const
{
	return GetBatteryFraction() > LowBatteryThreshold;
}

// =======================================================================
//  Signal link
// =======================================================================

void ASHDroneBase::ApplyJammingInterference(float JammingPower, const FVector& JammerLocation)
{
	AccumulatedJammingPower = FMath::Max(AccumulatedJammingPower, JammingPower);
	LastJammerLocation = JammerLocation;

	SH_LOG(LogSH_Drone, Verbose, "Jamming applied — Power: %.2f", JammingPower);
}

void ASHDroneBase::ClearJammingInterference()
{
	AccumulatedJammingPower = 0.f;

	if (CurrentState == ESHDroneState::Jammed)
	{
		// Attempt to recover to flying state
		SetDroneState(ESHDroneState::Flying);
		bSignalLost = false;
		AutonomousFlightTimer = 0.f;
	}
}

// =======================================================================
//  Noise profile
// =======================================================================

float ASHDroneBase::GetAudibleRange() const
{
	float Multiplier = 1.f;

	switch (CurrentState)
	{
	case ESHDroneState::Flying:
	case ESHDroneState::Returning:
		Multiplier = FlyingNoiseMultiplier;
		break;
	case ESHDroneState::Attacking:
		Multiplier = AttackNoiseMultiplier;
		break;
	case ESHDroneState::Loitering:
		Multiplier = 1.f;
		break;
	case ESHDroneState::Launching:
		Multiplier = 1.2f;
		break;
	default:
		Multiplier = 0.f;
		break;
	}

	return BaseAudibleRange * Multiplier;
}

float ASHDroneBase::GetNoiseLevel() const
{
	return CurrentNoiseLevel;
}

// =======================================================================
//  Flight
// =======================================================================

void ASHDroneBase::SetFlightTarget(const FVector& TargetPosition)
{
	FlightTargetPosition = TargetPosition;
}

void ASHDroneBase::SetCruiseAltitude(float AltitudeCm)
{
	CurrentCruiseAltitude = FMath::Max(AltitudeCm, 100.f);
}

float ASHDroneBase::GetCurrentSpeed() const
{
	return GetVelocity().Size();
}

// =======================================================================
//  State callbacks (virtual)
// =======================================================================

void ASHDroneBase::OnStateEntered(ESHDroneState NewState)
{
	switch (NewState)
	{
	case ESHDroneState::Launching:
		RotorSpinSpeed = 0.5f;
		break;

	case ESHDroneState::Flying:
		RotorSpinSpeed = 1.f;
		if (TrailVFX)
		{
			TrailVFX->Activate();
		}
		break;

	case ESHDroneState::Loitering:
		RotorSpinSpeed = 0.7f;
		break;

	case ESHDroneState::Attacking:
		RotorSpinSpeed = 1.f;
		break;

	case ESHDroneState::Returning:
		RotorSpinSpeed = 1.f;
		SetFlightTarget(HomePosition);
		break;

	case ESHDroneState::Jammed:
		RotorSpinSpeed = 0.3f;
		if (LEDLight)
		{
			LEDLight->SetLightColor(FLinearColor::Red);
		}
		break;

	case ESHDroneState::Destroyed:
		RotorSpinSpeed = 0.f;
		break;

	default:
		break;
	}
}

void ASHDroneBase::OnStateExited(ESHDroneState OldState)
{
	switch (OldState)
	{
	case ESHDroneState::Flying:
		if (TrailVFX)
		{
			TrailVFX->Deactivate();
		}
		break;

	case ESHDroneState::Jammed:
		if (LEDLight)
		{
			LEDLight->SetLightColor(FLinearColor::Green);
		}
		AccumulatedJammingPower = 0.f;
		break;

	default:
		break;
	}
}

void ASHDroneBase::TickDroneBehavior(float DeltaSeconds)
{
	// Base implementation — override in subclasses for drone-type-specific behavior.
}

void ASHDroneBase::OnBatteryDepleted()
{
	OnBatteryDepleted_Broadcast:
	OnBatteryDepleted.Broadcast(this);

	// Force return if possible, otherwise crash
	if (CurrentState != ESHDroneState::Destroyed && CurrentState != ESHDroneState::Returning)
	{
		if (bReturnHomeOnSignalLoss)
		{
			ReturnToOperator();
		}
		else
		{
			DestroyDrone(nullptr);
		}
	}
}

void ASHDroneBase::OnSignalLinkQualityChanged(ESHSignalLinkQuality OldQuality, ESHSignalLinkQuality NewQuality)
{
	OnSignalLinkChanged.Broadcast(OldQuality, NewQuality);

	if (NewQuality == ESHSignalLinkQuality::Lost)
	{
		bSignalLost = true;
		AutonomousFlightTimer = 0.f;

		if (bReturnHomeOnSignalLoss)
		{
			ReturnToOperator();
		}
	}
	else if (OldQuality == ESHSignalLinkQuality::Lost && NewQuality != ESHSignalLinkQuality::Lost)
	{
		bSignalLost = false;
		AutonomousFlightTimer = 0.f;
	}
}

// =======================================================================
//  Tick helpers
// =======================================================================

void ASHDroneBase::TickBattery(float DeltaSeconds)
{
	float DrainMultiplier = 1.f;

	switch (CurrentState)
	{
	case ESHDroneState::Loitering:
		DrainMultiplier = LoiterDrainMultiplier;
		break;
	case ESHDroneState::Flying:
	case ESHDroneState::Returning:
		DrainMultiplier = FlyingDrainMultiplier;
		break;
	case ESHDroneState::Attacking:
		DrainMultiplier = AttackDrainMultiplier;
		break;
	case ESHDroneState::Jammed:
		DrainMultiplier = LoiterDrainMultiplier; // Hovering while jammed
		break;
	default:
		DrainMultiplier = 0.f;
		break;
	}

	CurrentBatterySeconds -= DeltaSeconds * DrainMultiplier;

	if (CurrentBatterySeconds <= 0.f)
	{
		CurrentBatterySeconds = 0.f;
		OnBatteryDepleted();
	}
	else if (!HasSufficientBattery() && CurrentState != ESHDroneState::Returning)
	{
		SH_LOG(LogSH_Drone, Log, "Low battery — triggering RTH. Battery: %.1f%%", GetBatteryFraction() * 100.f);
		ReturnToOperator();
	}
}

void ASHDroneBase::TickSignalLink(float DeltaSeconds)
{
	// Base signal strength from distance to operator
	float DistanceToOperator = MaxOperationalRange;
	if (OperatorActor.IsValid())
	{
		DistanceToOperator = FVector::Dist(GetActorLocation(), OperatorActor->GetActorLocation());
	}

	float BaseSignal = FMath::Clamp(1.f - (DistanceToOperator * SignalDecayPerCm), 0.f, 1.f);

	// Apply jamming interference
	float JammingReduction = AccumulatedJammingPower;
	float EffectiveSignal = FMath::Clamp(BaseSignal - JammingReduction, 0.f, 1.f);

	// Decay jamming power over time if not being actively jammed
	AccumulatedJammingPower = FMath::Max(AccumulatedJammingPower - DeltaSeconds * 0.1f, 0.f);

	// Evaluate signal quality
	const ESHSignalLinkQuality OldQuality = SignalLinkQuality;
	SignalStrength = EffectiveSignal;
	SignalLinkQuality = ComputeSignalQuality(EffectiveSignal);

	if (OldQuality != SignalLinkQuality)
	{
		OnSignalLinkQualityChanged(OldQuality, SignalLinkQuality);
	}

	// If signal completely lost, count autonomous flight time
	if (bSignalLost)
	{
		AutonomousFlightTimer += DeltaSeconds;
		if (AutonomousFlightTimer >= AutonomousFlightDuration)
		{
			// Drone crashes after autonomous flight runs out
			if (CurrentState != ESHDroneState::Jammed && CurrentState != ESHDroneState::Destroyed)
			{
				SetDroneState(ESHDroneState::Jammed);
			}
		}
	}

	// Strong jamming forces the Jammed state
	if (AccumulatedJammingPower > 0.8f && CurrentState != ESHDroneState::Jammed && CurrentState != ESHDroneState::Destroyed)
	{
		SetDroneState(ESHDroneState::Jammed);
	}
}

void ASHDroneBase::TickFlight(float DeltaSeconds)
{
	if (FlightTargetPosition.IsNearlyZero())
	{
		return;
	}

	const FVector CurrentLocation = GetActorLocation();
	FVector TargetWithAltitude = FlightTargetPosition;

	// Maintain cruise altitude unless attacking
	if (CurrentState != ESHDroneState::Attacking)
	{
		TargetWithAltitude.Z = FMath::Max(TargetWithAltitude.Z, CurrentCruiseAltitude);
	}

	const FVector Direction = (TargetWithAltitude - CurrentLocation).GetSafeNormal();
	const float DistanceToTarget = FVector::Dist(CurrentLocation, TargetWithAltitude);

	// Determine target speed based on state and proximity
	float TargetSpeed = CruiseSpeed;
	if (CurrentState == ESHDroneState::Attacking)
	{
		TargetSpeed = MaxFlightSpeed;
	}
	else if (CurrentState == ESHDroneState::Returning && DistanceToTarget < 500.f)
	{
		TargetSpeed = CruiseSpeed * 0.3f; // Slow down for landing
	}

	// Apply movement input through the floating pawn movement component
	if (DistanceToTarget > 50.f)
	{
		AddMovementInput(Direction, TargetSpeed / MaxFlightSpeed);
	}

	// Rotate toward target
	const FRotator CurrentRotation = GetActorRotation();
	const FRotator TargetRotation = Direction.Rotation();
	const FRotator NewRotation = FMath::RInterpTo(CurrentRotation, TargetRotation, DeltaSeconds, TurnRate / 45.f);
	SetActorRotation(NewRotation);

	// Check if returning drone has reached home
	if (CurrentState == ESHDroneState::Returning && DistanceToTarget < 200.f)
	{
		SH_LOG(LogSH_Drone, Log, "Drone returned to home position.");
		SetDroneState(ESHDroneState::Inactive);

		if (MotorAudioComponent)
		{
			MotorAudioComponent->Stop();
		}
	}
}

void ASHDroneBase::TickNoise(float DeltaSeconds)
{
	float TargetNoise = 0.f;

	switch (CurrentState)
	{
	case ESHDroneState::Launching:
		TargetNoise = 0.4f;
		break;
	case ESHDroneState::Flying:
	case ESHDroneState::Returning:
	{
		float SpeedFraction = GetCurrentSpeed() / FMath::Max(MaxFlightSpeed, 1.f);
		TargetNoise = FMath::Lerp(0.3f, 0.8f, SpeedFraction);
		break;
	}
	case ESHDroneState::Loitering:
		TargetNoise = 0.3f;
		break;
	case ESHDroneState::Attacking:
		TargetNoise = 1.0f;
		break;
	case ESHDroneState::Jammed:
		TargetNoise = 0.5f; // Erratic thrust while jammed
		break;
	default:
		TargetNoise = 0.f;
		break;
	}

	// Smooth noise level transitions
	CurrentNoiseLevel = FMath::FInterpTo(CurrentNoiseLevel, TargetNoise, DeltaSeconds, 3.f);
}

void ASHDroneBase::TickLaunchSequence(float DeltaSeconds)
{
	LaunchElapsedTime += DeltaSeconds;

	// Ascend vertically from launch origin
	const float TargetZ = LaunchOrigin.Z + LaunchTargetHeight;
	const FVector CurrentLocation = GetActorLocation();

	if (CurrentLocation.Z < TargetZ)
	{
		const FVector AscentDirection = FVector::UpVector;
		AddMovementInput(AscentDirection, LaunchAscentSpeed / MaxFlightSpeed);
	}
	else
	{
		// Launch complete — transition to flying
		SetDroneState(ESHDroneState::Flying);
		SH_LOG(LogSH_Drone, Log, "Launch sequence complete — transitioning to Flying.");
	}
}

void ASHDroneBase::TickJammedBehavior(float DeltaSeconds)
{
	// Drone drifts erratically when jammed
	const float DriftAmplitude = 100.f;
	const float DriftFrequency = 2.f;
	const float Time = GetWorld()->GetTimeSeconds();

	FVector DriftOffset;
	DriftOffset.X = FMath::Sin(Time * DriftFrequency) * DriftAmplitude * DeltaSeconds;
	DriftOffset.Y = FMath::Cos(Time * DriftFrequency * 1.3f) * DriftAmplitude * DeltaSeconds;
	DriftOffset.Z = -50.f * DeltaSeconds; // Slowly descend

	AddMovementInput(DriftOffset.GetSafeNormal(), 0.3f);

	// Check if we've descended to ground level — crash
	FHitResult GroundHit;
	const FVector Start = GetActorLocation();
	const FVector End = Start - FVector(0.f, 0.f, 100.f);

	if (GetWorld()->LineTraceSingleByChannel(GroundHit, Start, End, ECC_WorldStatic))
	{
		if (GroundHit.Distance < 50.f)
		{
			DestroyDrone(nullptr);
		}
	}
}

ESHSignalLinkQuality ASHDroneBase::ComputeSignalQuality(float Strength) const
{
	if (Strength > 0.8f) return ESHSignalLinkQuality::Excellent;
	if (Strength > 0.6f) return ESHSignalLinkQuality::Good;
	if (Strength > 0.3f) return ESHSignalLinkQuality::Degraded;
	if (Strength > 0.05f) return ESHSignalLinkQuality::Critical;
	return ESHSignalLinkQuality::Lost;
}

void ASHDroneBase::UpdateMotorAudio()
{
	if (!MotorAudioComponent || !MotorLoopSound)
	{
		return;
	}

	const bool bShouldPlay = (CurrentState != ESHDroneState::Inactive && CurrentState != ESHDroneState::Destroyed);

	if (bShouldPlay && !MotorAudioComponent->IsPlaying())
	{
		MotorAudioComponent->Play();
	}
	else if (!bShouldPlay && MotorAudioComponent->IsPlaying())
	{
		MotorAudioComponent->Stop();
	}

	// Modulate pitch and volume based on state and speed
	if (bShouldPlay)
	{
		float PitchMultiplier = 1.f;
		float VolumeMultiplier = 1.f;

		const float SpeedFraction = GetCurrentSpeed() / FMath::Max(MaxFlightSpeed, 1.f);

		switch (CurrentState)
		{
		case ESHDroneState::Launching:
			PitchMultiplier = FMath::Lerp(0.8f, 1.2f, FMath::Min(LaunchElapsedTime / 2.f, 1.f));
			VolumeMultiplier = FMath::Lerp(0.5f, 1.f, FMath::Min(LaunchElapsedTime / 1.f, 1.f));
			break;

		case ESHDroneState::Flying:
		case ESHDroneState::Returning:
			PitchMultiplier = FMath::Lerp(0.9f, 1.3f, SpeedFraction);
			VolumeMultiplier = FMath::Lerp(0.7f, 1.f, SpeedFraction);
			break;

		case ESHDroneState::Loitering:
			PitchMultiplier = 0.9f;
			VolumeMultiplier = 0.6f;
			break;

		case ESHDroneState::Attacking:
			PitchMultiplier = 1.4f;
			VolumeMultiplier = 1.f;
			break;

		case ESHDroneState::Jammed:
			// Erratic pitch fluctuations when jammed
			PitchMultiplier = 0.7f + FMath::FRand() * 0.6f;
			VolumeMultiplier = 0.8f;
			break;

		default:
			break;
		}

		MotorAudioComponent->SetPitchMultiplier(PitchMultiplier);
		MotorAudioComponent->SetVolumeMultiplier(VolumeMultiplier);
	}
}

void ASHDroneBase::SpawnDestructionEffects()
{
	const FVector Location = GetActorLocation();
	const FRotator Rotation = GetActorRotation();

	if (DestructionExplosionVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), DestructionExplosionVFX, Location, Rotation);
	}

	if (DebrisFallingVFX)
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			GetWorld(), DebrisFallingVFX, Location, Rotation);
	}

	if (DestructionSound)
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), DestructionSound, Location);
	}
}

void ASHDroneBase::SpawnDebris()
{
	if (!DebrisActorClass)
	{
		return;
	}

	const FVector Location = GetActorLocation();

	for (int32 i = 0; i < DebrisCount; ++i)
	{
		// Scatter debris in random directions
		FVector Offset;
		Offset.X = FMath::FRandRange(-200.f, 200.f);
		Offset.Y = FMath::FRandRange(-200.f, 200.f);
		Offset.Z = FMath::FRandRange(50.f, 300.f);

		FRotator RandomRotation = FRotator(
			FMath::FRandRange(0.f, 360.f),
			FMath::FRandRange(0.f, 360.f),
			FMath::FRandRange(0.f, 360.f));

		FActorSpawnParameters SpawnParams;
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

		AActor* Debris = GetWorld()->SpawnActor<AActor>(DebrisActorClass, Location + Offset, RandomRotation, SpawnParams);
		if (Debris)
		{
			Debris->SetLifeSpan(15.f);

			// Apply impulse to scatter debris if it has a physics-enabled root
			if (UPrimitiveComponent* PrimComp = Cast<UPrimitiveComponent>(Debris->GetRootComponent()))
			{
				if (PrimComp->IsSimulatingPhysics())
				{
					const FVector Impulse = Offset.GetSafeNormal() * FMath::FRandRange(500.f, 1500.f);
					PrimComp->AddImpulse(Impulse, NAME_None, true);
				}
			}
		}
	}
}
