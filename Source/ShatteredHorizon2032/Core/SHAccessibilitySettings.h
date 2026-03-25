// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SHAccessibilitySettings.generated.h"

class UMaterialParameterCollection;
class UMaterialParameterCollectionInstance;

/* -----------------------------------------------------------------------
 *  Enums
 * --------------------------------------------------------------------- */

UENUM(BlueprintType)
enum class ESHColorblindMode : uint8
{
	Off          UMETA(DisplayName = "Off"),
	Protanopia   UMETA(DisplayName = "Protanopia"),
	Deuteranopia UMETA(DisplayName = "Deuteranopia"),
	Tritanopia   UMETA(DisplayName = "Tritanopia")
};

UENUM(BlueprintType)
enum class ESHClosedCaptionType : uint8
{
	Gunfire    UMETA(DisplayName = "Gunfire"),
	Explosion  UMETA(DisplayName = "Explosion"),
	Voice      UMETA(DisplayName = "Voice"),
	Movement   UMETA(DisplayName = "Movement"),
	Vehicle    UMETA(DisplayName = "Vehicle")
};

/* -----------------------------------------------------------------------
 *  Structs
 * --------------------------------------------------------------------- */

USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHClosedCaption
{
	GENERATED_BODY()

	/** Caption text to display. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Caption")
	FText Text;

	/** World-space direction the sound originated from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Caption")
	FVector Direction = FVector::ZeroVector;

	/** How long the caption should remain on screen (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Caption", meta = (ClampMin = "0"))
	float Duration = 3.0f;

	/** Type of combat audio this caption represents. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Caption")
	ESHClosedCaptionType CaptionType = ESHClosedCaptionType::Gunfire;
};

/* -----------------------------------------------------------------------
 *  Delegates
 * --------------------------------------------------------------------- */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnClosedCaptionTriggered, const FSHClosedCaption&, Caption);

/* -----------------------------------------------------------------------
 *  USHAccessibilitySettings
 * --------------------------------------------------------------------- */

/**
 * USHAccessibilitySettings
 *
 * Game instance subsystem managing all accessibility-related settings.
 * Provides colorblind correction, UI scaling, subtitle sizing,
 * closed captions for combat audio, and controller aim assist.
 */
UCLASS()
class SHATTEREDHORIZON2032_API USHAccessibilitySettings : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ------------------------------------------------------------------
	//  USubsystem overrides
	// ------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ------------------------------------------------------------------
	//  Colorblind
	// ------------------------------------------------------------------

	/** Set the colorblind correction mode. Applies a post-process material parameter. */
	UFUNCTION(BlueprintCallable, Category = "SH|Accessibility")
	void SetColorblindMode(ESHColorblindMode InMode);

	UFUNCTION(BlueprintPure, Category = "SH|Accessibility")
	ESHColorblindMode GetColorblindMode() const { return ColorblindMode; }

	// ------------------------------------------------------------------
	//  UI Scale
	// ------------------------------------------------------------------

	/** Set UI scale factor. Clamped to [0.75, 1.5]. */
	UFUNCTION(BlueprintCallable, Category = "SH|Accessibility")
	void SetUIScale(float Scale);

	UFUNCTION(BlueprintPure, Category = "SH|Accessibility")
	float GetUIScale() const { return UIScale; }

	// ------------------------------------------------------------------
	//  Subtitles
	// ------------------------------------------------------------------

	/** Set subtitle size multiplier. Clamped to [0.5, 2.0]. */
	UFUNCTION(BlueprintCallable, Category = "SH|Accessibility")
	void SetSubtitleSize(float Size);

	UFUNCTION(BlueprintPure, Category = "SH|Accessibility")
	float GetSubtitleSize() const { return SubtitleSize; }

	// ------------------------------------------------------------------
	//  Closed Captions
	// ------------------------------------------------------------------

	/**
	 * Enable or disable closed captions for combat audio.
	 * When enabled, combat sounds (gunfire direction, explosion proximity)
	 * are represented as on-screen text with directional indicators.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Accessibility")
	void SetClosedCaptions(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "SH|Accessibility")
	bool GetClosedCaptions() const { return bClosedCaptionsEnabled; }

	/** Trigger a closed caption. Only fires delegate if captions are enabled. */
	UFUNCTION(BlueprintCallable, Category = "SH|Accessibility")
	void TriggerClosedCaption(const FSHClosedCaption& Caption);

	// ------------------------------------------------------------------
	//  Aim Assist
	// ------------------------------------------------------------------

	/**
	 * Enable or disable controller aim assist.
	 * Provides subtle milsim-appropriate aim slowdown near targets.
	 * Only active when using a gamepad.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Accessibility")
	void SetControllerAimAssist(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "SH|Accessibility")
	bool GetControllerAimAssist() const { return bAimAssistEnabled; }

	// ------------------------------------------------------------------
	//  Bulk apply
	// ------------------------------------------------------------------

	/** Read all accessibility values from saved settings and apply them. */
	UFUNCTION(BlueprintCallable, Category = "SH|Accessibility")
	void ApplyAllSettings();

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Accessibility")
	FOnClosedCaptionTriggered OnClosedCaptionTriggered;

protected:
	/** Material parameter collection used for colorblind post-process. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Accessibility|Config")
	TSoftObjectPtr<UMaterialParameterCollection> ColorblindMPC;

	/** Aim assist slowdown factor applied when crosshair is near a target. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Accessibility|Config", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AimAssistSlowdownFactor = 0.4f;

	/** Radius in screen-space (0-1) around the crosshair where aim assist activates. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Accessibility|Config", meta = (ClampMin = "0.0", ClampMax = "0.2"))
	float AimAssistRadius = 0.05f;

private:
	void ApplyColorblindMaterialParams();
	void SaveSettingsToConfig();
	void LoadSettingsFromConfig();

	UPROPERTY()
	ESHColorblindMode ColorblindMode = ESHColorblindMode::Off;

	float UIScale = 1.0f;
	float SubtitleSize = 1.0f;
	bool bClosedCaptionsEnabled = false;
	bool bAimAssistEnabled = false;
};
