// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHOperationsGameMode.h"
#include "SHObjectiveSystem.h"
#include "Core/SHGameState.h"
#include "AI/Primordia/SHPrimordiaDecisionEngine.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogSHOperations, Log, All);

// =========================================================================
//  Construction
// =========================================================================

ASHOperationsGameMode::ASHOperationsGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;
}

// =========================================================================
//  AGameModeBase overrides
// =========================================================================

void ASHOperationsGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);

	// Parse optional operation time limit from URL options.
	const float OptionTimeLimit = UGameplayStatics::GetIntOption(Options, TEXT("OpTimeLimit"), 0);
	if (OptionTimeLimit > 0.f)
	{
		OperationTimeLimit = OptionTimeLimit;
	}
}

void ASHOperationsGameMode::StartPlay()
{
	Super::StartPlay();

	// Cache subsystems.
	if (UWorld* World = GetWorld())
	{
		ObjectiveSystem = World->GetSubsystem<USHObjectiveSystem>();
	}
	CachedGameState = GetGameState<ASHGameState>();

	// Initialize all sectors that start without explicit state to Neutral.
	for (FSHSector& Sector : Sectors)
	{
		if (Sector.State == ESHSectorState::Neutral)
		{
			// Leave as-is; designer intent.
		}
	}

	// Enter the Setup phase and generate initial objectives.
	OperationsPhase = ESHOperationsPhase::Setup;
	OperationsPhaseElapsedTime = 0.f;
	TotalOperationTime = 0.f;
	TotalOperationScore = 0.f;
	bOperationEnded = false;
	bVictory = false;

	GeneratePhaseObjectives(OperationsPhase);

	UE_LOG(LogSHOperations, Log, TEXT("StartPlay — Operations mode initialized with %d sectors. Phase: Setup"),
		Sectors.Num());
}

void ASHOperationsGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bOperationEnded)
	{
		return;
	}

	TotalOperationTime += DeltaSeconds;

	TickOperationsPhaseLogic(DeltaSeconds);
	TickSectors(DeltaSeconds);
	TickAIForceAllocation(DeltaSeconds);
	TickScoring(DeltaSeconds);
	EvaluateEndConditions();
}

// =========================================================================
//  Phase management
// =========================================================================

void ASHOperationsGameMode::AdvanceToOperationsPhase(ESHOperationsPhase NewPhase)
{
	if (NewPhase == OperationsPhase || bOperationEnded)
	{
		return;
	}

	const ESHOperationsPhase OldPhase = OperationsPhase;
	OperationsPhase = NewPhase;
	OperationsPhaseElapsedTime = 0.f;

	GeneratePhaseObjectives(NewPhase);

	OnOperationsPhaseChanged.Broadcast(OldPhase, NewPhase);

	// Request immediate AI re-evaluation after a phase transition.
	RequestPrimordiaEvaluation();

	UE_LOG(LogSHOperations, Log, TEXT("Phase advanced: %d -> %d (TotalTime: %.0f s)"),
		static_cast<int32>(OldPhase), static_cast<int32>(NewPhase), TotalOperationTime);
}

// =========================================================================
//  Sector management
// =========================================================================

int32 ASHOperationsGameMode::AddSector(const FSHSector& Sector)
{
	const int32 Index = Sectors.Add(Sector);
	UE_LOG(LogSHOperations, Log, TEXT("Sector added: %s (Index %d)"), *Sector.Name.ToString(), Index);
	return Index;
}

ESHSectorState ASHOperationsGameMode::GetSectorState(FName SectorName) const
{
	if (const FSHSector* Sector = FindSector(SectorName))
	{
		return Sector->State;
	}
	return ESHSectorState::Neutral;
}

bool ASHOperationsGameMode::GetSector(FName SectorName, FSHSector& OutSector) const
{
	if (const FSHSector* Sector = FindSector(SectorName))
	{
		OutSector = *Sector;
		return true;
	}
	return false;
}

