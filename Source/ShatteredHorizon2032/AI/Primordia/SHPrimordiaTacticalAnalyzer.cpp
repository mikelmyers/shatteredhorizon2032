// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "AI/Primordia/SHPrimordiaTacticalAnalyzer.h"
#include "NavigationSystem.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogPrimordiaAnalyzer, Log, All);

// -----------------------------------------------------------------------
USHPrimordiaTacticalAnalyzer::USHPrimordiaTacticalAnalyzer()
{
}

void USHPrimordiaTacticalAnalyzer::Initialize(UWorld* InWorld)
{
	WorldRef = InWorld;
	CumulativeCasualties = FSHCasualtySummary();
	EngagementHistory.Empty();
	PlayerMovementHistory.Empty();
	SectorAnalyses.Empty();
	DetectedTactics.Empty();

	UE_LOG(LogPrimordiaAnalyzer, Log, TEXT("Tactical Analyzer initialized."));
}

void USHPrimordiaTacticalAnalyzer::Tick(float DeltaSeconds)
{
	PruneMovementHistory();

	// Periodic terrain analysis.
	TerrainAnalysisAccumulator += DeltaSeconds;
	if (TerrainAnalysisAccumulator >= TerrainAnalysisInterval)
	{
		TerrainAnalysisAccumulator = 0.f;
		PerformTerrainAnalysis();
	}

	// Periodic tactic analysis (every 15 seconds).
	TacticAnalysisAccumulator += DeltaSeconds;
	if (TacticAnalysisAccumulator >= 15.f)
	{
		TacticAnalysisAccumulator = 0.f;
		AnalyzePlayerTactics();
		UpdateSectorAnalyses();
	}
}

// -----------------------------------------------------------------------
//  Data ingestion
// -----------------------------------------------------------------------

void USHPrimordiaTacticalAnalyzer::ReportPlayerPosition(int32 PlayerId, FVector Position, FVector Velocity)
{
	FSHMovementSample Sample;
	Sample.Timestamp = FPlatformTime::Seconds();
	Sample.Position = Position;
	Sample.Velocity = Velocity;

	TArray<FSHMovementSample>& History = PlayerMovementHistory.FindOrAdd(PlayerId);
	History.Add(Sample);
}

void USHPrimordiaTacticalAnalyzer::ReportCasualty(bool bIsPLA, bool bKilled, FVector Location)
{
	if (bIsPLA)
	{
		if (bKilled) CumulativeCasualties.PLAKilled++;
		else CumulativeCasualties.PLAWounded++;
	}
	else
	{
		if (bKilled) CumulativeCasualties.DefenderKilled++;
		else CumulativeCasualties.DefenderWounded++;
	}

	// Attribute to nearest sector.
	float BestDist = TNumericLimits<float>::Max();
	FName BestSector;
	for (const auto& Pair : SectorAnalyses)
	{
		const float Dist = FVector::Dist(Location, Pair.Value.Center);
		if (Dist < BestDist)
		{
			BestDist = Dist;
			BestSector = Pair.Key;
		}
	}

	UE_LOG(LogPrimordiaAnalyzer, Verbose, TEXT("Casualty reported: PLA=%d killed=%d at %s (sector %s)"),
		bIsPLA, bKilled, *Location.ToString(), *BestSector.ToString());
}

void USHPrimordiaTacticalAnalyzer::ReportSurrender(FVector Location)
{
	CumulativeCasualties.PLASurrendered++;
	UE_LOG(LogPrimordiaAnalyzer, Log, TEXT("PLA surrender reported at %s"), *Location.ToString());
}

