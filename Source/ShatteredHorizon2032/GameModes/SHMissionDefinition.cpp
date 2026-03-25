// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHMissionDefinition.h"

#include "Misc/Guid.h"

// -----------------------------------------------------------------------
//  Constructor
// -----------------------------------------------------------------------

USHMissionDefinition::USHMissionDefinition()
{
	MissionId = NAME_None;
	EstimatedPlayTime = 30.f;
}

// -----------------------------------------------------------------------
//  UPrimaryDataAsset interface
// -----------------------------------------------------------------------

FPrimaryAssetId USHMissionDefinition::GetPrimaryAssetId() const
{
	// Asset type "MissionDefinition" + the MissionId for a stable, human-readable key.
	return FPrimaryAssetId(TEXT("MissionDefinition"), MissionId.IsNone() ? GetFName() : MissionId);
}

// -----------------------------------------------------------------------
//  Queries
// -----------------------------------------------------------------------

const FSHPhaseDefinition* USHMissionDefinition::GetPhaseDefinition(ESHMissionPhase Phase) const
{
	for (const FSHPhaseDefinition& PhaseDef : Phases)
	{
		if (PhaseDef.Phase == Phase)
		{
			return &PhaseDef;
		}
	}
	return nullptr;
}

// -----------------------------------------------------------------------
//  Validation
// -----------------------------------------------------------------------

