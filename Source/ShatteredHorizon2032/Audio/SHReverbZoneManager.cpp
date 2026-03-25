// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHReverbZoneManager.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/ReverbEffect.h"
#include "AudioDevice.h"
#include "DrawDebugHelpers.h"

// =====================================================================
//  Static data — trace directions
// =====================================================================

const FVector USHReverbZoneManager::TraceDirections[6] = {
	FVector::ForwardVector,   // Forward
	FVector::BackwardVector,  // Back
	FVector::RightVector,     // Left (negative Y in UE is left, but we use RightVector for clarity)
	FVector::LeftVector,      // Right
	FVector::UpVector,        // Up
	FVector::DownVector       // Down
};

// =====================================================================
//  Constructor
// =====================================================================

USHReverbZoneManager::USHReverbZoneManager()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickGroup = TG_PostPhysics;

	TraceHitDistances.SetNum(NumTraceDirections);
	for (int32 i = 0; i < NumTraceDirections; ++i)
	{
		TraceHitDistances[i] = TraceDistance;
	}
}

// =====================================================================
//  UActorComponent overrides
// =====================================================================

void USHReverbZoneManager::BeginPlay()
{
	Super::BeginPlay();

	// Initialize hit distances to max trace distance.
	TraceHitDistances.SetNum(NumTraceDirections);
	for (int32 i = 0; i < NumTraceDirections; ++i)
	{
		TraceHitDistances[i] = TraceDistance;
	}

	// Perform an initial trace immediately.
	PerformEnvironmentTraces();
	CurrentEnvironmentType = ClassifyEnvironment();
	PreviousEnvironmentType = CurrentEnvironmentType;
}

void USHReverbZoneManager::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Rate-limited trace updates.
	TimeSinceLastUpdate += DeltaTime;
	if (TimeSinceLastUpdate >= UpdateInterval)
	{
		TimeSinceLastUpdate = 0.f;
		PerformEnvironmentTraces();

		// Classify environment.
		const ESHEnvironmentType NewType = ClassifyEnvironment();
		if (NewType != CurrentEnvironmentType)
		{
			PreviousEnvironmentType = CurrentEnvironmentType;
			CurrentEnvironmentType = NewType;
			CurrentReverbBlendWeight = 0.f;
			TargetReverbBlendWeight = 1.f;
			OnEnvironmentTypeChanged.Broadcast(PreviousEnvironmentType, CurrentEnvironmentType);
		}
	}

	// Smooth enclosure ratio.
	const float PreviousEnclosure = EnclosureRatio;
	EnclosureRatio = FMath::FInterpTo(EnclosureRatio, RawEnclosureRatio, DeltaTime, EnclosureInterpSpeed);
	if (FMath::Abs(EnclosureRatio - PreviousEnclosure) > 0.005f)
	{
		OnEnclosureRatioChanged.Broadcast(EnclosureRatio);
	}

	// Update reverb blending.
	UpdateReverbSettings(DeltaTime);
}

// =====================================================================
//  Environment traces
// =====================================================================

void USHReverbZoneManager::PerformEnvironmentTraces()
{
	const AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	const UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const FVector Origin = Owner->GetActorLocation();

	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Owner);
	QueryParams.bTraceComplex = false;

	int32 HitCount = 0;
	float TotalHitRatio = 0.f;

	for (int32 i = 0; i < NumTraceDirections; ++i)
	{
		const FVector TraceEnd = Origin + TraceDirections[i] * TraceDistance;
		FHitResult HitResult;

		const bool bHit = World->LineTraceSingleByChannel(
			HitResult,
			Origin,
			TraceEnd,
			TraceChannel,
			QueryParams);

		if (bHit)
		{
			TraceHitDistances[i] = HitResult.Distance;
			++HitCount;
			// Closer hits = more enclosed.
			TotalHitRatio += 1.f - FMath::Clamp(HitResult.Distance / TraceDistance, 0.f, 1.f);
		}
		else
		{
			TraceHitDistances[i] = TraceDistance;
		}
	}

	// Compute raw enclosure ratio.
	// Weight: a direction counts fully enclosed if hit, and its contribution scales by proximity.
	if (HitCount > 0)
	{
		RawEnclosureRatio = TotalHitRatio / static_cast<float>(NumTraceDirections);
	}
	else
	{
		RawEnclosureRatio = 0.f;
	}

	RawEnclosureRatio = FMath::Clamp(RawEnclosureRatio, 0.f, 1.f);
}

// =====================================================================
//  Environment classification
// =====================================================================

