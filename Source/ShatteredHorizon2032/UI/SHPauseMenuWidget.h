// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "SHPauseMenuWidget.generated.h"

/** Actions available from the pause menu. */
UENUM(BlueprintType)
enum class ESHPauseAction : uint8
{
	Resume			UMETA(DisplayName = "Resume"),
	Settings		UMETA(DisplayName = "Settings"),
	RestartMission	UMETA(DisplayName = "Restart Mission"),
	QuitToMenu		UMETA(DisplayName = "Quit to Menu")
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPauseAction, ESHPauseAction, Action);

/**
 * USHPauseMenuWidget
 *
 * In-game pause menu using CommonUI for unified input handling.
 * Pauses game time on activation and resumes on deactivation.
 *
 * Layout:
 *  - Resume
 *  - Settings
 *  - Restart Mission
 *  - Quit to Menu
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class SHATTEREDHORIZON2032_API USHPauseMenuWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	USHPauseMenuWidget();

	/** Execute a pause menu action. Called from Blueprint button bindings. */
	UFUNCTION(BlueprintCallable, Category = "SH|UI|PauseMenu")
	void ExecuteAction(ESHPauseAction Action);

	/** Fired when any pause action is selected. */
	UPROPERTY(BlueprintAssignable, Category = "SH|UI|PauseMenu")
	FOnPauseAction OnPauseAction;

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

private:
	/** Applies or removes the game pause. */
	void SetGamePaused(bool bPaused);
};
