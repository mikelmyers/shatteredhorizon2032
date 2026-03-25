// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "AI/SHCommandHierarchy.h"
#include "AI/Primordia/SHPrimordiaTacticalAnalyzer.h"
#include "AI/Primordia/SHPrimordiaDecisionEngine.h"
#include "AI/Primordia/SHPrimordiaClient.h"

DEFINE_LOG_CATEGORY_STATIC(LogCommandHierarchy, Log, All);

USHCommandHierarchy::USHCommandHierarchy()
{
}

void USHCommandHierarchy::Initialize(UWorld* InWorld, USHPrimordiaTacticalAnalyzer* Analyzer, USHPrimordiaDecisionEngine* Engine)
{
	WorldRef = InWorld;
	TacticalAnalyzer = Analyzer;
	DecisionEngine = Engine;

	UE_LOG(LogCommandHierarchy, Log, TEXT("Command Hierarchy initialized (difficulty: %d)"), static_cast<int32>(DifficultyTier));
}

void USHCommandHierarchy::Tick(float DeltaSeconds)
{
	// Strategic layer runs at a slow cadence (campaign-level thinking).
	StrategicEvalAccumulator += DeltaSeconds;
	if (StrategicEvalAccumulator >= StrategicEvalInterval)
	{
		StrategicEvalAccumulator = 0.f;

		// Only evaluate strategic layer at Hardened+ difficulty.
		if (DifficultyTier >= ESHDifficultyTier::Hardened)
		{
			EvaluateStrategicObjectives();
		}
	}

	// Operational layer runs more frequently (sector-level decisions).
	OperationalEvalAccumulator += DeltaSeconds;
	if (OperationalEvalAccumulator >= OperationalEvalInterval)
	{
		OperationalEvalAccumulator = 0.f;

		// Operational coordination requires Regular+ difficulty.
		if (DifficultyTier >= ESHDifficultyTier::Regular)
		{
			GenerateOperationalOrders();
		}
	}
}

// ======================================================================
//  Strategic Layer
// ======================================================================

void USHCommandHierarchy::EvaluateStrategicObjectives()
{
	if (!TacticalAnalyzer.IsValid())
	{
		return;
	}

	// Get all sector analyses from the tactical analyzer.
	TArray<FSHSectorAnalysis> Sectors = TacticalAnalyzer->GetAllSectorAnalyses();
	FSHCasualtySummary Casualties = TacticalAnalyzer->GetCasualtySummary();
	TArray<FSHDetectedTactic> Tactics = TacticalAnalyzer->GetDetectedTactics();

	StrategicObjectives.Empty();

	for (const FSHSectorAnalysis& Sector : Sectors)
	{
		FSHStrategicObjective Obj = EvaluateSector(Sector.SectorTag);
		if (!Obj.ObjectiveType.IsEmpty())
		{
			StrategicObjectives.Add(Obj);
			OnStrategicObjectiveIssued.Broadcast(Obj);
		}
	}

	// Sort by priority.
	StrategicObjectives.Sort([](const FSHStrategicObjective& A, const FSHStrategicObjective& B)
	{
		return A.Priority > B.Priority;
	});

	UE_LOG(LogCommandHierarchy, Log, TEXT("Strategic evaluation complete: %d objectives issued."), StrategicObjectives.Num());
}

