// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

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
	Def->EstimatedPlayTime = 45.f;

	// Difficulty scaling: enemy count multiplier per tier.
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
	BuildPhase_Counterattack(Def);

	return Def;
}

// =========================================================================
//  Squad Roster
// =========================================================================

void USHMission_TaoyuanBeach::BuildSquadRoster(USHMissionDefinition* Def)
{
	// Viper-2: Cpl. James Vasquez — Automatic Rifleman, Veteran
	{
		FSHSquadMemberDef Member;
		Member.Name = FText::FromString(TEXT("Cpl. James Vasquez"));
		Member.Callsign = FText::FromString(TEXT("Viper-2"));
		Member.Rank = FText::FromString(TEXT("Corporal"));
		Member.Role = ESHSquadMemberRole::AutomaticRifleman;
		Member.PersonalityTrait = ESHPersonalityTrait::Veteran;
		Member.BackstorySnippet = FText::FromString(
			TEXT("Puerto Rican from the Bronx. Third deployment. Married with a daughter back home. "
				 "The kind of Marine who doesn't talk about what he's seen but you can read it in his eyes. "
				 "The steady hand when everything goes wrong."));
		Def->SquadRoster.Add(Member);
	}

	// Viper-3: LCpl. David Kim — Grenadier, Calm
	{
		FSHSquadMemberDef Member;
		Member.Name = FText::FromString(TEXT("LCpl. David Kim"));
		Member.Callsign = FText::FromString(TEXT("Viper-3"));
		Member.Rank = FText::FromString(TEXT("Lance Corporal"));
		Member.Role = ESHSquadMemberRole::Grenadier;
		Member.PersonalityTrait = ESHPersonalityTrait::Calm;
		Member.BackstorySnippet = FText::FromString(
			TEXT("Korean-American, devout Christian, soft-spoken. Carries a small cross in his breast pocket. "
				 "The one who prays before every engagement and never raises his voice. "
				 "Steady under fire in a way that unsettles people who don't know him."));
		Def->SquadRoster.Add(Member);
	}

	// Viper-4: PFC. Sarah Chen — Marksman, Aggressive
	{
		FSHSquadMemberDef Member;
		Member.Name = FText::FromString(TEXT("PFC. Sarah Chen"));
		Member.Callsign = FText::FromString(TEXT("Viper-4"));
		Member.Rank = FText::FromString(TEXT("Private First Class"));
		Member.Role = ESHSquadMemberRole::Marksman;
		Member.PersonalityTrait = ESHPersonalityTrait::Aggressive;
		Member.BackstorySnippet = FText::FromString(
			TEXT("Taiwanese-American. Family still in Taipei — they were supposed to evacuate yesterday. "
				 "This is personal. She volunteered for this deployment. Aggressive, precise, "
				 "and carrying a fury she channels into every shot."));
		Def->SquadRoster.Add(Member);
	}

	// Viper-5: HN. Marcus Williams — Rifleman/Medic, Cautious
	{
		FSHSquadMemberDef Member;
		Member.Name = FText::FromString(TEXT("HN. Marcus Williams"));
		Member.Callsign = FText::FromString(TEXT("Viper-5"));
		Member.Rank = FText::FromString(TEXT("Hospitalman"));
		Member.Role = ESHSquadMemberRole::Rifleman;
		Member.PersonalityTrait = ESHPersonalityTrait::Cautious;
		Member.BackstorySnippet = FText::FromString(
			TEXT("Navy Corpsman attached to the squad. First deployment. From Atlanta. "
				 "Terrified but competent — the kind of scared that makes you careful, not frozen. "
				 "Carries more medical supplies than ammo. The one who keeps everyone alive."));
		Def->SquadRoster.Add(Member);
	}
}

// =========================================================================
//  Briefing
// =========================================================================

