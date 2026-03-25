// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHSpectatorController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/SpectatorPawn.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogSH_Spectator, Log, All);

// -----------------------------------------------------------------------
//  Constructor
// -----------------------------------------------------------------------

ASHSpectatorController::ASHSpectatorController()
{
	PrimaryActorTick.bCanEverTick = true;
	bShowMouseCursor = false;
	OverheadCurrentHeight = OverheadDefaultHeight;
}

// -----------------------------------------------------------------------
//  APlayerController overrides
// -----------------------------------------------------------------------

void ASHSpectatorController::BeginPlay()
{
	Super::BeginPlay();

	// Add spectator input mapping context
	if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (SpectatorMappingContext)
		{
			InputSubsystem->AddMappingContext(SpectatorMappingContext, 0);
		}
	}

	RefreshTargetList();
	SwitchMode(ESHSpectatorMode::FreeCamera);

	UE_LOG(LogSH_Spectator, Log, TEXT("Spectator controller initialized."));
}

void ASHSpectatorController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EIC)
	{
		return;
	}

	if (IA_SpectatorMove)
	{
		EIC->BindAction(IA_SpectatorMove, ETriggerEvent::Triggered, this, &ASHSpectatorController::Input_Move);
		EIC->BindAction(IA_SpectatorMove, ETriggerEvent::Completed, this, &ASHSpectatorController::Input_Move);
	}

	if (IA_SpectatorLook)
	{
		EIC->BindAction(IA_SpectatorLook, ETriggerEvent::Triggered, this, &ASHSpectatorController::Input_Look);
	}

	if (IA_SpectatorSpeedBoost)
	{
		EIC->BindAction(IA_SpectatorSpeedBoost, ETriggerEvent::Started, this, &ASHSpectatorController::Input_SpeedBoost_Started);
		EIC->BindAction(IA_SpectatorSpeedBoost, ETriggerEvent::Completed, this, &ASHSpectatorController::Input_SpeedBoost_Completed);
	}

	if (IA_SpectatorSwitchMode)
	{
		EIC->BindAction(IA_SpectatorSwitchMode, ETriggerEvent::Started, this, &ASHSpectatorController::Input_SwitchMode);
	}

	if (IA_SpectatorCycleForward)
	{
		EIC->BindAction(IA_SpectatorCycleForward, ETriggerEvent::Started, this, &ASHSpectatorController::Input_CycleTargetForward);
	}

	if (IA_SpectatorCycleBackward)
	{
		EIC->BindAction(IA_SpectatorCycleBackward, ETriggerEvent::Started, this, &ASHSpectatorController::Input_CycleTargetBackward);
	}

	if (IA_SpectatorZoom)
	{
		EIC->BindAction(IA_SpectatorZoom, ETriggerEvent::Triggered, this, &ASHSpectatorController::Input_Zoom);
	}

	if (IA_SpectatorToggleOverlay)
	{
		EIC->BindAction(IA_SpectatorToggleOverlay, ETriggerEvent::Started, this, &ASHSpectatorController::Input_ToggleOverlay);
	}
}

void ASHSpectatorController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Periodically refresh the target list
	TargetRefreshAccumulator += DeltaSeconds;
	if (TargetRefreshAccumulator >= TargetRefreshInterval)
	{
		TargetRefreshAccumulator = 0.0f;
		RefreshTargetList();
	}

	switch (CurrentMode)
	{
	case ESHSpectatorMode::FreeCamera:
		TickFreeCamera(DeltaSeconds);
		break;
	case ESHSpectatorMode::FirstPerson:
		TickFirstPerson(DeltaSeconds);
		break;
	case ESHSpectatorMode::TacticalOverhead:
		TickTacticalOverhead(DeltaSeconds);
		break;
	}
}

// -----------------------------------------------------------------------
//  Mode switching
// -----------------------------------------------------------------------

