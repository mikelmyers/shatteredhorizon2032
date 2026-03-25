// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHSaveGameSystem.h"
#include "SHSaveGame.h"
#include "Kismet/GameplayStatics.h"

// ============================================================================
//  Subsystem lifecycle
// ============================================================================

void USHSaveGameSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	LastSaveTime = FDateTime::MinValue();

	// Auto-load settings on startup so audio / input / accessibility
	// options are available before the player reaches the main menu.
	FSHSettingsSaveData LoadedSettings;
	if (LoadSettings(LoadedSettings))
	{
		CachedSettings = LoadedSettings;
		UE_LOG(LogTemp, Log, TEXT("[SHSaveGameSystem] Settings loaded successfully on startup."));
	}
	else
	{
		CachedSettings = FSHSettingsSaveData(); // defaults
		UE_LOG(LogTemp, Log, TEXT("[SHSaveGameSystem] No saved settings found — using defaults."));
	}
}

void USHSaveGameSystem::Deinitialize()
{
	Super::Deinitialize();
}

// ============================================================================
//  Campaign save / load
// ============================================================================

bool USHSaveGameSystem::SaveCampaign(int32 SlotIndex, const FSHCampaignSaveData& CampaignData)
{
	if (!IsValidCampaignSlot(SlotIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SHSaveGameSystem] SaveCampaign — invalid slot index %d."), SlotIndex);
		return false;
	}

	USHSaveGame* SaveGame = CreateSaveGameObject();
	if (!SaveGame)
	{
		return false;
	}

	SaveGame->CampaignData = CampaignData;
	SaveGame->SaveDisplayName = FString::Printf(TEXT("Campaign Slot %d"), SlotIndex);
	SaveGame->SaveTimestamp = FDateTime::UtcNow();

	const FString SlotName = GetSaveSlotName(TEXT("Campaign"), SlotIndex);
	const bool bSuccess = WriteSaveGameToSlot(SaveGame, SlotName);

	OnSaveComplete.Broadcast(SlotName, bSuccess);
	return bSuccess;
}

bool USHSaveGameSystem::LoadCampaign(int32 SlotIndex, FSHCampaignSaveData& OutCampaignData)
{
	if (!IsValidCampaignSlot(SlotIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SHSaveGameSystem] LoadCampaign — invalid slot index %d."), SlotIndex);
		return false;
	}

	const FString SlotName = GetSaveSlotName(TEXT("Campaign"), SlotIndex);
	USHSaveGame* SaveGame = LoadSaveGameObject(SlotName);

	if (!SaveGame)
	{
		OnLoadComplete.Broadcast(SlotName, false);
		return false;
	}

	OutCampaignData = SaveGame->CampaignData;
	OnLoadComplete.Broadcast(SlotName, true);
	return true;
}

bool USHSaveGameSystem::DeleteCampaign(int32 SlotIndex)
{
	if (!IsValidCampaignSlot(SlotIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SHSaveGameSystem] DeleteCampaign — invalid slot index %d."), SlotIndex);
		return false;
	}

	const FString SlotName = GetSaveSlotName(TEXT("Campaign"), SlotIndex);

	if (!UGameplayStatics::DoesSaveGameExist(SlotName, SaveUserIndex))
	{
		return false;
	}

	return UGameplayStatics::DeleteGameInSlot(SlotName, SaveUserIndex);
}

bool USHSaveGameSystem::HasCampaignSave(int32 SlotIndex) const
{
	if (!IsValidCampaignSlot(SlotIndex))
	{
		return false;
	}

	const FString SlotName = GetSaveSlotName(TEXT("Campaign"), SlotIndex);
	return UGameplayStatics::DoesSaveGameExist(SlotName, SaveUserIndex);
}

// ============================================================================
//  Settings save / load
// ============================================================================

bool USHSaveGameSystem::SaveSettings(const FSHSettingsSaveData& SettingsData)
{
	USHSaveGame* SaveGame = CreateSaveGameObject();
	if (!SaveGame)
	{
		return false;
	}

	SaveGame->SettingsData = SettingsData;
	SaveGame->SaveTimestamp = FDateTime::UtcNow();

	const FString SlotName = TEXT("SH2032_Settings");
	const bool bSuccess = WriteSaveGameToSlot(SaveGame, SlotName);

	if (bSuccess)
	{
		CachedSettings = SettingsData;
	}

	OnSaveComplete.Broadcast(SlotName, bSuccess);
	return bSuccess;
}

