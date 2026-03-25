// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHVehicleSeatSystem.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "ShatteredHorizon2032/ShatteredHorizon2032.h"

/* -----------------------------------------------------------------------
 *  Constructor
 * --------------------------------------------------------------------- */

USHVehicleSeatSystem::USHVehicleSeatSystem()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

/* -----------------------------------------------------------------------
 *  Lifecycle
 * --------------------------------------------------------------------- */

void USHVehicleSeatSystem::BeginPlay()
{
	Super::BeginPlay();

	if (Seats.Num() == 0)
	{
		InitializeDefaultSeats();
	}

	// Clamp to max seats
	if (Seats.Num() > MaxSeatsPerVehicle)
	{
		SH_WARNING(LogSH_Vehicle, "SeatSystem on '%s' has %d seats — clamping to %d",
			*GetOwner()->GetName(), Seats.Num(), MaxSeatsPerVehicle);
		Seats.SetNum(MaxSeatsPerVehicle);
	}

	// Ensure seat indices are correct
	for (int32 i = 0; i < Seats.Num(); ++i)
	{
		Seats[i].SeatIndex = i;
	}
}

/* -----------------------------------------------------------------------
 *  Entry / Exit
 * --------------------------------------------------------------------- */

bool USHVehicleSeatSystem::EnterVehicle(ACharacter* Character, int32 PreferredSeat)
{
	if (!Character)
	{
		SH_WARNING(LogSH_Vehicle, "EnterVehicle: null Character");
		return false;
	}

	// Check if character is already in a seat
	if (FindSeatForCharacter(Character) >= 0)
	{
		SH_WARNING(LogSH_Vehicle, "EnterVehicle: '%s' is already in a seat", *Character->GetName());
		return false;
	}

	// Determine target seat
	int32 TargetSeat = -1;

	if (PreferredSeat >= 0 && IsValidSeatIndex(PreferredSeat) && !Seats[PreferredSeat].IsOccupied())
	{
		TargetSeat = PreferredSeat;
	}
	else
	{
		TargetSeat = FindBestAvailableSeat();
	}

	if (TargetSeat < 0)
	{
		SH_LOG(LogSH_Vehicle, Log, "EnterVehicle: no available seat for '%s' on '%s'",
			*Character->GetName(), *GetOwner()->GetName());
		return false;
	}

	FSHVehicleSeat& Seat = Seats[TargetSeat];
	Seat.Occupant = Character;

	AttachCharacterToSeat(Character, Seat);

	SH_LOG(LogSH_Vehicle, Log, "'%s' entered vehicle '%s' seat %d (%s)",
		*Character->GetName(), *GetOwner()->GetName(), TargetSeat,
		*UEnum::GetValueAsString(Seat.SeatType));

	OnCharacterEnteredVehicle.Broadcast(Character, TargetSeat, Seat.SeatType);

	return true;
}

bool USHVehicleSeatSystem::ExitVehicle(ACharacter* Character)
{
	if (!Character)
	{
		return false;
	}

	const int32 SeatIdx = FindSeatForCharacter(Character);
	if (SeatIdx < 0)
	{
		SH_WARNING(LogSH_Vehicle, "ExitVehicle: '%s' is not in any seat on '%s'",
			*Character->GetName(), *GetOwner()->GetName());
		return false;
	}

	FSHVehicleSeat& Seat = Seats[SeatIdx];
	const ESHSeatType SeatType = Seat.SeatType;

	DetachCharacterFromSeat(Character, Seat);
	Seat.Occupant.Reset();

	SH_LOG(LogSH_Vehicle, Log, "'%s' exited vehicle '%s' seat %d",
		*Character->GetName(), *GetOwner()->GetName(), SeatIdx);

	OnCharacterExitedVehicle.Broadcast(Character, SeatIdx, SeatType);

	return true;
}

bool USHVehicleSeatSystem::SwitchSeat(ACharacter* Character, int32 NewSeat)
{
	if (!Character || !IsValidSeatIndex(NewSeat))
	{
		return false;
	}

	const int32 CurrentSeat = FindSeatForCharacter(Character);
	if (CurrentSeat < 0)
	{
		SH_WARNING(LogSH_Vehicle, "SwitchSeat: '%s' is not in any seat", *Character->GetName());
		return false;
	}

	if (CurrentSeat == NewSeat)
	{
		return true; // Already in that seat
	}

	if (Seats[NewSeat].IsOccupied())
	{
		SH_LOG(LogSH_Vehicle, Log, "SwitchSeat: seat %d is occupied", NewSeat);
		return false;
	}

	// Detach from current seat
	FSHVehicleSeat& OldSeat = Seats[CurrentSeat];
	const ESHSeatType OldType = OldSeat.SeatType;

	// We don't fully detach from the vehicle — just move the attachment point
	OldSeat.Occupant.Reset();
	OnCharacterExitedVehicle.Broadcast(Character, CurrentSeat, OldType);

	// Attach to new seat
	FSHVehicleSeat& NewSeatRef = Seats[NewSeat];
	NewSeatRef.Occupant = Character;

	AttachCharacterToSeat(Character, NewSeatRef);

	SH_LOG(LogSH_Vehicle, Log, "'%s' switched from seat %d to seat %d on '%s'",
		*Character->GetName(), CurrentSeat, NewSeat, *GetOwner()->GetName());

	OnCharacterEnteredVehicle.Broadcast(Character, NewSeat, NewSeatRef.SeatType);

	return true;
}

