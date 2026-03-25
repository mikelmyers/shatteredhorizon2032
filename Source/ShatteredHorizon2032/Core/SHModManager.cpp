// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHModManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/AssetManager.h"
#include "Engine/StreamableManager.h"
#include "UObject/Package.h"

// Forward reference — the weapon data asset header lives in the Weapons module.
#include "Weapons/SHWeaponData.h"

DEFINE_LOG_CATEGORY_STATIC(LogSH_Mods, Log, All);

// -----------------------------------------------------------------------
//  Subsystem lifecycle
// -----------------------------------------------------------------------

void USHModManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadEnabledModList();
	DiscoverMods();
	UE_LOG(LogSH_Mods, Log, TEXT("Mod manager initialized. %d mods discovered."), AvailableMods.Num());
}

void USHModManager::Deinitialize()
{
	SaveEnabledModList();

	// Unmount all content
	for (const FSHModInfo& Mod : AvailableMods)
	{
		if (MountedPaths.Contains(Mod.ContentPath))
		{
			UnmountModContent(Mod);
		}
	}

	AvailableMods.Empty();
	ModWeaponAssets.Empty();
	MountedPaths.Empty();

	Super::Deinitialize();
}

// -----------------------------------------------------------------------
//  Discovery
// -----------------------------------------------------------------------

void USHModManager::DiscoverMods()
{
	AvailableMods.Empty();

	const FString FullModRoot = FPaths::Combine(FPaths::ProjectSavedDir(), ModRootDirectory);
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	if (!PlatformFile.DirectoryExists(*FullModRoot))
	{
		UE_LOG(LogSH_Mods, Log, TEXT("Mod directory does not exist: %s — creating."), *FullModRoot);
		PlatformFile.CreateDirectoryTree(*FullModRoot);
		return;
	}

	// Iterate subdirectories in the mod root.
	TArray<FString> ModDirectories;
	PlatformFile.IterateDirectory(*FullModRoot, [&ModDirectories](const TCHAR* FilenameOrDir, bool bIsDirectory) -> bool
	{
		if (bIsDirectory)
		{
			ModDirectories.Add(FString(FilenameOrDir));
		}
		return true; // Continue iteration
	});

	// Previously enabled mod IDs (loaded from config)
	TSet<FName> PreviouslyEnabled;
	for (const FSHModInfo& Mod : AvailableMods)
	{
		if (Mod.bEnabled)
		{
			PreviouslyEnabled.Add(Mod.ModId);
		}
	}

	for (const FString& ModDir : ModDirectories)
	{
		const FString ManifestPath = FPaths::Combine(ModDir, ModManifestFilename);
		if (!PlatformFile.FileExists(*ManifestPath))
		{
			UE_LOG(LogSH_Mods, Warning, TEXT("Skipping mod directory with no manifest: %s"), *ModDir);
			continue;
		}

		FSHModInfo ModInfo;
		if (ParseModManifest(ManifestPath, ModInfo))
		{
			ModInfo.ContentPath = ModDir;

			// Restore enabled state from config
			if (PreviouslyEnabled.Contains(ModInfo.ModId))
			{
				ModInfo.bEnabled = true;
			}

			AvailableMods.Add(MoveTemp(ModInfo));
		}
	}

	// Re-apply enabled mod list from config
	LoadEnabledModList();

	UE_LOG(LogSH_Mods, Log, TEXT("Discovered %d mods."), AvailableMods.Num());
}

// -----------------------------------------------------------------------
//  Enable / Disable
// -----------------------------------------------------------------------

bool USHModManager::EnableMod(FName ModId)
{
	FSHModInfo* Mod = FindMod(ModId);
	if (!Mod)
	{
		UE_LOG(LogSH_Mods, Warning, TEXT("EnableMod: mod '%s' not found."), *ModId.ToString());
		return false;
	}

	if (Mod->bEnabled)
	{
		return true; // Already enabled
	}

	Mod->bEnabled = true;

	if (LoadModContent(ModId))
	{
		SaveEnabledModList();
		OnModLoaded.Broadcast(ModId);
		UE_LOG(LogSH_Mods, Log, TEXT("Mod '%s' enabled and loaded."), *ModId.ToString());
		return true;
	}

	// Failed to load — revert
	Mod->bEnabled = false;
	UE_LOG(LogSH_Mods, Error, TEXT("Failed to load content for mod '%s'."), *ModId.ToString());
	return false;
}