void USHMission_TaoyuanBeach::BuildBriefing(USHMissionDefinition* Def)
{
	FSHMissionBriefing& B = Def->Briefing;

	B.MissionName = FText::FromString(TEXT("Taoyuan Beach"));
	B.MissionCallsign = FText::FromString(TEXT("Operation Breakwater"));

	B.SettingDescription = FText::FromString(
		TEXT("0430 Local Time | 15 June 2032 | Taoyuan District, Northwestern Taiwan"));

	B.BriefingNarration = FText::FromString(
		TEXT("Intelligence confirms PLA amphibious forces staging across the strait. Satellite imagery shows "
			 "landing craft forming up at Pingtan and Dongshan Island. SIGINT indicates full-spectrum electronic "
			 "warfare activation within the hour.\n\n"
			 "Your squad — callsign Viper — is tasked with defending Sector 7, the beach approach to Taoyuan "
			 "International Airport. If that airport falls in the first 24 hours, Taiwan loses its primary logistics "
			 "hub for allied reinforcement. Every hour we hold changes the calculus for the entire theater.\n\n"
			 "The PLA will hit us with everything. Amphibious infantry, light armor, drones, electronic warfare, "
			 "preparatory artillery. They have numerical superiority and air dominance. We have prepared positions, "
			 "knowledge of the terrain, and the fact that they have to come to us.\n\n"
			 "Hold the line. Buy time. That's all we need from you. That's everything."));

	B.ObjectiveSummaries.Add(FText::FromString(TEXT("Defend Beach Sector 7 against PLA amphibious assault")));
	B.ObjectiveSummaries.Add(FText::FromString(TEXT("Protect the civilian evacuation route through Dayuan")));
	B.ObjectiveSummaries.Add(FText::FromString(TEXT("Fall back to urban positions if the beach is overrun")));
	B.ObjectiveSummaries.Add(FText::FromString(TEXT("Counterattack to secure the Dayuan intersection")));

	B.IntelNotes.Add(FText::FromString(
		TEXT("PLA force estimate: 285,000+ personnel across all Taiwan landing zones. Local sector estimate: 2,000-4,000 in first wave.")));
	B.IntelNotes.Add(FText::FromString(
		TEXT("First wave expected at dawn — amphibious infantry with light armor support. Type 05 AAVs and Type 08 IFVs confirmed in staging.")));
	B.IntelNotes.Add(FText::FromString(
		TEXT("Enemy has air superiority. Expect persistent drone overflights (BZK-005, Wing Loong II) and occasional manned air strikes.")));
	B.IntelNotes.Add(FText::FromString(
		TEXT("Electronic warfare assets are active. GPS denial, communications jamming, and radar disruption expected in your AO.")));
	B.IntelNotes.Add(FText::FromString(
		TEXT("Civilian evacuation of Dayuan District is incomplete. Estimated 12,000 civilians still in the area. ROE is strict near populated zones.")));
}

// =========================================================================
//  PHASE 1: PRE-INVASION — The last quiet minutes before dawn
// =========================================================================

void USHMission_TaoyuanBeach::BuildPhase_PreInvasion(USHMissionDefinition* Def)
{
	FSHPhaseDefinition Phase;
	Phase.Phase = ESHMissionPhase::PreInvasion;
	Phase.PhaseName = FText::FromString(TEXT("Pre-Invasion"));
	Phase.PhaseDescription = FText::FromString(
		TEXT("The last quiet minutes. Radio chatter grows more urgent. Distant rumbles on the horizon. "
			 "Your squad is dug in on the beach, watching the dark water. Everyone knows what's coming. "
			 "Nobody says it."));
	Phase.Duration = 180.f;
	Phase.AmbientWeather = ESHWeatherType::Overcast;
	Phase.TimeOfDayStart = 4.5f; // 0430

	Phase.AdvanceCondition = ESHPhaseAdvanceCondition::TimerExpired;

	// --- Objective: Prepare defensive position ---
	{
		FSHPhaseObjective Obj;
		Obj.ObjectiveId = FGuid(0x00000001, 0x00000000, 0x00000000, 0x00000001);
		Obj.DisplayName = FText::FromString(TEXT("Prepare Defensive Position"));
		Obj.Description = FText::FromString(TEXT("Check weapons, verify fields of fire, and ensure the squad is ready for contact."));
		Obj.Type = ESHObjectiveType::Defend;
		Obj.Priority = ESHObjectivePriority::Primary;
		Obj.WorldLocation = FVector(0.f, 0.f, 0.f); // Beach position
		Obj.AreaRadius = 5000.f;
		Obj.DefendDuration = 120.f;
		Phase.Objectives.Add(Obj);
	}

	// --- Dialogue ---
	Phase.DialogueTriggers.Add(MakeDialogue(0.f,
		TEXT("COMMAND"), TEXT("All stations, FLASH traffic. Missile launches detected across the strait. This is not a drill. I say again, this is not a drill."), true, 100));
	Phase.DialogueTriggers.Add(MakeDialogue(15.f,
		TEXT("Viper-2 (Vasquez)"), TEXT("Here we go. Check your lanes, make sure your mags are topped off."), false));
	Phase.DialogueTriggers.Add(MakeDialogue(35.f,
		TEXT("Viper-5 (Doc)"), TEXT("...I thought we'd have more time."), false));
	Phase.DialogueTriggers.Add(MakeDialogue(45.f,
		TEXT("Viper-4 (Riot)"), TEXT("My family is in Taipei. They were supposed to evacuate yesterday."), false));
	Phase.DialogueTriggers.Add(MakeDialogue(55.f,
		TEXT("Viper-2 (Vasquez)"), TEXT("They'll get out, Chen. Focus on what's in front of you."), false));
	Phase.DialogueTriggers.Add(MakeDialogue(90.f,
		TEXT("SIGINT"), TEXT("All units, SIGINT confirms electronic warfare activation. GPS degradation expected in your AO within minutes. Switch to backup navigation."), true, 80));
	Phase.DialogueTriggers.Add(MakeDialogue(110.f,
		TEXT("Viper-5 (Doc)"), TEXT("I'm getting static on the secondary channel. They're already jamming us."), false));
	Phase.DialogueTriggers.Add(MakeDialogue(120.f,
		TEXT("Viper-3 (Preacher)"), TEXT("Lord, watch over us. Watch over all of us."), false));
	Phase.DialogueTriggers.Add(MakeDialogue(140.f,
		TEXT("Viper-4 (Riot)"), TEXT("Flashes on the horizon. That's the airfield. They're hitting the airfield."), false));
	Phase.DialogueTriggers.Add(MakeDialogue(150.f,
		TEXT("COMMAND"), TEXT("All units, first wave visual. Bearing zero-two-zero. Multiple amphibious craft. Estimated two thousand personnel in first echelon. God speed."), true, 100));
	Phase.DialogueTriggers.Add(MakeDialogue(165.f,
		TEXT("Viper-2 (Vasquez)"), TEXT("Weapons free in thirty seconds. Nobody fires until I call it."), false));
	Phase.DialogueTriggers.Add(MakeDialogue(175.f,
		TEXT("Viper-3 (Preacher)"), TEXT("I see them. I see them on the water."), false));

	// --- Scripted Events ---
	// EW jamming zone activates
	Phase.ScriptedEvents.Add(MakeDelayedEvent(60.f, ESHScriptedEventType::EWJammingZone,
		TEXT("EW_GPSDenial"), TEXT("GPS denial zone activates across the AO"),
		FVector(0.f, 0.f, 0.f), 500000.f, 0.f, 0.7f)); // 5km radius, persistent

	// Distant missile strikes on the airfield
	Phase.ScriptedEvents.Add(MakeDelayedEvent(100.f, ESHScriptedEventType::Explosion,
		TEXT("MissileStrike_Airfield"), TEXT("PLA missile strikes hit Taoyuan International Airport"),
		FVector(300000.f, 100000.f, 0.f), 50000.f, 8.f, 0.8f)); // 3km away, visible flashes

	// PLA preparatory artillery on beach defenses
	Phase.ScriptedEvents.Add(MakeDelayedEvent(170.f, ESHScriptedEventType::ArtilleryBarrage,
		TEXT("PrepFire_Beach"), TEXT("PLA preparatory artillery barrage on beach defensive positions"),
		FVector(5000.f, 0.f, 0.f), 15000.f, 10.f, 0.9f));

	Def->Phases.Add(Phase);
}