bool USHSaveGameSystem::LoadSettings(FSHSettingsSaveData& OutSettingsData)
{
	const FString SlotName = TEXT("SH2032_Settings");
	USHSaveGame* SaveGame = LoadSaveGameObject(SlotName);

	if (!SaveGame)
	{
		OnLoadComplete.Broadcast(SlotName, false);
		return false;
	}

	OutSettingsData = SaveGame->SettingsData;
	CachedSettings = OutSettingsData;
	OnLoadComplete.Broadcast(SlotName, true);
	return true;
}

// ============================================================================
//  Progression save / load
// ============================================================================

bool USHSaveGameSystem::SaveProgression(const FSHProgressionSaveData& ProgressionData)
{
	USHSaveGame* SaveGame = CreateSaveGameObject();
	if (!SaveGame)
	{
		return false;
	}

	SaveGame->ProgressionData = ProgressionData;
	SaveGame->SaveTimestamp = FDateTime::UtcNow();

	const FString SlotName = TEXT("SH2032_Progression");
	const bool bSuccess = WriteSaveGameToSlot(SaveGame, SlotName);

	OnSaveComplete.Broadcast(SlotName, bSuccess);
	return bSuccess;
}

bool USHSaveGameSystem::LoadProgression(FSHProgressionSaveData& OutProgressionData)
{
	const FString SlotName = TEXT("SH2032_Progression");
	USHSaveGame* SaveGame = LoadSaveGameObject(SlotName);

	if (!SaveGame)
	{
		OnLoadComplete.Broadcast(SlotName, false);
		return false;
	}

	OutProgressionData = SaveGame->ProgressionData;
	OnLoadComplete.Broadcast(SlotName, true);
	return true;
}

// ============================================================================
//  Loadout save / load
// ============================================================================

bool USHSaveGameSystem::SaveLoadout(int32 SlotIndex, const FSHLoadoutSaveData& LoadoutData)
{
	if (!IsValidLoadoutSlot(SlotIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SHSaveGameSystem] SaveLoadout — invalid slot index %d."), SlotIndex);
		return false;
	}

	USHSaveGame* SaveGame = CreateSaveGameObject();
	if (!SaveGame)
	{
		return false;
	}

	SaveGame->LoadoutData = LoadoutData;
	SaveGame->SaveTimestamp = FDateTime::UtcNow();

	const FString SlotName = GetSaveSlotName(TEXT("Loadout"), SlotIndex);
	const bool bSuccess = WriteSaveGameToSlot(SaveGame, SlotName);

	OnSaveComplete.Broadcast(SlotName, bSuccess);
	return bSuccess;
}

bool USHSaveGameSystem::LoadLoadout(int32 SlotIndex, FSHLoadoutSaveData& OutLoadoutData)
{
	if (!IsValidLoadoutSlot(SlotIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SHSaveGameSystem] LoadLoadout — invalid slot index %d."), SlotIndex);
		return false;
	}

	const FString SlotName = GetSaveSlotName(TEXT("Loadout"), SlotIndex);
	USHSaveGame* SaveGame = LoadSaveGameObject(SlotName);

	if (!SaveGame)
	{
		OnLoadComplete.Broadcast(SlotName, false);
		return false;
	}

	OutLoadoutData = SaveGame->LoadoutData;
	OnLoadComplete.Broadcast(SlotName, true);
	return true;
}

// ============================================================================
//  Stats save / load
// ============================================================================

bool USHSaveGameSystem::SaveStats(const FSHStatsSaveData& StatsData)
{
	USHSaveGame* SaveGame = CreateSaveGameObject();
	if (!SaveGame)
	{
		return false;
	}

	SaveGame->StatsData = StatsData;
	SaveGame->SaveTimestamp = FDateTime::UtcNow();

	const FString SlotName = TEXT("SH2032_Stats");
	const bool bSuccess = WriteSaveGameToSlot(SaveGame, SlotName);

	OnSaveComplete.Broadcast(SlotName, bSuccess);
	return bSuccess;
}

