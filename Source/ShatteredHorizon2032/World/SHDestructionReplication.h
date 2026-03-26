// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "World/SHDestructionSystem.h"
#include "SHDestructionReplication.generated.h"

/**
 * USHDestructionReplication
 *
 * Lightweight replicated component that syncs destruction state for co-op.
 * Attach to any destructible actor. The server applies damage authoritatively;
 * this component replicates the destruction level, health fraction, breach
 * state, and lean angle to all clients.
 *
 * Uses OnRep callbacks to trigger client-side VFX and mesh swaps
 * without the server needing to know about visual details.
 */
UCLASS(ClassGroup = (SH), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHDestructionReplication : public UActorComponent
{
	GENERATED_BODY()

public:
	USHDestructionReplication();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ------------------------------------------------------------------
	//  Server interface (authority only)
	// ------------------------------------------------------------------

	/** Called by the destruction system when this actor's state changes. */
	void ServerUpdateState(ESHDestructionLevel NewLevel, float NewHealthFraction,
		bool bNewHasBreach, float NewLeanAngle);

	// ------------------------------------------------------------------
	//  Client queries
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|Destruction|Replication")
	ESHDestructionLevel GetReplicatedLevel() const { return ReplicatedLevel; }

	UFUNCTION(BlueprintPure, Category = "SH|Destruction|Replication")
	float GetReplicatedHealthFraction() const { return ReplicatedHealthFraction; }

	UFUNCTION(BlueprintPure, Category = "SH|Destruction|Replication")
	bool GetReplicatedHasBreach() const { return bReplicatedHasBreach; }

	UFUNCTION(BlueprintPure, Category = "SH|Destruction|Replication")
	float GetReplicatedLeanAngle() const { return ReplicatedLeanAngle; }

	/** Delegate broadcast on clients when destruction level changes. */
	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDestructionLevelReplicated, ESHDestructionLevel, NewLevel);

	UPROPERTY(BlueprintAssignable, Category = "SH|Destruction|Replication")
	FOnDestructionLevelReplicated OnDestructionLevelReplicated;

protected:
	UFUNCTION()
	void OnRep_DestructionLevel();

	UFUNCTION()
	void OnRep_HealthFraction();

	UFUNCTION()
	void OnRep_HasBreach();

	UFUNCTION()
	void OnRep_LeanAngle();

	UPROPERTY(ReplicatedUsing = OnRep_DestructionLevel)
	ESHDestructionLevel ReplicatedLevel = ESHDestructionLevel::Pristine;

	UPROPERTY(ReplicatedUsing = OnRep_HealthFraction)
	float ReplicatedHealthFraction = 1.0f;

	UPROPERTY(ReplicatedUsing = OnRep_HasBreach)
	bool bReplicatedHasBreach = false;

	UPROPERTY(ReplicatedUsing = OnRep_LeanAngle)
	float ReplicatedLeanAngle = 0.0f;

	/** Previous level for detecting transitions. */
	ESHDestructionLevel PreviousLevel = ESHDestructionLevel::Pristine;
};