// =========================================================================
//  PHASE 2: BEACH ASSAULT — The first wave hits
// =========================================================================

void USHMission_TaoyuanBeach::BuildPhase_BeachAssault(USHMissionDefinition* Def)
{
	FSHPhaseDefinition Phase;
	Phase.Phase = ESHMissionPhase::BeachAssault;
	Phase.PhaseName = FText::FromString(TEXT("Beach Assault"));
	Phase.PhaseDescription = FText::FromString(
		TEXT("Dawn. The first amphibious craft grind onto the beach. Ramps drop. PLA infantry pour out into "
			 "your prepared kill zones. But there are so many of them. Wave after wave, supported by armor and drones. "
			 "You are one squad holding one sector of a beach that stretches for kilometers. "
			 "Hold as long as you can."));
	Phase.Duration = 0.f; // Event-driven
	Phase.AmbientWeather = ESHWeatherType::Overcast;
	Phase.TimeOfDayStart = 5.0f; // 0500

	Phase.AdvanceCondition = ESHPhaseAdvanceCondition::EnemyBreachesLine;
	Phase.AdvanceThreshold = 0.6f;

	// --- Objectives ---
	{
		FSHPhaseObjective DefendBeach;
		DefendBeach.ObjectiveId = FGuid(0x00000002, 0x00000000, 0x00000000, 0x00000001);
		DefendBeach.DisplayName = FText::FromString(TEXT("Defend Beach Sector 7"));
		DefendBeach.Description = FText::FromString(TEXT("Hold the beach defensive line against the PLA amphibious assault. Every minute you hold buys time for the airport defense."));
		DefendBeach.Type = ESHObjectiveType::Defend;
		DefendBeach.Priority = ESHObjectivePriority::Primary;
		DefendBeach.WorldLocation = FVector(0.f, 0.f, 0.f);
		DefendBeach.AreaRadius = 8000.f;
		DefendBeach.DefendDuration = 600.f;
		DefendBeach.bIsCritical = true;
		Phase.Objectives.Add(DefendBeach);
	}
	{
		FSHPhaseObjective DestroyLanding;
		DestroyLanding.ObjectiveId = FGuid(0x00000002, 0x00000000, 0x00000000, 0x00000002);
		DestroyLanding.DisplayName = FText::FromString(TEXT("Destroy Landing Craft"));
		DestroyLanding.Description = FText::FromString(TEXT("Engage PLA amphibious vehicles before they can offload troops. Use anti-armor weapons."));
		DestroyLanding.Type = ESHObjectiveType::Destroy;
		DestroyLanding.Priority = ESHObjectivePriority::Secondary;
		DestroyLanding.WorldLocation = FVector(-5000.f, 0.f, 0.f); // Waterline
		Phase.Objectives.Add(DestroyLanding);
	}
	{
		FSHPhaseObjective ProtectCivilian;
		ProtectCivilian.ObjectiveId = FGuid(0x00000002, 0x00000000, 0x00000000, 0x00000003);
		ProtectCivilian.DisplayName = FText::FromString(TEXT("Protect Civilian Evacuation Route"));
		ProtectCivilian.Description = FText::FromString(TEXT("A civilian convoy is still moving through the area. Keep the evacuation route clear of enemy forces."));
		ProtectCivilian.Type = ESHObjectiveType::Defend;
		ProtectCivilian.Priority = ESHObjectivePriority::Secondary;
		ProtectCivilian.WorldLocation = FVector(10000.f, 5000.f, 0.f);
		ProtectCivilian.AreaRadius = 5000.f;
		ProtectCivilian.DefendDuration = 300.f;
		Phase.Objectives.Add(ProtectCivilian);
	}

	// --- Wave 1: First Landing Wave (0s) ---
	{
		FSHWaveDefinition Wave;
		Wave.WaveName = FText::FromString(TEXT("First Landing Wave"));
		Wave.DelayFromPhaseStart = 0.f;
		Wave.SpawnZoneTag = FName(TEXT("Beach_North"));
		Wave.TotalInfantry = 24;
		Wave.ApproachDirection = FVector(-1.f, 0.f, 0.f); // From the sea (north)

		Wave.SquadComposition.Add(MakePLASquad(3, 1, 1, 1)); // 6-man squad
		Wave.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		Wave.SquadComposition.Add(MakePLASquad(4, 1, 0, 1));
		Wave.SquadComposition.Add(MakePLASquad(4, 1, 0, 1));
		Phase.ReinforcementWaves.Add(Wave);
	}

	// --- Wave 2: Second Wave + Armor (120s) ---
	{
		FSHWaveDefinition Wave;
		Wave.WaveName = FText::FromString(TEXT("Second Wave"));
		Wave.DelayFromPhaseStart = 120.f;
		Wave.SpawnZoneTag = FName(TEXT("Beach_NorthEast"));
		Wave.TotalInfantry = 18;
		Wave.bIncludesVehicles = true;
		Wave.VehicleCount = 2; // APCs
		Wave.ApproachDirection = FVector(-0.7f, 0.7f, 0.f);

		Wave.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		Wave.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		Wave.SquadComposition.Add(MakePLASquad(4, 1, 0, 1));
		Phase.ReinforcementWaves.Add(Wave);
	}

	// --- Wave 3: Reinforcement + Drones (300s) ---
	{
		FSHWaveDefinition Wave;
		Wave.WaveName = FText::FromString(TEXT("Third Wave — Reinforcement"));
		Wave.DelayFromPhaseStart = 300.f;
		Wave.SpawnZoneTag = FName(TEXT("Beach_North"));
		Wave.TotalInfantry = 30;
		Wave.bIncludesVehicles = true;
		Wave.VehicleCount = 1;
		Wave.ApproachDirection = FVector(-1.f, 0.2f, 0.f);

		Wave.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		Wave.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		Wave.SquadComposition.Add(MakePLASquad(4, 1, 0, 1));
		Wave.SquadComposition.Add(MakePLASquad(4, 1, 0, 1));
		Wave.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		Phase.ReinforcementWaves.Add(Wave);
	}

	// --- Wave 4: Final Push (480s) ---
	{
		FSHWaveDefinition Wave;
		Wave.WaveName = FText::FromString(TEXT("Final Push"));
		Wave.DelayFromPhaseStart = 480.f;
		Wave.SpawnZoneTag = FName(TEXT("Beach_AllSectors"));
		Wave.TotalInfantry = 36;
		Wave.bIncludesVehicles = true;
		Wave.VehicleCount = 3;
		Wave.bIncludesAirSupport = true;
		Wave.ApproachDirection = FVector(-1.f, 0.f, 0.f);

		Wave.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		Wave.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		Wave.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		Wave.SquadComposition.Add(MakePLASquad(4, 1, 0, 1));
		Wave.SquadComposition.Add(MakePLASquad(4, 1, 0, 1));
		Wave.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		Phase.ReinforcementWaves.Add(Wave);
	}

	// --- Dialogue ---
	Phase.DialogueTriggers.Add(MakeDialogue(0.f,
		TEXT("Viper-5 (Doc)"), TEXT("Contact! Multiple craft on the waterline!"), false, 90));
	Phase.DialogueTriggers.Add(MakeDialogue(5.f,
		TEXT("Viper-2 (Vasquez)"), TEXT("I count six... no, eight craft. Ramps are dropping!"), false));
	Phase.DialogueTriggers.Add(MakeDialogue(15.f,
		TEXT("COMMAND"), TEXT("Viper-1, you are weapons free. Engage at will. Make them pay for every meter."), true, 100));
	Phase.DialogueTriggers.Add(MakeDialogue(60.f,
		TEXT("Viper-4 (Riot)"), TEXT("Confirmed kills on the waterline. They're still coming."), false));
	Phase.DialogueTriggers.Add(MakeDialogue(130.f,
		TEXT("Viper-2 (Vasquez)"), TEXT("Second wave inbound! Armor on the beach — I see APCs!"), false, 80));
	Phase.DialogueTriggers.Add(MakeDialogue(180.f,
		TEXT("Viper-2 (Vasquez)"), TEXT("They just keep coming. How many are there?"), false));
	Phase.DialogueTriggers.Add(MakeDialogue(200.f,
		TEXT("Viper-5 (Doc)"), TEXT("I'm running low on bandages already..."), false));
	Phase.DialogueTriggers.Add(MakeDialogue(300.f,
		TEXT("Viper-4 (Riot)"), TEXT("RPG! Vehicle on the road! Someone get a rocket on that thing!"), false, 80));
	Phase.DialogueTriggers.Add(MakeDialogue(310.f,
		TEXT("Viper-3 (Preacher)"), TEXT("Moving to engage the vehicle. Covering fire!"), false));
	Phase.DialogueTriggers.Add(MakeDialogue(360.f,
		TEXT("COMMAND"), TEXT("Viper-1, be advised, civilian convoy still moving through Grid 4. Watch your fire. Confirm ROE."), true, 90));
	Phase.DialogueTriggers.Add(MakeDialogue(400.f,
		TEXT("Viper-4 (Riot)"), TEXT("There are people on that road! Civilians! They're running!"), false));
	Phase.DialogueTriggers.Add(MakeDialogue(450.f,
		TEXT("Viper-2 (Vasquez)"), TEXT("Third wave. This is... this is a lot of men."), false));
	Phase.DialogueTriggers.Add(MakeDialogue(500.f,
		TEXT("Viper-3 (Preacher)"), TEXT("We can't hold this much longer. We need to fall back."), false));
	Phase.DialogueTriggers.Add(MakeDialogue(550.f,
		TEXT("Viper-5 (Doc)"), TEXT("I've got wounded all over this position! We need to move them!"), false));
	Phase.DialogueTriggers.Add(MakeDialogue(580.f,
		TEXT("COMMAND"), TEXT("All units, fall back to Phase Line Bravo. I say again, all units fall back to Phase Line Bravo. Urban defensive positions."), true, 100));
	Phase.DialogueTriggers.Add(MakeDialogue(590.f,
		TEXT("Viper-2 (Vasquez)"), TEXT("You heard them. Grab the wounded, leapfrog back. I'll cover!"), false));

	// --- Scripted Events ---
	Phase.ScriptedEvents.Add(MakeDelayedEvent(60.f, ESHScriptedEventType::DroneSwarm,
		TEXT("ISR_Overwatch"), TEXT("PLA ISR drones establish overwatch above the beach"),
		FVector(0.f, 0.f, 30000.f), 20000.f, 0.f, 0.5f));

	Phase.ScriptedEvents.Add(MakeDelayedEvent(240.f, ESHScriptedEventType::CounterBattery,
		TEXT("CounterBattery_Player"), TEXT("PLA counter-battery fire targets the player's position"),
		FVector(0.f, 0.f, 0.f), 8000.f, 15.f, 0.8f));

	Phase.ScriptedEvents.Add(MakeDelayedEvent(420.f, ESHScriptedEventType::AirStrike,
		TEXT("AirStrike_Beach"), TEXT("PLA jets make a strafing run along the beach defensive line"),
		FVector(0.f, -3000.f, 0.f), 12000.f, 8.f, 0.9f));

	Phase.ScriptedEvents.Add(MakeDelayedEvent(500.f, ESHScriptedEventType::WeatherChange,
		TEXT("Weather_Rain"), TEXT("Weather deteriorates — light rain begins"),
		FVector::ZeroVector, 0.f, 60.f, 0.4f));

	Def->Phases.Add(Phase);
}

