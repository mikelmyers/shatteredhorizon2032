// Copyright Shattered Horizon 2032. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "SHSquadOrder.h"
#include "SHSquadMember.generated.h"

class USHSquadManager;
class ASHSquadAIController;
class UAudioComponent;
class USoundCue;

/* ───────────────────────────────────────────────────────────── */
/*  Enumerations                                                */
/* ───────────────────────────────────────────────────────────── */

UENUM(BlueprintType)
enum class ESHSquadRole : uint8
{
	TeamLeader			UMETA(DisplayName = "Team Leader"),
	Rifleman			UMETA(DisplayName = "Rifleman"),
	AutomaticRifleman	UMETA(DisplayName = "Automatic Rifleman (SAW)"),
	Grenadier			UMETA(DisplayName = "Grenadier"),
	DesignatedMarksman	UMETA(DisplayName = "Designated Marksman")
};

UENUM(BlueprintType)
enum class ESHWoundState : uint8
{
	Healthy			UMETA(DisplayName = "Healthy"),
	LightWound		UMETA(DisplayName = "Light Wound"),
	HeavyWound		UMETA(DisplayName = "Heavy Wound"),
	Incapacitated	UMETA(DisplayName = "Incapacitated"),
	KIA				UMETA(DisplayName = "KIA")
};

UENUM(BlueprintType)
enum class ESHSuppressionLevel : uint8
{
	None		UMETA(DisplayName = "None"),
	Light		UMETA(DisplayName = "Light"),
	Moderate	UMETA(DisplayName = "Moderate"),
	Heavy		UMETA(DisplayName = "Heavy"),
	Pinned		UMETA(DisplayName = "Pinned")
};

UENUM(BlueprintType)
enum class ESHBuddyTeam : uint8
{
	Alpha	UMETA(DisplayName = "Alpha"),
	Bravo	UMETA(DisplayName = "Bravo")
};

UENUM(BlueprintType)
enum class ESHCombatStress : uint8
{
	Calm		UMETA(DisplayName = "Calm"),
	Alert		UMETA(DisplayName = "Alert"),
	Stressed	UMETA(DisplayName = "Stressed"),
	Panicked	UMETA(DisplayName = "Panicked")
};

/* ───────────────────────────────────────────────────────────── */
/*  Personality Traits                                          */
/* ───────────────────────────────────────────────────────────── */

USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHPersonalityTraits
{
	GENERATED_BODY()

	/** 0 = very cautious, 1 = very aggressive. Affects willingness to push forward. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Aggression = 0.5f;

	/** 0 = green recruit, 1 = hardened veteran. Affects suppression resistance & accuracy. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Experience = 0.5f;

	/** 0 = lone wolf, 1 = sticks to squad. Affects formation discipline. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Discipline = 0.7f;

	/** 0 = never takes initiative, 1 = acts without orders. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Initiative = 0.5f;

	/** How quickly this member recovers from suppression. Higher = faster. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Personality", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SuppressionResistance = 0.5f;
};

/* ───────────────────────────────────────────────────────────── */
/*  Ammunition Tracking                                         */
/* ───────────────────────────────────────────────────────────── */

USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHAmmoState
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	int32 CurrentMagazineRounds = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	int32 MagazineCapacity = 30;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	int32 TotalReserveMagazines = 7;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	int32 GrenadeCount = 2;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Ammo")
	int32 SmokeGrenadeCount = 1;

	bool IsCurrentMagazineEmpty() const { return CurrentMagazineRounds <= 0; }
	bool HasReserveAmmo() const { return TotalReserveMagazines > 0; }
	bool IsAmmoLow() const { return TotalReserveMagazines <= 2 && CurrentMagazineRounds < MagazineCapacity / 2; }
	bool IsAmmoOut() const { return TotalReserveMagazines <= 0 && CurrentMagazineRounds <= 0; }

	float GetAmmoFraction() const
	{
		const int32 Total = (TotalReserveMagazines * MagazineCapacity) + CurrentMagazineRounds;
		const int32 Max = ((TotalReserveMagazines + 1) * MagazineCapacity); // +1 for loaded mag at full
		return Max > 0 ? static_cast<float>(Total) / static_cast<float>(Max) : 0.f;
	}

	void Reload()
	{
		if (TotalReserveMagazines > 0)
		{
			TotalReserveMagazines--;
			CurrentMagazineRounds = MagazineCapacity;
		}
	}

	bool ConsumeRound()
	{
		if (CurrentMagazineRounds > 0)
		{
			CurrentMagazineRounds--;
			return true;
		}
		return false;
	}
};

