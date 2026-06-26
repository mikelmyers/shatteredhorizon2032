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
/** One active directional damage indicator (where incoming fire came from). */
struct FSHActiveHitIndicator
{
	float Angle = 0.f;       // degrees, 0 = ahead, clockwise
	float Intensity = 1.f;   // 0..1, scales size/opacity
	float StartTime = 0.f;   // world seconds
	float Duration = 1.5f;   // seconds visible
};

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

	/** Bound to the player's HitFeedback OnHitIndicator delegate; pushes a directional
	 *  damage indicator pointing toward the threat. */
	UFUNCTION()
	void HandleHitIndicator(float Angle, float Intensity, float Duration);

	/** Lazily bind to the possessed player's feedback delegates (the player exists a
	 *  frame or two after the HUD). */
	void EnsureFeedbackBound();

	/** Draw active directional damage indicators around the crosshair. */
	void DrawDamageIndicators(float CX, float CY);

	/** World time of the most recent hit marker (<= 0 = none yet). */
	float LastHitMarkerTime = -1.f;

	/** Whether the most recent hit marker was a kill (red, heavier). */
	bool bLastHitWasKill = false;

	/** Active directional damage indicators (expired ones are pruned each draw). */
	TArray<FSHActiveHitIndicator> ActiveIndicators;

	/** True once bound to the player delegates. */
	bool bFeedbackBound = false;
};
