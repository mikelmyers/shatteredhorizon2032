// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHPrimordiaMnemonic.h"
#include "GameFramework/SaveGame.h"
#include "Kismet/GameplayStatics.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

DEFINE_LOG_CATEGORY_STATIC(LogSH_Mnemonic, Log, All);

// -----------------------------------------------------------------------
//  Simple save game wrapper
// -----------------------------------------------------------------------

UCLASS()
class USHMnemonicSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY()
	FSHPlayerBehaviorProfile Profile;

	/** Serialized action buffer for carry-over between sessions. */
	UPROPERTY()
	TArray<FVector> ActionLocations;

	UPROPERTY()
	TArray<FName> ActionTypes;

	UPROPERTY()
	TArray<float> ActionTimestamps;
};

// -----------------------------------------------------------------------
//  Subsystem lifecycle
// -----------------------------------------------------------------------

void USHPrimordiaMnemonic::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadProfile();
	UE_LOG(LogSH_Mnemonic, Log, TEXT("Mnemonic subsystem initialized. %d prior actions loaded."),
		RecordedActions.Num());
}

void USHPrimordiaMnemonic::Deinitialize()
{
	SaveProfile();
	Super::Deinitialize();
}

// -----------------------------------------------------------------------
//  Action recording
// -----------------------------------------------------------------------

void USHPrimordiaMnemonic::RecordPlayerAction(FName ActionType, FVector Location, float Timestamp)
{
	FRecordedAction Action;
	Action.ActionType = ActionType;
	Action.Location = Location;
	Action.Timestamp = Timestamp;
	RecordedActions.Add(MoveTemp(Action));

	// FIFO eviction
	if (RecordedActions.Num() > MaxRecordedActions)
	{
		RecordedActions.RemoveAt(0, RecordedActions.Num() - MaxRecordedActions);
	}
}

// -----------------------------------------------------------------------
//  Analysis
// -----------------------------------------------------------------------

void USHPrimordiaMnemonic::AnalyzePlayerBehavior()
{
	if (RecordedActions.Num() < MinActionsForAnalysis)
	{
		UE_LOG(LogSH_Mnemonic, Log,
			TEXT("Not enough actions for analysis (%d / %d required)."),
			RecordedActions.Num(), MinActionsForAnalysis);
		return;
	}

	const int32 TotalActions = RecordedActions.Num();
	CurrentProfile.SampleCount = TotalActions;

	// --- Aggression level ---
	// High aggression = lots of Engage, Rush, Attack actions relative to total
	const int32 AggressiveActions =
		CountActions(FName("Engage")) +
		CountActions(FName("Rush")) +
		CountActions(FName("Attack"));
	CurrentProfile.AggressionLevel = FMath::Clamp(
		static_cast<float>(AggressiveActions) / static_cast<float>(TotalActions) * 2.0f,
		0.0f, 1.0f);

	// --- Flank frequency ---
	const int32 FlankActions = CountActions(FName("Flank"));
	CurrentProfile.FlankFrequency = FMath::Clamp(
		static_cast<float>(FlankActions) / static_cast<float>(TotalActions) * 3.0f,
		0.0f, 1.0f);

	// --- Sniper tendency ---
	const int32 SniperActions = CountActions(FName("Snipe"));
	CurrentProfile.SniperTendency = FMath::Clamp(
		static_cast<float>(SniperActions) / static_cast<float>(TotalActions) * 4.0f,
		0.0f, 1.0f);

	// --- Rush tendency ---
	const int32 RushActions = CountActions(FName("Rush"));
	CurrentProfile.RushTendency = FMath::Clamp(
		static_cast<float>(RushActions) / static_cast<float>(TotalActions) * 3.0f,
		0.0f, 1.0f);

	// --- Average engagement range ---
	CurrentProfile.AverageEngagementRange = ComputeAverageEngagementRange();

	// --- Preferred approach routes ---
	// Gather locations of Move and Flank actions as approach route indicators
	CurrentProfile.PreferredApproachRoute.Empty();
	for (int32 i = FMath::Max(0, RecordedActions.Num() - MaxApproachRoutePoints * 2);
		 i < RecordedActions.Num(); ++i)
	{
		const FRecordedAction& Act = RecordedActions[i];
		if (Act.ActionType == FName("Move") || Act.ActionType == FName("Flank"))
		{
			CurrentProfile.PreferredApproachRoute.Add(Act.Location);
			if (CurrentProfile.PreferredApproachRoute.Num() >= MaxApproachRoutePoints)
			{
				break;
			}
		}
	}

	OnProfileUpdated.Broadcast(CurrentProfile);

	UE_LOG(LogSH_Mnemonic, Log,
		TEXT("Player behavior analyzed (%d samples): Aggression=%.2f Flank=%.2f Sniper=%.2f Rush=%.2f AvgRange=%.0f"),
		TotalActions, CurrentProfile.AggressionLevel, CurrentProfile.FlankFrequency,
		CurrentProfile.SniperTendency, CurrentProfile.RushTendency,
		CurrentProfile.AverageEngagementRange);
}

