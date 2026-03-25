// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SHReplaySystem.generated.h"

/* -----------------------------------------------------------------------
 *  Delegates
 * --------------------------------------------------------------------- */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnReplayStarted, const FString&, ReplayName);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnReplayStopped, const FString&, ReplayName);

/* -----------------------------------------------------------------------
 *  USHReplaySystem
 * --------------------------------------------------------------------- */

/**
 * USHReplaySystem
 *
 * Game instance subsystem wrapping UE5's built-in DemoNetDriver replay
 * system. Provides high-level recording, playback, and management of
 * match replays for both competitive review and AI analysis.
 */
UCLASS()
class SHATTEREDHORIZON2032_API USHReplaySystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ------------------------------------------------------------------
	//  USubsystem overrides
	// ------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ------------------------------------------------------------------
	//  Recording
	// ------------------------------------------------------------------

	/** Start recording a replay with an auto-generated name. */
	UFUNCTION(BlueprintCallable, Category = "SH|Replay")
	void StartRecording();

	/** Stop the current recording. */
	UFUNCTION(BlueprintCallable, Category = "SH|Replay")
	void StopRecording();

	/** Whether a replay is currently being recorded. */
	UFUNCTION(BlueprintPure, Category = "SH|Replay")
	bool IsRecording() const { return bIsRecording; }

	// ------------------------------------------------------------------
	//  Playback
	// ------------------------------------------------------------------

	/** Play a replay by name. */
	UFUNCTION(BlueprintCallable, Category = "SH|Replay")
	void PlayReplay(const FString& ReplayName);

	/** Pause or resume current replay playback. */
	UFUNCTION(BlueprintCallable, Category = "SH|Replay")
	void PauseReplay();

	/** Set playback speed (1.0 = normal, 0.5 = half, 2.0 = double). */
	UFUNCTION(BlueprintCallable, Category = "SH|Replay")
	void SetPlaybackSpeed(float Speed);

	/** Stop current replay playback entirely. */
	UFUNCTION(BlueprintCallable, Category = "SH|Replay")
	void StopPlayback();

	/** Whether a replay is currently playing. */
	UFUNCTION(BlueprintPure, Category = "SH|Replay")
	bool IsPlaying() const { return bIsPlaying; }

	/** Get the current playback speed. */
	UFUNCTION(BlueprintPure, Category = "SH|Replay")
	float GetPlaybackSpeed() const { return CurrentPlaybackSpeed; }

	// ------------------------------------------------------------------
	//  Management
	// ------------------------------------------------------------------

	/** Get a list of all available replay files. */
	UFUNCTION(BlueprintCallable, Category = "SH|Replay")
	TArray<FString> GetAvailableReplays() const;

	/** Delete a replay file by name. */
	UFUNCTION(BlueprintCallable, Category = "SH|Replay")
	bool DeleteReplay(const FString& ReplayName);

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Replay")
	FOnReplayStarted OnReplayStarted;

	UPROPERTY(BlueprintAssignable, Category = "SH|Replay")
	FOnReplayStopped OnReplayStopped;

protected:
	/** Directory where replays are stored (relative to Saved). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Replay|Config")
	FString ReplayDirectory = TEXT("Demos");

	/** Replay file extension. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Replay|Config")
	FString ReplayExtension = TEXT(".replay");

private:
	/** Generate a timestamped replay name. */
	FString GenerateReplayName() const;

	/** Get the full path to the replay directory. */
	FString GetReplayDirectoryPath() const;

	bool bIsRecording = false;
	bool bIsPlaying = false;
	bool bIsPaused = false;
	float CurrentPlaybackSpeed = 1.0f;
	FString CurrentReplayName;
};