void USHPrimordiaTacticalAnalyzer::ReportEngagement(const FSHEngagementRecord& Record)
{
	EngagementHistory.Add(Record);

	// Update sector engagement stats.
	float BestDist = TNumericLimits<float>::Max();
	FName BestSector;
	for (const auto& Pair : SectorAnalyses)
	{
		const float Dist = FVector::Dist(Record.Location, Pair.Value.Center);
		if (Dist < BestDist)
		{
			BestDist = Dist;
			BestSector = Pair.Key;
		}
	}

	if (FSHSectorAnalysis* Sector = SectorAnalyses.Find(BestSector))
	{
		Sector->EngagementCount++;
		// Running success rate.
		const float OldTotal = static_cast<float>(Sector->EngagementCount - 1);
		Sector->AttackSuccessRate = (Sector->AttackSuccessRate * OldTotal + (Record.bSuccessful ? 1.f : 0.f)) / Sector->EngagementCount;
	}
}

void USHPrimordiaTacticalAnalyzer::RegisterSector(FName SectorTag, FVector Center, float Radius)
{
	FSHSectorAnalysis& Sector = SectorAnalyses.FindOrAdd(SectorTag);
	Sector.SectorTag = SectorTag;
	Sector.Center = Center;
	SectorRadii.Add(SectorTag, Radius);

	UE_LOG(LogPrimordiaAnalyzer, Log, TEXT("Registered sector '%s' at %s radius %.0f"), *SectorTag.ToString(), *Center.ToString(), Radius);
}

void USHPrimordiaTacticalAnalyzer::ReportHeavyWeapon(FName SectorTag, FVector Position)
{
	if (FSHSectorAnalysis* Sector = SectorAnalyses.Find(SectorTag))
	{
		Sector->HeavyWeaponCount++;
	}
}

// -----------------------------------------------------------------------
//  State snapshot
// -----------------------------------------------------------------------

FString USHPrimordiaTacticalAnalyzer::BuildBattlefieldStateJson() const
{
	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("timestamp"), FPlatformTime::Seconds());

	// --- Casualties ---
	{
		TSharedPtr<FJsonObject> Cas = MakeShared<FJsonObject>();
		Cas->SetNumberField(TEXT("pla_killed"), CumulativeCasualties.PLAKilled);
		Cas->SetNumberField(TEXT("pla_wounded"), CumulativeCasualties.PLAWounded);
		Cas->SetNumberField(TEXT("pla_surrendered"), CumulativeCasualties.PLASurrendered);
		Cas->SetNumberField(TEXT("defender_killed"), CumulativeCasualties.DefenderKilled);
		Cas->SetNumberField(TEXT("defender_wounded"), CumulativeCasualties.DefenderWounded);
		Root->SetObjectField(TEXT("casualties"), Cas);
	}

	// --- Sectors ---
	{
		TArray<TSharedPtr<FJsonValue>> SectorArray;
		for (const auto& Pair : SectorAnalyses)
		{
			SectorArray.Add(MakeShared<FJsonValueObject>(SectorToJson(Pair.Value)));
		}
		Root->SetArrayField(TEXT("sectors"), SectorArray);
	}

	// --- Player positions (latest) ---
	{
		TArray<TSharedPtr<FJsonValue>> PlayersArray;
		for (const auto& Pair : PlayerMovementHistory)
		{
			if (Pair.Value.Num() == 0) continue;

			const FSHMovementSample& Latest = Pair.Value.Last();
			TSharedPtr<FJsonObject> P = MakeShared<FJsonObject>();
			P->SetNumberField(TEXT("id"), Pair.Key);
			P->SetNumberField(TEXT("x"), Latest.Position.X);
			P->SetNumberField(TEXT("y"), Latest.Position.Y);
			P->SetNumberField(TEXT("z"), Latest.Position.Z);
			P->SetNumberField(TEXT("vx"), Latest.Velocity.X);
			P->SetNumberField(TEXT("vy"), Latest.Velocity.Y);
			P->SetNumberField(TEXT("vz"), Latest.Velocity.Z);
			PlayersArray.Add(MakeShared<FJsonValueObject>(P));
		}
		Root->SetArrayField(TEXT("players"), PlayersArray);
	}

	// --- Detected tactics ---
	{
		TArray<TSharedPtr<FJsonValue>> TacticsArray;
		for (const FSHDetectedTactic& Tactic : DetectedTactics)
		{
			TSharedPtr<FJsonObject> T = MakeShared<FJsonObject>();
			T->SetStringField(TEXT("type"), Tactic.TacticType);
			T->SetNumberField(TEXT("confidence"), Tactic.Confidence);
			T->SetStringField(TEXT("counter_suggestion"), Tactic.SuggestedCounter);

			TArray<TSharedPtr<FJsonValue>> SectorNames;
			for (const FName& S : Tactic.AffectedSectors)
			{
				SectorNames.Add(MakeShared<FJsonValueString>(S.ToString()));
			}
			T->SetArrayField(TEXT("sectors"), SectorNames);
			TacticsArray.Add(MakeShared<FJsonValueObject>(T));
		}
		Root->SetArrayField(TEXT("detected_tactics"), TacticsArray);
	}

	// --- Recent engagements (last 20) ---
	{
		TArray<TSharedPtr<FJsonValue>> EngArray;
		const int32 Start = FMath::Max(0, EngagementHistory.Num() - 20);
		for (int32 i = Start; i < EngagementHistory.Num(); ++i)
		{
			const FSHEngagementRecord& E = EngagementHistory[i];
			TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
			Obj->SetNumberField(TEXT("time"), E.Timestamp);
			Obj->SetNumberField(TEXT("x"), E.Location.X);
			Obj->SetNumberField(TEXT("y"), E.Location.Y);
			Obj->SetNumberField(TEXT("friendly_cas"), E.FriendlyCasualties);
			Obj->SetNumberField(TEXT("hostile_cas"), E.HostileCasualties);
			Obj->SetBoolField(TEXT("success"), E.bSuccessful);
			EngArray.Add(MakeShared<FJsonValueObject>(Obj));
		}
		Root->SetArrayField(TEXT("recent_engagements"), EngArray);
	}

	FString Output;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);
	return Output;
}

