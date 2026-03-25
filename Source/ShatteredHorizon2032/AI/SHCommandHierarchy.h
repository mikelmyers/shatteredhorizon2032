// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "SHDifficultyConfig.h"
#include "SHCommandHierarchy.generated.h"

class USHPrimordiaClient;
class USHPrimordiaDecisionEngine;
class USHPrimordiaTacticalAnalyzer;

/**
 * Command echelon in the three-tier hierarchy.
 * Doctrine: "Strategic Commander (Thinker, campaign-level),
 *            Operational Commander (sector-level),
 *            Tactical Unit (individual engagement)."
 */
UENUM(BlueprintType)
enum class ESHCommandEchelon : uint8
{
	/** Campaign-level: overall mission objectives, force allocation across sectors. */
	Strategic	UMETA(DisplayName = "Strategic Commander"),

	/** Sector-level: multi-squad coordination, timing, reserves within a sector. */
	Operational	UMETA(DisplayName = "Operational Commander"),

	/** Individual engagement: fire-and-maneuver, cover, suppression. */
	Tactical	UMETA(DisplayName = "Tactical Unit"),
};

/**
 * A strategic-level objective issued from the campaign layer.
 */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHStrategicObjective
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FGuid ObjectiveId;

	/** Objective type: "seize", "defend", "interdict", "withdraw", "feint". */
	UPROPERTY(BlueprintReadOnly)
	FString ObjectiveType;

	/** Target sector tag. */
	UPROPERTY(BlueprintReadOnly)
	FName TargetSector;

	/** Force allocation ratio (0..1 of total available forces). */
	UPROPERTY(BlueprintReadOnly)
	float ForceRatio = 0.f;

	/** Priority (higher = more important). */
	UPROPERTY(BlueprintReadOnly)
	int32 Priority = 0;

	/** Deadline — game time by which this objective should be accomplished. */
	UPROPERTY(BlueprintReadOnly)
	float Deadline = 0.f;
};

/**
 * An operational-level order decomposed from a strategic objective.
 */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHOperationalOrder
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FGuid OrderId;

	/** Originating strategic objective. */
	UPROPERTY(BlueprintReadOnly)
	FGuid SourceObjectiveId;

	/** Order type: "multi_axis_assault", "suppress_and_flank", "hold_sector",
	    "phased_withdrawal", "recon_in_force". */
	UPROPERTY(BlueprintReadOnly)
	FString OrderType;

	/** Sector this order applies to. */
	UPROPERTY(BlueprintReadOnly)
	FName SectorTag;

	/** Squads assigned to this operational order. */
	UPROPERTY(BlueprintReadOnly)
	TArray<int32> AssignedSquadIds;

	/** Phase timing — when this order should begin (game time). */
	UPROPERTY(BlueprintReadOnly)
	float ScheduledTime = 0.f;

	/** Whether reserves should be committed if the order stalls. */
	UPROPERTY(BlueprintReadOnly)
	bool bAutoEscalate = true;
};

// -----------------------------------------------------------------------
//  Delegates
// -----------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnStrategicObjectiveIssued, const FSHStrategicObjective&, Objective);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnOperationalOrderIssued, const FSHOperationalOrder&, Order);

/**
 * USHCommandHierarchy
 *
 * Three-tier command structure for enemy forces.
 *
 * Strategic Commander:
 *   - Receives campaign-level intelligence (from Primordia or local analysis)
 *   - Sets objectives: which sectors to attack, defend, withdraw from
 *   - Allocates force ratios across sectors
 *   - Adapts based on After-Action Reports
 *
 * Operational Commander:
 *   - Translates strategic objectives into multi-squad operational orders
 *   - Coordinates timing for multi-axis assaults
 *   - Manages reserves within a sector
 *   - Escalates to strategic commander when objectives fail
 *
 * Tactical Unit:
 *   - Executes orders autonomously (handled by SHEnemyAIController)
 *   - Makes fire-and-maneuver decisions within commander's intent
 *   - Reports contact and casualties upward
 *
 * Orders flow downward; execution is autonomous. If communications are
 * severed, units continue operating on last known orders.
 */
UCLASS(BlueprintType, Blueprintable)
class SHATTEREDHORIZON2032_API USHCommandHierarchy : public UObject
{
	GENERATED_BODY()

public:
	USHCommandHierarchy();

	void Initialize(UWorld* InWorld, USHPrimordiaTacticalAnalyzer* Analyzer, USHPrimordiaDecisionEngine* Engine);
	void Tick(float DeltaSeconds);

	// ------------------------------------------------------------------
	//  Strategic layer
	// ------------------------------------------------------------------

	/** Evaluate the battlefield and issue/update strategic objectives. */
	UFUNCTION(BlueprintCallable, Category = "SH|Command|Strategic")
	void EvaluateStrategicObjectives();

	/** Get all active strategic objectives. */
	UFUNCTION(BlueprintPure, Category = "SH|Command|Strategic")
	TArray<FSHStrategicObjective> GetActiveObjectives() const { return StrategicObjectives; }

	// ------------------------------------------------------------------
	//  Operational layer
	// ------------------------------------------------------------------

	/** Decompose strategic objectives into operational orders. */
	UFUNCTION(BlueprintCallable, Category = "SH|Command|Operational")
	void GenerateOperationalOrders();

	/** Get all active operational orders. */
	UFUNCTION(BlueprintPure, Category = "SH|Command|Operational")
	TArray<FSHOperationalOrder> GetActiveOperationalOrders() const { return OperationalOrders; }

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Current difficulty tier (affects which command layers are active). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Command|Config")
	ESHDifficultyTier DifficultyTier = ESHDifficultyTier::Veteran;

	/** How often to re-evaluate strategic objectives (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Command|Config")
	float StrategicEvalInterval = 60.f;

	/** How often to re-evaluate operational orders (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Command|Config")
	float OperationalEvalInterval = 20.f;

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Command")
	FOnStrategicObjectiveIssued OnStrategicObjectiveIssued;

	UPROPERTY(BlueprintAssignable, Category = "SH|Command")
	FOnOperationalOrderIssued OnOperationalOrderIssued;

private:
	/** Evaluate a single sector and determine the best strategic action. */
	FSHStrategicObjective EvaluateSector(FName SectorTag) const;

	/** Convert a strategic objective into operational orders. */
	void DecomposeObjectiveToOrders(const FSHStrategicObjective& Objective);

	TWeakObjectPtr<UWorld> WorldRef;
	TWeakObjectPtr<USHPrimordiaTacticalAnalyzer> TacticalAnalyzer;
	TWeakObjectPtr<USHPrimordiaDecisionEngine> DecisionEngine;

	TArray<FSHStrategicObjective> StrategicObjectives;
	TArray<FSHOperationalOrder> OperationalOrders;

	float StrategicEvalAccumulator = 0.f;
	float OperationalEvalAccumulator = 0.f;
};