FSHCounterStrategyRecommendation USHPrimordiaMnemonic::GetCounterStrategy() const
{
	FSHCounterStrategyRecommendation Rec;

	if (CurrentProfile.SampleCount < MinActionsForAnalysis)
	{
		Rec.Strategy = ESHCounterStrategy::None;
		Rec.Reasoning = TEXT("Insufficient player data for counter-strategy recommendation.");
		Rec.Confidence = 0.0f;
		return Rec;
	}

	// Select counter-strategy based on dominant player behavior
	if (CurrentProfile.RushTendency > 0.6f)
	{
		Rec.Strategy = ESHCounterStrategy::SetAmbush;
		Rec.Reasoning = FString::Printf(
			TEXT("Player rushes frequently (%.0f%%). Set ambushes along predicted approach routes."),
			CurrentProfile.RushTendency * 100.0f);
		Rec.Confidence = FMath::Min(CurrentProfile.RushTendency, 0.9f);
	}
	else if (CurrentProfile.SniperTendency > 0.5f)
	{
		Rec.Strategy = ESHCounterStrategy::CloseQuarters;
		Rec.Reasoning = FString::Printf(
			TEXT("Player favors long-range engagements (%.0f%%). Close distance to negate advantage. Avg range: %.0fcm."),
			CurrentProfile.SniperTendency * 100.0f, CurrentProfile.AverageEngagementRange);
		Rec.Confidence = FMath::Min(CurrentProfile.SniperTendency, 0.85f);
	}
	else if (CurrentProfile.FlankFrequency > 0.5f)
	{
		Rec.Strategy = ESHCounterStrategy::DefensiveHold;
		Rec.Reasoning = FString::Printf(
			TEXT("Player flanks frequently (%.0f%%). Establish all-around security and hold positions."),
			CurrentProfile.FlankFrequency * 100.0f);
		Rec.Confidence = FMath::Min(CurrentProfile.FlankFrequency, 0.85f);
	}
	else if (CurrentProfile.AggressionLevel > 0.6f)
	{
		Rec.Strategy = ESHCounterStrategy::SuppressAndMove;
		Rec.Reasoning = FString::Printf(
			TEXT("Player is highly aggressive (%.0f%%). Use suppression to pin then maneuver."),
			CurrentProfile.AggressionLevel * 100.0f);
		Rec.Confidence = FMath::Min(CurrentProfile.AggressionLevel * 0.8f, 0.85f);
	}
	else if (CurrentProfile.AggressionLevel < 0.3f)
	{
		Rec.Strategy = ESHCounterStrategy::AggressivePush;
		Rec.Reasoning = FString::Printf(
			TEXT("Player is passive (aggression %.0f%%). Press the attack to exploit caution."),
			CurrentProfile.AggressionLevel * 100.0f);
		Rec.Confidence = FMath::Min((1.0f - CurrentProfile.AggressionLevel) * 0.7f, 0.8f);
	}
	else
	{
		Rec.Strategy = ESHCounterStrategy::ForceFlank;
		Rec.Reasoning = TEXT("No dominant player pattern detected. Force flanking to create asymmetry.");
		Rec.Confidence = 0.4f;
	}

	return Rec;
}

// -----------------------------------------------------------------------
//  Persistence
// -----------------------------------------------------------------------

