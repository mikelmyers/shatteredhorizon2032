// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "SHGameMode.h"
#include "SHGameState.generated.h"

/** Defensive line identifiers from beach to interior. */
UENUM(BlueprintType)
enum class ESHDefensiveLine : uint8
{
	Primary     UMETA(DisplayName = "Primary (Beach)"),
	Secondary   UMETA(DisplayName = "Secondary (Seawall)"),
	Final       UMETA(DisplayName = "Final (Urban)")
};

/** Weather condition affecting gameplay. */
UENUM(BlueprintType)
enum class ESHWeatherCondition : uint8
{
	Clear,
	Overcast,
	Rain,
	HeavyRain,
	Fog,
	Typhoon
};

/** Replicated force disposition for a single side. */
USTRUCT(BlueprintType)
struct FSHForceDisposition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 InfantryCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 ArmorCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 AmphibiousCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 AirAssets = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 Casualties = 0;

	/** Zones currently held (zone tag names). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<FName> HeldZones;

	/** Total active combat strength: alive units minus suppressed. */
	int32 GetActiveStrength() const
	{
		return FMath::Max(0, InfantryCount + ArmorCount * 5 + AmphibiousCount * 3 + AirAssets * 8 - Casualties);
	}
};

/** Status of a single defensive line. */
USTRUCT(BlueprintType)
struct FSHDefensiveLineStatus
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESHDefensiveLine Line = ESHDefensiveLine::Primary;

	/** 0 = fully breached, 1 = fully intact. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Integrity = 1.f;

	/** Number of friendly units actively defending this line. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 DefenderCount = 0;

	/** Number of fortifications remaining. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 FortificationCount = 0;
};

/** Replicated environmental conditions. */
USTRUCT(BlueprintType)
struct FSHEnvironmentState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ESHWeatherCondition Weather = ESHWeatherCondition::Clear;

	/** 0..1 visibility factor (0 = zero vis, 1 = perfect). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float Visibility = 1.f;

	/** Time of day in 24 h float. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float TimeOfDayHours = 5.f;

	/** Wind speed in m/s — affects ballistics and smoke. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float WindSpeed = 3.f;

	/** Wind direction in degrees (0 = North). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	float WindDirectionDeg = 90.f;
};

/**
 * ASHGameState
 *
 * Replicated game state visible to all clients.
 * Holds current mission phase, force disposition, defensive line status,
 * environmental conditions, and a composite battle intensity metric that
 * drives audio, VFX, and AI aggression.
 */
UCLASS()
class SHATTEREDHORIZON2032_API ASHGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	ASHGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// ------------------------------------------------------------------
	//  Phase / objectives
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|GameState")
	ESHMissionPhase GetMissionPhase() const { return MissionPhase; }

	void SetMissionPhase(ESHMissionPhase Phase);

	UFUNCTION(BlueprintPure, Category = "SH|GameState")
	const TArray<FSHDynamicObjective>& GetActiveObjectives() const { return ActiveObjectives; }

	void SetActiveObjectives(const TArray<FSHDynamicObjective>& Objectives);

	// ------------------------------------------------------------------
	//  Force disposition
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|GameState")
	const FSHForceDisposition& GetFriendlyForces() const { return FriendlyForces; }

	UFUNCTION(BlueprintPure, Category = "SH|GameState")
	const FSHForceDisposition& GetEnemyForces() const { return EnemyForces; }

	/** Normalized enemy strength (0..1) relative to initial projection. */
	UFUNCTION(BlueprintPure, Category = "SH|GameState")
	float GetEnemyForceStrengthNormalized() const;

	/** Friendly casualty rate (0..1). */
	UFUNCTION(BlueprintPure, Category = "SH|GameState")
	float GetFriendlyCasualtyRate() const;

	void AddEnemyForces(int32 Infantry, int32 Armor, int32 Amphibious);
	void AddFriendlyCasualties(int32 Count);

	// ------------------------------------------------------------------
	//  Defensive lines
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|GameState")
	float GetDefensiveLineIntegrity(ESHDefensiveLine Line) const;

	UFUNCTION(BlueprintCallable, Category = "SH|GameState")
	void SetDefensiveLineIntegrity(ESHDefensiveLine Line, float Integrity);

	UFUNCTION(BlueprintCallable, Category = "SH|GameState")
	void DamageDefensiveLine(ESHDefensiveLine Line, float Damage);

	// ------------------------------------------------------------------
	//  Battle intensity & suppression
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|GameState")
	float GetBattleIntensity() const { return BattleIntensity; }

	void AddBattleIntensity(float Delta);
	void SetBattleIntensity(float Value);

	UFUNCTION(BlueprintPure, Category = "SH|GameState")
	float GetGlobalSuppressionModifier() const { return GlobalSuppressionModifier; }

	void SetGlobalSuppressionModifier(float Value);

	UFUNCTION(BlueprintPure, Category = "SH|GameState")
	float GetGlobalMoraleModifier() const { return GlobalMoraleModifier; }

	void SetGlobalMoraleModifier(float Value);

	// ------------------------------------------------------------------
	//  AI director feedback
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|AI")
	float GetAIAggressionModifier() const { return AIAggressionModifier; }

	void SetAIAggressionModifier(float Value);

	// ------------------------------------------------------------------
	//  Environment
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|Environment")
	const FSHEnvironmentState& GetEnvironmentState() const { return Environment; }

	void SetTimeOfDay(float Hours);
	void SetVisibility(float InVisibility);
	void SetWeather(ESHWeatherCondition InWeather);
	void SetWind(float Speed, float DirectionDeg);

protected:
	virtual void Tick(float DeltaSeconds) override;

private:
	/** Decay battle intensity over time toward a baseline. */
	void DecayBattleIntensity(float DeltaSeconds);

	// ------------------------------------------------------------------
	//  Replicated properties
	// ------------------------------------------------------------------

	UPROPERTY(Replicated)
	ESHMissionPhase MissionPhase = ESHMissionPhase::PreInvasion;

	UPROPERTY(Replicated)
	TArray<FSHDynamicObjective> ActiveObjectives;

	UPROPERTY(Replicated)
	FSHForceDisposition FriendlyForces;

	UPROPERTY(Replicated)
	FSHForceDisposition EnemyForces;

	UPROPERTY(Replicated)
	TArray<FSHDefensiveLineStatus> DefensiveLines;

	/** 0..1 composite battle intensity metric. */
	UPROPERTY(Replicated)
	float BattleIntensity = 0.f;

	UPROPERTY(Replicated)
	float GlobalSuppressionModifier = 0.f;

	UPROPERTY(Replicated)
	float GlobalMoraleModifier = 1.f;

	UPROPERTY(Replicated)
	float AIAggressionModifier = 0.f;

	UPROPERTY(Replicated)
	FSHEnvironmentState Environment;

	/** Expected peak enemy force strength for normalization. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Balance")
	int32 ExpectedPeakEnemyStrength = 500;

	/** Initial friendly force count for casualty rate computation. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Balance")
	int32 InitialFriendlyCount = 120;

	/** Rate at which battle intensity decays per second. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Balance")
	float BattleIntensityDecayRate = 0.02f;

	/** Minimum battle intensity during active assault phases. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Balance")
	float MinAssaultIntensity = 0.15f;
};
