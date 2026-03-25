// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHSquadPersonality.h"

#include "Components/AudioComponent.h"
#include "Engine/StreamableManager.h"
#include "Engine/AssetManager.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogSHSquadPersonality);

// -----------------------------------------------------------------------
//  Constructor
// -----------------------------------------------------------------------

USHSquadPersonality::USHSquadPersonality()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// -----------------------------------------------------------------------
//  Lifecycle
// -----------------------------------------------------------------------

void USHSquadPersonality::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogSHSquadPersonality, Log, TEXT("Squad personality initialized: %s (%s) — Trait: %d, VoiceSet: %d"),
		*Profile.Name.ToString(), *Profile.Callsign.ToString(),
		static_cast<int32>(Profile.PersonalityTrait), Profile.VoiceSet);
}

// -----------------------------------------------------------------------
//  Profile
// -----------------------------------------------------------------------

void USHSquadPersonality::SetProfile(const FSHPersonalityProfile& InProfile)
{
	Profile = InProfile;

	UE_LOG(LogSHSquadPersonality, Log, TEXT("Profile set: %s (%s), Rank: %s, Trait: %d"),
		*Profile.Name.ToString(), *Profile.Callsign.ToString(),
		*Profile.Rank.ToString(), static_cast<int32>(Profile.PersonalityTrait));
}

// -----------------------------------------------------------------------
//  Relationship tracking
// -----------------------------------------------------------------------

void USHSquadPersonality::ModifyRelationship(float Delta)
{
	if (bIsKilled)
	{
		return;
	}

	const float OldScore = Profile.RelationshipScore;
	Profile.RelationshipScore = FMath::Clamp(Profile.RelationshipScore + Delta, 0.f, 1.f);

	if (!FMath::IsNearlyEqual(OldScore, Profile.RelationshipScore))
	{
		UE_LOG(LogSHSquadPersonality, Verbose, TEXT("%s relationship: %.2f -> %.2f (delta: %+.2f)"),
			*Profile.Callsign.ToString(), OldScore, Profile.RelationshipScore, Delta);

		OnRelationshipChanged.Broadcast(this, OldScore, Profile.RelationshipScore);
	}
}

void USHSquadPersonality::RecordSuccessfulOperation()
{
	UE_LOG(LogSHSquadPersonality, Log, TEXT("%s: Successful operation recorded."), *Profile.Callsign.ToString());
	ModifyRelationship(SuccessfulOperationDelta);
}

void USHSquadPersonality::RecordFriendlyFire()
{
	UE_LOG(LogSHSquadPersonality, Warning, TEXT("%s: Friendly fire incident recorded!"), *Profile.Callsign.ToString());
	ModifyRelationship(FriendlyFirePenalty);
}

void USHSquadPersonality::RecordFailedOrder()
{
	UE_LOG(LogSHSquadPersonality, Log, TEXT("%s: Failed order recorded."), *Profile.Callsign.ToString());
	ModifyRelationship(FailedOrderPenalty);
}

// -----------------------------------------------------------------------
//  Voice lines
// -----------------------------------------------------------------------

bool USHSquadPersonality::PlayContextualVoiceLine(ESHVoiceLineContext Context)
{
	if (bIsKilled)
	{
		return false;
	}

	if (VoiceCooldownRemaining > 0.f)
	{
		return false;
	}

	TSoftObjectPtr<USoundBase> SelectedLine = SelectVoiceLine(Context);
	if (SelectedLine.IsNull())
	{
		UE_LOG(LogSHSquadPersonality, Verbose, TEXT("%s: No voice line available for context %d."),
			*Profile.Callsign.ToString(), static_cast<int32>(Context));
		return false;
	}

	LoadAndPlaySound(SelectedLine);

	// Start cooldown.
	VoiceCooldownRemaining = VoiceLineCooldown;

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			CooldownTimerHandle,
			[this]()
			{
				VoiceCooldownRemaining = 0.f;
			},
			VoiceLineCooldown,
			false
		);
	}

	UE_LOG(LogSHSquadPersonality, Verbose, TEXT("%s: Playing voice line for context %d."),
		*Profile.Callsign.ToString(), static_cast<int32>(Context));

	return true;
}

TArray<TSoftObjectPtr<USoundBase>> USHSquadPersonality::GetVoiceLinesForContext(ESHVoiceLineContext Context) const
{
	if (const TArray<TSoftObjectPtr<USoundBase>>* Lines = VoiceLineSets.Find(Context))
	{
		return *Lines;
	}
	return TArray<TSoftObjectPtr<USoundBase>>();
}

