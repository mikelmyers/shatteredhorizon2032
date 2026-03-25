// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "ShatteredHorizon2032/Core/SHGameMode.h"
#include "ShatteredHorizon2032/Progression/SHProgressionSystem.h"
#include "SHSaveGame.generated.h"

// ---------------------------------------------------------------------------
//  Campaign save data
// ---------------------------------------------------------------------------

/** Serializable campaign progress snapshot. */
USTRUCT(BlueprintType)
struct FSHCampaignSaveData
{
	GENERATED_BODY()

	/** Index of the current mission in the campaign sequence. */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Campaign")
	int32 CurrentMissionIndex = 0;

	/** Active mission phase. */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Campaign")
	ESHMissionPhase CurrentPhase = ESHMissionPhase::PreInvasion;

	/** GUIDs of objectives the player has completed. */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Campaign")
	TSet<FGuid> CompletedObjectives;

	/** Total seconds played in the current mission. */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Campaign")
	float MissionTimePlayed = 0.f;

	/** Difficulty level (0 = Recruit, 1 = Regular, 2 = Hardened, 3 = Veteran, 4 = Realistic). */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Campaign", meta = (ClampMin = "0", ClampMax = "4"))
	int32 DifficultyLevel = 1;

	/** Whether this save slot has been used at least once. */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Campaign")
	bool bHasBeenStarted = false;
};

// ---------------------------------------------------------------------------
//  Progression save data
// ---------------------------------------------------------------------------

/** Serializable player progression snapshot. */
USTRUCT(BlueprintType)
struct FSHProgressionSaveData
{
	GENERATED_BODY()

	/** XP accumulated toward the next rank. */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Progression")
	int32 CurrentXP = 0;

	/** Total career XP. */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Progression")
	int32 TotalXP = 0;

	/**
	 * Current rank stored as int32 that maps to ESHRank.
	 * Stored as integer to keep the save data decoupled from enum changes.
	 */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Progression")
	int32 CurrentRank = 0;

	/** Set of unlocked item identifiers (attachments, camo patterns, etc.). */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Progression")
	TSet<FName> UnlockedItems;
};

// ---------------------------------------------------------------------------
//  Loadout save data
// ---------------------------------------------------------------------------

/** Serializable loadout preset. */
USTRUCT(BlueprintType)
struct FSHLoadoutSaveData
{
	GENERATED_BODY()

	/** Primary weapon identifier. */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Loadout")
	FName PrimaryWeapon;

	/** Attachments equipped on the primary weapon. */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Loadout")
	TArray<FName> PrimaryAttachments;

	/** Secondary weapon identifier. */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Loadout")
	FName SecondaryWeapon;

	/** Equipment items (grenades, med kits, explosives, etc.). */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Loadout")
	TArray<FName> Equipment;

	/** Camouflage pattern identifier. */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Loadout")
	FName CamoPattern;
};

// ---------------------------------------------------------------------------
//  Settings save data
// ---------------------------------------------------------------------------

/** Serializable player settings. */
USTRUCT(BlueprintType)
struct FSHSettingsSaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Settings|Audio")
	float MasterVolume = 1.0f;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Settings|Audio")
	float SFXVolume = 1.0f;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Settings|Audio")
	float MusicVolume = 0.8f;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Settings|Audio")
	float VoiceVolume = 1.0f;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Settings|Input")
	float MouseSensitivity = 1.0f;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Settings|Input")
	float AimSensitivity = 0.7f;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Settings|Input")
	bool bInvertY = false;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Settings|Display")
	float FOV = 90.0f;

	/** Difficulty level (0 = Recruit through 4 = Realistic). */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Settings|Gameplay", meta = (ClampMin = "0", ClampMax = "4"))
	int32 DifficultyLevel = 1;

	/** Colorblind mode (0 = Off, 1 = Protanopia, 2 = Deuteranopia, 3 = Tritanopia). */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Settings|Accessibility", meta = (ClampMin = "0", ClampMax = "3"))
	int32 ColorblindMode = 0;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Settings|UI", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float UIScale = 1.0f;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Settings|Accessibility")
	bool bSubtitles = true;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Settings|Input")
	bool bControllerAimAssist = true;
};

