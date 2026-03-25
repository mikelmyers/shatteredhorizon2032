// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHTerrainManager.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Components/AudioComponent.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "CollisionQueryParams.h"

DEFINE_LOG_CATEGORY(LogSH_Terrain);

// ======================================================================
//  Construction
// ======================================================================

USHTerrainManager::USHTerrainManager()
{
}

// ======================================================================
//  USubsystem interface
// ======================================================================

void USHTerrainManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	InitializeTerrainProperties();
	InitializeBeachZoneThresholds();

	UE_LOG(LogSH_Terrain, Log, TEXT("TerrainManager initialised. Water level Z=%.0f, Shoreline Y=%.0f"),
		WaterLevelZ, ShorelineYPosition);
}

void USHTerrainManager::Deinitialize()
{
	ActiveFires.Empty();
	Super::Deinitialize();
}

void USHTerrainManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	TickWeatherEffects(DeltaTime);
	TickVegetationFire(DeltaTime);
}

TStatId USHTerrainManager::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(USHTerrainManager, STATGROUP_Tickables);
}

// ======================================================================
//  Initialisation
// ======================================================================

void USHTerrainManager::InitializeTerrainProperties()
{
	// Default properties for each terrain type — designers override via data assets
	auto AddDefault = [this](ESHTerrainType Type, float SpeedMul, float VolMul, float StaminaMul,
							 bool bProne, bool bVehicle, bool bTraces)
	{
		FSHTerrainProperties Props;
		Props.TerrainType = Type;
		Props.MovementSpeedMultiplier = SpeedMul;
		Props.FootstepVolumeMultiplier = VolMul;
		Props.StaminaDrainMultiplier = StaminaMul;
		Props.bProneConcealmentEffective = bProne;
		Props.bVehicleTraversable = bVehicle;
		Props.bLeavesTraces = bTraces;
		TerrainPropertyMap.Add(Type, Props);
	};

	if (TerrainPropertyMap.Num() == 0)
	{
		//                           Type                Speed   Vol    Stam   Prone  Veh    Trace
		AddDefault(ESHTerrainType::DeepWater,			0.3f,	0.4f,  2.0f,  false, false, false);
		AddDefault(ESHTerrainType::ShallowWater,		0.5f,	0.7f,  1.5f,  false, false, false);
		AddDefault(ESHTerrainType::WetSand,				0.8f,	0.6f,  1.2f,  true,  true,  true);
		AddDefault(ESHTerrainType::DrySand,				0.75f,	0.5f,  1.3f,  true,  true,  true);
		AddDefault(ESHTerrainType::Dunes,				0.6f,	0.4f,  1.5f,  true,  false, true);
		AddDefault(ESHTerrainType::VegetationLine,		0.85f,	0.8f,  1.1f,  true,  true,  false);
		AddDefault(ESHTerrainType::Grass,				0.95f,	0.7f,  1.0f,  true,  true,  false);
		AddDefault(ESHTerrainType::Dirt,				1.0f,	0.8f,  1.0f,  true,  true,  true);
		AddDefault(ESHTerrainType::Mud,					0.6f,	0.9f,  1.6f,  true,  true,  true);
		AddDefault(ESHTerrainType::Gravel,				1.0f,	1.2f,  1.0f,  false, true,  false);
		AddDefault(ESHTerrainType::Concrete,			1.0f,	1.3f,  1.0f,  false, true,  false);
		AddDefault(ESHTerrainType::Asphalt,				1.0f,	1.2f,  1.0f,  false, true,  false);
		AddDefault(ESHTerrainType::Metal,				1.0f,	1.5f,  1.0f,  false, true,  false);
		AddDefault(ESHTerrainType::Wood,				1.0f,	1.1f,  1.0f,  false, true,  false);
		AddDefault(ESHTerrainType::Rock,				0.9f,	1.0f,  1.1f,  false, true,  false);
		AddDefault(ESHTerrainType::Rubble,				0.7f,	1.4f,  1.4f,  true,  false, false);
	}
}

void USHTerrainManager::InitializeBeachZoneThresholds()
{
	// Beach zone layout is configured via editor properties.
	// No runtime init needed beyond default values.
}

// ======================================================================
//  Terrain queries
// ======================================================================