void ASHSpectatorController::SwitchMode(ESHSpectatorMode NewMode)
{
	if (CurrentMode == NewMode)
	{
		return;
	}

	const ESHSpectatorMode OldMode = CurrentMode;

	// Cleanup previous mode
	if (OldMode == ESHSpectatorMode::FirstPerson)
	{
		DetachFromTarget();
	}

	CurrentMode = NewMode;

	// Setup new mode
	switch (NewMode)
	{
	case ESHSpectatorMode::FreeCamera:
		{
			bShowMouseCursor = false;
		}
		break;

	case ESHSpectatorMode::FirstPerson:
		{
			bShowMouseCursor = false;
			if (SpectateTargets.Num() > 0)
			{
				CurrentTargetIndex = FMath::Clamp(CurrentTargetIndex, 0, SpectateTargets.Num() - 1);
				CurrentTarget = SpectateTargets[CurrentTargetIndex];
				AttachToTarget();
			}
		}
		break;

	case ESHSpectatorMode::TacticalOverhead:
		{
			bShowMouseCursor = false;
			OverheadCurrentHeight = OverheadDefaultHeight;

			// Position camera above the current location looking down
			if (APawn* SpectatorPawn = GetPawn())
			{
				FVector CurrentLoc = SpectatorPawn->GetActorLocation();
				CurrentLoc.Z = OverheadCurrentHeight;
				SpectatorPawn->SetActorLocation(CurrentLoc);
				SetControlRotation(FRotator(-90.0f, 0.0f, 0.0f));
			}
		}
		break;
	}

	OnSpectatorModeChanged.Broadcast(OldMode, NewMode);
	UE_LOG(LogSH_Spectator, Log, TEXT("Spectator mode switched from %d to %d."),
		static_cast<int32>(OldMode), static_cast<int32>(NewMode));
}

void ASHSpectatorController::CycleTarget(bool bForward)
{
	if (SpectateTargets.Num() == 0)
	{
		return;
	}

	if (bForward)
	{
		CurrentTargetIndex = (CurrentTargetIndex + 1) % SpectateTargets.Num();
	}
	else
	{
		CurrentTargetIndex = (CurrentTargetIndex - 1 + SpectateTargets.Num()) % SpectateTargets.Num();
	}

	AActor* OldTarget = CurrentTarget;
	CurrentTarget = SpectateTargets[CurrentTargetIndex];

	if (CurrentMode == ESHSpectatorMode::FirstPerson && CurrentTarget != OldTarget)
	{
		DetachFromTarget();
		AttachToTarget();
	}

	OnSpectatorTargetChanged.Broadcast(CurrentTarget);
}

// -----------------------------------------------------------------------
//  AI debug overlay
// -----------------------------------------------------------------------

void ASHSpectatorController::ToggleAIDecisionOverlay()
{
	bAIOverlayVisible = !bAIOverlayVisible;
	UE_LOG(LogSH_Spectator, Log, TEXT("AI decision overlay %s."),
		bAIOverlayVisible ? TEXT("shown") : TEXT("hidden"));
}

// -----------------------------------------------------------------------
//  Input handlers
// -----------------------------------------------------------------------

void ASHSpectatorController::Input_Move(const FInputActionValue& Value)
{
	PendingMoveInput = Value.Get<FVector>();
}

void ASHSpectatorController::Input_Look(const FInputActionValue& Value)
{
	PendingLookInput = Value.Get<FVector2D>();
}

void ASHSpectatorController::Input_SpeedBoost_Started(const FInputActionValue& Value)
{
	bSpeedBoosting = true;
}

void ASHSpectatorController::Input_SpeedBoost_Completed(const FInputActionValue& Value)
{
	bSpeedBoosting = false;
}

void ASHSpectatorController::Input_SwitchMode(const FInputActionValue& Value)
{
	// Cycle through modes: Free -> FirstPerson -> Overhead -> Free
	const int32 NextMode = (static_cast<int32>(CurrentMode) + 1) % 3;
	SwitchMode(static_cast<ESHSpectatorMode>(NextMode));
}

void ASHSpectatorController::Input_CycleTargetForward(const FInputActionValue& Value)
{
	CycleTarget(true);
}

void ASHSpectatorController::Input_CycleTargetBackward(const FInputActionValue& Value)
{
	CycleTarget(false);
}

void ASHSpectatorController::Input_Zoom(const FInputActionValue& Value)
{
	if (CurrentMode == ESHSpectatorMode::TacticalOverhead)
	{
		const float ZoomDelta = Value.Get<float>();
		OverheadCurrentHeight = FMath::Clamp(
			OverheadCurrentHeight - ZoomDelta * OverheadZoomStep,
			OverheadMinHeight,
			OverheadMaxHeight);
	}
}

void ASHSpectatorController::Input_ToggleOverlay(const FInputActionValue& Value)
{
	ToggleAIDecisionOverlay();
}

// -----------------------------------------------------------------------
//  Mode-specific tick
// -----------------------------------------------------------------------

