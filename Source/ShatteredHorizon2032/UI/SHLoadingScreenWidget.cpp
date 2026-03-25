// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHLoadingScreenWidget.h"

USHLoadingScreenWidget::USHLoadingScreenWidget()
{
}

// ------------------------------------------------------------------
//  Lifecycle
// ------------------------------------------------------------------

void USHLoadingScreenWidget::NativeOnActivated()
{
	Super::NativeOnActivated();

	// Ensure there is always at least one tip to show.
	if (TacticalTips.IsEmpty())
	{
		PopulateDefaultTips();
	}

	CurrentTipIndex = 0;
	TipTimer = 0.f;
	AdvanceTip();

	UE_LOG(LogTemp, Log, TEXT("[Loading] Loading screen activated — tips available: %d"),
		   TacticalTips.Num());
}

void USHLoadingScreenWidget::NativeOnDeactivated()
{
	Super::NativeOnDeactivated();
}

void USHLoadingScreenWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Auto-rotate tactical tips.
	if (TacticalTips.Num() > 1 && TipRotationInterval > 0.f)
	{
		TipTimer += InDeltaTime;
		if (TipTimer >= TipRotationInterval)
		{
			TipTimer -= TipRotationInterval;
			AdvanceTip();
		}
	}
}

// ------------------------------------------------------------------
//  Public API
// ------------------------------------------------------------------

void USHLoadingScreenWidget::SetLoadingProgress(float Progress)
{
	LoadingProgress = FMath::Clamp(Progress, 0.f, 1.f);
}

void USHLoadingScreenWidget::SetMissionInfo(FText Name, FText Desc)
{
	MissionName = MoveTemp(Name);
	MissionDescription = MoveTemp(Desc);

	UE_LOG(LogTemp, Log, TEXT("[Loading] Mission info set — %s"), *MissionName.ToString());
}

void USHLoadingScreenWidget::SetTacticalTips(const TArray<FText>& InTips)
{
	TacticalTips = InTips;
	CurrentTipIndex = 0;
	TipTimer = 0.f;

	if (!TacticalTips.IsEmpty())
	{
		AdvanceTip();
	}
}

void USHLoadingScreenWidget::ShowNextTip()
{
	AdvanceTip();
	TipTimer = 0.f; // Reset auto-rotation timer on manual advance.
}

// ------------------------------------------------------------------
//  Private helpers
// ------------------------------------------------------------------

void USHLoadingScreenWidget::AdvanceTip()
{
	if (TacticalTips.IsEmpty())
	{
		CurrentTip = FText::GetEmpty();
		return;
	}

	CurrentTip = TacticalTips[CurrentTipIndex];
	CurrentTipIndex = (CurrentTipIndex + 1) % TacticalTips.Num();
}

void USHLoadingScreenWidget::PopulateDefaultTips()
{
	TacticalTips.Empty();
	TacticalTips.Reserve(12);

	TacticalTips.Add(FText::FromString(TEXT("Use smoke grenades to break enemy line of sight before crossing open ground.")));
	TacticalTips.Add(FText::FromString(TEXT("Suppressive fire degrades enemy accuracy and morale. Keep their heads down.")));
	TacticalTips.Add(FText::FromString(TEXT("Prone dramatically improves weapon accuracy but limits your mobility.")));
	TacticalTips.Add(FText::FromString(TEXT("Stay close to your squad. Isolated operators are vulnerable targets.")));
	TacticalTips.Add(FText::FromString(TEXT("Reload behind cover. A tactical reload with a round in the chamber is faster.")));
	TacticalTips.Add(FText::FromString(TEXT("FPV drones can strike around corners and behind cover where direct fire cannot.")));
	TacticalTips.Add(FText::FromString(TEXT("Monitor squad morale. Sustained enemy fire and casualties degrade unit effectiveness.")));
	TacticalTips.Add(FText::FromString(TEXT("Switch to semi-auto at range for better accuracy and ammo conservation.")));
	TacticalTips.Add(FText::FromString(TEXT("Use ISR drones to scout ahead before committing your squad to an objective.")));
	TacticalTips.Add(FText::FromString(TEXT("Electronic warfare can disrupt enemy communications and drone operations.")));
	TacticalTips.Add(FText::FromString(TEXT("Fatigue accumulates during sustained movement. Rest to recover full combat effectiveness.")));
	TacticalTips.Add(FText::FromString(TEXT("Revive downed squad members quickly — they lose morale the longer they are incapacitated.")));
}