// -----------------------------------------------------------------------
//  Queries
// -----------------------------------------------------------------------

FSHSectorAnalysis USHPrimordiaTacticalAnalyzer::GetSectorAnalysis(FName SectorTag) const
{
	const FSHSectorAnalysis* Found = SectorAnalyses.Find(SectorTag);
	return Found ? *Found : FSHSectorAnalysis();
}

TArray<FSHSectorAnalysis> USHPrimordiaTacticalAnalyzer::GetAllSectorAnalyses() const
{
	TArray<FSHSectorAnalysis> Result;
	SectorAnalyses.GenerateValueArray(Result);
	return Result;
}

TArray<FSHDetectedTactic> USHPrimordiaTacticalAnalyzer::GetDetectedTactics() const
{
	return DetectedTactics;
}

FSHAfterActionReport USHPrimordiaTacticalAnalyzer::GenerateAAR(double StartTime, double EndTime) const
{
	FSHAfterActionReport AAR;
	AAR.StartTime = StartTime;
	AAR.EndTime = EndTime;
	AAR.Casualties = CumulativeCasualties; // Snapshot — ideally would be windowed.

	for (const FSHEngagementRecord& E : EngagementHistory)
	{
		if (E.Timestamp >= StartTime && E.Timestamp <= EndTime)
		{
			if (E.bSuccessful) AAR.DirectivesSucceeded++;
			else AAR.DirectivesFailed++;
		}
	}

	// Identify contested sectors (any with engagements in window).
	TSet<FName> Contested;
	for (const FSHEngagementRecord& E : EngagementHistory)
	{
		if (E.Timestamp < StartTime || E.Timestamp > EndTime) continue;

		for (const auto& Pair : SectorAnalyses)
		{
			const float Radius = SectorRadii.Contains(Pair.Key) ? SectorRadii[Pair.Key] : SectorQueryRadius;
			if (FVector::Dist(E.Location, Pair.Value.Center) < Radius)
			{
				Contested.Add(Pair.Key);
			}
		}
	}
	AAR.ContestedSectors = Contested.Array();

	// Build observations.
	if (CumulativeCasualties.GetPLAKDRatio() < 0.5f)
	{
		AAR.Observations.Add(TEXT("PLA suffering disproportionate casualties — consider adjusting approach vectors."));
	}
	if (DetectedTactics.Num() > 0)
	{
		for (const FSHDetectedTactic& T : DetectedTactics)
		{
			if (T.Confidence > 0.7f)
			{
				AAR.Observations.Add(FString::Printf(TEXT("High-confidence tactic detected: %s — counter: %s"),
					*T.TacticType, *T.SuggestedCounter));
			}
		}
	}

	return AAR;
}

