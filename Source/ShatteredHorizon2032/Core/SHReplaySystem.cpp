// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHReplaySystem.h"
#include "Engine/GameInstance.h"
#include "Engine/DemoNetDriver.h"
#include "Engine/World.h"
#include "Misc/Paths.h"
#include "Misc/DateTime.h"
#include "HAL/PlatformFileManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogSH_Replay, Log, All);

// -----------------------------------------------------------------------
//  Subsystem lifecycle
// -----------------------------------------------------------------------

void USHReplaySystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Ensure replay directory exists
	const FString ReplayDir = GetReplayDirectoryPath();
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*ReplayDir))
	{
		PlatformFile.CreateDirectoryTree(*ReplayDir);
	}

	UE_LOG(LogSH_Replay, Log, TEXT("Replay system initialized. Replay dir: %s"), *ReplayDir);
}

void USHReplaySystem::Deinitialize()
{
	if (bIsRecording)
	{
		StopRecording();
	}
	if (bIsPlaying)
	{
		StopPlayback();
	}

	Super::Deinitialize();
}

// -----------------------------------------------------------------------
//  Recording
// -----------------------------------------------------------------------

void USHReplaySystem::StartRecording()
{
	if (bIsRecording)
	{
		UE_LOG(LogSH_Replay, Warning, TEXT("Already recording a replay."));
		return;
	}

	if (bIsPlaying)
	{
		UE_LOG(LogSH_Replay, Warning, TEXT("Cannot record while playing a replay."));
		return;
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	CurrentReplayName = GenerateReplayName();

	// Use UE5's built-in DemoNetDriver for recording
	UWorld* World = GI->GetWorld();
	if (World)
	{
		// Start recording using the replay system
		TArray<FString> AdditionalOptions;
		World->GetGameInstance()->StartRecordingReplay(CurrentReplayName, CurrentReplayName, AdditionalOptions);

		bIsRecording = true;
		OnReplayStarted.Broadcast(CurrentReplayName);

		UE_LOG(LogSH_Replay, Log, TEXT("Started recording replay: %s"), *CurrentReplayName);
	}
}

void USHReplaySystem::StopRecording()
{
	if (!bIsRecording)
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	if (GI)
	{
		GI->StopRecordingReplay();
	}

	bIsRecording = false;
	OnReplayStopped.Broadcast(CurrentReplayName);

	UE_LOG(LogSH_Replay, Log, TEXT("Stopped recording replay: %s"), *CurrentReplayName);
	CurrentReplayName.Empty();
}

// -----------------------------------------------------------------------
//  Playback
// -----------------------------------------------------------------------

void USHReplaySystem::PlayReplay(const FString& ReplayName)
{
	if (bIsRecording)
	{
		UE_LOG(LogSH_Replay, Warning, TEXT("Cannot play replay while recording."));
		return;
	}

	UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}

	// Stop existing playback
	if (bIsPlaying)
	{
		StopPlayback();
	}

	CurrentReplayName = ReplayName;
	CurrentPlaybackSpeed = 1.0f;
	bIsPaused = false;

	// Use UE5 built-in replay playback
	TArray<FString> AdditionalOptions;
	GI->PlayReplay(ReplayName, nullptr, AdditionalOptions);

	bIsPlaying = true;
	OnReplayStarted.Broadcast(ReplayName);

	UE_LOG(LogSH_Replay, Log, TEXT("Playing replay: %s"), *ReplayName);
}

void USHReplaySystem::PauseReplay()
{
	if (!bIsPlaying)
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	if (!World)
	{
		return;
	}

	bIsPaused = !bIsPaused;

	// Access the DemoNetDriver to control playback
	UDemoNetDriver* DemoDriver = World->GetDemoNetDriver();
	if (DemoDriver)
	{
		if (bIsPaused)
		{
			// Pause by setting world time dilation to 0
			World->GetWorldSettings()->SetTimeDilation(0.0f);
		}
		else
		{
			World->GetWorldSettings()->SetTimeDilation(CurrentPlaybackSpeed);
		}
	}

	UE_LOG(LogSH_Replay, Log, TEXT("Replay %s."), bIsPaused ? TEXT("paused") : TEXT("resumed"));
}

