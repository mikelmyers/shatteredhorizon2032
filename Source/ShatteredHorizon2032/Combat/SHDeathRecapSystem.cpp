// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHDeathRecapSystem.h"
#include "Engine/World.h"

USHDeathRecapSystem::USHDeathRecapSystem()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 1.0f; // Low-frequency pruning.
}

void USHDeathRecapSystem::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	PruneOldRecords();
}

// =====================================================================
//  Damage recording
// =====================================================================

void USHDeathRecapSystem::RecordDamage(const FSHDamageRecord& Record)
{
	// Enforce max record limit.
	if (DamageHistory.Num() >= MaxRecords)
	{
		DamageHistory.RemoveAt(0); // Drop oldest.
	}

	DamageHistory.Add(Record);
}

// =====================================================================
//  Death recap generation
// =====================================================================

FSHDeathRecap USHDeathRecapSystem::GenerateDeathRecap()
{
	FSHDeathRecap Recap;

	if (DamageHistory.Num() == 0)
	{
		LastRecap = Recap;
		OnDeathRecapReady.Broadcast(Recap);
		return Recap;
	}

	const UWorld* World = GetWorld();
	const float CurrentTime = World ? World->GetTimeSeconds() : 0.0f;
	const float WindowStart = CurrentTime - RecapWindowSeconds;

	// Filter to events within the recap window.
	for (const FSHDamageRecord& Record : DamageHistory)
	{
		if (Record.Timestamp >= WindowStart)
		{
			Recap.DamageHistory.Add(Record);
			Recap.TotalDamageTaken += Record.DamageAmount;
		}
	}

	if (Recap.DamageHistory.Num() == 0)
	{
		LastRecap = Recap;
		OnDeathRecapReady.Broadcast(Recap);
		return Recap;
	}

	// Find the killing blow (last damage event or the one marked as killing).
	FSHDamageRecord KillingBlow;
	for (int32 i = Recap.DamageHistory.Num() - 1; i >= 0; --i)
	{
		if (Recap.DamageHistory[i].bWasKillingBlow)
		{
			KillingBlow = Recap.DamageHistory[i];
			break;
		}
	}

	// If no explicit killing blow, use the last damage event.
	if (!KillingBlow.bWasKillingBlow && Recap.DamageHistory.Num() > 0)
	{
		KillingBlow = Recap.DamageHistory.Last();
		KillingBlow.bWasKillingBlow = true;
	}

	// Fill recap from killing blow.
	Recap.KillerActor = KillingBlow.DamageSource;
	Recap.KillerWeapon = KillingBlow.WeaponName;
	Recap.KillDistanceCm = KillingBlow.DistanceCm;
	Recap.bKilledByHeadshot = KillingBlow.bWasHeadshot;
	Recap.KillerLocation = KillingBlow.SourceLocation;

	// Direction to killer from victim.
	const AActor* Owner = GetOwner();
	if (Owner && !KillingBlow.SourceLocation.IsNearlyZero())
	{
		Recap.DirectionToKiller = (KillingBlow.SourceLocation - Owner->GetActorLocation()).GetSafeNormal();
	}

	// Count unique damage sources.
	TSet<TWeakObjectPtr<AActor>> UniqueSources;
	for (const FSHDamageRecord& R : Recap.DamageHistory)
	{
		if (R.DamageSource.IsValid())
		{
			UniqueSources.Add(R.DamageSource);
		}
	}
	Recap.UniqueDamageSources = UniqueSources.Num();

	// Time under fire: first damage to last damage.
	if (Recap.DamageHistory.Num() >= 2)
	{
		Recap.TimeUnderFire = Recap.DamageHistory.Last().Timestamp - Recap.DamageHistory[0].Timestamp;
	}

	LastRecap = Recap;
	OnDeathRecapReady.Broadcast(Recap);

	UE_LOG(LogTemp, Log, TEXT("[DeathRecap] Killed by %s (%s) at %.0fm, headshot: %s, %d damage sources, %.1fs under fire"),
		Recap.KillerActor.IsValid() ? *Recap.KillerActor->GetName() : TEXT("Unknown"),
		*Recap.KillerWeapon.ToString(),
		Recap.KillDistanceCm / 100.0f,
		Recap.bKilledByHeadshot ? TEXT("YES") : TEXT("no"),
		Recap.UniqueDamageSources,
		Recap.TimeUnderFire);

	return Recap;
}

void USHDeathRecapSystem::ClearHistory()
{
	DamageHistory.Empty();
}

// =====================================================================
//  Internal — prune old records
// =====================================================================

void USHDeathRecapSystem::PruneOldRecords()
{
	const UWorld* World = GetWorld();
	if (!World) return;

	const float CurrentTime = World->GetTimeSeconds();
	const float CutoffTime = CurrentTime - RecapWindowSeconds;

	// Remove records older than the window.
	DamageHistory.RemoveAll([CutoffTime](const FSHDamageRecord& R)
	{
		return R.Timestamp < CutoffTime;
	});
}