bool USHModManager::DisableMod(FName ModId)
{
	FSHModInfo* Mod = FindMod(ModId);
	if (!Mod)
	{
		UE_LOG(LogSH_Mods, Warning, TEXT("DisableMod: mod '%s' not found."), *ModId.ToString());
		return false;
	}

	if (!Mod->bEnabled)
	{
		return true; // Already disabled
	}

	Mod->bEnabled = false;
	UnloadModAssets(ModId);
	UnmountModContent(*Mod);
	SaveEnabledModList();

	OnModUnloaded.Broadcast(ModId);
	UE_LOG(LogSH_Mods, Log, TEXT("Mod '%s' disabled and unloaded."), *ModId.ToString());
	return true;
}

// -----------------------------------------------------------------------
//  Queries
// -----------------------------------------------------------------------

TArray<FSHModInfo> USHModManager::GetEnabledMods() const
{
	TArray<FSHModInfo> Result;
	for (const FSHModInfo& Mod : AvailableMods)
	{
		if (Mod.bEnabled)
		{
			Result.Add(Mod);
		}
	}
	return Result;
}

TArray<FSHModInfo> USHModManager::GetAvailableMods() const
{
	return AvailableMods;
}

bool USHModManager::GetModInfo(FName ModId, FSHModInfo& OutInfo) const
{
	const FSHModInfo* Mod = FindMod(ModId);
	if (Mod)
	{
		OutInfo = *Mod;
		return true;
	}
	return false;
}

// -----------------------------------------------------------------------
//  Content loading
// -----------------------------------------------------------------------

bool USHModManager::LoadModContent(FName ModId)
{
	const FSHModInfo* Mod = FindMod(ModId);
	if (!Mod)
	{
		return false;
	}

	// Mount the content path if not already mounted
	if (!MountedPaths.Contains(Mod->ContentPath))
	{
		if (!MountModContent(*Mod))
		{
			return false;
		}
	}

	// Scan the mod's content directory for weapon data assets
	const FString WeaponDataPath = FPaths::Combine(Mod->ContentPath, TEXT("WeaponData"));
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	TArray<TObjectPtr<USHWeaponDataAsset>>& WeaponAssets = ModWeaponAssets.FindOrAdd(ModId);
	WeaponAssets.Empty();

	if (PlatformFile.DirectoryExists(*WeaponDataPath))
	{
		TArray<FString> AssetFiles;
		PlatformFile.IterateDirectory(*WeaponDataPath,
			[&AssetFiles](const TCHAR* FilenameOrDir, bool bIsDirectory) -> bool
			{
				if (!bIsDirectory)
				{
					FString Filename(FilenameOrDir);
					if (Filename.EndsWith(TEXT(".uasset")))
					{
						AssetFiles.Add(Filename);
					}
				}
				return true;
			});

		for (const FString& AssetFile : AssetFiles)
		{
			const FString AssetName = FPaths::GetBaseFilename(AssetFile);
			const FString PackagePath = FString::Printf(TEXT("/Mods/%s/WeaponData/%s.%s"),
				*ModId.ToString(), *AssetName, *AssetName);

			UObject* LoadedObj = StaticLoadObject(USHWeaponDataAsset::StaticClass(), nullptr, *PackagePath);
			if (USHWeaponDataAsset* WeaponData = Cast<USHWeaponDataAsset>(LoadedObj))
			{
				WeaponAssets.Add(WeaponData);
				UE_LOG(LogSH_Mods, Log, TEXT("Loaded weapon data '%s' from mod '%s'."),
					*AssetName, *ModId.ToString());
			}
		}
	}

	UE_LOG(LogSH_Mods, Log, TEXT("Mod '%s' content loaded: %d weapon assets."),
		*ModId.ToString(), WeaponAssets.Num());
	return true;
}

TArray<USHWeaponDataAsset*> USHModManager::GetModWeaponData(FName ModId) const
{
	TArray<USHWeaponDataAsset*> Result;
	const auto* Found = ModWeaponAssets.Find(ModId);
	if (Found)
	{
		for (const TObjectPtr<USHWeaponDataAsset>& Asset : *Found)
		{
			if (Asset)
			{
				Result.Add(Asset.Get());
			}
		}
	}
	return Result;
}

// -----------------------------------------------------------------------
//  Internal helpers
// -----------------------------------------------------------------------

