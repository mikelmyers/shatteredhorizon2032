// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SHProgressionSystem.generated.h"

/** USMC enlisted ranks used for player progression. */
UENUM(BlueprintType)
enum class ESHRank : uint8
{
	Private			UMETA(DisplayName = "Private (Pvt)"),
	PFC				UMETA(DisplayName = "Private First Class (PFC)"),
	LanceCorporal	UMETA(DisplayName = "Lance Corporal (LCpl)"),
	Corporal		UMETA(DisplayName = "Corporal (Cpl)"),
	Sergeant		UMETA(DisplayName = "Sergeant (Sgt)"),
	StaffSergeant	UMETA(DisplayName = "Staff Sergeant (SSgt)"),
	GunnerySergeant	UMETA(DisplayName = "Gunnery Sergeant (GySgt)")
};

/**
 * Serializable progression data that persists across sessions.
 * Covers XP, rank, and unlocked lateral progression items.
 */
USTRUCT(BlueprintType)
struct FSHProgressionData
{
	GENERATED_BODY()

	/** XP accumulated toward the next rank. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression")
	int32 CurrentXP = 0;

	/** Total XP earned across the player's entire career. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression")
	int32 TotalXP = 0;

	/** Current rank. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression")
	ESHRank CurrentRank = ESHRank::Private;

	/** Set of unlocked item IDs (suppressors, optics, grips, camo patterns). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Progression")
	TSet<FName> UnlockedItems;

	/** Serialize to/from archive for save/load. */
	bool Serialize(FArchive& Ar)
	{
		Ar << CurrentXP;
		Ar << TotalXP;

		uint8 RankByte = static_cast<uint8>(CurrentRank);
		Ar << RankByte;
		if (Ar.IsLoading())
		{
			CurrentRank = static_cast<ESHRank>(RankByte);
		}

		int32 ItemCount = UnlockedItems.Num();
		Ar << ItemCount;

		if (Ar.IsLoading())
		{
			UnlockedItems.Empty(ItemCount);
			for (int32 i = 0; i < ItemCount; ++i)
			{
				FName ItemName;
				Ar << ItemName;
				UnlockedItems.Add(ItemName);
			}
		}
		else
		{
			for (FName& Item : UnlockedItems)
			{
				Ar << Item;
			}
		}

		return true;
	}
};

// Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnXPGained, int32, AmountGained, int32, NewTotalXP, FName, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnRankUp, ESHRank, OldRank, ESHRank, NewRank);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnItemUnlocked, FName, ItemId);

/**
 * USHProgressionSystem
 *
 * Game instance subsystem managing XP accrual, USMC rank progression,
 * and lateral gear unlocks. Progression is explicitly non-pay-to-win:
 * unlocks are sidegrades (suppressors, optics, grips, camo patterns)
 * rather than power upgrades.
 *
 * XP sources include: kills, objective completions, squad leadership
 * effectiveness, and accuracy bonuses. Rank thresholds are configurable
 * via the XPThresholds map.
 *
 * Progression data is serializable for save/load through FSHProgressionData.
 */
