// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHBreachableEntry.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"

ASHBreachableEntry::ASHBreachableEntry()
{
	PrimaryActorTick.bCanEverTick = true;

	DoorFrame = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorFrame"));
	RootComponent = DoorFrame;

	DoorPanel = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DoorPanel"));
	DoorPanel->SetupAttachment(DoorFrame);
}

void ASHBreachableEntry::BeginPlay()
{
	Super::BeginPlay();

	if (bStartsBarricaded)
	{
		DoorState = ESHDoorState::Barricaded;
	}
	else if (bStartsLocked)
	{
		DoorState = ESHDoorState::Locked;
	}
}

void ASHBreachableEntry::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Breach timer.
	if (bBreachInProgress)
	{
		BreachTimer += DeltaSeconds;
		if (BreachTimer >= BreachDuration)
		{
			CompleteBreach();
		}
	}

	// Animate door swing.
	if (!FMath::IsNearlyEqual(CurrentSwingAngle, TargetSwingAngle, 0.5f))
	{
		CurrentSwingAngle = FMath::FInterpTo(CurrentSwingAngle, TargetSwingAngle, DeltaSeconds, 5.0f);
		if (DoorPanel)
		{
			DoorPanel->SetRelativeRotation(FRotator(0.0f, CurrentSwingAngle, 0.0f));
		}
	}
}

// =====================================================================
//  ISHInteractable
// =====================================================================

bool ASHBreachableEntry::Interact_Implementation(APlayerController* Interactor)
{
	if (!Interactor) return false;

	// Default interaction: try quiet open, or kick if locked.
	if (DoorState == ESHDoorState::Closed)
	{
		return AttemptBreach(ESHBreachMethod::Open, Interactor);
	}
	else if (DoorState == ESHDoorState::Locked)
	{
		return AttemptBreach(ESHBreachMethod::Kick, Interactor);
	}
	else if (DoorState == ESHDoorState::Open)
	{
		// Close the door.
		TargetSwingAngle = 0.0f;
		DoorState = ESHDoorState::Closed;
		OnDoorStateChanged.Broadcast(DoorState, ESHBreachMethod::Open);
		return true;
	}

	return false;
}

FText ASHBreachableEntry::GetInteractionPrompt_Implementation() const
{
	switch (DoorState)
	{
	case ESHDoorState::Closed:     return FText::FromString(TEXT("Open Door"));
	case ESHDoorState::Locked:     return FText::FromString(TEXT("Kick Door"));
	case ESHDoorState::Open:       return FText::FromString(TEXT("Close Door"));
	case ESHDoorState::Barricaded: return FText::FromString(TEXT("Breach Required"));
	case ESHDoorState::Breached:   return FText::FromString(TEXT(""));
	default:                       return FText::FromString(TEXT(""));
	}
}

bool ASHBreachableEntry::CanInteract_Implementation(APlayerController* Interactor) const
{
	return DoorState != ESHDoorState::Breached && DoorState != ESHDoorState::Opening && !bBreachInProgress;
}

// =====================================================================
//  Breach interface
// =====================================================================

bool ASHBreachableEntry::AttemptBreach(ESHBreachMethod Method, AController* Instigator)
{
	if (bBreachInProgress || DoorState == ESHDoorState::Breached || DoorState == ESHDoorState::Open)
	{
		return false;
	}

	if (!CanBreach(Method))
	{
		// Play locked/blocked sound.
		if (LockedSound)
		{
			UGameplayStatics::PlaySoundAtLocation(GetWorld(), LockedSound, GetActorLocation());
		}
		return false;
	}

	ActiveBreachMethod = Method;
	BreachInstigator = Instigator;
	bBreachInProgress = true;
	BreachTimer = 0.0f;

	// Set breach duration based on method.
	switch (Method)
	{
	case ESHBreachMethod::Open:      BreachDuration = QuietOpenTime; break;
	case ESHBreachMethod::Kick:      BreachDuration = KickBreachTime; break;
	case ESHBreachMethod::Shotgun:   BreachDuration = 0.3f; break; // Nearly instant
	case ESHBreachMethod::Explosive: BreachDuration = ExplosiveBreachTime; break;
	case ESHBreachMethod::PickLock:  BreachDuration = 8.0f; break; // Slow but silent
	}

	DoorState = ESHDoorState::Opening;
	OnBreachStarted.Broadcast(Method);

	UE_LOG(LogTemp, Log, TEXT("[Breach] %s — method: %d, duration: %.1fs"),
		*GetName(), static_cast<int32>(Method), BreachDuration);

	return true;
}