// ---------------------------------------------------------------------------
//  Stats save data
// ---------------------------------------------------------------------------

/** Serializable lifetime statistics. */
USTRUCT(BlueprintType)
struct FSHStatsSaveData
{
	GENERATED_BODY()

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Stats")
	int32 TotalKills = 0;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Stats")
	int32 TotalDeaths = 0;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Stats")
	float TotalPlayTime = 0.f;

	/** Longest confirmed kill distance in centimeters. */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Stats")
	float LongestKillDistance = 0.f;

	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Stats")
	int32 MissionsCompleted = 0;

	/** Highest difficulty level a mission was completed on. */
	UPROPERTY(SaveGame, EditAnywhere, BlueprintReadWrite, Category = "SaveData|Stats")
	int32 HighestDifficulty = 0;
};

// ---------------------------------------------------------------------------
//  USHSaveGame
// ---------------------------------------------------------------------------

/**
 * USHSaveGame
 *
 * Root save game object for Shattered Horizon 2032. Contains all
 * persistent player data: campaign progress, progression/unlocks,
 * loadout presets, user settings, and lifetime statistics.
 *
 * Designed for use with UGameplayStatics::SaveGameToSlot /
 * LoadGameFromSlot. Each data domain uses its own named slot
 * so that settings and progression persist independently of
 * campaign saves.
 */
UCLASS()
class SHATTEREDHORIZON2032_API USHSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	USHSaveGame();

	// ------------------------------------------------------------------
	//  Save metadata
	// ------------------------------------------------------------------

	/** Player-visible save name (auto-generated or player-entered). */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadWrite, Category = "SaveData|Meta")
	FString SaveDisplayName;

	/** UTC timestamp of the last save operation. */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadWrite, Category = "SaveData|Meta")
	FDateTime SaveTimestamp;

	/** Save format version for forward-compatible deserialization. */
	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadOnly, Category = "SaveData|Meta")
	int32 SaveVersion;

	// ------------------------------------------------------------------
	//  Data sections
	// ------------------------------------------------------------------

	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadWrite, Category = "SaveData")
	FSHCampaignSaveData CampaignData;

	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadWrite, Category = "SaveData")
	FSHProgressionSaveData ProgressionData;

	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadWrite, Category = "SaveData")
	FSHLoadoutSaveData LoadoutData;

	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadWrite, Category = "SaveData")
	FSHSettingsSaveData SettingsData;

	UPROPERTY(SaveGame, VisibleAnywhere, BlueprintReadWrite, Category = "SaveData")
	FSHStatsSaveData StatsData;

	// ------------------------------------------------------------------
	//  Getters
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SaveData")
	const FSHCampaignSaveData& GetCampaignData() const { return CampaignData; }

	UFUNCTION(BlueprintPure, Category = "SaveData")
	const FSHProgressionSaveData& GetProgressionData() const { return ProgressionData; }

	UFUNCTION(BlueprintPure, Category = "SaveData")
	const FSHLoadoutSaveData& GetLoadoutData() const { return LoadoutData; }

	UFUNCTION(BlueprintPure, Category = "SaveData")
	const FSHSettingsSaveData& GetSettingsData() const { return SettingsData; }

	UFUNCTION(BlueprintPure, Category = "SaveData")
	const FSHStatsSaveData& GetStatsData() const { return StatsData; }

	UFUNCTION(BlueprintPure, Category = "SaveData|Meta")
	const FDateTime& GetSaveTimestamp() const { return SaveTimestamp; }

	UFUNCTION(BlueprintPure, Category = "SaveData|Meta")
	int32 GetSaveVersion() const { return SaveVersion; }

	UFUNCTION(BlueprintPure, Category = "SaveData|Meta")
	const FString& GetSaveDisplayName() const { return SaveDisplayName; }

	// ------------------------------------------------------------------
	//  Constants
	// ------------------------------------------------------------------

	/** Current save format version. Increment when changing data layout. */
	static constexpr int32 LatestSaveVersion = 1;
};
