// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class USHMissionDefinition;

/**
 * USHMission_TaoyuanBeach
 *
 * Static factory that constructs the complete Mission 1 definition — the
 * Taoyuan Beach defense during the opening hours of the PLA invasion of
 * Taiwan.  Every phase, wave, objective, scripted event, and dialogue line
 * is populated by code so that the mission can be loaded without a data
 * asset on disk.
 *
 * Usage:
 *   USHMissionDefinition* M01 = USHMission_TaoyuanBeach::CreateMissionDefinition(this);
 */
class SHATTEREDHORIZON2032_API USHMission_TaoyuanBeach
{
public:
	USHMission_TaoyuanBeach() = delete;

	/**
	 * Allocate a new USHMissionDefinition, populate every field for
	 * Mission 1 — "Operation Breakwater", and return it.
	 *
	 * @param Outer  The UObject that will own the returned definition.
	 *               Pass GetTransientPackage() when no specific owner is needed.
	 * @return Fully populated mission definition.  Never returns nullptr.
	 */
	static USHMissionDefinition* CreateMissionDefinition(UObject* Outer);

private:
	// ------------------------------------------------------------------
	//  Phase builders — one per mission phase
	// ------------------------------------------------------------------
	static void BuildSquadRoster(USHMissionDefinition* Def);
	static void BuildBriefing(USHMissionDefinition* Def);

	static void BuildPhase_PreInvasion(USHMissionDefinition* Def);
	static void BuildPhase_BeachAssault(USHMissionDefinition* Def);
	static void BuildPhase_UrbanFallback(USHMissionDefinition* Def);
	static void BuildPhase_Counterattack(USHMissionDefinition* Def);

	// ------------------------------------------------------------------
	//  Helpers
	// ------------------------------------------------------------------

	/** Create a standard PLA infantry squad template (6-man). */
	static FSHSquadTemplate MakePLASquad(int32 Riflemen, int32 MG, int32 RPG, int32 Officers);

	/** Shorthand for a delay-triggered dialogue line. */
	static FSHDialogueTrigger MakeDialogue(
		float Delay,
		const FString& Speaker,
		const FString& Text,
		bool bRadio,
		int32 Priority = 0);

	/** Shorthand for a delay-triggered scripted event. */
	static FSHScriptedEvent MakeDelayedEvent(
		float Delay,
		ESHScriptedEventType EventType,
		const FString& EventId,
		const FString& Description,
		FVector Location = FVector::ZeroVector,
		float Radius = 0.f,
		float Duration = 0.f,
		float Intensity = 0.5f);
};
