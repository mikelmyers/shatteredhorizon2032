// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHCompassWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Slate/SlateBrushAsset.h"
#include "Fonts/SlateFontInfo.h"
#include "Framework/Application/SlateApplication.h"

DEFINE_LOG_CATEGORY(LogSH_Compass);

// ======================================================================
//  Construction
// ======================================================================

USHCompassWidget::USHCompassWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Default font — will be overridden by Blueprint
	CompassFont = FCoreStyle::GetDefaultFontStyle("Regular", 12);

	// Cardinal and intercardinal labels
	CardinalLabels.Add({ 0.f,   TEXT("N") });
	CardinalLabels.Add({ 45.f,  TEXT("NE") });
	CardinalLabels.Add({ 90.f,  TEXT("E") });
	CardinalLabels.Add({ 135.f, TEXT("SE") });
	CardinalLabels.Add({ 180.f, TEXT("S") });
	CardinalLabels.Add({ 225.f, TEXT("SW") });
	CardinalLabels.Add({ 270.f, TEXT("W") });
	CardinalLabels.Add({ 315.f, TEXT("NW") });
}

// ======================================================================
//  Widget lifecycle
// ======================================================================

void USHCompassWidget::NativeConstruct()
{
	Super::NativeConstruct();
	CurrentAlpha = MinAlpha;
	TargetAlpha = MinAlpha;
}

void USHCompassWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	// Update player bearing
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		APawn* Pawn = PC->GetPawn();
		if (Pawn)
		{
			FRotator Rot = Pawn->GetControlRotation();
			CurrentBearing = FMath::Fmod(Rot.Yaw + 360.f, 360.f);
		}
	}

	// Determine target alpha
	if (bIsADS || ForceVisibleTimer > 0.f || ContactMarkers.Num() > 0)
	{
		TargetAlpha = MaxAlpha;
	}
	else
	{
		TargetAlpha = MinAlpha;
	}

	// Interpolate alpha
	float LerpSpeed = (TargetAlpha > CurrentAlpha) ? 8.f : 2.f; // Snap in, fade out slowly
	CurrentAlpha = FMath::FInterpTo(CurrentAlpha, TargetAlpha, InDeltaTime, LerpSpeed);

	// Tick forced visibility timer
	if (ForceVisibleTimer > 0.f)
	{
		ForceVisibleTimer = FMath::Max(0.f, ForceVisibleTimer - InDeltaTime);
	}

	// Age and remove expired contact markers
	for (int32 i = ContactMarkers.Num() - 1; i >= 0; --i)
	{
		ContactMarkers[i].ElapsedTime += InDeltaTime;
		if (ContactMarkers[i].ElapsedTime >= ContactMarkers[i].Lifetime)
		{
			ContactMarkers.RemoveAt(i);
		}
	}
}

// ======================================================================
//  Paint — Slate OnPaint override for custom drawing
// ======================================================================

int32 USHCompassWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
									 const FSlateRect& MyCullingRect, FSlateClipRectangleList& OutDrawElements,
									 int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	// The compass paints itself using Slate draw elements for crisp rendering.
	// This is a Slate-level paint, not Canvas. We get a FSlateWindowElementList
	// from the parent via OutDrawElements, but since NativePaint uses a different
	// signature in UE5, we do the actual drawing in the widget's Slate representation.
	//
	// For production: this widget would use an SCompoundWidget or SLeafWidget internally
	// with full Slate OnPaint. The UMG wrapper here provides Blueprint bindability.

	return Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements,
							  LayerId, InWidgetStyle, bParentEnabled);
}

// ======================================================================
//  Contact markers
// ======================================================================

void USHCompassWidget::AddContactMarker(int32 ContactID, float BearingDegrees, float DistanceMeters,
										 bool bIsHostile, const FText& Label, float Lifetime)
{
	// Update existing
	for (FSHCompassContact& Contact : ContactMarkers)
	{
		if (Contact.ContactID == ContactID)
		{
			Contact.BearingDegrees = FMath::Fmod(BearingDegrees + 360.f, 360.f);
			Contact.DistanceMeters = DistanceMeters;
			Contact.bIsHostile = bIsHostile;
			Contact.Label = Label;
			Contact.ElapsedTime = 0.f; // Reset timer on update
			Contact.Lifetime = Lifetime;
			ForceVisibleForDuration(Lifetime);
			return;
		}
	}

	// Add new
	FSHCompassContact NewContact;
	NewContact.ContactID = (ContactID > 0) ? ContactID : NextContactID++;
	NewContact.BearingDegrees = FMath::Fmod(BearingDegrees + 360.f, 360.f);
	NewContact.DistanceMeters = DistanceMeters;
	NewContact.bIsHostile = bIsHostile;
	NewContact.Label = Label;
	NewContact.Lifetime = Lifetime;
	NewContact.ElapsedTime = 0.f;
	NewContact.ReportTimestamp = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;

	ContactMarkers.Add(NewContact);
	ForceVisibleForDuration(Lifetime);

	UE_LOG(LogSH_Compass, Log, TEXT("Contact added: ID=%d, Bearing=%.0f, Dist=%.0fm, Hostile=%s, Label=%s"),
		NewContact.ContactID, NewContact.BearingDegrees, NewContact.DistanceMeters,
		bIsHostile ? TEXT("Y") : TEXT("N"), *Label.ToString());
}

void USHCompassWidget::RemoveContactMarker(int32 ContactID)
{
	ContactMarkers.RemoveAll([ContactID](const FSHCompassContact& C) { return C.ContactID == ContactID; });
}

void USHCompassWidget::ClearAllContacts()
{
	ContactMarkers.Empty();
}

// ======================================================================
//  Objective marker
// ======================================================================

void USHCompassWidget::SetObjectiveMarker(float BearingDegrees, float DistanceMeters, const FText& Label)
{
	ObjectiveMarker.BearingDegrees = FMath::Fmod(BearingDegrees + 360.f, 360.f);
	ObjectiveMarker.DistanceMeters = DistanceMeters;
	ObjectiveMarker.Label = Label;
	ObjectiveMarker.bIsActive = true;
}

void USHCompassWidget::ClearObjectiveMarker()
{
	ObjectiveMarker.bIsActive = false;
}

// ======================================================================
//  Visibility
// ======================================================================

void USHCompassWidget::OnADSEnter()
{
	bIsADS = true;
}

void USHCompassWidget::OnADSExit()
{
	bIsADS = false;
}

void USHCompassWidget::ForceVisibleForDuration(float Seconds)
{
	ForceVisibleTimer = FMath::Max(ForceVisibleTimer, Seconds);
}

// ======================================================================
//  Bearing helpers
// ======================================================================

float USHCompassWidget::BearingToStripX(float BearingDegrees, float StripWidth) const
{
	float Delta = BearingDelta(BearingDegrees, CurrentBearing);
	float HalfArc = VisibleArcDegrees * 0.5f;

	if (FMath::Abs(Delta) > HalfArc) return -1.f; // Off-screen

	float NormalizedX = (Delta + HalfArc) / VisibleArcDegrees;
	return NormalizedX * StripWidth;
}

float USHCompassWidget::BearingDelta(float A, float B)
{
	float Delta = A - B;
	if (Delta > 180.f) Delta -= 360.f;
	if (Delta < -180.f) Delta += 360.f;
	return Delta;
}