// -----------------------------------------------------------------------
//  Terrain analysis
// -----------------------------------------------------------------------

void USHPrimordiaTacticalAnalyzer::PerformTerrainAnalysis()
{
	UWorld* World = WorldRef.Get();
	if (!World)
	{
		return;
	}

	for (auto& Pair : SectorAnalyses)
	{
		const float Radius = SectorRadii.Contains(Pair.Key) ? SectorRadii[Pair.Key] : SectorQueryRadius;
		Pair.Value.CoverDensity = EstimateCoverDensity(Pair.Value.Center, Radius);
		Pair.Value.ExposureScore = EstimateExposure(Pair.Value.Center, Radius);
		Pair.Value.ApproachRouteCount = CountApproachRoutes(Pair.Value.Center, Radius);
	}
}

float USHPrimordiaTacticalAnalyzer::EstimateCoverDensity(FVector Center, float Radius) const
{
	UWorld* World = WorldRef.Get();
	if (!World) return 0.5f;

	// Cast rays from multiple directions towards the center at soldier height.
	// Proportion of rays that hit geometry = cover density.
	int32 HitCount = 0;
	constexpr int32 NumRays = 16;
	constexpr float SoldierHeight = 170.f;

	FCollisionQueryParams Params;
	Params.bTraceComplex = false;

	for (int32 i = 0; i < NumRays; ++i)
	{
		const float Angle = (2.f * PI * i) / NumRays;
		const FVector Start = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius + FVector(0, 0, SoldierHeight);
		const FVector End = Center + FVector(0, 0, SoldierHeight);

		FHitResult Hit;
		if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
		{
			HitCount++;
		}
	}

	return static_cast<float>(HitCount) / NumRays;
}

float USHPrimordiaTacticalAnalyzer::EstimateExposure(FVector Center, float Radius) const
{
	UWorld* World = WorldRef.Get();
	if (!World) return 0.5f;

	// Trace from center outward — proportion of unobstructed rays = exposure.
	int32 ClearCount = 0;
	constexpr int32 NumRays = 16;
	constexpr float TraceHeight = 170.f;

	FCollisionQueryParams Params;
	Params.bTraceComplex = false;

	for (int32 i = 0; i < NumRays; ++i)
	{
		const float Angle = (2.f * PI * i) / NumRays;
		const FVector Start = Center + FVector(0, 0, TraceHeight);
		const FVector End = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius + FVector(0, 0, TraceHeight);

		FHitResult Hit;
		if (!World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
		{
			ClearCount++;
		}
	}

	return static_cast<float>(ClearCount) / NumRays;
}

int32 USHPrimordiaTacticalAnalyzer::CountApproachRoutes(FVector Center, float Radius) const
{
	UWorld* World = WorldRef.Get();
	if (!World) return 1;

	UNavigationSystemV1* NavSys = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
	if (!NavSys) return 1;

	// Sample points around the perimeter and check nav reachability.
	int32 ReachableCount = 0;
	constexpr int32 NumSamples = 8;

	for (int32 i = 0; i < NumSamples; ++i)
	{
		const float Angle = (2.f * PI * i) / NumSamples;
		const FVector TestPoint = Center + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.f) * Radius;

		FNavLocation NavLoc;
		if (NavSys->ProjectPointToNavigation(TestPoint, NavLoc, FVector(500.f, 500.f, 500.f)))
		{
			ReachableCount++;
		}
	}

	return FMath::Max(1, ReachableCount);
}

// -----------------------------------------------------------------------
//  Internal — Tactic analysis
// -----------------------------------------------------------------------

