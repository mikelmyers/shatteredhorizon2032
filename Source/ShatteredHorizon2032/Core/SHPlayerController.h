// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "SHPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class ASHPlayerCharacter;

/** Fire mode for the current weapon. */
UENUM(BlueprintType)
enum class ESHFireMode : uint8
{
	Semi        UMETA(DisplayName = "Semi-Auto"),
	Burst       UMETA(DisplayName = "Burst"),
	FullAuto    UMETA(DisplayName = "Full Auto")
};

/** Lean direction. */
UENUM(BlueprintType)
enum class ESHLeanState : uint8
{
	None,
	Left,
	Right
};

/** Squad command issued through the radial menu. */
UENUM(BlueprintType)
enum class ESHSquadCommand : uint8
{
	None,
	MoveToPosition,
	HoldPosition,
	Advance,
	Fallback,
	Suppress,
	FlankLeft,
	FlankRight,
	RequestMedic,
	RequestAmmo,
	RequestCASSupport,
	MarkTarget,
	Regroup
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHOnSquadCommandIssued, ESHSquadCommand, Command);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHOnFireModeChanged, ESHFireMode, NewMode);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHOnLeanStateChanged, ESHLeanState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHOnDroneControlToggled, bool, bActive);

/**
 * ASHPlayerController
 *
 * Enhanced Input-driven player controller for the squad leader.
 * Handles all input bindings, squad command radial menu, weapon mode
 * switching, lean mechanics, drone control toggling, and optics modes.
 */
UCLASS()
class SHATTEREDHORIZON2032_API ASHPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	ASHPlayerController();

	// ------------------------------------------------------------------
	//  APlayerController overrides
	// ------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void OnPossess(APawn* InPawn) override;
	virtual void Tick(float DeltaSeconds) override;

	// ------------------------------------------------------------------
	//  Squad commands
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "SH|Squad")
	void OpenSquadCommandMenu();

	UFUNCTION(BlueprintCallable, Category = "SH|Squad")
	void CloseSquadCommandMenu();

	UFUNCTION(BlueprintCallable, Category = "SH|Squad")
	void IssueSquadCommand(ESHSquadCommand Command);

	UFUNCTION(BlueprintCallable, Category = "SH|Squad")
	void IssueSquadCommandAtLocation(ESHSquadCommand Command, FVector WorldLocation);

	UPROPERTY(BlueprintAssignable, Category = "SH|Squad")
	FSHOnSquadCommandIssued OnSquadCommandIssued;

	// ------------------------------------------------------------------
	//  Weapon controls
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "SH|Weapon")
	void CycleFireMode();

	UFUNCTION(BlueprintCallable, Category = "SH|Weapon")
	void SwitchToPrimaryWeapon();

	UFUNCTION(BlueprintCallable, Category = "SH|Weapon")
	void SwitchToSidearm();

	UFUNCTION(BlueprintCallable, Category = "SH|Weapon")
	void SwitchToGrenade();

	UFUNCTION(BlueprintPure, Category = "SH|Weapon")
	ESHFireMode GetCurrentFireMode() const { return CurrentFireMode; }

	UPROPERTY(BlueprintAssignable, Category = "SH|Weapon")
	FSHOnFireModeChanged OnFireModeChanged;

	// ------------------------------------------------------------------
	//  Lean mechanics
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|Movement")
	ESHLeanState GetLeanState() const { return CurrentLeanState; }

	UPROPERTY(BlueprintAssignable, Category = "SH|Movement")
	FSHOnLeanStateChanged OnLeanStateChanged;

	// ------------------------------------------------------------------
	//  Drone control
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "SH|Drone")
	void ToggleDroneControl();

	UFUNCTION(BlueprintPure, Category = "SH|Drone")
	bool IsDroneControlActive() const { return bDroneControlActive; }

	UPROPERTY(BlueprintAssignable, Category = "SH|Drone")
	FSHOnDroneControlToggled OnDroneControlToggled;

	// ------------------------------------------------------------------
	//  Optics / comms
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "SH|Optics")
	void ToggleBinoculars();

	UFUNCTION(BlueprintPure, Category = "SH|Optics")
	bool IsBinocularActive() const { return bBinocularsActive; }

	UFUNCTION(BlueprintCallable, Category = "SH|Comms")
	void ActivateRadio();

	UFUNCTION(BlueprintCallable, Category = "SH|Comms")
	void DeactivateRadio();

	UFUNCTION(BlueprintPure, Category = "SH|Comms")
	bool IsRadioActive() const { return bRadioActive; }

	// ------------------------------------------------------------------
	//  Interaction system
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "SH|Interaction")
	void TryInteract();

