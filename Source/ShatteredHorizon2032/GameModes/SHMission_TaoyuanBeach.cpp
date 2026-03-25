// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.
//
// OPERATION BREAKWATER — Mission 1: Taoyuan Beach
//
// Narrative techniques applied from research:
//   - Saving Private Ryan: auditory exclusion as frequency filter, not volume cut
//   - Letters from Iwo Jima: assault seen through narrow apertures; mundane humanity before violence
//   - 13 Hours: abandoned by command — support denied/delayed
//   - Full Metal Jacket: single hidden threat paralyzes the force
//   - MW2019 "Clean House": domestic spaces as combat zones
//   - 1917: quiet stretches between violence build dread
//   - Spec Ops The Line: advance through consequences of earlier destruction
//   - Dunkirk: converging radio threads from other units
//   - Apocalypse Now: victory and horror occupy the same frame
//   - Brothers in Arms: named characters with relationships who can die
//   - This War of Mine: resource scarcity as moral pressure
//
// Mission structure: THIS MISSION IS ALWAYS A LOSS.
// Phase 1: Pre-Invasion (0430, silence and dread)
// Phase 2: Beach Assault (0500, overwhelming force)
// Phase 3: Urban Fallback (0630, fighting withdrawal through civilian areas)
// Phase 4: Collapse (0800, the line breaks, retreat into guerrilla positions)
//
// The airport falls. The beach is lost. The question is: who survives?

#include "SHMission_TaoyuanBeach.h"
#include "SHMissionDefinition.h"

// =========================================================================
//  Helpers
// =========================================================================

FSHSquadTemplate USHMission_TaoyuanBeach::MakePLASquad(int32 Riflemen, int32 MG, int32 RPG, int32 Officers)
{
	FSHSquadTemplate Squad;
	Squad.SquadSize = Riflemen + MG + RPG + Officers;
	if (Riflemen > 0) { Squad.Roles.Add(ESHEnemyRole::Rifleman, Riflemen); }
	if (MG > 0) { Squad.Roles.Add(ESHEnemyRole::MachineGunner, MG); }
	if (RPG > 0) { Squad.Roles.Add(ESHEnemyRole::RPG, RPG); }
	if (Officers > 0) { Squad.Roles.Add(ESHEnemyRole::Officer, Officers); Squad.bHasOfficer = true; }
	Squad.MoraleLevel = 0.9f;
	return Squad;
}

FSHDialogueTrigger USHMission_TaoyuanBeach::MakeDialogue(
	float Delay, const FString& Speaker, const FString& Text, bool bRadio, int32 Priority)
{
	FSHDialogueTrigger DT;
	DT.TriggerType = ESHDialogueTriggerType::Delay;
	DT.Delay = Delay;
	DT.Speaker = FText::FromString(Speaker);
	DT.DialogueText = FText::FromString(Text);
	DT.bIsRadioComm = bRadio;
	DT.Priority = Priority;
	return DT;
}

FSHScriptedEvent USHMission_TaoyuanBeach::MakeDelayedEvent(
	float Delay, ESHScriptedEventType EventType, const FString& EventId,
	const FString& Description, FVector Location, float Radius, float Duration, float Intensity)
{
	FSHScriptedEvent SE;
	SE.EventId = FName(*EventId);
	SE.TriggerType = ESHScriptedEventTriggerType::Delay;
	SE.TriggerDelay = Delay;
	SE.EventType = EventType;
	SE.EventLocation = Location;
	SE.EventRadius = Radius;
	SE.EventDuration = Duration;
	SE.EventIntensity = Intensity;
	SE.EventDescription = FText::FromString(Description);
	return SE;
}

// =========================================================================
//  Factory
// =========================================================================

USHMissionDefinition* USHMission_TaoyuanBeach::CreateMissionDefinition(UObject* Outer)
{
	USHMissionDefinition* Def = NewObject<USHMissionDefinition>(Outer, FName(TEXT("M01_TaoyuanBeach")));

	Def->MissionId = FName(TEXT("M01_TaoyuanBeach"));
	Def->MapAssetPath = FSoftObjectPath(TEXT("/Game/Maps/TaoyuanBeach"));
	Def->EstimatedPlayTime = 50.f;

	Def->DifficultyScaling.Add(0, 0.6f);  // Recruit
	Def->DifficultyScaling.Add(1, 0.8f);  // Regular
	Def->DifficultyScaling.Add(2, 1.0f);  // Hardened
	Def->DifficultyScaling.Add(3, 1.2f);  // Veteran
	Def->DifficultyScaling.Add(4, 1.5f);  // Primordia Unleashed

	BuildSquadRoster(Def);
	BuildBriefing(Def);
	BuildPhase_PreInvasion(Def);
	BuildPhase_BeachAssault(Def);
	BuildPhase_UrbanFallback(Def);
	BuildPhase_Counterattack(Def);  // Now "Collapse" — the line breaks

	return Def;
}

// =========================================================================
//  Squad Roster — These people matter. When they bleed, it should hurt.
// =========================================================================

void USHMission_TaoyuanBeach::BuildSquadRoster(USHMissionDefinition* Def)
{
	// Viper-2: Cpl. James Vasquez — Automatic Rifleman, Veteran
	// The steady hand. Three deployments. Has a daughter named Sofia, age 4.
	{
		FSHSquadMemberDef M;
		M.Name = FText::FromString(TEXT("Cpl. James Vasquez"));
		M.Callsign = FText::FromString(TEXT("Viper-2"));
		M.Rank = FText::FromString(TEXT("Corporal"));
		M.Role = ESHSquadMemberRole::AutomaticRifleman;
		M.PersonalityTrait = ESHPersonalityTrait::Veteran;
		M.BackstorySnippet = FText::FromString(
			TEXT("Puerto Rican from the Bronx. Third deployment. Married — daughter Sofia, age 4. "
				 "Carries a photo of her in his helmet band. The kind of Marine who doesn't talk about "
				 "what he's seen but you can read it in his eyes. When everything goes wrong, "
				 "Vasquez is the one who keeps talking in a steady voice."));
		Def->SquadRoster.Add(M);
	}

	// Viper-3: LCpl. David Kim — Grenadier, Calm
	// The quiet faith. Carries a small cross. Never raises his voice.
	{
		FSHSquadMemberDef M;
		M.Name = FText::FromString(TEXT("LCpl. David Kim"));
		M.Callsign = FText::FromString(TEXT("Viper-3"));
		M.Rank = FText::FromString(TEXT("Lance Corporal"));
		M.Role = ESHSquadMemberRole::Grenadier;
		M.PersonalityTrait = ESHPersonalityTrait::Calm;
		M.BackstorySnippet = FText::FromString(
			TEXT("Korean-American from Fullerton, California. Devout Christian — carries a small wooden "
				 "cross his grandmother gave him. Soft-spoken. The one who prays before every engagement "
				 "and never raises his voice, even under fire. Vasquez calls him 'the calmest man "
				 "in any room that's exploding.'"));
		Def->SquadRoster.Add(M);
	}

	// Viper-4: PFC. Sarah Chen — Marksman, Aggressive
	// This is personal. Family in Taipei. She volunteered for this.
	{
		FSHSquadMemberDef M;
		M.Name = FText::FromString(TEXT("PFC. Sarah Chen"));
		M.Callsign = FText::FromString(TEXT("Viper-4"));
		M.Rank = FText::FromString(TEXT("Private First Class"));
		M.Role = ESHSquadMemberRole::Marksman;
		M.PersonalityTrait = ESHPersonalityTrait::Aggressive;
		M.BackstorySnippet = FText::FromString(
			TEXT("Taiwanese-American. Parents and younger brother still in Taipei — they were supposed "
				 "to fly out three days ago. The flight was cancelled. She volunteered for this deployment "
				 "knowing exactly where she was going. Aggressive, precise, and carrying a fury she "
				 "channels into every shot. Kim worries about her. Vasquez respects her. "
				 "Doc is a little afraid of her."));
		Def->SquadRoster.Add(M);
	}

	// Viper-5: HN. Marcus Williams — Corpsman, Cautious
	// First deployment. Terrified. Carries more bandages than bullets.
	{
		FSHSquadMemberDef M;
		M.Name = FText::FromString(TEXT("HN. Marcus Williams"));
		M.Callsign = FText::FromString(TEXT("Viper-5"));
		M.Rank = FText::FromString(TEXT("Hospitalman"));
		M.Role = ESHSquadMemberRole::Rifleman;
		M.PersonalityTrait = ESHPersonalityTrait::Cautious;
		M.BackstorySnippet = FText::FromString(
			TEXT("Navy Corpsman attached to the squad. First deployment. From Atlanta. Twenty-two years old. "
				 "Terrified but competent — the kind of scared that makes you careful, not frozen. "
				 "Carries more medical supplies than ammunition. Writes letters to his mother that he "
				 "never sends. The one who keeps everyone alive, if anyone can."));
		Def->SquadRoster.Add(M);
	}
}

