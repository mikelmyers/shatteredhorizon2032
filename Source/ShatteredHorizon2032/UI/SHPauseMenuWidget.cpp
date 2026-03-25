// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHPauseMenuWidget.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

USHPauseMenuWidget::USHPauseMenuWidget()
{
}

void USHPauseMenuWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
	SetGamePaused(true);
}

void USHPauseMenuWidget::NativeOnDeactivated()
{
	SetGamePaused(false);
	Super::NativeOnDeactivated();
}

void USHPauseMenuWidget::ExecuteAction(ESHPauseAction Action)
{
	OnPauseAction.Broadcast(Action);

	switch (Action)
	{
	case ESHPauseAction::Resume:
		DeactivateWidget();
		break;

	case ESHPauseAction::Settings:
		UE_LOG(LogTemp, Log, TEXT("[PauseMenu] Opening settings"));
		// Settings widget is pushed onto the CommonUI stack by the owning HUD
		// or player controller listening to OnPauseAction.
		break;

	case ESHPauseAction::RestartMission:
	{
		UE_LOG(LogTemp, Log, TEXT("[PauseMenu] Restarting mission"));
		SetGamePaused(false);

		if (const UWorld* World = GetWorld())
		{
			const FString CurrentMapName = World->GetMapName();
			UGameplayStatics::OpenLevel(World, FName(*CurrentMapName));
		}
		break;
	}

	case ESHPauseAction::QuitToMenu:
	{
		UE_LOG(LogTemp, Log, TEXT("[PauseMenu] Quitting to main menu"));
		SetGamePaused(false);

		if (const UWorld* World = GetWorld())
		{
			// Navigate to the front-end map. Configured via project settings
			// or a dedicated game instance subsystem.
			UGameplayStatics::OpenLevel(World, FName(TEXT("MainMenu")));
		}
		break;
	}
	}
}

void USHPauseMenuWidget::SetGamePaused(bool bPaused)
{
	if (APlayerController* PC = GetOwningPlayer())
	{
		UGameplayStatics::SetGamePaused(GetWorld(), bPaused);
		PC->bShowMouseCursor = bPaused;

		if (bPaused)
		{
			PC->SetInputMode(FInputModeUIOnly().SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock));
		}
		else
		{
			PC->SetInputMode(FInputModeGameOnly());
		}
	}
}