/* ───────────────────────────────────────────────────────────── */
/*  Voice Line Types                                            */
/* ───────────────────────────────────────────────────────────── */

UENUM(BlueprintType)
enum class ESHVoiceLineType : uint8
{
	ContactReport		UMETA(DisplayName = "Contact Report"),
	OrderAcknowledge	UMETA(DisplayName = "Order Acknowledge"),
	Reloading			UMETA(DisplayName = "Reloading"),
	Suppressed			UMETA(DisplayName = "Suppressed"),
	WoundedSelf			UMETA(DisplayName = "Wounded Self"),
	WoundedFriendly		UMETA(DisplayName = "Wounded Friendly"),
	ManDown				UMETA(DisplayName = "Man Down"),
	AmmoLow				UMETA(DisplayName = "Ammo Low"),
	GrenadeThrowing		UMETA(DisplayName = "Grenade Out"),
	AllClear			UMETA(DisplayName = "All Clear"),
	MovingUp			UMETA(DisplayName = "Moving Up"),
	CoverMe				UMETA(DisplayName = "Cover Me"),
	TargetDown			UMETA(DisplayName = "Target Down"),
	EnemyGrenade		UMETA(DisplayName = "Enemy Grenade"),
	FriendlyFire		UMETA(DisplayName = "Friendly Fire")
};

/* ───────────────────────────────────────────────────────────── */
/*  Delegates                                                   */
/* ───────────────────────────────────────────────────────────── */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSHOnWoundStateChanged, ASHSquadMember*, Member, ESHWoundState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSHOnOrderStateChanged, ASHSquadMember*, Member, ESHOrderAckState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSHOnMoraleChanged, ASHSquadMember*, Member, float, NewMorale);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSHOnContactSpotted, ASHSquadMember*, Member, const FSHContactReport&, Report);

/* ───────────────────────────────────────────────────────────── */
/*  ASHSquadMember — Individual Squad Member Character          */
/* ───────────────────────────────────────────────────────────── */

UCLASS(Blueprintable)
class SHATTEREDHORIZON2032_API ASHSquadMember : public ACharacter
{
	GENERATED_BODY()

public:
	ASHSquadMember();

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual float TakeDamage(float DamageAmount, struct FDamageEvent const& DamageEvent,
							 AController* EventInstigator, AActor* DamageCauser) override;

	/* ── Internal tick sub-routines ─────────────────────────── */
	void UpdateSuppression(float DeltaTime);
	void UpdateMorale(float DeltaTime);
	void UpdateCombatStress(float DeltaTime);
	void UpdateAutonomousBehavior(float DeltaTime);
	void UpdateAmmoConservation(float DeltaTime);

public:
	/* ── Identity & Role ────────────────────────────────────── */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Identity")
	FName MemberName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Identity")
	FName Callsign;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Identity")
	ESHSquadRole Role = ESHSquadRole::Rifleman;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Identity")
	ESHBuddyTeam BuddyTeam = ESHBuddyTeam::Alpha;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Identity")
	int32 SquadIndex = -1;

	/* ── Personality ────────────────────────────────────────── */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Personality")
	FSHPersonalityTraits Personality;

