// Copyright Shattered Horizon 2032. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "SHSquadOrder.generated.h"

/* ───────────────────────────────────────────────────────────── */
/*  Order Type Enumeration                                      */
/* ───────────────────────────────────────────────────────────── */

UENUM(BlueprintType)
enum class ESHOrderType : uint8
{
	None				UMETA(DisplayName = "None"),
	MoveToPosition		UMETA(DisplayName = "Move To Position"),
	HoldPosition		UMETA(DisplayName = "Hold Position"),
	Suppress			UMETA(DisplayName = "Suppress"),
	FlankLeft			UMETA(DisplayName = "Flank Left"),
	FlankRight			UMETA(DisplayName = "Flank Right"),
	FallBack			UMETA(DisplayName = "Fall Back"),
	StackUp				UMETA(DisplayName = "Stack Up"),
	BreachAndClear		UMETA(DisplayName = "Breach And Clear"),
	ProvideOverwatch	UMETA(DisplayName = "Provide Overwatch"),
	CoverMe				UMETA(DisplayName = "Cover Me"),
	CeaseFire			UMETA(DisplayName = "Cease Fire"),
	FreeFire			UMETA(DisplayName = "Free Fire"),
	Rally				UMETA(DisplayName = "Rally"),
	Medic				UMETA(DisplayName = "Medic"),
	MarkTarget			UMETA(DisplayName = "Mark Target")
};

/* ───────────────────────────────────────────────────────────── */
/*  Order Acknowledgment State                                  */
/* ───────────────────────────────────────────────────────────── */

UENUM(BlueprintType)
enum class ESHOrderAckState : uint8
{
	Received	UMETA(DisplayName = "Received"),
	Executing	UMETA(DisplayName = "Executing"),
	Completed	UMETA(DisplayName = "Completed"),
	Failed		UMETA(DisplayName = "Failed"),
	Overridden	UMETA(DisplayName = "Overridden")
};

/* ───────────────────────────────────────────────────────────── */
/*  Order Priority                                              */
/* ───────────────────────────────────────────────────────────── */

UENUM(BlueprintType)
enum class ESHOrderPriority : uint8
{
	Low			UMETA(DisplayName = "Low"),
	Normal		UMETA(DisplayName = "Normal"),
	High		UMETA(DisplayName = "High"),
	Critical	UMETA(DisplayName = "Critical")
};

/* ───────────────────────────────────────────────────────────── */
/*  Squad Order Struct                                          */
/* ───────────────────────────────────────────────────────────── */

USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHSquadOrder
{
	GENERATED_BODY()

	/** The type of order being issued. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Order")
	ESHOrderType OrderType = ESHOrderType::None;

	/** World-space target location for movement / suppression / mark orders. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Order")
	FVector TargetLocation = FVector::ZeroVector;

	/** Optional actor target (enemy to suppress, door to breach, teammate to heal). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Order")
	TWeakObjectPtr<AActor> TargetActor = nullptr;

	/** Priority determines whether this order can override an existing one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Order")
	ESHOrderPriority Priority = ESHOrderPriority::Normal;

	/** Server timestamp when the order was issued (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Order")
	float Timestamp = 0.f;

	/** The actor (player or AI) that issued this order. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Order")
	TWeakObjectPtr<AActor> IssuedBy = nullptr;

	/** Current acknowledgment state of the order. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Order")
	ESHOrderAckState AckState = ESHOrderAckState::Received;

	/** Unique order ID for tracking. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Squad|Order")
	int32 OrderID = 0;

	/** Secondary direction hint (used for flanking maneuvers). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Squad|Order")
	FVector SecondaryDirection = FVector::ZeroVector;

	/* ── Helpers ────────────────────────────────────────────── */

	bool IsValid() const { return OrderType != ESHOrderType::None; }

	bool CanOverride(const FSHSquadOrder& Other) const
	{
		return static_cast<uint8>(Priority) >= static_cast<uint8>(Other.Priority);
	}

	bool IsMovementOrder() const
	{
		return OrderType == ESHOrderType::MoveToPosition
			|| OrderType == ESHOrderType::FlankLeft
			|| OrderType == ESHOrderType::FlankRight
			|| OrderType == ESHOrderType::FallBack
			|| OrderType == ESHOrderType::Rally
			|| OrderType == ESHOrderType::StackUp;
	}

	bool IsCombatOrder() const
	{
		return OrderType == ESHOrderType::Suppress
			|| OrderType == ESHOrderType::FreeFire
			|| OrderType == ESHOrderType::CeaseFire
			|| OrderType == ESHOrderType::ProvideOverwatch
			|| OrderType == ESHOrderType::CoverMe;
	}

	static FSHSquadOrder MakeOrder(ESHOrderType InType, const FVector& InLocation, AActor* InTarget,
									ESHOrderPriority InPriority, AActor* InIssuedBy)
	{
		static int32 GlobalOrderID = 0;
		FSHSquadOrder Order;
		Order.OrderType = InType;
		Order.TargetLocation = InLocation;
		Order.TargetActor = InTarget;
		Order.Priority = InPriority;
		Order.IssuedBy = InIssuedBy;
		Order.Timestamp = 0.f; // Caller should set from GetWorld()->GetTimeSeconds()
		Order.OrderID = ++GlobalOrderID;
		return Order;
	}

	void Reset()
	{
		OrderType = ESHOrderType::None;
		TargetLocation = FVector::ZeroVector;
		TargetActor = nullptr;
		Priority = ESHOrderPriority::Normal;
		AckState = ESHOrderAckState::Received;
		SecondaryDirection = FVector::ZeroVector;
	}
};

/* ───────────────────────────────────────────────────────────── */
/*  Contact Report Struct                                       */
/* ───────────────────────────────────────────────────────────── */

UENUM(BlueprintType)
enum class ESHContactDirection : uint8
{
	Front	UMETA(DisplayName = "Front"),
	Left	UMETA(DisplayName = "Left"),
	Right	UMETA(DisplayName = "Right"),
	Rear	UMETA(DisplayName = "Rear")
};

USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHContactReport
{
	GENERATED_BODY()

	/** The actor who spotted the contact. */
	UPROPERTY(BlueprintReadOnly, Category = "Squad|Contact")
	TWeakObjectPtr<AActor> Reporter = nullptr;

	/** Location of the spotted enemy. */
	UPROPERTY(BlueprintReadOnly, Category = "Squad|Contact")
	FVector ContactLocation = FVector::ZeroVector;

	/** Relative direction from the squad's heading. */
	UPROPERTY(BlueprintReadOnly, Category = "Squad|Contact")
	ESHContactDirection Direction = ESHContactDirection::Front;

	/** Estimated distance in game units. */
	UPROPERTY(BlueprintReadOnly, Category = "Squad|Contact")
	float DistanceMeters = 0.f;

	/** Compass bearing to contact (0-360). */
	UPROPERTY(BlueprintReadOnly, Category = "Squad|Contact")
	float BearingDegrees = 0.f;

	/** How many enemies were spotted in this report. */
	UPROPERTY(BlueprintReadOnly, Category = "Squad|Contact")
	int32 EnemyCount = 1;

	/** Server time of report. */
	UPROPERTY(BlueprintReadOnly, Category = "Squad|Contact")
	float Timestamp = 0.f;

	/** Generates a human-readable callout string. */
	FString GetCalloutString() const
	{
		FString DirStr;
		switch (Direction)
		{
		case ESHContactDirection::Front: DirStr = TEXT("front"); break;
		case ESHContactDirection::Left:  DirStr = TEXT("left");  break;
		case ESHContactDirection::Right: DirStr = TEXT("right"); break;
		case ESHContactDirection::Rear:  DirStr = TEXT("rear");  break;
		}
		return FString::Printf(TEXT("Contact %s! Enemy, %d meters, bearing %03d!"),
			*DirStr, FMath::RoundToInt(DistanceMeters), FMath::RoundToInt(BearingDegrees));
	}
};
