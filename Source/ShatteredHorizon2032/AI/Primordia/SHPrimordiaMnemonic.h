// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SHPrimordiaMnemonic.generated.h"

/* -----------------------------------------------------------------------
 *  Enums
 * --------------------------------------------------------------------- */

/** Counter-strategy suggestion types. */
UENUM(BlueprintType)
enum class ESHCounterStrategy : uint8
{
	SetAmbush       UMETA(DisplayName = "Set Ambush"),
	ForceFlank      UMETA(DisplayName = "Force Flanking Route"),
	AggressivePush  UMETA(DisplayName = "Aggressive Push"),
	DefensiveHold   UMETA(DisplayName = "Defensive Hold"),
	LongRangeSetup  UMETA(DisplayName = "Long Range Setup"),
	CloseQuarters   UMETA(DisplayName = "Close Quarters Engagement"),
	SuppressAndMove UMETA(DisplayName = "Suppress and Move"),
	None            UMETA(DisplayName = "No Specific Counter")
};

/* -----------------------------------------------------------------------
 *  Structs
 * --------------------------------------------------------------------- */

USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHPlayerBehaviorProfile
{
	GENERATED_BODY()

	/** Routes the player tends to approach from. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	TArray<FVector> PreferredApproachRoute;

	/** How aggressive the player is (0 = very passive, 1 = very aggressive). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile", meta = (ClampMin = "0", ClampMax = "1"))
	float AggressionLevel = 0.5f;

	/** How frequently the player performs flanking maneuvers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile", meta = (ClampMin = "0", ClampMax = "1"))
	float FlankFrequency = 0.3f;

	/** Tendency to engage at long range / sniper positions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile", meta = (ClampMin = "0", ClampMax = "1"))
	float SniperTendency = 0.2f;

	/** Tendency to rush / charge enemy positions. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile", meta = (ClampMin = "0", ClampMax = "1"))
	float RushTendency = 0.3f;

	/** Average engagement distance in cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
	float AverageEngagementRange = 5000.0f;

	/** Number of actions analyzed to produce this profile. */
	UPROPERTY(BlueprintReadOnly, Category = "Profile")
	int32 SampleCount = 0;
};

USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHCounterStrategyRecommendation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Counter")
	ESHCounterStrategy Strategy = ESHCounterStrategy::None;

	UPROPERTY(BlueprintReadOnly, Category = "Counter")
	FString Reasoning;

	UPROPERTY(BlueprintReadOnly, Category = "Counter", meta = (ClampMin = "0", ClampMax = "1"))
	float Confidence = 0.0f;
};

/* -----------------------------------------------------------------------
 *  Delegates
 * --------------------------------------------------------------------- */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnProfileUpdated, const FSHPlayerBehaviorProfile&, UpdatedProfile);

/* -----------------------------------------------------------------------
 *  USHPrimordiaMnemonic
 * --------------------------------------------------------------------- */

/**
 * USHPrimordiaMnemonic
 *
 * Cross-session learning subsystem. Records player actions over time,
 * builds a behavioral profile, and provides counter-strategy suggestions
 * for the AI to exploit detected patterns.
 *
 * Profiles are persisted via save game between sessions so the AI
 * can adapt to returning players.
 */
UCLASS()
class SHATTEREDHORIZON2032_API USHPrimordiaMnemonic : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ------------------------------------------------------------------
	//  USubsystem overrides
	// ------------------------------------------------------------------
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ------------------------------------------------------------------
	//  Action recording
	// ------------------------------------------------------------------

	/**
	 * Record a player action for behavioral analysis.
	 * @param ActionType  Named action (e.g. "Engage", "Flank", "Rush", "Snipe", "Retreat", "Move").
	 * @param Location    World-space location of the action.
	 * @param Timestamp   Game-time when the action occurred.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Mnemonic")
	void RecordPlayerAction(FName ActionType, FVector Location, float Timestamp);

	// ------------------------------------------------------------------
	//  Analysis
	// ------------------------------------------------------------------

	/** Analyze all recorded actions and recompute the behavior profile. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Mnemonic")
	void AnalyzePlayerBehavior();

	/** Get the current behavior profile. */
	UFUNCTION(BlueprintPure, Category = "SH|Primordia|Mnemonic")
	FSHPlayerBehaviorProfile GetBehaviorProfile() const { return CurrentProfile; }

	/** Get an AI counter-strategy based on the current player profile. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Mnemonic")
	FSHCounterStrategyRecommendation GetCounterStrategy() const;

	// ------------------------------------------------------------------
	//  Persistence
	// ------------------------------------------------------------------

	/** Save the current profile to disk. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Mnemonic")
	void SaveProfile();

	/** Load the profile from disk. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Mnemonic")
	void LoadProfile();

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Primordia|Mnemonic")
	FOnProfileUpdated OnProfileUpdated;

protected:
	/** Save slot name for the player behavior profile. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Primordia|Mnemonic|Config")
	FString SaveSlotName = TEXT("PlayerBehavior");

	/** Minimum number of actions required before analysis produces meaningful results. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Primordia|Mnemonic|Config", meta = (ClampMin = "5"))
	int32 MinActionsForAnalysis = 20;

	/** Maximum recorded actions to keep (FIFO eviction). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Primordia|Mnemonic|Config", meta = (ClampMin = "100"))
	int32 MaxRecordedActions = 5000;

	/** How many most-recent approach positions to store in the profile. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Primordia|Mnemonic|Config", meta = (ClampMin = "1"))
	int32 MaxApproachRoutePoints = 50;

private:
	struct FRecordedAction
	{
		FName ActionType;
		FVector Location;
		float Timestamp;
	};

	/** All recorded player actions. */
	TArray<FRecordedAction> RecordedActions;

	/** Current computed profile. */
	FSHPlayerBehaviorProfile CurrentProfile;

	/** Helper: count actions of a given type. */
	int32 CountActions(FName ActionType) const;

	/** Helper: compute average location of actions of a given type. */
	FVector AverageLocationForType(FName ActionType) const;

	/** Helper: compute average engagement range from combat actions. */
	float ComputeAverageEngagementRange() const;
};
