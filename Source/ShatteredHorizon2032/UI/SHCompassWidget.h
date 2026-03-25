// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SHCompassWidget.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSH_Compass, Log, All);

/** A contact marker displayed on the compass strip. */
USTRUCT(BlueprintType)
struct FSHCompassContact
{
	GENERATED_BODY()

	/** Unique ID for this contact (for updates/removal). */
	UPROPERTY(BlueprintReadWrite, Category = "Compass")
	int32 ContactID = 0;

	/** Compass bearing in degrees (0-360, 0 = North). */
	UPROPERTY(BlueprintReadWrite, Category = "Compass")
	float BearingDegrees = 0.f;

	/** Distance to contact in meters. */
	UPROPERTY(BlueprintReadWrite, Category = "Compass")
	float DistanceMeters = 0.f;

	/** Whether this is a hostile contact. */
	UPROPERTY(BlueprintReadWrite, Category = "Compass")
	bool bIsHostile = true;

	/** Display label (e.g., "INF x3"). */
	UPROPERTY(BlueprintReadWrite, Category = "Compass")
	FText Label;

	/** Time when this contact was reported. */
	UPROPERTY(BlueprintReadWrite, Category = "Compass")
	float ReportTimestamp = 0.f;

	/** How long this marker stays visible (seconds). */
	UPROPERTY(BlueprintReadWrite, Category = "Compass")
	float Lifetime = 15.f;

	/** Elapsed time. */
	float ElapsedTime = 0.f;
};

/** Objective marker on compass. */
USTRUCT(BlueprintType)
struct FSHCompassObjective
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Compass")
	float BearingDegrees = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Compass")
	float DistanceMeters = 0.f;

	UPROPERTY(BlueprintReadWrite, Category = "Compass")
	FText Label;

	UPROPERTY(BlueprintReadWrite, Category = "Compass")
	bool bIsActive = false;
};

/**
 * USHCompassWidget
 *
 * Military compass bearing strip rendered at the top of the screen.
 * Shows cardinal/intercardinal directions, degree markings every 10 degrees,
 * contact markers from squad callouts, and an objective direction indicator.
 * Fades when not in use; appears on ADS or when contacts are reported.
 */
UCLASS()
class SHATTEREDHORIZON2032_API USHCompassWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USHCompassWidget(const FObjectInitializer& ObjectInitializer);

	// ------------------------------------------------------------------
	//  UUserWidget overrides
	// ------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
							  const FSlateRect& MyCullingRect, FSlateClipRectangleList& OutDrawElements,
							  int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

	// ------------------------------------------------------------------
	//  Contact markers
	// ------------------------------------------------------------------

	/** Add or update a contact marker on the compass. */
	UFUNCTION(BlueprintCallable, Category = "SH|Compass")
	void AddContactMarker(int32 ContactID, float BearingDegrees, float DistanceMeters, bool bIsHostile, const FText& Label, float Lifetime = 15.f);

	/** Remove a specific contact marker. */
	UFUNCTION(BlueprintCallable, Category = "SH|Compass")
	void RemoveContactMarker(int32 ContactID);

	/** Remove all contact markers. */
	UFUNCTION(BlueprintCallable, Category = "SH|Compass")
	void ClearAllContacts();

	// ------------------------------------------------------------------
	//  Objective marker
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "SH|Compass")
	void SetObjectiveMarker(float BearingDegrees, float DistanceMeters, const FText& Label);

	UFUNCTION(BlueprintCallable, Category = "SH|Compass")
	void ClearObjectiveMarker();

	// ------------------------------------------------------------------
	//  Visibility control
	// ------------------------------------------------------------------

	/** Call when player enters ADS — compass becomes fully visible. */
	UFUNCTION(BlueprintCallable, Category = "SH|Compass")
	void OnADSEnter();

	/** Call when player exits ADS — compass starts fading. */
	UFUNCTION(BlueprintCallable, Category = "SH|Compass")
	void OnADSExit();

	/** Force compass visible for a duration (e.g., contact reported). */
	UFUNCTION(BlueprintCallable, Category = "SH|Compass")
	void ForceVisibleForDuration(float Seconds);

	/** Get current player bearing in degrees. */
	UFUNCTION(BlueprintPure, Category = "SH|Compass")
	float GetCurrentBearing() const { return CurrentBearing; }

protected:
	// ------------------------------------------------------------------
	//  Paint helpers
	// ------------------------------------------------------------------
	void PaintCompassStrip(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	void PaintBearingMarks(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	void PaintContactMarkers(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;
	void PaintObjectiveMarker(const FGeometry& Geometry, FSlateWindowElementList& OutDrawElements, int32 LayerId) const;

	/** Convert a world bearing to X position on the compass strip. Returns < 0 if off-screen. */
	float BearingToStripX(float BearingDegrees, float StripWidth) const;

	/** Shortest angular distance between two bearings. */
	static float BearingDelta(float A, float B);

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Degrees of compass visible on screen at once. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Compass|Config")
	float VisibleArcDegrees = 120.f;

	/** Compass strip height in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Compass|Config")
	float StripHeight = 32.f;

	/** Interval for degree tick marks. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Compass|Config")
	float DegreeTickInterval = 10.f;

	/** Height of minor tick marks (pixels). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Compass|Config")
	float MinorTickHeight = 8.f;

	/** Height of major tick marks (cardinal/intercardinal). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Compass|Config")
	float MajorTickHeight = 16.f;

	/** Time to fade out when idle (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Compass|Config")
	float FadeOutTime = 3.f;

	/** Minimum alpha when faded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Compass|Config")
	float MinAlpha = 0.15f;

	/** Full alpha when active. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Compass|Config")
	float MaxAlpha = 0.9f;

	/** Color for compass markings. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Compass|Style")
	FLinearColor CompassColor = FLinearColor(0.8f, 0.9f, 0.8f, 1.f);

	/** Color for hostile contact markers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Compass|Style")
	FLinearColor HostileContactColor = FLinearColor(1.f, 0.2f, 0.2f, 1.f);

	/** Color for friendly contact markers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Compass|Style")
	FLinearColor FriendlyContactColor = FLinearColor(0.2f, 0.6f, 1.f, 1.f);

	/** Color for objective marker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Compass|Style")
	FLinearColor ObjectiveColor = FLinearColor(1.f, 0.8f, 0.2f, 1.f);

	/** Font for compass labels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Compass|Style")
	FSlateFontInfo CompassFont;

private:
	// ------------------------------------------------------------------
	//  Runtime state
	// ------------------------------------------------------------------

	/** Current player facing bearing (0-360). */
	float CurrentBearing = 0.f;

	/** Current compass opacity. */
	float CurrentAlpha = 0.15f;
	float TargetAlpha = 0.15f;

	/** Active contact markers. */
	UPROPERTY()
	TArray<FSHCompassContact> ContactMarkers;

	/** Objective marker. */
	FSHCompassObjective ObjectiveMarker;

	/** Is the player currently in ADS. */
	bool bIsADS = false;

	/** Forced visibility timer. */
	float ForceVisibleTimer = 0.f;

	/** Next contact ID counter. */
	int32 NextContactID = 1;

	/** Cardinal direction labels. */
	struct FCardinalLabel
	{
		float Bearing;
		FString Label;
	};
	TArray<FCardinalLabel> CardinalLabels;
};
