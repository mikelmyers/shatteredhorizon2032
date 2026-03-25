// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHHUD.h"
#include "SHCompassWidget.h"
#include "SHSquadCommandWidget.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Blueprint/UserWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY(LogSH_HUD);

// ======================================================================
//  Construction
// ======================================================================

ASHHUD::ASHHUD()
{
	PrimaryActorTick.bCanEverTick = true;
}

// ======================================================================
//  BeginPlay — create UMG widgets
// ======================================================================

void ASHHUD::BeginPlay()
{
	Super::BeginPlay();

	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;

	// Create root HUD widget
	if (HUDWidgetClass)
	{
		HUDRootWidget = CreateWidget<UUserWidget>(PC, HUDWidgetClass);
		if (HUDRootWidget)
		{
			HUDRootWidget->AddToViewport(0);
		}
	}

	// Create compass widget
	if (CompassWidgetClass)
	{
		CompassWidget = CreateWidget<USHCompassWidget>(PC, CompassWidgetClass);
		if (CompassWidget)
		{
			CompassWidget->AddToViewport(10);
		}
	}

	// Create squad command widget (hidden by default)
	if (SquadCommandWidgetClass)
	{
		SquadCommandWidget = CreateWidget<USHSquadCommandWidget>(PC, SquadCommandWidgetClass);
		if (SquadCommandWidget)
		{
			SquadCommandWidget->AddToViewport(20);
			SquadCommandWidget->SetVisibility(ESlateVisibility::Collapsed);
		}
	}

	// Initialise injury array with all body regions
	CurrentInjuries.Empty();
	for (uint8 i = 0; i <= static_cast<uint8>(ESHBodyRegion::RightLeg); ++i)
	{
		FSHInjuryState Injury;
		Injury.Region = static_cast<ESHBodyRegion>(i);
		Injury.Health = 1.f;
		CurrentInjuries.Add(Injury);
	}

	UE_LOG(LogSH_HUD, Log, TEXT("HUD initialised. Immersive=%s, HitMarkers=%s"),
		bImmersiveMode ? TEXT("ON") : TEXT("OFF"),
		bShowHitMarkers ? TEXT("ON") : TEXT("OFF"));
}

// ======================================================================
//  Tick
// ======================================================================

void ASHHUD::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Smooth suppression interpolation
	SuppressionLevel = FMath::FInterpTo(SuppressionLevel, TargetSuppressionLevel, DeltaSeconds, SuppressionLerpSpeed);

	TickRadioMessages(DeltaSeconds);
	TickDamageIndicators(DeltaSeconds);

	// Hit marker fade
	if (HitMarkerTimer > 0.f)
	{
		HitMarkerTimer = FMath::Max(0.f, HitMarkerTimer - DeltaSeconds);
	}
}

// ======================================================================
//  DrawHUD — canvas rendering for full-screen overlays
// ======================================================================

void ASHHUD::DrawHUD()
{
	Super::DrawHUD();

	if (bImmersiveMode) return;

	DrawSuppressionVignette();
	DrawDamageIndicators();

	if (bShowHitMarkers && HitMarkerTimer > 0.f)
	{
		DrawHitMarker();
	}
}

// ======================================================================
//  Suppression vignette
// ======================================================================