bool USHSaveGameSystem::LoadStats(FSHStatsSaveData& OutStatsData)
{
	const FString SlotName = TEXT("SH2032_Stats");
	USHSaveGame* SaveGame = LoadSaveGameObject(SlotName);

	if (!SaveGame)
	{
		OnLoadComplete.Broadcast(SlotName, false);
		return false;
	}

	OutStatsData = SaveGame->StatsData;
	OnLoadComplete.Broadcast(SlotName, true);
	return true;
}

// ============================================================================
//  Auto-save
// ============================================================================

void USHSaveGameSystem::AutoSave(const FSHCampaignSaveData& CampaignData, const FSHProgressionSaveData& ProgressionData)
{
	if (ActiveCampaignSlot < 0)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SHSaveGameSystem] AutoSave skipped — no active campaign slot."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[SHSaveGameSystem] AutoSave — saving campaign slot %d and progression."), ActiveCampaignSlot);

	// Save campaign to the active slot
	SaveCampaign(ActiveCampaignSlot, CampaignData);

	// Save progression (single global slot)
	SaveProgression(ProgressionData);
}

void USHSaveGameSystem::SetActiveCampaignSlot(int32 SlotIndex)
{
	if (SlotIndex >= 0 && !IsValidCampaignSlot(SlotIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SHSaveGameSystem] SetActiveCampaignSlot — invalid slot %d."), SlotIndex);
		return;
	}

	ActiveCampaignSlot = SlotIndex;
}

// ============================================================================
//  Async save
// ============================================================================

void USHSaveGameSystem::AsyncSaveCampaign(int32 SlotIndex, const FSHCampaignSaveData& CampaignData)
{
	if (!IsValidCampaignSlot(SlotIndex))
	{
		UE_LOG(LogTemp, Warning, TEXT("[SHSaveGameSystem] AsyncSaveCampaign — invalid slot index %d."), SlotIndex);
		OnSaveComplete.Broadcast(GetSaveSlotName(TEXT("Campaign"), SlotIndex), false);
		return;
	}

	USHSaveGame* SaveGame = CreateSaveGameObject();
	if (!SaveGame)
	{
		OnSaveComplete.Broadcast(GetSaveSlotName(TEXT("Campaign"), SlotIndex), false);
		return;
	}

	SaveGame->CampaignData = CampaignData;
	SaveGame->SaveDisplayName = FString::Printf(TEXT("Campaign Slot %d"), SlotIndex);
	SaveGame->SaveTimestamp = FDateTime::UtcNow();

	const FString SlotName = GetSaveSlotName(TEXT("Campaign"), SlotIndex);

	FAsyncSaveGameToSlotDelegate AsyncDelegate;
	AsyncDelegate.BindUObject(this, &USHSaveGameSystem::HandleAsyncSaveComplete);

	UGameplayStatics::AsyncSaveGameToSlot(SaveGame, SlotName, SaveUserIndex, AsyncDelegate);

	UE_LOG(LogTemp, Log, TEXT("[SHSaveGameSystem] Async save initiated for slot '%s'."), *SlotName);
}

void USHSaveGameSystem::HandleAsyncSaveComplete(const FString& SlotName, const int32 UserIndex, bool bSuccess)
{
	if (bSuccess)
	{
		LastSaveTime = FDateTime::UtcNow();
		UE_LOG(LogTemp, Log, TEXT("[SHSaveGameSystem] Async save succeeded for slot '%s'."), *SlotName);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[SHSaveGameSystem] Async save FAILED for slot '%s'."), *SlotName);
	}

	OnSaveComplete.Broadcast(SlotName, bSuccess);
}

// ============================================================================
//  Slot queries
// ============================================================================

FString USHSaveGameSystem::GetSaveSlotName(const FString& Domain, int32 SlotIndex)
{
	return FString::Printf(TEXT("SH2032_%s_%d"), *Domain, SlotIndex);
}

TArray<FSHSaveSlotInfo> USHSaveGameSystem::GetAllCampaignSlotInfo() const
{
	TArray<FSHSaveSlotInfo> Results;
	Results.Reserve(MaxCampaignSlots);

	for (int32 i = 0; i < MaxCampaignSlots; ++i)
	{
		Results.Add(GetSaveSlotInfo(i));
	}

	return Results;
}

