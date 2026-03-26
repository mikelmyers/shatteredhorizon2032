// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHPlayerController.h"
#include "SHPlayerCharacter.h"
#include "SHInteractable.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/World.h"
#include "DrawDebugHelpers.h"

ASHPlayerController::ASHPlayerController()
{
	PrimaryActorTick.bCanEverTick = true;
}

// =======================================================================
//  APlayerController overrides
// =======================================================================

void ASHPlayerController::BeginPlay()
{
	Super::BeginPlay();

	// Register the default mapping context with the Enhanced Input subsystem.
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (DefaultMappingContext)
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void ASHPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();

	UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EIC)
	{
		return;
	}

	// Movement / look
	if (IA_Move)    { EIC->BindAction(IA_Move, ETriggerEvent::Triggered, this, &ASHPlayerController::Input_Move); }
	if (IA_Look)    { EIC->BindAction(IA_Look, ETriggerEvent::Triggered, this, &ASHPlayerController::Input_Look); }

	// Fire
	if (IA_Fire)
	{
		EIC->BindAction(IA_Fire, ETriggerEvent::Started,   this, &ASHPlayerController::Input_Fire_Started);
		EIC->BindAction(IA_Fire, ETriggerEvent::Completed, this, &ASHPlayerController::Input_Fire_Completed);
	}

	// ADS
	if (IA_ADS)
	{
		EIC->BindAction(IA_ADS, ETriggerEvent::Started,   this, &ASHPlayerController::Input_ADS_Started);
		EIC->BindAction(IA_ADS, ETriggerEvent::Completed, this, &ASHPlayerController::Input_ADS_Completed);
	}

	// Reload
	if (IA_Reload) { EIC->BindAction(IA_Reload, ETriggerEvent::Started, this, &ASHPlayerController::Input_Reload); }

	// Sprint
	if (IA_Sprint)
	{
		EIC->BindAction(IA_Sprint, ETriggerEvent::Started,   this, &ASHPlayerController::Input_Sprint_Started);
		EIC->BindAction(IA_Sprint, ETriggerEvent::Completed, this, &ASHPlayerController::Input_Sprint_Completed);
	}

	// Stance
	if (IA_Crouch) { EIC->BindAction(IA_Crouch, ETriggerEvent::Started, this, &ASHPlayerController::Input_Crouch); }
	if (IA_Prone)  { EIC->BindAction(IA_Prone,  ETriggerEvent::Started, this, &ASHPlayerController::Input_Prone); }
	if (IA_Jump)   { EIC->BindAction(IA_Jump,   ETriggerEvent::Started, this, &ASHPlayerController::Input_Jump); }

	// Lean
	if (IA_LeanLeft)
	{
		EIC->BindAction(IA_LeanLeft, ETriggerEvent::Started,   this, &ASHPlayerController::Input_LeanLeft_Started);
		EIC->BindAction(IA_LeanLeft, ETriggerEvent::Completed, this, &ASHPlayerController::Input_LeanLeft_Completed);
	}
	if (IA_LeanRight)
	{
		EIC->BindAction(IA_LeanRight, ETriggerEvent::Started,   this, &ASHPlayerController::Input_LeanRight_Started);
		EIC->BindAction(IA_LeanRight, ETriggerEvent::Completed, this, &ASHPlayerController::Input_LeanRight_Completed);
	}

	// Interaction
	if (IA_Interact) { EIC->BindAction(IA_Interact, ETriggerEvent::Started, this, &ASHPlayerController::Input_Interact); }

	// Squad menu
	if (IA_SquadMenu)
	{
		EIC->BindAction(IA_SquadMenu, ETriggerEvent::Started,   this, &ASHPlayerController::Input_SquadMenu_Started);
		EIC->BindAction(IA_SquadMenu, ETriggerEvent::Completed, this, &ASHPlayerController::Input_SquadMenu_Completed);
	}

	// Weapon switching / fire mode
	if (IA_CycleFireMode) { EIC->BindAction(IA_CycleFireMode, ETriggerEvent::Started, this, &ASHPlayerController::Input_CycleFireMode); }
	if (IA_PrimaryWeapon) { EIC->BindAction(IA_PrimaryWeapon, ETriggerEvent::Started, this, &ASHPlayerController::Input_PrimaryWeapon); }
	if (IA_Sidearm)       { EIC->BindAction(IA_Sidearm,       ETriggerEvent::Started, this, &ASHPlayerController::Input_Sidearm); }
	if (IA_Grenade)       { EIC->BindAction(IA_Grenade,       ETriggerEvent::Started, this, &ASHPlayerController::Input_Grenade); }

	// Drone / optics / comms
	if (IA_DroneToggle) { EIC->BindAction(IA_DroneToggle, ETriggerEvent::Started, this, &ASHPlayerController::Input_DroneToggle); }
	if (IA_Binoculars)  { EIC->BindAction(IA_Binoculars,  ETriggerEvent::Started, this, &ASHPlayerController::Input_Binoculars); }
	if (IA_Radio)
	{
		EIC->BindAction(IA_Radio, ETriggerEvent::Started,   this, &ASHPlayerController::Input_Radio_Started);
		EIC->BindAction(IA_Radio, ETriggerEvent::Completed, this, &ASHPlayerController::Input_Radio_Completed);
	}
}

