// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SHDeathRecapSystem.generated.h"

// -----------------------------------------------------------------------
//  Damage event record for the death recap
// -----------------------------------------------------------------------

/** A single damage event contributing to the player's death. */
USTRUCT(BlueprintType)
struct FSHDamageRecord
{
	GENERATED_BODY()

	/** Who dealt the damage (may be null if environmental). */
	UPROPERTY(BlueprintReadOnly, Category = "DeathRecap")
	TWeakObjectPtr<AActor> DamageSource;

	/** Instigating controller (for tracking killer identity). */
	UPROPERTY(BlueprintReadOnly, Category = "DeathRecap")
	TWeakObjectPtr<AController> InstigatorController;

	/** Damage amount dealt. */
	UPROPERTY(BlueprintReadOnly, Category = "DeathRecap")
	float DamageAmount = 0.0f;

	/** World location where the damage originated. */
	UPROPERTY(BlueprintReadOnly, Category = "DeathRecap")
	FVector SourceLocation = FVector::ZeroVector;

	/** Hit location on the victim. */
	UPROPERTY(BlueprintReadOnly, Category = "DeathRecap")
	FVector HitLocation = FVector::ZeroVector;

	/** Body region that was hit. */
	UPROPERTY(BlueprintReadOnly, Category = "DeathRecap")
	FName HitBone;

	/** Weapon or damage type name. */
	UPROPERTY(BlueprintReadOnly, Category = "DeathRecap")
	FName WeaponName;

	/** Distance from attacker to victim (cm). */
	UPROPERTY(BlueprintReadOnly, Category = "DeathRecap")
	float DistanceCm = 0.0f;

	/** Whether this was the killing blow. */
	UPROPERTY(BlueprintReadOnly, Category = "DeathRecap")
	bool bWasKillingBlow = false;

	/** Whether this was a headshot. */
	UPROPERTY(BlueprintReadOnly, Category = "DeathRecap")
	bool bWasHeadshot = false;

	/** World timestamp when this damage was dealt. */
	UPROPERTY(BlueprintReadOnly, Category = "DeathRecap")
	float Timestamp = 0.0f;
};

/** Complete death recap — everything the player needs to understand their death. */
USTRUCT(BlueprintType)
struct FSHDeathRecap
{
	GENERATED_BODY()

	/** All damage events in the last N seconds before death. */
	UPROPERTY(BlueprintReadOnly, Category = "DeathRecap")
	TArray<FSHDamageRecord> DamageHistory;

	/** The actor that dealt the killing blow. */
	UPROPERTY(BlueprintReadOnly, Category = "DeathRecap")
	TWeakObjectPtr<AActor> KillerActor;

	/** Killer's weapon name. */
	UPROPERTY(BlueprintReadOnly, Category = "DeathRecap")
	FName KillerWeapon;

	/** Distance of the kill shot (cm). */
	UPROPERTY(BlueprintReadOnly, Category = "DeathRecap")
	float KillDistanceCm = 0.0f;

	/** Whether the kill was a headshot. */
	UPROPERTY(BlueprintReadOnly, Category = "DeathRecap")
	bool bKilledByHeadshot = false;

	/** Total damage taken in the recap window. */
	UPROPERTY(BlueprintReadOnly, Category = "DeathRecap")
	float TotalDamageTaken = 0.0f;

	/** Number of unique damage sources. */
	UPROPERTY(BlueprintReadOnly, Category = "DeathRecap")
	int32 UniqueDamageSources = 0;

	/** Time survived from first damage to death (seconds). */
	UPROPERTY(BlueprintReadOnly, Category = "DeathRecap")
	float TimeUnderFire = 0.0f;

	/** Killer's world position at time of kill. */
	UPROPERTY(BlueprintReadOnly, Category = "DeathRecap")
	FVector KillerLocation = FVector::ZeroVector;

	/** Direction from victim to killer (for death camera). */
	UPROPERTY(BlueprintReadOnly, Category = "DeathRecap")
	FVector DirectionToKiller = FVector::ForwardVector;
};

// -----------------------------------------------------------------------
//  Delegates
// -----------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeathRecapReady, const FSHDeathRecap&, Recap);

// -----------------------------------------------------------------------
//  USHDeathRecapSystem
// -----------------------------------------------------------------------

/**
 * USHDeathRecapSystem
 *
 * Tracks incoming damage to the player and generates a death recap
 * when the player dies. The recap shows:
 * - Who killed you and with what weapon
 * - Kill distance and whether it was a headshot
 * - All damage events in the last 10 seconds
 * - Direction to the killer (for death camera orientation)
 *
 * This is NOT a killcam (no replay) — it's an informational recap
 * consistent with the game's milsim philosophy. The player learns
 * from their death without breaking immersion with a replay.
 */
UCLASS(ClassGroup = (SH), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHDeathRecapSystem : public UActorComponent
{
	GENERATED_BODY()

public:
	USHDeathRecapSystem();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// ------------------------------------------------------------------
	//  Damage recording
	// ------------------------------------------------------------------

	/**
	 * Record an incoming damage event. Should be called from TakeDamage.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|DeathRecap")
	void RecordDamage(const FSHDamageRecord& Record);

	/**
	 * Trigger death recap generation. Called when the player dies.
	 * Returns the generated recap and broadcasts OnDeathRecapReady.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|DeathRecap")
	FSHDeathRecap GenerateDeathRecap();

	/**
	 * Get the last generated death recap.
	 */
	UFUNCTION(BlueprintPure, Category = "SH|DeathRecap")
	const FSHDeathRecap& GetLastRecap() const { return LastRecap; }

	/**
	 * Clear damage history (e.g., on respawn or new phase).
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|DeathRecap")
	void ClearHistory();

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** How far back (seconds) to track damage for the recap. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|DeathRecap")
	float RecapWindowSeconds = 10.0f;

	/** Maximum damage records to retain. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|DeathRecap")
	int32 MaxRecords = 50;

	// ------------------------------------------------------------------
	//  Delegate
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|DeathRecap")
	FOnDeathRecapReady OnDeathRecapReady;

private:
	/** Prune old records beyond the recap window. */
	void PruneOldRecords();

	/** Ring buffer of recent damage events. */
	TArray<FSHDamageRecord> DamageHistory;

	/** Cached last death recap. */
	FSHDeathRecap LastRecap;
};
