// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SHSimpleHUD.generated.h"

/**
 * ASHSimpleHUD
 *
 * Minimal canvas HUD: crosshair, health, ammo, compass heading, phase line.
 * Stands in until the full UMG widget set (WBP_HUD) is authored; everything it
 * shows is read directly from the possessed ASHPlayerCharacter.
 */
UCLASS()
class SHATTEREDHORIZON2032_API ASHSimpleHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;
};
