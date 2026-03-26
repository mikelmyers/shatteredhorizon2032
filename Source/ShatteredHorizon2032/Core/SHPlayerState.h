// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "Combat/SHMoraleSystem.h"
#include "SHPlayerState.generated.h"

// ESHMoraleState is defined in Combat/SHMoraleSystem.h (authoritative source)

/** Ammo state for a single weapon type. */
USTRUCT(BlueprintType)
struct FSHAmmoState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FName AmmoType;

	/** Rounds currently in the magazine. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MagazineCount = 0;

	/** Total reserve rounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ReserveCount = 0;

	/** Magazine capacity for this ammo type. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 MagazineCapacity = 30;
};

/** Engagement/kill record for AAR and scoring. */
USTRUCT(BlueprintType)
struct FSHEngagementRecord
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Kills = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Assists = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 VehicleKills = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SquadOrdersIssued = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SquadOrdersCompleted = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ObjectivesCompleted = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Revives = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DamageDealt = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float DamageTaken = 0.f;

	/** Longest confirmed kill distance (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float LongestKillDistance = 0.f;

	/** Accuracy: hits / shots fired. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ShotsFired = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ShotsHit = 0;

	float GetAccuracy() const
	{
		return (ShotsFired > 0) ? static_cast<float>(ShotsHit) / static_cast<float>(ShotsFired) : 0.f;
	}
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHOnMoraleChanged, ESHMoraleState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSHOnAmmoChanged, FName, AmmoType, const FSHAmmoState&, NewState);

/**
 * ASHPlayerState
 *
 * Replicated per-player state tracking kill/engagement statistics,
 * morale, ammo/equipment state, and squad leader authority.
 */
UCLASS()
class SHATTEREDHORIZON2032_API ASHPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	ASHPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ------------------------------------------------------------------
	//  Engagement tracking
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|Stats")
	const FSHEngagementRecord& GetEngagementRecord() const { return EngagementRecord; }

	UFUNCTION(BlueprintCallable, Category = "SH|Stats")
	void RecordKill(float Distance);

	UFUNCTION(BlueprintCallable, Category = "SH|Stats")
	void RecordAssist();

	UFUNCTION(BlueprintCallable, Category = "SH|Stats")
	void RecordVehicleKill();

	UFUNCTION(BlueprintCallable, Category = "SH|Stats")
	void RecordShotFired();

	UFUNCTION(BlueprintCallable, Category = "SH|Stats")
	void RecordShotHit();

	UFUNCTION(BlueprintCallable, Category = "SH|Stats")
	void RecordDamageDealt(float Amount);

	UFUNCTION(BlueprintCallable, Category = "SH|Stats")
	void RecordDamageTaken(float Amount);

	UFUNCTION(BlueprintCallable, Category = "SH|Stats")
	void RecordRevive();

	UFUNCTION(BlueprintCallable, Category = "SH|Stats")
	void RecordSquadOrderIssued();

	UFUNCTION(BlueprintCallable, Category = "SH|Stats")
	void RecordSquadOrderCompleted();

	UFUNCTION(BlueprintCallable, Category = "SH|Stats")
	void RecordObjectiveCompleted();

	// ------------------------------------------------------------------
	//  Morale
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|Morale")
	ESHMoraleState GetMoraleState() const { return MoraleState; }

	UFUNCTION(BlueprintPure, Category = "SH|Morale")
	float GetMoraleValue() const { return MoraleValue; }

	/** Adjust morale by a delta. Positive = improve, negative = degrade. */
	UFUNCTION(BlueprintCallable, Category = "SH|Morale")
	void AdjustMorale(float Delta);

	UPROPERTY(BlueprintAssignable, Category = "SH|Morale")
	FSHOnMoraleChanged OnMoraleChanged;

	// ------------------------------------------------------------------
	//  Ammo / equipment
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|Ammo")
	const TArray<FSHAmmoState>& GetAmmoStates() const { return AmmoStates; }

	UFUNCTION(BlueprintCallable, Category = "SH|Ammo")
	void SetAmmoState(FName AmmoType, int32 MagazineCount, int32 ReserveCount);

	UFUNCTION(BlueprintCallable, Category = "SH|Ammo")
	bool ConsumeAmmo(FName AmmoType, int32 Count);

	UFUNCTION(BlueprintCallable, Category = "SH|Ammo")
	void AddReserveAmmo(FName AmmoType, int32 Count);

	UFUNCTION(BlueprintCallable, Category = "SH|Ammo")
	bool ReloadWeapon(FName AmmoType);

	UPROPERTY(BlueprintAssignable, Category = "SH|Ammo")
	FSHOnAmmoChanged OnAmmoChanged;

	// ------------------------------------------------------------------
	//  Squad leader authority
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|Squad")
	int32 GetSquadLeaderAuthorityLevel() const { return SquadLeaderAuthorityLevel; }

	UFUNCTION(BlueprintCallable, Category = "SH|Squad")
	void SetSquadLeaderAuthorityLevel(int32 Level);

	/** Whether this player is the designated squad leader. */
	UFUNCTION(BlueprintPure, Category = "SH|Squad")
	bool IsSquadLeader() const { return SquadLeaderAuthorityLevel > 0; }

protected:
	/** Recalculate morale state from the continuous morale value. */
	void UpdateMoraleState();

private:
	// ------------------------------------------------------------------
	//  Replicated data
	// ------------------------------------------------------------------

	UPROPERTY(Replicated)
	FSHEngagementRecord EngagementRecord;

	UPROPERTY(Replicated)
	ESHMoraleState MoraleState = ESHMoraleState::Steady;

	/** Continuous morale value: 0 = routed, 100 = resolute. */
	UPROPERTY(Replicated)
	float MoraleValue = 75.f;

	UPROPERTY(Replicated)
	TArray<FSHAmmoState> AmmoStates;

	/**
	 * Squad leader authority level.
	 * 0 = not a squad leader. Higher values unlock more command options.
	 * 1 = fire team leader, 2 = squad leader, 3 = platoon-level authority.
	 */
	UPROPERTY(Replicated)
	int32 SquadLeaderAuthorityLevel = 2;
};
