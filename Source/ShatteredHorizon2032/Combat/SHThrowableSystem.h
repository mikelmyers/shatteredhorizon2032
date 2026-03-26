// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SHThrowableSystem.generated.h"

class USHSuppressionSystem;

// -----------------------------------------------------------------------
//  Enums
// -----------------------------------------------------------------------

/** Throwable type determines behavior on detonation. */
UENUM(BlueprintType)
enum class ESHThrowableType : uint8
{
	FragGrenade     UMETA(DisplayName = "Fragmentation Grenade"),
	SmokeGrenade    UMETA(DisplayName = "Smoke Grenade"),
	Flashbang       UMETA(DisplayName = "Flashbang"),
	Incendiary      UMETA(DisplayName = "Incendiary"),
	C4Charge        UMETA(DisplayName = "C4 Demolition Charge"),
	Flare           UMETA(DisplayName = "Signal Flare")
};

/** Throwable state machine. */
UENUM(BlueprintType)
enum class ESHThrowableState : uint8
{
	Idle            UMETA(DisplayName = "Idle"),
	Cooking         UMETA(DisplayName = "Cooking"),
	InFlight        UMETA(DisplayName = "In Flight"),
	Bouncing        UMETA(DisplayName = "Bouncing / Rolling"),
	Detonated       UMETA(DisplayName = "Detonated"),
	Dud             UMETA(DisplayName = "Dud")
};

// -----------------------------------------------------------------------
//  Throwable data
// -----------------------------------------------------------------------

/** Configuration for a throwable type. */
USTRUCT(BlueprintType)
struct FSHThrowableData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throwable")
	ESHThrowableType Type = ESHThrowableType::FragGrenade;

	/** Fuse time in seconds (from pin pull, not release). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throwable", meta = (ClampMin = "0.1"))
	float FuseTime = 4.0f;

	/** Maximum throw velocity (cm/s). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throwable", meta = (ClampMin = "100"))
	float MaxThrowSpeed = 2000.0f;

	/** Underhand throw speed (fraction of max). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throwable", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float UnderhandSpeedFraction = 0.4f;

	/** Mass in grams (affects throw arc and bounce). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throwable", meta = (ClampMin = "50"))
	float MassGrams = 400.0f;

	/** Coefficient of restitution for bouncing (0 = no bounce, 1 = perfect). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throwable", meta = (ClampMin = "0", ClampMax = "0.8"))
	float BounceCoefficient = 0.3f;

	/** Friction coefficient for rolling/sliding after landing. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throwable", meta = (ClampMin = "0", ClampMax = "1"))
	float FrictionCoefficient = 0.6f;

	// --- Fragmentation (FragGrenade) ---

	/** Lethal radius (cm) — 50% chance of death inside this radius. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throwable|Frag", meta = (ClampMin = "0"))
	float LethalRadiusCm = 500.0f;

	/** Casualty radius (cm) — fragments can wound at this range. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throwable|Frag", meta = (ClampMin = "0"))
	float CasualtyRadiusCm = 1500.0f;

	/** Number of fragments generated. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throwable|Frag", meta = (ClampMin = "0"))
	int32 FragmentCount = 200;

	/** Damage per fragment at point-blank. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throwable|Frag", meta = (ClampMin = "0"))
	float FragmentDamage = 40.0f;

	/** Blast overpressure damage at center. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throwable|Frag", meta = (ClampMin = "0"))
	float BlastDamage = 200.0f;

	// --- Smoke ---

	/** Smoke cloud radius (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throwable|Smoke", meta = (ClampMin = "0"))
	float SmokeRadiusCm = 800.0f;

	/** Smoke duration (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throwable|Smoke", meta = (ClampMin = "0"))
	float SmokeDurationSeconds = 45.0f;

	// --- Flashbang ---

	/** Flashbang blind duration (seconds) at center. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throwable|Flash", meta = (ClampMin = "0"))
	float FlashBlindDuration = 5.0f;

	/** Flashbang deafening duration (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throwable|Flash", meta = (ClampMin = "0"))
	float FlashDeafDuration = 8.0f;

	/** Flashbang effective radius (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throwable|Flash", meta = (ClampMin = "0"))
	float FlashRadiusCm = 600.0f;

	// --- Incendiary ---

	/** Fire radius (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throwable|Incendiary", meta = (ClampMin = "0"))
	float FireRadiusCm = 400.0f;

	/** Burn duration (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throwable|Incendiary", meta = (ClampMin = "0"))
	float BurnDurationSeconds = 15.0f;

	/** Damage per second to entities in fire zone. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Throwable|Incendiary", meta = (ClampMin = "0"))
	float BurnDamagePerSecond = 25.0f;
};

// -----------------------------------------------------------------------
//  Delegates
// -----------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnThrowableDetonated, ESHThrowableType, Type, FVector, Location);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnThrowableCooked, float, RemainingFuse);

// -----------------------------------------------------------------------
//  ASHThrowableProjectile — The actual thrown object
// -----------------------------------------------------------------------

UCLASS(BlueprintType, Blueprintable)
class SHATTEREDHORIZON2032_API ASHThrowableProjectile : public AActor
{
	GENERATED_BODY()

public:
	ASHThrowableProjectile();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/** Initialize and throw with given velocity. */
	UFUNCTION(BlueprintCallable, Category = "SH|Throwable")
	void Throw(const FVector& ThrowVelocity, const FSHThrowableData& InData, AController* InInstigator);

	/** Start cooking (hold grenade with pin pulled). */
	UFUNCTION(BlueprintCallable, Category = "SH|Throwable")
	void StartCooking();

	/** Trigger detonation manually (C4). */
	UFUNCTION(BlueprintCallable, Category = "SH|Throwable")
	void ManualDetonate();

	UFUNCTION(BlueprintPure, Category = "SH|Throwable")
	ESHThrowableState GetState() const { return State; }

	UFUNCTION(BlueprintPure, Category = "SH|Throwable")
	float GetRemainingFuse() const { return RemainingFuseTime; }

	UPROPERTY(BlueprintAssignable, Category = "SH|Throwable")
	FOnThrowableDetonated OnDetonated;

	UPROPERTY(BlueprintAssignable, Category = "SH|Throwable")
	FOnThrowableCooked OnCooked;

