// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "SHSettingsWidget.h"
#include "SHSettingsSaveGame.generated.h"

/**
 * USHSettingsSaveGame
 *
 * Minimal USaveGame wrapper for persisting player settings.
 */
UCLASS()
class SHATTEREDHORIZON2032_API USHSettingsSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FSHGameSettings Settings;
};
