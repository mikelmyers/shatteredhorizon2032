// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHAccessibilitySettings.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogSH_Accessibility, Log, All);

// -----------------------------------------------------------------------
//  Subsystem lifecycle
// -----------------------------------------------------------------------

void USHAccessibilitySettings::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadSettingsFromConfig();
	UE_LOG(LogSH_Accessibility, Log, TEXT("Accessibility subsystem initialized."));
}

void USHAccessibilitySettings::Deinitialize()
{
	SaveSettingsToConfig();
	Super::Deinitialize();
}

// -----------------------------------------------------------------------
//  Colorblind
// -----------------------------------------------------------------------

void USHAccessibilitySettings::SetColorblindMode(ESHColorblindMode InMode)
{
	if (ColorblindMode == InMode)
	{
		return;
	}

	ColorblindMode = InMode;
	ApplyColorblindMaterialParams();
	SaveSettingsToConfig();

	UE_LOG(LogSH_Accessibility, Log, TEXT("Colorblind mode set to %d."), static_cast<int32>(InMode));
}

void USHAccessibilitySettings::ApplyColorblindMaterialParams()
{
	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	UMaterialParameterCollection* MPC = ColorblindMPC.LoadSynchronous();
	if (!MPC)
	{
		UE_LOG(LogSH_Accessibility, Warning,
			TEXT("Colorblind MPC not set — cannot apply colorblind correction."));
		return;
	}

	UMaterialParameterCollectionInstance* Instance = World->GetParameterCollectionInstance(MPC);
	if (!Instance)
	{
		return;
	}

	// Encode the colorblind mode as a scalar parameter.
	// 0 = Off, 1 = Protanopia, 2 = Deuteranopia, 3 = Tritanopia
	// The post-process material interprets this value to shift colours accordingly.
	const float ModeValue = static_cast<float>(ColorblindMode);
	Instance->SetScalarParameterValue(FName(TEXT("ColorblindMode")), ModeValue);

	// Set per-mode severity weights so the material can use component-level adjustments.
	const float bProtanopia   = (ColorblindMode == ESHColorblindMode::Protanopia)   ? 1.0f : 0.0f;
	const float bDeuteranopia = (ColorblindMode == ESHColorblindMode::Deuteranopia) ? 1.0f : 0.0f;
	const float bTritanopia   = (ColorblindMode == ESHColorblindMode::Tritanopia)   ? 1.0f : 0.0f;

	Instance->SetScalarParameterValue(FName(TEXT("Protanopia")),   bProtanopia);
	Instance->SetScalarParameterValue(FName(TEXT("Deuteranopia")), bDeuteranopia);
	Instance->SetScalarParameterValue(FName(TEXT("Tritanopia")),   bTritanopia);
}

// -----------------------------------------------------------------------
//  UI Scale
// -----------------------------------------------------------------------

void USHAccessibilitySettings::SetUIScale(float Scale)
{
	UIScale = FMath::Clamp(Scale, 0.75f, 1.5f);
	SaveSettingsToConfig();

	UE_LOG(LogSH_Accessibility, Log, TEXT("UI scale set to %.2f."), UIScale);
}

// -----------------------------------------------------------------------
//  Subtitle Size
// -----------------------------------------------------------------------

void USHAccessibilitySettings::SetSubtitleSize(float Size)
{
	SubtitleSize = FMath::Clamp(Size, 0.5f, 2.0f);
	SaveSettingsToConfig();

	UE_LOG(LogSH_Accessibility, Log, TEXT("Subtitle size set to %.2f."), SubtitleSize);
}

// -----------------------------------------------------------------------
//  Closed Captions
// -----------------------------------------------------------------------

void USHAccessibilitySettings::SetClosedCaptions(bool bEnabled)
{
	bClosedCaptionsEnabled = bEnabled;
	SaveSettingsToConfig();

	UE_LOG(LogSH_Accessibility, Log, TEXT("Closed captions %s."),
		bEnabled ? TEXT("enabled") : TEXT("disabled"));
}