// =========================================================================
//  PHASE 3: URBAN FALLBACK — The beach is lost. Fight through the streets.
// =========================================================================

void USHMission_TaoyuanBeach::BuildPhase_UrbanFallback(USHMissionDefinition* Def)
{
	FSHPhaseDefinition Phase;
	Phase.Phase = ESHMissionPhase::UrbanFallback;
	Phase.PhaseName = FText::FromString(TEXT("Urban Fallback"));
	Phase.PhaseDescription = FText::FromString(
		TEXT("The beach is lost. Your squad fights through the streets of Dayuan, building to building, "
			 "under rain and smoke. Civilians are still here — families who didn't evacuate in time. "
			 "The enemy is behind you, beside you, and pushing hard. Every block you hold is a block "
			 "the airport doesn't lose."));
	Phase.Duration = 0.f;
	Phase.AmbientWeather = ESHWeatherType::LightRain;
	Phase.TimeOfDayStart = 6.5f;

	Phase.AdvanceCondition = ESHPhaseAdvanceCondition::AllObjectivesComplete;

	// --- Objectives ---
	{
		FSHPhaseObjective RallyPoint;
		RallyPoint.ObjectiveId = FGuid(0x00000003, 0x00000000, 0x00000000, 0x00000001);
		RallyPoint.DisplayName = FText::FromString(TEXT("Reach Rally Point Alpha"));
		RallyPoint.Description = FText::FromString(TEXT("Move through Dayuan to the school building on Route 4. Friendly forces are consolidating there."));
		RallyPoint.Type = ESHObjectiveType::Extract;
		RallyPoint.Priority = ESHObjectivePriority::Primary;
		RallyPoint.WorldLocation = FVector(40000.f, 10000.f, 0.f);
		RallyPoint.AreaRadius = 3000.f;
		RallyPoint.bIsCritical = true;
		Phase.Objectives.Add(RallyPoint);
	}
	{
		FSHPhaseObjective CommsRelay;
		CommsRelay.ObjectiveId = FGuid(0x00000003, 0x00000000, 0x00000000, 0x00000002);
		CommsRelay.DisplayName = FText::FromString(TEXT("Neutralize Enemy Comms Relay"));
		CommsRelay.Description = FText::FromString(TEXT("A PLA forward comms relay is coordinating pursuit forces. Destroy it to buy breathing room."));
		CommsRelay.Type = ESHObjectiveType::Neutralize;
		CommsRelay.Priority = ESHObjectivePriority::Secondary;
		CommsRelay.WorldLocation = FVector(25000.f, -5000.f, 0.f);
		Phase.Objectives.Add(CommsRelay);
	}
	{
		FSHPhaseObjective RescueMarine;
		RescueMarine.ObjectiveId = FGuid(0x00000003, 0x00000000, 0x00000000, 0x00000003);
		RescueMarine.DisplayName = FText::FromString(TEXT("Rescue Wounded Marine"));
		RescueMarine.Description = FText::FromString(TEXT("A wounded Marine from 2nd Platoon is trapped in a building nearby. Get to him before he bleeds out."));
		RescueMarine.Type = ESHObjectiveType::Extract;
		RescueMarine.Priority = ESHObjectivePriority::Secondary;
		RescueMarine.WorldLocation = FVector(20000.f, 8000.f, 0.f);
		RescueMarine.AreaRadius = 1500.f;
		RescueMarine.TimeLimit = 180.f;
		Phase.Objectives.Add(RescueMarine);
	}

	// --- Waves ---
	{
		FSHWaveDefinition W;
		W.WaveName = FText::FromString(TEXT("Pursuit Force"));
		W.DelayFromPhaseStart = 0.f;
		W.SpawnZoneTag = FName(TEXT("Urban_Beach_Side"));
		W.TotalInfantry = 20;
		W.ApproachDirection = FVector(-1.f, 0.f, 0.f);
		W.SquadComposition.Add(MakePLASquad(4, 1, 0, 1));
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		W.SquadComposition.Add(MakePLASquad(4, 0, 1, 1));
		Phase.ReinforcementWaves.Add(W);
	}
	{
		FSHWaveDefinition W;
		W.WaveName = FText::FromString(TEXT("Flanking Element"));
		W.DelayFromPhaseStart = 180.f;
		W.SpawnZoneTag = FName(TEXT("Urban_East"));
		W.TotalInfantry = 12;
		W.ApproachDirection = FVector(0.f, 1.f, 0.f);
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		Phase.ReinforcementWaves.Add(W);
	}
	{
		FSHWaveDefinition W;
		W.WaveName = FText::FromString(TEXT("Armor Probe"));
		W.DelayFromPhaseStart = 360.f;
		W.SpawnZoneTag = FName(TEXT("Urban_MainRoad"));
		W.TotalInfantry = 8;
		W.bIncludesVehicles = true;
		W.VehicleCount = 2;
		W.ApproachDirection = FVector(-0.5f, 0.5f, 0.f);
		W.SquadComposition.Add(MakePLASquad(4, 1, 1, 1));
		Phase.ReinforcementWaves.Add(W);
	}

	// --- Dialogue ---
	Phase.DialogueTriggers.Add(MakeDialogue(0.f,    TEXT("COMMAND"), TEXT("Viper-1, rally point Alpha is the school building on Route 4. Two klicks inland. Move now."), true, 100));
	Phase.DialogueTriggers.Add(MakeDialogue(10.f,   TEXT("Viper-2 (Vasquez)"), TEXT("Covering! Move move move! Stay off the main road!"), false, 80));
	Phase.DialogueTriggers.Add(MakeDialogue(45.f,   TEXT("Viper-5 (Doc)"), TEXT("There are people in these buildings. I can hear them crying."), false));
	Phase.DialogueTriggers.Add(MakeDialogue(80.f,   TEXT("Viper-4 (Riot)"), TEXT("Families. Kids. They didn't get out in time."), false));
	Phase.DialogueTriggers.Add(MakeDialogue(90.f,   TEXT("Viper-3 (Preacher)"), TEXT("We can't help them if we're dead. Keep moving, Chen."), false));
	Phase.DialogueTriggers.Add(MakeDialogue(120.f,  TEXT("COMMAND"), TEXT("Viper-1, SIGINT has a PLA comms relay at Grid Reference 3-7. Take it out if you can."), true));
	Phase.DialogueTriggers.Add(MakeDialogue(180.f,  TEXT("Viper-5 (Doc)"), TEXT("I've got a wounded Marine in that building! 2nd Platoon markings. We can't leave him!"), false, 90));
	Phase.DialogueTriggers.Add(MakeDialogue(200.f,  TEXT("Viper-2 (Vasquez)"), TEXT("Go get him, Doc. We'll cover. You've got three minutes."), false));
	Phase.DialogueTriggers.Add(MakeDialogue(300.f,  TEXT("Viper-3 (Preacher)"), TEXT("Contact rear! They're behind us! Two squads in the alley!"), false, 80));
	Phase.DialogueTriggers.Add(MakeDialogue(350.f,  TEXT("Viper-4 (Riot)"), TEXT("I can see the school! Two hundred meters! Push through!"), false));

	// --- Scripted Events ---
	Phase.ScriptedEvents.Add(MakeDelayedEvent(0.f,   ESHScriptedEventType::WeatherChange, TEXT("Weather_SteadyRain"), TEXT("Rain intensifies"), FVector::ZeroVector, 0.f, 120.f, 0.6f));
	Phase.ScriptedEvents.Add(MakeDelayedEvent(90.f,  ESHScriptedEventType::EWJammingZone, TEXT("EW_CommsDegrade"), TEXT("EW zone expands — comms unreliable"), FVector(20000.f, 0.f, 0.f), 600000.f, 0.f, 0.8f));
	Phase.ScriptedEvents.Add(MakeDelayedEvent(240.f, ESHScriptedEventType::Explosion, TEXT("BuildingCollapse"), TEXT("Artillery collapses a building near the player"), FVector(22000.f, 3000.f, 0.f), 3000.f, 3.f, 0.9f));

	Def->Phases.Add(Phase);
}

