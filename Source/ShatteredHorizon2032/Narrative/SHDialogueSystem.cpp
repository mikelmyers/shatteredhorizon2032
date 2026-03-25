// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHDialogueSystem.h"

#include "Components/AudioComponent.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY(LogSHDialogue);

// -----------------------------------------------------------------------
//  Subsystem lifecycle
// -----------------------------------------------------------------------

void USHDialogueSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LineQueue.Reserve(MaxQueueDepth);
	BindToWorldTick();

	UE_LOG(LogSHDialogue, Log, TEXT("USHDialogueSystem initialized."));
}

void USHDialogueSystem::Deinitialize()
{
	if (TickDelegateHandle.IsValid())
	{
		if (UWorld* World = GetWorld())
		{
			World->OnWorldBeginPlay.Remove(TickDelegateHandle);
			// Unbind tick delegate.
			FWorldDelegates::OnWorldTickStart.RemoveAll(this);
		}
	}

	StopAllDialogue();
	Super::Deinitialize();
}

void USHDialogueSystem::BindToWorldTick()
{
	if (UWorld* World = GetWorld())
	{
		FWorldDelegates::OnWorldTickStart.AddWeakLambda(this, [this](UWorld* TickWorld, ELevelTick TickType, float DeltaSeconds)
		{
			if (TickWorld == GetWorld())
			{
				Tick(DeltaSeconds);
			}
		});
	}
}

// -----------------------------------------------------------------------
//  Tick
// -----------------------------------------------------------------------

void USHDialogueSystem::Tick(float DeltaSeconds)
{
	// Handle inter-line delay.
	if (!bIsPlaying && InterLineTimer > 0.f)
	{
		InterLineTimer -= DeltaSeconds;
		if (InterLineTimer <= 0.f)
		{
			InterLineTimer = 0.f;
			AdvanceQueue();
		}
		return;
	}

	if (!bIsPlaying)
	{
		return;
	}

	// Count down the remaining time.
	TimeRemaining -= DeltaSeconds;

	// Broadcast subtitle update every frame while playing.
	OnSubtitleUpdate.Broadcast(CurrentLine.Speaker, CurrentLine.Line, FMath::Max(TimeRemaining, 0.f));

	if (TimeRemaining <= 0.f)
	{
		EndCurrentLine();
	}
}

// -----------------------------------------------------------------------
//  Public interface
// -----------------------------------------------------------------------

void USHDialogueSystem::PlayDialogueLine(const FSHDialogueLine& InLine)
{
	if (bIsPlaying)
	{
		// If the new line outprioritizes the current one, interrupt.
		if (InLine.OutPrioritizes(CurrentLine))
		{
			UE_LOG(LogSHDialogue, Verbose, TEXT("Interrupting [%s] for higher-priority line [%s]."),
				*CurrentLine.Line.ToString(), *InLine.Line.ToString());
			StopAudioPlayback();

			// Notify that the old line ended (interrupted).
			OnDialogueEnded.Broadcast(CurrentLine);
			bIsPlaying = false;

			// Play the new line immediately.
			BeginLine(InLine);
		}
		else
		{
			// Enqueue the new line.
			EnqueueLine(InLine);
		}
	}
	else if (InterLineTimer > 0.f)
	{
		// In the gap between lines — if high priority, skip the gap.
		if (InLine.Priority >= static_cast<int32>(ESHDialoguePriority::CombatBark))
		{
			InterLineTimer = 0.f;
			BeginLine(InLine);
		}
		else
		{
			EnqueueLine(InLine);
		}
	}
	else
	{
		BeginLine(InLine);
	}
}

void USHDialogueSystem::StopCurrentLine()
{
	if (!bIsPlaying)
	{
		return;
	}

	StopAudioPlayback();
	OnDialogueEnded.Broadcast(CurrentLine);

	// Clear subtitle.
	OnSubtitleUpdate.Broadcast(FText::GetEmpty(), FText::GetEmpty(), 0.f);

	bIsPlaying = false;
	TimeRemaining = 0.f;

	// Immediately advance the queue — no inter-line delay on manual stop.
	AdvanceQueue();
}

void USHDialogueSystem::StopAllDialogue()
{
	StopAudioPlayback();

	if (bIsPlaying)
	{
		OnDialogueEnded.Broadcast(CurrentLine);
		OnSubtitleUpdate.Broadcast(FText::GetEmpty(), FText::GetEmpty(), 0.f);
	}

	bIsPlaying = false;
	TimeRemaining = 0.f;
	InterLineTimer = 0.f;
	LineQueue.Empty();

	UE_LOG(LogSHDialogue, Verbose, TEXT("All dialogue stopped and queue cleared."));
}

void USHDialogueSystem::PlayCombatBark(FText Speaker, FText BarkText, TSoftObjectPtr<USoundBase> AudioCue, float Duration)
{
	FSHDialogueLine Bark;
	Bark.Speaker = MoveTemp(Speaker);
	Bark.Line = MoveTemp(BarkText);
	Bark.AudioCue = MoveTemp(AudioCue);
	Bark.Duration = Duration;
	Bark.Priority = static_cast<int32>(ESHDialoguePriority::CombatBark);

	PlayDialogueLine(Bark);
}

