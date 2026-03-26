// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Combat/SHMoraleSystem.h"
#include "SHEnemyCharacter.generated.h"

class USHAIPerceptionConfig;
class USHDeathSystem;

// -----------------------------------------------------------------------
//  Enums
// -----------------------------------------------------------------------

/** PLA equipment/role variant. */
UENUM(BlueprintType)
enum class ESHEnemyRole : uint8
{
	Rifleman      UMETA(DisplayName = "Rifleman"),
	MachineGunner UMETA(DisplayName = "Machine Gunner"),
	RPG           UMETA(DisplayName = "RPG"),
	Officer       UMETA(DisplayName = "Officer"),
	Engineer      UMETA(DisplayName = "Engineer"),
	Marksman      UMETA(DisplayName = "Marksman"),
	Medic         UMETA(DisplayName = "Medic")
};

// ESHMoraleState is defined in Combat/SHMoraleSystem.h (authoritative source)

/** Voice line context for Mandarin callouts. */
UENUM(BlueprintType)
enum class ESHVoiceLineContext : uint8
{
	ContactFront,
	ContactLeft,
	ContactRight,
	ContactRear,
	Reloading,
	ThrowingGrenade,
	Suppressed,
	ManDown,
	Retreating,
	AdvanceOrder,
	HoldPosition,
	Surrendering,
	EnemyDown,
	RequestingSupport,
	CoveringFire
};

// -----------------------------------------------------------------------
//  Structs
// -----------------------------------------------------------------------