FSHTerrainQueryResult USHTerrainManager::QueryTerrain(FVector WorldPosition) const
{
	FSHTerrainQueryResult Result;
	Result.bValid = true;

	// Determine terrain type via downward trace
	Result.TerrainType = DetermineTerrainFromTrace(WorldPosition);
	Result.BeachZone = GetBeachZoneAt(WorldPosition);

	// Water depth
	Result.WaterDepthCM = GetWaterDepthAt(WorldPosition);
	Result.WaterDepth = ClassifyWaterDepth(Result.WaterDepthCM);

	// Override terrain type based on water presence
	if (Result.WaterDepthCM > 160.f)
	{
		Result.TerrainType = ESHTerrainType::DeepWater;
	}
	else if (Result.WaterDepthCM > 10.f)
	{
		Result.TerrainType = ESHTerrainType::ShallowWater;
	}

	// Weather-driven mud conversion
	if (CurrentGroundWetness > 0.6f &&
		(Result.TerrainType == ESHTerrainType::Dirt || Result.TerrainType == ESHTerrainType::DrySand))
	{
		Result.TerrainType = ESHTerrainType::Mud;
	}

	// Elevation
	Result.Elevation = WorldPosition.Z;

	// Surface normal and slope — use the trace result
	UWorld* World = GetWorld();
	if (World)
	{
		FHitResult Hit;
		FVector Start = WorldPosition + FVector(0, 0, 200.f);
		FVector End = WorldPosition - FVector(0, 0, 500.f);
		FCollisionQueryParams Params(SCENE_QUERY_STAT(TerrainQuery), true);

		if (World->LineTraceSingleByChannel(Hit, Start, End, TerrainTraceChannel, Params))
		{
			Result.SurfaceNormal = Hit.ImpactNormal;
			Result.SlopeAngleDegrees = FMath::RadiansToDegrees(FMath::Acos(FVector::DotProduct(Hit.ImpactNormal, FVector::UpVector)));
			Result.Elevation = Hit.ImpactPoint.Z;
		}
	}

	// Properties
	if (const FSHTerrainProperties* Props = TerrainPropertyMap.Find(Result.TerrainType))
	{
		Result.Properties = *Props;
	}

	// Tactical elevation advantage (relative to surrounding mean)
	Result.TacticalElevationAdvantage = FMath::Clamp(Result.Elevation / 500.f, -1.f, 1.f);

	return Result;
}

ESHTerrainType USHTerrainManager::GetTerrainTypeAt(FVector WorldPosition) const
{
	// Fast path: check water first
	float WaterDepth = GetWaterDepthAt(WorldPosition);
	if (WaterDepth > 160.f) return ESHTerrainType::DeepWater;
	if (WaterDepth > 10.f) return ESHTerrainType::ShallowWater;

	// Check beach zone
	ESHBeachZone Zone = GetBeachZoneAt(WorldPosition);
	switch (Zone)
	{
	case ESHBeachZone::Ocean:		return ESHTerrainType::DeepWater;
	case ESHBeachZone::SurfZone:	return ESHTerrainType::ShallowWater;
	case ESHBeachZone::WetSand:		return ESHTerrainType::WetSand;
	case ESHBeachZone::DrySand:		return ESHTerrainType::DrySand;
	case ESHBeachZone::Dunes:		return ESHTerrainType::Dunes;
	case ESHBeachZone::VegetationLine: return ESHTerrainType::VegetationLine;
	default: break;
	}

	// Inland — use physical material trace
	return DetermineTerrainFromTrace(WorldPosition);
}

ESHBeachZone USHTerrainManager::GetBeachZoneAt(FVector WorldPosition) const
{
	// Beach zones are laid out along the Y axis from shoreline inland.
	// ShorelineYPosition is the Y coordinate of the water line.
	float DistFromShoreline = WorldPosition.Y - ShorelineYPosition;

	if (DistFromShoreline < -SurfZoneWidth)
	{
		return ESHBeachZone::Ocean;
	}
	if (DistFromShoreline < 0.f)
	{
		return ESHBeachZone::SurfZone;
	}

	float Cursor = 0.f;

	Cursor += WetSandWidth;
	if (DistFromShoreline < Cursor) return ESHBeachZone::WetSand;

	Cursor += DrySandWidth;
	if (DistFromShoreline < Cursor) return ESHBeachZone::DrySand;

	Cursor += DuneWidth;
	if (DistFromShoreline < Cursor) return ESHBeachZone::Dunes;

	Cursor += VegetationLineWidth;
	if (DistFromShoreline < Cursor) return ESHBeachZone::VegetationLine;

	return ESHBeachZone::Inland;
}

