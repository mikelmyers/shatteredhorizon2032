// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHPrimordiaReplayTimeline.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogSH_Timeline, Log, All);

// -----------------------------------------------------------------------
//  Constructor
// -----------------------------------------------------------------------

USHPrimordiaReplayTimeline::USHPrimordiaReplayTimeline()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// -----------------------------------------------------------------------
//  UActorComponent overrides
// -----------------------------------------------------------------------

void USHPrimordiaReplayTimeline::BeginPlay()
{
	Super::BeginPlay();
	Events.Empty();
	Events.Reserve(1024); // Pre-allocate a reasonable initial chunk
	UE_LOG(LogSH_Timeline, Verbose, TEXT("Replay timeline initialized on %s (max %d events)."),
		*GetOwner()->GetName(), MaxEvents);
}

// -----------------------------------------------------------------------
//  Recording
// -----------------------------------------------------------------------

void USHPrimordiaReplayTimeline::RecordEvent(const FSHAITimelineEvent& Event)
{
	// Enforce capacity limit — FIFO eviction
	if (Events.Num() >= MaxEvents)
	{
		Events.RemoveAt(0);
	}

	// Auto-stamp timestamp if not set
	FSHAITimelineEvent StampedEvent = Event;
	if (StampedEvent.Timestamp <= 0.0f)
	{
		const UWorld* World = GetWorld();
		StampedEvent.Timestamp = World ? World->GetTimeSeconds() : 0.0f;
	}

	// Insert in sorted order (most events arrive in-order, so this is
	// usually an O(1) append, but we handle out-of-order gracefully)
	if (Events.Num() == 0 || StampedEvent.Timestamp >= Events.Last().Timestamp)
	{
		Events.Add(StampedEvent);
	}
	else
	{
		// Binary search for insertion point
		int32 Lo = 0;
		int32 Hi = Events.Num();
		while (Lo < Hi)
		{
			const int32 Mid = (Lo + Hi) / 2;
			if (Events[Mid].Timestamp <= StampedEvent.Timestamp)
			{
				Lo = Mid + 1;
			}
			else
			{
				Hi = Mid;
			}
		}
		Events.Insert(StampedEvent, Lo);
	}
}

// -----------------------------------------------------------------------
//  Queries
// -----------------------------------------------------------------------

TArray<FSHAITimelineEvent> USHPrimordiaReplayTimeline::GetEventsInRange(float StartTime, float EndTime) const
{
	TArray<FSHAITimelineEvent> Result;

	if (Events.Num() == 0 || StartTime > EndTime)
	{
		return Result;
	}

	const int32 StartIdx = FindFirstEventAtOrAfter(StartTime);
	if (StartIdx == INDEX_NONE)
	{
		return Result;
	}

	for (int32 i = StartIdx; i < Events.Num(); ++i)
	{
		if (Events[i].Timestamp > EndTime)
		{
			break;
		}
		Result.Add(Events[i]);
	}

	return Result;
}

TArray<FVector> USHPrimordiaReplayTimeline::GetMovementPath(int32 ActorId) const
{
	TArray<FVector> Path;
	for (const FSHAITimelineEvent& Event : Events)
	{
		if (Event.ActorId == ActorId && Event.EventType == ESHAITimelineEventType::Movement)
		{
			Path.Add(Event.WorldLocation);
		}
	}
	return Path;
}

TArray<FSHAITimelineEvent> USHPrimordiaReplayTimeline::GetDecisionPoints(int32 ActorId) const
{
	TArray<FSHAITimelineEvent> Result;
	for (const FSHAITimelineEvent& Event : Events)
	{
		if (Event.ActorId == ActorId && Event.EventType == ESHAITimelineEventType::Decision)
		{
			Result.Add(Event);
		}
	}
	return Result;
}

TArray<FSHAITimelineEvent> USHPrimordiaReplayTimeline::GetEventsForActor(int32 ActorId) const
{
	TArray<FSHAITimelineEvent> Result;
	for (const FSHAITimelineEvent& Event : Events)
	{
		if (Event.ActorId == ActorId)
		{
			Result.Add(Event);
		}
	}
	return Result;
}