void USHReplaySystem::SetPlaybackSpeed(float Speed)
{
	CurrentPlaybackSpeed = FMath::Clamp(Speed, 0.1f, 10.0f);

	if (bIsPlaying && !bIsPaused)
	{
		UGameInstance* GI = GetGameInstance();
		UWorld* World = GI ? GI->GetWorld() : nullptr;
		if (World)
		{
			World->GetWorldSettings()->SetTimeDilation(CurrentPlaybackSpeed);
		}
	}

	UE_LOG(LogSH_Replay, Log, TEXT("Playback speed set to %.2fx."), CurrentPlaybackSpeed);
}

void USHReplaySystem::StopPlayback()
{
	if (!bIsPlaying)
	{
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	if (World)
	{
		// Reset time dilation
		World->GetWorldSettings()->SetTimeDilation(1.0f);

		// Stop the demo net driver
		UDemoNetDriver* DemoDriver = World->GetDemoNetDriver();
		if (DemoDriver)
		{
			DemoDriver->StopDemo();
		}
	}

	bIsPlaying = false;
	bIsPaused = false;

	OnReplayStopped.Broadcast(CurrentReplayName);
	UE_LOG(LogSH_Replay, Log, TEXT("Stopped replay playback: %s"), *CurrentReplayName);
	CurrentReplayName.Empty();
}

// -----------------------------------------------------------------------
//  Management
// -----------------------------------------------------------------------

TArray<FString> USHReplaySystem::GetAvailableReplays() const
{
	TArray<FString> Result;
	const FString ReplayDir = GetReplayDirectoryPath();

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*ReplayDir))
	{
		return Result;
	}

	const FString SearchPattern = TEXT("*") + ReplayExtension;
	TArray<FString> FoundFiles;
	PlatformFile.FindFiles(FoundFiles, *ReplayDir, *SearchPattern);

	for (const FString& FilePath : FoundFiles)
	{
		Result.Add(FPaths::GetBaseFilename(FilePath));
	}

	// Sort alphabetically (most recent first due to timestamp naming)
	Result.Sort([](const FString& A, const FString& B) { return A > B; });

	return Result;
}

bool USHReplaySystem::DeleteReplay(const FString& ReplayName)
{
	if (bIsPlaying && CurrentReplayName == ReplayName)
	{
		UE_LOG(LogSH_Replay, Warning, TEXT("Cannot delete replay that is currently playing."));
		return false;
	}

	if (bIsRecording && CurrentReplayName == ReplayName)
	{
		UE_LOG(LogSH_Replay, Warning, TEXT("Cannot delete replay that is currently recording."));
		return false;
	}

	const FString FilePath = FPaths::Combine(GetReplayDirectoryPath(), ReplayName + ReplayExtension);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (!PlatformFile.FileExists(*FilePath))
	{
		UE_LOG(LogSH_Replay, Warning, TEXT("Replay file not found: %s"), *FilePath);
		return false;
	}

	const bool bDeleted = PlatformFile.DeleteFile(*FilePath);
	if (bDeleted)
	{
		UE_LOG(LogSH_Replay, Log, TEXT("Deleted replay: %s"), *ReplayName);
	}
	else
	{
		UE_LOG(LogSH_Replay, Error, TEXT("Failed to delete replay: %s"), *ReplayName);
	}
	return bDeleted;
}

// -----------------------------------------------------------------------
//  Internal helpers
// -----------------------------------------------------------------------

FString USHReplaySystem::GenerateReplayName() const
{
	const FDateTime Now = FDateTime::Now();
	return FString::Printf(TEXT("SH_%04d%02d%02d_%02d%02d%02d"),
		Now.GetYear(), Now.GetMonth(), Now.GetDay(),
		Now.GetHour(), Now.GetMinute(), Now.GetSecond());
}

FString USHReplaySystem::GetReplayDirectoryPath() const
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), ReplayDirectory);
}
