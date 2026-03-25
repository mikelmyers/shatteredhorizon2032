// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SHDamageSystem.h"
#include "SHDeathSystem.generated.h"

class USkeletalMeshComponent;
class UCapsuleComponent;

// ─────────────────────────────────────────────────────────────
//  Enums
// ─────────────────────────────────────────────────────────────

/** Contextual death animation/physics response type. */
UENUM(BlueprintType)
enum class ESHDeathResponseType : uint8
{
	HeadSnap		UMETA(DisplayName = "Head Snap-Back"),
	TorsoSlump		UMETA(DisplayName = "Torso Slump/Crumple"),
	LegCollapse		UMETA(DisplayName = "Leg Collapse Forward"),
	ExplosiveThrow	UMETA(DisplayName = "Explosive Throw"),
	GenericFall		UMETA(DisplayName = "Generic Fall")
};

// ─────────────────────────────────────────────────────────────
//  Structs
// ─────────────────────────────────────────────────────────────

/** Full context of a death event used to drive ragdoll physics. */
USTRUCT(BlueprintType)
struct FSHDeathContext
{
	GENERATED_BODY()

	/** The hit zone of the killing blow. */
	UPROPERTY(BlueprintReadOnly, Category = "Death")
	ESHHitZone KillingBlowZone = ESHHitZone::Torso;

	/** Damage type of the killing blow. */
	UPROPERTY(BlueprintReadOnly, Category = "Death")
	ESHDamageType KillingBlowDamageType = ESHDamageType::Ballistic;

	/** World-space direction the killing blow came from (normalized). */
	UPROPERTY(BlueprintReadOnly, Category = "Death")
	FVector DamageDirection = FVector::ZeroVector;

	/** World-space impact location. */
	UPROPERTY(BlueprintReadOnly, Category = "Death")
	FVector ImpactLocation = FVector::ZeroVector;

	/** Base damage of the killing blow. */
	UPROPERTY(BlueprintReadOnly, Category = "Death")
	float BaseDamage = 0.f;

	/** Projectile velocity at impact (cm/s). Zero if not a projectile hit. */
	UPROPERTY(BlueprintReadOnly, Category = "Death")
	FVector ProjectileVelocity = FVector::ZeroVector;

	/** Projectile mass in grams. Zero if not a projectile hit. */
	UPROPERTY(BlueprintReadOnly, Category = "Death")
	float ProjectileMassGrams = 0.f;

	/** Computed death response type. */
	UPROPERTY(BlueprintReadOnly, Category = "Death")
	ESHDeathResponseType ResponseType = ESHDeathResponseType::GenericFall;

	/** World time the death occurred. */
	UPROPERTY(BlueprintReadOnly, Category = "Death")
	float DeathTime = 0.f;
};

/** Per-zone impulse tuning. */
USTRUCT(BlueprintType)
struct FSHDeathImpulseConfig
{
	GENERATED_BODY()

	/** Scalar multiplier applied to the computed physics impulse for this zone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death", meta = (ClampMin = "0"))
	float ImpulseScale = 1.f;

	/** Additional upward impulse component (cm/s) to prevent bodies from sliding flat. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death", meta = (ClampMin = "0"))
	float UpwardBias = 0.f;
};

// ─────────────────────────────────────────────────────────────
//  Delegates
// ─────────────────────────────────────────────────────────────

/** Broadcast when the ragdoll has fully stabilized and the body is persistent. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnDeathComplete,
	const FSHDeathContext&, DeathContext);

/** Broadcast immediately when death is executed (before stabilization). */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOnDeathStarted,
	const FSHDeathContext&, DeathContext);

// ─────────────────────────────────────────────────────────────
//  Component
// ─────────────────────────────────────────────────────────────