// =========================================================================
//  Briefing — Updated with verified CSIS/DoD data
// =========================================================================

void USHMission_TaoyuanBeach::BuildBriefing(USHMissionDefinition* Def)
{
	FSHMissionBriefing& B = Def->Briefing;

	B.MissionName = FText::FromString(TEXT("Taoyuan Beach"));
	B.MissionCallsign = FText::FromString(TEXT("Operation Breakwater"));

	B.SettingDescription = FText::FromString(
		TEXT("0430 Local Time | 15 June 2032 | Dayuan District, Taoyuan City, Northwestern Taiwan\n"
			 "Sector 7 — Beach approach to Taoyuan International Airport"));

	B.BriefingNarration = FText::FromString(
		TEXT("Forty-eight hours ago, the People's Liberation Army began staging amphibious forces "
			 "at Pingtan and Dongshan Island. Twelve hours ago, cyber attacks hit Taiwan's financial "
			 "systems and government networks. Six hours ago, ballistic missile launches were detected "
			 "across the strait.\n\n"
			 "This is happening.\n\n"
			 "Your squad — callsign Viper — is attached to the ROC 206th Infantry Brigade defending "
			 "Sector 7: the beach approach to Taoyuan International Airport. That airport is the primary "
			 "logistics hub for allied reinforcement. If it falls in the first twenty-four hours, "
			 "there is no way to get enough people and material onto this island to stop what comes next.\n\n"
			 "PLA first-wave estimates are twenty-five to forty thousand across ten to twenty landing zones. "
			 "Your sector will receive two to four thousand. They have air superiority. They have naval "
			 "fire support. They have drones, electronic warfare, and the numbers.\n\n"
			 "We have prepared positions, knowledge of the terrain, and the fact that they have to "
			 "cross open water to reach us.\n\n"
			 "III MEF out of Okinawa is spinning up. Seventh Fleet is moving. But they need time. "
			 "Seventy-two hours. That's the number. Every hour you hold changes the math for "
			 "the entire theater.\n\n"
			 "Hold the line. Buy time. That's all we need from you.\n\n"
			 "That's everything."));

	B.ObjectiveSummaries.Add(FText::FromString(TEXT("Defend Beach Sector 7 against PLA amphibious assault")));
	B.ObjectiveSummaries.Add(FText::FromString(TEXT("Protect civilian evacuation route through Dayuan")));
	B.ObjectiveSummaries.Add(FText::FromString(TEXT("Delay enemy advance as long as possible")));
	B.ObjectiveSummaries.Add(FText::FromString(TEXT("Survive. Get your people into the city.")));

	B.IntelNotes.Add(FText::FromString(
		TEXT("FORCE ESTIMATE: PLA theater-level invasion force assessed at 280,000+ across all echelons. "
			 "First-wave amphibious lift capacity: 25,000-40,000. Local sector estimate: 2,000-4,000 initial.")));
	B.IntelNotes.Add(FText::FromString(
		TEXT("ORDER OF BATTLE: First wave will be PLANMC amphibious infantry from 72nd/73rd Group Armies. "
			 "Type 05 AAVs and Type 08 IFVs confirmed in staging. Expect squad-level organization with "
			 "organic MG, RPG, and officer elements.")));
	B.IntelNotes.Add(FText::FromString(
		TEXT("AIR PICTURE: PLAAF has functional air superiority over the strait. 300+ J-20s, 450+ J-16s "
			 "against ROCAF's 140 F-16Vs. Expect persistent drone overflights (BZK-005, Wing Loong II) "
			 "and periodic manned air strikes. Northern Taiwan has NO active fighter base — air defense "
			 "is entirely SAM-based.")));
	B.IntelNotes.Add(FText::FromString(
		TEXT("ELECTRONIC WARFARE: PLA EW activation confirmed. GPS denial, communications jamming, and "
			 "radar disruption expected across your AO. Undersea fiber-optic cables to Taiwan may be cut. "
			 "Switch to backup navigation and alternate comms protocols.")));
	B.IntelNotes.Add(FText::FromString(
		TEXT("CIVILIAN STATUS: Dayuan District population ~87,000. Evacuation is incomplete — "
			 "estimated 10,000-15,000 civilians remain in the area. ROE is strict near populated zones. "
			 "Positive identification required before engagement in built-up areas.")));
	B.IntelNotes.Add(FText::FromString(
		TEXT("FRIENDLY FORCES: ROC 206th Infantry Brigade is primary defense force for Taoyuan sector. "
			 "USMC advisory and fire support elements attached. Limited artillery available. "
			 "No naval fire support — 7th Fleet not yet in range.")));
}

// =========================================================================
//  PHASE 1: PRE-INVASION
//
//  Technique: Letters from Iwo Jima — mundane humanity before violence.
//  Technique: 1917 — quiet stretches where silence IS the tension.
//
//  3 minutes of dread. Mostly silence. The player sits in a trench
//  in the dark, hearing waves. Dialogue is sparse — when it comes,
//  it hits harder because of the silence around it. The invasion is
//  seen through narrow apertures: firing slits, trench walls.
//  The player never sees the full scope until it's on top of them.
// =========================================================================