void ASHOperationsGameMode::SetSectorState(FName SectorName, ESHSectorState NewState)
{
	if (FSHSector* Sector = FindSectorMutable(SectorName))
	{
		const ESHSectorState OldState = Sector->State;
		if (OldState == NewState)
		{
			return;
		}

		Sector->State = NewState;
		OnSectorStateChanged.Broadcast(SectorName, OldState, NewState);

		// Award capture bonus when transitioning to Friendly from Enemy or Contested.
		if (NewState == ESHSectorState::Friendly &&
			(OldState == ESHSectorState::Enemy || OldState == ESHSectorState::Contested))
		{
			Sector->SectorScore += SectorCaptureBonus;
			TotalOperationScore += SectorCaptureBonus;

			FSHOperationScoreEntry Entry;
			Entry.SectorName = SectorName;
			Entry.Score = SectorCaptureBonus;
			OnOperationScoreChanged.Broadcast(TotalOperationScore, Entry);
		}

		UE_LOG(LogSHOperations, Log, TEXT("Sector %s: %d -> %d"),
			*SectorName.ToString(), static_cast<int32>(OldState), static_cast<int32>(NewState));
	}
}

int32 ASHOperationsGameMode::GetSectorCountByState(ESHSectorState State) const
{
	int32 Count = 0;
	for (const FSHSector& Sector : Sectors)
	{
		if (Sector.State == State)
		{
			++Count;
		}
	}
	return Count;
}

// =========================================================================
//  Score
// =========================================================================

TArray<FSHOperationScoreEntry> ASHOperationsGameMode::GetScoreBreakdown() const
{
	TArray<FSHOperationScoreEntry> Entries;
	Entries.Reserve(Sectors.Num());

	for (const FSHSector& Sector : Sectors)
	{
		FSHOperationScoreEntry Entry;
		Entry.SectorName = Sector.Name;
		Entry.Score = Sector.SectorScore;
		Entries.Add(Entry);
	}
	return Entries;
}

// =========================================================================
//  Sector helpers
// =========================================================================

FSHSector* ASHOperationsGameMode::FindSectorMutable(FName SectorName)
{
	for (FSHSector& Sector : Sectors)
	{
		if (Sector.Name == SectorName)
		{
			return &Sector;
		}
	}
	return nullptr;
}

const FSHSector* ASHOperationsGameMode::FindSector(FName SectorName) const
{
	for (const FSHSector& Sector : Sectors)
	{
		if (Sector.Name == SectorName)
		{
			return &Sector;
		}
	}
	return nullptr;
}

void ASHOperationsGameMode::RecalculateSectorState(FSHSector& Sector)
{
	const float Delta = Sector.DefenderStrength - Sector.AttackerStrength;
	const ESHSectorState PreviousState = Sector.State;

	if (Sector.DefenderStrength < KINDA_SMALL_NUMBER && Sector.AttackerStrength < KINDA_SMALL_NUMBER)
	{
		Sector.State = ESHSectorState::Neutral;
	}
	else if (FMath::Abs(Delta) <= ContestedThreshold)
	{
		Sector.State = ESHSectorState::Contested;
	}
	else if (Sector.DefenderStrength >= OwnershipThreshold &&
			 Sector.DefenderStrength > Sector.AttackerStrength)
	{
		Sector.State = ESHSectorState::Friendly;
	}
	else if (Sector.AttackerStrength >= OwnershipThreshold &&
			 Sector.AttackerStrength > Sector.DefenderStrength)
	{
		Sector.State = ESHSectorState::Enemy;
	}
	else
	{
		Sector.State = ESHSectorState::Contested;
	}

	if (Sector.State != PreviousState)
	{
		OnSectorStateChanged.Broadcast(Sector.Name, PreviousState, Sector.State);

		// Award capture bonus.
		if (Sector.State == ESHSectorState::Friendly &&
			(PreviousState == ESHSectorState::Enemy || PreviousState == ESHSectorState::Contested))
		{
			Sector.SectorScore += SectorCaptureBonus;
			TotalOperationScore += SectorCaptureBonus;

			FSHOperationScoreEntry Entry;
			Entry.SectorName = Sector.Name;
			Entry.Score = SectorCaptureBonus;
			OnOperationScoreChanged.Broadcast(TotalOperationScore, Entry);
		}

		UE_LOG(LogSHOperations, Verbose, TEXT("Sector %s recalculated: %d -> %d (Def: %.2f, Atk: %.2f)"),
			*Sector.Name.ToString(),
			static_cast<int32>(PreviousState), static_cast<int32>(Sector.State),
			Sector.DefenderStrength, Sector.AttackerStrength);
	}
}

// =========================================================================
//  Tick helpers
// =========================================================================

