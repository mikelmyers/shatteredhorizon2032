// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "SHHUD.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSH_HUD, Log, All);

class USHCompassWidget;
class USHSquadCommandWidget;
class UUserWidget;

/** Player stance for HUD indicator. */
UENUM(BlueprintType)
enum class ESHPlayerStance : uint8
{
	Standing	UMETA(DisplayName = "Standing"),
	Crouching	UMETA(DisplayName = "Crouching"),
	Prone		UMETA(DisplayName = "Prone")
};

/** Body region for the injury display. */
UENUM(BlueprintType)
enum class ESHBodyRegion : uint8
{
	Head		UMETA(DisplayName = "Head"),
	Torso		UMETA(DisplayName = "Torso"),
	LeftArm		UMETA(DisplayName = "Left Arm"),
	RightArm	UMETA(DisplayName = "Right Arm"),
	LeftLeg		UMETA(DisplayName = "Left Leg"),
	RightLeg	UMETA(DisplayName = "Right Leg")
};

/** Injury state for a body region. */
USTRUCT(BlueprintType)
struct FSHInjuryState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "HUD")
	ESHBodyRegion Region = ESHBodyRegion::Torso;

	/** Health fraction for this region (0 = destroyed, 1 = healthy). */
	UPROPERTY(BlueprintReadWrite, Category = "HUD", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Health = 1.f;

	/** Whether this region is bleeding. */
	UPROPERTY(BlueprintReadWrite, Category = "HUD")
	bool bIsBleeding = false;

	/** Whether a tourniquet or bandage is applied. */
	UPROPERTY(BlueprintReadWrite, Category = "HUD")
	bool bIsTreated = false;
};

/** Squad member status for the squad panel. */
USTRUCT(BlueprintType)
struct FSHSquadMemberHUDInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "HUD")
	FText DisplayName;

	UPROPERTY(BlueprintReadWrite, Category = "HUD")
	float HealthFraction = 1.f;

	/** Distance from player in meters. */
	UPROPERTY(BlueprintReadWrite, Category = "HUD")
	float DistanceMeters = 0.f;

	/** Whether this member is alive. */
	UPROPERTY(BlueprintReadWrite, Category = "HUD")
	bool bIsAlive = true;

	/** Whether this member is downed (needs revive). */
	UPROPERTY(BlueprintReadWrite, Category = "HUD")
	bool bIsDowned = false;

	/** Current order text (e.g., "Holding", "Moving"). */
	UPROPERTY(BlueprintReadWrite, Category = "HUD")
	FText CurrentOrderText;
};

/** Radio message for the subtitle area. */
USTRUCT(BlueprintType)
struct FSHRadioMessage
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "HUD")
	FText SpeakerName;

	UPROPERTY(BlueprintReadWrite, Category = "HUD")
	FText Message;

	/** How long to display in seconds. */
	UPROPERTY(BlueprintReadWrite, Category = "HUD")
	float Duration = 4.f;

	/** Elapsed display time. */
	float ElapsedTime = 0.f;
};

/** Damage directional indicator. */
USTRUCT(BlueprintType)
struct FSHDamageIndicator
{
	GENERATED_BODY()

	/** Direction of incoming damage in degrees (0 = forward, clockwise). */
	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	float DirectionDegrees = 0.f;

	/** Intensity (0..1) — fades over time. */
	UPROPERTY(BlueprintReadOnly, Category = "HUD")
	float Intensity = 1.f;

	/** Elapsed time since this indicator was created. */
	float ElapsedTime = 0.f;

	/** Duration before fade-out completes. */
	float FadeDuration = 2.f;
};

/**
 * ASHHUD
 *
 * Military-authentic heads-up display. Minimal, functional, no gamey elements.
 * No floating health bars, no minimap by default, no hit markers (optional).
 * Can be toggled to full immersive mode (no HUD at all).
 *
 * Rendering is done via a UMG root widget owned by this HUD class. Canvas
 * draw calls are used only for the suppression vignette and damage indicators
 * which need full-screen post-process-like behaviour.
 */
UCLASS()
class SHATTEREDHORIZON2032_API ASHHUD : public AHUD
{
	GENERATED_BODY()

public:
	ASHHUD();

	// ------------------------------------------------------------------
	//  AHUD overrides
	// ------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void DrawHUD() override;

	// ------------------------------------------------------------------
	//  HUD mode
	// ------------------------------------------------------------------

	/** Toggle the entire HUD on/off for immersive mode. */
	UFUNCTION(BlueprintCallable, Category = "SH|HUD")
	void ToggleImmersiveMode();

	UFUNCTION(BlueprintPure, Category = "SH|HUD")
	bool IsImmersiveMode() const { return bImmersiveMode; }