void ASHPlayerController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	SHCharacter = Cast<ASHPlayerCharacter>(InPawn);
}

void ASHPlayerController::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Continuous fire logic for automatic weapons.
	if (bIsFiring && SHCharacter && CurrentFireMode == ESHFireMode::FullAuto)
	{
		SHCharacter->StartFire();
	}
}

// =======================================================================
//  Squad commands
// =======================================================================

void ASHPlayerController::OpenSquadCommandMenu()
{
	bSquadMenuOpen = true;

	// Slow time slightly while selecting commands (single-player) or
	// show widget overlay (multiplayer).
	if (GetWorld() && !GetWorld()->IsNetMode(NM_Client))
	{
		GetWorld()->GetWorldSettings()->SetTimeDilation(0.3f);
	}

	UE_LOG(LogTemp, Log, TEXT("[SHPlayerController] Squad command menu opened"));
}

void ASHPlayerController::CloseSquadCommandMenu()
{
	bSquadMenuOpen = false;

	if (GetWorld() && !GetWorld()->IsNetMode(NM_Client))
	{
		GetWorld()->GetWorldSettings()->SetTimeDilation(1.f);
	}
}

void ASHPlayerController::IssueSquadCommand(ESHSquadCommand Command)
{
	if (Command == ESHSquadCommand::None)
	{
		return;
	}

	// For position-based commands, trace to find world location.
	if (Command == ESHSquadCommand::MoveToPosition || Command == ESHSquadCommand::MarkTarget)
	{
		FVector TargetLocation;
		if (PerformSquadCommandTrace(TargetLocation))
		{
			IssueSquadCommandAtLocation(Command, TargetLocation);
			return;
		}
	}

	OnSquadCommandIssued.Broadcast(Command);

	UE_LOG(LogTemp, Log, TEXT("[SHPlayerController] Squad command issued: %d"), static_cast<int32>(Command));
	CloseSquadCommandMenu();
}

void ASHPlayerController::IssueSquadCommandAtLocation(ESHSquadCommand Command, FVector WorldLocation)
{
	OnSquadCommandIssued.Broadcast(Command);

	UE_LOG(LogTemp, Log, TEXT("[SHPlayerController] Squad command %d at location: %s"),
		static_cast<int32>(Command), *WorldLocation.ToString());
	CloseSquadCommandMenu();
}

// =======================================================================
//  Weapon controls
// =======================================================================

void ASHPlayerController::CycleFireMode()
{
	switch (CurrentFireMode)
	{
	case ESHFireMode::Semi:
		CurrentFireMode = ESHFireMode::Burst;
		break;
	case ESHFireMode::Burst:
		CurrentFireMode = ESHFireMode::FullAuto;
		break;
	case ESHFireMode::FullAuto:
		CurrentFireMode = ESHFireMode::Semi;
		break;
	}

	OnFireModeChanged.Broadcast(CurrentFireMode);
	UE_LOG(LogTemp, Log, TEXT("[SHPlayerController] Fire mode: %d"), static_cast<int32>(CurrentFireMode));
}

void ASHPlayerController::SwitchToPrimaryWeapon()
{
	if (SHCharacter)
	{
		SHCharacter->EquipSlot(0);
	}
}

void ASHPlayerController::SwitchToSidearm()
{
	if (SHCharacter)
	{
		SHCharacter->EquipSlot(1);
	}
}