UCLASS()
class SHATTEREDHORIZON2032_API USHProgressionSystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ------------------------------------------------------------------
	//  XP management
	// ------------------------------------------------------------------

	/**
	 * Award XP to the player from a named source.
	 * Automatically checks for and processes rank-ups.
	 *
	 * @param Amount   XP to award (must be positive).
	 * @param Reason   Source tag (e.g. "Kill", "Objective", "SquadLeadership", "AccuracyBonus").
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Progression")
	void AwardXP(int32 Amount, FName Reason);

	// ------------------------------------------------------------------
	//  Rank queries
	// ------------------------------------------------------------------

	/** Get the player's current USMC rank. */
	UFUNCTION(BlueprintPure, Category = "SH|Progression")
	ESHRank GetCurrentRank() const { return ProgressionData.CurrentRank; }

	/** Get the XP required to reach the next rank. Returns 0 if at max rank. */
	UFUNCTION(BlueprintPure, Category = "SH|Progression")
	int32 GetXPToNextRank() const;

	/** Get normalized progress toward the next rank (0.0 to 1.0). Returns 1.0 at max rank. */
	UFUNCTION(BlueprintPure, Category = "SH|Progression")
	float GetProgressToNextRank() const;

	/** Get the display name for a rank. */
	UFUNCTION(BlueprintPure, Category = "SH|Progression")
	static FText GetRankDisplayName(ESHRank Rank);

	/** Get the abbreviated rank title. */
	UFUNCTION(BlueprintPure, Category = "SH|Progression")
	static FText GetRankAbbreviation(ESHRank Rank);

	// ------------------------------------------------------------------
	//  XP queries
	// ------------------------------------------------------------------

	/** Get XP accumulated toward the current rank. */
	UFUNCTION(BlueprintPure, Category = "SH|Progression")
	int32 GetCurrentXP() const { return ProgressionData.CurrentXP; }

	/** Get total career XP. */
	UFUNCTION(BlueprintPure, Category = "SH|Progression")
	int32 GetTotalXP() const { return ProgressionData.TotalXP; }

	// ------------------------------------------------------------------
	//  Item unlocks (lateral progression)
	// ------------------------------------------------------------------

	/** Check whether an item is unlocked. */
	UFUNCTION(BlueprintPure, Category = "SH|Progression")
	bool IsItemUnlocked(FName ItemId) const;

	/**
	 * Unlock a lateral progression item (suppressor, optic, grip, camo).
	 * Does nothing if the item is already unlocked.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Progression")
	void UnlockItem(FName ItemId);

	/** Get the full set of unlocked item IDs. */
	UFUNCTION(BlueprintPure, Category = "SH|Progression")
	const TSet<FName>& GetUnlockedItems() const { return ProgressionData.UnlockedItems; }

	// ------------------------------------------------------------------
	//  Save / Load
	// ------------------------------------------------------------------

	/** Save the current progression data to a binary archive. */
	UFUNCTION(BlueprintCallable, Category = "SH|Progression")
	bool SaveProgression(const FString& SlotName);

	/** Load progression data from a binary archive. */
	UFUNCTION(BlueprintCallable, Category = "SH|Progression")
	bool LoadProgression(const FString& SlotName);

	/** Get the raw progression data struct. */
	UFUNCTION(BlueprintPure, Category = "SH|Progression")
	const FSHProgressionData& GetProgressionData() const { return ProgressionData; }

	/** Overwrite the progression data (e.g., from an external save system). */
	UFUNCTION(BlueprintCallable, Category = "SH|Progression")
	void SetProgressionData(const FSHProgressionData& InData);

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/**
	 * XP thresholds per rank. Maps each rank to the total XP required
	 * to advance from that rank to the next. The final rank has no
	 * threshold (max rank).
	 */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Progression")
	TMap<ESHRank, int32> XPThresholds;

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Progression")
	FOnXPGained OnXPGained;

	UPROPERTY(BlueprintAssignable, Category = "SH|Progression")
	FOnRankUp OnRankUp;

	UPROPERTY(BlueprintAssignable, Category = "SH|Progression")
	FOnItemUnlocked OnItemUnlocked;

private:
	/** Check if the current XP warrants a rank-up and process it. */
	void CheckRankUp();

	/** Get the XP threshold for the given rank. Returns 0 if at max or not configured. */
	int32 GetThresholdForRank(ESHRank Rank) const;

	/** Get the next rank after the given one. Returns the same rank if at max. */
	static ESHRank GetNextRank(ESHRank Rank);

	/** Check if the given rank is the maximum achievable rank. */
	static bool IsMaxRank(ESHRank Rank);

	/** Initialize default XP thresholds if the map is empty. */
	void InitializeDefaultThresholds();

	/** Build the save file path for a given slot name. */
	FString GetSaveFilePath(const FString& SlotName) const;

	UPROPERTY()
	FSHProgressionData ProgressionData;
};