float USHTerrainManager::GetWaterDepthAt(FVector WorldPosition) const
{
	float Depth = WaterLevelZ - WorldPosition.Z;
	return FMath::Max(0.f, Depth);
}

ESHWaterDepth USHTerrainManager::ClassifyWaterDepth(float DepthCM) const
{
	if (DepthCM <= 0.f) return ESHWaterDepth::None;
	if (DepthCM < 30.f) return ESHWaterDepth::Ankle;
	if (DepthCM < 60.f) return ESHWaterDepth::Knee;
	if (DepthCM < 120.f) return ESHWaterDepth::Waist;
	if (DepthCM < 160.f) return ESHWaterDepth::Chest;
	return ESHWaterDepth::Swimming;
}

float USHTerrainManager::GetElevationAt(FVector2D WorldXY) const
{
	UWorld* World = GetWorld();
	if (!World) return 0.f;

	FHitResult Hit;
	FVector Start(WorldXY.X, WorldXY.Y, 50000.f);
	FVector End(WorldXY.X, WorldXY.Y, -50000.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ElevationQuery), true);

	if (World->LineTraceSingleByChannel(Hit, Start, End, TerrainTraceChannel, Params))
	{
		return Hit.ImpactPoint.Z;
	}

	return 0.f;
}

float USHTerrainManager::ComputeElevationAdvantage(FVector AttackerPos, FVector DefenderPos) const
{
	float HeightDiff = AttackerPos.Z - DefenderPos.Z;
	float HorizontalDist = FVector2D::Distance(FVector2D(AttackerPos), FVector2D(DefenderPos));

	if (HorizontalDist < KINDA_SMALL_NUMBER) return 0.f;

	// Angle in degrees — positive means attacker has high ground
	float AngleDeg = FMath::RadiansToDegrees(FMath::Atan2(HeightDiff, HorizontalDist));

	// Normalize to a -1..1 advantage factor
	// +15 degrees or more = full advantage, -15 degrees = full disadvantage
	return FMath::Clamp(AngleDeg / 15.f, -1.f, 1.f);
}

float USHTerrainManager::GetMovementSpeedMultiplier(FVector WorldPosition) const
{
	ESHTerrainType Type = GetTerrainTypeAt(WorldPosition);

	if (const FSHTerrainProperties* Props = TerrainPropertyMap.Find(Type))
	{
		float BaseSpeed = Props->MovementSpeedMultiplier;

		// Weather penalty: rain on non-paved surfaces
		if (CurrentGroundWetness > 0.3f)
		{
			bool bIsHardSurface = (Type == ESHTerrainType::Concrete || Type == ESHTerrainType::Asphalt ||
								   Type == ESHTerrainType::Metal || Type == ESHTerrainType::Rock);
			if (!bIsHardSurface)
			{
				BaseSpeed *= FMath::Lerp(1.f, 0.7f, CurrentGroundWetness);
			}
		}

		return BaseSpeed;
	}

	return 1.f;
}

FSHTerrainProperties USHTerrainManager::GetTerrainProperties(ESHTerrainType Type) const
{
	if (const FSHTerrainProperties* Props = TerrainPropertyMap.Find(Type))
	{
		return *Props;
	}

	return FSHTerrainProperties();
}

// ======================================================================
//  Footstep sounds
// ======================================================================

USoundBase* USHTerrainManager::GetFootstepSound(ESHTerrainType TerrainType, bool bIsSprinting, bool bIsCrawling) const
{
	const FSHFootstepSoundSet* SoundSet = nullptr;
	for (const FSHFootstepSoundSet& Set : FootstepSoundSets)
	{
		if (Set.TerrainType == TerrainType)
		{
			SoundSet = &Set;
			break;
		}
	}

	if (!SoundSet) return nullptr;

	const TArray<TSoftObjectPtr<USoundBase>>* Sounds = nullptr;

	if (bIsCrawling)
	{
		Sounds = &SoundSet->CrawlSounds;
	}
	else if (bIsSprinting)
	{
		Sounds = &SoundSet->RunSounds;
	}
	else
	{
		Sounds = &SoundSet->WalkSounds;
	}

	if (!Sounds || Sounds->Num() == 0) return nullptr;

	int32 Index = FMath::RandRange(0, Sounds->Num() - 1);
	const TSoftObjectPtr<USoundBase>& SoundRef = (*Sounds)[Index];

	if (SoundRef.IsValid())
	{
		return SoundRef.Get();
	}

	// Synchronous load fallback — in production, pre-load via streaming manager
	return SoundRef.LoadSynchronous();
}

