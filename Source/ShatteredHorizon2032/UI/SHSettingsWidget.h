// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CommonActivatableWidget.h"
#include "SHSettingsWidget.generated.h"

/** Settings tab categories. */
UENUM(BlueprintType)
enum class ESHSettingsTab : uint8
{
	Video		UMETA(DisplayName = "Video"),
	Audio		UMETA(DisplayName = "Audio"),
	Controls	UMETA(DisplayName = "Controls"),
	Gameplay	UMETA(DisplayName = "Gameplay"),
	Accessibility UMETA(DisplayName = "Accessibility")
};

/** Video quality preset. */
UENUM(BlueprintType)
enum class ESHVideoPreset : uint8
{
	Low		UMETA(DisplayName = "Low"),
	Medium	UMETA(DisplayName = "Medium"),
	High	UMETA(DisplayName = "High"),
	Ultra	UMETA(DisplayName = "Ultra"),
	Custom	UMETA(DisplayName = "Custom")
};

/** Persistent settings data. */
USTRUCT(BlueprintType)
struct FSHGameSettings
{
	GENERATED_BODY()

	// --- Video ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite) ESHVideoPreset VideoPreset = ESHVideoPreset::High;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) FIntPoint Resolution = FIntPoint(1920, 1080);
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bFullscreen = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bVSync = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 FrameRateLimit = 0; // 0 = unlimited
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float RenderScale = 100.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRayTracing = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float FOV = 90.f;

	// --- Audio ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MasterVolume = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SFXVolume = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MusicVolume = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float VoiceVolume = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AmbientVolume = 0.8f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bSubtitles = true;

	// --- Controls ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float MouseSensitivity = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float AimSensitivity = 0.5f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bInvertY = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float ControllerDeadzone = 0.15f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bToggleADS = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bToggleSprint = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bToggleCrouch = true;

	// --- Gameplay ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 DifficultyLevel = 2; // 0=Recruit .. 4=Primordia Unleashed
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bShowHitIndicator = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float HeadBobIntensity = 1.f;

	// --- Accessibility ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 ColorblindMode = 0; // 0=Off, 1=Protanopia, 2=Deuteranopia, 3=Tritanopia
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float UIScale = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bClosedCaptions = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) float SubtitleSize = 1.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bControllerAimAssist = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnSettingsApplied, const FSHGameSettings&, NewSettings);

/**
 * USHSettingsWidget
 *
 * Tabbed settings screen with Video, Audio, Controls, Gameplay,
 * and Accessibility panels. Persists to save game.
 */
UCLASS(Abstract, BlueprintType, Blueprintable)
class SHATTEREDHORIZON2032_API USHSettingsWidget : public UCommonActivatableWidget
{
	GENERATED_BODY()

public:
	USHSettingsWidget();

	/** Load settings from save. */
	UFUNCTION(BlueprintCallable, Category = "SH|UI|Settings")
	void LoadSettings();

	/** Apply current settings to the engine. */
	UFUNCTION(BlueprintCallable, Category = "SH|UI|Settings")
	void ApplySettings();

	/** Save settings to disk. */
	UFUNCTION(BlueprintCallable, Category = "SH|UI|Settings")
	void SaveSettings();

	/** Revert to defaults. */
	UFUNCTION(BlueprintCallable, Category = "SH|UI|Settings")
	void ResetToDefaults();

	/** Get the current settings. */
	UFUNCTION(BlueprintPure, Category = "SH|UI|Settings")
	const FSHGameSettings& GetCurrentSettings() const { return CurrentSettings; }

	/** Set a specific setting value. */
	UFUNCTION(BlueprintCallable, Category = "SH|UI|Settings")
	void SetVideoPreset(ESHVideoPreset Preset);

	UPROPERTY(BlueprintAssignable, Category = "SH|UI|Settings")
	FOnSettingsApplied OnSettingsApplied;

protected:
	UPROPERTY(BlueprintReadWrite, Category = "SH|UI|Settings")
	FSHGameSettings CurrentSettings;

	UPROPERTY(BlueprintReadWrite, Category = "SH|UI|Settings")
	ESHSettingsTab ActiveTab = ESHSettingsTab::Video;

	void ApplyVideoSettings();
	void ApplyAudioSettings();
	void ApplyControlSettings();
};
