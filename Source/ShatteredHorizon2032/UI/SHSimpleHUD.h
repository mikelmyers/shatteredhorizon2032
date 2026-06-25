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

private:
	/** Bound to the player's HitFeedback OnHitMarkerTriggered delegate; stamps the
	 *  marker time so DrawHUD can render a brief, fading hit marker at screen center. */
	UFUNCTION()
	void HandleHitMarker(bool bKill);

	/** Lazily bind to the possessed player's hit-marker delegate (player exists a
	 *  frame or two after the HUD). */
	void EnsureHitMarkerBound();

	/** World time of the most recent hit marker (<= 0 = none yet). */
	float LastHitMarkerTime = -1.f;

	/** Whether the most recent hit marker was a kill (red, heavier). */
	bool bLastHitWasKill = false;

	/** True once bound to the player delegate. */
	bool bHitMarkerBound = false;
};