float USHTerrainManager::GetFootstepVolumeMultiplier(FVector WorldPosition) const
{
	ESHTerrainType Type = GetTerrainTypeAt(WorldPosition);

	float BaseMul = 1.f;
	if (const FSHTerrainProperties* Props = TerrainPropertyMap.Find(Type))
	{
		BaseMul = Props->FootstepVolumeMultiplier;
	}

	// Rain masks footstep sounds
	if (CurrentRainIntensity > 0.2f)
	{
		BaseMul *= FMath::Lerp(1.f, 0.5f, FMath::Clamp(CurrentRainIntensity, 0.f, 1.f));
	}

	// Wet surfaces are quieter for soft terrain, louder for hard terrain
	if (CurrentGroundWetness > 0.3f)
	{
		bool bIsSoft = (Type == ESHTerrainType::DrySand || Type == ESHTerrainType::Dirt ||
						Type == ESHTerrainType::Grass || Type == ESHTerrainType::Dunes);
		if (bIsSoft)
		{
			BaseMul *= 0.85f;
		}
		else
		{
			BaseMul *= 1.1f; // Wet concrete/metal is slightly louder (splashing)
		}
	}

	return BaseMul;
}

// ======================================================================
//  Weather effects on terrain
// ======================================================================

void USHTerrainManager::OnWeatherChanged(ESHTerrainType PreviousWeather, float RainIntensity, float WindSpeed)
{
	CurrentRainIntensity = FMath::Clamp(RainIntensity, 0.f, 1.f);
	CurrentWindSpeed = WindSpeed;

	UE_LOG(LogSH_Terrain, Log, TEXT("Weather changed: Rain=%.2f, Wind=%.1f"), RainIntensity, WindSpeed);
}

void USHTerrainManager::TickWeatherEffects(float DeltaTime)
{
	// Update ground wetness based on rain
	if (CurrentRainIntensity > MudGenerationRainThreshold)
	{
		float WetnessIncrease = WetnessIncreaseRate * CurrentRainIntensity * DeltaTime;
		CurrentGroundWetness = FMath::Min(1.f, CurrentGroundWetness + WetnessIncrease);
	}
	else
	{
		float WetnessDecrease = WetnessDryRate * DeltaTime;
		CurrentGroundWetness = FMath::Max(0.f, CurrentGroundWetness - WetnessDecrease);
	}
}

// ======================================================================
//  Vegetation interaction
// ======================================================================

void USHTerrainManager::InteractWithVegetation(FVector WorldPosition, float InteractionRadius)
{
	// In production, this sends data to the foliage interaction system to bend/part grass.
	// For now, log the interaction for the VFX system to pick up.
	UE_LOG(LogSH_Terrain, Verbose, TEXT("Vegetation interaction at %s, radius %.0f"),
		*WorldPosition.ToString(), InteractionRadius);
}

void USHTerrainManager::IgniteVegetation(FVector WorldPosition, float Radius)
{
	// Check if fire already exists nearby — merge rather than duplicate
	for (FSHVegetationInstance& Fire : ActiveFires)
	{
		if (FVector::Dist(Fire.Location, WorldPosition) < Radius)
		{
			Fire.bOnFire = true;
			Fire.Health = FMath::Max(Fire.Health, 0.5f);
			return;
		}
	}

	// Wet conditions inhibit fire
	if (CurrentGroundWetness > 0.7f)
	{
		UE_LOG(LogSH_Terrain, Log, TEXT("Ignition failed at %s — ground too wet (%.2f)"),
			*WorldPosition.ToString(), CurrentGroundWetness);
		return;
	}

	FSHVegetationInstance NewFire;
	NewFire.Location = WorldPosition;
	NewFire.Health = 1.f;
	NewFire.bOnFire = true;
	NewFire.FireSpreadTimer = 0.f;
	ActiveFires.Add(NewFire);

	UE_LOG(LogSH_Terrain, Log, TEXT("Vegetation ignited at %s"), *WorldPosition.ToString());
}

