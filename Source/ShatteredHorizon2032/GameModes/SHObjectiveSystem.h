// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "SHObjectiveSystem.generated.h"

/** Objective types that drive different gameplay patterns. */
UENUM(BlueprintType)
enum class ESHObjectiveType : uint8
{
	Capture		UMETA(DisplayName = "Capture"),		// Occupy and hold an area
	Destroy		UMETA(DisplayName = "Destroy"),		// Eliminate a target
	Defend		UMETA(DisplayName = "Defend"),		// Protect a location for a duration
	Extract		UMETA(DisplayName = "Extract"),		// Move to extraction point
	Recon		UMETA(DisplayName = "Recon"),		// Observe and report without engaging
	Neutralize	UMETA(DisplayName = "Neutralize")	// Disable an enemy system (EW, AA, etc.)
};

/** Objective urgency / priority. */
UENUM(BlueprintType)
enum class ESHObjectivePriority : uint8
{
	Primary		UMETA(DisplayName = "Primary"),
	Secondary	UMETA(DisplayName = "Secondary"),
	Bonus		UMETA(DisplayName = "Bonus")
};

/** Objective status. */
UENUM(BlueprintType)
enum class ESHObjectiveStatus : uint8
{
	Inactive	UMETA(DisplayName = "Inactive"),
	Active		UMETA(DisplayName = "Active"),
	Completed	UMETA(DisplayName = "Completed"),
	Failed		UMETA(DisplayName = "Failed"),
	Expired		UMETA(DisplayName = "Expired")
};

/** Full objective descriptor. */
USTRUCT(BlueprintType)
struct FSHObjective
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	FGuid ObjectiveId;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	FText DisplayName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	FText Description;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	ESHObjectiveType Type = ESHObjectiveType::Capture;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	ESHObjectivePriority Priority = ESHObjectivePriority::Primary;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	ESHObjectiveStatus Status = ESHObjectiveStatus::Inactive;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	FVector WorldLocation = FVector::ZeroVector;

	/** For Defend objectives: duration in seconds the area must be held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	float DefendDuration = 0.f;

	/** Completion progress [0..1]. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	float Progress = 0.f;

	/** Time limit in seconds (0 = no limit). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	float TimeLimit = 0.f;

	/** Elapsed time since activation. */
	UPROPERTY(BlueprintReadOnly, Category = "Objective")
	float ElapsedTime = 0.f;

	/** Radius for area-based objectives (Capture, Defend) in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	float AreaRadius = 2000.f;

	/** Required friendly count in area to progress Capture/Defend. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Objective")
	int32 RequiredPresence = 1;
};

// Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnObjectiveActivated, const FSHObjective&, Objective);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnObjectiveCompleted, const FSHObjective&, Objective);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnObjectiveFailed, const FSHObjective&, Objective);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnObjectiveProgressChanged, const FGuid&, ObjectiveId, float, NewProgress);

/**
 * USHObjectiveSystem
 *
 * World subsystem that manages all gameplay objectives.
 * Supports capture, destroy, defend, extract, recon, and neutralize
 * objective types with area-based progress tracking.
 */
UCLASS()
class SHATTEREDHORIZON2032_API USHObjectiveSystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/** Called from game mode tick. */
	void Tick(float DeltaSeconds);

	// ------------------------------------------------------------------
	//  Objective management
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "SH|Objectives")
	FGuid AddObjective(const FSHObjective& Objective);

	UFUNCTION(BlueprintCallable, Category = "SH|Objectives")
	void ActivateObjective(const FGuid& ObjectiveId);

	UFUNCTION(BlueprintCallable, Category = "SH|Objectives")
	void CompleteObjective(const FGuid& ObjectiveId);

	UFUNCTION(BlueprintCallable, Category = "SH|Objectives")
	void FailObjective(const FGuid& ObjectiveId);

	UFUNCTION(BlueprintCallable, Category = "SH|Objectives")
	void SetObjectiveProgress(const FGuid& ObjectiveId, float Progress);

	// ------------------------------------------------------------------
	//  Queries
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|Objectives")
	const FSHObjective* GetObjective(const FGuid& ObjectiveId) const;

	UFUNCTION(BlueprintPure, Category = "SH|Objectives")
	TArray<FSHObjective> GetActiveObjectives() const;

	UFUNCTION(BlueprintPure, Category = "SH|Objectives")
	TArray<FSHObjective> GetObjectivesByPriority(ESHObjectivePriority Priority) const;

	UFUNCTION(BlueprintPure, Category = "SH|Objectives")
	int32 GetCompletedObjectiveCount() const;

	UFUNCTION(BlueprintPure, Category = "SH|Objectives")
	int32 GetFailedObjectiveCount() const;

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Objectives")
	FOnObjectiveActivated OnObjectiveActivated;

	UPROPERTY(BlueprintAssignable, Category = "SH|Objectives")
	FOnObjectiveCompleted OnObjectiveCompleted;

	UPROPERTY(BlueprintAssignable, Category = "SH|Objectives")
	FOnObjectiveFailed OnObjectiveFailed;

	UPROPERTY(BlueprintAssignable, Category = "SH|Objectives")
	FOnObjectiveProgressChanged OnObjectiveProgressChanged;

private:
	void TickObjective(FSHObjective& Obj, float DeltaSeconds);
	void TickCaptureObjective(FSHObjective& Obj, float DeltaSeconds);
	void TickDefendObjective(FSHObjective& Obj, float DeltaSeconds);
	int32 CountFriendliesInRadius(const FVector& Center, float Radius) const;
	int32 CountEnemiesInRadius(const FVector& Center, float Radius) const;

	void BindToWorldTick();
	FDelegateHandle TickDelegateHandle;

	UPROPERTY()
	TMap<FGuid, FSHObjective> Objectives;
};
