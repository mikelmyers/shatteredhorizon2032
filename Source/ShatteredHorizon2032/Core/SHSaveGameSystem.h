// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SHSaveGame.h"
#include "SHSaveGameSystem.generated.h"

class USHSaveGame;

// ---------------------------------------------------------------------------
//  Save slot info — lightweight metadata for UI lists
// ---------------------------------------------------------------------------

/** Lightweight save slot descriptor for UI display. */
USTRUCT(BlueprintType)
struct FSHSaveSlotInfo
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "SaveSystem")
	int32 SlotIndex = -1;

	UPROPERTY(BlueprintReadOnly, Category = "SaveSystem")
	FString SlotName;

	UPROPERTY(BlueprintReadOnly, Category = "SaveSystem")
	FString DisplayName;

	UPROPERTY(BlueprintReadOnly, Category = "SaveSystem")
	FDateTime SaveTimestamp;

	UPROPERTY(BlueprintReadOnly, Category = "SaveSystem")
	bool bIsOccupied = false;

	UPROPERTY(BlueprintReadOnly, Category = "SaveSystem")
	int32 MissionIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "SaveSystem")
	int32 DifficultyLevel = 1;

	UPROPERTY(BlueprintReadOnly, Category = "SaveSystem")
	float TotalPlayTime = 0.f;
};

// ---------------------------------------------------------------------------
//  Delegates
// ---------------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSHSaveComplete, const FString&, SlotName, bool, bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSHLoadComplete, const FString&, SlotName, bool, bSuccess);

// ---------------------------------------------------------------------------
//  USHSaveGameSystem
// ---------------------------------------------------------------------------

/**
 * USHSaveGameSystem
 *
 * Game instance subsystem that owns all save/load operations for
 * Shattered Horizon 2032. Provides separate persistence domains
 * for campaign progress, player progression, loadout presets, and
 * user settings so that each can be saved and loaded independently.
 *
 * Slot naming convention:
 *   Campaign   : "SH2032_Campaign_<SlotIndex>"  (0-2, up to 3 slots)
 *   Settings   : "SH2032_Settings"
 *   Progression: "SH2032_Progression"
 *   Loadout    : "SH2032_Loadout_<SlotIndex>"   (0-4, up to 5 presets)
 *   Stats      : "SH2032_Stats"
 *
 * Settings are automatically loaded on subsystem initialization so
 * that audio levels, input sensitivities, and accessibility options
 * are active before the player reaches the main menu.
 */