TSoftObjectPtr<USoundBase> USHSquadPersonality::SelectVoiceLine(ESHVoiceLineContext Context) const
{
	const TArray<TSoftObjectPtr<USoundBase>>* Lines = VoiceLineSets.Find(Context);
	if (!Lines || Lines->Num() == 0)
	{
		return TSoftObjectPtr<USoundBase>();
	}

	// Personality-driven selection strategy:
	// - Calm / Cautious: prefer the first half of the array (measured, composed lines).
	// - Aggressive: prefer the second half (intense, urgent lines).
	// - Joker: fully random across all lines.
	// - Veteran: weighted toward the middle (experienced, matter-of-fact).
	// This convention assumes designers order lines from calm -> intense within each array.

	const int32 LineCount = Lines->Num();

	if (LineCount == 1)
	{
		return (*Lines)[0];
	}

	int32 SelectedIndex = 0;

	switch (Profile.PersonalityTrait)
	{
	case ESHPersonalityTrait::Calm:
	case ESHPersonalityTrait::Cautious:
	{
		// Bias toward the first half.
		const int32 HalfCount = FMath::Max(1, LineCount / 2);
		SelectedIndex = FMath::RandRange(0, HalfCount - 1);
		break;
	}

	case ESHPersonalityTrait::Aggressive:
	{
		// Bias toward the second half.
		const int32 MidPoint = LineCount / 2;
		SelectedIndex = FMath::RandRange(MidPoint, LineCount - 1);
		break;
	}

	case ESHPersonalityTrait::Joker:
	{
		// Fully random.
		SelectedIndex = FMath::RandRange(0, LineCount - 1);
		break;
	}

	case ESHPersonalityTrait::Veteran:
	{
		// Weighted toward the center.
		const int32 QuarterCount = FMath::Max(1, LineCount / 4);
		const int32 Start = QuarterCount;
		const int32 End = FMath::Min(LineCount - 1, LineCount - QuarterCount - 1);
		SelectedIndex = (Start <= End) ? FMath::RandRange(Start, End) : FMath::RandRange(0, LineCount - 1);
		break;
	}

	default:
		SelectedIndex = FMath::RandRange(0, LineCount - 1);
		break;
	}

	return (*Lines)[SelectedIndex];
}

// -----------------------------------------------------------------------
//  Audio playback
// -----------------------------------------------------------------------

void USHSquadPersonality::LoadAndPlaySound(const TSoftObjectPtr<USoundBase>& SoundRef)
{
	// Cancel any pending load.
	if (VoiceLoadHandle.IsValid())
	{
		VoiceLoadHandle->CancelHandle();
		VoiceLoadHandle.Reset();
	}

	// Stop any currently playing voice line.
	if (VoiceAudioComponent && VoiceAudioComponent->IsPlaying())
	{
		VoiceAudioComponent->Stop();
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// If already loaded, play immediately.
	if (USoundBase* LoadedSound = SoundRef.Get())
	{
		if (!VoiceAudioComponent)
		{
			VoiceAudioComponent = UGameplayStatics::SpawnSoundAttached(
				LoadedSound, Owner->GetRootComponent(), NAME_None,
				FVector::ZeroVector, EAttachLocation::KeepRelativeOffset,
				false, 1.f, 1.f, 0.f, nullptr, nullptr, false);
		}
		else
		{
			VoiceAudioComponent->SetSound(LoadedSound);
			VoiceAudioComponent->Play();
		}
		return;
	}

	// Async load.
	FSoftObjectPath AssetPath = SoundRef.ToSoftObjectPath();
	if (!AssetPath.IsValid())
	{
		return;
	}

	VoiceLoadHandle = UAssetManager::GetStreamableManager().RequestAsyncLoad(
		AssetPath,
		FStreamableDelegate::CreateWeakLambda(this, [this, SoundRef]()
		{
			USoundBase* LoadedSound = SoundRef.Get();
			if (!LoadedSound)
			{
				UE_LOG(LogSHSquadPersonality, Warning, TEXT("%s: Failed to load voice line audio."),
					*Profile.Callsign.ToString());
				return;
			}

			AActor* Owner = GetOwner();
			if (!Owner)
			{
				return;
			}

			if (!VoiceAudioComponent)
			{
				VoiceAudioComponent = UGameplayStatics::SpawnSoundAttached(
					LoadedSound, Owner->GetRootComponent(), NAME_None,
					FVector::ZeroVector, EAttachLocation::KeepRelativeOffset,
					false, 1.f, 1.f, 0.f, nullptr, nullptr, false);
			}
			else
			{
				VoiceAudioComponent->SetSound(LoadedSound);
				VoiceAudioComponent->Play();
			}
		})
	);
}

// -----------------------------------------------------------------------
//  Death
// -----------------------------------------------------------------------

void USHSquadPersonality::NotifyKilled()
{
	if (bIsKilled)
	{
		return;
	}

	bIsKilled = true;

	// Stop any playing voice line.
	if (VoiceAudioComponent && VoiceAudioComponent->IsPlaying())
	{
		VoiceAudioComponent->Stop();
	}

	// Cancel pending loads.
	if (VoiceLoadHandle.IsValid())
	{
		VoiceLoadHandle->CancelHandle();
		VoiceLoadHandle.Reset();
	}

	// Clear cooldown timer.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(CooldownTimerHandle);
	}

	UE_LOG(LogSHSquadPersonality, Warning, TEXT("Squad member KILLED: %s (%s), Rank: %s, Relationship: %.2f"),
		*Profile.Name.ToString(), *Profile.Callsign.ToString(),
		*Profile.Rank.ToString(), Profile.RelationshipScore);

	OnSquadMemberKilled.Broadcast(this, Profile);
}