	// ------------------------------------------------------------------
	//  Ammo display
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "SH|HUD")
	void UpdateAmmoDisplay(int32 CurrentMagazine, int32 MagazineCapacity, int32 ReserveAmmo);

	// ------------------------------------------------------------------
	//  Stance
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "SH|HUD")
	void UpdateStance(ESHPlayerStance NewStance);

	// ------------------------------------------------------------------
	//  Health / injury
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "SH|HUD")
	void UpdateInjuryState(const TArray<FSHInjuryState>& Injuries);

	// ------------------------------------------------------------------
	//  Squad status
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "SH|HUD")
	void UpdateSquadStatus(const TArray<FSHSquadMemberHUDInfo>& SquadMembers);

	// ------------------------------------------------------------------
	//  Radio messages / subtitles
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "SH|HUD")
	void ShowRadioMessage(const FText& Speaker, const FText& Message, float Duration = 4.f);

	// ------------------------------------------------------------------
	//  Suppression
	// ------------------------------------------------------------------

	/** Set suppression level (0 = none, 1 = max). Drives vignette intensity. */
	UFUNCTION(BlueprintCallable, Category = "SH|HUD")
	void SetSuppressionLevel(float Level);

	UFUNCTION(BlueprintPure, Category = "SH|HUD")
	float GetSuppressionLevel() const { return SuppressionLevel; }

	// ------------------------------------------------------------------
	//  Damage indicator
	// ------------------------------------------------------------------

	/** Add a directional damage indicator. Direction in world-space. */
	UFUNCTION(BlueprintCallable, Category = "SH|HUD")
	void AddDamageIndicator(FVector DamageWorldDirection, float Intensity = 1.f);

	// ------------------------------------------------------------------
	//  Objective
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "SH|HUD")
	void SetObjectiveInfo(const FText& ObjectiveName, float BearingDegrees, float DistanceMeters);

	UFUNCTION(BlueprintCallable, Category = "SH|HUD")
	void ClearObjective();

	// ------------------------------------------------------------------
	//  Settings
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "SH|HUD")
	void SetHitMarkersEnabled(bool bEnabled) { bShowHitMarkers = bEnabled; }

	UFUNCTION(BlueprintCallable, Category = "SH|HUD")
	void ShowHitMarker();

	// ------------------------------------------------------------------
	//  Widget access
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|HUD")
	USHCompassWidget* GetCompassWidget() const { return CompassWidget; }

	UFUNCTION(BlueprintPure, Category = "SH|HUD")
	USHSquadCommandWidget* GetSquadCommandWidget() const { return SquadCommandWidget; }

protected:
	// ------------------------------------------------------------------
	//  Canvas draw helpers (suppression, damage indicators)
	// ------------------------------------------------------------------
	void DrawSuppressionVignette();
	void DrawDamageIndicators();
	void DrawHitMarker();
	void TickRadioMessages(float DeltaSeconds);
	void TickDamageIndicators(float DeltaSeconds);

	// ------------------------------------------------------------------
	//  Widget classes (set in Blueprint)
	// ------------------------------------------------------------------

	/** Root UMG widget class for the entire HUD layout. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|HUD|Widgets")
	TSubclassOf<UUserWidget> HUDWidgetClass;

	/** Compass widget class. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|HUD|Widgets")
	TSubclassOf<USHCompassWidget> CompassWidgetClass;

	/** Squad command radial widget class. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|HUD|Widgets")
	TSubclassOf<USHSquadCommandWidget> SquadCommandWidgetClass;

	// ------------------------------------------------------------------
	//  Visual config
	// ------------------------------------------------------------------

	/** Suppression vignette color (usually dark red/black). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|HUD|Suppression")
	FLinearColor SuppressionVignetteColor = FLinearColor(0.f, 0.f, 0.f, 0.7f);

	/** Suppression vignette inner radius fraction (0..1 of screen). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|HUD|Suppression")
	float SuppressionVignetteInnerRadius = 0.4f;

	/** Damage indicator texture. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|HUD|DamageIndicator")
	TObjectPtr<UTexture2D> DamageIndicatorTexture;

	/** Damage indicator display radius from center (fraction of screen). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|HUD|DamageIndicator")
	float DamageIndicatorRadius = 0.25f;

	/** Damage indicator fade time in seconds. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|HUD|DamageIndicator")
	float DamageIndicatorFadeTime = 2.f;

	/** Hit marker display duration. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|HUD|HitMarker")
	float HitMarkerDuration = 0.15f;

	/** HUD color — military green tint. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|HUD|Style")
	FLinearColor HUDColor = FLinearColor(0.6f, 0.8f, 0.6f, 0.85f);

	/** HUD warning color. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|HUD|Style")
	FLinearColor HUDWarningColor = FLinearColor(0.9f, 0.7f, 0.2f, 0.9f);

	/** HUD critical color. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|HUD|Style")
	FLinearColor HUDCriticalColor = FLinearColor(0.9f, 0.2f, 0.2f, 0.9f);

private:
	// ------------------------------------------------------------------
	//  Owned widgets
	// ------------------------------------------------------------------

	UPROPERTY()
	TObjectPtr<UUserWidget> HUDRootWidget;

	UPROPERTY()
	TObjectPtr<USHCompassWidget> CompassWidget;

	UPROPERTY()
	TObjectPtr<USHSquadCommandWidget> SquadCommandWidget;

	// ------------------------------------------------------------------
	//  Runtime state
	// ------------------------------------------------------------------

	bool bImmersiveMode = false;

	// Ammo
	int32 CurrentMagAmmo = 0;
	int32 MagCapacity = 0;
	int32 ReserveAmmo = 0;

	// Stance
	ESHPlayerStance CurrentStance = ESHPlayerStance::Standing;

	// Injuries
	TArray<FSHInjuryState> CurrentInjuries;

	// Squad
	TArray<FSHSquadMemberHUDInfo> SquadMembers;

	// Radio messages
	TArray<FSHRadioMessage> ActiveRadioMessages;
	static constexpr int32 MaxRadioMessages = 4;

	// Suppression
	float SuppressionLevel = 0.f;
	float TargetSuppressionLevel = 0.f;

	// Damage indicators
	TArray<FSHDamageIndicator> DamageIndicators;

	// Objective
	FText CurrentObjectiveName;
	float ObjectiveBearing = 0.f;
	float ObjectiveDistance = 0.f;
	bool bHasObjective = false;

	// Hit marker
	bool bShowHitMarkers = false; // Off by default — milsim
	float HitMarkerTimer = 0.f;

	// Suppression vignette interpolation
	static constexpr float SuppressionLerpSpeed = 4.f;
};