void ASHSpectatorController::TickFreeCamera(float DeltaSeconds)
{
	APawn* SpectatorPawn = GetPawn();
	if (!SpectatorPawn)
	{
		return;
	}

	// Apply look input
	if (!PendingLookInput.IsNearlyZero())
	{
		AddYawInput(PendingLookInput.X * FreeCameraLookSensitivity);
		AddPitchInput(-PendingLookInput.Y * FreeCameraLookSensitivity);
		PendingLookInput = FVector2D::ZeroVector;
	}

	// Apply movement
	if (!PendingMoveInput.IsNearlyZero())
	{
		const float SpeedMul = bSpeedBoosting ? SpeedBoostMultiplier : 1.0f;
		const float Speed = FreeCameraSpeed * SpeedMul * DeltaSeconds;

		const FRotator ControlRot = GetControlRotation();
		const FVector Forward = FRotationMatrix(ControlRot).GetUnitAxis(EAxis::X);
		const FVector Right = FRotationMatrix(ControlRot).GetUnitAxis(EAxis::Y);
		const FVector Up = FVector::UpVector;

		const FVector Movement =
			Forward * PendingMoveInput.X * Speed +
			Right * PendingMoveInput.Y * Speed +
			Up * PendingMoveInput.Z * Speed;

		SpectatorPawn->AddActorWorldOffset(Movement, true);
	}
}

void ASHSpectatorController::TickFirstPerson(float DeltaSeconds)
{
	// Validate target still exists
	if (!CurrentTarget || CurrentTarget->IsPendingKillPending())
	{
		// Try next target
		if (SpectateTargets.Num() > 0)
		{
			CycleTarget(true);
		}
		return;
	}

	// Follow the target's view
	APawn* TargetPawn = Cast<APawn>(CurrentTarget);
	if (TargetPawn)
	{
		if (APawn* SpectatorPawn = GetPawn())
		{
			// Position at the target's eye location
			FVector ViewLocation;
			FRotator ViewRotation;
			TargetPawn->GetActorEyesViewPoint(ViewLocation, ViewRotation);

			SpectatorPawn->SetActorLocation(ViewLocation);
			SetControlRotation(ViewRotation);
		}
	}
}

void ASHSpectatorController::TickTacticalOverhead(float DeltaSeconds)
{
	APawn* SpectatorPawn = GetPawn();
	if (!SpectatorPawn)
	{
		return;
	}

	// Apply lateral movement (WASD moves the overhead camera)
	if (!PendingMoveInput.IsNearlyZero())
	{
		const float SpeedMul = bSpeedBoosting ? SpeedBoostMultiplier : 1.0f;
		const float Speed = FreeCameraSpeed * SpeedMul * DeltaSeconds;

		const FVector Movement(
			PendingMoveInput.X * Speed,
			PendingMoveInput.Y * Speed,
			0.0f);

		SpectatorPawn->AddActorWorldOffset(Movement, true);
	}

	// Maintain overhead height
	FVector Loc = SpectatorPawn->GetActorLocation();
	Loc.Z = OverheadCurrentHeight;
	SpectatorPawn->SetActorLocation(Loc);

	// Always look straight down
	SetControlRotation(FRotator(-90.0f, 0.0f, 0.0f));
}

// -----------------------------------------------------------------------
//  Target management
// -----------------------------------------------------------------------

void ASHSpectatorController::RefreshTargetList()
{
	SpectateTargets.Empty();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Gather all pawns that are valid spectate targets
	for (TActorIterator<APawn> It(World); It; ++It)
	{
		APawn* CandidatePawn = *It;
		if (!CandidatePawn || CandidatePawn == GetPawn())
		{
			continue;
		}

		// Skip spectator pawns
		if (CandidatePawn->IsA<ASpectatorPawn>())
		{
			continue;
		}

		// Only spectate alive pawns
		if (CandidatePawn->IsPendingKillPending())
		{
			continue;
		}

		SpectateTargets.Add(CandidatePawn);
	}

	// Validate current target index
	if (SpectateTargets.Num() > 0)
	{
		if (CurrentTargetIndex >= SpectateTargets.Num() || CurrentTargetIndex < 0)
		{
			CurrentTargetIndex = 0;
		}
		// Ensure current target is still valid
		if (CurrentTarget && !SpectateTargets.Contains(CurrentTarget))
		{
			CurrentTargetIndex = 0;
			CurrentTarget = SpectateTargets[0];
		}
	}
	else
	{
		CurrentTargetIndex = INDEX_NONE;
		CurrentTarget = nullptr;
	}
}

void ASHSpectatorController::AttachToTarget()
{
	if (!CurrentTarget)
	{
		return;
	}

	// Set view target with blend for smooth transition
	SetViewTargetWithBlend(CurrentTarget, 0.3f);
}

void ASHSpectatorController::DetachFromTarget()
{
	// Restore view to own pawn
	if (APawn* SpectatorPawn = GetPawn())
	{
		SetViewTarget(SpectatorPawn);
	}
}
