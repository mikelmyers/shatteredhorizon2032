// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "UI/SHSimpleHUD.h"
#include "Core/SHPlayerCharacter.h"
#include "Core/SHGameState.h"
#include "Weapons/SHWeaponBase.h"
#include "Combat/SHHitFeedback.h"
#include "Engine/Canvas.h"
#include "Engine/Font.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

void ASHSimpleHUD::EnsureHitMarkerBound()
{
	if (bHitMarkerBound)
	{
		return;
	}

	if (ASHPlayerCharacter* Player = Cast<ASHPlayerCharacter>(GetOwningPawn()))
	{
		if (USHHitFeedback* HitFeedback = Player->HitFeedback)
		{
			HitFeedback->OnHitMarkerTriggered.AddDynamic(this, &ASHSimpleHUD::HandleHitMarker);
			bHitMarkerBound = true;
		}
	}
}

void ASHSimpleHUD::HandleHitMarker(bool bKill)
{
	LastHitMarkerTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	bLastHitWasKill = bKill;
}

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

	const ASHPlayerCharacter* Player = Cast<ASHPlayerCharacter>(GetOwningPawn());

	// --- Dynamic crosshair: gap widens with movement / suppression / firing,
	//     tightens when aiming down sights. ---
	float Gap = 5.f;
	const float Arm = 9.f;
	if (Player)
	{
		float Bloom = FMath::Min(Player->GetVelocity().Size2D() / 600.f, 1.f) * 8.f;
		Bloom += Player->GetSuppressionLevel() * 10.f;
		if (const ASHWeaponBase* CW = Player->GetEquippedWeapon())
		{
			if (CW->GetWeaponState() == ESHWeaponState::Firing) { Bloom += 6.f; }
			if (CW->IsADS()) { Bloom *= 0.35f; }
		}
		Gap += Bloom;
	}
	DrawLine(CX - Gap - Arm, CY, CX - Gap, CY, HudGreen, 1.5f);
	DrawLine(CX + Gap, CY, CX + Gap + Arm, CY, HudGreen, 1.5f);
	DrawLine(CX, CY - Gap - Arm, CX, CY - Gap, HudGreen, 1.5f);
	DrawLine(CX, CY + Gap, CX, CY + Gap + Arm, HudGreen, 1.5f);

	// --- Hit marker: brief fading "X" at center when the player lands a hit;
	//     white for a hit, red and heavier for a kill. Drawn procedurally so it
	//     needs no art assets (matches the canvas crosshair). ---
	EnsureHitMarkerBound();
	if (LastHitMarkerTime > 0.f)
	{
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		const float Age = Now - LastHitMarkerTime;
		const float MarkerDuration = bLastHitWasKill ? 0.5f : 0.35f;
		if (Age >= 0.f && Age < MarkerDuration)
		{
			const float Alpha = 1.f - (Age / MarkerDuration);
			const FLinearColor MarkerColor = bLastHitWasKill
				? FLinearColor(1.f, 0.25f, 0.2f, Alpha)
				: FLinearColor(1.f, 1.f, 1.f, Alpha);
			const float Inner = 4.f;
			const float Outer = 11.f + (1.f - Alpha) * 4.f; // expands slightly as it fades
			const float Thick = bLastHitWasKill ? 2.6f : 1.8f;
			DrawLine(CX - Outer, CY - Outer, CX - Inner, CY - Inner, MarkerColor, Thick);
			DrawLine(CX + Inner, CY - Inner, CX + Outer, CY - Outer, MarkerColor, Thick);
			DrawLine(CX - Outer, CY + Outer, CX - Inner, CY + Inner, MarkerColor, Thick);
			DrawLine(CX + Inner, CY + Inner, CX + Outer, CY + Outer, MarkerColor, Thick);
		}
	}

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

		// Fire-mode indicator under the ammo count.
		const TCHAR* ModeStr = TEXT("SEMI");
		switch (Weapon->GetCurrentFireMode())
		{
		case ESHFireMode::Burst: ModeStr = TEXT("BURST"); break;
		case ESHFireMode::Auto:  ModeStr = TEXT("AUTO");  break;
		default:                 ModeStr = TEXT("SEMI");  break;
		}
		DrawText(ModeStr, HudGreen, Canvas->ClipX - 170.f, Canvas->ClipY - 54.f, Font, 1.1f);
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