// =========================================================================
//  PHASE 4: COUNTERATTACK — The first moment of hope
// =========================================================================

void USHMission_TaoyuanBeach::BuildPhase_Counterattack(USHMissionDefinition* Def)
{
	FSHPhaseDefinition Phase;
	Phase.Phase = ESHMissionPhase::Counterattack;
	Phase.PhaseName = FText::FromString(TEXT("Counterattack"));
	Phase.PhaseDescription = FText::FromString(
		TEXT("For the first time since dawn, you are moving forward. A limited counterattack to retake "
			 "the Dayuan intersection — the last road to the airport. If you hold it, armor arrives. "
			 "If armor arrives, the airport stands. If the airport stands, everything that happened "
			 "today meant something."));
	Phase.Duration = 0.f;
	Phase.AmbientWeather = ESHWeatherType::LightRain;
	Phase.TimeOfDayStart = 7.5f;

	Phase.AdvanceCondition = ESHPhaseAdvanceCondition::AllObjectivesComplete;

	// --- Objectives ---
	{
		FSHPhaseObjective Obj;
		Obj.ObjectiveId = FGuid(0x00000004, 0x00000000, 0x00000000, 0x00000001);
		Obj.DisplayName = FText::FromString(TEXT("Secure Dayuan Intersection"));
		Obj.Description = FText::FromString(TEXT("Assault and capture the main intersection. This controls the last open road to the airport."));
		Obj.Type = ESHObjectiveType::Capture;
		Obj.Priority = ESHObjectivePriority::Primary;
		Obj.WorldLocation = FVector(60000.f, 15000.f, 0.f);
		Obj.AreaRadius = 6000.f;
		Obj.bIsCritical = true;
		Phase.Objectives.Add(Obj);
	}
	{
		FSHPhaseObjective Obj;
		Obj.ObjectiveId = FGuid(0x00000004, 0x00000000, 0x00000000, 0x00000002);
		Obj.DisplayName = FText::FromString(TEXT("Hold Until Relieved"));
		Obj.Description = FText::FromString(TEXT("Hold the intersection until friendly armor arrives. Five minutes. Do not let it fall."));
		Obj.Type = ESHObjectiveType::Defend;
		Obj.Priority = ESHObjectivePriority::Primary;
		Obj.WorldLocation = FVector(60000.f, 15000.f, 0.f);
		Obj.AreaRadius = 6000.f;
		Obj.DefendDuration = 300.f;
		Obj.bIsCritical = true;
		Phase.Objectives.Add(Obj);
	}
	{
		FSHPhaseObjective Obj;
		Obj.ObjectiveId = FGuid(0x00000004, 0x00000000, 0x00000000, 0x00000003);
		Obj.DisplayName = FText::FromString(TEXT("Call Fire Mission on Enemy Staging Area"));
		Obj.Description = FText::FromString(TEXT("Enemy staging area identified 800m north. Call in artillery to disrupt reinforcement."));
		Obj.Type = ESHObjectiveType::Destroy;
		Obj.Priority = ESHObjectivePriority::Secondary;
		Obj.WorldLocation = FVector(60000.f, 95000.f, 0.f);
		Phase.Objectives.Add(Obj);
	}

	// --- Waves ---
	{
		FSHWaveDefinition W;
		W.WaveName = FText::FromString(TEXT("Intersection Defenders"));
		W.DelayFromPhaseStart = 0.f;
		W.SpawnZoneTag = FName(TEXT("Intersection_Center"));
		W.TotalInfantry = 24;
		W.ApproachDirection = FVector(0.f, 1.f, 0.f);
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		Phase.ReinforcementWaves.Add(W);
	}
	{
		FSHWaveDefinition W;
		W.WaveName = FText::FromString(TEXT("Enemy QRF"));
		W.DelayFromPhaseStart = 180.f;
		W.SpawnZoneTag = FName(TEXT("Intersection_North"));
		W.TotalInfantry = 18;
		W.bIncludesVehicles = true;
		W.VehicleCount = 2;
		W.ApproachDirection = FVector(0.f, 1.f, 0.f);
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		Phase.ReinforcementWaves.Add(W);
	}
	{
		FSHWaveDefinition W;
		W.WaveName = FText::FromString(TEXT("Final Counterattack Wave"));
		W.DelayFromPhaseStart = 300.f;
		W.SpawnZoneTag = FName(TEXT("Intersection_AllSides"));
		W.TotalInfantry = 30;
		W.ApproachDirection = FVector(-0.5f, 0.5f, 0.f);
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		W.SquadComposition.Add(MakePLASquad(3, 1, 1, 1));
		W.SquadComposition.Add(MakePLASquad(4, 1, 0, 1));
		W.SquadComposition.Add(MakePLASquad(4, 1, 0, 1));
		Phase.ReinforcementWaves.Add(W);
	}

	// --- Dialogue ---
	Phase.DialogueTriggers.Add(MakeDialogue(0.f,    TEXT("COMMAND"), TEXT("Viper-1, command has authorized a counterattack on the Dayuan intersection. You lead the assault. Friendly armor en route — ETA five mikes."), true, 100));
	Phase.DialogueTriggers.Add(MakeDialogue(15.f,   TEXT("Viper-2 (Vasquez)"), TEXT("First time today I'm going forward instead of back. Let's make it count."), false));
	Phase.DialogueTriggers.Add(MakeDialogue(30.f,   TEXT("Viper-4 (Riot)"), TEXT("We hold this intersection, we hold the road to the airport. This matters."), false));
	Phase.DialogueTriggers.Add(MakeDialogue(60.f,   TEXT("Viper-3 (Preacher)"), TEXT("Intersection ahead. Multiple hostiles in defensive positions. They've had time to dig in."), false));
	Phase.DialogueTriggers.Add(MakeDialogue(90.f,   TEXT("Viper-2 (Vasquez)"), TEXT("Smoke out, then push. Preacher, put a grenade on that MG position."), false));
	Phase.DialogueTriggers.Add(MakeDialogue(150.f,  TEXT("COMMAND"), TEXT("Viper-1, artillery support is now available. Fire mission authorized on the enemy staging area to your north."), true, 90));
	Phase.DialogueTriggers.Add(MakeDialogue(180.f,  TEXT("COMMAND"), TEXT("Viper-1, enemy QRF inbound from the north. Heavy presence. Two vehicles. Hold your ground."), true, 90));
	Phase.DialogueTriggers.Add(MakeDialogue(200.f,  TEXT("Viper-5 (Doc)"), TEXT("There's a lot of them coming. A LOT of them."), false));
	Phase.DialogueTriggers.Add(MakeDialogue(240.f,  TEXT("Viper-4 (Riot)"), TEXT("We didn't fight through all of that to lose it here. Not here."), false));
	Phase.DialogueTriggers.Add(MakeDialogue(260.f,  TEXT("Viper-3 (Preacher)"), TEXT("Steady. Steady. Pick your targets."), false));
	Phase.DialogueTriggers.Add(MakeDialogue(280.f,  TEXT("Viper-2 (Vasquez)"), TEXT("Armor is one mike out! Just hold! One more minute!"), false, 80));
	Phase.DialogueTriggers.Add(MakeDialogue(300.f,  TEXT("COMMAND"), TEXT("Viper-1, friendly armor has arrived. M1A2s are engaging. Intersection secured. Outstanding work, Viper."), true, 100));
	Phase.DialogueTriggers.Add(MakeDialogue(310.f,  TEXT("COMMAND"), TEXT("Shattered Horizon actual sends: the airport stands because of you. Operation Breakwater is a success."), true, 100));
	Phase.DialogueTriggers.Add(MakeDialogue(320.f,  TEXT("Viper-2 (Vasquez)"), TEXT("...did we actually just do that?"), false));
	Phase.DialogueTriggers.Add(MakeDialogue(328.f,  TEXT("Viper-5 (Doc)"), TEXT("We did. But this isn't over, is it?"), false));
	Phase.DialogueTriggers.Add(MakeDialogue(335.f,  TEXT("Viper-4 (Riot)"), TEXT("No. This is just the beginning."), false));

	// --- Scripted Events ---
	Phase.ScriptedEvents.Add(MakeDelayedEvent(0.f,   ESHScriptedEventType::WeatherChange, TEXT("Weather_Clearing"), TEXT("Dawn breaks through — weather clears"), FVector::ZeroVector, 0.f, 300.f, 0.2f));
	Phase.ScriptedEvents.Add(MakeDelayedEvent(150.f, ESHScriptedEventType::Reinforcement, TEXT("ArtillerySupport"), TEXT("Artillery fire support authorized"), FVector::ZeroVector, 0.f, 0.f, 1.f));
	Phase.ScriptedEvents.Add(MakeDelayedEvent(240.f, ESHScriptedEventType::DroneSwarm, TEXT("FriendlyISR"), TEXT("Friendly ISR drone provides overwatch"), FVector(60000.f, 15000.f, 25000.f), 15000.f, 0.f, 0.3f));

	Def->Phases.Add(Phase);
}