void ASHOperationsGameMode::TickSectors(float DeltaSeconds)
{
	for (FSHSector& Sector : Sectors)
	{
		RecalculateSectorState(Sector);

		// Update linked objective progress for capture-type objectives.
		if (Sector.LinkedObjectiveId.IsValid() && ObjectiveSystem)
		{
			if (const FSHObjective* Obj = ObjectiveSystem->GetObjective(Sector.LinkedObjectiveId))
			{
				if (Obj->Status == ESHObjectiveStatus::Active)
				{
					if (Sector.State == ESHSectorState::Friendly)
					{
						ObjectiveSystem->SetObjectiveProgress(Sector.LinkedObjectiveId, 1.f);
					}
					else if (Sector.State == ESHSectorState::Contested)
					{
						// Contested sectors have partial progress based on defender advantage.
						const float ContestProgress = FMath::Clamp(
							Sector.DefenderStrength / FMath::Max(Sector.AttackerStrength + Sector.DefenderStrength, KINDA_SMALL_NUMBER),
							0.f, 0.99f);
						ObjectiveSystem->SetObjectiveProgress(Sector.LinkedObjectiveId, ContestProgress);
					}
				}
			}
		}
	}
}

void ASHOperationsGameMode::TickOperationsPhaseLogic(float DeltaSeconds)
{
	OperationsPhaseElapsedTime += DeltaSeconds;

	if (ShouldAdvanceOperationsPhase())
	{
		switch (OperationsPhase)
		{
		case ESHOperationsPhase::Setup:
			AdvanceToOperationsPhase(ESHOperationsPhase::Assault);
			break;
		case ESHOperationsPhase::Assault:
			AdvanceToOperationsPhase(ESHOperationsPhase::Consolidate);
			break;
		case ESHOperationsPhase::Consolidate:
			AdvanceToOperationsPhase(ESHOperationsPhase::Extract);
			break;
		case ESHOperationsPhase::Extract:
			// Terminal phase — end conditions are checked in EvaluateEndConditions.
			break;
		}
	}
}

void ASHOperationsGameMode::TickAIForceAllocation(float DeltaSeconds)
{
	AIAllocationAccumulator += DeltaSeconds;
	if (AIAllocationAccumulator < AIAllocationInterval)
	{
		return;
	}
	AIAllocationAccumulator -= AIAllocationInterval;

	// Only allocate during active combat phases.
	if (OperationsPhase == ESHOperationsPhase::Setup)
	{
		return;
	}

	if (!CachedGameState)
	{
		return;
	}

	// Identify the most contested or vulnerable sector and bias AI aggression there.
	float HighestContestDelta = 0.f;
	FName MostContestedSector = NAME_None;

	for (const FSHSector& Sector : Sectors)
	{
		if (Sector.State == ESHSectorState::Contested || Sector.State == ESHSectorState::Friendly)
		{
			// Priority: sectors where the enemy is close to taking over.
			const float Threat = Sector.AttackerStrength - Sector.DefenderStrength;
			if (Threat > HighestContestDelta)
			{
				HighestContestDelta = Threat;
				MostContestedSector = Sector.Name;
			}
		}
	}

	// Determine overall aggression based on how many sectors the enemy holds.
	const int32 EnemySectors = GetSectorCountByState(ESHSectorState::Enemy);
	const int32 ContestedSectors = GetSectorCountByState(ESHSectorState::Contested);
	const int32 TotalSectors = FMath::Max(Sectors.Num(), 1);

	const float EnemyDominance = static_cast<float>(EnemySectors) / TotalSectors;
	const float ContestPressure = static_cast<float>(ContestedSectors) / TotalSectors;

	// Higher enemy dominance = more aggressive; more contested areas = escalation.
	const float Aggression = FMath::Clamp(
		EnemyDominance * 0.4f + ContestPressure * 0.35f + HighestContestDelta * 0.25f,
		0.f, 1.f);

	CachedGameState->SetAIAggressionModifier(Aggression);

	UE_LOG(LogSHOperations, Verbose,
		TEXT("AI allocation — Aggression: %.2f, EnemySectors: %d, Contested: %d, HotSpot: %s"),
		Aggression, EnemySectors, ContestedSectors, *MostContestedSector.ToString());
}

void ASHOperationsGameMode::TickScoring(float DeltaSeconds)
{
	for (FSHSector& Sector : Sectors)
	{
		if (Sector.State == ESHSectorState::Friendly)
		{
			const float ScoreDelta = FriendlySectorScoreRate * DeltaSeconds;
			Sector.SectorScore += ScoreDelta;
			TotalOperationScore += ScoreDelta;
		}
	}
}