// -----------------------------------------------------------------------
//  Internal — line management
// -----------------------------------------------------------------------

void USHDialogueSystem::BeginLine(const FSHDialogueLine& InLine)
{
	CurrentLine = InLine;
	TimeRemaining = InLine.Duration;
	bIsPlaying = true;

	UE_LOG(LogSHDialogue, Log, TEXT("[%s]: \"%s\" (%.1fs, pri %d)"),
		*InLine.Speaker.ToString(), *InLine.Line.ToString(), InLine.Duration, InLine.Priority);

	OnDialogueStarted.Broadcast(CurrentLine);
	OnSubtitleUpdate.Broadcast(CurrentLine.Speaker, CurrentLine.Line, TimeRemaining);

	// Load and play the audio cue if one is set.
	if (!InLine.AudioCue.IsNull())
	{
		LoadAndPlayAudio(InLine);
	}
}

void USHDialogueSystem::EndCurrentLine()
{
	StopAudioPlayback();

	OnDialogueEnded.Broadcast(CurrentLine);
	OnSubtitleUpdate.Broadcast(FText::GetEmpty(), FText::GetEmpty(), 0.f);

	bIsPlaying = false;
	TimeRemaining = 0.f;

	// Start inter-line delay before advancing the queue.
	if (LineQueue.Num() > 0)
	{
		InterLineTimer = InterLineDelay;
	}
}

void USHDialogueSystem::AdvanceQueue()
{
	if (LineQueue.Num() == 0)
	{
		return;
	}

	// Queue is sorted by priority (highest first). Pop the front.
	FSHDialogueLine NextLine = LineQueue[0];
	LineQueue.RemoveAt(0);

	BeginLine(NextLine);
}

void USHDialogueSystem::EnqueueLine(const FSHDialogueLine& InLine)
{
	// Enforce queue depth — if full, drop the lowest-priority (last) element.
	if (LineQueue.Num() >= MaxQueueDepth)
	{
		// Only drop if the new line has higher priority than the lowest queued.
		if (LineQueue.Num() > 0 && InLine.Priority <= LineQueue.Last().Priority)
		{
			UE_LOG(LogSHDialogue, Warning, TEXT("Dialogue queue full — dropping line [%s]."),
				*InLine.Line.ToString());
			return;
		}
		LineQueue.Pop();
	}

	// Insert in priority order (highest priority first, FIFO within same priority).
	int32 InsertIndex = 0;
	for (; InsertIndex < LineQueue.Num(); ++InsertIndex)
	{
		if (InLine.Priority > LineQueue[InsertIndex].Priority)
		{
			break;
		}
	}

	LineQueue.Insert(InLine, InsertIndex);

	UE_LOG(LogSHDialogue, Verbose, TEXT("Enqueued line [%s] at position %d (queue depth: %d)."),
		*InLine.Line.ToString(), InsertIndex, LineQueue.Num());
}

// -----------------------------------------------------------------------
//  Internal — audio playback
// -----------------------------------------------------------------------

void USHDialogueSystem::LoadAndPlayAudio(const FSHDialogueLine& InLine)
{
	// Cancel any pending async load.
	if (AudioLoadHandle.IsValid())
	{
		AudioLoadHandle->CancelHandle();
		AudioLoadHandle.Reset();
	}

	// If already loaded, play immediately.
	if (USoundBase* LoadedSound = InLine.AudioCue.Get())
	{
		if (!DialogueAudioComponent)
		{
			DialogueAudioComponent = UGameplayStatics::CreateSound2D(GetWorld(), LoadedSound, 1.f, 1.f, 0.f, nullptr, false, false);
		}

		if (DialogueAudioComponent)
		{
			DialogueAudioComponent->SetSound(LoadedSound);
			DialogueAudioComponent->Play();
		}
		return;
	}

	// Async load the sound asset.
	FSoftObjectPath AssetPath = InLine.AudioCue.ToSoftObjectPath();
	if (AssetPath.IsValid())
	{
		AudioLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
			AssetPath,
			FStreamableDelegate::CreateWeakLambda(this, [this]()
			{
				if (!bIsPlaying)
				{
					return;
				}

				USoundBase* LoadedSound = CurrentLine.AudioCue.Get();
				if (!LoadedSound)
				{
					UE_LOG(LogSHDialogue, Warning, TEXT("Failed to load audio cue for line [%s]."),
						*CurrentLine.Line.ToString());
					return;
				}

				if (!DialogueAudioComponent)
				{
					DialogueAudioComponent = UGameplayStatics::CreateSound2D(GetWorld(), LoadedSound, 1.f, 1.f, 0.f, nullptr, false, false);
				}

				if (DialogueAudioComponent)
				{
					DialogueAudioComponent->SetSound(LoadedSound);
					DialogueAudioComponent->Play();
				}
			})
		);
	}
}

void USHDialogueSystem::StopAudioPlayback()
{
	if (AudioLoadHandle.IsValid())
	{
		AudioLoadHandle->CancelHandle();
		AudioLoadHandle.Reset();
	}

	if (DialogueAudioComponent && DialogueAudioComponent->IsPlaying())
	{
		DialogueAudioComponent->Stop();
	}
}