/**
 * USHDeathSystem
 *
 * Handles the animation-to-ragdoll transition at character death with
 * zero-blend-time physics activation, projectile momentum transfer,
 * contextual death responses, ragdoll stabilization, and permanent
 * body persistence.
 *
 * Design doctrine:
 *  - Zero blend time: instant physics takeover, no animation blend
 *  - Momentum transfer: killing projectile velocity/mass -> ragdoll impulse
 *  - Contextual responses: head = snap-back, chest = crumple, leg = collapse
 *  - Mass-distribution-aware fall behavior
 *  - Body persistence: no despawn, world state persistence
 *  - Ragdoll stabilization: lock physics after settling to prevent jitter
 *  - Zero tolerance: no floating bodies, T-poses, or wall-leaning corpses
 */
UCLASS(ClassGroup = (Combat), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHDeathSystem : public UActorComponent
{
	GENERATED_BODY()

public:
	USHDeathSystem();

	// ------------------------------------------------------------------
	//  UActorComponent overrides
	// ------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ------------------------------------------------------------------
	//  Death execution
	// ------------------------------------------------------------------

	/**
	 * Main entry point. Triggers immediate ragdoll with physics-driven
	 * death response based on the killing blow context.
	 * Call this instead of (or from) ASHPlayerCharacter::Die() / ASHEnemyCharacter::Die().
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Death")
	void ExecuteDeath(const FSHDamageInfo& KillingBlow);

	// ------------------------------------------------------------------
	//  Projectile momentum (set before death by weapon/projectile system)
	// ------------------------------------------------------------------

	/**
	 * Store the killing projectile's velocity and mass for impulse calculation.
	 * Must be called before ExecuteDeath for momentum transfer to apply.
	 * @param Velocity  Projectile velocity at moment of impact (cm/s).
	 * @param MassGrams Projectile mass in grams.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Death")
	void SetProjectileMomentum(const FVector& Velocity, float MassGrams);

	// ------------------------------------------------------------------
	//  Queries
	// ------------------------------------------------------------------

	/** Determine the contextual death response type for a given hit zone. */
	UFUNCTION(BlueprintPure, Category = "SH|Death")
	ESHDeathResponseType GetDeathResponseType(ESHHitZone Zone) const;

	UFUNCTION(BlueprintPure, Category = "SH|Death")
	bool IsDead() const { return bIsDead; }

	UFUNCTION(BlueprintPure, Category = "SH|Death")
	bool IsRagdollStabilized() const { return bRagdollStabilized; }

	UFUNCTION(BlueprintPure, Category = "SH|Death")
	const FSHDeathContext& GetDeathContext() const { return DeathContext; }

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	/** Fires immediately when death is executed. */
	UPROPERTY(BlueprintAssignable, Category = "SH|Death")
	FOnDeathStarted OnDeathStarted;

	/** Fires after ragdoll has stabilized and body is persistent. */
	UPROPERTY(BlueprintAssignable, Category = "SH|Death")
	FOnDeathComplete OnDeathComplete;

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Per-zone impulse scaling configuration. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Death|Config")
	TMap<ESHHitZone, FSHDeathImpulseConfig> ZoneImpulseConfigs;

	/** Global impulse multiplier applied after zone-specific scaling. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Death|Config", meta = (ClampMin = "0"))
	float GlobalImpulseScale = 1.f;

	/** Maximum impulse magnitude (cm/s * g) to clamp unreasonable forces. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Death|Config", meta = (ClampMin = "0"))
	float MaxImpulseMagnitude = 150000.f;

	/** Velocity threshold (cm/s) below which the ragdoll is considered settled. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Death|Config", meta = (ClampMin = "0"))
	float SettleVelocityThreshold = 5.f;

	/** Duration (seconds) the ragdoll must remain below velocity threshold to stabilize. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Death|Config", meta = (ClampMin = "0"))
	float SettleDurationRequired = 1.5f;

	/** Maximum time (seconds) before forcing stabilization regardless of velocity. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Death|Config", meta = (ClampMin = "0"))
	float StabilizationTimeout = 10.f;

	/** Bone name targeted for head impulses. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Death|Config")
	FName HeadBoneName = FName(TEXT("head"));

	/** Bone name targeted for torso/chest impulses. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Death|Config")
	FName SpineBoneName = FName(TEXT("spine_03"));

	/** Bone name targeted for pelvis/root impulses. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Death|Config")
	FName PelvisBoneName = FName(TEXT("pelvis"));

	/** Left thigh bone for leg collapse impulse. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Death|Config")
	FName LeftThighBoneName = FName(TEXT("thigh_l"));

	/** Right thigh bone for leg collapse impulse. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Death|Config")
	FName RightThighBoneName = FName(TEXT("thigh_r"));

	/**
	 * Conversion factor from projectile momentum (g * cm/s) to Unreal impulse units.
	 * Tweak this to get the feel right for your character mass.
	 */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Death|Config", meta = (ClampMin = "0"))
	float MomentumToImpulseScale = 0.01f;

	/** Downward trace distance (cm) to detect ground and prevent floating. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Death|Config", meta = (ClampMin = "0"))
	float GroundCheckDistance = 200.f;

	/** Additional downward impulse (cm/s) applied at death to ensure grounding. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Death|Config", meta = (ClampMin = "0"))
	float AntiFloatDownwardImpulse = 500.f;

protected:
	// ------------------------------------------------------------------
	//  Internal methods
	// ------------------------------------------------------------------

	/** Build the full death context from the killing blow and stored momentum. */
	FSHDeathContext BuildDeathContext(const FSHDamageInfo& KillingBlow) const;

	/** Activate ragdoll physics with zero blend time. Disables capsule, animation, movement. */
	void ActivateRagdoll();

	/** Compute and apply physics impulse to ragdoll based on death context. */
	void ApplyDeathImpulse();

	/** Apply contextual forces based on death response type (head snap, leg collapse, etc.). */
	void ApplyContextualForces();

	/** Apply a downward impulse and trace to prevent floating bodies. */
	void EnforceGroundContact();

	/** Tick-driven: monitor ragdoll velocity and stabilize when settled. */
	void TickStabilizationCheck(float DeltaTime);

	/** Lock the ragdoll in place: disable physics sim, set collision to block-all, disable tick. */
	void StabilizeRagdoll();

	/** Validate that no body parts are in an invalid state (T-pose, wall-lean, floating). */
	void ValidateBodyState();

	/** Get the skeletal mesh component from the owning character. */
	USkeletalMeshComponent* GetOwnerMesh() const;

	/** Get the capsule component from the owning character. */
	UCapsuleComponent* GetOwnerCapsule() const;

	/** Get the bone name to target for impulse based on hit zone. */
	FName GetBoneForZone(ESHHitZone Zone) const;

	/** Initialize default zone impulse configs if none are set. */
	void InitDefaultZoneImpulseConfigs();

private:
	// ------------------------------------------------------------------
	//  Runtime state
	// ------------------------------------------------------------------

	/** Whether death has been executed. */
	bool bIsDead = false;

	/** Whether the ragdoll has stabilized (physics locked). */
	bool bRagdollStabilized = false;

	/** Stored projectile velocity for momentum transfer. */
	FVector StoredProjectileVelocity = FVector::ZeroVector;

	/** Stored projectile mass in grams. */
	float StoredProjectileMassGrams = 0.f;

	/** Whether projectile momentum has been set (valid for next death). */
	bool bHasProjectileMomentum = false;

	/** Full context of the death event. */
	FSHDeathContext DeathContext;

	/** Accumulated time the ragdoll has been below the settle velocity threshold. */
	float SettleTimer = 0.f;

	/** Total elapsed time since death (for stabilization timeout). */
	float TimeSinceDeath = 0.f;

	/** Cached mesh reference. */
	UPROPERTY()
	TObjectPtr<USkeletalMeshComponent> CachedMesh = nullptr;
};
