// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SHPrimordiaTacticalAnalyzer.generated.h"

// -----------------------------------------------------------------------
//  Data structures for battlefield intelligence
// -----------------------------------------------------------------------

/** A tracked engagement event between forces. */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHEngagementRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	double Timestamp = 0.0;

	UPROPERTY(BlueprintReadOnly)
	FVector Location = FVector::ZeroVector;

	/** Number of friendly (PLA) casualties in this engagement. */
	UPROPERTY(BlueprintReadOnly)
	int32 FriendlyCasualties = 0;

	/** Number of hostile (player-side) casualties. */
	UPROPERTY(BlueprintReadOnly)
	int32 HostileCasualties = 0;

	/** The approach vector that was used. */
	UPROPERTY(BlueprintReadOnly)
	FVector ApproachDirection = FVector::ForwardVector;

	/** Whether this engagement was considered successful from the PLA perspective. */
	UPROPERTY(BlueprintReadOnly)
	bool bSuccessful = false;
};

/** Per-sector defensive analysis. */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHSectorAnalysis
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FName SectorTag;

	/** World-space center of the sector. */
	UPROPERTY(BlueprintReadOnly)
	FVector Center = FVector::ZeroVector;

	/** Estimated number of player-side defenders in this sector. */
	UPROPERTY(BlueprintReadOnly)
	int32 EstimatedDefenderCount = 0;

	/** Number of heavy weapons (MGs, AT) detected in this sector. */
	UPROPERTY(BlueprintReadOnly)
	int32 HeavyWeaponCount = 0;

	/** Cover density score (0..1, higher = more cover available for attackers). */
	UPROPERTY(BlueprintReadOnly)
	float CoverDensity = 0.5f;

	/** Sight-line exposure score (0..1, higher = more exposed approach routes). */
	UPROPERTY(BlueprintReadOnly)
	float ExposureScore = 0.5f;

	/** Number of viable approach routes identified. */
	UPROPERTY(BlueprintReadOnly)
	int32 ApproachRouteCount = 0;

	/** Defensive strength composite score (0..1, higher = harder to attack). */
	UPROPERTY(BlueprintReadOnly)
	float DefensiveStrength = 0.5f;

	/** Recent engagement success rate for PLA attacks in this sector. */
	UPROPERTY(BlueprintReadOnly)
	float AttackSuccessRate = 0.f;

	/** Number of engagements in this sector. */
	UPROPERTY(BlueprintReadOnly)
	int32 EngagementCount = 0;
};

/** Detected player tactic pattern. */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHDetectedTactic
{
	GENERATED_BODY()

	/** Tactic category, e.g. "static_defense", "roaming_patrol", "ambush", "counter_attack". */
	UPROPERTY(BlueprintReadOnly)
	FString TacticType;

	/** Confidence that this tactic is actively being employed (0..1). */
	UPROPERTY(BlueprintReadOnly)
	float Confidence = 0.f;

	/** Sectors where this tactic is observed. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FName> AffectedSectors;

	/** Suggested counter from the analyzer (informational — Primordia decides). */
	UPROPERTY(BlueprintReadOnly)
	FString SuggestedCounter;
};

/** Casualty summary for both sides. */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHCasualtySummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	int32 PLAKilled = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 PLAWounded = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 PLASurrendered = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 DefenderKilled = 0;

	UPROPERTY(BlueprintReadOnly)
	int32 DefenderWounded = 0;

	/** Kill/death ratio from PLA perspective. */
	float GetPLAKDRatio() const
	{
		const int32 TotalPLALosses = PLAKilled + PLASurrendered;
		return TotalPLALosses > 0 ? static_cast<float>(DefenderKilled) / TotalPLALosses : 0.f;
	}
};

/** After-Action Report for a completed phase or time window. */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHAfterActionReport
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	double StartTime = 0.0;

	UPROPERTY(BlueprintReadOnly)
	double EndTime = 0.0;

	UPROPERTY(BlueprintReadOnly)
	FSHCasualtySummary Casualties;

	/** Sectors that were successfully contested / captured. */
	UPROPERTY(BlueprintReadOnly)
	TArray<FName> ContestedSectors;

	/** Directives that succeeded. */
	UPROPERTY(BlueprintReadOnly)
	int32 DirectivesSucceeded = 0;

	/** Directives that failed. */
	UPROPERTY(BlueprintReadOnly)
	int32 DirectivesFailed = 0;

	/** Key observations as free-form text (fed to Primordia). */
	UPROPERTY(BlueprintReadOnly)
	TArray<FString> Observations;
};

/** Movement sample for a tracked entity (player or squad). */
USTRUCT()
struct FSHMovementSample
{
	GENERATED_BODY()

	double Timestamp = 0.0;
	FVector Position = FVector::ZeroVector;
	FVector Velocity = FVector::ZeroVector;
};

// -----------------------------------------------------------------------