void USHMission_TaoyuanBeach::BuildPhase_PreInvasion(USHMissionDefinition* Def)
{
	FSHPhaseDefinition Phase;
	Phase.Phase = ESHMissionPhase::PreInvasion;
	Phase.PhaseName = FText::FromString(TEXT("Before Dawn"));
	Phase.PhaseDescription = FText::FromString(
		TEXT("Darkness. The sound of waves on sand. Your squad is dug into a trench line "
			 "facing the Taiwan Strait. The pre-dawn air is warm and humid. Someone is breathing too fast. "
			 "Nobody speaks for a long time."));
	Phase.Duration = 210.f; // 3.5 minutes — longer, to let the silence work
	Phase.AmbientWeather = ESHWeatherType::Overcast;
	Phase.TimeOfDayStart = 4.5f;
	Phase.AdvanceCondition = ESHPhaseAdvanceCondition::TimerExpired;

	// --- Objective ---
	{
		FSHPhaseObjective Obj;
		Obj.ObjectiveId = FGuid(0x00000001, 0x00000000, 0x00000000, 0x00000001);
		Obj.DisplayName = FText::FromString(TEXT("Hold Position"));
		Obj.Description = FText::FromString(TEXT("Wait."));
		Obj.Type = ESHObjectiveType::Defend;
		Obj.Priority = ESHObjectivePriority::Primary;
		Obj.WorldLocation = FVector(0.f, 0.f, 0.f);
		Obj.AreaRadius = 5000.f;
		Obj.DefendDuration = 180.f;
		Phase.Objectives.Add(Obj);
	}

	// --- Dialogue: SPARSE. Long gaps. Let silence carry. ---

	// 30 seconds of nothing but waves. Then:
	Phase.DialogueTriggers.Add(MakeDialogue(30.f,
		TEXT("Viper-5 (Doc)"), TEXT("[quiet] ...anybody got gum?"), false));

	// 15 more seconds of silence.
	Phase.DialogueTriggers.Add(MakeDialogue(48.f,
		TEXT("Viper-2 (Vasquez)"), TEXT("[low voice] Left pocket."), false));

	// Long pause. 30 seconds.
	Phase.DialogueTriggers.Add(MakeDialogue(80.f,
		TEXT("Viper-4 (Riot)"), TEXT("[barely audible] ...my brother's birthday is Thursday."), false));

	// Nobody responds. 20 seconds.

	// Then the radio breaks the silence like a gunshot:
	Phase.DialogueTriggers.Add(MakeDialogue(105.f,
		TEXT("WATCHDOG"), TEXT("All stations, all stations. FLASH traffic. Missile launches detected "
		"across the strait. Multiple sites. Trajectory consistent with Taiwan strike package. "
		"This is not a drill."), true, 100));

	// 10 seconds for that to sink in.
	Phase.DialogueTriggers.Add(MakeDialogue(118.f,
		TEXT("Viper-2 (Vasquez)"), TEXT("Check your lanes. Mags topped. Nobody fires until I call it."), false));

	Phase.DialogueTriggers.Add(MakeDialogue(135.f,
		TEXT("Viper-3 (Preacher)"), TEXT("[murmured, almost to himself] The Lord is my shepherd. "
		"I shall not want..."), false));

	// EW notification — clinical, cold
	Phase.DialogueTriggers.Add(MakeDialogue(150.f,
		TEXT("SIGINT"), TEXT("All units, GPS degradation in effect across northern Taiwan. "
		"Comms reliability is degrading. Switch to backup protocols."), true, 80));

	Phase.DialogueTriggers.Add(MakeDialogue(160.f,
		TEXT("Viper-5 (Doc)"), TEXT("I'm getting static on secondary. They're jamming everything."), false));

	// Distant flashes. Missiles hitting the airfield.
	Phase.DialogueTriggers.Add(MakeDialogue(175.f,
		TEXT("Viper-4 (Riot)"), TEXT("Flashes. Southwest. That's the airfield."), false));

	Phase.DialogueTriggers.Add(MakeDialogue(180.f,
		TEXT("Viper-3 (Preacher)"), TEXT("God help them."), false));

	// The final call — what they see through the firing slits
	Phase.DialogueTriggers.Add(MakeDialogue(195.f,
		TEXT("WATCHDOG"), TEXT("All units. First wave visual. Bearing zero-two-zero. Multiple "
		"amphibious craft. Estimated two thousand personnel. This is it. God speed."), true, 100));

	Phase.DialogueTriggers.Add(MakeDialogue(205.f,
		TEXT("Viper-2 (Vasquez)"), TEXT("[very quiet] ...Sofia, daddy loves you."), false));

	// --- Scripted Events ---

	// Narrow aperture: player sees the assault ONLY through firing slits for first 30s of Phase 2
	Phase.ScriptedEvents.Add(MakeDelayedEvent(0.f, ESHScriptedEventType::NarrowAperture,
		TEXT("TrenchView"), TEXT("Player view restricted to trench firing slits"),
		FVector::ZeroVector, 0.f, 210.f, 1.f));

	// Quiet stretch: suppress ambient combat audio, emphasize waves and breathing
	Phase.ScriptedEvents.Add(MakeDelayedEvent(0.f, ESHScriptedEventType::QuietStretch,
		TEXT("PreDawnSilence"), TEXT("Suppress combat audio — only waves, wind, breathing"),
		FVector::ZeroVector, 0.f, 100.f, 0.9f));

	// EW jamming activates
	Phase.ScriptedEvents.Add(MakeDelayedEvent(100.f, ESHScriptedEventType::EWJammingZone,
		TEXT("EW_GPSDenial"), TEXT("PLA GPS denial zone activates across the AO"),
		FVector(0.f, 0.f, 0.f), 500000.f, 0.f, 0.7f));

	// Distant missile strikes on the airfield — player sees flashes through the slit
	Phase.ScriptedEvents.Add(MakeDelayedEvent(170.f, ESHScriptedEventType::Explosion,
		TEXT("MissileStrike_Airfield"), TEXT("PLA missiles hit Taoyuan International Airport"),
		FVector(300000.f, 100000.f, 0.f), 50000.f, 8.f, 0.8f));

	// Preparatory artillery on beach
	Phase.ScriptedEvents.Add(MakeDelayedEvent(198.f, ESHScriptedEventType::ArtilleryBarrage,
		TEXT("PrepFire_Beach"), TEXT("PLA preparatory artillery barrage"),
		FVector(5000.f, 0.f, 0.f), 15000.f, 12.f, 0.9f));

	Def->Phases.Add(Phase);
}

// =========================================================================
//  PHASE 2: BEACH ASSAULT
//
//  Technique: Saving Private Ryan — auditory exclusion at peak stress
//  Technique: 13 Hours — support requests denied, growing isolation
//  Technique: Dunkirk — converging radio threads from other units
//  Technique: Black Hawk Down — cumulative physical degradation
//
//  Overwhelming force. The player fights to hold but the math is wrong.
//  Too many of them. Support is denied. Other sectors are falling.
//  The radio tells the story of a battle being lost everywhere at once.
// =========================================================================

