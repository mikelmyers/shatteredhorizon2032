// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHPlayerState.h"
#include "Net/UnrealNetwork.h"
#include "Progression/SHProgressionSystem.h"

ASHPlayerState::ASHPlayerState()
{
	// Initialize default ammo loadout for a US Marine squad leader.
	// M4A1/M27 IAR primary.
	FSHAmmoState PrimaryAmmo;
	PrimaryAmmo.AmmoType = FName(TEXT("5.56x45mm"));
	PrimaryAmmo.MagazineCount = 30;
	PrimaryAmmo.ReserveCount = 180;
	PrimaryAmmo.MagazineCapacity = 30;
	AmmoStates.Add(PrimaryAmmo);

	// M17 sidearm.
	FSHAmmoState SidearmAmmo;
	SidearmAmmo.AmmoType = FName(TEXT("9x19mm"));
	SidearmAmmo.MagazineCount = 17;
	SidearmAmmo.ReserveCount = 34;
	SidearmAmmo.MagazineCapacity = 17;
	AmmoStates.Add(SidearmAmmo);

	// Fragmentation grenades.
	FSHAmmoState FragAmmo;
	FragAmmo.AmmoType = FName(TEXT("M67_Frag"));
	FragAmmo.MagazineCount = 1;
	FragAmmo.ReserveCount = 2;
	FragAmmo.MagazineCapacity = 1;
	AmmoStates.Add(FragAmmo);

	// Smoke grenades.
	FSHAmmoState SmokeAmmo;
	SmokeAmmo.AmmoType = FName(TEXT("M18_Smoke"));
	SmokeAmmo.MagazineCount = 1;
	SmokeAmmo.ReserveCount = 1;
	SmokeAmmo.MagazineCapacity = 1;
	AmmoStates.Add(SmokeAmmo);
}

void ASHPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(ASHPlayerState, EngagementRecord);
	DOREPLIFETIME(ASHPlayerState, MoraleState);
	DOREPLIFETIME(ASHPlayerState, MoraleValue);
	DOREPLIFETIME(ASHPlayerState, AmmoStates);
	DOREPLIFETIME(ASHPlayerState, SquadLeaderAuthorityLevel);
}

// =======================================================================
//  Engagement tracking
// =======================================================================

void ASHPlayerState::RecordKill(float Distance)
{
	EngagementRecord.Kills++;
	EngagementRecord.LongestKillDistance = FMath::Max(EngagementRecord.LongestKillDistance, Distance);

	// Kills improve morale.
	AdjustMorale(3.f);

	// Award XP via progression system.
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USHProgressionSystem* Prog = GI->GetSubsystem<USHProgressionSystem>())
		{
			int32 XP = 100; // Base kill XP.
			// Bonus for long-range kills (>300m = 30000cm).
			if (Distance > 30000.f) { XP += 50; }
			if (Distance > 60000.f) { XP += 100; }
			Prog->AwardXP(XP, FName(TEXT("Kill")));
		}
	}
}

void ASHPlayerState::RecordAssist()
{
	EngagementRecord.Assists++;
	AdjustMorale(1.f);
}

void ASHPlayerState::RecordVehicleKill()
{
	EngagementRecord.VehicleKills++;
	AdjustMorale(5.f);
}

void ASHPlayerState::RecordShotFired()
{
	EngagementRecord.ShotsFired++;
}

void ASHPlayerState::RecordShotHit()
{
	EngagementRecord.ShotsHit++;
}

void ASHPlayerState::RecordDamageDealt(float Amount)
{
	EngagementRecord.DamageDealt += Amount;
}

void ASHPlayerState::RecordDamageTaken(float Amount)
{
	EngagementRecord.DamageTaken += Amount;

	// Taking damage degrades morale.
	AdjustMorale(-Amount * 0.05f);
}

void ASHPlayerState::RecordRevive()
{
	EngagementRecord.Revives++;
	AdjustMorale(4.f);

	// Award XP for teamwork.
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USHProgressionSystem* Prog = GI->GetSubsystem<USHProgressionSystem>())
		{
			Prog->AwardXP(150, FName(TEXT("Teamwork")));
		}
	}
}

void ASHPlayerState::RecordSquadOrderIssued()
{
	EngagementRecord.SquadOrdersIssued++;
}

void ASHPlayerState::RecordSquadOrderCompleted()
{
	EngagementRecord.SquadOrdersCompleted++;
	AdjustMorale(2.f);
}

