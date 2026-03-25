// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHCoopGameMode.h"
#include "Squad/SHSquadManager.h"
#include "Squad/SHSquadMember.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Character.h"
#include "AIController.h"

ASHCoopGameMode::ASHCoopGameMode()
{
	// Co-op supports up to 4 players.
	// Slot 0 is the squad leader (host), slots 1-3 are squad members.
}

void ASHCoopGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// Pre-allocate co-op slot array. Actual population happens after squad spawns.
	CoopSlots.SetNum(MaxCoopPlayers - 1); // Exclude squad leader slot (host).
	for (int32 i = 0; i < CoopSlots.Num(); ++i)
	{
		CoopSlots[i].SlotIndex = i;
	}
}

void ASHCoopGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	if (!NewPlayer)
	{
		return;
	}

	// First player is the squad leader — handle normally.
	if (GetHumanPlayerCount() == 0)
	{
		Super::HandleStartingNewPlayer_Implementation(NewPlayer);

		// After the host spawns, find and cache the squad manager.
		if (APawn* Pawn = NewPlayer->GetPawn())
		{
			CachedSquadManager = Pawn->FindComponentByClass<USHSquadManager>();
			InitializeSlots();
		}
		return;
	}

	// Subsequent players join as squad members.
	if (!bAllowMidMissionJoin && GetWorld() && GetWorld()->HasBegunPlay())
	{
		UE_LOG(LogTemp, Warning, TEXT("[SHCoopGameMode] Mid-mission join denied for %s"),
			   *NewPlayer->GetName());
		return;
	}

	const int32 SlotIdx = FindAvailableSlot();
	if (SlotIdx == INDEX_NONE)
	{
		UE_LOG(LogTemp, Warning, TEXT("[SHCoopGameMode] No available slots for %s"),
			   *NewPlayer->GetName());
		return;
	}

	AssignPlayerToSlot(NewPlayer, SlotIdx);
}

void ASHCoopGameMode::Logout(AController* Exiting)
{
	if (APlayerController* PC = Cast<APlayerController>(Exiting))
	{
		// Find which slot this player occupied and release it.
		for (int32 i = 0; i < CoopSlots.Num(); ++i)
		{
			if (CoopSlots[i].bIsHumanControlled && CoopSlots[i].PlayerController == PC)
			{
				ReleaseSlot(i);
				break;
			}
		}
	}

	Super::Logout(Exiting);
}

void ASHCoopGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// Lazy initialization: if squad manager wasn't ready at InitGame.
	if (!CachedSquadManager && GetWorld())
	{
		for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
		{
			if (APlayerController* PC = It->Get())
			{
				if (APawn* Pawn = PC->GetPawn())
				{
					CachedSquadManager = Pawn->FindComponentByClass<USHSquadManager>();
					if (CachedSquadManager)
					{
						InitializeSlots();
						break;
					}
				}
			}
		}
	}
}

// =========================================================================
//  Slot management
// =========================================================================

int32 ASHCoopGameMode::FindAvailableSlot() const
{
	for (int32 i = 0; i < CoopSlots.Num(); ++i)
	{
		if (!CoopSlots[i].bIsHumanControlled && CoopSlots[i].SquadMember.IsValid())
		{
			return i;
		}
	}
	return INDEX_NONE;
}

bool ASHCoopGameMode::AssignPlayerToSlot(APlayerController* Player, int32 SlotIndex)
{
	if (!Player || SlotIndex < 0 || SlotIndex >= CoopSlots.Num())
	{
		return false;
	}

	FSHCoopSlot& Slot = CoopSlots[SlotIndex];
	if (Slot.bIsHumanControlled)
	{
		return false; // Already occupied.
	}

	ASHSquadMember* Member = Slot.SquadMember.Get();
	if (!Member)
	{
		return false;
	}

	// Take control from AI.
	PossessSquadMember(Player, Member);

	Slot.bIsHumanControlled = true;
	Slot.PlayerController = Player;

	OnCoopPlayerJoined.Broadcast(Player, SlotIndex);

	UE_LOG(LogTemp, Log, TEXT("[SHCoopGameMode] Player %s assigned to slot %d"),
		   *Player->GetName(), SlotIndex);

	return true;
}

void ASHCoopGameMode::ReleaseSlot(int32 SlotIndex)
{
	if (SlotIndex < 0 || SlotIndex >= CoopSlots.Num())
	{
		return;
	}

	FSHCoopSlot& Slot = CoopSlots[SlotIndex];
	if (!Slot.bIsHumanControlled)
	{
		return;
	}

	APlayerController* LeavingPC = Slot.PlayerController.Get();
	ASHSquadMember* Member = Slot.SquadMember.Get();

	if (Member)
	{
		// Unpossess and return to AI.
		if (LeavingPC && LeavingPC->GetPawn() == Member)
		{
			LeavingPC->UnPossess();
		}
		ReturnToAIControl(Member);
	}

	Slot.bIsHumanControlled = false;
	Slot.PlayerController = nullptr;

	if (LeavingPC)
	{
		OnCoopPlayerLeft.Broadcast(LeavingPC, SlotIndex);
	}

	UE_LOG(LogTemp, Log, TEXT("[SHCoopGameMode] Slot %d released to AI"), SlotIndex);
}

int32 ASHCoopGameMode::GetHumanPlayerCount() const
{
	int32 Count = 1; // Squad leader is always human.
	for (const FSHCoopSlot& Slot : CoopSlots)
	{
		if (Slot.bIsHumanControlled)
		{
			++Count;
		}
	}
	return Count;
}

// =========================================================================
//  Internal
// =========================================================================

void ASHCoopGameMode::PossessSquadMember(APlayerController* PC, ASHSquadMember* Member)
{
	if (!PC || !Member)
	{
		return;
	}

	// Detach the AI controller first.
	if (AAIController* AIC = Cast<AAIController>(Member->GetController()))
	{
		AIC->UnPossess();
	}

	PC->Possess(Member);
}

void ASHCoopGameMode::ReturnToAIControl(ASHSquadMember* Member)
{
	if (!Member)
	{
		return;
	}

	// Spawn a new AI controller for this squad member.
	FActorSpawnParameters SpawnParams;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	if (UWorld* World = GetWorld())
	{
		if (AAIController* NewAIC = World->SpawnActor<AAIController>(
				Member->AIControllerClass, Member->GetActorLocation(),
				Member->GetActorRotation(), SpawnParams))
		{
			NewAIC->Possess(Member);
		}
	}
}

void ASHCoopGameMode::InitializeSlots()
{
	if (!CachedSquadManager)
	{
		return;
	}

	const TArray<ASHSquadMember*>& Members = CachedSquadManager->GetSquadMembers();
	const int32 SlotCount = FMath::Min(Members.Num(), CoopSlots.Num());

	for (int32 i = 0; i < SlotCount; ++i)
	{
		CoopSlots[i].SquadMember = Members[i];
		CoopSlots[i].SlotIndex = i;
	}

	UE_LOG(LogTemp, Log, TEXT("[SHCoopGameMode] Initialized %d co-op slots"), SlotCount);
}
