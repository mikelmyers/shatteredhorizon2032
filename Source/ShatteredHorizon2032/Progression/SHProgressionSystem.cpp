// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHProgressionSystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"

// =======================================================================
//  Subsystem lifecycle
// =======================================================================

void USHProgressionSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	ProgressionData = FSHProgressionData();
	InitializeDefaultThresholds();

	UE_LOG(LogTemp, Log, TEXT("[SHProgressionSystem] Initialized. Rank: %d, TotalXP: %d"),
		static_cast<int32>(ProgressionData.CurrentRank), ProgressionData.TotalXP);
}

void USHProgressionSystem::Deinitialize()
{
	UE_LOG(LogTemp, Log, TEXT("[SHProgressionSystem] Deinitializing. Final rank: %d, TotalXP: %d"),
		static_cast<int32>(ProgressionData.CurrentRank), ProgressionData.TotalXP);

	Super::Deinitialize();
}

// =======================================================================
//  XP management
// =======================================================================

void USHProgressionSystem::AwardXP(int32 Amount, FName Reason)
{
	if (Amount <= 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SHProgressionSystem] AwardXP called with non-positive amount: %d. Ignoring."), Amount);
		return;
	}

	if (IsMaxRank(ProgressionData.CurrentRank))
	{
		// Max rank — still track total XP for stats, but no rank progression.
		ProgressionData.TotalXP += Amount;
		OnXPGained.Broadcast(Amount, ProgressionData.TotalXP, Reason);

		UE_LOG(LogTemp, Log, TEXT("[SHProgressionSystem] +%d XP (%s) — Max rank. Total: %d"),
			Amount, *Reason.ToString(), ProgressionData.TotalXP);
		return;
	}

	ProgressionData.CurrentXP += Amount;
	ProgressionData.TotalXP += Amount;

	UE_LOG(LogTemp, Log, TEXT("[SHProgressionSystem] +%d XP (%s) — Current: %d, Total: %d, Rank: %d"),
		Amount, *Reason.ToString(), ProgressionData.CurrentXP, ProgressionData.TotalXP,
		static_cast<int32>(ProgressionData.CurrentRank));

	OnXPGained.Broadcast(Amount, ProgressionData.TotalXP, Reason);

	// Check for rank-up(s). Multiple rank-ups are possible from a single large award.
	CheckRankUp();
}

// =======================================================================
//  Rank queries
// =======================================================================

int32 USHProgressionSystem::GetXPToNextRank() const
{
	if (IsMaxRank(ProgressionData.CurrentRank))
	{
		return 0;
	}

	const int32 Threshold = GetThresholdForRank(ProgressionData.CurrentRank);
	return FMath::Max(0, Threshold - ProgressionData.CurrentXP);
}

float USHProgressionSystem::GetProgressToNextRank() const
{
	if (IsMaxRank(ProgressionData.CurrentRank))
	{
		return 1.f;
	}

	const int32 Threshold = GetThresholdForRank(ProgressionData.CurrentRank);
	if (Threshold <= 0)
	{
		return 1.f;
	}

	return FMath::Clamp(static_cast<float>(ProgressionData.CurrentXP) / static_cast<float>(Threshold), 0.f, 1.f);
}

FText USHProgressionSystem::GetRankDisplayName(ESHRank Rank)
{
	switch (Rank)
	{
	case ESHRank::Private:			return FText::FromString(TEXT("Private"));
	case ESHRank::PFC:				return FText::FromString(TEXT("Private First Class"));
	case ESHRank::LanceCorporal:	return FText::FromString(TEXT("Lance Corporal"));
	case ESHRank::Corporal:			return FText::FromString(TEXT("Corporal"));
	case ESHRank::Sergeant:			return FText::FromString(TEXT("Sergeant"));
	case ESHRank::StaffSergeant:	return FText::FromString(TEXT("Staff Sergeant"));
	case ESHRank::GunnerySergeant:	return FText::FromString(TEXT("Gunnery Sergeant"));
	default:						return FText::FromString(TEXT("Unknown"));
	}
}

FText USHProgressionSystem::GetRankAbbreviation(ESHRank Rank)
{
	switch (Rank)
	{
	case ESHRank::Private:			return FText::FromString(TEXT("Pvt"));
	case ESHRank::PFC:				return FText::FromString(TEXT("PFC"));
	case ESHRank::LanceCorporal:	return FText::FromString(TEXT("LCpl"));
	case ESHRank::Corporal:			return FText::FromString(TEXT("Cpl"));
	case ESHRank::Sergeant:			return FText::FromString(TEXT("Sgt"));
	case ESHRank::StaffSergeant:	return FText::FromString(TEXT("SSgt"));
	case ESHRank::GunnerySergeant:	return FText::FromString(TEXT("GySgt"));
	default:						return FText::FromString(TEXT("Unk"));
	}
}

// =======================================================================
//  Item unlocks
// =======================================================================

bool USHProgressionSystem::IsItemUnlocked(FName ItemId) const
{
	return ProgressionData.UnlockedItems.Contains(ItemId);
}

void USHProgressionSystem::UnlockItem(FName ItemId)
{
	if (ItemId.IsNone())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SHProgressionSystem] UnlockItem called with NAME_None. Ignoring."));
		return;
	}

	if (ProgressionData.UnlockedItems.Contains(ItemId))
	{
		UE_LOG(LogTemp, Verbose, TEXT("[SHProgressionSystem] Item '%s' already unlocked."), *ItemId.ToString());
		return;
	}

	ProgressionData.UnlockedItems.Add(ItemId);

	UE_LOG(LogTemp, Log, TEXT("[SHProgressionSystem] Item unlocked: '%s'. Total unlocks: %d"),
		*ItemId.ToString(), ProgressionData.UnlockedItems.Num());

	OnItemUnlocked.Broadcast(ItemId);
}