bool USHMissionDefinition::Validate(TArray<FText>& OutErrors) const
{
	OutErrors.Reset();

	// --- Mission identity ---

	if (MissionId.IsNone())
	{
		OutErrors.Add(FText::FromString(TEXT("MissionId is not set.")));
	}

	if (Briefing.MissionName.IsEmpty())
	{
		OutErrors.Add(FText::FromString(TEXT("Briefing: MissionName is empty.")));
	}

	if (Briefing.MissionCallsign.IsEmpty())
	{
		OutErrors.Add(FText::FromString(TEXT("Briefing: MissionCallsign is empty.")));
	}

	if (Briefing.BriefingNarration.IsEmpty())
	{
		OutErrors.Add(FText::FromString(TEXT("Briefing: BriefingNarration is empty.")));
	}

	if (Briefing.ObjectiveSummaries.Num() == 0)
	{
		OutErrors.Add(FText::FromString(TEXT("Briefing: No objective summaries defined.")));
	}

	// --- Map ---

	if (!MapAssetPath.IsValid())
	{
		OutErrors.Add(FText::FromString(TEXT("MapAssetPath is not set or invalid.")));
	}

	// --- Phases ---

	if (Phases.Num() == 0)
	{
		OutErrors.Add(FText::FromString(TEXT("No phases defined.")));
	}

	// Collect all objective IDs across every phase for cross-reference checks.
	TSet<FGuid> AllObjectiveIds;

	for (int32 PhaseIdx = 0; PhaseIdx < Phases.Num(); ++PhaseIdx)
	{
		const FSHPhaseDefinition& PhaseDef = Phases[PhaseIdx];
		const FString PhaseLabel = FString::Printf(TEXT("Phase[%d] (%s)"),
			PhaseIdx, *PhaseDef.PhaseName.ToString());

		if (PhaseDef.PhaseName.IsEmpty())
		{
			OutErrors.Add(FText::FromString(FString::Printf(
				TEXT("%s: PhaseName is empty."), *PhaseLabel)));
		}

		// Objectives
		if (PhaseDef.Objectives.Num() == 0)
		{
			OutErrors.Add(FText::FromString(FString::Printf(
				TEXT("%s: No objectives defined."), *PhaseLabel)));
		}

		for (int32 ObjIdx = 0; ObjIdx < PhaseDef.Objectives.Num(); ++ObjIdx)
		{
			const FSHPhaseObjective& Obj = PhaseDef.Objectives[ObjIdx];
			const FString ObjLabel = FString::Printf(TEXT("%s/Objective[%d]"), *PhaseLabel, ObjIdx);

			if (!Obj.ObjectiveId.IsValid())
			{
				OutErrors.Add(FText::FromString(FString::Printf(
					TEXT("%s: ObjectiveId is invalid (not generated)."), *ObjLabel)));
			}
			else if (AllObjectiveIds.Contains(Obj.ObjectiveId))
			{
				OutErrors.Add(FText::FromString(FString::Printf(
					TEXT("%s: Duplicate ObjectiveId %s."), *ObjLabel, *Obj.ObjectiveId.ToString())));
			}
			else
			{
				AllObjectiveIds.Add(Obj.ObjectiveId);
			}

			if (Obj.DisplayName.IsEmpty())
			{
				OutErrors.Add(FText::FromString(FString::Printf(
					TEXT("%s: DisplayName is empty."), *ObjLabel)));
			}
		}

		// Reinforcement waves
		for (int32 WaveIdx = 0; WaveIdx < PhaseDef.ReinforcementWaves.Num(); ++WaveIdx)
		{
			const FSHWaveDefinition& Wave = PhaseDef.ReinforcementWaves[WaveIdx];
			const FString WaveLabel = FString::Printf(TEXT("%s/Wave[%d]"), *PhaseLabel, WaveIdx);

			if (Wave.SpawnZoneTag.IsNone())
			{
				OutErrors.Add(FText::FromString(FString::Printf(
					TEXT("%s: SpawnZoneTag is not set."), *WaveLabel)));
			}

			if (Wave.TotalInfantry <= 0 && Wave.SquadComposition.Num() == 0)
			{
				OutErrors.Add(FText::FromString(FString::Printf(
					TEXT("%s: Wave has no infantry and no squad composition."), *WaveLabel)));
			}

			if (Wave.bIncludesVehicles && Wave.VehicleCount <= 0)
			{
				OutErrors.Add(FText::FromString(FString::Printf(
					TEXT("%s: bIncludesVehicles is true but VehicleCount is 0."), *WaveLabel)));
			}
		}

		// Scripted events — check for orphaned objective references
		for (int32 EvtIdx = 0; EvtIdx < PhaseDef.ScriptedEvents.Num(); ++EvtIdx)
		{
			const FSHScriptedEvent& Evt = PhaseDef.ScriptedEvents[EvtIdx];
			const FString EvtLabel = FString::Printf(TEXT("%s/ScriptedEvent[%d] (%s)"),
				*PhaseLabel, EvtIdx, *Evt.EventId.ToString());

			if (Evt.EventId.IsNone())
			{
				OutErrors.Add(FText::FromString(FString::Printf(
					TEXT("%s: EventId is not set."), *EvtLabel)));
			}

			const bool bNeedsObjective =
				Evt.TriggerType == ESHScriptedEventTriggerType::ObjectiveComplete ||
				Evt.TriggerType == ESHScriptedEventTriggerType::ObjectiveFailed;

			if (bNeedsObjective)
			{
				if (!Evt.TriggerObjectiveId.IsValid())
				{
					OutErrors.Add(FText::FromString(FString::Printf(
						TEXT("%s: Trigger requires an objective but TriggerObjectiveId is invalid."),
						*EvtLabel)));
				}
				else if (!AllObjectiveIds.Contains(Evt.TriggerObjectiveId))
				{
					OutErrors.Add(FText::FromString(FString::Printf(
						TEXT("%s: TriggerObjectiveId %s does not match any defined objective."),
						*EvtLabel, *Evt.TriggerObjectiveId.ToString())));
				}
			}

			if (Evt.TriggerType == ESHScriptedEventTriggerType::PlayerEntersArea &&
				Evt.TriggerVolumeName.IsNone())
			{
				OutErrors.Add(FText::FromString(FString::Printf(
					TEXT("%s: PlayerEntersArea trigger requires a TriggerVolumeName."), *EvtLabel)));
			}
		}

		// Dialogue triggers
		for (int32 DlgIdx = 0; DlgIdx < PhaseDef.DialogueTriggers.Num(); ++DlgIdx)
		{
			const FSHDialogueTrigger& Dlg = PhaseDef.DialogueTriggers[DlgIdx];
			const FString DlgLabel = FString::Printf(TEXT("%s/Dialogue[%d]"), *PhaseLabel, DlgIdx);

			if (Dlg.Speaker.IsEmpty())
			{
				OutErrors.Add(FText::FromString(FString::Printf(
					TEXT("%s: Speaker is empty."), *DlgLabel)));
			}

			if (Dlg.DialogueText.IsEmpty())
			{
				OutErrors.Add(FText::FromString(FString::Printf(
					TEXT("%s: DialogueText is empty."), *DlgLabel)));
			}
		}
	}

	// --- Squad roster ---

	if (SquadRoster.Num() == 0)
	{
		OutErrors.Add(FText::FromString(TEXT("SquadRoster is empty.")));
	}

	for (int32 MemberIdx = 0; MemberIdx < SquadRoster.Num(); ++MemberIdx)
	{
		const FSHSquadMemberDef& Member = SquadRoster[MemberIdx];
		const FString MemberLabel = FString::Printf(TEXT("SquadRoster[%d]"), MemberIdx);

		if (Member.Name.IsEmpty())
		{
			OutErrors.Add(FText::FromString(FString::Printf(
				TEXT("%s: Name is empty."), *MemberLabel)));
		}

		if (Member.Callsign.IsEmpty())
		{
			OutErrors.Add(FText::FromString(FString::Printf(
				TEXT("%s: Callsign is empty."), *MemberLabel)));
		}
	}

	// --- Difficulty scaling ---

	if (DifficultyScaling.Num() == 0)
	{
		OutErrors.Add(FText::FromString(TEXT("DifficultyScaling map is empty (no difficulty tiers defined).")));
	}

	for (const TPair<int32, float>& Pair : DifficultyScaling)
	{
		if (Pair.Value <= 0.f)
		{
			OutErrors.Add(FText::FromString(FString::Printf(
				TEXT("DifficultyScaling[%d]: Multiplier %.2f is not positive."),
				Pair.Key, Pair.Value)));
		}
	}

	return OutErrors.Num() == 0;
}

// -----------------------------------------------------------------------
//  Editor support
// -----------------------------------------------------------------------

#if WITH_EDITOR
void USHMissionDefinition::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Auto-validate on every property edit so designers see issues immediately.
	TArray<FText> ValidationErrors;
	Validate(ValidationErrors);

	for (const FText& Error : ValidationErrors)
	{
		UE_LOG(LogTemp, Warning, TEXT("MissionDefinition '%s' validation: %s"),
			*MissionId.ToString(), *Error.ToString());
	}
}
#endif
