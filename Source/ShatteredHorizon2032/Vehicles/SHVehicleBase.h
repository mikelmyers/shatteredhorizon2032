// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "SHVehicleBase.generated.h"

class USkeletalMeshComponent;
class UStaticMeshComponent;
class UChaosVehicleMovementComponent;
class UNiagaraSystem;
class UNiagaraComponent;
class USHVehicleSeatSystem;

/* -----------------------------------------------------------------------
 *  Enums
 * --------------------------------------------------------------------- */

UENUM(BlueprintType)
enum class ESHVehicleType : uint8
{
	Technical          UMETA(DisplayName = "Technical"),
	APC                UMETA(DisplayName = "APC"),
	MRAP               UMETA(DisplayName = "MRAP"),
	StaticEmplacement  UMETA(DisplayName = "Static Emplacement"),
	BoatLanding        UMETA(DisplayName = "Landing Boat"),
};

UENUM(BlueprintType)
enum class ESHVehicleMovementState : uint8
{
	Operational  UMETA(DisplayName = "Operational"),
	Damaged      UMETA(DisplayName = "Damaged"),
	Immobilized  UMETA(DisplayName = "Immobilized"),
	Destroyed    UMETA(DisplayName = "Destroyed"),
};

/* -----------------------------------------------------------------------
 *  Damage zone struct
 * --------------------------------------------------------------------- */

USTRUCT(BlueprintType)
struct FSHVehicleDamageZone
{
	GENERATED_BODY()

	/** Identifier for this zone (e.g. "Engine", "Hull", "Turret"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|DamageZone")
	FName ZoneName = NAME_None;

	/** Maximum health for this zone. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|DamageZone", meta = (ClampMin = "1"))
	float MaxHealth = 100.0f;

	/** Current health. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|DamageZone")
	float CurrentHealth = 100.0f;

	/**
	 * Armor rating for penetration checks.
	 * Incoming PenetrationRating must meet or exceed this to deal full damage.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|DamageZone", meta = (ClampMin = "0"))
	int32 ArmorRating = 0;

	/** True when CurrentHealth <= 0. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|DamageZone")
	bool bIsDisabled = false;

	/** Multiplier applied to incoming damage for this zone (e.g. 2.0 for fuel tank). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|DamageZone", meta = (ClampMin = "0"))
	float DamageMultiplier = 1.0f;

	/** Socket / bone on the skeletal mesh that defines the spatial region. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|DamageZone")
	FName AttachedBone = NAME_None;
};

/* -----------------------------------------------------------------------
 *  Delegates
 * --------------------------------------------------------------------- */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSHOnVehicleDestroyed, ASHVehicleBase*, Vehicle);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSHOnVehicleZoneDisabled, ASHVehicleBase*, Vehicle, FName, ZoneName);

/* -----------------------------------------------------------------------
 *  ASHVehicleBase
 * --------------------------------------------------------------------- */

UCLASS(Abstract, Blueprintable)
class SHATTEREDHORIZON2032_API ASHVehicleBase : public APawn
{
	GENERATED_BODY()

public:
	ASHVehicleBase();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	/* --- Damage Interface --- */

	virtual float TakeDamage(
		float DamageAmount,
		const FDamageEvent& DamageEvent,
		AController* EventInstigator,
		AActor* DamageCauser) override;

	/**
	 * Apply damage directly to a named zone with a penetration rating.
	 * If PenetrationRating < zone ArmorRating, damage is heavily reduced.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vehicle|Damage")
	void ApplyDamageToZone(FName Zone, float Damage, int32 PenetrationRating);

	/* --- Queries --- */

	UFUNCTION(BlueprintPure, Category = "Vehicle|State")
	ESHVehicleMovementState GetVehicleState() const { return MovementState; }

	UFUNCTION(BlueprintPure, Category = "Vehicle|State")
	float GetZoneHealth(FName Zone) const;

	UFUNCTION(BlueprintPure, Category = "Vehicle|State")
	float GetZoneHealthPercent(FName Zone) const;