/* -----------------------------------------------------------------------
 *  Queries
 * --------------------------------------------------------------------- */

ACharacter* USHVehicleSeatSystem::GetOccupant(int32 SeatIndex) const
{
	if (!IsValidSeatIndex(SeatIndex))
	{
		return nullptr;
	}
	return Seats[SeatIndex].Occupant.Get();
}

TArray<int32> USHVehicleSeatSystem::GetAvailableSeats() const
{
	TArray<int32> Available;
	Available.Reserve(Seats.Num());
	for (const FSHVehicleSeat& Seat : Seats)
	{
		if (!Seat.IsOccupied())
		{
			Available.Add(Seat.SeatIndex);
		}
	}
	return Available;
}

bool USHVehicleSeatSystem::IsSeatOccupied(int32 SeatIndex) const
{
	if (!IsValidSeatIndex(SeatIndex))
	{
		return false;
	}
	return Seats[SeatIndex].IsOccupied();
}

int32 USHVehicleSeatSystem::GetDriverSeatIndex() const
{
	for (const FSHVehicleSeat& Seat : Seats)
	{
		if (Seat.SeatType == ESHSeatType::Driver)
		{
			return Seat.SeatIndex;
		}
	}
	return -1;
}

int32 USHVehicleSeatSystem::GetGunnerSeatIndex() const
{
	for (const FSHVehicleSeat& Seat : Seats)
	{
		if (Seat.SeatType == ESHSeatType::Gunner)
		{
			return Seat.SeatIndex;
		}
	}
	return -1;
}

ACharacter* USHVehicleSeatSystem::GetDriver() const
{
	const int32 Idx = GetDriverSeatIndex();
	return (Idx >= 0) ? GetOccupant(Idx) : nullptr;
}

ACharacter* USHVehicleSeatSystem::GetGunner() const
{
	const int32 Idx = GetGunnerSeatIndex();
	return (Idx >= 0) ? GetOccupant(Idx) : nullptr;
}

bool USHVehicleSeatSystem::IsAnyoneInVehicle() const
{
	for (const FSHVehicleSeat& Seat : Seats)
	{
		if (Seat.IsOccupied())
		{
			return true;
		}
	}
	return false;
}

int32 USHVehicleSeatSystem::GetOccupantCount() const
{
	int32 Count = 0;
	for (const FSHVehicleSeat& Seat : Seats)
	{
		if (Seat.IsOccupied())
		{
			++Count;
		}
	}
	return Count;
}

int32 USHVehicleSeatSystem::FindSeatForCharacter(const ACharacter* Character) const
{
	if (!Character)
	{
		return -1;
	}

	for (const FSHVehicleSeat& Seat : Seats)
	{
		if (Seat.Occupant.Get() == Character)
		{
			return Seat.SeatIndex;
		}
	}
	return -1;
}

bool USHVehicleSeatSystem::GetSeatInfo(int32 SeatIndex, FSHVehicleSeat& OutSeat) const
{
	if (!IsValidSeatIndex(SeatIndex))
	{
		return false;
	}
	OutSeat = Seats[SeatIndex];
	return true;
}

bool USHVehicleSeatSystem::IsSeatExposed(int32 SeatIndex) const
{
	if (!IsValidSeatIndex(SeatIndex))
	{
		return false;
	}
	return Seats[SeatIndex].bIsExposed;
}

/* -----------------------------------------------------------------------
 *  Attachment
 * --------------------------------------------------------------------- */

void USHVehicleSeatSystem::AttachCharacterToSeat(ACharacter* Character, const FSHVehicleSeat& Seat)
{
	if (!Character || !GetOwner())
	{
		return;
	}

	// Disable character movement
	if (UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement())
	{
		MovementComp->DisableMovement();
		MovementComp->StopMovementImmediately();
	}

	// Disable collision on capsule so it doesn't interfere with vehicle
	if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	// Find the vehicle mesh to attach to
	USceneComponent* AttachTarget = GetOwner()->GetRootComponent();
	FName SocketToUse = Seat.AttachSocket;

	// Attach the character
	if (AttachTarget)
	{
		FAttachmentTransformRules AttachRules(
			EAttachmentRule::SnapToTarget,
			EAttachmentRule::SnapToTarget,
			EAttachmentRule::KeepWorld,
			true);

		Character->AttachToComponent(AttachTarget, AttachRules, SocketToUse);
	}
}

