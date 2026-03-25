// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHMainMenuWidget.h"
#include "Kismet/GameplayStatics.h"

USHMainMenuWidget::USHMainMenuWidget()
{
}

void USHMainMenuWidget::NativeOnActivated()
{
	Super::NativeOnActivated();
}

void USHMainMenuWidget::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();
}

void USHMainMenuWidget::ExecuteAction(ESHMainMenuAction Action)
{
	OnMainMenuAction.Broadcast(Action);

	switch (Action)
	{
	case ESHMainMenuAction::Campaign:
		UE_LOG(LogTemp, Log, TEXT("[MainMenu] Campaign selected"));
		break;
	case ESHMainMenuAction::Operations:
		UE_LOG(LogTemp, Log, TEXT("[MainMenu] Operations selected"));
		break;
	case ESHMainMenuAction::Training:
		UE_LOG(LogTemp, Log, TEXT("[MainMenu] Training selected"));
		break;
	case ESHMainMenuAction::Loadout:
		UE_LOG(LogTemp, Log, TEXT("[MainMenu] Loadout selected"));
		break;
	case ESHMainMenuAction::Settings:
		UE_LOG(LogTemp, Log, TEXT("[MainMenu] Settings selected"));
		break;
	case ESHMainMenuAction::Quit:
		UGameplayStatics::GetPlayerController(GetWorld(), 0)->ConsoleCommand(TEXT("quit"));
		break;
	}
}

bool USHMainMenuWidget::HasExistingSave() const
{
	return UGameplayStatics::DoesSaveGameExist(TEXT("SH2032_Campaign"), 0);
}