bool USHModManager::ParseModManifest(const FString& ManifestPath, FSHModInfo& OutInfo) const
{
	FString JsonString;
	if (!FFileHelper::LoadFileToString(JsonString, *ManifestPath))
	{
		UE_LOG(LogSH_Mods, Error, TEXT("Failed to read mod manifest: %s"), *ManifestPath);
		return false;
	}

	TSharedPtr<FJsonObject> JsonObj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonString);
	if (!FJsonSerializer::Deserialize(Reader, JsonObj) || !JsonObj.IsValid())
	{
		UE_LOG(LogSH_Mods, Error, TEXT("Failed to parse mod manifest JSON: %s"), *ManifestPath);
		return false;
	}

	FString IdStr;
	if (!JsonObj->TryGetStringField(TEXT("id"), IdStr))
	{
		UE_LOG(LogSH_Mods, Error, TEXT("Mod manifest missing 'id' field: %s"), *ManifestPath);
		return false;
	}

	OutInfo.ModId = FName(*IdStr);

	FString NameStr;
	if (JsonObj->TryGetStringField(TEXT("name"), NameStr))
	{
		OutInfo.DisplayName = FText::FromString(NameStr);
	}

	FString AuthorStr;
	if (JsonObj->TryGetStringField(TEXT("author"), AuthorStr))
	{
		OutInfo.Author = FText::FromString(AuthorStr);
	}

	JsonObj->TryGetStringField(TEXT("version"), OutInfo.Version);

	return true;
}

bool USHModManager::MountModContent(const FSHModInfo& ModInfo)
{
	// Register the mod content path with the package file system
	const FString MountPoint = FString::Printf(TEXT("/Mods/%s/"), *ModInfo.ModId.ToString());

	FPackageName::RegisterMountPoint(MountPoint, ModInfo.ContentPath + TEXT("/"));
	MountedPaths.Add(ModInfo.ContentPath);

	UE_LOG(LogSH_Mods, Log, TEXT("Mounted mod content: %s -> %s"),
		*MountPoint, *ModInfo.ContentPath);
	return true;
}

void USHModManager::UnmountModContent(const FSHModInfo& ModInfo)
{
	const FString MountPoint = FString::Printf(TEXT("/Mods/%s/"), *ModInfo.ModId.ToString());
	FPackageName::UnRegisterMountPoint(MountPoint, ModInfo.ContentPath + TEXT("/"));
	MountedPaths.Remove(ModInfo.ContentPath);

	UE_LOG(LogSH_Mods, Log, TEXT("Unmounted mod content: %s"), *MountPoint);
}

void USHModManager::UnloadModAssets(FName ModId)
{
	ModWeaponAssets.Remove(ModId);
}

void USHModManager::SaveEnabledModList()
{
	TArray<FString> EnabledIds;
	for (const FSHModInfo& Mod : AvailableMods)
	{
		if (Mod.bEnabled)
		{
			EnabledIds.Add(Mod.ModId.ToString());
		}
	}

	const FString Value = FString::Join(EnabledIds, TEXT(","));
	GConfig->SetString(TEXT("ShatteredHorizon.Mods"), TEXT("EnabledMods"), *Value, GGameUserSettingsIni);
	GConfig->Flush(false, GGameUserSettingsIni);
}

void USHModManager::LoadEnabledModList()
{
	FString Value;
	if (!GConfig->GetString(TEXT("ShatteredHorizon.Mods"), TEXT("EnabledMods"), Value, GGameUserSettingsIni))
	{
		return;
	}

	TArray<FString> EnabledIds;
	Value.ParseIntoArray(EnabledIds, TEXT(","), true);

	TSet<FName> EnabledSet;
	for (const FString& Id : EnabledIds)
	{
		EnabledSet.Add(FName(*Id.TrimStartAndEnd()));
	}

	for (FSHModInfo& Mod : AvailableMods)
	{
		Mod.bEnabled = EnabledSet.Contains(Mod.ModId);
	}
}

FSHModInfo* USHModManager::FindMod(FName ModId)
{
	for (FSHModInfo& Mod : AvailableMods)
	{
		if (Mod.ModId == ModId)
		{
			return &Mod;
		}
	}
	return nullptr;
}

const FSHModInfo* USHModManager::FindMod(FName ModId) const
{
	for (const FSHModInfo& Mod : AvailableMods)
	{
		if (Mod.ModId == ModId)
		{
			return &Mod;
		}
	}
	return nullptr;
}