void USHMission_TaoyuanBeach::BuildPhase_BeachAssault(USHMissionDefinition* Def)
{
	FSHPhaseDefinition Phase;
	Phase.Phase = ESHMissionPhase::BeachAssault;
	Phase.PhaseName = FText::FromString(TEXT("Beach Assault"));
	Phase.PhaseDescription = FText::FromString(
		TEXT("Dawn. The ramps drop. They come out of the water in numbers that don't make sense. "
			 "Your prepared kill zones work — for the first sixty seconds. Then there are too many. "
			 "You are one squad on one sector of a beach that stretches for kilometers, and every "
			 "sector is screaming for help on the same radio net."));
	Phase.Duration = 0.f;
	Phase.AmbientWeather = ESHWeatherType::Overcast;
	Phase.TimeOfDayStart = 5.0f;
	Phase.AdvanceCondition = ESHPhaseAdvanceCondition::EnemyBreachesLine;
	Phase.AdvanceThreshold = 0.6f;

	// --- Objectives ---
	{
		FSHPhaseObjective Obj;
		Obj.ObjectiveId = FGuid(0x00000002, 0x00000000, 0x00000000, 0x00000001);
		Obj.DisplayName = FText::FromString(TEXT("Hold Beach Sector 7"));
		Obj.Description = FText::FromString(TEXT("Every minute you hold buys time for the airport defense. Hold as long as you can."));
		Obj.Type = ESHObjectiveType::Defend;
		Obj.Priority = ESHObjectivePriority::Primary;
		Obj.WorldLocation = FVector(0.f, 0.f, 0.f);
		Obj.AreaRadius = 8000.f;
		Obj.DefendDuration = 600.f;
		Obj.bIsCritical = false; // NOT critical — this objective WILL fail. That's the point.
		Phase.Objectives.Add(Obj);
	}
	{
		FSHPhaseObjective Obj;
		Obj.ObjectiveId = FGuid(0x00000002, 0x00000000, 0x00000000, 0x00000002);
		Obj.DisplayName = FText::FromString(TEXT("Protect Civilian Evacuation Route"));
		Obj.Description = FText::FromString(TEXT("Civilian convoy moving through Grid 4. Keep the route open."));
		Obj.Type = ESHObjectiveType::Defend;
		Obj.Priority = ESHObjectivePriority::Secondary;
		Obj.WorldLocation = FVector(10000.f, 5000.f, 0.f);
		Obj.AreaRadius = 5000.f;
		Obj.DefendDuration = 300.f;
		Phase.Objectives.Add(Obj);
	}

	// --- Wave 1: First Landing (0s) — 24 infantry ---
	{
		FSHWaveDefinition W;
		W.WaveName = FText::FromString(TEXT("First Landing Wave"));
		W.DelayFromPhaseStart = 0.f;
		W.SpawnZoneTag = FName(TEXT("Beach_North"));
		W.TotalInfantry = 24;
		W.ApproachDirection = FVector(-1.f, 0.f, 0.f);
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		W.SquadComposition.Add(MakePLASquad(4, 1, 0, 1));
		W.SquadComposition.Add(MakePLASquad(4, 1, 0, 1));
		Phase.ReinforcementWaves.Add(W);
	}
	// --- Wave 2: Second Wave + Armor (90s) ---
	{
		FSHWaveDefinition W;
		W.WaveName = FText::FromString(TEXT("Second Wave"));
		W.DelayFromPhaseStart = 90.f;
		W.SpawnZoneTag = FName(TEXT("Beach_NorthEast"));
		W.TotalInfantry = 18;
		W.bIncludesVehicles = true;
		W.VehicleCount = 2;
		W.ApproachDirection = FVector(-0.7f, 0.7f, 0.f);
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		Phase.ReinforcementWaves.Add(W);
	}
	// --- Wave 3: Reinforcement + Drones (210s) ---
	{
		FSHWaveDefinition W;
		W.WaveName = FText::FromString(TEXT("Third Wave"));
		W.DelayFromPhaseStart = 210.f;
		W.SpawnZoneTag = FName(TEXT("Beach_North"));
		W.TotalInfantry = 30;
		W.bIncludesVehicles = true;
		W.VehicleCount = 1;
		W.ApproachDirection = FVector(-1.f, 0.2f, 0.f);
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		W.SquadComposition.Add(MakePLASquad(4, 1, 0, 1));
		W.SquadComposition.Add(MakePLASquad(4, 1, 0, 1));
		Phase.ReinforcementWaves.Add(W);
	}
	// --- Wave 4: Final Push (360s) ---
	{
		FSHWaveDefinition W;
		W.WaveName = FText::FromString(TEXT("Final Push"));
		W.DelayFromPhaseStart = 360.f;
		W.SpawnZoneTag = FName(TEXT("Beach_AllSectors"));
		W.TotalInfantry = 42;
		W.bIncludesVehicles = true;
		W.VehicleCount = 4;
		W.bIncludesAirSupport = true;
		W.ApproachDirection = FVector(-1.f, 0.f, 0.f);
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		W.SquadComposition.Add(MakePLASquad(4, 1, 0, 1));
		W.SquadComposition.Add(MakePLASquad(4, 1, 0, 1));
		W.SquadComposition.Add(MakePLASquad(4, 1, 0, 1));
		Phase.ReinforcementWaves.Add(W);
	}

	// --- Dialogue: builds the story of support being denied ---

	Phase.DialogueTriggers.Add(MakeDialogue(3.f,
		TEXT("Viper-2 (Vasquez)"), TEXT("Contact waterline! Ramps dropping!"), false, 90));
	Phase.DialogueTriggers.Add(MakeDialogue(8.f,
		TEXT("WATCHDOG"), TEXT("Viper-1, weapons free. Engage at will."), true, 100));

	// Player fights for 50 seconds. Then the radio starts telling the larger story:
	Phase.DialogueTriggers.Add(MakeDialogue(55.f,
		TEXT("BRAVO-6"), TEXT("[other unit, faint] Bravo is in heavy contact Sector 3! Multiple APCs "
		"on the beach! Requesting immediate fire support!"), true, 40));

	Phase.DialogueTriggers.Add(MakeDialogue(70.f,
		TEXT("WATCHDOG"), TEXT("Bravo-6, fire support is... unavailable at this time. All assets committed."), true, 50));

	// Abandoned by command — first denial
	Phase.DialogueTriggers.Add(MakeDialogue(100.f,
		TEXT("Viper-2 (Vasquez)"), TEXT("Watchdog, Viper-1. Requesting CAS on the beach approach. We've got "
		"armor in the open, danger close—"), false, 80));
	Phase.DialogueTriggers.Add(MakeDialogue(108.f,
		TEXT("WATCHDOG"), TEXT("Viper-1... negative on CAS. PLAAF has air denial over the strait. "
		"No fixed-wing available. You're on your own for now."), true, 90));

	Phase.DialogueTriggers.Add(MakeDialogue(120.f,
		TEXT("Viper-5 (Doc)"), TEXT("On our own? There's a thousand of them out there!"), false));

	Phase.DialogueTriggers.Add(MakeDialogue(130.f,
		TEXT("Viper-4 (Riot)"), TEXT("Then we kill a thousand of them."), false));

	// Second wave arrives — growing desperation
	Phase.DialogueTriggers.Add(MakeDialogue(160.f,
		TEXT("Viper-3 (Preacher)"), TEXT("Second wave. APCs on the sand. They're not stopping."), false));

	// More radio threads — other units in trouble (Dunkirk technique)
	Phase.DialogueTriggers.Add(MakeDialogue(200.f,
		TEXT("CHARLIE-2"), TEXT("[distant, stressed] Charlie-2 is falling back! Sector 5 is breached! "
		"We've lost the seawall!"), true, 40));

	Phase.DialogueTriggers.Add(MakeDialogue(220.f,
		TEXT("Viper-2 (Vasquez)"), TEXT("Sector 5 is gone. We're it. Just us and whatever's left of 206th."), false));

	// Civilian convoy — ROE tension
	Phase.DialogueTriggers.Add(MakeDialogue(260.f,
		TEXT("WATCHDOG"), TEXT("All units Sector 7, civilian convoy still moving through Grid 4. "
		"Positive ID before engagement. ROE is STRICT."), true, 80));

	Phase.DialogueTriggers.Add(MakeDialogue(270.f,
		TEXT("Viper-4 (Riot)"), TEXT("There are people on that road! Families with suitcases! "
		"They're running between the impacts!"), false));

	// Third wave — abandoned by command, second denial
	Phase.DialogueTriggers.Add(MakeDialogue(300.f,
		TEXT("Viper-2 (Vasquez)"), TEXT("Watchdog, Viper-1 requesting artillery, Grid—"), false, 80));
	Phase.DialogueTriggers.Add(MakeDialogue(305.f,
		TEXT("WATCHDOG"), TEXT("Viper-1, all artillery assets are... [static] ...priority fire mission "
		"for Sector... [static] ...negative your request."), true, 80));
	Phase.DialogueTriggers.Add(MakeDialogue(312.f,
		TEXT("Viper-2 (Vasquez)"), TEXT("Say again, Watchdog? Watchdog?"), false));
	Phase.DialogueTriggers.Add(MakeDialogue(316.f,
		TEXT("Viper-5 (Doc)"), TEXT("They're gone. The comms are gone."), false));

	// The moment it breaks
	Phase.DialogueTriggers.Add(MakeDialogue(380.f,
		TEXT("Viper-3 (Preacher)"), TEXT("There's too many. There's too many of them."), false));

	Phase.DialogueTriggers.Add(MakeDialogue(400.f,
		TEXT("Viper-5 (Doc)"), TEXT("I've got wounded everywhere! I can't— I don't have enough "
		"bandages for this!"), false));

	// The fall back order
	Phase.DialogueTriggers.Add(MakeDialogue(430.f,
		TEXT("WATCHDOG"), TEXT("[through heavy static] All units... fall back to Phase Line Bravo. "
		"Urban defensive positions. The beach... the beach is lost."), true, 100));

	Phase.DialogueTriggers.Add(MakeDialogue(438.f,
		TEXT("Viper-2 (Vasquez)"), TEXT("You heard it. Grab who you can. We move NOW."), false, 90));

	Phase.DialogueTriggers.Add(MakeDialogue(445.f,
		TEXT("Viper-4 (Riot)"), TEXT("[shouting over gunfire] The airfield— they're going to take "
		"the airfield!"), false));

	Phase.DialogueTriggers.Add(MakeDialogue(450.f,
		TEXT("Viper-2 (Vasquez)"), TEXT("We can't save the airfield! We save who's next to us! Move!"), false));

	// --- Scripted Events ---

	// Drone overwatch
	Phase.ScriptedEvents.Add(MakeDelayedEvent(45.f, ESHScriptedEventType::DroneSwarm,
		TEXT("ISR_Overwatch"), TEXT("PLA ISR drones establish overwatch"),
		FVector(0.f, 0.f, 30000.f), 20000.f, 0.f, 0.5f));

	// Shell shock moment (Saving Private Ryan technique) — at peak combat intensity
	Phase.ScriptedEvents.Add(MakeDelayedEvent(180.f, ESHScriptedEventType::ShellShock,
		TEXT("ShellShock_Beach"), TEXT("Explosion triggers auditory exclusion — selective frequency "
		"dropout, tinny hum, time feels wrong for 4 seconds"),
		FVector(200.f, 0.f, 0.f), 500.f, 4.f, 1.f));

	// Counter-battery on player position
	Phase.ScriptedEvents.Add(MakeDelayedEvent(240.f, ESHScriptedEventType::CounterBattery,
		TEXT("CounterBattery"), TEXT("PLA counter-battery fire targets player position"),
		FVector(0.f, 0.f, 0.f), 8000.f, 15.f, 0.8f));

	// Air strike — PLA jets
	Phase.ScriptedEvents.Add(MakeDelayedEvent(340.f, ESHScriptedEventType::AirStrike,
		TEXT("AirStrike_Beach"), TEXT("PLA J-16 strafing run along beach defenses"),
		FVector(0.f, -3000.f, 0.f), 12000.f, 8.f, 0.9f));

	// Abandoned by command — CAS denied, artillery denied (13 Hours technique)
	Phase.ScriptedEvents.Add(MakeDelayedEvent(100.f, ESHScriptedEventType::AbandonedByCommand,
		TEXT("CAS_Denied"), TEXT("Close air support request denied — PLAAF has air denial"),
		FVector::ZeroVector, 0.f, 0.f, 1.f));

	Phase.ScriptedEvents.Add(MakeDelayedEvent(300.f, ESHScriptedEventType::AbandonedByCommand,
		TEXT("Arty_Denied"), TEXT("Artillery request denied — all assets committed elsewhere"),
		FVector::ZeroVector, 0.f, 0.f, 1.f));

	// Weather deteriorates
	Phase.ScriptedEvents.Add(MakeDelayedEvent(350.f, ESHScriptedEventType::WeatherChange,
		TEXT("Weather_Rain"), TEXT("Weather deteriorates — light rain"),
		FVector::ZeroVector, 0.f, 60.f, 0.4f));

	// Civilian presence on the road (MW2019 technique)
	Phase.ScriptedEvents.Add(MakeDelayedEvent(250.f, ESHScriptedEventType::CivilianPresence,
		TEXT("Civilian_Convoy"), TEXT("Civilian evacuation convoy visible on the road — families with "
		"suitcases running between artillery impacts"),
		FVector(10000.f, 5000.f, 0.f), 3000.f, 120.f, 0.7f));

	Def->Phases.Add(Phase);
}

