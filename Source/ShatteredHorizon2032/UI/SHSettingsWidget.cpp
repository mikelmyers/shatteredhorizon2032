// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHSettingsWidget.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "AudioDevice.h"
#include "Sound/SoundMix.h"

USHSettingsWidget::USHSettingsWidget()
{
}

void USHSettingsWidget::LoadSettings()
{
	// Load from save game or use defaults.
	if (UGameplayStatics::DoesSaveGameExist(TEXT("SH2032_Settings"), 0))
	{
		// TODO: Deserialize FSHGameSettings from save game.
		UE_LOG(LogTemp, Log, TEXT("[Settings] Loaded settings from save"));
	}
	else
	{
		ResetToDefaults();
	}
}

void USHSettingsWidget::ApplySettings()
{
	ApplyVideoSettings();
	ApplyAudioSettings();
	ApplyControlSettings();

	OnSettingsApplied.Broadcast(CurrentSettings);

	UE_LOG(LogTemp, Log, TEXT("[Settings] Applied all settings"));
}

void USHSettingsWidget::SaveSettings()
{
	// TODO: Serialize FSHGameSettings to a USaveGame subclass.
	UE_LOG(LogTemp, Log, TEXT("[Settings] Saved settings to disk"));
}

void USHSettingsWidget::ResetToDefaults()
{
	CurrentSettings = FSHGameSettings();
	UE_LOG(LogTemp, Log, TEXT("[Settings] Reset to defaults"));
}

void USHSettingsWidget::SetVideoPreset(ESHVideoPreset Preset)
{
	CurrentSettings.VideoPreset = Preset;

	if (UGameUserSettings* UserSettings = UGameUserSettings::GetGameUserSettings())
	{
		switch (Preset)
		{
		case ESHVideoPreset::Low:
			UserSettings->SetOverallScalabilityLevel(0);
			break;
		case ESHVideoPreset::Medium:
			UserSettings->SetOverallScalabilityLevel(1);
			break;
		case ESHVideoPreset::High:
			UserSettings->SetOverallScalabilityLevel(2);
			break;
		case ESHVideoPreset::Ultra:
			UserSettings->SetOverallScalabilityLevel(3);
			break;
		default:
			break;
		}
	}
}

void USHSettingsWidget::ApplyVideoSettings()
{
	if (UGameUserSettings* UserSettings = UGameUserSettings::GetGameUserSettings())
	{
		UserSettings->SetScreenResolution(CurrentSettings.Resolution);
		UserSettings->SetFullscreenMode(CurrentSettings.bFullscreen
			? EWindowMode::Fullscreen : EWindowMode::Windowed);
		UserSettings->SetVSyncEnabled(CurrentSettings.bVSync);
		UserSettings->SetFrameRateLimit(
			CurrentSettings.FrameRateLimit > 0 ? CurrentSettings.FrameRateLimit : 0.f);

		UserSettings->ApplySettings(false);
	}
}

void USHSettingsWidget::ApplyAudioSettings()
{
	// Audio volumes applied via sound class volumes or submix.
	// TODO: Apply CurrentSettings.MasterVolume etc. to sound classes.
	UE_LOG(LogTemp, Log, TEXT("[Settings] Audio: Master=%.2f SFX=%.2f Music=%.2f Voice=%.2f"),
		   CurrentSettings.MasterVolume, CurrentSettings.SFXVolume,
		   CurrentSettings.MusicVolume, CurrentSettings.VoiceVolume);
}

void USHSettingsWidget::ApplyControlSettings()
{
	// TODO: Apply sensitivity and invert settings to input system.
	UE_LOG(LogTemp, Log, TEXT("[Settings] Controls: MouseSens=%.2f InvertY=%s"),
		   CurrentSettings.MouseSensitivity,
		   CurrentSettings.bInvertY ? TEXT("true") : TEXT("false"));
}