/** Armor plate configuration. */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHArmorConfig
{
	GENERATED_BODY()

	/** Damage reduction multiplier for torso hits (0..1, lower = more protection). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "1"))
	float TorsoDamageMultiplier = 0.5f;

	/** Whether this soldier has a helmet (headshot protection, partial). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	bool bHasHelmet = true;

	/** Helmet ricochet chance (0..1). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0", ClampMax = "1"))
	float HelmetRicochetChance = 0.2f;

	/** Armor hit points — degrades with hits. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta = (ClampMin = "0"))
	float ArmorHP = 100.f;

	float CurrentArmorHP = 100.f;
};

/** Equipment loadout definition. */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHEnemyLoadout
{
	GENERATED_BODY()

	/** Primary weapon class. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AActor> PrimaryWeaponClass;

	/** Sidearm class (may be null). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TSubclassOf<AActor> SidearmClass;

	/** Number of frag grenades. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 FragGrenadeCount = 2;

	/** Number of smoke grenades. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 SmokeGrenadeCount = 1;

	/** Primary weapon magazine count. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	int32 PrimaryMagazines = 6;

	/** Equipment items to drop on death (mesh/blueprint references). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TArray<TSubclassOf<AActor>> DropOnDeathClasses;
};

/** Delegate broadcast when morale changes significantly. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnMoraleChanged, ASHEnemyCharacter*, Character, ESHMoraleState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemyDeath, ASHEnemyCharacter*, Character);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnEnemySurrender, ASHEnemyCharacter*, Character);

// -----------------------------------------------------------------------

/**
 * ASHEnemyCharacter
 *
 * PLA enemy soldier character.  Handles health, armor, morale, equipment
 * loadout, death/wound state, equipment drops, squad cohesion awareness,
 * and contextual Mandarin voice lines.
 */
UCLASS(BlueprintType, Blueprintable)
class SHATTEREDHORIZON2032_API ASHEnemyCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ASHEnemyCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	// ------------------------------------------------------------------
	//  Health & Damage
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|Enemy")
	float GetHealth() const { return CurrentHealth; }

	UFUNCTION(BlueprintPure, Category = "SH|Enemy")
	float GetHealthPercent() const { return MaxHealth > 0.f ? CurrentHealth / MaxHealth : 0.f; }

	UFUNCTION(BlueprintPure, Category = "SH|Enemy")
	bool IsAlive() const { return CurrentHealth > 0.f && MoraleState != ESHMoraleState::Surrendered; }

	UFUNCTION(BlueprintPure, Category = "SH|Enemy")
	bool IsDead() const { return CurrentHealth <= 0.f; }

	// ------------------------------------------------------------------
	//  Role & Equipment
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|Enemy")
	ESHEnemyRole GetRole() const { return Role; }

	UFUNCTION(BlueprintCallable, Category = "SH|Enemy")
	void SetRole(ESHEnemyRole NewRole);

	UFUNCTION(BlueprintPure, Category = "SH|Enemy")
	int32 GetFragGrenadesRemaining() const { return Loadout.FragGrenadeCount; }

	UFUNCTION(BlueprintPure, Category = "SH|Enemy")
	int32 GetSmokeGrenadesRemaining() const { return Loadout.SmokeGrenadeCount; }

	UFUNCTION(BlueprintCallable, Category = "SH|Enemy")
	void ConsumeFragGrenade() { Loadout.FragGrenadeCount = FMath::Max(0, Loadout.FragGrenadeCount - 1); }

	UFUNCTION(BlueprintCallable, Category = "SH|Enemy")
	void ConsumeSmokeGrenade() { Loadout.SmokeGrenadeCount = FMath::Max(0, Loadout.SmokeGrenadeCount - 1); }

	// ------------------------------------------------------------------
	//  Morale
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|Enemy|Morale")
	ESHMoraleState GetMoraleState() const { return MoraleState; }

	UFUNCTION(BlueprintPure, Category = "SH|Enemy|Morale")
	float GetMoraleValue() const { return Morale; }

	/** Apply a morale modifier (positive = boost, negative = stress). */
	UFUNCTION(BlueprintCallable, Category = "SH|Enemy|Morale")
	void ApplyMoraleModifier(float Delta);

	/** Force a specific morale state (e.g., for scripted events). */
	UFUNCTION(BlueprintCallable, Category = "SH|Enemy|Morale")
	void ForceMoraleState(ESHMoraleState NewState);

	// ------------------------------------------------------------------
	//  Surrender
	// ------------------------------------------------------------------

	/** Attempt surrender. Succeeds only if heavily suppressed and isolated. */
	UFUNCTION(BlueprintCallable, Category = "SH|Enemy|Morale")
	bool AttemptSurrender();

	UFUNCTION(BlueprintPure, Category = "SH|Enemy|Morale")
	bool HasSurrendered() const { return MoraleState == ESHMoraleState::Surrendered; }

	// ------------------------------------------------------------------
	//  Squad cohesion
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|Enemy|Squad")
	int32 GetSquadId() const { return SquadId; }

	UFUNCTION(BlueprintCallable, Category = "SH|Enemy|Squad")
	void SetSquadId(int32 InSquadId) { SquadId = InSquadId; }

	UFUNCTION(BlueprintPure, Category = "SH|Enemy|Squad")
	int32 GetNearbySquadMateCount() const { return NearbySquadMateCount; }

	/** Cohesion bonus multiplier (1.0 = alone, up to ~1.3 when fully surrounded by squad). */
	UFUNCTION(BlueprintPure, Category = "SH|Enemy|Squad")
	float GetSquadCohesionBonus() const;

	// ------------------------------------------------------------------
	//  Suppression
	// ------------------------------------------------------------------

	/** Add suppression pressure (from incoming fire). */
	UFUNCTION(BlueprintCallable, Category = "SH|Enemy|Combat")
	void AddSuppression(float Amount);

	UFUNCTION(BlueprintPure, Category = "SH|Enemy|Combat")
	float GetSuppressionLevel() const { return SuppressionLevel; }

	UFUNCTION(BlueprintPure, Category = "SH|Enemy|Combat")
	bool IsHeavilySuppressed() const { return SuppressionLevel > HeavySuppressionThreshold; }

	// ------------------------------------------------------------------
	//  Voice lines
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintCallable, Category = "SH|Enemy|Voice")
	void PlayVoiceLine(ESHVoiceLineContext Context);

	// ------------------------------------------------------------------
	//  Death & Cleanup
	// ------------------------------------------------------------------

	/** Spawn equipment drops and transition to ragdoll. */
	UFUNCTION(BlueprintCallable, Category = "SH|Enemy")
	void Die();

	/** Spawn wound state (incapacitated but not dead). */
	UFUNCTION(BlueprintCallable, Category = "SH|Enemy")
	void BecomeWounded();

	// ------------------------------------------------------------------
	//  Perception Config
	// ------------------------------------------------------------------

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Enemy|Perception")
	TObjectPtr<USHAIPerceptionConfig> PerceptionConfig;

	/** Death physics — ragdoll with momentum transfer and body persistence. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|Enemy|Components")
	TObjectPtr<USHDeathSystem> DeathSystemComp;

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Enemy")
	FOnMoraleChanged OnMoraleChanged;

	UPROPERTY(BlueprintAssignable, Category = "SH|Enemy")
	FOnEnemyDeath OnDeathEvent;

	UPROPERTY(BlueprintAssignable, Category = "SH|Enemy")
	FOnEnemySurrender OnSurrenderEvent;

	// ------------------------------------------------------------------
	//  Configuration (EditDefaultsOnly per variant blueprint)
	// ------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Enemy|Config")
	ESHEnemyRole Role = ESHEnemyRole::Rifleman;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Enemy|Config")
	float MaxHealth = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Enemy|Config")
	FSHArmorConfig Armor;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Enemy|Config")
	FSHEnemyLoadout Loadout;

	/** Radius to check for nearby squad mates (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Enemy|Config")
	float SquadCohesionRadius = 2000.f;

	/** Suppression level above which the soldier is considered heavily suppressed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Enemy|Config")
	float HeavySuppressionThreshold = 0.7f;

	/** Suppression decay rate per second. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Enemy|Config")
	float SuppressionDecayRate = 0.15f;

	/** Base morale recovery rate per second when in cover / not under fire. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Enemy|Config")
	float MoraleRecoveryRate = 0.02f;

	/** Role-based loadout presets (maps applied in SetRole). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Enemy|Config")
	TMap<ESHEnemyRole, FSHEnemyLoadout> RoleLoadoutPresets;

	// ------------------------------------------------------------------
	//  Voice line data
	// ------------------------------------------------------------------

	/** Voice line sound cues indexed by context. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Enemy|Voice")
	TMap<ESHVoiceLineContext, USoundBase*> VoiceLines;

	/** Minimum interval between voice lines (seconds). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "SH|Enemy|Voice")
	float VoiceLineCooldown = 3.f;

protected:
	// ------------------------------------------------------------------
	//  Internal helpers
	// ------------------------------------------------------------------

	/** Evaluate morale thresholds and transition state. */
	void EvaluateMoraleState();

	/** Count nearby alive squad mates (cached periodically). */
	void UpdateSquadCohesion();

	/** Apply armor to incoming damage and return modified damage. */
	float ApplyArmorDamageReduction(float RawDamage, const FDamageEvent& DamageEvent);

	/** Spawn loot/equipment drop actors. */
	void SpawnEquipmentDrops();

	/** Activate ragdoll physics. */
	void EnableRagdoll();

private:
	// ------------------------------------------------------------------
	//  Runtime state
	// ------------------------------------------------------------------

	UPROPERTY(Replicated)
	float CurrentHealth;

	UPROPERTY(Replicated)
	ESHMoraleState MoraleState = ESHMoraleState::Steady;

	/** Morale value 0..1. */
	float Morale = 0.8f;

	/** Current suppression level 0..1. */
	float SuppressionLevel = 0.f;

	/** Squad identifier (assigned by spawning system). */
	UPROPERTY(Replicated)
	int32 SquadId = INDEX_NONE;

	/** Cached count of nearby alive squad members. */
	int32 NearbySquadMateCount = 0;

	/** Timer for periodic squad cohesion check. */
	float SquadCohesionCheckTimer = 0.f;

	/** Cooldown tracker for voice lines. */
	float VoiceLineCooldownRemaining = 0.f;

	/** Whether this character is in a wounded (incapacitated) state. */
	bool bIsWounded = false;

	/** Whether ragdoll has been activated. */
	bool bRagdollActive = false;
};
