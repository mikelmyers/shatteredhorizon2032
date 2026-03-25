// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SHGameMode.h"
#include "SHCoopGameMode.generated.h"

class ASHSquadMember;
class USHSquadManager;

/** Slot state for a co-op player position in the squad. */
USTRUCT(BlueprintType)
struct FSHCoopSlot
{
	GENERATED_BODY()

	/** Index in the squad (0-3). */
	UPROPERTY(BlueprintReadOnly, Category = "Coop")
	int32 SlotIndex = INDEX_NONE;

	/** True if occupied by a human player. */
	UPROPERTY(BlueprintReadOnly, Category = "Coop")
	bool bIsHumanControlled = false;

	/** The player controller occupying this slot (null if AI). */
	UPROPERTY(BlueprintReadOnly, Category = "Coop")
	TWeakObjectPtr<APlayerController> PlayerController = nullptr;

	/** The squad member character in this slot. */
	UPROPERTY(BlueprintReadOnly, Category = "Coop")
	TWeakObjectPtr<ASHSquadMember> SquadMember = nullptr;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCoopPlayerJoined, APlayerController*, Player, int32, SlotIndex);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnCoopPlayerLeft, APlayerController*, Player, int32, SlotIndex);

/**
 * ASHCoopGameMode
 *
 * Extends the base game mode for 4-player cooperative play.
 * Human players replace AI squad members seamlessly — the squad
 * system (formations, cohesion, orders) continues to function
 * with humans filling AI slots.
 *
 * The squad leader (host) retains command authority. Other players
 * receive squad orders as suggested objectives rather than AI commands.
 *
 * Supports drop-in/drop-out: when a human leaves, AI resumes
 * control of that squad member.
 */
UCLASS()
class SHATTEREDHORIZON2032_API ASHCoopGameMode : public ASHGameMode
{
	GENERATED_BODY()

public:
	ASHCoopGameMode();

	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;
	virtual void Tick(float DeltaSeconds) override;

	// ------------------------------------------------------------------
	//  Slot management
	// ------------------------------------------------------------------

	/** Get all co-op slots. */
	UFUNCTION(BlueprintPure, Category = "SH|Coop")
	const TArray<FSHCoopSlot>& GetCoopSlots() const { return CoopSlots; }

	/** Find the next available AI-controlled slot for a joining player. */
	UFUNCTION(BlueprintPure, Category = "SH|Coop")
	int32 FindAvailableSlot() const;

	/** Assign a player to a specific slot, replacing the AI. */
	UFUNCTION(BlueprintCallable, Category = "SH|Coop")
	bool AssignPlayerToSlot(APlayerController* Player, int32 SlotIndex);

	/** Release a slot back to AI control. */
	UFUNCTION(BlueprintCallable, Category = "SH|Coop")
	void ReleaseSlot(int32 SlotIndex);

	/** Get the number of human players currently in the squad. */
	UFUNCTION(BlueprintPure, Category = "SH|Coop")
	int32 GetHumanPlayerCount() const;

	// ------------------------------------------------------------------
	//  Session
	// ------------------------------------------------------------------

	/** Maximum co-op players (including squad leader). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Coop")
	int32 MaxCoopPlayers = 4;

	/** Whether to allow mid-mission join. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Coop")
	bool bAllowMidMissionJoin = true;

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Coop")
	FOnCoopPlayerJoined OnCoopPlayerJoined;

	UPROPERTY(BlueprintAssignable, Category = "SH|Coop")
	FOnCoopPlayerLeft OnCoopPlayerLeft;

protected:
	/** Possess the squad member character with the given player controller. */
	void PossessSquadMember(APlayerController* PC, ASHSquadMember* Member);

	/** Return a squad member to AI control. */
	void ReturnToAIControl(ASHSquadMember* Member);

	/** Initialize co-op slots from the squad manager. */
	void InitializeSlots();

private:
	UPROPERTY()
	TArray<FSHCoopSlot> CoopSlots;

	/** Cached reference to the squad manager on the host player. */
	UPROPERTY()
	TObjectPtr<USHSquadManager> CachedSquadManager = nullptr;
};
