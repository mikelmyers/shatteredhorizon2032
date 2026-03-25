// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SHPrimordiaAletheia.generated.h"

/* -----------------------------------------------------------------------
 *  Enums
 * --------------------------------------------------------------------- */

/** Types of AI tactical decisions that can be validated. */
UENUM(BlueprintType)
enum class ESHAIDecisionType : uint8
{
	Attack    UMETA(DisplayName = "Attack"),
	Retreat   UMETA(DisplayName = "Retreat"),
	Flank     UMETA(DisplayName = "Flank"),
	Suppress  UMETA(DisplayName = "Suppress"),
	Hold      UMETA(DisplayName = "Hold"),
	Advance   UMETA(DisplayName = "Advance")
};

/* -----------------------------------------------------------------------
 *  Structs
 * --------------------------------------------------------------------- */

USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHAIDecision
{
	GENERATED_BODY()

	/** What type of action is being proposed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decision")
	ESHAIDecisionType DecisionType = ESHAIDecisionType::Hold;

	/** Target world location for this decision. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decision")
	FVector TargetLocation = FVector::ZeroVector;

	/** AI confidence in this decision (0 = no confidence, 1 = full confidence). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decision", meta = (ClampMin = "0", ClampMax = "1"))
	float Confidence = 0.5f;

	/** Game-time when this decision was generated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Decision")
	float Timestamp = 0.0f;
};

/** Result of a decision validation. */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHAIDecisionValidation
{
	GENERATED_BODY()

	/** Whether the decision passed validation. */
	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	bool bValid = true;

	/** Human-readable rejection reason (empty if valid). */
	UPROPERTY(BlueprintReadOnly, Category = "Validation")
	FString RejectionReason;
};

/* -----------------------------------------------------------------------
 *  USHPrimordiaAletheia
 * --------------------------------------------------------------------- */

/**
 * USHPrimordiaAletheia
 *
 * Decision validation component for AI units. Evaluates proposed actions
 * before execution to prevent obviously wrong decisions such as advancing
 * into a confirmed kill zone, retreating when a flanking advantage exists,
 * or attacking with broken morale.
 *
 * Tracks decision history for pattern detection and exposes diagnostic
 * metrics such as rejection rate.
 */
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHPrimordiaAletheia : public UActorComponent
{
	GENERATED_BODY()

public:
	USHPrimordiaAletheia();

	// ------------------------------------------------------------------
	//  UActorComponent overrides
	// ------------------------------------------------------------------
	virtual void BeginPlay() override;

	// ------------------------------------------------------------------
	//  Validation
	// ------------------------------------------------------------------

	/**
	 * Validate a proposed AI decision before execution.
	 * Returns true if the decision should proceed, false if rejected.
	 * The OutValidation struct contains the rejection reason on failure.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Aletheia")
	bool ValidateDecision(const FSHAIDecision& Decision, FSHAIDecisionValidation& OutValidation);

	/** Simplified validation — returns bool only. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Aletheia")
	bool IsDecisionValid(const FSHAIDecision& Decision);

	// ------------------------------------------------------------------
	//  Context setters (called by other AI systems)
	// ------------------------------------------------------------------

	/** Report that a location has been identified as a kill zone. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Aletheia")
	void ReportKillZone(FVector Location, float Radius);

	/** Report current morale value for the owning unit. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Aletheia")
	void SetCurrentMorale(float Morale);

	/** Report whether the unit currently has a flanking advantage. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Aletheia")
	void SetFlankingAdvantage(bool bHasAdvantage);

	/** Report known friendly positions for isolation checks. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Aletheia")
	void SetNearbyFriendlyCount(int32 Count);

	/** Report whether the unit is currently suppressed. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Aletheia")
	void SetSuppressed(bool bIsSuppressed);

	// ------------------------------------------------------------------
	//  Diagnostics
	// ------------------------------------------------------------------

	/** Get the overall rejection rate (rejected / total validated). */
	UFUNCTION(BlueprintPure, Category = "SH|Primordia|Aletheia")
	float GetRejectionRate() const;

	/** Get the decision history. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Aletheia")
	TArray<FSHAIDecision> GetDecisionHistory() const { return DecisionHistory; }

	/** Get total number of validations performed. */
	UFUNCTION(BlueprintPure, Category = "SH|Primordia|Aletheia")
	int32 GetTotalValidations() const { return TotalValidations; }

	/** Get total number of rejections. */
	UFUNCTION(BlueprintPure, Category = "SH|Primordia|Aletheia")
	int32 GetTotalRejections() const { return TotalRejections; }

protected:
	/** Morale threshold below which attacks are rejected. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Primordia|Aletheia|Config", meta = (ClampMin = "0", ClampMax = "1"))
	float MoraleAttackThreshold = 0.2f;

	/** Minimum confidence required to approve a decision. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Primordia|Aletheia|Config", meta = (ClampMin = "0", ClampMax = "1"))
	float MinConfidenceThreshold = 0.15f;

	/** Kill zone safety margin (cm) — decisions targeting within this distance of a kill zone are rejected. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Primordia|Aletheia|Config", meta = (ClampMin = "0"))
	float KillZoneSafetyMargin = 500.0f;

	/** Maximum decision history entries to keep. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Primordia|Aletheia|Config", meta = (ClampMin = "10"))
	int32 MaxHistorySize = 200;

	/** How many consecutive identical decisions before pattern rejection kicks in. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Primordia|Aletheia|Config", meta = (ClampMin = "2"))
	int32 RepetitionThreshold = 5;

private:
	// ------------------------------------------------------------------
	//  Validation rules
	// ------------------------------------------------------------------
	bool CheckKillZone(const FSHAIDecision& Decision, FString& OutReason) const;
	bool CheckMorale(const FSHAIDecision& Decision, FString& OutReason) const;
	bool CheckFlankingRetreat(const FSHAIDecision& Decision, FString& OutReason) const;
	bool CheckConfidence(const FSHAIDecision& Decision, FString& OutReason) const;
	bool CheckRepetitivePattern(const FSHAIDecision& Decision, FString& OutReason) const;
	bool CheckSuppressionAdvance(const FSHAIDecision& Decision, FString& OutReason) const;

	// ------------------------------------------------------------------
	//  State
	// ------------------------------------------------------------------

	struct FKillZone
	{
		FVector Location;
		float Radius;
		float ReportedTime;
	};

	TArray<FKillZone> KnownKillZones;
	TArray<FSHAIDecision> DecisionHistory;

	float CurrentMorale = 1.0f;
	bool bHasFlankingAdvantage = false;
	int32 NearbyFriendlyCount = 0;
	bool bIsSuppressed = false;

	int32 TotalValidations = 0;
	int32 TotalRejections = 0;
};
