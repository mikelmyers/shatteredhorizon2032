// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SHPrimordiaDebugOverlay.generated.h"

/* -----------------------------------------------------------------------
 *  Structs
 * --------------------------------------------------------------------- */

USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHAIDebugEntry
{
	GENERATED_BODY()

	/** Name of the AI subsystem that produced this entry (e.g. "Aletheia", "Simulon"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	FName SubsystemName;

	/** Description of the input state that led to the decision. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	FString InputState;

	/** Description of the output decision. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	FString OutputDecision;

	/** Confidence level of the decision (0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug", meta = (ClampMin = "0", ClampMax = "1"))
	float Confidence = 0.0f;

	/** Game-time when this entry was recorded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	float Timestamp = 0.0f;
};

/* -----------------------------------------------------------------------
 *  USHPrimordiaDebugOverlay
 * --------------------------------------------------------------------- */

/**
 * USHPrimordiaDebugOverlay
 *
 * Debug overlay component for AI units. Records AI decision entries
 * from all Primordia subsystems into a fixed-size ring buffer for
 * real-time inspection during gameplay or spectator mode.
 *
 * Performance target: < 0.1ms overhead when recording.
 * Max 100 entries in ring buffer.
 */
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHPrimordiaDebugOverlay : public UActorComponent
{
	GENERATED_BODY()

public:
	USHPrimordiaDebugOverlay();

	// ------------------------------------------------------------------
	//  UActorComponent overrides
	// ------------------------------------------------------------------
	virtual void BeginPlay() override;

	// ------------------------------------------------------------------
	//  Recording
	// ------------------------------------------------------------------

	/**
	 * Record an AI decision entry. Stamps the timestamp automatically.
	 * Performance: O(1) ring buffer insertion, < 0.1ms.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Debug")
	void RecordDecision(const FSHAIDebugEntry& Entry);

	// ------------------------------------------------------------------
	//  Queries
	// ------------------------------------------------------------------

	/**
	 * Get the most recent N entries from the ring buffer.
	 * Returns entries in reverse chronological order (newest first).
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Debug")
	TArray<FSHAIDebugEntry> GetRecentDecisions(int32 Count) const;

	/** Get total number of decisions recorded (including evicted ones). */
	UFUNCTION(BlueprintPure, Category = "SH|Primordia|Debug")
	int32 GetTotalRecordedCount() const { return TotalRecordedCount; }

	/** Get current number of entries in the buffer. */
	UFUNCTION(BlueprintPure, Category = "SH|Primordia|Debug")
	int32 GetBufferCount() const { return FMath::Min(TotalRecordedCount, MaxEntries); }

	// ------------------------------------------------------------------
	//  Overlay visibility
	// ------------------------------------------------------------------

	/** Toggle the debug overlay visibility. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Debug")
	void SetOverlayVisible(bool bVisible);

	UFUNCTION(BlueprintPure, Category = "SH|Primordia|Debug")
	bool IsOverlayVisible() const { return bOverlayVisible; }

protected:
	/** Maximum entries in the ring buffer. */
	static constexpr int32 MaxEntries = 100;

private:
	/** Ring buffer storage. */
	FSHAIDebugEntry RingBuffer[MaxEntries];

	/** Current write index in the ring buffer. */
	int32 WriteIndex = 0;

	/** Total number of entries ever recorded (may exceed MaxEntries). */
	int32 TotalRecordedCount = 0;

	/** Whether the debug overlay is displayed. */
	bool bOverlayVisible = false;
};
