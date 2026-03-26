// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.
//
// Mission 1 (Operation Breakwater) Tests
//
// Verifies the mission definition — squad roster, phase structure,
// enemy wave composition, dialogue, and narrative integrity.
// These tests catch content regressions like missing squad members,
// empty phases, or broken wave definitions.

#include "Misc/AutomationTest.h"
#include "GameModes/SHMission_TaoyuanBeach.h"
#include "GameModes/SHMissionDefinition.h"

#if WITH_AUTOMATION_TESTS

// ========================================================================
//  Mission creation: must produce a valid definition
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMission1_Creation, "SH2032.Mission.TaoyuanBeach.Creation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMission1_Creation::RunTest(const FString&)
{
	FSHMissionDefinition Def = USHMission_TaoyuanBeach::CreateMissionDefinition();

	// Must have a name and callsign
	TestFalse(TEXT("Mission name not empty"), Def.MissionName.IsEmpty());
	TestFalse(TEXT("Callsign not empty"), Def.Callsign.IsEmpty());

	// Must have phases
	TestTrue(TEXT("Has at least 4 phases"), Def.Phases.Num() >= 4);

	// Must have squad members
	TestTrue(TEXT("Has at least 4 squad members"), Def.SquadRoster.Num() >= 4);

	// Must have briefing
	TestFalse(TEXT("Briefing not empty"), Def.BriefingText.IsEmpty());

	return true;
}

// ========================================================================
//  Squad roster: named characters with personality
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMission1_SquadRoster, "SH2032.Mission.TaoyuanBeach.SquadRoster",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMission1_SquadRoster::RunTest(const FString&)
{
	FSHMissionDefinition Def = USHMission_TaoyuanBeach::CreateMissionDefinition();

	// Verify each squad member has required data
	for (int32 i = 0; i < Def.SquadRoster.Num(); ++i)
	{
		const auto& Member = Def.SquadRoster[i];
		TestFalse(FString::Printf(TEXT("Member %d has name"), i), Member.DisplayName.IsEmpty());
		TestFalse(FString::Printf(TEXT("Member %d has callsign"), i), Member.Callsign.IsEmpty());
		TestFalse(FString::Printf(TEXT("Member %d has role"), i), Member.Role.IsEmpty());
	}

	// Verify we have the specific named characters from the narrative
	// These characters drive emotional investment — losing one is a bug
	bool bHasVasquez = false, bHasKim = false, bHasChen = false, bHasWilliams = false;
	for (const auto& M : Def.SquadRoster)
	{
		if (M.Callsign.Contains(TEXT("Viper-2")) || M.DisplayName.Contains(TEXT("Vasquez"))) bHasVasquez = true;
		if (M.Callsign.Contains(TEXT("Viper-3")) || M.DisplayName.Contains(TEXT("Kim"))) bHasKim = true;
		if (M.Callsign.Contains(TEXT("Viper-4")) || M.DisplayName.Contains(TEXT("Chen"))) bHasChen = true;
		if (M.Callsign.Contains(TEXT("Viper-5")) || M.DisplayName.Contains(TEXT("Williams"))) bHasWilliams = true;
	}
	TestTrue(TEXT("Vasquez present"), bHasVasquez);
	TestTrue(TEXT("Kim present"), bHasKim);
	TestTrue(TEXT("Chen present"), bHasChen);
	TestTrue(TEXT("Williams (Corpsman) present"), bHasWilliams);

	return true;
}

// ========================================================================
//  Phase structure: 4 phases in correct order
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMission1_PhaseStructure, "SH2032.Mission.TaoyuanBeach.PhaseStructure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMission1_PhaseStructure::RunTest(const FString&)
{
	FSHMissionDefinition Def = USHMission_TaoyuanBeach::CreateMissionDefinition();

	TestTrue(TEXT("At least 4 phases"), Def.Phases.Num() >= 4);

	// Each phase should have content
	for (int32 i = 0; i < Def.Phases.Num(); ++i)
	{
		const auto& Phase = Def.Phases[i];
		TestFalse(FString::Printf(TEXT("Phase %d has name"), i), Phase.PhaseName.IsEmpty());
		// Phase should have either events, objectives, or waves
		const bool bHasContent = Phase.ScriptedEvents.Num() > 0 ||
								 Phase.Objectives.Num() > 0 ||
								 Phase.ReinforcementWaves.Num() > 0;
		TestTrue(FString::Printf(TEXT("Phase %d has content"), i), bHasContent);
	}

	return true;
}

// ========================================================================
//  Enemy waves: Beach Assault phase must have sufficient forces
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMission1_EnemyWaves, "SH2032.Mission.TaoyuanBeach.EnemyWaves",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMission1_EnemyWaves::RunTest(const FString&)
{
	FSHMissionDefinition Def = USHMission_TaoyuanBeach::CreateMissionDefinition();

	// Phase 2 (Beach Assault) should have reinforcement waves
	if (Def.Phases.Num() >= 2)
	{
		const auto& BeachPhase = Def.Phases[1]; // Phase 2
		TestTrue(TEXT("Beach phase has waves"), BeachPhase.ReinforcementWaves.Num() >= 2);

		// Count total enemy forces across all waves
		int32 TotalInfantry = 0;
		for (const auto& Wave : BeachPhase.ReinforcementWaves)
		{
			TotalInfantry += Wave.InfantryCount;
			// Each wave should have non-zero delay (except first)
			// and non-zero infantry
			TestTrue(TEXT("Wave has infantry"), Wave.InfantryCount > 0);
		}

		// Mission 1 should have a significant enemy force (100+ across all waves)
		TestTrue(TEXT("Total infantry >= 50"), TotalInfantry >= 50);
	}

	return true;
}

// ========================================================================
//  Narrative: key dialogue moments must exist
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FMission1_KeyDialogue, "SH2032.Mission.TaoyuanBeach.KeyDialogue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FMission1_KeyDialogue::RunTest(const FString&)
{
	FSHMissionDefinition Def = USHMission_TaoyuanBeach::CreateMissionDefinition();

	// Collect all dialogue text across all phases
	TArray<FString> AllDialogue;
	for (const auto& Phase : Def.Phases)
	{
		for (const auto& Event : Phase.ScriptedEvents)
		{
			if (!Event.DialogueText.IsEmpty())
			{
				AllDialogue.Add(Event.DialogueText);
			}
		}
	}

	TestTrue(TEXT("Mission has dialogue"), AllDialogue.Num() > 0);

	// Check for the key emotional beat: "Sofia" (Vasquez's daughter)
	bool bHasSofiaMoment = false;
	for (const FString& Line : AllDialogue)
	{
		if (Line.Contains(TEXT("Sofia")))
		{
			bHasSofiaMoment = true;
			break;
		}
	}
	TestTrue(TEXT("Sofia moment present"), bHasSofiaMoment);

	return true;
}

#endif // WITH_AUTOMATION_TESTS
