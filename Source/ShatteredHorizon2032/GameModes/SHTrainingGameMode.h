// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Core/SHGameMode.h"
#include "SHTrainingGameMode.generated.h"

class ASHPlayerCharacter;
class USHSquadManager;

/** Training sections representing pre-deployment military qualification stages. */
UENUM(BlueprintType)
enum class ESHTrainingSection : uint8
{
	Movement		UMETA(DisplayName = "Movement & Maneuver"),
	Shooting		UMETA(DisplayName = "Marksmanship Qualification"),
	SquadCommands	UMETA(DisplayName = "Squad Command Drill"),
	IndirectFire	UMETA(DisplayName = "Indirect Fire Coordination"),
	CombinedArms	UMETA(DisplayName = "Combined Arms Exercise"),
	Complete		UMETA(DisplayName = "Training Complete")
};

/** Descriptor for a pop-up target in the marksmanship qualification. */
USTRUCT(BlueprintType)
struct FSHTrainingTarget
{
	GENERATED_BODY()

	/** Unique target identifier. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training|Targets")
	int32 TargetId = 0;

	/** World location of the target. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training|Targets")
	FVector Location = FVector::ZeroVector;

	/** Distance from firing line in meters. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training|Targets")
	float DistanceMeters = 50.f;

	/** Duration the target stays up before auto-lowering (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Training|Targets")
	float ExposureDuration = 5.f;

	/** True if this target has been hit during the current engagement. */
	UPROPERTY(BlueprintReadOnly, Category = "Training|Targets")
	bool bIsHit = false;

	/** True if this target is currently raised and visible. */
	UPROPERTY(BlueprintReadOnly, Category = "Training|Targets")
	bool bIsActive = false;

	/** Time remaining before the target lowers automatically. */
	UPROPERTY(BlueprintReadOnly, Category = "Training|Targets")
	float TimeRemaining = 0.f;
};

/** Tracks completion criteria for each movement sub-task. */
USTRUCT(BlueprintType)
struct FSHMovementTaskStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Training|Movement")
	bool bSprintToWaypoint = false;

	UPROPERTY(BlueprintReadOnly, Category = "Training|Movement")
	bool bVaultObstacle = false;

	UPROPERTY(BlueprintReadOnly, Category = "Training|Movement")
	bool bGoProne = false;

	UPROPERTY(BlueprintReadOnly, Category = "Training|Movement")
	bool bLean = false;

	bool IsComplete() const
	{
		return bSprintToWaypoint && bVaultObstacle && bGoProne && bLean;
	}
};

/** Tracks marksmanship hits at each range band. */
USTRUCT(BlueprintType)
struct FSHShootingTaskStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Training|Shooting")
	bool bHit50m = false;

	UPROPERTY(BlueprintReadOnly, Category = "Training|Shooting")
	bool bHit100m = false;

	UPROPERTY(BlueprintReadOnly, Category = "Training|Shooting")
	bool bHit200m = false;

	UPROPERTY(BlueprintReadOnly, Category = "Training|Shooting")
	bool bHit300m = false;

	bool IsComplete() const
	{
		return bHit50m && bHit100m && bHit200m && bHit300m;
	}
};

/** Tracks squad command sub-tasks. */
USTRUCT(BlueprintType)
struct FSHSquadCommandTaskStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Training|SquadCommands")
	bool bIssuedMoveOrder = false;

	UPROPERTY(BlueprintReadOnly, Category = "Training|SquadCommands")
	bool bIssuedSuppressOrder = false;

	UPROPERTY(BlueprintReadOnly, Category = "Training|SquadCommands")
	bool bIssuedFlankOrder = false;

	bool IsComplete() const
	{
		return bIssuedMoveOrder && bIssuedSuppressOrder && bIssuedFlankOrder;
	}
};

// Delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTrainingSectionCompleted, ESHTrainingSection, CompletedSection);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnTrainingCompleted, float, TotalTimeSeconds);