	/* ── Morale & Suppression ───────────────────────────────── */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Morale", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float Morale = 80.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Morale")
	ESHSuppressionLevel SuppressionLevel = ESHSuppressionLevel::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Morale")
	ESHCombatStress CombatStress = ESHCombatStress::Calm;

	/** Raw suppression accumulator (decays over time). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Morale")
	float SuppressionValue = 0.f;

	/** Rate at which suppression decays per second. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Morale")
	float SuppressionDecayRate = 15.f;

	/** Threshold values for suppression levels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Morale")
	float SuppressionLightThreshold = 20.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Morale")
	float SuppressionModerateThreshold = 45.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Morale")
	float SuppressionHeavyThreshold = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Morale")
	float SuppressionPinnedThreshold = 90.f;

	/* ── Wound / Health System ──────────────────────────────── */

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Health")
	ESHWoundState WoundState = ESHWoundState::Healthy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Health")
	float MaxHealth = 100.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Health")
	float CurrentHealth = 100.f;

	/** Health threshold below which the member becomes light-wounded. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Health")
	float LightWoundThreshold = 70.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Health")
	float HeavyWoundThreshold = 35.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Health")
	float IncapacitatedThreshold = 10.f;

	/** Bleedout timer when incapacitated (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Health")
	float BleedoutTimeMax = 60.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Health")
	float BleedoutTimeRemaining = 60.f;

	/** Is this member currently being dragged to cover? */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Health")
	bool bIsBeingDragged = false;

	/** The teammate currently dragging this member. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Health")
	TWeakObjectPtr<ASHSquadMember> DraggedBy = nullptr;

	/* ── Ammunition ─────────────────────────────────────────── */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Ammo")
	FSHAmmoState AmmoState;

	/** When ammo fraction falls below this, AI conserves rounds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Ammo")
	float AmmoConservationThreshold = 0.3f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Ammo")
	bool bIsConservingAmmo = false;

	/* ── Current Order ──────────────────────────────────────── */

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Order")
	FSHSquadOrder CurrentOrder;

	/** Queue of pending orders if current cannot be overridden. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Order")
	TArray<FSHSquadOrder> OrderQueue;

	/* ── Combat State ───────────────────────────────────────── */

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Combat")
	bool bIsInCover = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Combat")
	bool bIsReloading = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Combat")
	bool bIsFiring = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Combat")
	bool bHasLineOfSightToEnemy = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Combat")
	TWeakObjectPtr<AActor> CurrentTarget = nullptr;

	/** Time since last shot fired (for burst control). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Combat")
	float TimeSinceLastShot = 0.f;

	/** Time since last received incoming fire. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Combat")
	float TimeSinceLastIncoming = 999.f;

	/* ── Voice / Callout System ─────────────────────────────── */

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Voice")
	TMap<ESHVoiceLineType, USoundCue*> VoiceLines;

	/** Calm variant voice lines — used under low stress. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Voice")
	TMap<ESHVoiceLineType, USoundCue*> VoiceLinesCalmVariant;

	/** Stressed variant voice lines — used under high stress. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Voice")
	TMap<ESHVoiceLineType, USoundCue*> VoiceLinesStressedVariant;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Voice")
	float VoiceCooldownRemaining = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Voice")
	float VoiceCooldownDuration = 2.f;

	/* ── Autonomous Behavior Timers ─────────────────────────── */

	/** How long without orders before the AI starts acting on its own. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Autonomous")
	float AutonomousActionDelay = 3.f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Autonomous")
	float TimeSinceLastOrder = 0.f;

	/* ── Effectiveness Modifier ─────────────────────────────── */

	/** Overall effectiveness multiplier [0..1] derived from wounds, morale, suppression. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Stats")
	float EffectivenessModifier = 1.f;

	/* ── Audio Component ────────────────────────────────────── */

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Audio")
	TObjectPtr<UAudioComponent> VoiceAudioComponent;

	/* ── Delegates ──────────────────────────────────────────── */

	UPROPERTY(BlueprintAssignable, Category = "Squad|Events")
	FSHOnWoundStateChanged OnWoundStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Squad|Events")
	FSHOnOrderStateChanged OnOrderStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "Squad|Events")
	FSHOnMoraleChanged OnMoraleChanged;

	UPROPERTY(BlueprintAssignable, Category = "Squad|Events")
	FSHOnContactSpotted OnContactSpotted;

	/* ────────────────────────────────────────────────────────── */
	/*  Public Interface                                         */
	/* ────────────────────────────────────────────────────────── */

	/** Receive and process a squad order. */
	UFUNCTION(BlueprintCallable, Category = "Squad|Order")
	void ReceiveOrder(const FSHSquadOrder& InOrder);

	/** Report current order acknowledgment state. */
	UFUNCTION(BlueprintCallable, Category = "Squad|Order")
	void SetOrderAckState(ESHOrderAckState NewState);

	/** Apply suppression from incoming fire. */
	UFUNCTION(BlueprintCallable, Category = "Squad|Combat")
	void ApplySuppression(float Amount, const FVector& SourceLocation);

	/** Apply damage and update wound state. */
	UFUNCTION(BlueprintCallable, Category = "Squad|Health")
	void ApplyWound(float Damage);

	/** Attempt to stabilize / heal (buddy aid). */
	UFUNCTION(BlueprintCallable, Category = "Squad|Health")
	bool ApplyBuddyAid(float HealAmount);

	/** Begin dragging an incapacitated teammate. */
	UFUNCTION(BlueprintCallable, Category = "Squad|Health")
	void StartDragging(ASHSquadMember* WoundedMember);

	/** Stop dragging. */
	UFUNCTION(BlueprintCallable, Category = "Squad|Health")
	void StopDragging();

	/** Fire weapon (single round). Returns true if round was fired. */
	UFUNCTION(BlueprintCallable, Category = "Squad|Combat")
	bool FireWeapon();

	/** Initiate reload sequence. */
	UFUNCTION(BlueprintCallable, Category = "Squad|Combat")
	void BeginReload();

	/** Complete reload (called after animation/timer). */
	UFUNCTION(BlueprintCallable, Category = "Squad|Combat")
	void FinishReload();

	/** Play a voice line with stress-dependent variant selection. */
	UFUNCTION(BlueprintCallable, Category = "Squad|Voice")
	void PlayVoiceLine(ESHVoiceLineType LineType);

	/** Report a contact to the squad. */
	UFUNCTION(BlueprintCallable, Category = "Squad|Combat")
	void ReportContact(AActor* SpottedEnemy, const FVector& EnemyLocation);

	/** Get the movement speed modifier based on current state. */
	UFUNCTION(BlueprintPure, Category = "Squad|Stats")
	float GetMovementSpeedModifier() const;

	/** Get accuracy modifier based on suppression, wounds, stress. */
	UFUNCTION(BlueprintPure, Category = "Squad|Stats")
	float GetAccuracyModifier() const;

	/** Is this member combat-effective? */
	UFUNCTION(BlueprintPure, Category = "Squad|Stats")
	bool IsCombatEffective() const;

	/** Is this member alive? */
	UFUNCTION(BlueprintPure, Category = "Squad|Stats")
	bool IsAlive() const;

	/** Can this member currently execute orders? */
	UFUNCTION(BlueprintPure, Category = "Squad|Stats")
	bool CanExecuteOrders() const;

	/** Get optimal engagement range based on role/weapon. */
	UFUNCTION(BlueprintPure, Category = "Squad|Combat")
	float GetOptimalEngagementRange() const;

	/** Get maximum engagement range based on role/weapon. */
	UFUNCTION(BlueprintPure, Category = "Squad|Combat")
	float GetMaxEngagementRange() const;

	/** Calculate desired burst length based on role, ammo, suppression target. */
	UFUNCTION(BlueprintPure, Category = "Squad|Combat")
	int32 GetDesiredBurstLength() const;

	/** Get the buddy team partner. */
	UFUNCTION(BlueprintPure, Category = "Squad|Identity")
	ASHSquadMember* GetBuddyTeamPartner() const;

	/** Owning squad manager. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Manager")
	TWeakObjectPtr<USHSquadManager> OwningSquadManager;

private:
	void RecalculateEffectiveness();
	void UpdateWoundState();

	/** Dragging reference — who this member is currently dragging. */
	UPROPERTY()
	TWeakObjectPtr<ASHSquadMember> DraggingTarget = nullptr;

	/** Reload timer handle. */
	FTimerHandle ReloadTimerHandle;
};
