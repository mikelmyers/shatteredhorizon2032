// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHObjectiveSystem.h"
#include "GameFramework/Character.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "EngineUtils.h"

void USHObjectiveSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	BindToWorldTick();
}

void USHObjectiveSystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		if (TickDelegateHandle.IsValid())
		{
			World->OnWorldPreActorTick.Remove(TickDelegateHandle);
		}
	}
	Super::Deinitialize();
}

void USHObjectiveSystem::BindToWorldTick()
{
	if (UWorld* World = GetWorld())
	{
		TickDelegateHandle = World->OnWorldPreActorTick.AddLambda(
			[this](UWorld*, ELevelTick, float DeltaSeconds)
			{
				Tick(DeltaSeconds);
			});
	}
}

void USHObjectiveSystem::Tick(float DeltaSeconds)
{
	for (auto& Pair : Objectives)
	{
		FSHObjective& Obj = Pair.Value;
		if (Obj.Status == ESHObjectiveStatus::Active)
		{
			TickObjective(Obj, DeltaSeconds);
		}
	}
}

// =========================================================================
//  Management
// =========================================================================

FGuid USHObjectiveSystem::AddObjective(const FSHObjective& Objective)
{
	FSHObjective NewObj = Objective;
	if (!NewObj.ObjectiveId.IsValid())
	{
		NewObj.ObjectiveId = FGuid::NewGuid();
	}
	NewObj.Status = ESHObjectiveStatus::Inactive;

	const FGuid Id = NewObj.ObjectiveId;
	Objectives.Add(Id, MoveTemp(NewObj));
	return Id;
}

void USHObjectiveSystem::ActivateObjective(const FGuid& ObjectiveId)
{
	if (FSHObjective* Obj = Objectives.Find(ObjectiveId))
	{
		if (Obj->Status == ESHObjectiveStatus::Inactive)
		{
			Obj->Status = ESHObjectiveStatus::Active;
			Obj->ElapsedTime = 0.f;
			OnObjectiveActivated.Broadcast(*Obj);
		}
	}
}

void USHObjectiveSystem::CompleteObjective(const FGuid& ObjectiveId)
{
	if (FSHObjective* Obj = Objectives.Find(ObjectiveId))
	{
		if (Obj->Status == ESHObjectiveStatus::Active)
		{
			Obj->Status = ESHObjectiveStatus::Completed;
			Obj->Progress = 1.f;
			OnObjectiveCompleted.Broadcast(*Obj);
		}
	}
}

void USHObjectiveSystem::FailObjective(const FGuid& ObjectiveId)
{
	if (FSHObjective* Obj = Objectives.Find(ObjectiveId))
	{
		if (Obj->Status == ESHObjectiveStatus::Active)
		{
			Obj->Status = ESHObjectiveStatus::Failed;
			OnObjectiveFailed.Broadcast(*Obj);
		}
	}
}

void USHObjectiveSystem::SetObjectiveProgress(const FGuid& ObjectiveId, float Progress)
{
	if (FSHObjective* Obj = Objectives.Find(ObjectiveId))
	{
		Obj->Progress = FMath::Clamp(Progress, 0.f, 1.f);
		OnObjectiveProgressChanged.Broadcast(ObjectiveId, Obj->Progress);

		if (Obj->Progress >= 1.f)
		{
			CompleteObjective(ObjectiveId);
		}
	}
}

// =========================================================================
//  Queries
// =========================================================================

const FSHObjective* USHObjectiveSystem::GetObjective(const FGuid& ObjectiveId) const
{
	return Objectives.Find(ObjectiveId);
}

TArray<FSHObjective> USHObjectiveSystem::GetActiveObjectives() const
{
	TArray<FSHObjective> Result;
	for (const auto& Pair : Objectives)
	{
		if (Pair.Value.Status == ESHObjectiveStatus::Active)
		{
			Result.Add(Pair.Value);
		}
	}
	return Result;
}

TArray<FSHObjective> USHObjectiveSystem::GetObjectivesByPriority(ESHObjectivePriority Priority) const
{
	TArray<FSHObjective> Result;
	for (const auto& Pair : Objectives)
	{
		if (Pair.Value.Priority == Priority && Pair.Value.Status == ESHObjectiveStatus::Active)
		{
			Result.Add(Pair.Value);
		}
	}
	return Result;
}

int32 USHObjectiveSystem::GetCompletedObjectiveCount() const
{
	int32 Count = 0;
	for (const auto& Pair : Objectives)
	{
		if (Pair.Value.Status == ESHObjectiveStatus::Completed) { ++Count; }
	}
	return Count;
}