bool ASHBreachableEntry::CanBreach(ESHBreachMethod Method) const
{
	switch (Method)
	{
	case ESHBreachMethod::Open:
		return DoorState == ESHDoorState::Closed; // Can't open a locked door quietly.

	case ESHBreachMethod::Kick:
		return DoorState == ESHDoorState::Closed || DoorState == ESHDoorState::Locked;

	case ESHBreachMethod::Shotgun:
		return DoorState == ESHDoorState::Locked || DoorState == ESHDoorState::Closed;

	case ESHBreachMethod::Explosive:
		return true; // Explosives can breach anything.

	case ESHBreachMethod::PickLock:
		return DoorState == ESHDoorState::Locked;

	default:
		return false;
	}
}

void ASHBreachableEntry::GetStackPositions(FVector& OutLeft, FVector& OutRight) const
{
	const FVector DoorPos = GetActorLocation();
	const FVector Right = GetActorRightVector();
	const FVector Forward = GetActorForwardVector();

	// Stack positions: 80cm to either side, 60cm back from the door.
	OutLeft = DoorPos - Right * 80.0f - Forward * 60.0f;
	OutRight = DoorPos + Right * 80.0f - Forward * 60.0f;
}

// =====================================================================
//  Internal
// =====================================================================

void ASHBreachableEntry::CompleteBreach()
{
	bBreachInProgress = false;

	// Play method-specific sound.
	USoundBase* SoundToPlay = nullptr;
	float NoiseRadius = 0.0f;

	switch (ActiveBreachMethod)
	{
	case ESHBreachMethod::Open:
		SoundToPlay = OpenSound;
		NoiseRadius = OpenNoiseRadius;
		DoorState = ESHDoorState::Open;
		TargetSwingAngle = 90.0f; // Swing open.
		break;

	case ESHBreachMethod::Kick:
		SoundToPlay = KickSound;
		NoiseRadius = KickNoiseRadius;
		DoorState = ESHDoorState::Open;
		TargetSwingAngle = 110.0f; // Kicked wide.
		DoorHealth -= 40.0f;
		break;

	case ESHBreachMethod::Shotgun:
		SoundToPlay = ShotgunBreachSound;
		NoiseRadius = ShotgunNoiseRadius;
		DoorState = ESHDoorState::Open;
		TargetSwingAngle = 100.0f;
		DoorHealth -= 80.0f;
		break;

	case ESHBreachMethod::Explosive:
	{
		SoundToPlay = ExplosiveBreachSound;
		NoiseRadius = ExplosiveNoiseRadius;
		DoorState = ESHDoorState::Breached;
		DoorHealth = 0.0f;

		// Destroy the door panel.
		if (DoorPanel)
		{
			DoorPanel->SetVisibility(false, true);
			DoorPanel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		}

		// Apply explosive damage to entities near the door (behind it).
		if (ExplosiveBreachDamage > 0.0f)
		{
			TArray<AActor*> IgnoreActors;
			IgnoreActors.Add(this);
			UGameplayStatics::ApplyRadialDamage(
				GetWorld(),
				ExplosiveBreachDamage,
				GetActorLocation() + GetActorForwardVector() * 100.0f, // Center behind door
				ExplosiveStunRadius,
				UDamageType::StaticClass(),
				IgnoreActors,
				this,
				BreachInstigator.Get());
		}
		break;
	}

	case ESHBreachMethod::PickLock:
		SoundToPlay = OpenSound; // Silent open after lock picked.
		NoiseRadius = 100.0f; // Near-silent.
		DoorState = ESHDoorState::Open;
		TargetSwingAngle = 90.0f;
		break;
	}

	// Play sound.
	if (SoundToPlay)
	{
		UGameplayStatics::PlaySoundAtLocation(GetWorld(), SoundToPlay, GetActorLocation());
	}

	// Generate noise for AI.
	MakeBreachNoise(NoiseRadius);

	OnDoorStateChanged.Broadcast(DoorState, ActiveBreachMethod);
	OnBreachComplete.Broadcast();

	UE_LOG(LogTemp, Log, TEXT("[Breach] %s — breach complete, state: %d, door health: %.0f"),
		*GetName(), static_cast<int32>(DoorState), DoorHealth);
}

void ASHBreachableEntry::MakeBreachNoise(float Radius)
{
	if (Radius <= 0.0f) return;

	// MakeNoise with volume proportional to radius.
	const float NoiseVolume = FMath::Clamp(Radius / 5000.0f, 0.1f, 1.0f);

	AActor* NoiseInstigator = nullptr;
	if (BreachInstigator.IsValid())
	{
		if (APawn* Pawn = BreachInstigator->GetPawn())
		{
			NoiseInstigator = Pawn;
		}
	}

	if (NoiseInstigator)
	{
		NoiseInstigator->MakeNoise(NoiseVolume, Cast<APawn>(NoiseInstigator), GetActorLocation());
	}
	else
	{
		MakeNoise(NoiseVolume, nullptr, GetActorLocation());
	}
}