void USHVehicleSeatSystem::DetachCharacterFromSeat(ACharacter* Character, const FSHVehicleSeat& Seat)
{
	if (!Character)
	{
		return;
	}

	// Detach
	FDetachmentTransformRules DetachRules(EDetachmentRule::KeepWorld, true);
	Character->DetachFromActor(DetachRules);

	// Calculate exit position
	const FVector ExitLoc = CalculateExitLocation(Seat);
	Character->SetActorLocation(ExitLoc);

	// Re-enable collision
	if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}

	// Re-enable movement
	if (UCharacterMovementComponent* MovementComp = Character->GetCharacterMovement())
	{
		MovementComp->SetMovementMode(MOVE_Falling); // Let them settle to ground
	}
}

/* -----------------------------------------------------------------------
 *  Helpers
 * --------------------------------------------------------------------- */

int32 USHVehicleSeatSystem::FindBestAvailableSeat() const
{
	// Priority order: Driver, Gunner, Commander, Passenger
	static const ESHSeatType Priority[] = {
		ESHSeatType::Driver,
		ESHSeatType::Gunner,
		ESHSeatType::Commander,
		ESHSeatType::Passenger
	};

	for (ESHSeatType Type : Priority)
	{
		for (const FSHVehicleSeat& Seat : Seats)
		{
			if (Seat.SeatType == Type && !Seat.IsOccupied())
			{
				return Seat.SeatIndex;
			}
		}
	}

	return -1;
}

FVector USHVehicleSeatSystem::CalculateExitLocation(const FSHVehicleSeat& Seat) const
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return FVector::ZeroVector;
	}

	// Transform the exit offset from vehicle local space to world space
	const FTransform VehicleTransform = Owner->GetActorTransform();
	FVector WorldExitLoc = VehicleTransform.TransformPosition(Seat.ExitOffset);

	// Trace downward to find ground
	if (UWorld* World = GetWorld())
	{
		FHitResult GroundHit;
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(Owner);

		const FVector TraceStart = WorldExitLoc + FVector(0.0f, 0.0f, 200.0f);
		const FVector TraceEnd = WorldExitLoc - FVector(0.0f, 0.0f, 500.0f);

		if (World->LineTraceSingleByChannel(GroundHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
		{
			// Place slightly above the ground
			WorldExitLoc = GroundHit.ImpactPoint + FVector(0.0f, 0.0f, 90.0f);
		}
	}

	return WorldExitLoc;
}

bool USHVehicleSeatSystem::IsValidSeatIndex(int32 Index) const
{
	return Index >= 0 && Index < Seats.Num();
}

void USHVehicleSeatSystem::InitializeDefaultSeats()
{
	Seats.Empty();

	// Seat 0: Driver
	{
		FSHVehicleSeat Seat;
		Seat.SeatIndex = 0;
		Seat.SeatType = ESHSeatType::Driver;
		Seat.AttachSocket = TEXT("seat_driver");
		Seat.bIsExposed = false;
		Seat.bCanFire = false;
		Seat.CameraOffset = FVector(0.0f, 0.0f, 80.0f);
		Seat.ExitOffset = FVector(0.0f, -250.0f, 0.0f);
		Seats.Add(Seat);
	}

	// Seat 1: Gunner
	{
		FSHVehicleSeat Seat;
		Seat.SeatIndex = 1;
		Seat.SeatType = ESHSeatType::Gunner;
		Seat.AttachSocket = TEXT("seat_gunner");
		Seat.bIsExposed = true;
		Seat.bCanFire = true;
		Seat.CameraOffset = FVector(0.0f, 0.0f, 120.0f);
		Seat.ExitOffset = FVector(0.0f, 250.0f, 0.0f);
		Seats.Add(Seat);
	}

	// Seat 2: Commander
	{
		FSHVehicleSeat Seat;
		Seat.SeatIndex = 2;
		Seat.SeatType = ESHSeatType::Commander;
		Seat.AttachSocket = TEXT("seat_commander");
		Seat.bIsExposed = true;
		Seat.bCanFire = false;
		Seat.CameraOffset = FVector(0.0f, 0.0f, 100.0f);
		Seat.ExitOffset = FVector(-200.0f, 0.0f, 0.0f);
		Seats.Add(Seat);
	}

	// Seats 3-5: Passengers
	for (int32 i = 0; i < 3; ++i)
	{
		FSHVehicleSeat Seat;
		Seat.SeatIndex = 3 + i;
		Seat.SeatType = ESHSeatType::Passenger;
		Seat.AttachSocket = *FString::Printf(TEXT("seat_passenger_%d"), i);
		Seat.bIsExposed = false;
		Seat.bCanFire = false;
		Seat.CameraOffset = FVector(0.0f, 0.0f, 80.0f);
		Seat.ExitOffset = FVector(-300.0f, (i - 1) * 150.0f, 0.0f);
		Seats.Add(Seat);
	}
}