void ASHOperationsGameMode::EvaluateEndConditions()
{
	if (bOperationEnded)
	{
		return;
	}

	// Victory: all primary objectives completed.
	if (ObjectiveSystem && PrimaryObjectiveCount > 0)
	{
		const TArray<FSHObjective> PrimaryObjectives = ObjectiveSystem->GetObjectivesByPriority(ESHObjectivePriority::Primary);
		int32 CompletedPrimary = 0;
		for (const FSHObjective& Obj : PrimaryObjectives)
		{
			if (Obj.Status == ESHObjectiveStatus::Completed)
			{
				++CompletedPrimary;
			}
		}

		if (CompletedPrimary >= PrimaryObjectiveCount)
		{
			EndOperation(/*bIsVictory=*/ true);
			return;
		}
	}

	// Defeat: all sectors lost to the enemy.
	if (Sectors.Num() > 0)
	{
		const int32 EnemySectors = GetSectorCountByState(ESHSectorState::Enemy);
		if (EnemySectors >= Sectors.Num())
		{
			EndOperation(/*bIsVictory=*/ false);
			return;
		}
	}

	// Defeat: global operation time limit expired.
	if (OperationTimeLimit > 0.f && TotalOperationTime >= OperationTimeLimit)
	{
		UE_LOG(LogSHOperations, Warning, TEXT("Operation time limit reached (%.0f s). Defeat."), OperationTimeLimit);
		EndOperation(/*bIsVictory=*/ false);
		return;
	}

	// Defeat: Extract phase time limit expired.
	if (OperationsPhase == ESHOperationsPhase::Extract &&
		ExtractTimeLimit > 0.f &&
		OperationsPhaseElapsedTime >= ExtractTimeLimit)
	{
		UE_LOG(LogSHOperations, Warning, TEXT("Extract phase time limit reached. Defeat."));
		EndOperation(/*bIsVictory=*/ false);
		return;
	}
}

// =========================================================================
//  Phase transition helpers
// =========================================================================

bool ASHOperationsGameMode::ShouldAdvanceOperationsPhase() const
{
	switch (OperationsPhase)
	{
	case ESHOperationsPhase::Setup:
		return OperationsPhaseElapsedTime >= SetupPhaseDuration;

	case ESHOperationsPhase::Assault:
	{
		if (Sectors.Num() == 0)
		{
			return false;
		}
		const int32 FriendlySectors = GetSectorCountByState(ESHSectorState::Friendly);
		const float Ratio = static_cast<float>(FriendlySectors) / Sectors.Num();
		return Ratio >= AssaultCompletionSectorRatio;
	}

	case ESHOperationsPhase::Consolidate:
		return OperationsPhaseElapsedTime >= ConsolidateHoldDuration;

	case ESHOperationsPhase::Extract:
		// Terminal — victory is checked via objectives, defeat via timer.
		return false;
	}

	return false;
}