void ASHPlayerController::SwitchToGrenade()
{
	if (SHCharacter)
	{
		SHCharacter->EquipSlot(2);
	}
}

// =======================================================================
//  Drone / optics / comms
// =======================================================================

void ASHPlayerController::ToggleDroneControl()
{
	bDroneControlActive = !bDroneControlActive;
	OnDroneControlToggled.Broadcast(bDroneControlActive);

	if (bDroneControlActive)
	{
		// Switch view to drone camera.
		UE_LOG(LogTemp, Log, TEXT("[SHPlayerController] Drone control activated"));
	}
	else
	{
		UE_LOG(LogTemp, Log, TEXT("[SHPlayerController] Drone control deactivated"));
	}
}

void ASHPlayerController::ToggleBinoculars()
{
	bBinocularsActive = !bBinocularsActive;
	UE_LOG(LogTemp, Log, TEXT("[SHPlayerController] Binoculars: %s"), bBinocularsActive ? TEXT("On") : TEXT("Off"));
}

void ASHPlayerController::ActivateRadio()
{
	bRadioActive = true;
	UE_LOG(LogTemp, Log, TEXT("[SHPlayerController] Radio active"));
}

void ASHPlayerController::DeactivateRadio()
{
	bRadioActive = false;
	UE_LOG(LogTemp, Log, TEXT("[SHPlayerController] Radio off"));
}

// =======================================================================
//  Interaction
// =======================================================================

void ASHPlayerController::TryInteract()
{
	FHitResult Hit;
	if (PerformInteractionTrace(Hit) && Hit.GetActor())
	{
		AActor* HitActor = Hit.GetActor();

		// Check if the hit actor implements the interaction interface.
		if (HitActor->GetClass()->ImplementsInterface(USHInteractable::StaticClass()))
		{
			if (ISHInteractable::Execute_CanInteract(HitActor, this))
			{
				const bool bConsumed = ISHInteractable::Execute_Interact(HitActor, this);
				UE_LOG(LogTemp, Log, TEXT("[SHPlayerController] Interact with: %s (consumed: %s)"),
					*HitActor->GetName(), bConsumed ? TEXT("true") : TEXT("false"));
			}
		}
	}
}

bool ASHPlayerController::PerformInteractionTrace(FHitResult& OutHit) const
{
	FVector CamLoc;
	FRotator CamRot;
	GetPlayerViewPoint(CamLoc, CamRot);

	const FVector TraceEnd = CamLoc + CamRot.Vector() * InteractionTraceDistance;

	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetPawn());

	return GetWorld()->LineTraceSingleByChannel(OutHit, CamLoc, TraceEnd, ECC_Visibility, Params);
}

bool ASHPlayerController::PerformSquadCommandTrace(FVector& OutLocation) const
{
	FVector CamLoc;
	FRotator CamRot;
	GetPlayerViewPoint(CamLoc, CamRot);

	const FVector TraceEnd = CamLoc + CamRot.Vector() * SquadCommandTraceDistance;

	FHitResult Hit;
	FCollisionQueryParams Params;
	Params.AddIgnoredActor(GetPawn());

	if (GetWorld()->LineTraceSingleByChannel(Hit, CamLoc, TraceEnd, ECC_Visibility, Params))
	{
		OutLocation = Hit.ImpactPoint;
		return true;
	}

	return false;
}

// =======================================================================
//  Enhanced Input callbacks
// =======================================================================

void ASHPlayerController::Input_Move(const FInputActionValue& Value)
{
	const FVector2D MoveInput = Value.Get<FVector2D>();
	if (SHCharacter)
	{
		SHCharacter->AddMovementInput(SHCharacter->GetActorForwardVector(), MoveInput.Y);
		SHCharacter->AddMovementInput(SHCharacter->GetActorRightVector(), MoveInput.X);
	}
}

void ASHPlayerController::Input_Look(const FInputActionValue& Value)
{
	const FVector2D LookInput = Value.Get<FVector2D>();
	AddYawInput(LookInput.X);
	AddPitchInput(LookInput.Y);
}

void ASHPlayerController::Input_Fire_Started(const FInputActionValue& Value)
{
	bIsFiring = true;
	if (SHCharacter)
	{
		SHCharacter->StartFire();
	}
}

void ASHPlayerController::Input_Fire_Completed(const FInputActionValue& Value)
{
	bIsFiring = false;
	if (SHCharacter)
	{
		SHCharacter->StopFire();
	}
}