void ASHPlayerState::RecordObjectiveCompleted()
{
	EngagementRecord.ObjectivesCompleted++;
	AdjustMorale(10.f);

	// Award XP for objective completion.
	if (UGameInstance* GI = GetGameInstance())
	{
		if (USHProgressionSystem* Prog = GI->GetSubsystem<USHProgressionSystem>())
		{
			Prog->AwardXP(500, FName(TEXT("Objective")));
		}
	}
}

// =======================================================================
//  Morale
// =======================================================================

void ASHPlayerState::AdjustMorale(float Delta)
{
	const ESHMoraleState OldState = MoraleState;

	MoraleValue = FMath::Clamp(MoraleValue + Delta, 0.f, 100.f);
	UpdateMoraleState();

	if (MoraleState != OldState)
	{
		OnMoraleChanged.Broadcast(MoraleState);
		UE_LOG(LogTemp, Log, TEXT("[SHPlayerState] Morale changed: %d -> %d (value: %.1f)"),
			static_cast<int32>(OldState), static_cast<int32>(MoraleState), MoraleValue);
	}
}

void ASHPlayerState::UpdateMoraleState()
{
	if (MoraleValue >= 90.f)
	{
		MoraleState = ESHMoraleState::Resolute;
	}
	else if (MoraleValue >= 60.f)
	{
		MoraleState = ESHMoraleState::Steady;
	}
	else if (MoraleValue >= 35.f)
	{
		MoraleState = ESHMoraleState::Shaken;
	}
	else if (MoraleValue >= 10.f)
	{
		MoraleState = ESHMoraleState::Breaking;
	}
	else
	{
		MoraleState = ESHMoraleState::Routed;
	}
}

// =======================================================================
//  Ammo / equipment
// =======================================================================

void ASHPlayerState::SetAmmoState(FName AmmoType, int32 MagazineCount, int32 ReserveCount)
{
	for (FSHAmmoState& State : AmmoStates)
	{
		if (State.AmmoType == AmmoType)
		{
			State.MagazineCount = FMath::Max(0, MagazineCount);
			State.ReserveCount = FMath::Max(0, ReserveCount);
			OnAmmoChanged.Broadcast(AmmoType, State);
			return;
		}
	}

	// Ammo type not found — add new entry.
	FSHAmmoState NewState;
	NewState.AmmoType = AmmoType;
	NewState.MagazineCount = FMath::Max(0, MagazineCount);
	NewState.ReserveCount = FMath::Max(0, ReserveCount);
	AmmoStates.Add(NewState);
	OnAmmoChanged.Broadcast(AmmoType, NewState);
}

bool ASHPlayerState::ConsumeAmmo(FName AmmoType, int32 Count)
{
	for (FSHAmmoState& State : AmmoStates)
	{
		if (State.AmmoType == AmmoType)
		{
			if (State.MagazineCount >= Count)
			{
				State.MagazineCount -= Count;
				OnAmmoChanged.Broadcast(AmmoType, State);
				return true;
			}
			return false; // not enough in magazine
		}
	}
	return false; // ammo type not found
}

void ASHPlayerState::AddReserveAmmo(FName AmmoType, int32 Count)
{
	for (FSHAmmoState& State : AmmoStates)
	{
		if (State.AmmoType == AmmoType)
		{
			State.ReserveCount += Count;
			OnAmmoChanged.Broadcast(AmmoType, State);
			return;
		}
	}

	// New ammo type — add it.
	FSHAmmoState NewState;
	NewState.AmmoType = AmmoType;
	NewState.ReserveCount = Count;
	AmmoStates.Add(NewState);
	OnAmmoChanged.Broadcast(AmmoType, NewState);
}

bool ASHPlayerState::ReloadWeapon(FName AmmoType)
{
	for (FSHAmmoState& State : AmmoStates)
	{
		if (State.AmmoType == AmmoType)
		{
			const int32 RoundsNeeded = State.MagazineCapacity - State.MagazineCount;
			if (RoundsNeeded <= 0 || State.ReserveCount <= 0)
			{
				return false;
			}

			const int32 RoundsToLoad = FMath::Min(RoundsNeeded, State.ReserveCount);
			State.MagazineCount += RoundsToLoad;
			State.ReserveCount -= RoundsToLoad;

			OnAmmoChanged.Broadcast(AmmoType, State);

			UE_LOG(LogTemp, Log, TEXT("[SHPlayerState] Reloaded %s: %d/%d (reserve: %d)"),
				*AmmoType.ToString(), State.MagazineCount, State.MagazineCapacity, State.ReserveCount);

			return true;
		}
	}
	return false;
}

// =======================================================================
//  Squad leader authority
// =======================================================================

void ASHPlayerState::SetSquadLeaderAuthorityLevel(int32 Level)
{
	SquadLeaderAuthorityLevel = FMath::Max(0, Level);
}