void USHPrimordiaMnemonic::SaveProfile()
{
	USHMnemonicSaveGame* SaveGame = Cast<USHMnemonicSaveGame>(
		UGameplayStatics::CreateSaveGameObject(USHMnemonicSaveGame::StaticClass()));

	if (!SaveGame)
	{
		return;
	}

	SaveGame->Profile = CurrentProfile;

	// Serialize the last N actions for cross-session continuity
	const int32 ActionsToSave = FMath::Min(RecordedActions.Num(), MaxRecordedActions);
	SaveGame->ActionLocations.Reserve(ActionsToSave);
	SaveGame->ActionTypes.Reserve(ActionsToSave);
	SaveGame->ActionTimestamps.Reserve(ActionsToSave);

	for (int32 i = RecordedActions.Num() - ActionsToSave; i < RecordedActions.Num(); ++i)
	{
		SaveGame->ActionLocations.Add(RecordedActions[i].Location);
		SaveGame->ActionTypes.Add(RecordedActions[i].ActionType);
		SaveGame->ActionTimestamps.Add(RecordedActions[i].Timestamp);
	}

	if (UGameplayStatics::SaveGameToSlot(SaveGame, SaveSlotName, 0))
	{
		UE_LOG(LogSH_Mnemonic, Log, TEXT("Player behavior profile saved (%d actions)."), ActionsToSave);
	}
	else
	{
		UE_LOG(LogSH_Mnemonic, Error, TEXT("Failed to save player behavior profile."));
	}
}

void USHPrimordiaMnemonic::LoadProfile()
{
	USHMnemonicSaveGame* SaveGame = Cast<USHMnemonicSaveGame>(
		UGameplayStatics::LoadGameFromSlot(SaveSlotName, 0));

	if (!SaveGame)
	{
		UE_LOG(LogSH_Mnemonic, Log, TEXT("No existing player behavior profile found. Starting fresh."));
		return;
	}

	CurrentProfile = SaveGame->Profile;

	// Restore recorded actions
	RecordedActions.Empty();
	const int32 Count = FMath::Min3(
		SaveGame->ActionLocations.Num(),
		SaveGame->ActionTypes.Num(),
		SaveGame->ActionTimestamps.Num());

	RecordedActions.Reserve(Count);
	for (int32 i = 0; i < Count; ++i)
	{
		FRecordedAction Action;
		Action.Location = SaveGame->ActionLocations[i];
		Action.ActionType = SaveGame->ActionTypes[i];
		Action.Timestamp = SaveGame->ActionTimestamps[i];
		RecordedActions.Add(MoveTemp(Action));
	}

	UE_LOG(LogSH_Mnemonic, Log, TEXT("Loaded player behavior profile (%d actions, %d samples)."),
		RecordedActions.Num(), CurrentProfile.SampleCount);
}

// -----------------------------------------------------------------------
//  Internal helpers
// -----------------------------------------------------------------------

int32 USHPrimordiaMnemonic::CountActions(FName ActionType) const
{
	int32 Count = 0;
	for (const FRecordedAction& Action : RecordedActions)
	{
		if (Action.ActionType == ActionType)
		{
			Count++;
		}
	}
	return Count;
}

FVector USHPrimordiaMnemonic::AverageLocationForType(FName ActionType) const
{
	FVector Sum = FVector::ZeroVector;
	int32 Count = 0;
	for (const FRecordedAction& Action : RecordedActions)
	{
		if (Action.ActionType == ActionType)
		{
			Sum += Action.Location;
			Count++;
		}
	}
	return Count > 0 ? Sum / static_cast<float>(Count) : FVector::ZeroVector;
}

float USHPrimordiaMnemonic::ComputeAverageEngagementRange() const
{
	// Engagement range approximated from distance between consecutive
	// Engage/Attack actions and prior Move actions
	float TotalRange = 0.0f;
	int32 RangeSamples = 0;

	FVector LastMoveLocation = FVector::ZeroVector;
	bool bHasLastMove = false;

	for (const FRecordedAction& Action : RecordedActions)
	{
		if (Action.ActionType == FName("Move"))
		{
			LastMoveLocation = Action.Location;
			bHasLastMove = true;
		}
		else if (bHasLastMove &&
			(Action.ActionType == FName("Engage") ||
			 Action.ActionType == FName("Attack") ||
			 Action.ActionType == FName("Snipe")))
		{
			const float Range = FVector::Dist(LastMoveLocation, Action.Location);
			if (Range > 100.0f) // Ignore very short ranges (likely same position)
			{
				TotalRange += Range;
				RangeSamples++;
			}
		}
	}

	return RangeSamples > 0 ? TotalRange / static_cast<float>(RangeSamples) : 5000.0f;
}
