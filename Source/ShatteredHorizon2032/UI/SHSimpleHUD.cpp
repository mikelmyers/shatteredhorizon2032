// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "UI/SHSimpleHUD.h"
#include "Core/SHPlayerCharacter.h"
#include "Core/SHGameState.h"
#include "Weapons/SHWeaponBase.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Engine.h"

void ASHSimpleHUD::DrawHUD()
{
	Super::DrawHUD();

	if (!Canvas)
	{
		return;
	}

	const float CX = Canvas->ClipX * 0.5f;
	const float CY = Canvas->ClipY * 0.5f;
	const FLinearColor HudGreen(0.65f, 0.85f, 0.65f, 0.85f);

	// --- Crosshair ---
	static const float Gap = 5.f;
	static const float Arm = 9.f;
	DrawLine(CX - Gap - Arm, CY, CX - Gap, CY, HudGreen, 1.5f);
	DrawLine(CX + Gap, CY, CX + Gap + Arm, CY, HudGreen, 1.5f);
	DrawLine(CX, CY - Gap - Arm, CX, CY - Gap, HudGreen, 1.5f);
	DrawLine(CX, CY + Gap, CX, CY + Gap + Arm, HudGreen, 1.5f);

	const ASHPlayerCharacter* Player = Cast<ASHPlayerCharacter>(GetOwningPawn());
	if (!Player)
	{
		return;
	}

	UFont* Font = GEngine ? GEngine->GetMediumFont() : nullptr;

	// --- Health (bottom-left) ---
	const FString HealthStr = FString::Printf(TEXT("HP %3.0f"), Player->GetCurrentHealth());
	DrawText(HealthStr, Player->GetCurrentHealth() > 30.f ? HudGreen
		: FLinearColor(0.9f, 0.25f, 0.2f, 0.9f), 40.f, Canvas->ClipY - 80.f, Font, 1.4f);

	const float Suppression = Player->GetSuppressionLevel();
	if (Suppression > 0.05f)
	{
		DrawText(FString::Printf(TEXT("SUPPRESSED %2.0f%%"), Suppression * 100.f),
			FLinearColor(0.95f, 0.6f, 0.2f, 0.9f), 40.f, Canvas->ClipY - 56.f, Font, 1.f);
	}

	// --- Ammo (bottom-right) ---
	if (const ASHWeaponBase* Weapon = Player->GetEquippedWeapon())
	{
		const FString AmmoStr = FString::Printf(TEXT("%d / %d"),
			Weapon->GetCurrentMagAmmo(), Weapon->GetReserveAmmo());
		DrawText(AmmoStr, HudGreen, Canvas->ClipX - 170.f, Canvas->ClipY - 80.f, Font, 1.6f);
	}

	// --- Compass heading (top-center) ---
	if (const APlayerController* PC = GetOwningPlayerController())
	{
		const float Yaw = FRotator::NormalizeAxis(PC->GetControlRotation().Yaw);
		const float Heading = FMath::Fmod(Yaw + 360.f, 360.f);
		static const TCHAR* Cardinals[] = { TEXT("N"), TEXT("NE"), TEXT("E"), TEXT("SE"),
			TEXT("S"), TEXT("SW"), TEXT("W"), TEXT("NW") };
		const int32 CardinalIdx = FMath::RoundToInt(Heading / 45.f) % 8;
		DrawText(FString::Printf(TEXT("%03.0f  %s"), Heading, Cardinals[CardinalIdx]),
			HudGreen, CX - 36.f, 28.f, Font, 1.2f);
	}

	// --- Mission phase (top-left) ---
	if (const ASHGameState* GS = GetWorld() ? GetWorld()->GetGameState<ASHGameState>() : nullptr)
	{
		DrawText(FString::Printf(TEXT("M01 TAOYUAN BEACH  —  PHASE: %s"),
			*UEnum::GetDisplayValueAsText(GS->GetMissionPhase()).ToString()),
			FLinearColor(0.75f, 0.8f, 0.75f, 0.7f), 40.f, 28.f, Font, 1.f);
	}
}