void ASHHUD::DrawSuppressionVignette()
{
	if (SuppressionLevel < 0.01f) return;
	if (!Canvas) return;

	float ScreenW = Canvas->SizeX;
	float ScreenH = Canvas->SizeY;
	float CenterX = ScreenW * 0.5f;
	float CenterY = ScreenH * 0.5f;

	// Draw radial gradient vignette using concentric rectangles
	// More suppression = tighter vignette, darker edges
	int32 Steps = 16;
	float MaxAlpha = SuppressionVignetteColor.A * SuppressionLevel;

	for (int32 i = Steps; i >= 0; --i)
	{
		float Fraction = static_cast<float>(i) / Steps;
		float InnerFrac = SuppressionVignetteInnerRadius + (1.f - SuppressionVignetteInnerRadius) * Fraction;

		// Only draw outside the inner radius
		if (InnerFrac < SuppressionVignetteInnerRadius) continue;

		float Alpha = MaxAlpha * (1.f - Fraction);
		FLinearColor StepColor = SuppressionVignetteColor;
		StepColor.A = Alpha;

		float RectW = ScreenW * InnerFrac;
		float RectH = ScreenH * InnerFrac;
		float X = CenterX - RectW * 0.5f;
		float Y = CenterY - RectH * 0.5f;

		// Draw border strips (top, bottom, left, right)
		float BorderSize = (ScreenW - RectW) * 0.5f;

		FCanvasTileItem TopTile(FVector2D(0, 0), FVector2D(ScreenW, Y), StepColor);
		TopTile.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(TopTile);

		FCanvasTileItem BottomTile(FVector2D(0, Y + RectH), FVector2D(ScreenW, ScreenH - Y - RectH), StepColor);
		BottomTile.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(BottomTile);

		FCanvasTileItem LeftTile(FVector2D(0, Y), FVector2D(X, RectH), StepColor);
		LeftTile.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(LeftTile);

		FCanvasTileItem RightTile(FVector2D(X + RectW, Y), FVector2D(ScreenW - X - RectW, RectH), StepColor);
		RightTile.BlendMode = SE_BLEND_Translucent;
		Canvas->DrawItem(RightTile);

		break; // Simplified: one pass with edge alpha. In production, use a material-based vignette.
	}
}

// ======================================================================
//  Damage directional indicators
// ======================================================================

void ASHHUD::DrawDamageIndicators()
{
	if (DamageIndicators.Num() == 0) return;
	if (!Canvas) return;

	float ScreenW = Canvas->SizeX;
	float ScreenH = Canvas->SizeY;
	float CenterX = ScreenW * 0.5f;
	float CenterY = ScreenH * 0.5f;
	float IndicatorDist = FMath::Min(ScreenW, ScreenH) * DamageIndicatorRadius;

	for (const FSHDamageIndicator& Indicator : DamageIndicators)
	{
		float AngleRad = FMath::DegreesToRadians(Indicator.DirectionDegrees - 90.f); // -90 to orient 0=up

		float PosX = CenterX + FMath::Cos(AngleRad) * IndicatorDist;
		float PosY = CenterY + FMath::Sin(AngleRad) * IndicatorDist;

		FLinearColor IndicatorColor = HUDCriticalColor;
		IndicatorColor.A = Indicator.Intensity;

		if (DamageIndicatorTexture)
		{
			FCanvasTileItem TileItem(
				FVector2D(PosX - 16.f, PosY - 32.f),
				FVector2D(32.f, 64.f),
				DamageIndicatorTexture,
				IndicatorColor);
			TileItem.BlendMode = SE_BLEND_Translucent;
			TileItem.Rotation = FRotator(0, Indicator.DirectionDegrees, 0);
			TileItem.PivotPoint = FVector2D(0.5f, 0.5f);
			Canvas->DrawItem(TileItem);
		}
		else
		{
			// Fallback: draw a simple arrow shape
			float ArrowLen = 40.f;
			float EndX = PosX + FMath::Cos(AngleRad) * ArrowLen;
			float EndY = PosY + FMath::Sin(AngleRad) * ArrowLen;

			FCanvasLineItem Line(FVector2D(PosX, PosY), FVector2D(EndX, EndY));
			Line.SetColor(IndicatorColor);
			Line.LineThickness = 3.f;
			Canvas->DrawItem(Line);
		}
	}
}

// ======================================================================
//  Hit marker
// ======================================================================

