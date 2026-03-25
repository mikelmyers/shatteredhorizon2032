// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "ShatteredHorizon2032/AI/SHEnemyCharacter.h"
#include "SHSquadPersonality.generated.h"

class UAudioComponent;
class USoundBase;

DECLARE_LOG_CATEGORY_EXTERN(LogSHSquadPersonality, Log, All);

// ─────────────────────────────────────────────────────────────
//  Enums
// ─────────────────────────────────────────────────────────────

/** Archetype personality trait that drives voice line selection and behaviour flavour. */
UENUM(BlueprintType)
enum class ESHPersonalityTrait : uint8
{
	Calm		UMETA(DisplayName = "Calm"),
	Aggressive	UMETA(DisplayName = "Aggressive"),
	Cautious	UMETA(DisplayName = "Cautious"),
	Joker		UMETA(DisplayName = "Joker"),
	Veteran		UMETA(DisplayName = "Veteran")
};

// ─────────────────────────────────────────────────────────────
//  Structs
// ─────────────────────────────────────────────────────────────

/** Personality profile for a named squad member. */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHPersonalityProfile
{
	GENERATED_BODY()

	/** Display name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Personality")
	FText Name;

	/** Radio callsign. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Personality")
	FText Callsign;

	/** Military rank. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Personality")
	FText Rank;

	/** Core personality archetype. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Personality")
	ESHPersonalityTrait PersonalityTrait = ESHPersonalityTrait::Calm;

	/** Index into personality-specific voice set arrays. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Personality")
	int32 VoiceSet = 0;

	/** Short backstory snippet displayed during loading or in the squad menu. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Personality", meta = (MultiLine = "true"))
	FText BackstorySnippet;

	/** Relationship score with the player [0..1]. 0 = hostile, 1 = deep trust. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Personality", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float RelationshipScore = 0.5f;
};

// ─────────────────────────────────────────────────────────────
//  Delegates
// ─────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOnRelationshipChanged,
	USHSquadPersonality*, Component,
	float, OldScore,
	float, NewScore);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnSquadMemberKilled,
	USHSquadPersonality*, Component,
	const FSHPersonalityProfile&, Profile);

// ─────────────────────────────────────────────────────────────
//  USHSquadPersonality — Actor Component
// ─────────────────────────────────────────────────────────────

/**
 * USHSquadPersonality
 *
 * Actor component attached to squad member characters that gives
 * them a distinct personality: name, callsign, voice set, backstory,
 * and a relationship score that evolves over the course of a
 * campaign. Personality drives contextual voice line selection,
 * and the relationship score adjusts based on gameplay outcomes.
 *
 * When the owning squad member dies, the component broadcasts its
 * personality profile for narrative consequence systems (memorial
 * screens, morale shifts, dialogue references).
 */
UCLASS(ClassGroup = (ShatteredHorizon), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHSquadPersonality : public UActorComponent
{
	GENERATED_BODY()

public:
	USHSquadPersonality();

protected:
	virtual void BeginPlay() override;

public:
	// ------------------------------------------------------------------
	//  Profile access
	// ------------------------------------------------------------------

	/** Get the full personality profile. */
	UFUNCTION(BlueprintPure, Category = "SH|Personality")
	const FSHPersonalityProfile& GetProfile() const { return Profile; }

	/** Set a new personality profile (typically at spawn / load). */
	UFUNCTION(BlueprintCallable, Category = "SH|Personality")
	void SetProfile(const FSHPersonalityProfile& InProfile);

	// ------------------------------------------------------------------
	//  Relationship tracking
	// ------------------------------------------------------------------

	/** Get the current relationship score [0..1]. */
	UFUNCTION(BlueprintPure, Category = "SH|Personality")
	float GetRelationshipScore() const { return Profile.RelationshipScore; }

	/**
	 * Modify the relationship score by a signed delta.
	 * Positive values improve the relationship, negative values degrade it.
	 * The result is clamped to [0..1].
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Personality")
	void ModifyRelationship(float Delta);

	/** Record a successful operation — improves relationship. */
	UFUNCTION(BlueprintCallable, Category = "SH|Personality")
	void RecordSuccessfulOperation();

	/** Record a friendly-fire incident — degrades relationship. */
	UFUNCTION(BlueprintCallable, Category = "SH|Personality")
	void RecordFriendlyFire();

	/** Record a failed order — degrades relationship. */
	UFUNCTION(BlueprintCallable, Category = "SH|Personality")
	void RecordFailedOrder();

	// ------------------------------------------------------------------
	//  Voice lines
	// ------------------------------------------------------------------

	/**
	 * Play a contextual voice line determined by the personality trait
	 * and the combat context.
	 *
	 * @param Context  The combat context that triggered the voice line.
	 * @return True if a voice line was found and playback started.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Personality")
	bool PlayContextualVoiceLine(ESHVoiceLineContext Context);

	/** Get all voice lines available for a given context. */
	UFUNCTION(BlueprintPure, Category = "SH|Personality")
	TArray<TSoftObjectPtr<USoundBase>> GetVoiceLinesForContext(ESHVoiceLineContext Context) const;

	// ------------------------------------------------------------------
	//  Death
	// ------------------------------------------------------------------

	/**
	 * Notify the system that this squad member has been killed.
	 * Broadcasts the OnSquadMemberKilled delegate with the full profile
	 * for downstream narrative systems.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Personality")
	void NotifyKilled();

	/** Has this squad member been marked as killed? */
	UFUNCTION(BlueprintPure, Category = "SH|Personality")
	bool IsKilled() const { return bIsKilled; }

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Personality")
	FOnRelationshipChanged OnRelationshipChanged;

	UPROPERTY(BlueprintAssignable, Category = "SH|Personality")
	FOnSquadMemberKilled OnSquadMemberKilled;

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** The personality profile for this squad member. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Personality")
	FSHPersonalityProfile Profile;

	/**
	 * Voice line sets keyed by combat context. Each context maps to an
	 * array of soft sound references. At playback time one is selected
	 * based on the personality trait and randomization.
	 *
	 * Reuses ESHVoiceLineContext from SHEnemyCharacter (ContactFront,
	 * Reloading, Suppressed, ManDown, etc.).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Personality|Voice")
	TMap<ESHVoiceLineContext, TArray<TSoftObjectPtr<USoundBase>>> VoiceLineSets;

	/** Minimum cooldown between voice lines (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Personality|Voice", meta = (ClampMin = "0.0"))
	float VoiceLineCooldown = 2.5f;

protected:
	// ------------------------------------------------------------------
	//  Configuration — relationship deltas
	// ------------------------------------------------------------------

	/** Relationship improvement per successful operation. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Personality|Config")
	float SuccessfulOperationDelta = 0.05f;

	/** Relationship penalty per friendly-fire incident. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Personality|Config")
	float FriendlyFirePenalty = -0.15f;

	/** Relationship penalty per failed order. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Personality|Config")
	float FailedOrderPenalty = -0.08f;

private:
	// ------------------------------------------------------------------
	//  Internal helpers
	// ------------------------------------------------------------------

	/** Select a voice line from the available set for the given context. */
	TSoftObjectPtr<USoundBase> SelectVoiceLine(ESHVoiceLineContext Context) const;

	/** Async-load and play a soft-referenced sound. */
	void LoadAndPlaySound(const TSoftObjectPtr<USoundBase>& SoundRef);

	// ------------------------------------------------------------------
	//  Runtime state
	// ------------------------------------------------------------------

	/** Remaining cooldown before another voice line can be played. */
	float VoiceCooldownRemaining = 0.f;

	/** Has this member been killed? */
	bool bIsKilled = false;

	/** Audio component for voice line playback. */
	UPROPERTY()
	TObjectPtr<UAudioComponent> VoiceAudioComponent;

	/** Handle for async audio loading. */
	TSharedPtr<struct FStreamableHandle> VoiceLoadHandle;

	/** Timer handle for cooldown tick. */
	FTimerHandle CooldownTimerHandle;
};
