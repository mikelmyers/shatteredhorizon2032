// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SHDialogueSystem.generated.h"

class UAudioComponent;
class USoundBase;

DECLARE_LOG_CATEGORY_EXTERN(LogSHDialogue, Log, All);

// ─────────────────────────────────────────────────────────────
//  Enums
// ─────────────────────────────────────────────────────────────

/** Priority tiers for dialogue lines. Higher values take precedence. */
UENUM(BlueprintType)
enum class ESHDialoguePriority : uint8
{
	/** Background ambient chatter — lowest priority. */
	AmbientChatter	= 0	UMETA(DisplayName = "Ambient Chatter"),

	/** Standard dialogue lines (story, briefing context). */
	Dialogue		= 50	UMETA(DisplayName = "Dialogue"),

	/** Combat barks triggered by gameplay events. */
	CombatBark		= 100	UMETA(DisplayName = "Combat Bark"),

	/** Critical narrative moments — cannot be interrupted. */
	Critical		= 200	UMETA(DisplayName = "Critical")
};

// ─────────────────────────────────────────────────────────────
//  Structs
// ─────────────────────────────────────────────────────────────

/** A single dialogue line with associated metadata for playback. */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHDialogueLine
{
	GENERATED_BODY()

	/** Display name of the speaker. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Dialogue")
	FText Speaker;

	/** Subtitle text for this line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Dialogue")
	FText Line;

	/** Soft reference to the audio cue. Loaded asynchronously on demand. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Dialogue")
	TSoftObjectPtr<USoundBase> AudioCue;

	/** Duration in seconds the line should display (used if no audio). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Dialogue", meta = (ClampMin = "0.1"))
	float Duration = 3.f;

	/** Priority value. Higher priorities interrupt lower ones. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Dialogue")
	int32 Priority = static_cast<int32>(ESHDialoguePriority::Dialogue);

	/** Returns true if this line has higher priority than another. */
	bool OutPrioritizes(const FSHDialogueLine& Other) const { return Priority > Other.Priority; }
};

// ─────────────────────────────────────────────────────────────
//  Delegates
// ─────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnDialogueStarted,
	const FSHDialogueLine&, DialogueLine);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnDialogueEnded,
	const FSHDialogueLine&, DialogueLine);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnSubtitleUpdate,
	FText, Speaker,
	FText, Text,
	float, RemainingTime);

// ─────────────────────────────────────────────────────────────
//  USHDialogueSystem — World Subsystem
// ─────────────────────────────────────────────────────────────

/**
 * USHDialogueSystem
 *
 * World subsystem that manages triggered voice lines and subtitles.
 * Supports a priority-based queue: combat barks can interrupt ambient
 * chatter, and critical narrative beats override everything. Lines
 * that arrive while a same-or-higher priority line is playing are
 * enqueued and played in priority-then-FIFO order.
 */
UCLASS()
class SHATTEREDHORIZON2032_API USHDialogueSystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	// ------------------------------------------------------------------
	//  Subsystem lifecycle
	// ------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

	// ------------------------------------------------------------------
	//  Public interface
	// ------------------------------------------------------------------

	/**
	 * Play a dialogue line. If a line is already playing and the new line
	 * has higher priority, the current line is interrupted. Otherwise the
	 * new line is enqueued.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Dialogue")
	void PlayDialogueLine(const FSHDialogueLine& InLine);

	/** Immediately stop the current line and clear the queue. */
	UFUNCTION(BlueprintCallable, Category = "SH|Dialogue")
	void StopCurrentLine();

	/** Stop the current line and clear every queued line. */
	UFUNCTION(BlueprintCallable, Category = "SH|Dialogue")
	void StopAllDialogue();

	/** Returns true if a line is currently playing. */
	UFUNCTION(BlueprintPure, Category = "SH|Dialogue")
	bool IsPlaying() const { return bIsPlaying; }

	/** Get the line currently being played (only valid when IsPlaying). */
	UFUNCTION(BlueprintPure, Category = "SH|Dialogue")
	const FSHDialogueLine& GetCurrentLine() const { return CurrentLine; }

	/** Get the number of lines waiting in the queue. */
	UFUNCTION(BlueprintPure, Category = "SH|Dialogue")
	int32 GetQueueDepth() const { return LineQueue.Num(); }

	/**
	 * Play a short combat bark. Convenience wrapper that sets the priority
	 * to CombatBark and constructs the line from minimal parameters.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Dialogue")
	void PlayCombatBark(FText Speaker, FText BarkText, TSoftObjectPtr<USoundBase> AudioCue, float Duration = 2.f);

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Dialogue")
	FOnDialogueStarted OnDialogueStarted;

	UPROPERTY(BlueprintAssignable, Category = "SH|Dialogue")
	FOnDialogueEnded OnDialogueEnded;

	UPROPERTY(BlueprintAssignable, Category = "SH|Dialogue")
	FOnSubtitleUpdate OnSubtitleUpdate;

protected:
	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Maximum number of lines allowed in the queue. Oldest low-priority lines are dropped when exceeded. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Dialogue|Config")
	int32 MaxQueueDepth = 8;

	/** Grace period after a line ends before the next queued line starts (seconds). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Dialogue|Config")
	float InterLineDelay = 0.15f;

private:
	// ------------------------------------------------------------------
	//  Internal helpers
	// ------------------------------------------------------------------

	/** Tick driven from a world-tick delegate. */
	void Tick(float DeltaSeconds);
	void BindToWorldTick();

	/** Begin playback of a line (assumes no line is currently active). */
	void BeginLine(const FSHDialogueLine& InLine);

	/** End the currently active line and advance the queue. */
	void EndCurrentLine();

	/** Start playing the next line from the queue, if any. */
	void AdvanceQueue();

	/** Insert a line into the queue, maintaining priority order. */
	void EnqueueLine(const FSHDialogueLine& InLine);

	/** Async-load the audio cue and start playback. */
	void LoadAndPlayAudio(const FSHDialogueLine& InLine);

	/** Stop the audio component if it is playing. */
	void StopAudioPlayback();

	FDelegateHandle TickDelegateHandle;

	// ------------------------------------------------------------------
	//  Runtime state
	// ------------------------------------------------------------------

	/** The line currently being played. */
	FSHDialogueLine CurrentLine;

	/** Is a line actively being played? */
	bool bIsPlaying = false;

	/** Remaining time on the current line (seconds). */
	float TimeRemaining = 0.f;

	/** Delay timer between lines (seconds). */
	float InterLineTimer = 0.f;

	/** Priority-sorted queue of pending lines. */
	TArray<FSHDialogueLine> LineQueue;

	/** Audio component used for playback (created on first use). */
	UPROPERTY()
	TObjectPtr<UAudioComponent> DialogueAudioComponent;

	/** Handle for the async load of the current cue. */
	TSharedPtr<struct FStreamableHandle> AudioLoadHandle;
};