void ASHHUD::DrawHitMarker()
{
	if (!Canvas) return;

	float ScreenW = Canvas->SizeX;
	float ScreenH = Canvas->SizeY;
	float CenterX = ScreenW * 0.5f;
	float CenterY = ScreenH * 0.5f;

	float Alpha = FMath::Clamp(HitMarkerTimer / HitMarkerDuration, 0.f, 1.f);
	FLinearColor MarkerColor(1.f, 1.f, 1.f, Alpha);

	float Size = 8.f;
	float Gap = 3.f;

	// Four diagonal lines forming an X
	auto DrawLine = [&](float X1, float Y1, float X2, float Y2)
	{
		FCanvasLineItem Line(FVector2D(CenterX + X1, CenterY + Y1), FVector2D(CenterX + X2, CenterY + Y2));
		Line.SetColor(MarkerColor);
		Line.LineThickness = 2.f;
		Canvas->DrawItem(Line);
	};

	DrawLine(-Size, -Size, -Gap, -Gap);
	DrawLine(Gap, -Gap, Size, -Size);
	DrawLine(-Size, Size, -Gap, Gap);
	DrawLine(Gap, Gap, Size, Size);
}

// ======================================================================
//  HUD mode toggle
// ======================================================================

void ASHHUD::ToggleImmersiveMode()
{
	bImmersiveMode = !bImmersiveMode;

	ESlateVisibility Vis = bImmersiveMode ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible;

	if (HUDRootWidget) HUDRootWidget->SetVisibility(Vis);
	if (CompassWidget) CompassWidget->SetVisibility(Vis);

	UE_LOG(LogSH_HUD, Log, TEXT("Immersive mode: %s"), bImmersiveMode ? TEXT("ON") : TEXT("OFF"));
}

// ======================================================================
//  Data update methods
// ======================================================================

void ASHHUD::UpdateAmmoDisplay(int32 InCurrentMagazine, int32 InMagazineCapacity, int32 InReserveAmmo)
{
	CurrentMagAmmo = InCurrentMagazine;
	MagCapacity = InMagazineCapacity;
	ReserveAmmo = InReserveAmmo;

	// In production, push to UMG bindings via property binding or event dispatch.
	// NOTE: Reserve ammo is NOT shown as exact count on HUD. Use GetReserveAmmoEstimate().
}

FText ASHHUD::GetReserveAmmoEstimate() const
{
	// Doctrine: no magical ammo counter. Player estimates reserve by feel.
	// Display a rough categorical estimate, not an exact number.
	if (MagCapacity <= 0) return FText::FromString(TEXT("---"));

	const int32 FullLoadMags = 7; // Typical combat load: 7 magazines
	const float MagsRemaining = static_cast<float>(ReserveAmmo) / FMath::Max(1, MagCapacity);

	if (MagsRemaining >= FullLoadMags * 0.75f)
		return FText::FromString(TEXT("FULL"));
	else if (MagsRemaining >= FullLoadMags * 0.5f)
		return FText::FromString(TEXT("GOOD"));
	else if (MagsRemaining >= FullLoadMags * 0.25f)
		return FText::FromString(TEXT("LOW"));
	else if (ReserveAmmo > 0)
		return FText::FromString(TEXT("LAST MAG"));
	else
		return FText::FromString(TEXT("DRY"));
}

void ASHHUD::UpdateStance(ESHPlayerStance NewStance)
{
	CurrentStance = NewStance;
}

void ASHHUD::UpdateInjuryState(const TArray<FSHInjuryState>& Injuries)
{
	CurrentInjuries = Injuries;
}

void ASHHUD::UpdateSquadStatus(const TArray<FSHSquadMemberHUDInfo>& InSquadMembers)
{
	SquadMembers = InSquadMembers;
}

