// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SHModManager.generated.h"

class USHWeaponDataAsset;

/* -----------------------------------------------------------------------
 *  Structs
 * --------------------------------------------------------------------- */

USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHModInfo
{
	GENERATED_BODY()

	/** Unique identifier for this mod. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mod")
	FName ModId;

	/** User-facing display name. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mod")
	FText DisplayName;

	/** Mod author. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mod")
	FText Author;

	/** Semantic version string (e.g. "1.2.0"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mod")
	FString Version;

	/** Whether this mod is currently enabled. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Mod")
	bool bEnabled = false;

	/** Root content path for this mod's assets. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Mod")
	FString ContentPath;
};

/* -----------------------------------------------------------------------
 *  Delegates
 * --------------------------------------------------------------------- */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnModLoaded, FName, ModId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnModUnloaded, FName, ModId);

/* -----------------------------------------------------------------------
 *  USHModManager
 * --------------------------------------------------------------------- */

/**
 * USHModManager
 *
 * Game instance subsystem responsible for discovering, loading, and
 * managing mods. Mods may provide weapon tuning DataAssets, AI parameter
 * overrides, and other data-driven content.
 */
UCLASS()
class SHATTEREDHORIZON2032_API USHModManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ------------------------------------------------------------------
	//  USubsystem overrides
	// ------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ------------------------------------------------------------------
	//  Discovery
	// ------------------------------------------------------------------

	/** Scan the mod directory for available mods and populate the list. */
	UFUNCTION(BlueprintCallable, Category = "SH|Mods")
	void DiscoverMods();

	// ------------------------------------------------------------------
	//  Enable / Disable
	// ------------------------------------------------------------------

	/** Enable a mod by its unique identifier. */
	UFUNCTION(BlueprintCallable, Category = "SH|Mods")
	bool EnableMod(FName ModId);

	/** Disable a mod by its unique identifier. */
	UFUNCTION(BlueprintCallable, Category = "SH|Mods")
	bool DisableMod(FName ModId);

	// ------------------------------------------------------------------
	//  Queries
	// ------------------------------------------------------------------

	/** Get list of all enabled mods. */
	UFUNCTION(BlueprintCallable, Category = "SH|Mods")
	TArray<FSHModInfo> GetEnabledMods() const;

	/** Get list of all available mods (enabled and disabled). */
	UFUNCTION(BlueprintCallable, Category = "SH|Mods")
	TArray<FSHModInfo> GetAvailableMods() const;

	/** Find mod info by ID. Returns false if not found. */
	UFUNCTION(BlueprintCallable, Category = "SH|Mods")
	bool GetModInfo(FName ModId, FSHModInfo& OutInfo) const;

	// ------------------------------------------------------------------
	//  Content loading
	// ------------------------------------------------------------------

	/**
	 * Load mod content for the given mod.
	 * This mounts the mod's content path and loads any DataAssets
	 * (weapon data, AI parameters) registered by the mod.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Mods")
	bool LoadModContent(FName ModId);

	/**
	 * Retrieve all weapon data assets loaded from a specific mod.
	 * This exposes USHWeaponDataAsset for modding (weapon tuning).
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Mods")
	TArray<USHWeaponDataAsset*> GetModWeaponData(FName ModId) const;

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Mods")
	FOnModLoaded OnModLoaded;

	UPROPERTY(BlueprintAssignable, Category = "SH|Mods")
	FOnModUnloaded OnModUnloaded;

protected:
	/** Root directory where mods are discovered. Relative to project saved dir. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Mods|Config")
	FString ModRootDirectory = TEXT("Mods");

	/** Filename expected in each mod folder containing metadata. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Mods|Config")
	FString ModManifestFilename = TEXT("ModManifest.json");

private:
	/** Parse a mod manifest JSON file and populate an FSHModInfo. */
	bool ParseModManifest(const FString& ManifestPath, FSHModInfo& OutInfo) const;

	/** Mount a mod's content directory into the asset registry. */
	bool MountModContent(const FSHModInfo& ModInfo);

	/** Unmount a mod's content directory. */
	void UnmountModContent(const FSHModInfo& ModInfo);

	/** Unload all assets from a specific mod. */
	void UnloadModAssets(FName ModId);

	/** Save enabled mod list to config. */
	void SaveEnabledModList();

	/** Load enabled mod list from config. */
	void LoadEnabledModList();

	/** Find a mod by ID (mutable). */
	FSHModInfo* FindMod(FName ModId);
	const FSHModInfo* FindMod(FName ModId) const;

	/** All discovered mods. */
	UPROPERTY()
	TArray<FSHModInfo> AvailableMods;

	/** Mod ID -> loaded weapon data assets. */
	TMap<FName, TArray<TObjectPtr<USHWeaponDataAsset>>> ModWeaponAssets;

	/** Set of currently mounted mod content paths. */
	TSet<FString> MountedPaths;
};
