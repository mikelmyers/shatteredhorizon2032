// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SHPrimordiaReplayTimeline.generated.h"

/* -----------------------------------------------------------------------
 *  Enums
 * --------------------------------------------------------------------- */

UENUM(BlueprintType)
enum class ESHAITimelineEventType : uint8
{
	Decision      UMETA(DisplayName = "Decision"),
	Movement      UMETA(DisplayName = "Movement"),
	Combat        UMETA(DisplayName = "Combat"),
	Communication UMETA(DisplayName = "Communication")
};

/* -----------------------------------------------------------------------
 *  Structs
 * --------------------------------------------------------------------- */

USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHAITimelineEvent
{
	GENERATED_BODY()

	/** Game-time when this event occurred. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timeline")
	float Timestamp = 0.0f;

	/** Unique actor identifier for grouping events per AI unit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timeline")
	int32 ActorId = 0;

	/** Classification of this event. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timeline")
	ESHAITimelineEventType EventType = ESHAITimelineEventType::Decision;

	/** Human-readable description of the event. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timeline")
	FString Description;

	/** World-space location where the event occurred. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timeline")
	FVector WorldLocation = FVector::ZeroVector;

	/** Arbitrary JSON data payload for detailed analysis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timeline")
	FString Data;
};

/* -----------------------------------------------------------------------
 *  USHPrimordiaReplayTimeline
 * --------------------------------------------------------------------- */

/**
 * USHPrimordiaReplayTimeline
 *
 * Records AI events into a timeline buffer for post-action review,
 * replay scrubbing, and offline analysis. Supports filtering by
 * time range, actor, and event type. Can export the full timeline
 * to JSON for external analysis tools.
 *
 * Max 10,000 events per timeline.
 */
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHPrimordiaReplayTimeline : public UActorComponent
{
	GENERATED_BODY()

public:
	USHPrimordiaReplayTimeline();

	// ------------------------------------------------------------------
	//  UActorComponent overrides
	// ------------------------------------------------------------------
	virtual void BeginPlay() override;

	// ------------------------------------------------------------------
	//  Recording
	// ------------------------------------------------------------------

	/** Record an event into the timeline. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Timeline")
	void RecordEvent(const FSHAITimelineEvent& Event);

	// ------------------------------------------------------------------
	//  Queries
	// ------------------------------------------------------------------

	/**
	 * Get all events within a time range (inclusive).
	 * Used for replay scrubbing.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Timeline")
	TArray<FSHAITimelineEvent> GetEventsInRange(float StartTime, float EndTime) const;

	/**
	 * Get the movement path for a specific actor as a sequence of world locations.
	 * Extracted from Movement-type events.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Timeline")
	TArray<FVector> GetMovementPath(int32 ActorId) const;

	/**
	 * Get decision point locations and timestamps for a specific actor.
	 * Extracted from Decision-type events.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Timeline")
	TArray<FSHAITimelineEvent> GetDecisionPoints(int32 ActorId) const;

	/**
	 * Get all events for a specific actor.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Timeline")
	TArray<FSHAITimelineEvent> GetEventsForActor(int32 ActorId) const;

	/** Get total event count. */
	UFUNCTION(BlueprintPure, Category = "SH|Primordia|Timeline")
	int32 GetEventCount() const { return Events.Num(); }

	/** Get the time range covered by the timeline. */
	UFUNCTION(BlueprintPure, Category = "SH|Primordia|Timeline")
	void GetTimeRange(float& OutStart, float& OutEnd) const;

	// ------------------------------------------------------------------
	//  Export
	// ------------------------------------------------------------------

	/**
	 * Export the full timeline to a JSON file for external analysis.
	 * @param FilePath  Full file path to write (e.g. "C:/Output/timeline.json").
	 * @return          True if the export succeeded.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Timeline")
	bool ExportTimeline(const FString& FilePath) const;

	// ------------------------------------------------------------------
	//  Management
	// ------------------------------------------------------------------

	/** Clear all recorded events. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Timeline")
	void ClearTimeline();

protected:
	/** Maximum number of events per timeline. */
	static constexpr int32 MaxEvents = 10000;

private:
	/** All recorded events, sorted by timestamp. */
	TArray<FSHAITimelineEvent> Events;

	/** Binary search helper for finding events by timestamp. */
	int32 FindFirstEventAtOrAfter(float Time) const;
};