void USHPrimordiaReplayTimeline::GetTimeRange(float& OutStart, float& OutEnd) const
{
	if (Events.Num() == 0)
	{
		OutStart = 0.0f;
		OutEnd = 0.0f;
		return;
	}

	OutStart = Events[0].Timestamp;
	OutEnd = Events.Last().Timestamp;
}

// -----------------------------------------------------------------------
//  Export
// -----------------------------------------------------------------------

bool USHPrimordiaReplayTimeline::ExportTimeline(const FString& FilePath) const
{
	// Build JSON array
	TArray<TSharedPtr<FJsonValue>> JsonEvents;
	JsonEvents.Reserve(Events.Num());

	for (const FSHAITimelineEvent& Event : Events)
	{
		TSharedPtr<FJsonObject> EventObj = MakeShared<FJsonObject>();
		EventObj->SetNumberField(TEXT("timestamp"), Event.Timestamp);
		EventObj->SetNumberField(TEXT("actorId"), Event.ActorId);
		EventObj->SetStringField(TEXT("eventType"), UEnum::GetValueAsString(Event.EventType));
		EventObj->SetStringField(TEXT("description"), Event.Description);

		// Location as object
		TSharedPtr<FJsonObject> LocObj = MakeShared<FJsonObject>();
		LocObj->SetNumberField(TEXT("x"), Event.WorldLocation.X);
		LocObj->SetNumberField(TEXT("y"), Event.WorldLocation.Y);
		LocObj->SetNumberField(TEXT("z"), Event.WorldLocation.Z);
		EventObj->SetObjectField(TEXT("worldLocation"), LocObj);

		EventObj->SetStringField(TEXT("data"), Event.Data);

		JsonEvents.Add(MakeShared<FJsonValueObject>(EventObj));
	}

	// Wrap in root object
	TSharedPtr<FJsonObject> RootObj = MakeShared<FJsonObject>();
	RootObj->SetNumberField(TEXT("eventCount"), Events.Num());

	float StartTime = 0.0f, EndTime = 0.0f;
	GetTimeRange(StartTime, EndTime);
	RootObj->SetNumberField(TEXT("startTime"), StartTime);
	RootObj->SetNumberField(TEXT("endTime"), EndTime);
	RootObj->SetArrayField(TEXT("events"), JsonEvents);

	// Serialize to string
	FString OutputString;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutputString);
	if (!FJsonSerializer::Serialize(RootObj.ToSharedRef(), Writer))
	{
		UE_LOG(LogSH_Timeline, Error, TEXT("Failed to serialize timeline to JSON."));
		return false;
	}

	// Write to file
	if (!FFileHelper::SaveStringToFile(OutputString, *FilePath))
	{
		UE_LOG(LogSH_Timeline, Error, TEXT("Failed to write timeline to file: %s"), *FilePath);
		return false;
	}

	UE_LOG(LogSH_Timeline, Log, TEXT("Timeline exported to %s (%d events)."), *FilePath, Events.Num());
	return true;
}

// -----------------------------------------------------------------------
//  Management
// -----------------------------------------------------------------------

void USHPrimordiaReplayTimeline::ClearTimeline()
{
	Events.Empty();
	UE_LOG(LogSH_Timeline, Log, TEXT("Timeline cleared."));
}

// -----------------------------------------------------------------------
//  Internal
// -----------------------------------------------------------------------

int32 USHPrimordiaReplayTimeline::FindFirstEventAtOrAfter(float Time) const
{
	if (Events.Num() == 0)
	{
		return INDEX_NONE;
	}

	// Binary search for the first event with Timestamp >= Time
	int32 Lo = 0;
	int32 Hi = Events.Num();
	while (Lo < Hi)
	{
		const int32 Mid = (Lo + Hi) / 2;
		if (Events[Mid].Timestamp < Time)
		{
			Lo = Mid + 1;
		}
		else
		{
			Hi = Mid;
		}
	}

	return Lo < Events.Num() ? Lo : INDEX_NONE;
}