FSHStrategicObjective USHCommandHierarchy::EvaluateSector(FName SectorTag) const
{
	FSHStrategicObjective Obj;
	Obj.ObjectiveId = FGuid::NewGuid();
	Obj.TargetSector = SectorTag;

	const FSHSectorAnalysis Analysis = TacticalAnalyzer->GetSectorAnalysis(SectorTag);

	// Strategic decision logic:
	// - Weak sectors (low defensive strength) → seize
	// - Strong sectors with multiple approach routes → feint + main assault
	// - Sectors where previous attacks failed → change approach or bypass
	// - Sectors with high attack success rate → press the advantage

	if (Analysis.DefensiveStrength < 0.3f)
	{
		// Weak point — priority target.
		Obj.ObjectiveType = TEXT("seize");
		Obj.Priority = 8;
		Obj.ForceRatio = 0.4f;
	}
	else if (Analysis.DefensiveStrength < 0.5f && Analysis.ApproachRouteCount >= 2)
	{
		// Moderate defense with flanking options — feint and assault.
		Obj.ObjectiveType = TEXT("feint");
		Obj.Priority = 6;
		Obj.ForceRatio = 0.3f;
	}
	else if (Analysis.AttackSuccessRate > 0.6f && Analysis.EngagementCount >= 2)
	{
		// We've been winning here — press advantage.
		Obj.ObjectiveType = TEXT("seize");
		Obj.Priority = 7;
		Obj.ForceRatio = 0.35f;
	}
	else if (Analysis.DefensiveStrength > 0.7f && Analysis.AttackSuccessRate < 0.3f)
	{
		// Heavily defended and we keep losing — interdict (cut supply/reinforce).
		Obj.ObjectiveType = TEXT("interdict");
		Obj.Priority = 4;
		Obj.ForceRatio = 0.15f;
	}
	else
	{
		// Default — maintain pressure with probing attacks.
		Obj.ObjectiveType = TEXT("seize");
		Obj.Priority = 3;
		Obj.ForceRatio = 0.2f;
	}

	// At Primordia Unleashed, increase force commitment.
	if (DifficultyTier == ESHDifficultyTier::PrimordiaUnleashed)
	{
		Obj.ForceRatio = FMath::Min(Obj.ForceRatio * 1.3f, 0.8f);
	}

	return Obj;
}

// ======================================================================
//  Operational Layer
// ======================================================================

void USHCommandHierarchy::GenerateOperationalOrders()
{
	if (!DecisionEngine.IsValid())
	{
		return;
	}

	OperationalOrders.Empty();

	for (const FSHStrategicObjective& Obj : StrategicObjectives)
	{
		DecomposeObjectiveToOrders(Obj);
	}

	UE_LOG(LogCommandHierarchy, Log, TEXT("Operational planning complete: %d orders generated."), OperationalOrders.Num());
}