	UFUNCTION(BlueprintPure, Category = "Vehicle|State")
	bool IsOperational() const { return MovementState == ESHVehicleMovementState::Operational; }

	UFUNCTION(BlueprintPure, Category = "Vehicle|State")
	bool IsDestroyed() const { return MovementState == ESHVehicleMovementState::Destroyed; }

	UFUNCTION(BlueprintPure, Category = "Vehicle|Identity")
	ESHVehicleType GetVehicleType() const { return VehicleType; }

	/** Get the total aggregated health percentage across all zones. */
	UFUNCTION(BlueprintPure, Category = "Vehicle|State")
	float GetOverallHealthPercent() const;

	/* --- Delegates --- */

	UPROPERTY(BlueprintAssignable, Category = "Vehicle|Events")
	FSHOnVehicleDestroyed OnVehicleDestroyed;

	UPROPERTY(BlueprintAssignable, Category = "Vehicle|Events")
	FSHOnVehicleZoneDisabled OnZoneDisabled;

	/* --- Components (public for BP access) --- */

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Components")
	TObjectPtr<USkeletalMeshComponent> VehicleMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Vehicle|Components")
	TObjectPtr<USHVehicleSeatSystem> SeatSystem;

protected:
	/* --- Configuration --- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Identity")
	ESHVehicleType VehicleType = ESHVehicleType::Technical;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Identity")
	FText VehicleDisplayName;

	/** Per-zone health/armor configuration. Populated in editor or constructor. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Damage")
	TArray<FSHVehicleDamageZone> DamageZones;

	/** Speed multiplier when in Damaged state (0-1). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Movement", meta = (ClampMin = "0", ClampMax = "1"))
	float DamagedSpeedMultiplier = 0.5f;

	/** Total hull health threshold (percent) below which the vehicle enters Damaged state. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Damage", meta = (ClampMin = "0", ClampMax = "1"))
	float DamagedThresholdPercent = 0.5f;

	/* --- Destruction VFX --- */

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Destruction")
	TSoftObjectPtr<UNiagaraSystem> DestructionExplosionVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Destruction")
	TSoftObjectPtr<UNiagaraSystem> BurningFireVFX;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Destruction")
	TSoftObjectPtr<USoundBase> DestructionExplosionSound;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Destruction")
	TSoftObjectPtr<USoundBase> BurningLoopSound;

	/** Wreck static mesh swapped in after destruction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Destruction")
	TSoftObjectPtr<UStaticMesh> WreckMesh;

	/** Time in seconds from fuel fire ignition to full destruction. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Vehicle|Destruction", meta = (ClampMin = "1"))
	float FuelFireDestructionTime = 15.0f;

	/* --- Runtime State --- */

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Vehicle|State")
	ESHVehicleMovementState MovementState = ESHVehicleMovementState::Operational;

	/** Active destruction fire VFX component (spawned at runtime). */
	UPROPERTY()
	TObjectPtr<UNiagaraComponent> ActiveFireVFX;

	/** True when the fuel zone is on fire and counting down to destruction. */
	bool bFuelFireActive = false;

	/** Elapsed seconds since fuel fire started. */
	float FuelFireElapsed = 0.0f;

	/* --- Internal Helpers --- */

	/** Determine which damage zone was hit based on a world-space hit location. */
	FName ResolveZoneFromHit(const FHitResult& HitResult) const;

	/** Find a damage zone by name. Returns nullptr if not found. */
	FSHVehicleDamageZone* FindZone(FName ZoneName);
	const FSHVehicleDamageZone* FindZone(FName ZoneName) const;

	/** Recalculate movement state based on current zone health. */
	void RecalculateMovementState();

	/** Called when a zone transitions to disabled. */
	void HandleZoneDisabled(FName ZoneName);

	/** Initiate the destruction sequence (VFX, mesh swap, eject crew). */
	void BeginDestructionSequence();

	/** Start fuel fire (gradual burn leading to destruction). */
	void StartFuelFire();

	/** Tick fuel fire countdown. */
	void TickFuelFire(float DeltaTime);

	/** Populate the default damage zones for this vehicle type. */
	void InitializeDefaultZones();
};