void ASHPlayerController::Input_ADS_Started(const FInputActionValue& Value)
{
	bIsADS = true;
	if (SHCharacter)
	{
		SHCharacter->StartADS();
	}
}

void ASHPlayerController::Input_ADS_Completed(const FInputActionValue& Value)
{
	bIsADS = false;
	if (SHCharacter)
	{
		SHCharacter->StopADS();
	}
}

void ASHPlayerController::Input_Reload(const FInputActionValue& Value)
{
	if (SHCharacter)
	{
		SHCharacter->Reload();
	}
}

void ASHPlayerController::Input_Sprint_Started(const FInputActionValue& Value)
{
	bIsSprinting = true;
	if (SHCharacter)
	{
		SHCharacter->StartSprint();
	}
}

void ASHPlayerController::Input_Sprint_Completed(const FInputActionValue& Value)
{
	bIsSprinting = false;
	if (SHCharacter)
	{
		SHCharacter->StopSprint();
	}
}

void ASHPlayerController::Input_Crouch(const FInputActionValue& Value)
{
	if (SHCharacter)
	{
		SHCharacter->ToggleCrouch();
	}
}

void ASHPlayerController::Input_Prone(const FInputActionValue& Value)
{
	if (SHCharacter)
	{
		SHCharacter->ToggleProne();
	}
}

void ASHPlayerController::Input_Jump(const FInputActionValue& Value)
{
	if (SHCharacter)
	{
		SHCharacter->TryVault();
	}
}

void ASHPlayerController::Input_LeanLeft_Started(const FInputActionValue& Value)
{
	CurrentLeanState = ESHLeanState::Left;
	OnLeanStateChanged.Broadcast(CurrentLeanState);
	if (SHCharacter)
	{
		SHCharacter->SetLeanState(CurrentLeanState);
	}
}

void ASHPlayerController::Input_LeanLeft_Completed(const FInputActionValue& Value)
{
	if (CurrentLeanState == ESHLeanState::Left)
	{
		CurrentLeanState = ESHLeanState::None;
		OnLeanStateChanged.Broadcast(CurrentLeanState);
		if (SHCharacter)
		{
			SHCharacter->SetLeanState(CurrentLeanState);
		}
	}
}

void ASHPlayerController::Input_LeanRight_Started(const FInputActionValue& Value)
{
	CurrentLeanState = ESHLeanState::Right;
	OnLeanStateChanged.Broadcast(CurrentLeanState);
	if (SHCharacter)
	{
		SHCharacter->SetLeanState(CurrentLeanState);
	}
}

void ASHPlayerController::Input_LeanRight_Completed(const FInputActionValue& Value)
{
	if (CurrentLeanState == ESHLeanState::Right)
	{
		CurrentLeanState = ESHLeanState::None;
		OnLeanStateChanged.Broadcast(CurrentLeanState);
		if (SHCharacter)
		{
			SHCharacter->SetLeanState(CurrentLeanState);
		}
	}
}

void ASHPlayerController::Input_Interact(const FInputActionValue& Value)
{
	TryInteract();
}

void ASHPlayerController::Input_SquadMenu_Started(const FInputActionValue& Value)
{
	OpenSquadCommandMenu();
}

void ASHPlayerController::Input_SquadMenu_Completed(const FInputActionValue& Value)
{
	CloseSquadCommandMenu();
}

void ASHPlayerController::Input_CycleFireMode(const FInputActionValue& Value)
{
	CycleFireMode();
}

void ASHPlayerController::Input_PrimaryWeapon(const FInputActionValue& Value)
{
	SwitchToPrimaryWeapon();
}

void ASHPlayerController::Input_Sidearm(const FInputActionValue& Value)
{
	SwitchToSidearm();
}

void ASHPlayerController::Input_Grenade(const FInputActionValue& Value)
{
	SwitchToGrenade();
}

void ASHPlayerController::Input_DroneToggle(const FInputActionValue& Value)
{
	ToggleDroneControl();
}

void ASHPlayerController::Input_Binoculars(const FInputActionValue& Value)
{
	ToggleBinoculars();
}

void ASHPlayerController::Input_Radio_Started(const FInputActionValue& Value)
{
	ActivateRadio();
}

void ASHPlayerController::Input_Radio_Completed(const FInputActionValue& Value)
{
	DeactivateRadio();
}