void USHCommandHierarchy::DecomposeObjectiveToOrders(const FSHStrategicObjective& Objective)
{
	const float GameTime = WorldRef.IsValid() ? WorldRef->GetTimeSeconds() : 0.f;

	if (Objective.ObjectiveType == TEXT("seize"))
	{
		// Multi-phase: suppress → assault → exploit.
		// Phase 1: Suppressive fire from support-by-fire position.
		{
			FSHOperationalOrder Suppress;
			Suppress.OrderId = FGuid::NewGuid();
			Suppress.SourceObjectiveId = Objective.ObjectiveId;
			Suppress.OrderType = TEXT("suppress_and_fix");
			Suppress.SectorTag = Objective.TargetSector;
			Suppress.ScheduledTime = GameTime;
			Suppress.bAutoEscalate = false;
			OperationalOrders.Add(Suppress);
			OnOperationalOrderIssued.Broadcast(Suppress);
		}

		// Phase 2: Assault element moves under covering fire (delayed).
		{
			FSHOperationalOrder Assault;
			Assault.OrderId = FGuid::NewGuid();
			Assault.SourceObjectiveId = Objective.ObjectiveId;
			Assault.OrderType = TEXT("assault");
			Assault.SectorTag = Objective.TargetSector;
			Assault.ScheduledTime = GameTime + 15.f; // 15 seconds after suppression begins
			Assault.bAutoEscalate = true;
			OperationalOrders.Add(Assault);
			OnOperationalOrderIssued.Broadcast(Assault);
		}

		// At Veteran+, add exploitation phase.
		if (DifficultyTier >= ESHDifficultyTier::Veteran)
		{
			FSHOperationalOrder Exploit;
			Exploit.OrderId = FGuid::NewGuid();
			Exploit.SourceObjectiveId = Objective.ObjectiveId;
			Exploit.OrderType = TEXT("exploit_breakthrough");
			Exploit.SectorTag = Objective.TargetSector;
			Exploit.ScheduledTime = GameTime + 45.f; // Follow-on force
			Exploit.bAutoEscalate = false;
			OperationalOrders.Add(Exploit);
			OnOperationalOrderIssued.Broadcast(Exploit);
		}
	}
	else if (Objective.ObjectiveType == TEXT("feint"))
	{
		// Diversionary attack + main effort on different axis.
		{
			FSHOperationalOrder Feint;
			Feint.OrderId = FGuid::NewGuid();
			Feint.SourceObjectiveId = Objective.ObjectiveId;
			Feint.OrderType = TEXT("feint_attack");
			Feint.SectorTag = Objective.TargetSector;
			Feint.ScheduledTime = GameTime;
			Feint.bAutoEscalate = false;
			OperationalOrders.Add(Feint);
			OnOperationalOrderIssued.Broadcast(Feint);
		}

		{
			FSHOperationalOrder MainEffort;
			MainEffort.OrderId = FGuid::NewGuid();
			MainEffort.SourceObjectiveId = Objective.ObjectiveId;
			MainEffort.OrderType = TEXT("main_assault");
			MainEffort.SectorTag = Objective.TargetSector;
			MainEffort.ScheduledTime = GameTime + 20.f;
			MainEffort.bAutoEscalate = true;
			OperationalOrders.Add(MainEffort);
			OnOperationalOrderIssued.Broadcast(MainEffort);
		}
	}
	else if (Objective.ObjectiveType == TEXT("interdict"))
	{
		// Recon-in-force to probe defenses and disrupt.
		FSHOperationalOrder Probe;
		Probe.OrderId = FGuid::NewGuid();
		Probe.SourceObjectiveId = Objective.ObjectiveId;
		Probe.OrderType = TEXT("recon_in_force");
		Probe.SectorTag = Objective.TargetSector;
		Probe.ScheduledTime = GameTime;
		Probe.bAutoEscalate = false;
		OperationalOrders.Add(Probe);
		OnOperationalOrderIssued.Broadcast(Probe);
	}
	else if (Objective.ObjectiveType == TEXT("withdraw"))
	{
		// Phased withdrawal — covering force then main body.
		FSHOperationalOrder Withdraw;
		Withdraw.OrderId = FGuid::NewGuid();
		Withdraw.SourceObjectiveId = Objective.ObjectiveId;
		Withdraw.OrderType = TEXT("phased_withdrawal");
		Withdraw.SectorTag = Objective.TargetSector;
		Withdraw.ScheduledTime = GameTime;
		Withdraw.bAutoEscalate = false;
		OperationalOrders.Add(Withdraw);
		OnOperationalOrderIssued.Broadcast(Withdraw);
	}

	// Feed operational orders into the Decision Engine as directive batches.
	// This bridges the operational and tactical layers.
	if (DecisionEngine.IsValid())
	{
		for (const FSHOperationalOrder& OpOrder : OperationalOrders)
		{
			FSHPrimordiaTacticalDirective Directive;
			Directive.DirectiveId = OpOrder.OrderId.ToString();
			Directive.OrderType = OpOrder.OrderType;
			Directive.SectorTag = OpOrder.SectorTag;
			Directive.ExecutionTime = OpOrder.ScheduledTime;
			Directive.ForceAllocationRatio = 0.3f;
			Directive.AssignedSquadIds = OpOrder.AssignedSquadIds;

			FSHPrimordiaDirectiveBatch Batch;
			Batch.BatchId = FGuid::NewGuid().ToString();
			Batch.ServerTimestamp = GameTime;
			Batch.Directives.Add(Directive);

			DecisionEngine->IngestDirectiveBatch(Batch);
		}
	}
}