FSHSaveSlotInfo USHSaveGameSystem::GetSaveSlotInfo(int32 SlotIndex) const
{
	FSHSaveSlotInfo Info;
	Info.SlotIndex = SlotIndex;
	Info.SlotName = GetSaveSlotName(TEXT("Campaign"), SlotIndex);

	if (!IsValidCampaignSlot(SlotIndex))
	{
		return Info;
	}

	if (!UGameplayStatics::DoesSaveGameExist(Info.SlotName, SaveUserIndex))
	{
		Info.bIsOccupied = false;
		return Info;
	}

	USaveGame* RawSave = UGameplayStatics::LoadGameFromSlot(Info.SlotName, SaveUserIndex);
	USHSaveGame* SaveGame = Cast<USHSaveGame>(RawSave);

	if (!SaveGame)
	{
		Info.bIsOccupied = false;
		return Info;
	}

	Info.bIsOccupied = true;
	Info.DisplayName = SaveGame->SaveDisplayName;
	Info.SaveTimestamp = SaveGame->SaveTimestamp;
	Info.MissionIndex = SaveGame->CampaignData.CurrentMissionIndex;
	Info.DifficultyLevel = SaveGame->CampaignData.DifficultyLevel;
	Info.TotalPlayTime = SaveGame->CampaignData.MissionTimePlayed;

	return Info;
}

// ============================================================================
//  Internal helpers
// ============================================================================

bool USHSaveGameSystem::IsValidCampaignSlot(int32 SlotIndex) const
{
	return SlotIndex >= 0 && SlotIndex < MaxCampaignSlots;
}

bool USHSaveGameSystem::IsValidLoadoutSlot(int32 SlotIndex) const
{
	return SlotIndex >= 0 && SlotIndex < MaxLoadoutSlots;
}

USHSaveGame* USHSaveGameSystem::CreateSaveGameObject() const
{
	USHSaveGame* SaveGame = Cast<USHSaveGame>(
		UGameplayStatics::CreateSaveGameObject(USHSaveGame::StaticClass()));

	if (!SaveGame)
	{
		UE_LOG(LogTemp, Error, TEXT("[SHSaveGameSystem] Failed to create USHSaveGame object."));
	}

	return SaveGame;
}

USHSaveGame* USHSaveGameSystem::LoadSaveGameObject(const FString& SlotName) const
{
	if (!UGameplayStatics::DoesSaveGameExist(SlotName, SaveUserIndex))
	{
		UE_LOG(LogTemp, Log, TEXT("[SHSaveGameSystem] Save slot '%s' does not exist."), *SlotName);
		return nullptr;
	}

	USaveGame* RawSave = UGameplayStatics::LoadGameFromSlot(SlotName, SaveUserIndex);
	USHSaveGame* SaveGame = Cast<USHSaveGame>(RawSave);

	if (!SaveGame)
	{
		UE_LOG(LogTemp, Error, TEXT("[SHSaveGameSystem] Failed to load or cast save from slot '%s'."), *SlotName);
		return nullptr;
	}

	// Version check — future-proof for schema migrations
	if (SaveGame->SaveVersion > USHSaveGame::LatestSaveVersion)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SHSaveGameSystem] Save slot '%s' has version %d (latest = %d). Possible data loss."),
			*SlotName, SaveGame->SaveVersion, USHSaveGame::LatestSaveVersion);
	}

	return SaveGame;
}

bool USHSaveGameSystem::WriteSaveGameToSlot(USHSaveGame* SaveGame, const FString& SlotName)
{
	if (!SaveGame)
	{
		return false;
	}

	SaveGame->SaveVersion = USHSaveGame::LatestSaveVersion;
	SaveGame->SaveTimestamp = FDateTime::UtcNow();

	const bool bSuccess = UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, SaveUserIndex);

	if (bSuccess)
	{
		LastSaveTime = FDateTime::UtcNow();
		UE_LOG(LogTemp, Log, TEXT("[SHSaveGameSystem] Save succeeded for slot '%s'."), *SlotName);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("[SHSaveGameSystem] Save FAILED for slot '%s'."), *SlotName);
	}

	return bSuccess;
}