void ASHHUD::ShowRadioMessage(const FText& Speaker, const FText& Message, float Duration)
{
	// Enforce max queue size
	while (ActiveRadioMessages.Num() >= MaxRadioMessages)
	{
		ActiveRadioMessages.RemoveAt(0);
	}

	FSHRadioMessage Msg;
	Msg.SpeakerName = Speaker;
	Msg.Message = Message;
	Msg.Duration = Duration;
	Msg.ElapsedTime = 0.f;
	ActiveRadioMessages.Add(Msg);

	UE_LOG(LogSH_HUD, Log, TEXT("Radio: [%s] %s"), *Speaker.ToString(), *Message.ToString());
}

void ASHHUD::SetSuppressionLevel(float Level)
{
	TargetSuppressionLevel = FMath::Clamp(Level, 0.f, 1.f);
}

void ASHHUD::AddDamageIndicator(FVector DamageWorldDirection, float Intensity)
{
	APlayerController* PC = GetOwningPlayerController();
	if (!PC) return;

	APawn* Pawn = PC->GetPawn();
	if (!Pawn) return;

	// Convert world direction to screen-relative angle
	FVector Forward = Pawn->GetActorForwardVector();
	FVector Right = Pawn->GetActorRightVector();

	FVector DamageDir2D = DamageWorldDirection.GetSafeNormal2D();
	float DotForward = FVector::DotProduct(Forward.GetSafeNormal2D(), DamageDir2D);
	float DotRight = FVector::DotProduct(Right.GetSafeNormal2D(), DamageDir2D);

	float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(DotRight, DotForward));
	if (AngleDeg < 0.f) AngleDeg += 360.f;

	FSHDamageIndicator Indicator;
	Indicator.DirectionDegrees = AngleDeg;
	Indicator.Intensity = FMath::Clamp(Intensity, 0.f, 1.f);
	Indicator.ElapsedTime = 0.f;
	Indicator.FadeDuration = DamageIndicatorFadeTime;

	DamageIndicators.Add(Indicator);
}

void ASHHUD::SetObjectiveInfo(const FText& ObjectiveName, float BearingDegrees, float DistanceMeters)
{
	CurrentObjectiveName = ObjectiveName;
	ObjectiveBearing = BearingDegrees;
	ObjectiveDistance = DistanceMeters;
	bHasObjective = true;

	// Forward to compass widget
	if (CompassWidget)
	{
		CompassWidget->SetObjectiveMarker(BearingDegrees, DistanceMeters, ObjectiveName);
	}
}

void ASHHUD::ClearObjective()
{
	bHasObjective = false;
	if (CompassWidget)
	{
		CompassWidget->ClearObjectiveMarker();
	}
}

void ASHHUD::ShowHitMarker()
{
	if (bShowHitMarkers)
	{
		HitMarkerTimer = HitMarkerDuration;
	}
}

// ======================================================================
//  Tick helpers
// ======================================================================

void ASHHUD::TickRadioMessages(float DeltaSeconds)
{
	for (int32 i = ActiveRadioMessages.Num() - 1; i >= 0; --i)
	{
		ActiveRadioMessages[i].ElapsedTime += DeltaSeconds;
		if (ActiveRadioMessages[i].ElapsedTime >= ActiveRadioMessages[i].Duration)
		{
			ActiveRadioMessages.RemoveAt(i);
		}
	}
}

void ASHHUD::TickDamageIndicators(float DeltaSeconds)
{
	for (int32 i = DamageIndicators.Num() - 1; i >= 0; --i)
	{
		FSHDamageIndicator& Indicator = DamageIndicators[i];
		Indicator.ElapsedTime += DeltaSeconds;

		// Fade out
		float FadeAlpha = 1.f - FMath::Clamp(Indicator.ElapsedTime / Indicator.FadeDuration, 0.f, 1.f);
		Indicator.Intensity *= FadeAlpha > 0.f ? FadeAlpha : 0.f;

		if (Indicator.ElapsedTime >= Indicator.FadeDuration)
		{
			DamageIndicators.RemoveAt(i);
		}
	}
}
