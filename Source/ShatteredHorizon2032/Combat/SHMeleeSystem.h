// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SHMeleeSystem.generated.h"

// -----------------------------------------------------------------------
//  Enums
// -----------------------------------------------------------------------

/** Melee attack type. */
UENUM(BlueprintType)
enum class ESHMeleeType : uint8
{
	Punch       UMETA(DisplayName = "Punch / Rifle Butt"),
	Knife       UMETA(DisplayName = "Knife Stab"),
	Takedown    UMETA(DisplayName = "CQB Takedown"),
	Bayonet     UMETA(DisplayName = "Bayonet Thrust")
};

/** Melee attack direction for animation selection. */
UENUM(BlueprintType)
enum class ESHMeleeDirection : uint8
{
	Front   UMETA(DisplayName = "Front"),
	Left    UMETA(DisplayName = "Left"),
	Right   UMETA(DisplayName = "Right"),
	Behind  UMETA(DisplayName = "Behind")
};

// -----------------------------------------------------------------------
//  Structs
// -----------------------------------------------------------------------

/** Configuration for a melee attack. */
USTRUCT(BlueprintType)
struct FSHMeleeAttackData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
	ESHMeleeType Type = ESHMeleeType::Punch;

	/** Damage dealt on hit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee", meta = (ClampMin = "0"))
	float Damage = 35.0f;

	/** Range (cm) of the melee trace. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee", meta = (ClampMin = "50"))
	float Range = 150.0f;

	/** Sphere trace radius (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee", meta = (ClampMin = "5"))
	float TraceRadius = 25.0f;

	/** Time from input to damage frame (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee", meta = (ClampMin = "0.05"))
	float WindupTime = 0.15f;

	/** Recovery time after the attack before the next action (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee", meta = (ClampMin = "0.1"))
	float RecoveryTime = 0.4f;

	/** If true, a successful hit from behind is an instant kill. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee")
	bool bInstantKillFromBehind = false;

	/** Stamina cost (fraction 0-1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Melee", meta = (ClampMin = "0", ClampMax = "1"))
	float StaminaCost = 0.1f;
};

/** Result of a melee attack. */
USTRUCT(BlueprintType)
struct FSHMeleeResult
{
	GENERATED_BODY()

	bool bHit = false;
	bool bKill = false;
	bool bFromBehind = false;
	float DamageDealt = 0.0f;

	UPROPERTY()
	TObjectPtr<AActor> HitActor = nullptr;

	FVector HitLocation = FVector::ZeroVector;
	FVector HitNormal = FVector::ZeroVector;
};

// -----------------------------------------------------------------------
//  Delegates
// -----------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnMeleeHit, const FSHMeleeResult&, Result);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMeleeStarted);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnMeleeRecovered);

// -----------------------------------------------------------------------
//  USHMeleeSystem
// -----------------------------------------------------------------------

/**
 * USHMeleeSystem
 *
 * Component handling all melee combat: punch/rifle butt, knife, bayonet,
 * and CQB takedowns. Supports directional attacks with behind-kill logic,
 * stamina costs, and animation-driven damage timing.
 */
UCLASS(ClassGroup = (SH), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHMeleeSystem : public UActorComponent
{
	GENERATED_BODY()

public:
	USHMeleeSystem();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ------------------------------------------------------------------
	//  Actions
	// ------------------------------------------------------------------

	/** Initiate a melee attack. */
	UFUNCTION(BlueprintCallable, Category = "SH|Melee")
	void StartMeleeAttack(ESHMeleeType Type);

	/** Cancel a windup if the attack hasn't landed yet. */
	UFUNCTION(BlueprintCallable, Category = "SH|Melee")
	void CancelAttack();

	// ------------------------------------------------------------------
	//  Queries
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|Melee")
	bool IsAttacking() const { return bIsAttacking; }

	UFUNCTION(BlueprintPure, Category = "SH|Melee")
	bool CanAttack() const;

	UFUNCTION(BlueprintPure, Category = "SH|Melee")
	ESHMeleeType GetActiveAttackType() const { return ActiveAttack.Type; }

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Punch / rifle butt attack data. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Melee")
	FSHMeleeAttackData PunchData;

	/** Knife attack data. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Melee")
	FSHMeleeAttackData KnifeData;

	/** CQB takedown data. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Melee")
	FSHMeleeAttackData TakedownData;

	/** Bayonet thrust data. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Melee")
	FSHMeleeAttackData BayonetData;

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Melee")
	FOnMeleeHit OnMeleeHit;

	UPROPERTY(BlueprintAssignable, Category = "SH|Melee")
	FOnMeleeStarted OnMeleeStarted;

	UPROPERTY(BlueprintAssignable, Category = "SH|Melee")
	FOnMeleeRecovered OnMeleeRecovered;

private:
	/** Execute the trace and apply damage. */
	FSHMeleeResult ExecuteMeleeTrace(const FSHMeleeAttackData& Data);

	/** Determine attack direction relative to target facing. */
	ESHMeleeDirection ComputeAttackDirection(const AActor* Target) const;

	/** Get attack data for a type. */
	const FSHMeleeAttackData& GetAttackData(ESHMeleeType Type) const;

	// --- State ---

	bool bIsAttacking = false;
	bool bDamageApplied = false;
	float AttackTimer = 0.0f;
	float RecoveryTimer = 0.0f;
	FSHMeleeAttackData ActiveAttack;
};