/**
 * ASHTrainingGameMode
 *
 * Pre-deployment military training game mode. This is NOT a tutorial —
 * it is framed as a mandatory qualification exercise before the player
 * deploys to the Taoyuan Beach defense scenario.
 *
 * Sections are completed linearly. Each section requires the player to
 * demonstrate competency in the relevant skill set before advancing.
 * The full exercise is designed to complete in under 15 minutes.
 *
 * Returning players may skip training entirely via SkipTraining().
 */
UCLASS()
class SHATTEREDHORIZON2032_API ASHTrainingGameMode : public ASHGameMode
{
	GENERATED_BODY()

public:
	ASHTrainingGameMode();

	// ------------------------------------------------------------------
	//  AGameModeBase overrides
	// ------------------------------------------------------------------
	virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ------------------------------------------------------------------
	//  Section progression
	// ------------------------------------------------------------------

	/** Advance to the specified training section. Validates linear ordering. */
	UFUNCTION(BlueprintCallable, Category = "SH|Training")
	void AdvanceToSection(ESHTrainingSection NewSection);

	/** Get the current active training section. */
	UFUNCTION(BlueprintPure, Category = "SH|Training")
	ESHTrainingSection GetCurrentSection() const { return CurrentSection; }

	/** Get the completion progress of the current section (0.0 to 1.0). */
	UFUNCTION(BlueprintPure, Category = "SH|Training")
	float GetSectionProgress() const;

	/** Skip all remaining training and mark as complete. For returning players. */
	UFUNCTION(BlueprintCallable, Category = "SH|Training")
	void SkipTraining();

	// ------------------------------------------------------------------
	//  Target management (marksmanship qualification)
	// ------------------------------------------------------------------

	/** Spawn a pop-up target at the specified location and range. */
	UFUNCTION(BlueprintCallable, Category = "SH|Training")
	int32 SpawnPopUpTarget(FVector Location, float DistanceMeters, float ExposureDuration = 5.f);

	/** Register a hit on a training target. */
	UFUNCTION(BlueprintCallable, Category = "SH|Training")
	void RegisterTargetHit(int32 TargetId);

	/** Get all currently active training targets. */
	UFUNCTION(BlueprintPure, Category = "SH|Training")
	TArray<FSHTrainingTarget> GetActiveTargets() const;

	// ------------------------------------------------------------------
	//  Event registration (called by player/squad systems)
	// ------------------------------------------------------------------

	/** Notify that the player sprinted to the designated waypoint. */
	UFUNCTION(BlueprintCallable, Category = "SH|Training")
	void NotifySprintToWaypoint();

	/** Notify that the player vaulted an obstacle. */
	UFUNCTION(BlueprintCallable, Category = "SH|Training")
	void NotifyVaultObstacle();

	/** Notify that the player went prone. */
	UFUNCTION(BlueprintCallable, Category = "SH|Training")
	void NotifyGoProne();

	/** Notify that the player leaned. */
	UFUNCTION(BlueprintCallable, Category = "SH|Training")
	void NotifyLean();

	/** Notify that the player issued a squad move order. */
	UFUNCTION(BlueprintCallable, Category = "SH|Training")
	void NotifySquadMoveOrder();

	/** Notify that the player issued a suppress order. */
	UFUNCTION(BlueprintCallable, Category = "SH|Training")
	void NotifySquadSuppressOrder();

	/** Notify that the player issued a flank order. */
	UFUNCTION(BlueprintCallable, Category = "SH|Training")
	void NotifySquadFlankOrder();

	/** Notify that the player called for fire on the marked target. */
	UFUNCTION(BlueprintCallable, Category = "SH|Training")
	void NotifyCallForFire();

	/** Notify that the player coordinated a combined arms engagement. */
	UFUNCTION(BlueprintCallable, Category = "SH|Training")
	void NotifyCombinedArmsComplete();

	// ------------------------------------------------------------------
	//  Time tracking
	// ------------------------------------------------------------------

