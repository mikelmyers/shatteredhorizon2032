// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHBriefingSystem.h"

#include "Components/AudioComponent.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"

DEFINE_LOG_CATEGORY(LogSHBriefing);

// -----------------------------------------------------------------------
//  Constructor
// -----------------------------------------------------------------------

USHBriefingSystem::USHBriefingSystem()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

// -----------------------------------------------------------------------
//  Lifecycle
// -----------------------------------------------------------------------

void USHBriefingSystem::BeginPlay()
{
	Super::BeginPlay();
}

void USHBriefingSystem::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!bIsBriefingActive)
	{
		return;
	}

	SectionTimer += DeltaTime;

	const float SectionDuration = GetCurrentSectionDuration();
	if (SectionTimer >= SectionDuration)
	{
		AdvanceSection();
	}
}

// -----------------------------------------------------------------------
//  Public interface
// -----------------------------------------------------------------------

void USHBriefingSystem::StartBriefing(const FSHBriefingData& InBriefingData)
{
	if (bIsBriefingActive)
	{
		UE_LOG(LogSHBriefing, Warning, TEXT("Attempted to start briefing while one is already active. Stopping current briefing."));
		CompleteBriefing();
	}

	if (!InBriefingData.IsValid())
	{
		UE_LOG(LogSHBriefing, Error, TEXT("Invalid briefing data — MissionName or BriefingText is empty."));
		return;
	}

	ActiveBriefingData = InBriefingData;
	bIsBriefingActive = true;
	CurrentSectionIndex = 0;
	SectionTimer = 0.f;

	SetComponentTickEnabled(true);

	UE_LOG(LogSHBriefing, Log, TEXT("Briefing started: %s"), *ActiveBriefingData.MissionName.ToString());

	OnBriefingStarted.Broadcast(ActiveBriefingData);

	// Begin with the overview section.
	EnterSection(ESHBriefingSection::MissionOverview);

	// Start narration audio if available.
	StartNarrationAudio();
}

void USHBriefingSystem::SkipBriefing()
{
	if (!bIsBriefingActive)
	{
		return;
	}

	UE_LOG(LogSHBriefing, Log, TEXT("Briefing skipped: %s"), *ActiveBriefingData.MissionName.ToString());
	CompleteBriefing();
}

// -----------------------------------------------------------------------
//  Section progression
// -----------------------------------------------------------------------

void USHBriefingSystem::AdvanceSection()
{
	SectionTimer = 0.f;

	switch (CurrentSection)
	{
	case ESHBriefingSection::MissionOverview:
		if (ActiveBriefingData.ObjectiveList.Num() > 0)
		{
			CurrentSectionIndex = 0;
			EnterSection(ESHBriefingSection::Objectives);
		}
		else if (ActiveBriefingData.IntelNotes.Num() > 0)
		{
			CurrentSectionIndex = 0;
			EnterSection(ESHBriefingSection::IntelNotes);
		}
		else if (!ActiveBriefingData.MapOverlayTexture.IsNull())
		{
			CurrentSectionIndex = 0;
			EnterSection(ESHBriefingSection::MapOverlay);
		}
		else
		{
			CompleteBriefing();
		}
		break;

	case ESHBriefingSection::Objectives:
		CurrentSectionIndex++;
		if (CurrentSectionIndex < ActiveBriefingData.ObjectiveList.Num())
		{
			OnBriefingSectionAdvanced.Broadcast(ESHBriefingSection::Objectives, CurrentSectionIndex);
			UE_LOG(LogSHBriefing, Verbose, TEXT("Objective %d / %d"),
				CurrentSectionIndex + 1, ActiveBriefingData.ObjectiveList.Num());
		}
		else if (ActiveBriefingData.IntelNotes.Num() > 0)
		{
			CurrentSectionIndex = 0;
			EnterSection(ESHBriefingSection::IntelNotes);
		}
		else if (!ActiveBriefingData.MapOverlayTexture.IsNull())
		{
			CurrentSectionIndex = 0;
			EnterSection(ESHBriefingSection::MapOverlay);
		}
		else
		{
			CompleteBriefing();
		}
		break;

	case ESHBriefingSection::IntelNotes:
		CurrentSectionIndex++;
		if (CurrentSectionIndex < ActiveBriefingData.IntelNotes.Num())
		{
			OnBriefingSectionAdvanced.Broadcast(ESHBriefingSection::IntelNotes, CurrentSectionIndex);
			UE_LOG(LogSHBriefing, Verbose, TEXT("Intel note %d / %d"),
				CurrentSectionIndex + 1, ActiveBriefingData.IntelNotes.Num());
		}
		else if (!ActiveBriefingData.MapOverlayTexture.IsNull())
		{
			CurrentSectionIndex = 0;
			EnterSection(ESHBriefingSection::MapOverlay);
		}
		else
		{
			CompleteBriefing();
		}
		break;

	case ESHBriefingSection::MapOverlay:
		CompleteBriefing();
		break;

	default:
		CompleteBriefing();
		break;
	}
}

