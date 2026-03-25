// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHSuppressionSystem.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "Camera/CameraShakeBase.h"
#include "Kismet/GameplayStatics.h"

// =====================================================================
//  Subsystem lifecycle
// =====================================================================

void USHSuppressionSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	InitDefaultCaliberValues();
}

void USHSuppressionSystem::Deinitialize()
{
	// Unbind from world tick if still bound.
	if (TickDelegateHandle.IsValid())
	{
		if (const UGameInstance* GI = GetGameInstance())
		{
			if (UWorld* World = GI->GetWorld())
			{
				World->OnWorldBeginPlay.RemoveAll(this);
			}
		}
		TickDelegateHandle.Reset();
	}

	SuppressionMap.Empty();
	Super::Deinitialize();
}

void USHSuppressionSystem::BindToWorldTick()
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (UWorld* World = GI->GetWorld())
		{
			TickDelegateHandle = World->OnTickDispatch().AddUObject(this, &USHSuppressionSystem::Tick);
		}
	}
}

void USHSuppressionSystem::InitDefaultCaliberValues()
{
	if (CaliberSuppressionValues.Num() == 0)
	{
		CaliberSuppressionValues.Add(ESHCaliber::Pistol, 0.06f);
		CaliberSuppressionValues.Add(ESHCaliber::IntermediateRifle, 0.10f);
		CaliberSuppressionValues.Add(ESHCaliber::FullPowerRifle, 0.15f);
		CaliberSuppressionValues.Add(ESHCaliber::HeavyMG, 0.25f);
		CaliberSuppressionValues.Add(ESHCaliber::Shotgun, 0.08f);
		CaliberSuppressionValues.Add(ESHCaliber::Explosive, 0.50f);
		CaliberSuppressionValues.Add(ESHCaliber::Autocannon, 0.35f);
	}
}

// =====================================================================
//  Tick
// =====================================================================

void USHSuppressionSystem::Tick(float DeltaSeconds)
{
	const UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}
	const UWorld* World = GI->GetWorld();
	if (!World)
	{
		return;
	}

	const float WorldTime = World->GetTimeSeconds();

	TArray<TObjectPtr<AActor>> StaleKeys;

	for (auto& Pair : SuppressionMap)
	{
		AActor* Actor = Pair.Key;
		if (!IsValid(Actor))
		{
			StaleKeys.Add(Pair.Key);
			continue;
		}

		FSHSuppressionEntry& Entry = Pair.Value;

		// --- Decay ---
		const float TimeSinceLastImpulse = WorldTime - Entry.LastImpulseTime;
		if (TimeSinceLastImpulse > DecayGracePeriod && Entry.SuppressionValue > 0.f)
		{
			// Decay accelerates at higher suppression so pinned-down soldiers
			// recover faster once fire stops (inverse of the real panic curve).
			const float DecayMultiplier = 1.f + Entry.SuppressionValue * 0.5f;
			Entry.SuppressionValue -= DecayRatePerSecond * DecayMultiplier * DeltaSeconds;
			Entry.SuppressionValue = FMath::Max(Entry.SuppressionValue, 0.f);
		}

		// --- Update derived state ---
		const ESHSuppressionLevel OldLevel = Entry.Level;
		UpdateDerivedState(Actor, Entry);

		if (Entry.Level != OldLevel)
		{
			OnSuppressionLevelChanged.Broadcast(Actor, OldLevel, Entry.Level);
		}

		// Fire pinned event when entering Pinned.
		if (Entry.bIsPinned && OldLevel != ESHSuppressionLevel::Pinned)
		{
			OnActorPinned.Broadcast(Actor, Entry.SuppressionValue);
		}
	}

	// Cleanup stale references.
	for (const auto& Key : StaleKeys)
	{
		SuppressionMap.Remove(Key);
	}
}

// =====================================================================
//  Registration
// =====================================================================

void USHSuppressionSystem::RegisterCharacter(AActor* Character)
{
	if (!IsValid(Character) || SuppressionMap.Contains(Character))
	{
		return;
	}

	SuppressionMap.Add(Character, FSHSuppressionEntry());

	// Lazy-bind the tick delegate on first registration.
	if (!TickDelegateHandle.IsValid())
	{
		BindToWorldTick();
	}
}