void ASHOperationsGameMode::GeneratePhaseObjectives(ESHOperationsPhase Phase)
{
	if (!ObjectiveSystem)
	{
		return;
	}

	PrimaryObjectiveCount = 0;

	switch (Phase)
	{
	case ESHOperationsPhase::Setup:
	{
		// No combat objectives during setup; a single informational "prepare" objective.
		FSHObjective PrepObj;
		PrepObj.DisplayName = FText::FromString(TEXT("Prepare for Operations"));
		PrepObj.Description = FText::FromString(TEXT("Review the tactical map and finalize squad loadouts."));
		PrepObj.Type = ESHObjectiveType::Recon;
		PrepObj.Priority = ESHObjectivePriority::Secondary;
		PrepObj.TimeLimit = SetupPhaseDuration;
		const FGuid Id = ObjectiveSystem->AddObjective(PrepObj);
		ObjectiveSystem->ActivateObjective(Id);
		break;
	}

	case ESHOperationsPhase::Assault:
	{
		// Generate a primary capture objective for each enemy-held sector.
		for (FSHSector& Sector : Sectors)
		{
			if (Sector.State == ESHSectorState::Enemy || Sector.State == ESHSectorState::Neutral)
			{
				FSHObjective CaptureObj;
				CaptureObj.DisplayName = FText::FromStringView(
					FString::Printf(TEXT("Capture Sector %s"), *Sector.Name.ToString()));
				CaptureObj.Description = FText::FromStringView(
					FString::Printf(TEXT("Secure control of sector %s by eliminating enemy forces."), *Sector.Name.ToString()));
				CaptureObj.Type = ESHObjectiveType::Capture;
				CaptureObj.Priority = ESHObjectivePriority::Primary;
				CaptureObj.WorldLocation = Sector.Location;
				CaptureObj.AreaRadius = Sector.Radius;
				CaptureObj.RequiredPresence = 2;

				const FGuid ObjId = ObjectiveSystem->AddObjective(CaptureObj);
				ObjectiveSystem->ActivateObjective(ObjId);

				Sector.LinkedObjectiveId = ObjId;
				++PrimaryObjectiveCount;
			}
		}

		UE_LOG(LogSHOperations, Log, TEXT("Assault phase: generated %d primary capture objectives."),
			PrimaryObjectiveCount);
		break;
	}

	case ESHOperationsPhase::Consolidate:
	{
		// Generate defend objectives for all friendly sectors.
		for (FSHSector& Sector : Sectors)
		{
			if (Sector.State == ESHSectorState::Friendly)
			{
				FSHObjective DefendObj;
				DefendObj.DisplayName = FText::FromStringView(
					FString::Printf(TEXT("Hold Sector %s"), *Sector.Name.ToString()));
				DefendObj.Description = FText::FromStringView(
					FString::Printf(TEXT("Defend sector %s against enemy counter-attacks."), *Sector.Name.ToString()));
				DefendObj.Type = ESHObjectiveType::Defend;
				DefendObj.Priority = ESHObjectivePriority::Primary;
				DefendObj.WorldLocation = Sector.Location;
				DefendObj.AreaRadius = Sector.Radius;
				DefendObj.DefendDuration = ConsolidateHoldDuration;
				DefendObj.RequiredPresence = 1;

				const FGuid ObjId = ObjectiveSystem->AddObjective(DefendObj);
				ObjectiveSystem->ActivateObjective(ObjId);

				Sector.LinkedObjectiveId = ObjId;
				++PrimaryObjectiveCount;
			}
		}

		UE_LOG(LogSHOperations, Log, TEXT("Consolidate phase: generated %d primary defend objectives."),
			PrimaryObjectiveCount);
		break;
	}

	case ESHOperationsPhase::Extract:
	{
		// Generate a single extraction objective.
		FSHObjective ExtractObj;
		ExtractObj.DisplayName = FText::FromString(TEXT("Extract from AO"));
		ExtractObj.Description = FText::FromString(TEXT("Move all personnel to the designated extraction point."));
		ExtractObj.Type = ESHObjectiveType::Extract;
		ExtractObj.Priority = ESHObjectivePriority::Primary;
		ExtractObj.TimeLimit = ExtractTimeLimit;

		// Place extraction at the average location of friendly sectors or map origin.
		FVector ExfilLocation = FVector::ZeroVector;
		int32 FriendlyCount = 0;
		for (const FSHSector& Sector : Sectors)
		{
			if (Sector.State == ESHSectorState::Friendly)
			{
				ExfilLocation += Sector.Location;
				++FriendlyCount;
			}
		}
		if (FriendlyCount > 0)
		{
			ExfilLocation /= static_cast<float>(FriendlyCount);
		}
		ExtractObj.WorldLocation = ExfilLocation;

		const FGuid ObjId = ObjectiveSystem->AddObjective(ExtractObj);
		ObjectiveSystem->ActivateObjective(ObjId);

		PrimaryObjectiveCount = 1;

		UE_LOG(LogSHOperations, Log, TEXT("Extract phase: extraction objective generated at (%.0f, %.0f, %.0f)."),
			ExfilLocation.X, ExfilLocation.Y, ExfilLocation.Z);
		break;
	}
	}
}

// =========================================================================
//  End operation
// =========================================================================

void ASHOperationsGameMode::EndOperation(bool bIsVictory)
{
	if (bOperationEnded)
	{
		return;
	}

	bOperationEnded = true;
	bVictory = bIsVictory;

	OnOperationEnded.Broadcast(bIsVictory);

	UE_LOG(LogSHOperations, Log, TEXT("Operation ended — %s | Score: %.0f | Time: %.0f s"),
		bIsVictory ? TEXT("VICTORY") : TEXT("DEFEAT"),
		TotalOperationScore, TotalOperationTime);
}