// =========================================================================
//  PHASE 3: URBAN FALLBACK
//
//  Technique: Full Metal Jacket — single sniper stops all movement
//  Technique: MW2019 "Clean House" — fighting through someone's home
//  Technique: 1917 — quiet stretches between violence, transition architecture
//  Technique: Black Hawk Down — navigational confusion, identical streets
//  Technique: Valiant Hearts — civilians as presence, not objectives
//
//  The beach is lost. The squad fights through Dayuan's streets.
//  Domestic spaces. Families still here. A sniper pins the squad.
//  Quiet stretches where empty streets are more terrifying than gunfire.
// =========================================================================

void USHMission_TaoyuanBeach::BuildPhase_UrbanFallback(USHMissionDefinition* Def)
{
	FSHPhaseDefinition Phase;
	Phase.Phase = ESHMissionPhase::UrbanFallback;
	Phase.PhaseName = FText::FromString(TEXT("Dayuan Streets"));
	Phase.PhaseDescription = FText::FromString(
		TEXT("Rain. Smoke. The streets of Dayuan. Your squad is moving through a place where people "
			 "lived normal lives twelve hours ago. Dinner is still on some of the tables. There are "
			 "shoes by front doors. The enemy is behind you, and somewhere ahead. Every corner could "
			 "be either one."));
	Phase.Duration = 0.f;
	Phase.AmbientWeather = ESHWeatherType::LightRain;
	Phase.TimeOfDayStart = 6.5f;
	Phase.AdvanceCondition = ESHPhaseAdvanceCondition::AllObjectivesComplete;

	// --- Objectives ---
	{
		FSHPhaseObjective Obj;
		Obj.ObjectiveId = FGuid(0x00000003, 0x00000000, 0x00000000, 0x00000001);
		Obj.DisplayName = FText::FromString(TEXT("Reach Rally Point Alpha"));
		Obj.Description = FText::FromString(TEXT("The school on Route 4. Two kilometers. Get there."));
		Obj.Type = ESHObjectiveType::Extract;
		Obj.Priority = ESHObjectivePriority::Primary;
		Obj.WorldLocation = FVector(40000.f, 10000.f, 0.f);
		Obj.AreaRadius = 3000.f;
		Obj.bIsCritical = true;
		Phase.Objectives.Add(Obj);
	}
	{
		FSHPhaseObjective Obj;
		Obj.ObjectiveId = FGuid(0x00000003, 0x00000000, 0x00000000, 0x00000002);
		Obj.DisplayName = FText::FromString(TEXT("Rescue Wounded Marine"));
		Obj.Description = FText::FromString(TEXT("2nd Platoon Marine, trapped in a building. Bleeding out."));
		Obj.Type = ESHObjectiveType::Extract;
		Obj.Priority = ESHObjectivePriority::Secondary;
		Obj.WorldLocation = FVector(20000.f, 8000.f, 0.f);
		Obj.AreaRadius = 1500.f;
		Obj.TimeLimit = 180.f;
		Phase.Objectives.Add(Obj);
	}

	// --- Waves (pursuit force, lighter than beach — the pressure is psychological, not numerical) ---
	{
		FSHWaveDefinition W;
		W.WaveName = FText::FromString(TEXT("Pursuit Force"));
		W.DelayFromPhaseStart = 0.f;
		W.SpawnZoneTag = FName(TEXT("Urban_Beach_Side"));
		W.TotalInfantry = 16;
		W.ApproachDirection = FVector(-1.f, 0.f, 0.f);
		W.SquadComposition.Add(MakePLASquad(4, 1, 0, 1));
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		Phase.ReinforcementWaves.Add(W);
	}
	{
		FSHWaveDefinition W;
		W.WaveName = FText::FromString(TEXT("Flanking Element"));
		W.DelayFromPhaseStart = 200.f;
		W.SpawnZoneTag = FName(TEXT("Urban_East"));
		W.TotalInfantry = 12;
		W.ApproachDirection = FVector(0.f, 1.f, 0.f);
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		Phase.ReinforcementWaves.Add(W);
	}

	// --- Dialogue ---

	Phase.DialogueTriggers.Add(MakeDialogue(0.f,
		TEXT("Viper-2 (Vasquez)"), TEXT("Off the beach. Into the streets. Stay low, stay together."), false, 80));

	// Quiet stretch — 30 seconds of nothing. Empty street. Distant gunfire from other sectors.
	Phase.DialogueTriggers.Add(MakeDialogue(35.f,
		TEXT("Viper-5 (Doc)"), TEXT("[whispering] It's quiet here. Why is it quiet?"), false));

	Phase.DialogueTriggers.Add(MakeDialogue(50.f,
		TEXT("Viper-3 (Preacher)"), TEXT("[whispering] Because they haven't gotten here yet."), false));

	// Domestic space — fighting through someone's home
	Phase.DialogueTriggers.Add(MakeDialogue(80.f,
		TEXT("Viper-4 (Riot)"), TEXT("[entering a building] ...there's dinner on the table. "
		"Rice and fish. They left in the middle of dinner."), false));

	Phase.DialogueTriggers.Add(MakeDialogue(90.f,
		TEXT("Viper-5 (Doc)"), TEXT("Kids' shoes by the door. Little ones."), false));

	Phase.DialogueTriggers.Add(MakeDialogue(100.f,
		TEXT("Viper-2 (Vasquez)"), TEXT("Keep moving. Don't think about it."), false));

	// Civilian encounter — old man who won't leave
	Phase.DialogueTriggers.Add(MakeDialogue(130.f,
		TEXT("Viper-4 (Riot)"), TEXT("[in Mandarin, then English] Sir— sir, you have to go. "
		"[to squad] He won't leave. He says this is his home."), false));

	Phase.DialogueTriggers.Add(MakeDialogue(140.f,
		TEXT("Viper-3 (Preacher)"), TEXT("We can't make him. Chen, tell him to get to the school. "
		"There's people there."), false));

	// SNIPER — Full Metal Jacket technique. Everything stops.
	Phase.DialogueTriggers.Add(MakeDialogue(170.f,
		TEXT("Viper-5 (Doc)"), TEXT("[SHARP] DOWN! Sniper!"), false, 100));

	Phase.DialogueTriggers.Add(MakeDialogue(175.f,
		TEXT("Viper-2 (Vasquez)"), TEXT("Where?! Where is he?!"), false, 90));

	Phase.DialogueTriggers.Add(MakeDialogue(180.f,
		TEXT("Viper-4 (Riot)"), TEXT("I don't know! Somewhere in those buildings! "
		"I can hear the crack but I can't place the report!"), false));

	Phase.DialogueTriggers.Add(MakeDialogue(195.f,
		TEXT("Viper-3 (Preacher)"), TEXT("[calm] Listen. Listen to the next shot. "
		"Crack comes first, then the thump. Time the delay — that's the distance."), false));

	Phase.DialogueTriggers.Add(MakeDialogue(215.f,
		TEXT("Viper-2 (Vasquez)"), TEXT("Chen, can you spot him? You've got the scope."), false));

	Phase.DialogueTriggers.Add(MakeDialogue(225.f,
		TEXT("Viper-4 (Riot)"), TEXT("Working on it. Third floor... maybe fourth. "
		"The building with the green awning. I need another shot from him."), false));

	// Rescue the wounded Marine
	Phase.DialogueTriggers.Add(MakeDialogue(250.f,
		TEXT("Viper-5 (Doc)"), TEXT("I can see him! In that doorway — 2nd Platoon markings. "
		"He's not moving much. I need to get to him."), false, 80));

	Phase.DialogueTriggers.Add(MakeDialogue(258.f,
		TEXT("Viper-2 (Vasquez)"), TEXT("Go. We'll cover. You've got three minutes, Doc."), false));

	// After rescue — quiet moment (Hacksaw Ridge medical perspective)
	Phase.DialogueTriggers.Add(MakeDialogue(310.f,
		TEXT("Viper-5 (Doc)"), TEXT("[breathing hard] Tourniquet's on. He's stable. "
		"He keeps saying a name. Hannah. I think it's his wife."), false));

	Phase.DialogueTriggers.Add(MakeDialogue(325.f,
		TEXT("Viper-3 (Preacher)"), TEXT("He'll see her again. Come on, let's move."), false));

	// Push to rally point
	Phase.DialogueTriggers.Add(MakeDialogue(360.f,
		TEXT("Viper-4 (Riot)"), TEXT("Contact rear! They've caught up!"), false, 80));

	Phase.DialogueTriggers.Add(MakeDialogue(400.f,
		TEXT("Viper-2 (Vasquez)"), TEXT("I can see the school! Two hundred meters! GO!"), false, 90));

	// --- Scripted Events ---

	// Quiet stretch at phase start (1917 technique)
	Phase.ScriptedEvents.Add(MakeDelayedEvent(0.f, ESHScriptedEventType::QuietStretch,
		TEXT("UrbanSilence"), TEXT("30 seconds of eerie quiet — distant gunfire only"),
		FVector::ZeroVector, 0.f, 30.f, 0.8f));

	// Domestic space (MW2019 technique) — combat through a residential building
	Phase.ScriptedEvents.Add(MakeDelayedEvent(75.f, ESHScriptedEventType::DomesticSpace,
		TEXT("ApartmentFight"), TEXT("Squad moves through abandoned apartment — dinner on table, "
		"children's drawings on the fridge, family photos on the walls"),
		FVector(15000.f, 3000.f, 0.f), 2000.f, 30.f, 0.8f));

	// Sniper pin (Full Metal Jacket technique)
	Phase.ScriptedEvents.Add(MakeDelayedEvent(168.f, ESHScriptedEventType::SniperPin,
		TEXT("SniperPin_Street"), TEXT("Single PLA marksman pins the squad — must use acoustic "
		"triangulation to locate. No visual indicators. Movement = death."),
		FVector(18000.f, 4000.f, 1500.f), 8000.f, 60.f, 0.9f));

	// Civilian presence — old man who won't leave his home
	Phase.ScriptedEvents.Add(MakeDelayedEvent(125.f, ESHScriptedEventType::CivilianPresence,
		TEXT("OldMan_Home"), TEXT("Elderly Taiwanese man refuses to leave his home"),
		FVector(16000.f, 2000.f, 0.f), 500.f, 30.f, 0.5f));

	// EW intensifies
	Phase.ScriptedEvents.Add(MakeDelayedEvent(150.f, ESHScriptedEventType::EWJammingZone,
		TEXT("EW_CommsDegrade"), TEXT("EW zone expands — comms unreliable"),
		FVector(20000.f, 0.f, 0.f), 600000.f, 0.f, 0.8f));

	// Building collapse
	Phase.ScriptedEvents.Add(MakeDelayedEvent(280.f, ESHScriptedEventType::Explosion,
		TEXT("BuildingCollapse"), TEXT("Artillery collapses a building near the squad"),
		FVector(22000.f, 3000.f, 0.f), 3000.f, 3.f, 0.9f));

	Def->Phases.Add(Phase);
}