void USHBriefingSystem::EnterSection(ESHBriefingSection Section)
{
	CurrentSection = Section;
	SectionTimer = 0.f;

	OnBriefingSectionAdvanced.Broadcast(CurrentSection, CurrentSectionIndex);

	UE_LOG(LogSHBriefing, Log, TEXT("Entering briefing section: %d, index: %d"),
		static_cast<int32>(Section), CurrentSectionIndex);
}

void USHBriefingSystem::CompleteBriefing()
{
	if (!bIsBriefingActive)
	{
		return;
	}

	StopNarrationAudio();

	bIsBriefingActive = false;
	CurrentSection = ESHBriefingSection::Complete;
	CurrentSectionIndex = 0;
	SectionTimer = 0.f;

	SetComponentTickEnabled(false);

	UE_LOG(LogSHBriefing, Log, TEXT("Briefing completed: %s"), *ActiveBriefingData.MissionName.ToString());

	OnBriefingCompleted.Broadcast(ActiveBriefingData);
}

float USHBriefingSystem::GetCurrentSectionDuration() const
{
	switch (CurrentSection)
	{
	case ESHBriefingSection::MissionOverview:
		return ActiveBriefingData.OverviewDuration;

	case ESHBriefingSection::Objectives:
		return ActiveBriefingData.SecondsPerObjective;

	case ESHBriefingSection::IntelNotes:
		return ActiveBriefingData.SecondsPerIntelNote;

	case ESHBriefingSection::MapOverlay:
		return ActiveBriefingData.MapOverlayDuration;

	default:
		return 0.f;
	}
}

// -----------------------------------------------------------------------
//  Narration audio
// -----------------------------------------------------------------------

void USHBriefingSystem::StartNarrationAudio()
{
	if (ActiveBriefingData.NarrationAudio.IsNull())
	{
		return;
	}

	// Cancel any pending load.
	if (NarrationLoadHandle.IsValid())
	{
		NarrationLoadHandle->CancelHandle();
		NarrationLoadHandle.Reset();
	}

	// If already loaded, play immediately.
	if (USoundBase* LoadedSound = ActiveBriefingData.NarrationAudio.Get())
	{
		NarrationAudioComponent = UGameplayStatics::CreateSound2D(this, LoadedSound, 1.f, 1.f, 0.f, nullptr, false, false);
		if (NarrationAudioComponent)
		{
			NarrationAudioComponent->Play();
		}
		return;
	}

	// Async load the narration asset.
	FSoftObjectPath AssetPath = ActiveBriefingData.NarrationAudio.ToSoftObjectPath();
	if (AssetPath.IsValid())
	{
		NarrationLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
			AssetPath,
			FStreamableDelegate::CreateWeakLambda(this, [this]()
			{
				if (!bIsBriefingActive)
				{
					return;
				}

				USoundBase* LoadedSound = ActiveBriefingData.NarrationAudio.Get();
				if (!LoadedSound)
				{
					UE_LOG(LogSHBriefing, Warning, TEXT("Failed to load narration audio for briefing [%s]."),
						*ActiveBriefingData.MissionName.ToString());
					return;
				}

				NarrationAudioComponent = UGameplayStatics::CreateSound2D(this, LoadedSound, 1.f, 1.f, 0.f, nullptr, false, false);
				if (NarrationAudioComponent)
				{
					NarrationAudioComponent->Play();
				}
			})
		);
	}
}

void USHBriefingSystem::StopNarrationAudio()
{
	if (NarrationLoadHandle.IsValid())
	{
		NarrationLoadHandle->CancelHandle();
		NarrationLoadHandle.Reset();
	}

	if (NarrationAudioComponent && NarrationAudioComponent->IsPlaying())
	{
		NarrationAudioComponent->FadeOut(0.5f, 0.f);
	}
}
