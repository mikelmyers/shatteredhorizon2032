// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "SHMainMenuWidget.generated.h"

/**
 * Main menu entry options.
 */
UENUM(BlueprintType)
enum class ESHMainMenuAction : uint8
{
	Campaign		UMETA(DisplayName = "Campaign"),
	Operations		UMETA(DisplayName = "Operations"),
	Training		UMETA(DisplayName = "Training"),
	Loadout			UMETA(DisplayName = "Loadout"),
	Settings		UMETA(DisplayName = "Settings"),
	Quit			UMETA(DisplayName = "Quit")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMainMenuAction, ESHMainMenuAction, Action);

/**
 * USHMainMenuWidget
 *
 * Root widget for the main menu. Uses CommonUI for cross-platform
 * input handling (mouse, keyboard, gamepad).
 *
 * Layout:
 *  - Game title / background
 *  - Campaign (continue / new)
 *  - Operations (join / host)
 *  - Training
 *  - Loadout
 *  - Settings
 *  - Quit
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class SHATTEREDHORIZON2032_API USHMainMenuWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	USHMainMenuWidget();

	/** Execute a menu action. */
	UFUNCTION(BlueprintCallable, Category = "SH|UI|MainMenu")
	void ExecuteAction(ESHMainMenuAction Action);

	/** Whether a save exists for "Continue Campaign". */
	UFUNCTION(BlueprintPure, Category = "SH|UI|MainMenu")
	bool HasExistingSave() const;

	UPROPERTY(BlueprintAssignable, Category = "SH|UI|MainMenu")
	FOnMainMenuAction OnMainMenuAction;

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
};