protected:
	// ------------------------------------------------------------------
	//  Enhanced Input — action bindings
	// ------------------------------------------------------------------

	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_Fire_Started(const FInputActionValue& Value);
	void Input_Fire_Completed(const FInputActionValue& Value);
	void Input_ADS_Started(const FInputActionValue& Value);
	void Input_ADS_Completed(const FInputActionValue& Value);
	void Input_Reload(const FInputActionValue& Value);
	void Input_Sprint_Started(const FInputActionValue& Value);
	void Input_Sprint_Completed(const FInputActionValue& Value);
	void Input_Crouch(const FInputActionValue& Value);
	void Input_Prone(const FInputActionValue& Value);
	void Input_Jump(const FInputActionValue& Value);
	void Input_LeanLeft_Started(const FInputActionValue& Value);
	void Input_LeanLeft_Completed(const FInputActionValue& Value);
	void Input_LeanRight_Started(const FInputActionValue& Value);
	void Input_LeanRight_Completed(const FInputActionValue& Value);
	void Input_Interact(const FInputActionValue& Value);
	void Input_SquadMenu_Started(const FInputActionValue& Value);
	void Input_SquadMenu_Completed(const FInputActionValue& Value);
	void Input_CycleFireMode(const FInputActionValue& Value);
	void Input_PrimaryWeapon(const FInputActionValue& Value);
	void Input_Sidearm(const FInputActionValue& Value);
	void Input_Grenade(const FInputActionValue& Value);
	void Input_DroneToggle(const FInputActionValue& Value);
	void Input_Binoculars(const FInputActionValue& Value);
	void Input_Radio_Started(const FInputActionValue& Value);
	void Input_Radio_Completed(const FInputActionValue& Value);

	/** Perform a line trace from the camera for interaction detection. */
	bool PerformInteractionTrace(FHitResult& OutHit) const;

	/** Perform a line trace for squad command world-position targeting. */
	bool PerformSquadCommandTrace(FVector& OutLocation) const;

	// ------------------------------------------------------------------
	//  Input assets (set in editor or via code)
	// ------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, Category = "SH|Input")
	TObjectPtr<UInputMappingContext> DefaultMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Input")
	TObjectPtr<UInputAction> IA_Move;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Input")
	TObjectPtr<UInputAction> IA_Look;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Input")
	TObjectPtr<UInputAction> IA_Fire;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Input")
	TObjectPtr<UInputAction> IA_ADS;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Input")
	TObjectPtr<UInputAction> IA_Reload;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Input")
	TObjectPtr<UInputAction> IA_Sprint;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Input")
	TObjectPtr<UInputAction> IA_Crouch;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Input")
	TObjectPtr<UInputAction> IA_Prone;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Input")
	TObjectPtr<UInputAction> IA_Jump;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Input")
	TObjectPtr<UInputAction> IA_LeanLeft;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Input")
	TObjectPtr<UInputAction> IA_LeanRight;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Input")
	TObjectPtr<UInputAction> IA_Interact;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Input")
	TObjectPtr<UInputAction> IA_SquadMenu;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Input")
	TObjectPtr<UInputAction> IA_CycleFireMode;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Input")
	TObjectPtr<UInputAction> IA_PrimaryWeapon;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Input")
	TObjectPtr<UInputAction> IA_Sidearm;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Input")
	TObjectPtr<UInputAction> IA_Grenade;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Input")
	TObjectPtr<UInputAction> IA_DroneToggle;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Input")
	TObjectPtr<UInputAction> IA_Binoculars;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Input")
	TObjectPtr<UInputAction> IA_Radio;

	// ------------------------------------------------------------------
	//  Config
	// ------------------------------------------------------------------

	/** Max distance for interaction traces (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Interaction")
	float InteractionTraceDistance = 250.f;

	/** Max distance for squad command placement traces (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Squad")
	float SquadCommandTraceDistance = 50000.f;

private:
	UPROPERTY()
	TObjectPtr<ASHPlayerCharacter> SHCharacter = nullptr;

	ESHFireMode CurrentFireMode = ESHFireMode::Semi;
	ESHLeanState CurrentLeanState = ESHLeanState::None;

	bool bDroneControlActive = false;
	bool bBinocularsActive = false;
	bool bRadioActive = false;
	bool bSquadMenuOpen = false;
	bool bIsFiring = false;
	bool bIsADS = false;
	bool bIsSprinting = false;
};
