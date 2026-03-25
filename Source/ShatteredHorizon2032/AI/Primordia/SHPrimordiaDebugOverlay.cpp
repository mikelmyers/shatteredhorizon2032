// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHPrimordiaDebugOverlay.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogSH_DebugOverlay, Log, All);

// -----------------------------------------------------------------------
//  Constructor
// -----------------------------------------------------------------------

USHPrimordiaDebugOverlay::USHPrimordiaDebugOverlay()
{
	// No tick required — recording is purely reactive
	PrimaryComponentTick.bCanEverTick = false;
}

// -----------------------------------------------------------------------
//  UActorComponent overrides
// -----------------------------------------------------------------------

void USHPrimordiaDebugOverlay::BeginPlay()
{
	Super::BeginPlay();

	// Zero-initialize the ring buffer
	for (int32 i = 0; i < MaxEntries; ++i)
	{
		RingBuffer[i] = FSHAIDebugEntry();
	}

	WriteIndex = 0;
	TotalRecordedCount = 0;

	UE_LOG(LogSH_DebugOverlay, Verbose, TEXT("Debug overlay initialized on %s (ring buffer: %d entries)."),
		*GetOwner()->GetName(), MaxEntries);
}

// -----------------------------------------------------------------------
//  Recording
// -----------------------------------------------------------------------

void USHPrimordiaDebugOverlay::RecordDecision(const FSHAIDebugEntry& Entry)
{
	// Auto-stamp timestamp if not already set
	FSHAIDebugEntry StampedEntry = Entry;
	if (StampedEntry.Timestamp <= 0.0f)
	{
		const UWorld* World = GetWorld();
		StampedEntry.Timestamp = World ? World->GetTimeSeconds() : 0.0f;
	}

	// O(1) ring buffer insertion
	RingBuffer[WriteIndex] = StampedEntry;
	WriteIndex = (WriteIndex + 1) % MaxEntries;
	TotalRecordedCount++;
}

// -----------------------------------------------------------------------
//  Queries
// -----------------------------------------------------------------------

TArray<FSHAIDebugEntry> USHPrimordiaDebugOverlay::GetRecentDecisions(int32 Count) const
{
	TArray<FSHAIDebugEntry> Result;

	const int32 Available = FMath::Min(TotalRecordedCount, MaxEntries);
	const int32 ToReturn = FMath::Clamp(Count, 0, Available);

	if (ToReturn == 0)
	{
		return Result;
	}

	Result.Reserve(ToReturn);

	// Read backwards from the most recent entry
	for (int32 i = 0; i < ToReturn; ++i)
	{
		int32 ReadIndex = (WriteIndex - 1 - i + MaxEntries) % MaxEntries;
		Result.Add(RingBuffer[ReadIndex]);
	}

	return Result;
}

// -----------------------------------------------------------------------
//  Overlay visibility
// -----------------------------------------------------------------------

void USHPrimordiaDebugOverlay::SetOverlayVisible(bool bVisible)
{
	bOverlayVisible = bVisible;
	UE_LOG(LogSH_DebugOverlay, Log, TEXT("[%s] Debug overlay %s."),
		*GetOwner()->GetName(), bVisible ? TEXT("shown") : TEXT("hidden"));
}
