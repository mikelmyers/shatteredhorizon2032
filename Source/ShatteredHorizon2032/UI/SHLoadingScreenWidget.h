// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "SHLoadingScreenWidget.generated.h"

/**
 * USHLoadingScreenWidget
 *
 * Loading screen displayed during level transitions. Shows:
 *  - Mission name and briefing snippet
 *  - Rotating tactical tips
 *  - Loading progress bar
 *
 * Uses CommonUI so that it integrates correctly with the widget stack
 * and can block input during loading.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class SHATTEREDHORIZON2032_API USHLoadingScreenWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	USHLoadingScreenWidget();

	// ------------------------------------------------------------------
	//  Public API
	// ------------------------------------------------------------------

	/** Set the loading progress bar value (0.0 - 1.0). */
	UFUNCTION(BlueprintCallable, Category = "SH|UI|Loading")
	void SetLoadingProgress(float Progress);

	/** Set mission name and description shown on the loading screen. */
	UFUNCTION(BlueprintCallable, Category = "SH|UI|Loading")
	void SetMissionInfo(FText Name, FText Desc);

	/** Set the pool of tactical tips to rotate through. */
	UFUNCTION(BlueprintCallable, Category = "SH|UI|Loading")
	void SetTacticalTips(const TArray<FText>& InTips);

	/** Manually advance to the next tactical tip. */
	UFUNCTION(BlueprintCallable, Category = "SH|UI|Loading")
	void ShowNextTip();

	// ------------------------------------------------------------------
	//  Accessors (for Blueprint widget binding)
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|UI|Loading")
	float GetLoadingProgress() const { return LoadingProgress; }

	UFUNCTION(BlueprintPure, Category = "SH|UI|Loading")
	const FText& GetMissionName() const { return MissionName; }

	UFUNCTION(BlueprintPure, Category = "SH|UI|Loading")
	const FText& GetMissionDescription() const { return MissionDescription; }

	UFUNCTION(BlueprintPure, Category = "SH|UI|Loading")
	const FText& GetCurrentTip() const { return CurrentTip; }

	// ------------------------------------------------------------------
	//  Config
	// ------------------------------------------------------------------

	/** Seconds between automatic tip rotations. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|UI|Loading")
	float TipRotationInterval = 8.f;

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UPROPERTY(BlueprintReadOnly, Category = "SH|UI|Loading")
	float LoadingProgress = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "SH|UI|Loading")
	FText MissionName;

	UPROPERTY(BlueprintReadOnly, Category = "SH|UI|Loading")
	FText MissionDescription;

	UPROPERTY(BlueprintReadOnly, Category = "SH|UI|Loading")
	FText CurrentTip;

private:
	/** Pool of tactical tips. */
	UPROPERTY()
	TArray<FText> TacticalTips;

	/** Index of the currently displayed tip. */
	int32 CurrentTipIndex = 0;

	/** Accumulated time since the last tip rotation. */
	float TipTimer = 0.f;

	/** Select the next tip from the pool and update CurrentTip. */
	void AdvanceTip();

	/** Populate default tips when none are provided. */
	void PopulateDefaultTips();
};
