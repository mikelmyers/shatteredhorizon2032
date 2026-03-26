// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHFootstepSystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"

USHFootstepSystem::USHFootstepSystem()
{
	PrimaryComponentTick.bCanEverTick = false; // Driven by AnimNotify, not tick.
}

void USHFootstepSystem::BeginPlay()
{
	Super::BeginPlay();

	if (FootstepDatabase.Num() == 0)
	{
		InitDefaultDatabase();
	}
}

// =====================================================================
//  Primary interface
// =====================================================================

void USHFootstepSystem::PlayFootstep(bool bIsRightFoot)
{
	FVector ImpactPoint;
	CurrentSurface = DetectSurface(ImpactPoint);

	const FSHFootstepSoundEntry* Entry = FindFootstepEntry(CurrentSurface);
	if (!Entry)
	{
		return;
	}

	// Select sound array based on movement mode.
	const TArray<TSoftObjectPtr<USoundBase>>* SoundArray = nullptr;
	const EMovementMode Mode = GetCurrentMovementMode();

	switch (Mode)
	{
	case EMovementMode::Sprint:  SoundArray = &Entry->SprintSounds; break;
	case EMovementMode::Run:     SoundArray = &Entry->RunSounds; break;
	case EMovementMode::Crouch:  SoundArray = &Entry->CrouchSounds; break;
	case EMovementMode::Prone:   SoundArray = &Entry->ProneSounds; break;
	default:                     SoundArray = &Entry->WalkSounds; break;
	}

	// Fall back to walk sounds if specific array is empty.
	if (!SoundArray || SoundArray->Num() == 0)
	{
		SoundArray = &Entry->WalkSounds;
	}
	if (!SoundArray || SoundArray->Num() == 0)
	{
		return;
	}

	// Random sound selection (avoid repeating the same sound twice).
	const int32 SoundIndex = FMath::RandRange(0, SoundArray->Num() - 1);
	USoundBase* Sound = (*SoundArray)[SoundIndex].LoadSynchronous();
	if (!Sound)
	{
		return;
	}

	// Calculate volume.
	float Volume = Entry->VolumeMultiplier;
	switch (Mode)
	{
	case EMovementMode::Sprint: Volume *= SprintVolumeMultiplier; break;
	case EMovementMode::Crouch: Volume *= CrouchVolumeMultiplier; break;
	case EMovementMode::Prone:  Volume *= ProneVolumeMultiplier; break;
	default: break;
	}

	// Slight pitch randomization for natural variation.
	const float Pitch = FMath::RandRange(0.93f, 1.07f);

	UGameplayStatics::PlaySoundAtLocation(
		GetWorld(), Sound, ImpactPoint, Volume, Pitch);

	// Spawn footstep VFX (dust, splash, etc.).
	if (Entry->StepVFX.IsValid())
	{
		if (UParticleSystem* VFX = Entry->StepVFX.LoadSynchronous())
		{
			UGameplayStatics::SpawnEmitterAtLocation(
				GetWorld(), VFX, ImpactPoint, FRotator::ZeroRotator,
				FVector(0.5f), true);
		}
	}

	// Report noise to AI perception system.
	if (AActor* Owner = GetOwner())
	{
		const float NoiseVolume = Volume * Entry->DetectionRangeMultiplier;
		Owner->MakeNoise(NoiseVolume, Cast<APawn>(Owner), ImpactPoint);
	}

	OnFootstepPlayed.Broadcast(CurrentSurface, ImpactPoint, Volume);
}

void USHFootstepSystem::PlayLanding(float FallSpeed)
{
	FVector ImpactPoint;
	CurrentSurface = DetectSurface(ImpactPoint);

	const FSHFootstepSoundEntry* Entry = FindFootstepEntry(CurrentSurface);
	if (!Entry || Entry->LandingSounds.Num() == 0)
	{
		return;
	}

	const int32 Idx = FMath::RandRange(0, Entry->LandingSounds.Num() - 1);
	USoundBase* Sound = Entry->LandingSounds[Idx].LoadSynchronous();
	if (!Sound) return;

	// Landing volume scales with fall speed.
	const float FallFraction = FMath::Clamp(FallSpeed / 1000.0f, 0.3f, 2.0f);
	const float Volume = Entry->VolumeMultiplier * FallFraction;

	UGameplayStatics::PlaySoundAtLocation(GetWorld(), Sound, ImpactPoint, Volume);

	if (AActor* Owner = GetOwner())
	{
		Owner->MakeNoise(Volume * Entry->DetectionRangeMultiplier * 1.5f,
			Cast<APawn>(Owner), ImpactPoint);
	}
}

// =====================================================================
//  Queries
// =====================================================================

float USHFootstepSystem::GetCurrentFootstepVolume() const
{
	const FSHFootstepSoundEntry* Entry = FindFootstepEntry(CurrentSurface);
	if (!Entry) return 1.0f;

	float Volume = Entry->VolumeMultiplier;
	switch (GetCurrentMovementMode())
	{
	case EMovementMode::Sprint: Volume *= SprintVolumeMultiplier; break;
	case EMovementMode::Crouch: Volume *= CrouchVolumeMultiplier; break;
	case EMovementMode::Prone:  Volume *= ProneVolumeMultiplier; break;
	default: break;
	}
	return Volume;
}