	/** Get total elapsed training time in seconds. */
	UFUNCTION(BlueprintPure, Category = "SH|Training")
	float GetTotalTrainingTime() const { return TotalTrainingTime; }

	/** Get the target completion time in seconds (15 minutes). */
	UFUNCTION(BlueprintPure, Category = "SH|Training")
	float GetTargetCompletionTime() const { return TargetCompletionTime; }

	// ------------------------------------------------------------------
	//  Task status queries
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|Training")
	const FSHMovementTaskStatus& GetMovementTaskStatus() const { return MovementTasks; }

	UFUNCTION(BlueprintPure, Category = "SH|Training")
	const FSHShootingTaskStatus& GetShootingTaskStatus() const { return ShootingTasks; }

	UFUNCTION(BlueprintPure, Category = "SH|Training")
	const FSHSquadCommandTaskStatus& GetSquadCommandTaskStatus() const { return SquadCommandTasks; }

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Training")
	FOnTrainingSectionCompleted OnSectionCompleted;

	UPROPERTY(BlueprintAssignable, Category = "SH|Training")
	FOnTrainingCompleted OnTrainingCompleted;

protected:
	// ------------------------------------------------------------------
	//  Internal tick helpers
	// ------------------------------------------------------------------
	void TickMovementSection(float DeltaSeconds);
	void TickShootingSection(float DeltaSeconds);
	void TickSquadCommandsSection(float DeltaSeconds);
	void TickIndirectFireSection(float DeltaSeconds);
	void TickCombinedArmsSection(float DeltaSeconds);
	void TickTrainingTargets(float DeltaSeconds);

	/** Complete the current section and advance to the next. */
	void CompleteCurrentSection();

	/** Set up the marksmanship range targets for the shooting section. */
	void SetupShootingRange();

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Target completion time in seconds (15 minutes). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Training")
	float TargetCompletionTime = 900.f;

	/** Waypoint location for the movement sprint task. */
	UPROPERTY(EditAnywhere, Category = "SH|Training|Movement")
	FVector SprintWaypointLocation = FVector(5000.f, 0.f, 0.f);

	/** Firing line origin for marksmanship qualification. */
	UPROPERTY(EditAnywhere, Category = "SH|Training|Shooting")
	FVector FiringLineOrigin = FVector::ZeroVector;

	/** Forward direction of the firing range. */
	UPROPERTY(EditAnywhere, Category = "SH|Training|Shooting")
	FVector FiringRangeDirection = FVector(1.f, 0.f, 0.f);

	/** Location of the indirect fire target marker. */
	UPROPERTY(EditAnywhere, Category = "SH|Training|IndirectFire")
	FVector IndirectFireTargetLocation = FVector(15000.f, 0.f, 0.f);

private:
	// ------------------------------------------------------------------
	//  Runtime state
	// ------------------------------------------------------------------

	UPROPERTY()
	ESHTrainingSection CurrentSection = ESHTrainingSection::Movement;

	/** Total elapsed training time in seconds. */
	float TotalTrainingTime = 0.f;

	/** Time spent in the current section. */
	float SectionElapsedTime = 0.f;

	/** Movement section task tracking. */
	FSHMovementTaskStatus MovementTasks;

	/** Shooting section task tracking. */
	FSHShootingTaskStatus ShootingTasks;

	/** Squad command section task tracking. */
	FSHSquadCommandTaskStatus SquadCommandTasks;

	/** Whether the call-for-fire was successfully executed. */
	bool bCallForFireComplete = false;

	/** Whether the combined arms exercise was completed. */
	bool bCombinedArmsComplete = false;

	/** Active pop-up targets. */
	UPROPERTY()
	TArray<FSHTrainingTarget> TrainingTargets;

	/** Next target ID to assign. */
	int32 NextTargetId = 1;

	/** Whether training has been skipped. */
	bool bTrainingSkipped = false;

	/** Cached player character reference. */
	UPROPERTY()
	TObjectPtr<ASHPlayerCharacter> CachedPlayerCharacter = nullptr;
};
