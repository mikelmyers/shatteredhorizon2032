// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHAfterActionWidget.h"

USHAfterActionWidget::USHAfterActionWidget()
{
}

void USHAfterActionWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	UE_LOG(LogTemp, Log, TEXT("[AAR] After Action Report activated — Kills: %d, Accuracy: %.1f%%, XP: %d"),
		   EngagementRecord.Kills,
		   GetAccuracyPercent(),
		   GetTotalXPAwarded());
}

void USHAfterActionWidget::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();
}

// ------------------------------------------------------------------
//  Data population
// ------------------------------------------------------------------

void USHAfterActionWidget::SetAfterActionData(
	const FSHEngagementRecord& InRecord,
	const FText& InAIAnalysis,
	const FSHXPBreakdown& InXPBreakdown,
	const FSHRankProgress& InRankProgress)
{
	EngagementRecord = InRecord;
	AIAnalysis = InAIAnalysis;
	XPBreakdown = InXPBreakdown;
	RankProgress = InRankProgress;

	UE_LOG(LogTemp, Log,
		TEXT("[AAR] Data set — K: %d  A: %d  Obj: %d  Revives: %d  "
			 "Accuracy: %.1f%%  DmgDealt: %.0f  DmgTaken: %.0f  "
			 "LongestKill: %.0fm  XP: %d"),
		EngagementRecord.Kills,
		EngagementRecord.Assists,
		EngagementRecord.ObjectivesCompleted,
		EngagementRecord.Revives,
		GetAccuracyPercent(),
		EngagementRecord.DamageDealt,
		EngagementRecord.DamageTaken,
		GetLongestKillDistanceMeters(),
		GetTotalXPAwarded());
}

// ------------------------------------------------------------------
//  Accessors
// ------------------------------------------------------------------

float USHAfterActionWidget::GetAccuracyPercent() const
{
	return EngagementRecord.GetAccuracy() * 100.f;
}

float USHAfterActionWidget::GetLongestKillDistanceMeters() const
{
	// Internal distance is stored in centimeters; convert to meters for display.
	return EngagementRecord.LongestKillDistance / 100.f;
}

// ------------------------------------------------------------------
//  Actions
// ------------------------------------------------------------------

void USHAfterActionWidget::Continue()
{
	UE_LOG(LogTemp, Log, TEXT("[AAR] Player pressed Continue"));

	OnContinue.Broadcast();
	DeactivateWidget();
}
