// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHDestructionReplication.h"
#include "Net/UnrealNetwork.h"

USHDestructionReplication::USHDestructionReplication()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void USHDestructionReplication::GetLifetimeReplicatedProps(
	TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(USHDestructionReplication, ReplicatedLevel);
	DOREPLIFETIME(USHDestructionReplication, ReplicatedHealthFraction);
	DOREPLIFETIME(USHDestructionReplication, bReplicatedHasBreach);
	DOREPLIFETIME(USHDestructionReplication, ReplicatedLeanAngle);
}

// =====================================================================
//  Server interface
// =====================================================================

void USHDestructionReplication::ServerUpdateState(
	ESHDestructionLevel NewLevel,
	float NewHealthFraction,
	bool bNewHasBreach,
	float NewLeanAngle)
{
	// Only server should call this.
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		return;
	}

	ReplicatedLevel = NewLevel;
	ReplicatedHealthFraction = NewHealthFraction;
	bReplicatedHasBreach = bNewHasBreach;
	ReplicatedLeanAngle = NewLeanAngle;

	// Force net update for immediate replication on significant changes.
	if (NewLevel != PreviousLevel)
	{
		GetOwner()->ForceNetUpdate();
		PreviousLevel = NewLevel;
	}
}

// =====================================================================
//  OnRep callbacks — client-side visual updates
// =====================================================================

void USHDestructionReplication::OnRep_DestructionLevel()
{
	// Broadcast for Blueprint/code listeners to swap meshes, apply decals, etc.
	OnDestructionLevelReplicated.Broadcast(ReplicatedLevel);

	UE_LOG(LogTemp, Verbose, TEXT("[DestructionRepl] %s → level %d"),
		GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"),
		static_cast<int32>(ReplicatedLevel));
}

void USHDestructionReplication::OnRep_HealthFraction()
{
	// Health fraction can drive continuous effects like smoke intensity,
	// fire probability, or dust particle density.
}

void USHDestructionReplication::OnRep_HasBreach()
{
	// Breach state changes AI sightlines and player traversal.
	// Client should update nav mesh obstacles or visibility volumes.
	if (bReplicatedHasBreach)
	{
		UE_LOG(LogTemp, Log, TEXT("[DestructionRepl] %s has been BREACHED — new sightline/path available"),
			GetOwner() ? *GetOwner()->GetName() : TEXT("Unknown"));
	}
}

void USHDestructionReplication::OnRep_LeanAngle()
{
	// Apply visual lean to the structure mesh on clients.
	// This is subtle but powerful — players see buildings leaning before collapse.
	if (AActor* Owner = GetOwner())
	{
		if (UStaticMeshComponent* Mesh = Owner->FindComponentByClass<UStaticMeshComponent>())
		{
			FRotator CurrentRot = Mesh->GetRelativeRotation();
			CurrentRot.Roll = ReplicatedLeanAngle;
			Mesh->SetRelativeRotation(CurrentRot);
		}
	}
}