protected:
	/** Execute detonation effects based on throwable type. */
	void Detonate();

	/** Apply fragmentation damage (frag grenades). */
	void ApplyFragmentation(const FVector& Origin);

	/** Apply blast overpressure damage. */
	void ApplyBlastDamage(const FVector& Origin);

	/** Spawn smoke cloud (smoke grenades). */
	void SpawnSmokeCloud(const FVector& Origin);

	/** Apply flashbang effect (blind + deafen). */
	void ApplyFlashbangEffect(const FVector& Origin);

	/** Spawn fire zone (incendiary). */
	void SpawnFireZone(const FVector& Origin);

	/** Handle bounce/impact with surfaces. */
	UFUNCTION()
	void OnProjectileHit(UPrimitiveComponent* HitComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit);

	// --- Components ---

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|Components")
	TObjectPtr<UStaticMeshComponent> MeshComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|Components")
	TObjectPtr<class UProjectileMovementComponent> ProjectileMovement;

	// --- State ---

	UPROPERTY(BlueprintReadOnly, Category = "SH|Throwable")
	ESHThrowableState State = ESHThrowableState::Idle;

	UPROPERTY(BlueprintReadOnly, Category = "SH|Throwable")
	FSHThrowableData ThrowableData;

	float RemainingFuseTime = 4.0f;
	float CookStartTime = 0.0f;
	bool bIsCooking = false;
	int32 BounceCount = 0;

	UPROPERTY()
	TObjectPtr<AController> InstigatorController;

	// --- VFX / Audio references (set in Blueprint subclass) ---

	UPROPERTY(EditDefaultsOnly, Category = "SH|VFX")
	TObjectPtr<UParticleSystem> DetonationVFX;

	UPROPERTY(EditDefaultsOnly, Category = "SH|VFX")
	TObjectPtr<UParticleSystem> SmokeVFX;

	UPROPERTY(EditDefaultsOnly, Category = "SH|VFX")
	TObjectPtr<UParticleSystem> FlashVFX;

	UPROPERTY(EditDefaultsOnly, Category = "SH|VFX")
	TObjectPtr<UParticleSystem> FireVFX;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio")
	TObjectPtr<USoundBase> DetonationSound;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio")
	TObjectPtr<USoundBase> BounceSound;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio")
	TObjectPtr<USoundBase> PinPullSound;

	/** Max bounces before grenade settles. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Throwable")
	int32 MaxBounces = 5;
};

// -----------------------------------------------------------------------
//  Default throwable definitions (factory pattern like weapons)
// -----------------------------------------------------------------------

UCLASS()
class SHATTEREDHORIZON2032_API USHThrowableDataFactory : public UObject
{
	GENERATED_BODY()

public:
	/** M67 Fragmentation Grenade — US standard issue. */
	static FSHThrowableData CreateM67FragGrenade();

	/** M18 Smoke Grenade — colored smoke for screening/signaling. */
	static FSHThrowableData CreateM18SmokeGrenade();

	/** M84 Stun Grenade (Flashbang). */
	static FSHThrowableData CreateM84Flashbang();

	/** AN-M14 TH3 Incendiary Grenade — thermite. */
	static FSHThrowableData CreateANM14Incendiary();

	/** M112 C4 Demolition Charge — remote detonation. */
	static FSHThrowableData CreateM112C4();

	/** PLA Type 82 Fragmentation Grenade — enemy throwable. */
	static FSHThrowableData CreateType82FragGrenade();
};