// =========================================================================
//  PHASE 4: COLLAPSE
//
//  Technique: Spec Ops The Line — advance through consequences
//  Technique: Apocalypse Now — the journey from order to chaos
//  Technique: Brothers in Arms — failure as narrative culmination
//  Technique: This War of Mine — the cost of survival
//
//  THIS IS NOT A VICTORY. The airport falls. The defensive line breaks.
//  The squad retreats deeper into Dayuan, through the wreckage of the
//  earlier phases. Bodies on the ground that weren't there before.
//  The mission ends when the squad reaches a defensible position
//  in the urban maze — setting up Mission 2's guerrilla holdout.
//
//  The last line of the mission is not triumph. It is survival.
//  "Seventy-two hours. That's what they said. Seventy-two hours."
// =========================================================================

void USHMission_TaoyuanBeach::BuildPhase_Counterattack(USHMissionDefinition* Def)
{
	FSHPhaseDefinition Phase;
	Phase.Phase = ESHMissionPhase::Collapse;
	Phase.PhaseName = FText::FromString(TEXT("Collapse"));
	Phase.PhaseDescription = FText::FromString(
		TEXT("The line breaks. The airport is burning. You can see the smoke column from everywhere "
			 "in Dayuan. There is no counterattack. There is no cavalry. There is only the squad, "
			 "the city, and the next seventy-two hours."));
	Phase.Duration = 0.f;
	Phase.AmbientWeather = ESHWeatherType::LightRain;
	Phase.TimeOfDayStart = 8.0f;
	Phase.AdvanceCondition = ESHPhaseAdvanceCondition::AllObjectivesComplete;

	// --- Objectives: survive and establish a defensible position ---
	{
		FSHPhaseObjective Obj;
		Obj.ObjectiveId = FGuid(0x00000004, 0x00000000, 0x00000000, 0x00000001);
		Obj.DisplayName = FText::FromString(TEXT("Reach the Warehouse District"));
		Obj.Description = FText::FromString(TEXT("Find a defensible position in the Dayuan warehouse district. Somewhere to hide. Somewhere to fight from."));
		Obj.Type = ESHObjectiveType::Extract;
		Obj.Priority = ESHObjectivePriority::Primary;
		Obj.WorldLocation = FVector(65000.f, 20000.f, 0.f);
		Obj.AreaRadius = 4000.f;
		Obj.bIsCritical = true;
		Phase.Objectives.Add(Obj);
	}
	{
		FSHPhaseObjective Obj;
		Obj.ObjectiveId = FGuid(0x00000004, 0x00000000, 0x00000000, 0x00000002);
		Obj.DisplayName = FText::FromString(TEXT("Salvage Supplies"));
		Obj.Description = FText::FromString(TEXT("Grab what you can. Ammunition, medical supplies, food. You're going to need it."));
		Obj.Type = ESHObjectiveType::Recon;
		Obj.Priority = ESHObjectivePriority::Secondary;
		Obj.WorldLocation = FVector(50000.f, 12000.f, 0.f);
		Obj.AreaRadius = 6000.f;
		Phase.Objectives.Add(Obj);
	}

	// --- Waves: pursuit, but the player is running, not fighting ---
	{
		FSHWaveDefinition W;
		W.WaveName = FText::FromString(TEXT("PLA Advance Guard"));
		W.DelayFromPhaseStart = 30.f;
		W.SpawnZoneTag = FName(TEXT("Urban_West"));
		W.TotalInfantry = 20;
		W.bIncludesVehicles = true;
		W.VehicleCount = 2;
		W.ApproachDirection = FVector(-1.f, 0.f, 0.f);
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		W.SquadComposition.Add(MakePLASquad(4, 1, 0, 1));
		Phase.ReinforcementWaves.Add(W);
	}
	{
		FSHWaveDefinition W;
		W.WaveName = FText::FromString(TEXT("Sweep Element"));
		W.DelayFromPhaseStart = 180.f;
		W.SpawnZoneTag = FName(TEXT("Urban_South"));
		W.TotalInfantry = 12;
		W.ApproachDirection = FVector(0.f, -1.f, 0.f);
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		Phase.ReinforcementWaves.Add(W);
	}

	// --- Dialogue: the loss sinks in ---

	// The airport falls — the player hears it on the radio
	Phase.DialogueTriggers.Add(MakeDialogue(0.f,
		TEXT("WATCHDOG"), TEXT("[broken, static-heavy] All units... Taoyuan airport... has fallen. "
		"PLA forces have... [static] ...the runway. All defensive positions "
		"are... [static] ...fall back to secondary lines. Acknowledge."), true, 100));

	Phase.DialogueTriggers.Add(MakeDialogue(10.f,
		TEXT("Viper-4 (Riot)"), TEXT("[quiet] ...the airport."), false));

	Phase.DialogueTriggers.Add(MakeDialogue(15.f,
		TEXT("Viper-2 (Vasquez)"), TEXT("I heard it. Keep moving."), false));

	Phase.DialogueTriggers.Add(MakeDialogue(20.f,
		TEXT("Viper-4 (Riot)"), TEXT("The airport is GONE, Vasquez! That was the whole point! "
		"Everything we did on that beach—"), false));

	Phase.DialogueTriggers.Add(MakeDialogue(27.f,
		TEXT("Viper-2 (Vasquez)"), TEXT("Was not nothing. We bought time. We got people off that road. "
		"Now we survive this. Chen. Look at me. We survive this."), false));

	// Consequence walk (Spec Ops technique) — advancing through earlier destruction
	Phase.DialogueTriggers.Add(MakeDialogue(60.f,
		TEXT("Viper-5 (Doc)"), TEXT("[passing through area from Phase 2] ...these are ours. "
		"These bodies. These are 206th."), false));

	Phase.DialogueTriggers.Add(MakeDialogue(70.f,
		TEXT("Viper-3 (Preacher)"), TEXT("Don't look. Keep your eyes forward."), false));

	// The radio is dying — other units going dark (Dunkirk technique: threads going silent)
	Phase.DialogueTriggers.Add(MakeDialogue(100.f,
		TEXT("BRAVO-6"), TEXT("[faint, desperate] Bravo-6 to any station! Bravo is combat ineffective! "
		"Seven KIA, we are— [static]"), true, 40));

	Phase.DialogueTriggers.Add(MakeDialogue(120.f,
		TEXT("Viper-5 (Doc)"), TEXT("Bravo's gone. That's the third unit we've lost on the net."), false));

	// Finding the warehouse — a moment of relative safety
	Phase.DialogueTriggers.Add(MakeDialogue(180.f,
		TEXT("Viper-3 (Preacher)"), TEXT("Warehouse district ahead. Good walls. Multiple exits. "
		"We can work with this."), false));

	Phase.DialogueTriggers.Add(MakeDialogue(200.f,
		TEXT("Viper-2 (Vasquez)"), TEXT("Get inside. Check the building. Doc, set up a casualty point. "
		"Kim, find me exits and sightlines. Chen, roof — I want eyes on the street."), false));

	// Salvaging supplies
	Phase.DialogueTriggers.Add(MakeDialogue(230.f,
		TEXT("Viper-5 (Doc)"), TEXT("Found a convenience store two doors down. Water, some food. "
		"Not much, but it's something."), false));

	Phase.DialogueTriggers.Add(MakeDialogue(245.f,
		TEXT("Viper-4 (Riot)"), TEXT("[from the roof] I can see the airport from here. "
		"The whole terminal is on fire."), false));

	// The final exchange — setting up Mission 2
	Phase.DialogueTriggers.Add(MakeDialogue(270.f,
		TEXT("WATCHDOG"), TEXT("[barely audible through static] ...all surviving units... "
		"transition to... [static] ...resistance operations. Seventh Fleet ETA... "
		"seventy... [static] ...two hours. Hold your positions. Watchdog out."), true, 100));

	Phase.DialogueTriggers.Add(MakeDialogue(285.f,
		TEXT("Viper-5 (Doc)"), TEXT("Seventy-two hours?"), false));

	Phase.DialogueTriggers.Add(MakeDialogue(290.f,
		TEXT("Viper-2 (Vasquez)"), TEXT("Seventy-two hours."), false));

	Phase.DialogueTriggers.Add(MakeDialogue(295.f,
		TEXT("Viper-3 (Preacher)"), TEXT("[quietly] We can do seventy-two hours."), false));

	Phase.DialogueTriggers.Add(MakeDialogue(302.f,
		TEXT("Viper-4 (Riot)"), TEXT("[from the roof, looking at the burning airport] "
		"...we don't have a choice."), false));

	// Silence. Mission ends.

	// --- Scripted Events ---

	// Consequence walk — player passes through Phase 2 battlefield
	Phase.ScriptedEvents.Add(MakeDelayedEvent(45.f, ESHScriptedEventType::ConsequenceWalk,
		TEXT("ConsequenceWalk_Beach"), TEXT("Squad moves through aftermath of beach battle — "
		"friendly and enemy bodies, destroyed equipment, craters. World-state persistence "
		"from Phase 2 is the mechanism. Player walks through their own failure."),
		FVector(10000.f, 0.f, 0.f), 15000.f, 60.f, 1.f));

	// Airport explosion visible — smoke column on the horizon
	Phase.ScriptedEvents.Add(MakeDelayedEvent(0.f, ESHScriptedEventType::Explosion,
		TEXT("Airport_Burns"), TEXT("Taoyuan airport is on fire — massive smoke column visible from "
		"everywhere in Dayuan. The thing you were supposed to protect is gone."),
		FVector(300000.f, 100000.f, 0.f), 100000.f, 0.f, 1.f));

	// Weather: not clearing. The rain continues. No triumphant sunrise.
	Phase.ScriptedEvents.Add(MakeDelayedEvent(0.f, ESHScriptedEventType::WeatherChange,
		TEXT("Weather_Continues"), TEXT("Rain continues. No clearing. No dawn breakthrough. Just grey."),
		FVector::ZeroVector, 0.f, 0.f, 0.5f));

	// Shell shock from the accumulated stress of the entire mission
	Phase.ScriptedEvents.Add(MakeDelayedEvent(55.f, ESHScriptedEventType::ShellShock,
		TEXT("ShellShock_Aftermath"), TEXT("Walking through the bodies triggers a brief dissociative "
		"moment — audio drops, vision tunnels, time slows. 3 seconds."),
		FVector(10000.f, 0.f, 0.f), 1000.f, 3.f, 0.8f));

	Def->Phases.Add(Phase);
}