void USHPrimordiaTacticalAnalyzer::AnalyzePlayerTactics()
{
	DetectedTactics.Empty();

	const double Now = FPlatformTime::Seconds();
	const double WindowStart = Now - MovementHistoryWindow;

	// Analyze movement patterns across all tracked players.
	float TotalMovement = 0.f;
	int32 StationaryCount = 0;
	int32 TrackedPlayers = 0;

	TMap<FName, int32> SectorPresence; // How many players are in each sector.

	for (const auto& Pair : PlayerMovementHistory)
	{
		const TArray<FSHMovementSample>& Samples = Pair.Value;
		if (Samples.Num() < 2) continue;

		TrackedPlayers++;

		// Compute total distance moved in window.
		float DistMoved = 0.f;
		for (int32 i = 1; i < Samples.Num(); ++i)
		{
			if (Samples[i].Timestamp < WindowStart) continue;
			DistMoved += FVector::Dist(Samples[i].Position, Samples[i - 1].Position);
		}

		TotalMovement += DistMoved;

		// Low movement = stationary / static defense.
		if (DistMoved < 500.f) // Less than 5m in the window.
		{
			StationaryCount++;
		}

		// Determine which sector the latest position is in.
		const FVector& LatestPos = Samples.Last().Position;
		for (const auto& SectorPair : SectorAnalyses)
		{
			const float Radius = SectorRadii.Contains(SectorPair.Key) ? SectorRadii[SectorPair.Key] : SectorQueryRadius;
			if (FVector::Dist(LatestPos, SectorPair.Value.Center) < Radius)
			{
				SectorPresence.FindOrAdd(SectorPair.Key)++;
			}
		}
	}

	if (TrackedPlayers == 0) return;

	const float StationaryRatio = static_cast<float>(StationaryCount) / TrackedPlayers;

	// --- Static Defense detection ---
	if (StationaryRatio > 0.6f)
	{
		FSHDetectedTactic StaticDef;
		StaticDef.TacticType = TEXT("static_defense");
		StaticDef.Confidence = FMath::Clamp(StationaryRatio, 0.f, 1.f);
		StaticDef.SuggestedCounter = TEXT("flanking_maneuver");

		for (const auto& SP : SectorPresence)
		{
			if (SP.Value > 0) StaticDef.AffectedSectors.Add(SP.Key);
		}
		DetectedTactics.Add(MoveTemp(StaticDef));
	}

	// --- Roaming Patrol detection ---
	if (StationaryRatio < 0.3f && TotalMovement / TrackedPlayers > 2000.f)
	{
		FSHDetectedTactic Roaming;
		Roaming.TacticType = TEXT("roaming_patrol");
		Roaming.Confidence = FMath::Clamp(1.f - StationaryRatio, 0.f, 1.f);
		Roaming.SuggestedCounter = TEXT("ambush_preparation");
		DetectedTactics.Add(MoveTemp(Roaming));
	}

	// --- Ambush detection (clustered engagement locations with high success) ---
	{
		int32 AmbushLikeEngagements = 0;
		for (const FSHEngagementRecord& E : EngagementHistory)
		{
			if (E.Timestamp < WindowStart) continue;
			// High defender kills with low friendly casualties suggests ambush.
			if (E.HostileCasualties == 0 && E.FriendlyCasualties > 0)
			{
				AmbushLikeEngagements++;
			}
		}

		if (AmbushLikeEngagements >= 2)
		{
			FSHDetectedTactic Ambush;
			Ambush.TacticType = TEXT("ambush");
			Ambush.Confidence = FMath::Clamp(static_cast<float>(AmbushLikeEngagements) / 5.f, 0.f, 1.f);
			Ambush.SuggestedCounter = TEXT("recon_by_fire");
			DetectedTactics.Add(MoveTemp(Ambush));
		}
	}

	// --- Counter-attack detection (player movement towards PLA positions) ---
	{
		// Check if average player velocity vector is pointing towards PLA-held sectors.
		// Simplified: if many players are moving at high speed, they may be counter-attacking.
		int32 FastMovers = 0;
		for (const auto& Pair : PlayerMovementHistory)
		{
			if (Pair.Value.Num() == 0) continue;
			const FSHMovementSample& Latest = Pair.Value.Last();
			if (Latest.Velocity.Size() > 400.f) // Running speed.
			{
				FastMovers++;
			}
		}

		if (FastMovers > TrackedPlayers / 2)
		{
			FSHDetectedTactic CounterAttack;
			CounterAttack.TacticType = TEXT("counter_attack");
			CounterAttack.Confidence = FMath::Clamp(static_cast<float>(FastMovers) / TrackedPlayers, 0.f, 1.f);
			CounterAttack.SuggestedCounter = TEXT("defensive_hold_with_reserves");
			DetectedTactics.Add(MoveTemp(CounterAttack));
		}
	}
}

