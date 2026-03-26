// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHSettingsWidget.h"
#include "SHSettingsSaveGame.h"
#include "GameFramework/GameUserSettings.h"
#include "GameFramework/InputSettings.h"
#include "Kismet/GameplayStatics.h"
#include "AudioDevice.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundMix.h"

USHSettingsWidget::USHSettingsWidget()
{
}

void USHSettingsWidget::LoadSettings()
{
	if (UGameplayStatics::DoesSaveGameExist(TEXT("SH2032_Settings"), 0))
	{
		if (USHSettingsSaveGame* SaveGame = Cast<USHSettingsSaveGame>(
				UGameplayStatics::LoadGameFromSlot(TEXT("SH2032_Settings"), 0)))
		{
			CurrentSettings = SaveGame->Settings;
			UE_LOG(LogTemp, Log, TEXT("[Settings] Loaded settings from save"));
		}
		else
		{
			ResetToDefaults();
		}
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
	USHSettingsSaveGame* SaveGame = NewObject<USHSettingsSaveGame>();
	SaveGame->Settings = CurrentSettings;

	if (UGameplayStatics::SaveGameToSlot(SaveGame, TEXT("SH2032_Settings"), 0))
	{
		UE_LOG(LogTemp, Log, TEXT("[Settings] Saved settings to disk"));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Settings] Failed to save settings"));
	}
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
	// Apply volume levels via sound class property overrides.
	// Sound classes are expected at standard Content paths.
	auto SetSoundClassVolume = [](const TCHAR* ClassPath, float Volume)
	{
		if (USoundClass* SC = FindObject<USoundClass>(nullptr, ClassPath))
		{
			SC->Properties.Volume = FMath::Clamp(Volume, 0.f, 1.f);
		}
	};

	const float Master = FMath::Clamp(CurrentSettings.MasterVolume, 0.f, 1.f);
	SetSoundClassVolume(TEXT("/Game/Audio/SoundClasses/SC_Master"), Master);
	SetSoundClassVolume(TEXT("/Game/Audio/SoundClasses/SC_SFX"), CurrentSettings.SFXVolume * Master);
	SetSoundClassVolume(TEXT("/Game/Audio/SoundClasses/SC_Music"), CurrentSettings.MusicVolume * Master);
	SetSoundClassVolume(TEXT("/Game/Audio/SoundClasses/SC_Voice"), CurrentSettings.VoiceVolume * Master);
	SetSoundClassVolume(TEXT("/Game/Audio/SoundClasses/SC_Ambient"), CurrentSettings.AmbientVolume * Master);

	UE_LOG(LogTemp, Log, TEXT("[Settings] Audio applied: Master=%.2f SFX=%.2f Music=%.2f Voice=%.2f"),
		   CurrentSettings.MasterVolume, CurrentSettings.SFXVolume,
		   CurrentSettings.MusicVolume, CurrentSettings.VoiceVolume);
}

void USHSettingsWidget::ApplyControlSettings()
{
	if (UWorld* World = GetWorld())
	{
		if (APlayerController* PC = World->GetFirstPlayerController())
		{
			const float SensScale = FMath::Clamp(CurrentSettings.MouseSensitivity, 0.1f, 5.f);
			const float InvertY = CurrentSettings.bInvertY ? -1.f : 1.f;

			PC->InputYawScale_DEPRECATED = 2.5f * SensScale;
			PC->InputPitchScale_DEPRECATED = 2.5f * SensScale * InvertY;
		}
	}

	UE_LOG(LogTemp, Log, TEXT("[Settings] Controls applied: MouseSens=%.2f InvertY=%s"),
		   CurrentSettings.MouseSensitivity,
		   CurrentSettings.bInvertY ? TEXT("true") : TEXT("false"));
}