void USHAccessibilitySettings::TriggerClosedCaption(const FSHClosedCaption& Caption)
{
	if (!bClosedCaptionsEnabled)
	{
		return;
	}

	OnClosedCaptionTriggered.Broadcast(Caption);
}

// -----------------------------------------------------------------------
//  Aim Assist
// -----------------------------------------------------------------------

void USHAccessibilitySettings::SetControllerAimAssist(bool bEnabled)
{
	bAimAssistEnabled = bEnabled;
	SaveSettingsToConfig();

	UE_LOG(LogSH_Accessibility, Log, TEXT("Controller aim assist %s."),
		bEnabled ? TEXT("enabled") : TEXT("disabled"));
}

// -----------------------------------------------------------------------
//  Bulk apply
// -----------------------------------------------------------------------

void USHAccessibilitySettings::ApplyAllSettings()
{
	LoadSettingsFromConfig();
	ApplyColorblindMaterialParams();

	UE_LOG(LogSH_Accessibility, Log,
		TEXT("All accessibility settings applied. Colorblind=%d UIScale=%.2f SubSize=%.2f CC=%d AimAssist=%d"),
		static_cast<int32>(ColorblindMode), UIScale, SubtitleSize,
		bClosedCaptionsEnabled ? 1 : 0, bAimAssistEnabled ? 1 : 0);
}

// -----------------------------------------------------------------------
//  Persistence helpers
// -----------------------------------------------------------------------

void USHAccessibilitySettings::SaveSettingsToConfig()
{
	GConfig->SetInt(
		TEXT("ShatteredHorizon.Accessibility"), TEXT("ColorblindMode"),
		static_cast<int32>(ColorblindMode), GGameUserSettingsIni);
	GConfig->SetFloat(
		TEXT("ShatteredHorizon.Accessibility"), TEXT("UIScale"),
		UIScale, GGameUserSettingsIni);
	GConfig->SetFloat(
		TEXT("ShatteredHorizon.Accessibility"), TEXT("SubtitleSize"),
		SubtitleSize, GGameUserSettingsIni);
	GConfig->SetBool(
		TEXT("ShatteredHorizon.Accessibility"), TEXT("ClosedCaptions"),
		bClosedCaptionsEnabled, GGameUserSettingsIni);
	GConfig->SetBool(
		TEXT("ShatteredHorizon.Accessibility"), TEXT("AimAssist"),
		bAimAssistEnabled, GGameUserSettingsIni);

	GConfig->Flush(false, GGameUserSettingsIni);
}

void USHAccessibilitySettings::LoadSettingsFromConfig()
{
	int32 ModeInt = 0;
	if (GConfig->GetInt(TEXT("ShatteredHorizon.Accessibility"), TEXT("ColorblindMode"), ModeInt, GGameUserSettingsIni))
	{
		ColorblindMode = static_cast<ESHColorblindMode>(FMath::Clamp(ModeInt, 0, 3));
	}

	GConfig->GetFloat(TEXT("ShatteredHorizon.Accessibility"), TEXT("UIScale"), UIScale, GGameUserSettingsIni);
	UIScale = FMath::Clamp(UIScale, 0.75f, 1.5f);

	GConfig->GetFloat(TEXT("ShatteredHorizon.Accessibility"), TEXT("SubtitleSize"), SubtitleSize, GGameUserSettingsIni);
	SubtitleSize = FMath::Clamp(SubtitleSize, 0.5f, 2.0f);

	GConfig->GetBool(TEXT("ShatteredHorizon.Accessibility"), TEXT("ClosedCaptions"), bClosedCaptionsEnabled, GGameUserSettingsIni);
	GConfig->GetBool(TEXT("ShatteredHorizon.Accessibility"), TEXT("AimAssist"), bAimAssistEnabled, GGameUserSettingsIni);
}