void USHPrimordiaTacticalAnalyzer::UpdateSectorAnalyses()
{
	for (auto& Pair : SectorAnalyses)
	{
		FSHSectorAnalysis& Sector = Pair.Value;
		const float Radius = SectorRadii.Contains(Pair.Key) ? SectorRadii[Pair.Key] : SectorQueryRadius;

		// Count players currently in this sector.
		int32 DefenderCount = 0;
		for (const auto& PlayerPair : PlayerMovementHistory)
		{
			if (PlayerPair.Value.Num() == 0) continue;
			const FVector& Pos = PlayerPair.Value.Last().Position;
			if (FVector::Dist(Pos, Sector.Center) < Radius)
			{
				DefenderCount++;
			}
		}
		Sector.EstimatedDefenderCount = DefenderCount;

		// Compute composite defensive strength.
		const float DefenderWeight = FMath::Clamp(static_cast<float>(DefenderCount) / 8.f, 0.f, 1.f); // 8 defenders = max.
		const float WeaponWeight = FMath::Clamp(static_cast<float>(Sector.HeavyWeaponCount) / 4.f, 0.f, 1.f);
		const float CoverWeight = 1.f - Sector.CoverDensity; // Less cover for attackers = harder.
		const float ExposureWeight = Sector.ExposureScore; // More exposure = harder to approach.

		Sector.DefensiveStrength = (DefenderWeight * 0.35f + WeaponWeight * 0.25f + CoverWeight * 0.2f + ExposureWeight * 0.2f);
	}
}

void USHPrimordiaTacticalAnalyzer::PruneMovementHistory()
{
	const double Cutoff = FPlatformTime::Seconds() - MovementHistoryWindow;

	for (auto& Pair : PlayerMovementHistory)
	{
		TArray<FSHMovementSample>& Samples = Pair.Value;
		while (Samples.Num() > 0 && Samples[0].Timestamp < Cutoff)
		{
			Samples.RemoveAt(0);
		}
	}
}

TSharedPtr<FJsonObject> USHPrimordiaTacticalAnalyzer::SectorToJson(const FSHSectorAnalysis& Sector) const
{
	TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
	Obj->SetStringField(TEXT("tag"), Sector.SectorTag.ToString());
	Obj->SetNumberField(TEXT("center_x"), Sector.Center.X);
	Obj->SetNumberField(TEXT("center_y"), Sector.Center.Y);
	Obj->SetNumberField(TEXT("center_z"), Sector.Center.Z);
	Obj->SetNumberField(TEXT("defenders"), Sector.EstimatedDefenderCount);
	Obj->SetNumberField(TEXT("heavy_weapons"), Sector.HeavyWeaponCount);
	Obj->SetNumberField(TEXT("cover_density"), Sector.CoverDensity);
	Obj->SetNumberField(TEXT("exposure"), Sector.ExposureScore);
	Obj->SetNumberField(TEXT("approach_routes"), Sector.ApproachRouteCount);
	Obj->SetNumberField(TEXT("defensive_strength"), Sector.DefensiveStrength);
	Obj->SetNumberField(TEXT("attack_success_rate"), Sector.AttackSuccessRate);
	Obj->SetNumberField(TEXT("engagements"), Sector.EngagementCount);
	return Obj;
}