/**
 * USHPrimordiaTacticalAnalyzer
 *
 * Aggregates battlefield intelligence and compiles it into a structured
 * state snapshot that the PrimordiaClient sends to the Primordia service.
 * Tracks player positions, engagement history, casualty data, and
 * terrain characteristics to enable Primordia to make informed strategic
 * decisions.
 */
UCLASS(BlueprintType, Blueprintable)
class SHATTEREDHORIZON2032_API USHPrimordiaTacticalAnalyzer : public UObject
{
	GENERATED_BODY()

public:
	USHPrimordiaTacticalAnalyzer();

	// ------------------------------------------------------------------
	//  Lifecycle
	// ------------------------------------------------------------------

	void Initialize(UWorld* InWorld);
	void Tick(float DeltaSeconds);

	// ------------------------------------------------------------------
	//  Data ingestion (called by game systems)
	// ------------------------------------------------------------------

	/** Record a player/squad position update. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Analyzer")
	void ReportPlayerPosition(int32 PlayerId, FVector Position, FVector Velocity);

	/** Record a casualty event. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Analyzer")
	void ReportCasualty(bool bIsPLA, bool bKilled, FVector Location);

	/** Record a surrender event. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Analyzer")
	void ReportSurrender(FVector Location);

	/** Record an engagement outcome. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Analyzer")
	void ReportEngagement(const FSHEngagementRecord& Record);

	/** Register a sector for tracking. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Analyzer")
	void RegisterSector(FName SectorTag, FVector Center, float Radius);

	/** Report a detected heavy weapon emplacement. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Analyzer")
	void ReportHeavyWeapon(FName SectorTag, FVector Position);

	// ------------------------------------------------------------------
	//  State snapshot (for PrimordiaClient)
	// ------------------------------------------------------------------

	/** Build the full battlefield state JSON for transmission to Primordia. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Analyzer")
	FString BuildBattlefieldStateJson() const;

	// ------------------------------------------------------------------
	//  Queries
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Analyzer")
	FSHSectorAnalysis GetSectorAnalysis(FName SectorTag) const;

	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Analyzer")
	TArray<FSHSectorAnalysis> GetAllSectorAnalyses() const;

	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Analyzer")
	TArray<FSHDetectedTactic> GetDetectedTactics() const;

	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Analyzer")
	FSHCasualtySummary GetCasualtySummary() const { return CumulativeCasualties; }

	/** Generate an After-Action Report for the given time window. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Analyzer")
	FSHAfterActionReport GenerateAAR(double StartTime, double EndTime) const;

	// ------------------------------------------------------------------
	//  Terrain analysis (performed once at map load + periodically)
	// ------------------------------------------------------------------

	/** Run a terrain analysis pass for all registered sectors. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Analyzer")
	void PerformTerrainAnalysis();

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** How far back (seconds) to look for movement pattern analysis. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Primordia|Analyzer|Config")
	float MovementHistoryWindow = 120.f;

	/** How often to re-run terrain analysis (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Primordia|Analyzer|Config")
	float TerrainAnalysisInterval = 60.f;

	/** Radius for sector heavy-weapon detection queries (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Primordia|Analyzer|Config")
	float SectorQueryRadius = 5000.f;

private:
	// ------------------------------------------------------------------
	//  Internal helpers
	// ------------------------------------------------------------------

	/** Classify detected player tactics from movement & engagement data. */
	void AnalyzePlayerTactics();

	/** Update sector defensive strengths based on latest data. */
	void UpdateSectorAnalyses();

	/** Prune stale movement samples. */
	void PruneMovementHistory();

	/** Perform line trace based cover density estimation for a sector. */
	float EstimateCoverDensity(FVector Center, float Radius) const;

	/** Estimate exposure along approach routes to a sector. */
	float EstimateExposure(FVector Center, float Radius) const;

	/** Count approach routes (nav mesh query). */
	int32 CountApproachRoutes(FVector Center, float Radius) const;

	/** Serialize a sector to a JSON object. */
	TSharedPtr<class FJsonObject> SectorToJson(const FSHSectorAnalysis& Sector) const;

	// ------------------------------------------------------------------
	//  State
	// ------------------------------------------------------------------

	TWeakObjectPtr<UWorld> WorldRef;

	/** Per-player movement history. */
	TMap<int32, TArray<FSHMovementSample>> PlayerMovementHistory;

	/** Engagement records. */
	TArray<FSHEngagementRecord> EngagementHistory;

	/** Cumulative casualty tracking. */
	FSHCasualtySummary CumulativeCasualties;

	/** Sector analyses keyed by sector tag. */
	TMap<FName, FSHSectorAnalysis> SectorAnalyses;

	/** Sector radii for spatial queries. */
	TMap<FName, float> SectorRadii;

	/** Detected player tactics (updated periodically). */
	TArray<FSHDetectedTactic> DetectedTactics;

	/** Accumulator for terrain analysis. */
	float TerrainAnalysisAccumulator = 0.f;

	/** Accumulator for tactic analysis. */
	float TacticAnalysisAccumulator = 0.f;
};
