// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SHBriefingSystem.generated.h"

class UAudioComponent;
class USoundBase;
class UTexture2D;

DECLARE_LOG_CATEGORY_EXTERN(LogSHBriefing, Log, All);

// ─────────────────────────────────────────────────────────────
//  Enums
// ─────────────────────────────────────────────────────────────

/** Section types within a briefing, played sequentially. */
UENUM(BlueprintType)
enum class ESHBriefingSection : uint8
{
	MissionOverview		UMETA(DisplayName = "Mission Overview"),
	Objectives			UMETA(DisplayName = "Objectives"),
	IntelNotes			UMETA(DisplayName = "Intel Notes"),
	MapOverlay			UMETA(DisplayName = "Map Overlay"),
	Complete			UMETA(DisplayName = "Complete")
};

// ─────────────────────────────────────────────────────────────
//  Structs
// ─────────────────────────────────────────────────────────────

/** Complete data for a pre-mission briefing. */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHBriefingData
{
	GENERATED_BODY()

	/** Display name of the mission. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Briefing")
	FText MissionName;

	/** Main briefing narrative text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Briefing")
	FText BriefingText;

	/** Ordered list of mission objectives. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Briefing")
	TArray<FText> ObjectiveList;

	/** Supplementary intel notes (intercepted comms, recon data, etc.). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Briefing")
	TArray<FText> IntelNotes;

	/** Narration voice-over audio for the briefing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Briefing")
	TSoftObjectPtr<USoundBase> NarrationAudio;

	/** Tactical map overlay texture shown during briefing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Briefing")
	TSoftObjectPtr<UTexture2D> MapOverlayTexture;

	/** Time in seconds to display each objective line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Briefing", meta = (ClampMin = "0.5"))
	float SecondsPerObjective = 3.f;

	/** Time in seconds to display each intel note. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Briefing", meta = (ClampMin = "0.5"))
	float SecondsPerIntelNote = 4.f;

	/** Time in seconds to display the overview section. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Briefing", meta = (ClampMin = "1.0"))
	float OverviewDuration = 6.f;

	/** Time in seconds to display the map overlay. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Briefing", meta = (ClampMin = "1.0"))
	float MapOverlayDuration = 5.f;

	/** Is this briefing data valid for playback? */
	bool IsValid() const { return !MissionName.IsEmpty() && !BriefingText.IsEmpty(); }
};

// ─────────────────────────────────────────────────────────────
//  Delegates
// ─────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnBriefingStarted,
	const FSHBriefingData&, BriefingData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnBriefingCompleted,
	const FSHBriefingData&, BriefingData);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnBriefingSectionAdvanced,
	ESHBriefingSection, NewSection,
	int32, SectionIndex);

// ─────────────────────────────────────────────────────────────
//  USHBriefingSystem — Actor Component
// ─────────────────────────────────────────────────────────────

/**
 * USHBriefingSystem
 *
 * Actor component that drives pre-mission briefing sequences.
 * Progresses linearly through sections: overview, objectives,
 * intel notes, and map overlay. Each section is timed and
 * broadcasts delegates so the UI can present the appropriate
 * visuals and text. Narration audio plays throughout.
 */
UCLASS(ClassGroup = (ShatteredHorizon), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHBriefingSystem : public UActorComponent
{
	GENERATED_BODY()

public:
	USHBriefingSystem();

protected:
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

public:
	// ------------------------------------------------------------------
	//  Public interface
	// ------------------------------------------------------------------

	/** Start a briefing sequence with the given data. */
	UFUNCTION(BlueprintCallable, Category = "SH|Briefing")
	void StartBriefing(const FSHBriefingData& InBriefingData);

	/** Skip the remaining briefing and jump to completion. */
	UFUNCTION(BlueprintCallable, Category = "SH|Briefing")
	void SkipBriefing();

	/** Is a briefing currently active? */
	UFUNCTION(BlueprintPure, Category = "SH|Briefing")
	bool IsBriefingActive() const { return bIsBriefingActive; }

	/** Get the current section being displayed. */
	UFUNCTION(BlueprintPure, Category = "SH|Briefing")
	ESHBriefingSection GetCurrentSection() const { return CurrentSection; }

	/** Get the current sub-index within the active section (e.g., which objective). */
	UFUNCTION(BlueprintPure, Category = "SH|Briefing")
	int32 GetCurrentSectionIndex() const { return CurrentSectionIndex; }

	/** Get the active briefing data. */
	UFUNCTION(BlueprintPure, Category = "SH|Briefing")
	const FSHBriefingData& GetBriefingData() const { return ActiveBriefingData; }

	/** Get elapsed time in the current section. */
	UFUNCTION(BlueprintPure, Category = "SH|Briefing")
	float GetSectionElapsedTime() const { return SectionTimer; }

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Briefing")
	FOnBriefingStarted OnBriefingStarted;

	UPROPERTY(BlueprintAssignable, Category = "SH|Briefing")
	FOnBriefingCompleted OnBriefingCompleted;

	UPROPERTY(BlueprintAssignable, Category = "SH|Briefing")
	FOnBriefingSectionAdvanced OnBriefingSectionAdvanced;

private:
	// ------------------------------------------------------------------
	//  Internal helpers
	// ------------------------------------------------------------------

	/** Advance to the next section (or sub-item within a section). */
	void AdvanceSection();

	/** Transition to a specific section. */
	void EnterSection(ESHBriefingSection Section);

	/** Complete the briefing sequence. */
	void CompleteBriefing();

	/** Get the duration for the current section / sub-item. */
	float GetCurrentSectionDuration() const;

	/** Start narration audio playback. */
	void StartNarrationAudio();

	/** Stop narration audio playback. */
	void StopNarrationAudio();

	// ------------------------------------------------------------------
	//  Runtime state
	// ------------------------------------------------------------------

	/** Is a briefing currently in progress? */
	bool bIsBriefingActive = false;

	/** The active briefing data being presented. */
	FSHBriefingData ActiveBriefingData;

	/** Current section type. */
	ESHBriefingSection CurrentSection = ESHBriefingSection::MissionOverview;

	/** Sub-index within a section (e.g., which objective or intel note). */
	int32 CurrentSectionIndex = 0;

	/** Timer counting up within the current section / sub-item. */
	float SectionTimer = 0.f;

	/** Audio component for narration playback. */
	UPROPERTY()
	TObjectPtr<UAudioComponent> NarrationAudioComponent;

	/** Handle for async loading the narration audio asset. */
	TSharedPtr<struct FStreamableHandle> NarrationLoadHandle;
};
