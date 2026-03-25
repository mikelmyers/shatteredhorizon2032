// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "SHSpectatorController.generated.h"

class UInputMappingContext;
class UInputAction;
class APawn;

/* -----------------------------------------------------------------------
 *  Enums
 * --------------------------------------------------------------------- */

UENUM(BlueprintType)
enum class ESHSpectatorMode : uint8
{
	FreeCamera        UMETA(DisplayName = "Free Camera"),
	FirstPerson       UMETA(DisplayName = "First Person"),
	TacticalOverhead  UMETA(DisplayName = "Tactical Overhead")
};

/* -----------------------------------------------------------------------
 *  Delegates
 * --------------------------------------------------------------------- */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnSpectatorModeChanged, ESHSpectatorMode, OldMode, ESHSpectatorMode, NewMode);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnSpectatorTargetChanged, AActor*, NewTarget);

/* -----------------------------------------------------------------------
 *  ASHSpectatorController
 * --------------------------------------------------------------------- */

/**
 * ASHSpectatorController
 *
 * Spectator/observer controller supporting three camera modes:
 * - Free camera: WASD + mouse flight, shift for speed boost
 * - First-person: cycle through alive players and AI units
 * - Tactical overhead: top-down view with zoom control
 *
 * Also provides a toggle for the AI decision debug overlay,
 * showing which Primordia subsystem issued the most recent decision.
 */
UCLASS()
class SHATTEREDHORIZON2032_API ASHSpectatorController : public APlayerController
{
	GENERATED_BODY()

public:
	ASHSpectatorController();

	// ------------------------------------------------------------------
	//  APlayerController overrides
	// ------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void Tick(float DeltaSeconds) override;

	// ------------------------------------------------------------------
	//  Mode switching
	// ------------------------------------------------------------------

	/** Switch to the specified spectator mode. */
	UFUNCTION(BlueprintCallable, Category = "SH|Spectator")
	void SwitchMode(ESHSpectatorMode NewMode);

	/** Cycle to the next/previous spectatable target. */
	UFUNCTION(BlueprintCallable, Category = "SH|Spectator")
	void CycleTarget(bool bForward);

	// ------------------------------------------------------------------
	//  Queries
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|Spectator")
	ESHSpectatorMode GetSpectatorMode() const { return CurrentMode; }

	UFUNCTION(BlueprintPure, Category = "SH|Spectator")
	AActor* GetCurrentTarget() const { return CurrentTarget; }

	// ------------------------------------------------------------------
	//  AI debug overlay
	// ------------------------------------------------------------------

	/** Toggle the AI decision overlay (shows which subsystem fired). */
	UFUNCTION(BlueprintCallable, Category = "SH|Spectator|Debug")
	void ToggleAIDecisionOverlay();

	UFUNCTION(BlueprintPure, Category = "SH|Spectator|Debug")
	bool IsAIDecisionOverlayVisible() const { return bAIOverlayVisible; }

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Spectator")
	FOnSpectatorModeChanged OnSpectatorModeChanged;

	UPROPERTY(BlueprintAssignable, Category = "SH|Spectator")
	FOnSpectatorTargetChanged OnSpectatorTargetChanged;

protected:
	// ------------------------------------------------------------------
	//  Input handlers
	// ------------------------------------------------------------------
	void Input_Move(const FInputActionValue& Value);
	void Input_Look(const FInputActionValue& Value);
	void Input_SpeedBoost_Started(const FInputActionValue& Value);
	void Input_SpeedBoost_Completed(const FInputActionValue& Value);
	void Input_SwitchMode(const FInputActionValue& Value);
	void Input_CycleTargetForward(const FInputActionValue& Value);
	void Input_CycleTargetBackward(const FInputActionValue& Value);
	void Input_Zoom(const FInputActionValue& Value);
	void Input_ToggleOverlay(const FInputActionValue& Value);

	// ------------------------------------------------------------------
	//  Mode-specific tick
	// ------------------------------------------------------------------
	void TickFreeCamera(float DeltaSeconds);
	void TickFirstPerson(float DeltaSeconds);
	void TickTacticalOverhead(float DeltaSeconds);

	// ------------------------------------------------------------------
	//  Target management
	// ------------------------------------------------------------------

	/** Build the list of spectatable targets (players + AI units). */
	void RefreshTargetList();

	/** Attach the camera to the current target in first-person mode. */
	void AttachToTarget();

	/** Detach from any followed target. */
	void DetachFromTarget();

	// ------------------------------------------------------------------
	//  Input assets
	// ------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, Category = "SH|Spectator|Input")
	TObjectPtr<UInputMappingContext> SpectatorMappingContext;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Spectator|Input")
	TObjectPtr<UInputAction> IA_SpectatorMove;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Spectator|Input")
	TObjectPtr<UInputAction> IA_SpectatorLook;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Spectator|Input")
	TObjectPtr<UInputAction> IA_SpectatorSpeedBoost;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Spectator|Input")
	TObjectPtr<UInputAction> IA_SpectatorSwitchMode;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Spectator|Input")
	TObjectPtr<UInputAction> IA_SpectatorCycleForward;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Spectator|Input")
	TObjectPtr<UInputAction> IA_SpectatorCycleBackward;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Spectator|Input")
	TObjectPtr<UInputAction> IA_SpectatorZoom;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Spectator|Input")
	TObjectPtr<UInputAction> IA_SpectatorToggleOverlay;

	// ------------------------------------------------------------------
	//  Config
	// ------------------------------------------------------------------

	/** Base free camera speed (cm/s). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Spectator|Config")
	float FreeCameraSpeed = 1200.0f;

	/** Speed multiplier when shift/boost is held. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Spectator|Config")
	float SpeedBoostMultiplier = 3.0f;

	/** Mouse sensitivity for free camera rotation. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Spectator|Config")
	float FreeCameraLookSensitivity = 1.0f;

	/** Default overhead camera height (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Spectator|Config")
	float OverheadDefaultHeight = 15000.0f;

	/** Min overhead zoom height (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Spectator|Config")
	float OverheadMinHeight = 5000.0f;

	/** Max overhead zoom height (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Spectator|Config")
	float OverheadMaxHeight = 50000.0f;

	/** Zoom step per scroll tick (cm). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Spectator|Config")
	float OverheadZoomStep = 1000.0f;

	/** How often to refresh the target list (seconds). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Spectator|Config")
	float TargetRefreshInterval = 2.0f;

private:
	ESHSpectatorMode CurrentMode = ESHSpectatorMode::FreeCamera;

	UPROPERTY()
	TObjectPtr<AActor> CurrentTarget = nullptr;

	UPROPERTY()
	TArray<TObjectPtr<AActor>> SpectateTargets;

	int32 CurrentTargetIndex = INDEX_NONE;

	bool bSpeedBoosting = false;
	bool bAIOverlayVisible = false;

	float OverheadCurrentHeight = 15000.0f;

	/** Accumulated time since last target list refresh. */
	float TargetRefreshAccumulator = 0.0f;

	/** Pending move input for free camera. */
	FVector PendingMoveInput = FVector::ZeroVector;

	/** Pending look input. */
	FVector2D PendingLookInput = FVector2D::ZeroVector;
};
