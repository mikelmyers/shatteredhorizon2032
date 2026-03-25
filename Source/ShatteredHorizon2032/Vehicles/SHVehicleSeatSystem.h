// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SHVehicleSeatSystem.generated.h"

class ACharacter;

/* -----------------------------------------------------------------------
 *  Enums
 * --------------------------------------------------------------------- */

UENUM(BlueprintType)
enum class ESHSeatType : uint8
{
	Driver     UMETA(DisplayName = "Driver"),
	Gunner     UMETA(DisplayName = "Gunner"),
	Commander  UMETA(DisplayName = "Commander"),
	Passenger  UMETA(DisplayName = "Passenger"),
};

/* -----------------------------------------------------------------------
 *  Seat struct
 * --------------------------------------------------------------------- */

USTRUCT(BlueprintType)
struct FSHVehicleSeat
{
	GENERATED_BODY()

	/** Unique index for this seat within the vehicle (0-based). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Seat", meta = (ClampMin = "0", ClampMax = "5"))
	int32 SeatIndex = 0;

	/** Role of this seat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Seat")
	ESHSeatType SeatType = ESHSeatType::Passenger;

	/** Socket on the vehicle mesh where the occupant is attached. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Seat")
	FName AttachSocket = NAME_None;

	/** Current occupant (weak ref to avoid preventing GC). */
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Vehicle|Seat")
	TWeakObjectPtr<ACharacter> Occupant;

	/** If true, this seat is exposed — occupant can be shot by external fire. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Seat")
	bool bIsExposed = false;

	/** If true, the occupant can fire a weapon from this seat. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Seat")
	bool bCanFire = false;

	/** Camera offset from the attach socket (local space). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Seat")
	FVector CameraOffset = FVector(0.0f, 0.0f, 80.0f);

	/** Exit offset relative to the vehicle — where the character is placed on exit. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Seat")
	FVector ExitOffset = FVector(0.0f, 200.0f, 0.0f);

	bool IsOccupied() const { return Occupant.IsValid(); }
};

/* -----------------------------------------------------------------------
 *  Delegates
 * --------------------------------------------------------------------- */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FSHOnCharacterEnteredVehicle, ACharacter*, Character, int32, SeatIndex, ESHSeatType, SeatType);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FSHOnCharacterExitedVehicle, ACharacter*, Character, int32, SeatIndex, ESHSeatType, SeatType);

/* -----------------------------------------------------------------------
 *  USHVehicleSeatSystem
 * --------------------------------------------------------------------- */

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHVehicleSeatSystem : public UActorComponent
{
	GENERATED_BODY()

public:
	USHVehicleSeatSystem();

	virtual void BeginPlay() override;

	/** Maximum seats allowed per vehicle. */
	static constexpr int32 MaxSeatsPerVehicle = 6;

	/* --- Entry / Exit --- */

	/**
	 * Attempt to enter the vehicle. Finds the best available seat.
	 * @param Character       The character entering.
	 * @param PreferredSeat   Preferred seat index (-1 = auto-assign).
	 * @return True if the character was successfully seated.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vehicle|Seat")
	bool EnterVehicle(ACharacter* Character, int32 PreferredSeat = -1);

	/**
	 * Exit the vehicle. Detaches the character and places them near the vehicle.
	 * @param Character  The character exiting.
	 * @return True if the character was successfully removed from a seat.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vehicle|Seat")
	bool ExitVehicle(ACharacter* Character);

	/**
	 * Move a character to a different seat.
	 * @param Character  The character to move.
	 * @param NewSeat    Target seat index.
	 * @return True if the switch was successful.
	 */
	UFUNCTION(BlueprintCallable, Category = "Vehicle|Seat")
	bool SwitchSeat(ACharacter* Character, int32 NewSeat);

	/* --- Queries --- */

	/** Get the occupant of a specific seat. Returns nullptr if empty or invalid index. */
	UFUNCTION(BlueprintPure, Category = "Vehicle|Seat")
	ACharacter* GetOccupant(int32 SeatIndex) const;

	/** Get indices of all unoccupied seats. */
	UFUNCTION(BlueprintPure, Category = "Vehicle|Seat")
	TArray<int32> GetAvailableSeats() const;

	/** Check if a specific seat is occupied. */
	UFUNCTION(BlueprintPure, Category = "Vehicle|Seat")
	bool IsSeatOccupied(int32 SeatIndex) const;

	/** Get the seat index of the driver seat. Returns -1 if none configured. */
	UFUNCTION(BlueprintPure, Category = "Vehicle|Seat")
	int32 GetDriverSeatIndex() const;

	/** Get the seat index of the gunner seat. Returns -1 if none configured. */
	UFUNCTION(BlueprintPure, Category = "Vehicle|Seat")
	int32 GetGunnerSeatIndex() const;

	/** Get the driver character, if any. */
	UFUNCTION(BlueprintPure, Category = "Vehicle|Seat")
	ACharacter* GetDriver() const;

	/** Get the gunner character, if any. */
	UFUNCTION(BlueprintPure, Category = "Vehicle|Seat")
	ACharacter* GetGunner() const;

	/** Returns true if at least one seat is occupied. */
	UFUNCTION(BlueprintPure, Category = "Vehicle|Seat")
	bool IsAnyoneInVehicle() const;

	/** Get the number of currently occupied seats. */
	UFUNCTION(BlueprintPure, Category = "Vehicle|Seat")
	int32 GetOccupantCount() const;

	/** Get the seat index that a character occupies. Returns -1 if not found. */
	UFUNCTION(BlueprintPure, Category = "Vehicle|Seat")
	int32 FindSeatForCharacter(const ACharacter* Character) const;

	/** Get a read-only view of the seat at a given index. */
	UFUNCTION(BlueprintPure, Category = "Vehicle|Seat")
	bool GetSeatInfo(int32 SeatIndex, FSHVehicleSeat& OutSeat) const;

	/** Check if a seat is exposed (occupant can take external fire). */
	UFUNCTION(BlueprintPure, Category = "Vehicle|Seat")
	bool IsSeatExposed(int32 SeatIndex) const;

	/* --- Delegates --- */

	UPROPERTY(BlueprintAssignable, Category = "Vehicle|Events")
	FSHOnCharacterEnteredVehicle OnCharacterEnteredVehicle;

	UPROPERTY(BlueprintAssignable, Category = "Vehicle|Events")
	FSHOnCharacterExitedVehicle OnCharacterExitedVehicle;

	/* --- Configuration --- */

	/** Configured seats for this vehicle. Populated in editor on the owning vehicle BP. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Vehicle|Seat")
	TArray<FSHVehicleSeat> Seats;

protected:
	/** Attach a character to a seat socket on the vehicle mesh. */
	void AttachCharacterToSeat(ACharacter* Character, const FSHVehicleSeat& Seat);

	/** Detach a character from the vehicle and place at exit location. */
	void DetachCharacterFromSeat(ACharacter* Character, const FSHVehicleSeat& Seat);

	/** Find the best available seat (priority: Driver > Gunner > Commander > Passenger). */
	int32 FindBestAvailableSeat() const;

	/** Calculate world-space exit location for a seat, with ground trace. */
	FVector CalculateExitLocation(const FSHVehicleSeat& Seat) const;

	/** Validate that a seat index is within range. */
	bool IsValidSeatIndex(int32 Index) const;

	/** Initialize default seats if none are configured. */
	void InitializeDefaultSeats();
};