// =====================================================================
//  Surface detection
// =====================================================================

ESHSurfaceType USHFootstepSystem::DetectSurface(FVector& OutImpactPoint) const
{
	const AActor* Owner = GetOwner();
	if (!Owner) return ESHSurfaceType::Concrete;

	const UWorld* World = GetWorld();
	if (!World) return ESHSurfaceType::Concrete;

	const FVector Start = Owner->GetActorLocation();
	const FVector End = Start - FVector(0.0f, 0.0f, TraceDistance);

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(FootstepSurface), true);
	Params.AddIgnoredActor(Owner);
	Params.bReturnPhysicalMaterial = true; // Critical: request physical material.

	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		OutImpactPoint = Hit.ImpactPoint;

		// Check physical material for surface type.
		if (const UPhysicalMaterial* PhysMat = Hit.PhysMaterial.Get())
		{
			const EPhysicalSurface SurfaceType = PhysMat->SurfaceType;
			if (const ESHSurfaceType* MappedType = PhysMatMapping.Find(SurfaceType))
			{
				return *MappedType;
			}
		}

		// Fallback: check material name for hints.
		if (const UPrimitiveComponent* Comp = Hit.GetComponent())
		{
			const FString MatName = Comp->GetName().ToLower();
			if (MatName.Contains(TEXT("metal"))) return ESHSurfaceType::Metal;
			if (MatName.Contains(TEXT("wood"))) return ESHSurfaceType::Wood;
			if (MatName.Contains(TEXT("sand"))) return ESHSurfaceType::Sand;
			if (MatName.Contains(TEXT("grass"))) return ESHSurfaceType::Grass;
			if (MatName.Contains(TEXT("water"))) return ESHSurfaceType::Water;
		}
	}

	OutImpactPoint = Start - FVector(0, 0, TraceDistance);
	return ESHSurfaceType::Concrete; // Default fallback.
}

const FSHFootstepSoundEntry* USHFootstepSystem::FindFootstepEntry(ESHSurfaceType Surface) const
{
	for (const FSHFootstepSoundEntry& Entry : FootstepDatabase)
	{
		if (Entry.Surface == Surface)
		{
			return &Entry;
		}
	}
	return nullptr;
}

USHFootstepSystem::EMovementMode USHFootstepSystem::GetCurrentMovementMode() const
{
	const ACharacter* Char = Cast<ACharacter>(GetOwner());
	if (!Char) return EMovementMode::Walk;

	const UCharacterMovementComponent* CMC = Char->GetCharacterMovement();
	if (!CMC) return EMovementMode::Walk;

	if (CMC->IsCrouching())
	{
		return EMovementMode::Crouch;
	}

	const float Speed = CMC->Velocity.Size2D();

	// Threshold-based mode detection.
	if (Speed > CMC->MaxWalkSpeed * 0.9f)
	{
		return EMovementMode::Sprint;
	}
	else if (Speed > CMC->MaxWalkSpeed * 0.5f)
	{
		return EMovementMode::Run;
	}
	else if (Speed > 10.0f)
	{
		return EMovementMode::Walk;
	}

	return EMovementMode::Walk;
}

// =====================================================================
//  Default database: volume and detection multipliers per surface
// =====================================================================

void USHFootstepSystem::InitDefaultDatabase()
{
	auto AddSurface = [this](ESHSurfaceType Type, float VolMult, float DetMult)
	{
		FSHFootstepSoundEntry Entry;
		Entry.Surface = Type;
		Entry.VolumeMultiplier = VolMult;
		Entry.DetectionRangeMultiplier = DetMult;
		FootstepDatabase.Add(Entry);
	};

	// Surface       Volume  AI Detection
	AddSurface(ESHSurfaceType::Concrete,  1.0f,  1.0f);
	AddSurface(ESHSurfaceType::Asphalt,   0.9f,  0.9f);
	AddSurface(ESHSurfaceType::Metal,     1.5f,  1.8f);  // Metal is LOUD
	AddSurface(ESHSurfaceType::Wood,      0.8f,  0.8f);
	AddSurface(ESHSurfaceType::Tile,      1.1f,  1.1f);
	AddSurface(ESHSurfaceType::Dirt,      0.5f,  0.5f);  // Quiet
	AddSurface(ESHSurfaceType::Sand,      0.3f,  0.3f);  // Very quiet
	AddSurface(ESHSurfaceType::Mud,       0.6f,  0.6f);  // Squelch
	AddSurface(ESHSurfaceType::Grass,     0.4f,  0.4f);
	AddSurface(ESHSurfaceType::Gravel,    1.3f,  1.5f);  // Crunchy and loud
	AddSurface(ESHSurfaceType::Water,     0.8f,  1.0f);  // Splash
	AddSurface(ESHSurfaceType::Snow,      0.2f,  0.2f);  // Crunch but quiet
	AddSurface(ESHSurfaceType::Debris,    1.2f,  1.4f);  // Clattery
	AddSurface(ESHSurfaceType::Glass,     1.8f,  2.0f);  // Broken glass = VERY loud
	AddSurface(ESHSurfaceType::Carpet,    0.15f, 0.1f);  // Nearly silent
}