int32 USHObjectiveSystem::GetFailedObjectiveCount() const
{
	int32 Count = 0;
	for (const auto& Pair : Objectives)
	{
		if (Pair.Value.Status == ESHObjectiveStatus::Failed) { ++Count; }
	}
	return Count;
}

// =========================================================================
//  Tick helpers
// =========================================================================

void USHObjectiveSystem::TickObjective(FSHObjective& Obj, float DeltaSeconds)
{
	Obj.ElapsedTime += DeltaSeconds;

	// Time limit check.
	if (Obj.TimeLimit > 0.f && Obj.ElapsedTime >= Obj.TimeLimit)
	{
		Obj.Status = ESHObjectiveStatus::Expired;
		OnObjectiveFailed.Broadcast(Obj);
		return;
	}

	switch (Obj.Type)
	{
	case ESHObjectiveType::Capture:
		TickCaptureObjective(Obj, DeltaSeconds);
		break;
	case ESHObjectiveType::Defend:
		TickDefendObjective(Obj, DeltaSeconds);
		break;
	default:
		// Destroy, Extract, Recon, Neutralize are event-driven via SetObjectiveProgress.
		break;
	}
}

void USHObjectiveSystem::TickCaptureObjective(FSHObjective& Obj, float DeltaSeconds)
{
	const int32 FriendlyCount = CountFriendliesInRadius(Obj.WorldLocation, Obj.AreaRadius);
	const int32 EnemyCount = CountEnemiesInRadius(Obj.WorldLocation, Obj.AreaRadius);

	if (FriendlyCount >= Obj.RequiredPresence && EnemyCount == 0)
	{
		// Capturing: progress toward completion.
		const float CaptureRate = 1.f / FMath::Max(Obj.DefendDuration, 10.f);
		Obj.Progress = FMath::Min(1.f, Obj.Progress + CaptureRate * DeltaSeconds);
		OnObjectiveProgressChanged.Broadcast(Obj.ObjectiveId, Obj.Progress);

		if (Obj.Progress >= 1.f)
		{
			Obj.Status = ESHObjectiveStatus::Completed;
			OnObjectiveCompleted.Broadcast(Obj);
		}
	}
	else if (EnemyCount > 0 && FriendlyCount > 0)
	{
		// Contested: progress stalls.
	}
	else if (FriendlyCount == 0)
	{
		// Losing ground: slow regression.
		Obj.Progress = FMath::Max(0.f, Obj.Progress - 0.02f * DeltaSeconds);
		OnObjectiveProgressChanged.Broadcast(Obj.ObjectiveId, Obj.Progress);
	}
}

void USHObjectiveSystem::TickDefendObjective(FSHObjective& Obj, float DeltaSeconds)
{
	const int32 FriendlyCount = CountFriendliesInRadius(Obj.WorldLocation, Obj.AreaRadius);

	if (FriendlyCount >= Obj.RequiredPresence)
	{
		const float DefendRate = 1.f / FMath::Max(Obj.DefendDuration, 10.f);
		Obj.Progress = FMath::Min(1.f, Obj.Progress + DefendRate * DeltaSeconds);
		OnObjectiveProgressChanged.Broadcast(Obj.ObjectiveId, Obj.Progress);

		if (Obj.Progress >= 1.f)
		{
			Obj.Status = ESHObjectiveStatus::Completed;
			OnObjectiveCompleted.Broadcast(Obj);
		}
	}
}

int32 USHObjectiveSystem::CountFriendliesInRadius(const FVector& Center, float Radius) const
{
	int32 Count = 0;
	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			ACharacter* Char = *It;
			if (!Char || Char->IsHidden())
			{
				continue;
			}

			// Consider player-controlled or AI squad members as friendlies.
			if (Char->IsPlayerControlled() || Char->Tags.Contains(FName(TEXT("Friendly"))))
			{
				if (FVector::Dist(Char->GetActorLocation(), Center) <= Radius)
				{
					++Count;
				}
			}
		}
	}
	return Count;
}

int32 USHObjectiveSystem::CountEnemiesInRadius(const FVector& Center, float Radius) const
{
	int32 Count = 0;
	if (const UWorld* World = GetWorld())
	{
		for (TActorIterator<ACharacter> It(World); It; ++It)
		{
			ACharacter* Char = *It;
			if (!Char || Char->IsHidden())
			{
				continue;
			}

			if (Char->Tags.Contains(FName(TEXT("Enemy"))))
			{
				if (FVector::Dist(Char->GetActorLocation(), Center) <= Radius)
				{
					++Count;
				}
			}
		}
	}
	return Count;
}