void USHTerrainManager::TickVegetationFire(float DeltaTime)
{
	for (int32 i = ActiveFires.Num() - 1; i >= 0; --i)
	{
		FSHVegetationInstance& Fire = ActiveFires[i];

		if (!Fire.bOnFire) continue;

		// Consume vegetation health
		float BurnRate = 0.1f * DeltaTime;

		// Wind accelerates fire
		BurnRate *= FMath::Lerp(1.f, 2.f, FMath::Clamp(CurrentWindSpeed / 20.f, 0.f, 1.f));

		// Rain suppresses fire
		BurnRate *= FMath::Lerp(1.f, 0.1f, FMath::Clamp(CurrentRainIntensity, 0.f, 1.f));

		Fire.Health -= BurnRate;

		// Fire spread
		Fire.FireSpreadTimer += DeltaTime;
		if (Fire.FireSpreadTimer > 5.f && Fire.Health > 0.2f)
		{
			float SpreadRadius = FireSpreadRateCMPerSec * Fire.FireSpreadTimer;
			if (SpreadRadius < MaxFireSpreadRadius)
			{
				// Attempt to spread in wind direction
				FVector SpreadDir = FVector(FMath::Cos(FMath::DegreesToRadians(CurrentWindSpeed)),
											FMath::Sin(FMath::DegreesToRadians(CurrentWindSpeed)), 0.f);
				FVector SpreadPos = Fire.Location + SpreadDir * SpreadRadius;

				// Only spread into vegetation zones
				ESHBeachZone Zone = GetBeachZoneAt(SpreadPos);
				if (Zone == ESHBeachZone::VegetationLine || Zone == ESHBeachZone::Inland)
				{
					IgniteVegetation(SpreadPos, FireSpreadRateCMPerSec);
				}
			}
		}

		// Remove burned-out fires
		if (Fire.Health <= 0.f)
		{
			Fire.bOnFire = false;
			ActiveFires.RemoveAt(i);
		}
	}
}

// ======================================================================
//  Internal helpers
// ======================================================================

ESHTerrainType USHTerrainManager::DetermineTerrainFromPhysMat(const UPhysicalMaterial* PhysMat) const
{
	if (!PhysMat) return ESHTerrainType::Dirt;

	// Map physical material surface type to our terrain type.
	// In production this uses a data table lookup keyed on the phys mat's SurfaceType.
	FName MatName = PhysMat->GetFName();
	FString NameStr = MatName.ToString().ToLower();

	if (NameStr.Contains(TEXT("sand")))		return ESHTerrainType::DrySand;
	if (NameStr.Contains(TEXT("concrete")))	return ESHTerrainType::Concrete;
	if (NameStr.Contains(TEXT("asphalt")))	return ESHTerrainType::Asphalt;
	if (NameStr.Contains(TEXT("metal")))		return ESHTerrainType::Metal;
	if (NameStr.Contains(TEXT("wood")))		return ESHTerrainType::Wood;
	if (NameStr.Contains(TEXT("gravel")))	return ESHTerrainType::Gravel;
	if (NameStr.Contains(TEXT("rock")))		return ESHTerrainType::Rock;
	if (NameStr.Contains(TEXT("mud")))		return ESHTerrainType::Mud;
	if (NameStr.Contains(TEXT("grass")))		return ESHTerrainType::Grass;
	if (NameStr.Contains(TEXT("rubble")))	return ESHTerrainType::Rubble;
	if (NameStr.Contains(TEXT("water")))		return ESHTerrainType::ShallowWater;

	return ESHTerrainType::Dirt;
}

ESHTerrainType USHTerrainManager::DetermineTerrainFromTrace(FVector WorldPosition) const
{
	UWorld* World = GetWorld();
	if (!World) return ESHTerrainType::Dirt;

	FHitResult Hit;
	FVector Start = WorldPosition + FVector(0, 0, 200.f);
	FVector End = WorldPosition - FVector(0, 0, 500.f);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TerrainTypeQuery), true);

	if (World->LineTraceSingleByChannel(Hit, Start, End, TerrainTraceChannel, Params))
	{
		if (Hit.PhysMaterial.IsValid())
		{
			return DetermineTerrainFromPhysMat(Hit.PhysMaterial.Get());
		}
	}

	return ESHTerrainType::Dirt;
}
