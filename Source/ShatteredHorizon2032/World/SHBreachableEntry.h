// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Core/SHInteractable.h"
#include "SHBreachableEntry.generated.h"

// -----------------------------------------------------------------------
//  Breach method
// -----------------------------------------------------------------------

UENUM(BlueprintType)
enum class ESHBreachMethod : uint8
{
	Open        UMETA(DisplayName = "Open (Quiet)"),
	Kick        UMETA(DisplayName = "Kick"),
	Shotgun     UMETA(DisplayName = "Shotgun Breach"),
	Explosive   UMETA(DisplayName = "Explosive Breach"),
	PickLock    UMETA(DisplayName = "Pick Lock")
};

/** Door/entry state. */
UENUM(BlueprintType)
enum class ESHDoorState : uint8
{
	Closed      UMETA(DisplayName = "Closed"),
	Locked      UMETA(DisplayName = "Locked"),
	Opening     UMETA(DisplayName = "Opening"),
	Open        UMETA(DisplayName = "Open"),
	Breached    UMETA(DisplayName = "Breached / Destroyed"),
	Barricaded  UMETA(DisplayName = "Barricaded")
};

// -----------------------------------------------------------------------
//  Delegates
// -----------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnDoorStateChanged, ESHDoorState, NewState, ESHBreachMethod, Method);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnBreachStarted, ESHBreachMethod, Method);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnBreachComplete);

// -----------------------------------------------------------------------
//  ASHBreachableEntry
// -----------------------------------------------------------------------

/**
 * ASHBreachableEntry
 *
 * A door, gate, or entry point that can be opened normally, kicked,
 * breached with a shotgun or explosive, or picked. Each method has
 * different noise levels, speed, and gameplay implications.
 *
 * Implements ISHInteractable for player interaction and exposes
 * an AI interface for squad breach procedures.
 *
 * Noise levels affect AI detection:
 * - Open: near-silent
 * - Kick: moderate (audible ~15m)
 * - Shotgun: loud (audible ~50m)
 * - Explosive: very loud (audible ~100m+)
 */
UCLASS(BlueprintType, Blueprintable)
class SHATTEREDHORIZON2032_API ASHBreachableEntry : public AActor, public ISHInteractable
{
	GENERATED_BODY()

public:
	ASHBreachableEntry();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;

	// ------------------------------------------------------------------
	//  ISHInteractable
	// ------------------------------------------------------------------

	virtual bool Interact_Implementation(APlayerController* Interactor) override;
	virtual FText GetInteractionPrompt_Implementation() const override;
	virtual bool CanInteract_Implementation(APlayerController* Interactor) const override;

	// ------------------------------------------------------------------
	//  Breach interface
	// ------------------------------------------------------------------

	/** Attempt to breach with a given method. Returns true if breach began. */
	UFUNCTION(BlueprintCallable, Category = "SH|Breach")
	bool AttemptBreach(ESHBreachMethod Method, AController* Instigator);

	/** Get current door state. */
	UFUNCTION(BlueprintPure, Category = "SH|Breach")
	ESHDoorState GetDoorState() const { return DoorState; }

	/** Whether this entry can be breached with a given method. */
	UFUNCTION(BlueprintPure, Category = "SH|Breach")
	bool CanBreach(ESHBreachMethod Method) const;

	/** Get the stacking positions for AI breach procedure (2 positions, one each side). */
	UFUNCTION(BlueprintCallable, Category = "SH|Breach")
	void GetStackPositions(FVector& OutLeft, FVector& OutRight) const;

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Is this door initially locked? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Breach")
	bool bStartsLocked = false;

	/** Is this door barricaded (requires explosive breach)? */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Breach")
	bool bStartsBarricaded = false;

	/** Health of the door (for breaching). 0 = destroyed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Breach", meta = (ClampMin = "0"))
	float DoorHealth = 100.0f;

	/** Max door health. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Breach", meta = (ClampMin = "1"))
	float MaxDoorHealth = 100.0f;

	/** Time to open quietly (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Breach|Timing", meta = (ClampMin = "0.1"))
	float QuietOpenTime = 1.5f;

	/** Time for a kick breach (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Breach|Timing", meta = (ClampMin = "0.1"))
	float KickBreachTime = 0.5f;

	/** Time to plant and detonate an explosive breach (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Breach|Timing", meta = (ClampMin = "0.5"))
	float ExplosiveBreachTime = 3.0f;

	/** Noise radius for each breach method (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Breach|Noise")
	float OpenNoiseRadius = 300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Breach|Noise")
	float KickNoiseRadius = 1500.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Breach|Noise")
	float ShotgunNoiseRadius = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Breach|Noise")
	float ExplosiveNoiseRadius = 10000.0f;

	/** Damage dealt to enemies behind the door on explosive breach. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Breach|Damage", meta = (ClampMin = "0"))
	float ExplosiveBreachDamage = 80.0f;

	/** Explosive breach stun radius (flashbang-like effect). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Breach|Damage", meta = (ClampMin = "0"))
	float ExplosiveStunRadius = 400.0f;

	// ------------------------------------------------------------------
	//  Components
	// ------------------------------------------------------------------

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|Components")
	TObjectPtr<UStaticMeshComponent> DoorFrame;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "SH|Components")
	TObjectPtr<UStaticMeshComponent> DoorPanel;

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Breach")
	FOnDoorStateChanged OnDoorStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "SH|Breach")
	FOnBreachStarted OnBreachStarted;

	UPROPERTY(BlueprintAssignable, Category = "SH|Breach")
	FOnBreachComplete OnBreachComplete;

	// ------------------------------------------------------------------
	//  Audio (set in Blueprint)
	// ------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio")
	TObjectPtr<USoundBase> OpenSound;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio")
	TObjectPtr<USoundBase> KickSound;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio")
	TObjectPtr<USoundBase> ShotgunBreachSound;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio")
	TObjectPtr<USoundBase> ExplosiveBreachSound;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Audio")
	TObjectPtr<USoundBase> LockedSound;

protected:
	/** Complete the breach after the timer expires. */
	void CompleteBreach();

	/** Make noise for AI detection. */
	void MakeBreachNoise(float Radius);

	UPROPERTY(BlueprintReadOnly, Category = "SH|Breach")
	ESHDoorState DoorState = ESHDoorState::Closed;

	/** Active breach method (while breaching). */
	ESHBreachMethod ActiveBreachMethod = ESHBreachMethod::Open;

	/** Timer for breach in progress. */
	float BreachTimer = 0.0f;
	float BreachDuration = 0.0f;
	bool bBreachInProgress = false;

	/** Door swing angle for opening animation. */
	float CurrentSwingAngle = 0.0f;
	float TargetSwingAngle = 0.0f;

	/** Instigator of the current breach. */
	UPROPERTY()
	TWeakObjectPtr<AController> BreachInstigator;
};