void USHSuppressionSystem::UnregisterCharacter(AActor* Character)
{
	SuppressionMap.Remove(Character);
}

// =====================================================================
//  Suppression impulses
// =====================================================================

void USHSuppressionSystem::ReportIncomingRound(const FVector& ImpactLocation, ESHCaliber Caliber, AActor* Instigator)
{
	const float* BaseValue = CaliberSuppressionValues.Find(Caliber);
	if (!BaseValue)
	{
		return;
	}

	const UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}
	const UWorld* World = GI->GetWorld();
	if (!World)
	{
		return;
	}
	const float WorldTime = World->GetTimeSeconds();

	const float ProximityRadiusSq = ProximityRadius * ProximityRadius;

	for (auto& Pair : SuppressionMap)
	{
		AActor* Actor = Pair.Key;
		if (!IsValid(Actor) || Actor == Instigator)
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(Actor->GetActorLocation(), ImpactLocation);
		if (DistSq > ProximityRadiusSq)
		{
			continue;
		}

		// Falloff: closer rounds are more suppressive.
		const float Dist = FMath::Sqrt(DistSq);
		const float Falloff = 1.f - FMath::Clamp(Dist / ProximityRadius, 0.f, 1.f);
		const float Impulse = (*BaseValue) * Falloff;

		ApplySuppressionImpulse(Actor, Impulse, WorldTime);
	}
}

void USHSuppressionSystem::ReportExplosion(const FVector& Origin, float OuterRadius, float InnerRadius)
{
	const UGameInstance* GI = GetGameInstance();
	if (!GI)
	{
		return;
	}
	const UWorld* World = GI->GetWorld();
	if (!World)
	{
		return;
	}
	const float WorldTime = World->GetTimeSeconds();

	const float OuterRadiusSq = OuterRadius * OuterRadius;

	for (auto& Pair : SuppressionMap)
	{
		AActor* Actor = Pair.Key;
		if (!IsValid(Actor))
		{
			continue;
		}

		const float DistSq = FVector::DistSquared(Actor->GetActorLocation(), Origin);
		if (DistSq > OuterRadiusSq)
		{
			continue;
		}

		const float Dist = FMath::Sqrt(DistSq);
		float Impulse;
		if (Dist <= InnerRadius)
		{
			// Inside inner radius: massive spike.
			Impulse = 0.8f;
		}
		else
		{
			// Falloff between inner and outer.
			const float Alpha = (Dist - InnerRadius) / FMath::Max(OuterRadius - InnerRadius, 1.f);
			Impulse = FMath::Lerp(0.8f, 0.15f, FMath::Clamp(Alpha, 0.f, 1.f));
		}

		ApplySuppressionImpulse(Actor, Impulse, WorldTime);
	}
}

void USHSuppressionSystem::ApplySuppressionImpulse(AActor* Character, float Impulse, float WorldTime)
{
	FSHSuppressionEntry* Entry = SuppressionMap.Find(Character);
	if (!Entry)
	{
		return;
	}

	// Diminishing returns: the closer to 1.0, the less each impulse adds.
	const float Headroom = 1.f - Entry->SuppressionValue;
	const float EffectiveImpulse = Impulse * (0.5f + 0.5f * Headroom);

	Entry->SuppressionValue = FMath::Clamp(Entry->SuppressionValue + EffectiveImpulse, 0.f, 1.f);
	Entry->LastImpulseTime = WorldTime;
}

// =====================================================================
//  Derived state computation
// =====================================================================