// =======================================================================
//  Save / Load
// =======================================================================

bool USHProgressionSystem::SaveProgression(const FString& SlotName)
{
	const FString FilePath = GetSaveFilePath(SlotName);

	FBufferArchive SaveArchive;
	ProgressionData.Serialize(SaveArchive);

	if (SaveArchive.Num() == 0)
	{
		UE_LOG(LogTemp, Error, TEXT("[SHProgressionSystem] SaveProgression — Serialization produced empty buffer."));
		return false;
	}

	if (FFileHelper::SaveArrayToFile(SaveArchive, *FilePath))
	{
		UE_LOG(LogTemp, Log, TEXT("[SHProgressionSystem] Progression saved to '%s'. Size: %d bytes."),
			*FilePath, SaveArchive.Num());
		SaveArchive.FlushCache();
		SaveArchive.Empty();
		return true;
	}

	UE_LOG(LogTemp, Error, TEXT("[SHProgressionSystem] Failed to save progression to '%s'."), *FilePath);
	SaveArchive.FlushCache();
	SaveArchive.Empty();
	return false;
}

bool USHProgressionSystem::LoadProgression(const FString& SlotName)
{
	const FString FilePath = GetSaveFilePath(SlotName);

	TArray<uint8> LoadData;
	if (!FFileHelper::LoadFileToArray(LoadData, *FilePath))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SHProgressionSystem] No save file found at '%s'. Using default progression."),
			*FilePath);
		return false;
	}

	FMemoryReader LoadArchive(LoadData, true);
	ProgressionData.Serialize(LoadArchive);

	LoadArchive.FlushCache();
	LoadArchive.Close();

	UE_LOG(LogTemp, Log, TEXT("[SHProgressionSystem] Progression loaded from '%s'. Rank: %d, TotalXP: %d, Unlocks: %d"),
		*FilePath, static_cast<int32>(ProgressionData.CurrentRank),
		ProgressionData.TotalXP, ProgressionData.UnlockedItems.Num());

	return true;
}

void USHProgressionSystem::SetProgressionData(const FSHProgressionData& InData)
{
	ProgressionData = InData;

	UE_LOG(LogTemp, Log, TEXT("[SHProgressionSystem] Progression data overwritten. Rank: %d, TotalXP: %d"),
		static_cast<int32>(ProgressionData.CurrentRank), ProgressionData.TotalXP);
}

// =======================================================================
//  Internal helpers
// =======================================================================

void USHProgressionSystem::CheckRankUp()
{
	// Loop to handle multiple rank-ups from a single large XP award.
	while (!IsMaxRank(ProgressionData.CurrentRank))
	{
		const int32 Threshold = GetThresholdForRank(ProgressionData.CurrentRank);
		if (Threshold <= 0 || ProgressionData.CurrentXP < Threshold)
		{
			break;
		}

		// Rank up — carry over excess XP.
		ProgressionData.CurrentXP -= Threshold;
		const ESHRank OldRank = ProgressionData.CurrentRank;
		ProgressionData.CurrentRank = GetNextRank(OldRank);

		UE_LOG(LogTemp, Log, TEXT("[SHProgressionSystem] RANK UP: %s -> %s. Remaining XP: %d"),
			*GetRankAbbreviation(OldRank).ToString(),
			*GetRankAbbreviation(ProgressionData.CurrentRank).ToString(),
			ProgressionData.CurrentXP);

		OnRankUp.Broadcast(OldRank, ProgressionData.CurrentRank);
	}
}

int32 USHProgressionSystem::GetThresholdForRank(ESHRank Rank) const
{
	if (const int32* Threshold = XPThresholds.Find(Rank))
	{
		return *Threshold;
	}
	return 0;
}

ESHRank USHProgressionSystem::GetNextRank(ESHRank Rank)
{
	const uint8 CurrentValue = static_cast<uint8>(Rank);
	const uint8 MaxValue = static_cast<uint8>(ESHRank::GunnerySergeant);

	if (CurrentValue >= MaxValue)
	{
		return Rank;
	}

	return static_cast<ESHRank>(CurrentValue + 1);
}

bool USHProgressionSystem::IsMaxRank(ESHRank Rank)
{
	return Rank == ESHRank::GunnerySergeant;
}

void USHProgressionSystem::InitializeDefaultThresholds()
{
	if (XPThresholds.Num() > 0)
	{
		return;
	}

	// Default XP thresholds (designed for roughly 15-20 hours of play to max rank).
	// These represent XP needed to advance FROM the given rank to the next.
	XPThresholds.Add(ESHRank::Private,			1000);	// Pvt -> PFC
	XPThresholds.Add(ESHRank::PFC,				2500);	// PFC -> LCpl
	XPThresholds.Add(ESHRank::LanceCorporal,	5000);	// LCpl -> Cpl
	XPThresholds.Add(ESHRank::Corporal,			8000);	// Cpl -> Sgt
	XPThresholds.Add(ESHRank::Sergeant,			12000);	// Sgt -> SSgt
	XPThresholds.Add(ESHRank::StaffSergeant,	18000);	// SSgt -> GySgt
	// GunnerySergeant has no threshold — it is the max rank.
}

FString USHProgressionSystem::GetSaveFilePath(const FString& SlotName) const
{
	return FPaths::ProjectSavedDir() / TEXT("Progression") / (SlotName + TEXT(".sav"));
}