ESHEnvironmentType USHReverbZoneManager::ClassifyEnvironment() const
{
	// Trace indices: 0=Forward, 1=Back, 2=Right, 3=Left, 4=Up, 5=Down
	const float ForwardDist = TraceHitDistances[0];
	const float BackDist = TraceHitDistances[1];
	const float RightDist = TraceHitDistances[2];
	const float LeftDist = TraceHitDistances[3];
	const float UpDist = TraceHitDistances[4];
	const float DownDist = TraceHitDistances[5];

	const float HorizontalAvg = (ForwardDist + BackDist + RightDist + LeftDist) * 0.25f;
	const bool bCeilingClose = UpDist < EnclosedThreshold;
	const bool bFloorClose = DownDist < EnclosedThreshold;

	// Count how many horizontal directions have nearby walls.
	int32 NearWallCount = 0;
	if (ForwardDist < EnclosedThreshold) ++NearWallCount;
	if (BackDist < EnclosedThreshold) ++NearWallCount;
	if (RightDist < EnclosedThreshold) ++NearWallCount;
	if (LeftDist < EnclosedThreshold) ++NearWallCount;

	// Tunnel: ceiling close, floor close, most horizontal directions enclosed,
	// but at least one horizontal direction is open (the tunnel length).
	if (bCeilingClose && NearWallCount >= 2 && NearWallCount <= 3)
	{
		// Check if there's at least one open horizontal direction (the tunnel axis).
		const bool bHasOpenAxis = (ForwardDist >= EnclosedThreshold) || (BackDist >= EnclosedThreshold)
			|| (RightDist >= EnclosedThreshold) || (LeftDist >= EnclosedThreshold);
		if (bHasOpenAxis)
		{
			return ESHEnvironmentType::Tunnel;
		}
	}

	// Interior: all directions enclosed (ceiling + most horizontal).
	if (bCeilingClose && NearWallCount >= 3)
	{
		return ESHEnvironmentType::Interior;
	}

	// Urban Canyon: lateral walls close, but ceiling open.
	// Detect parallel walls: either (left+right close) or (forward+back close) with open sky.
	const bool bLateralWalls = (RightDist < UrbanCanyonWallThreshold && LeftDist < UrbanCanyonWallThreshold);
	const bool bLongitudinalWalls = (ForwardDist < UrbanCanyonWallThreshold && BackDist < UrbanCanyonWallThreshold);

	if (!bCeilingClose && (bLateralWalls || bLongitudinalWalls))
	{
		return ESHEnvironmentType::UrbanCanyon;
	}

	// Forest: moderate horizontal occlusion from many directions, but open sky and
	// hits at medium range (trees). Characterized by medium enclosure without
	// clear wall structure.
	if (!bCeilingClose && NearWallCount == 0 && HorizontalAvg < TraceDistance * 0.7f && HorizontalAvg > EnclosedThreshold)
	{
		return ESHEnvironmentType::Forest;
	}

	// Default: open field.
	return ESHEnvironmentType::OpenField;
}

// =====================================================================
//  Reverb settings update
// =====================================================================

void USHReverbZoneManager::UpdateReverbSettings(float DeltaTime)
{
	// Blend toward the target reverb weight.
	CurrentReverbBlendWeight = FMath::FInterpTo(
		CurrentReverbBlendWeight, TargetReverbBlendWeight,
		DeltaTime, ReverbBlendSpeed);

	const FSHReverbPreset* CurrentPreset = FindPresetForType(CurrentEnvironmentType);
	if (!CurrentPreset || !CurrentPreset->ReverbEffect)
	{
		return;
	}

	// Apply the reverb effect as an audio volume override.
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FAudioDevice* AudioDevice = World->GetAudioDevice().GetAudioDevice();
	if (!AudioDevice)
	{
		return;
	}

	// Use the activate reverb effect API with blended volume.
	const float BlendedVolume = CurrentPreset->Volume * CurrentReverbBlendWeight * EnclosureRatio;
	AudioDevice->ActivateReverbEffect(
		CurrentPreset->ReverbEffect,
		FName(TEXT("SHReverbZone")),
		CurrentPreset->Priority,
		BlendedVolume,
		-1.f); // FadeTime — negative means instant within the blend we control.
}

// =====================================================================
//  Preset lookup
// =====================================================================

const FSHReverbPreset* USHReverbZoneManager::FindPresetForType(ESHEnvironmentType Type) const
{
	for (const FSHReverbPreset& Preset : ReverbPresets)
	{
		if (Preset.EnvironmentType == Type)
		{
			return &Preset;
		}
	}
	return nullptr;
}

FSHReverbPreset USHReverbZoneManager::GetActiveReverbPreset() const
{
	const FSHReverbPreset* Preset = FindPresetForType(CurrentEnvironmentType);
	if (Preset)
	{
		return *Preset;
	}
	return FSHReverbPreset();
}

// =====================================================================
//  Configuration setters
// =====================================================================

void USHReverbZoneManager::SetTraceDistance(float NewDistance)
{
	TraceDistance = FMath::Max(NewDistance, 100.f);
}

void USHReverbZoneManager::SetUpdateInterval(float NewInterval)
{
	UpdateInterval = FMath::Max(NewInterval, 0.01f);
}