void USHSuppressionSystem::UpdateDerivedState(AActor* Character, FSHSuppressionEntry& Entry)
{
	const float S = Entry.SuppressionValue;
	Entry.Level = ComputeLevel(S);

	// --- Aim wobble ---
	// Light:  1.0 - 1.3
	// Medium: 1.3 - 2.0
	// Heavy:  2.0 - 3.5
	// Pinned: 3.5 - 5.0
	Entry.AimWobbleMultiplier = 1.f + S * 4.f;

	// --- Movement speed ---
	// No penalty below 0.4; then linear reduction to 0.4 of normal at S=1.
	if (S < 0.4f)
	{
		Entry.MovementSpeedMultiplier = 1.f;
	}
	else
	{
		const float T = (S - 0.4f) / 0.6f; // 0..1 over the 0.4..1.0 range
		Entry.MovementSpeedMultiplier = FMath::Lerp(1.f, 0.4f, T);
	}

	// --- ADS ---
	Entry.bCanADS = (S < 0.6f);

	// --- Pinned flag ---
	Entry.bIsPinned = (S >= 0.8f);

	// --- Camera shake (applied locally for player-controlled characters) ---
	if (S >= 0.4f && SuppressionCameraShake)
	{
		if (const ACharacter* Char = Cast<ACharacter>(Character))
		{
			if (APlayerController* PC = Cast<APlayerController>(Char->GetController()))
			{
				const float ShakeScale = FMath::GetMappedRangeValueClamped(
					FVector2D(0.4f, 1.f), FVector2D(0.2f, MaxCameraShakeScale), S);
				PC->ClientStartCameraShake(SuppressionCameraShake, ShakeScale);
			}
		}
	}
}

ESHSuppressionLevel USHSuppressionSystem::ComputeLevel(float Value) const
{
	if (Value >= 0.8f) return ESHSuppressionLevel::Pinned;
	if (Value >= 0.6f) return ESHSuppressionLevel::Heavy;
	if (Value >= 0.4f) return ESHSuppressionLevel::Medium;
	if (Value >= 0.2f) return ESHSuppressionLevel::Light;
	return ESHSuppressionLevel::None;
}

// =====================================================================
//  Queries
// =====================================================================

float USHSuppressionSystem::GetSuppressionValue(const AActor* Character) const
{
	const FSHSuppressionEntry* Entry = SuppressionMap.Find(const_cast<AActor*>(Character));
	return Entry ? Entry->SuppressionValue : 0.f;
}

ESHSuppressionLevel USHSuppressionSystem::GetSuppressionLevel(const AActor* Character) const
{
	const FSHSuppressionEntry* Entry = SuppressionMap.Find(const_cast<AActor*>(Character));
	return Entry ? Entry->Level : ESHSuppressionLevel::None;
}

FSHSuppressionEntry USHSuppressionSystem::GetSuppressionEntry(const AActor* Character) const
{
	const FSHSuppressionEntry* Entry = SuppressionMap.Find(const_cast<AActor*>(Character));
	return Entry ? *Entry : FSHSuppressionEntry();
}

bool USHSuppressionSystem::IsCharacterPinned(const AActor* Character) const
{
	const FSHSuppressionEntry* Entry = SuppressionMap.Find(const_cast<AActor*>(Character));
	return Entry ? Entry->bIsPinned : false;
}

float USHSuppressionSystem::GetAimWobbleMultiplier(const AActor* Character) const
{
	const FSHSuppressionEntry* Entry = SuppressionMap.Find(const_cast<AActor*>(Character));
	return Entry ? Entry->AimWobbleMultiplier : 1.f;
}

float USHSuppressionSystem::GetMovementSpeedMultiplier(const AActor* Character) const
{
	const FSHSuppressionEntry* Entry = SuppressionMap.Find(const_cast<AActor*>(Character));
	return Entry ? Entry->MovementSpeedMultiplier : 1.f;
}

// =====================================================================
//  Post-process / audio helpers
// =====================================================================

float USHSuppressionSystem::GetDesaturationStrength(const AActor* Character) const
{
	const float S = GetSuppressionValue(Character);
	// Desaturation starts at Light suppression, reaches 0.7 at Pinned.
	if (S < 0.2f) return 0.f;
	return FMath::GetMappedRangeValueClamped(FVector2D(0.2f, 1.f), FVector2D(0.f, 0.7f), S);
}

float USHSuppressionSystem::GetVignetteIntensity(const AActor* Character) const
{
	const float S = GetSuppressionValue(Character);
	if (S < 0.2f) return 0.f;
	return FMath::GetMappedRangeValueClamped(FVector2D(0.2f, 1.f), FVector2D(0.f, 1.f), S);
}

float USHSuppressionSystem::GetTinnitusVolume(const AActor* Character) const
{
	const float S = GetSuppressionValue(Character);
	// Tinnitus starts at Medium.
	if (S < 0.4f) return 0.f;
	return FMath::GetMappedRangeValueClamped(FVector2D(0.4f, 1.f), FVector2D(0.f, 1.f), S);
}
