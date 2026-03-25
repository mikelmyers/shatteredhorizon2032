// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "Core/SHPlayerState.h"
#include "SHAfterActionWidget.generated.h"

/** Breakdown of XP awarded at end of mission. */
USTRUCT(BlueprintType)
struct FSHXPBreakdown
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XP")
	int32 CombatXP = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XP")
	int32 ObjectiveXP = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XP")
	int32 TeamworkXP = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XP")
	int32 AccuracyBonusXP = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "XP")
	int32 SurvivalBonusXP = 0;

	/** Total XP from all sources. */
	int32 GetTotal() const
	{
		return CombatXP + ObjectiveXP + TeamworkXP + AccuracyBonusXP + SurvivalBonusXP;
	}
};

/** Data needed to display rank progress. */
USTRUCT(BlueprintType)
struct FSHRankProgress
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rank")
	FText CurrentRankName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rank")
	FText NextRankName;

	/** XP accumulated toward the next rank. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rank")
	int32 CurrentXP = 0;

	/** XP required to reach the next rank. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Rank")
	int32 RequiredXP = 1000;

	/** Normalized 0-1 progress bar value. */
	float GetProgress() const
	{
		return (RequiredXP > 0)
			? FMath::Clamp(static_cast<float>(CurrentXP) / static_cast<float>(RequiredXP), 0.f, 1.f)
			: 1.f;
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAfterActionContinue);

/**
 * USHAfterActionWidget
 *
 * Post-mission After Action Report (AAR) screen. Presents:
 *  - Engagement statistics from FSHEngagementRecord
 *  - AI analysis summary text
 *  - XP awarded breakdown
 *  - Rank progress bar
 *
 * Uses CommonUI for unified input handling.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class SHATTEREDHORIZON2032_API USHAfterActionWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	USHAfterActionWidget();

	// ------------------------------------------------------------------
	//  Data population
	// ------------------------------------------------------------------

	/** Populate the AAR with engagement data and supporting information. */
	UFUNCTION(BlueprintCallable, Category = "SH|UI|AAR")
	void SetAfterActionData(
		const FSHEngagementRecord& InRecord,
		const FText& InAIAnalysis,
		const FSHXPBreakdown& InXPBreakdown,
		const FSHRankProgress& InRankProgress);

	// ------------------------------------------------------------------
	//  Accessors
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|UI|AAR")
	const FSHEngagementRecord& GetEngagementRecord() const { return EngagementRecord; }

	UFUNCTION(BlueprintPure, Category = "SH|UI|AAR")
	const FText& GetAIAnalysis() const { return AIAnalysis; }

	UFUNCTION(BlueprintPure, Category = "SH|UI|AAR")
	const FSHXPBreakdown& GetXPBreakdown() const { return XPBreakdown; }

	UFUNCTION(BlueprintPure, Category = "SH|UI|AAR")
	const FSHRankProgress& GetRankProgress() const { return RankProgress; }

	/** Accuracy percentage (0-100). */
	UFUNCTION(BlueprintPure, Category = "SH|UI|AAR")
	float GetAccuracyPercent() const;

	/** Longest kill distance converted from cm to meters for display. */
	UFUNCTION(BlueprintPure, Category = "SH|UI|AAR")
	float GetLongestKillDistanceMeters() const;

	/** Total XP awarded. */
	UFUNCTION(BlueprintPure, Category = "SH|UI|AAR")
	int32 GetTotalXPAwarded() const { return XPBreakdown.GetTotal(); }

	/** Normalized rank progress (0-1). */
	UFUNCTION(BlueprintPure, Category = "SH|UI|AAR")
	float GetRankProgressNormalized() const { return RankProgress.GetProgress(); }

	// ------------------------------------------------------------------
	//  Actions
	// ------------------------------------------------------------------

	/** Called when the player presses Continue to leave the AAR screen. */
	UFUNCTION(BlueprintCallable, Category = "SH|UI|AAR")
	void Continue();

	UPROPERTY(BlueprintAssignable, Category = "SH|UI|AAR")
	FOnAfterActionContinue OnContinue;

protected:
	virtual void NativeOnActivated() override;
	virtual void NativeOnDeactivated() override;

	UPROPERTY(BlueprintReadOnly, Category = "SH|UI|AAR")
	FSHEngagementRecord EngagementRecord;

	UPROPERTY(BlueprintReadOnly, Category = "SH|UI|AAR")
	FText AIAnalysis;

	UPROPERTY(BlueprintReadOnly, Category = "SH|UI|AAR")
	FSHXPBreakdown XPBreakdown;

	UPROPERTY(BlueprintReadOnly, Category = "SH|UI|AAR")
	FSHRankProgress RankProgress;
};