UCLASS()
class SHATTEREDHORIZON2032_API USHSaveGameSystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ------------------------------------------------------------------
	//  Subsystem lifecycle
	// ------------------------------------------------------------------

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ------------------------------------------------------------------
	//  Campaign save / load  (slot 0-2)
	// ------------------------------------------------------------------

	/** Save campaign progress to the given slot (0-2). */
	UFUNCTION(BlueprintCallable, Category = "SH|SaveSystem")
	bool SaveCampaign(int32 SlotIndex, const FSHCampaignSaveData& CampaignData);

	/** Load campaign progress from the given slot. Returns true on success. */
	UFUNCTION(BlueprintCallable, Category = "SH|SaveSystem")
	bool LoadCampaign(int32 SlotIndex, FSHCampaignSaveData& OutCampaignData);

	/** Delete a campaign save slot. */
	UFUNCTION(BlueprintCallable, Category = "SH|SaveSystem")
	bool DeleteCampaign(int32 SlotIndex);

	/** Check whether the given campaign slot contains a valid save. */
	UFUNCTION(BlueprintPure, Category = "SH|SaveSystem")
	bool HasCampaignSave(int32 SlotIndex) const;

	// ------------------------------------------------------------------
	//  Settings save / load  (auto-loaded on Initialize)
	// ------------------------------------------------------------------

	/** Persist current settings. */
	UFUNCTION(BlueprintCallable, Category = "SH|SaveSystem")
	bool SaveSettings(const FSHSettingsSaveData& SettingsData);

	/** Load settings from disk. Called automatically during Initialize(). */
	UFUNCTION(BlueprintCallable, Category = "SH|SaveSystem")
	bool LoadSettings(FSHSettingsSaveData& OutSettingsData);

	// ------------------------------------------------------------------
	//  Progression save / load
	// ------------------------------------------------------------------

	/** Save player progression (XP, rank, unlocks). */
	UFUNCTION(BlueprintCallable, Category = "SH|SaveSystem")
	bool SaveProgression(const FSHProgressionSaveData& ProgressionData);

	/** Load player progression. */
	UFUNCTION(BlueprintCallable, Category = "SH|SaveSystem")
	bool LoadProgression(FSHProgressionSaveData& OutProgressionData);

	// ------------------------------------------------------------------
	//  Loadout save / load  (slot 0-4, up to 5 presets)
	// ------------------------------------------------------------------

	/** Save a loadout preset to the given slot (0-4). */
	UFUNCTION(BlueprintCallable, Category = "SH|SaveSystem")
	bool SaveLoadout(int32 SlotIndex, const FSHLoadoutSaveData& LoadoutData);

	/** Load a loadout preset from the given slot. */
	UFUNCTION(BlueprintCallable, Category = "SH|SaveSystem")
	bool LoadLoadout(int32 SlotIndex, FSHLoadoutSaveData& OutLoadoutData);

	// ------------------------------------------------------------------
	//  Stats save / load
	// ------------------------------------------------------------------

	/** Save lifetime statistics. */
	UFUNCTION(BlueprintCallable, Category = "SH|SaveSystem")
	bool SaveStats(const FSHStatsSaveData& StatsData);

	/** Load lifetime statistics. */
	UFUNCTION(BlueprintCallable, Category = "SH|SaveSystem")
	bool LoadStats(FSHStatsSaveData& OutStatsData);

	// ------------------------------------------------------------------
	//  Auto-save (campaign + progression checkpoint)
	// ------------------------------------------------------------------

	/**
	 * Auto-save campaign and progression data at a mission checkpoint.
	 * Uses the currently active campaign slot. No-op if no campaign is active.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|SaveSystem")
	void AutoSave(const FSHCampaignSaveData& CampaignData, const FSHProgressionSaveData& ProgressionData);

	/** Set the active campaign slot used by AutoSave. */
	UFUNCTION(BlueprintCallable, Category = "SH|SaveSystem")
	void SetActiveCampaignSlot(int32 SlotIndex);

	/** Get the active campaign slot index (-1 if none). */
	UFUNCTION(BlueprintPure, Category = "SH|SaveSystem")
	int32 GetActiveCampaignSlot() const { return ActiveCampaignSlot; }

	// ------------------------------------------------------------------
	//  Async save
	// ------------------------------------------------------------------

	/**
	 * Perform an asynchronous save for large data payloads.
	 * Fires OnSaveComplete when the operation finishes.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|SaveSystem")
	void AsyncSaveCampaign(int32 SlotIndex, const FSHCampaignSaveData& CampaignData);

	// ------------------------------------------------------------------
	//  Slot queries (for UI)
	// ------------------------------------------------------------------

	/** Get the generated slot name string for a given domain and index. */
	UFUNCTION(BlueprintPure, Category = "SH|SaveSystem")
	static FString GetSaveSlotName(const FString& Domain, int32 SlotIndex);

	/** Retrieve lightweight info for all campaign save slots. */
	UFUNCTION(BlueprintCallable, Category = "SH|SaveSystem")
	TArray<FSHSaveSlotInfo> GetAllCampaignSlotInfo() const;

	/** Get metadata for a specific campaign slot. */
	UFUNCTION(BlueprintCallable, Category = "SH|SaveSystem")
	FSHSaveSlotInfo GetSaveSlotInfo(int32 SlotIndex) const;

	/** Get the timestamp of the last successful save operation. */
	UFUNCTION(BlueprintPure, Category = "SH|SaveSystem")
	FDateTime GetLastSaveTime() const { return LastSaveTime; }

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|SaveSystem")
	FOnSHSaveComplete OnSaveComplete;

	UPROPERTY(BlueprintAssignable, Category = "SH|SaveSystem")
	FOnSHLoadComplete OnLoadComplete;

	// ------------------------------------------------------------------
	//  Constants
	// ------------------------------------------------------------------

	static constexpr int32 MaxCampaignSlots = 3;
	static constexpr int32 MaxLoadoutSlots = 5;

private:
	// ------------------------------------------------------------------
	//  Internal helpers
	// ------------------------------------------------------------------

	/** Validate a campaign slot index [0, MaxCampaignSlots). */
	bool IsValidCampaignSlot(int32 SlotIndex) const;

	/** Validate a loadout slot index [0, MaxLoadoutSlots). */
	bool IsValidLoadoutSlot(int32 SlotIndex) const;

	/** Create or retrieve a save game object for writing. */
	USHSaveGame* CreateSaveGameObject() const;

	/** Load a save game object from the named slot. Returns nullptr on failure. */
	USHSaveGame* LoadSaveGameObject(const FString& SlotName) const;

	/** Write a save game object to disk synchronously. */
	bool WriteSaveGameToSlot(USHSaveGame* SaveGame, const FString& SlotName);

	/** Callback for async save completion. */
	void HandleAsyncSaveComplete(const FString& SlotName, const int32 UserIndex, bool bSuccess);

	// ------------------------------------------------------------------
	//  State
	// ------------------------------------------------------------------

	/** Currently active campaign slot for auto-save (-1 = none). */
	int32 ActiveCampaignSlot = -1;

	/** Timestamp of the last successful save. */
	FDateTime LastSaveTime;

	/** User index for save operations (local player). */
	static constexpr int32 SaveUserIndex = 0;

	/** Cached settings loaded on startup. */
	UPROPERTY()
	FSHSettingsSaveData CachedSettings;
};
