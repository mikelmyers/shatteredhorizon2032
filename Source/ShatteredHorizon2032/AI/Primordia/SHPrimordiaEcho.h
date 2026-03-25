// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "SHPrimordiaEcho.generated.h"

/* -----------------------------------------------------------------------
 *  Enums
 * --------------------------------------------------------------------- */

/** Types of AI communication messages. */
UENUM(BlueprintType)
enum class ESHAICommsMessageType : uint8
{
	Contact         UMETA(DisplayName = "Contact"),
	OrderAck        UMETA(DisplayName = "Order Acknowledgment"),
	StatusReport    UMETA(DisplayName = "Status Report"),
	RequestSupport  UMETA(DisplayName = "Request Support"),
	Warning         UMETA(DisplayName = "Warning")
};

/** Hand signal types for visual communication between enemy AI units. */
UENUM(BlueprintType)
enum class ESHHandSignal : uint8
{
	PointTarget   UMETA(DisplayName = "Point Target"),
	FormUp        UMETA(DisplayName = "Form Up"),
	MoveForward   UMETA(DisplayName = "Move Forward"),
	HoldPosition  UMETA(DisplayName = "Hold Position"),
	TakeCover     UMETA(DisplayName = "Take Cover")
};

/* -----------------------------------------------------------------------
 *  Structs
 * --------------------------------------------------------------------- */

USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHAICommsMessage
{
	GENERATED_BODY()

	/** Actor that sent this message. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Comms")
	TWeakObjectPtr<AActor> Sender = nullptr;

	/** Classification of this communication. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Comms")
	ESHAICommsMessageType MessageType = ESHAICommsMessageType::Contact;

	/** World location the message references (e.g. enemy contact position). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Comms")
	FVector TargetLocation = FVector::ZeroVector;

	/** Estimated enemy count (relevant for Contact / RequestSupport). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Comms")
	int32 EnemyCount = 0;

	/** Threat level assessment 0-1 (relevant for Contact / Warning). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Comms")
	float ThreatLevel = 0.0f;

	/** Game-time timestamp when the message was created. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Comms")
	float Timestamp = 0.0f;

	/** Optional hand signal that accompanies this message for animation. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Comms")
	ESHHandSignal HandSignal = ESHHandSignal::PointTarget;
};

/* -----------------------------------------------------------------------
 *  Delegates
 * --------------------------------------------------------------------- */

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAIMessageReceived, const FSHAICommsMessage&, Message);

/* -----------------------------------------------------------------------
 *  USHPrimordiaEcho
 * --------------------------------------------------------------------- */

/**
 * USHPrimordiaEcho
 *
 * AI communication layer component. Broadcasts squad callouts and
 * coordination signals between AI units. Tracks message history for
 * replay/debug purposes and outputs hand signal types to drive
 * enemy AI communication animations.
 */
UCLASS(ClassGroup = (AI), meta = (BlueprintSpawnableComponent))
class SHATTEREDHORIZON2032_API USHPrimordiaEcho : public UActorComponent
{
	GENERATED_BODY()

public:
	USHPrimordiaEcho();

	// ------------------------------------------------------------------
	//  UActorComponent overrides
	// ------------------------------------------------------------------
	virtual void BeginPlay() override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	// ------------------------------------------------------------------
	//  Broadcasting
	// ------------------------------------------------------------------

	/**
	 * Broadcast a message to all nearby AI units within communication range.
	 * Automatically stamps the message with the current game time and
	 * determines the appropriate hand signal for visual feedback.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Echo")
	void BroadcastMessage(const FSHAICommsMessage& Message);

	// ------------------------------------------------------------------
	//  Queries
	// ------------------------------------------------------------------

	/** Get the full message history for debug/replay purposes. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Echo")
	TArray<FSHAICommsMessage> GetMessageHistory() const { return MessageHistory; }

	/** Get the last N received messages. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia|Echo")
	TArray<FSHAICommsMessage> GetRecentMessages(int32 Count) const;

	/** Get the most recent hand signal type (for animation system). */
	UFUNCTION(BlueprintPure, Category = "SH|Primordia|Echo")
	ESHHandSignal GetCurrentHandSignal() const { return CurrentHandSignal; }

	/** Whether a hand signal is currently being displayed. */
	UFUNCTION(BlueprintPure, Category = "SH|Primordia|Echo")
	bool IsSignaling() const { return bIsSignaling; }

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Primordia|Echo")
	FOnAIMessageReceived OnMessageReceived;

protected:
	/** Maximum communication range (cm). Messages outside this range are dropped. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Primordia|Echo|Config", meta = (ClampMin = "0"))
	float CommunicationRange = 5000.0f;

	/** Maximum number of messages stored in history. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Primordia|Echo|Config", meta = (ClampMin = "10"))
	int32 MaxHistorySize = 200;

	/** Duration a hand signal stays active (seconds). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Primordia|Echo|Config", meta = (ClampMin = "0.1"))
	float HandSignalDuration = 2.0f;

private:
	/** Receive a message from another Echo component. */
	void ReceiveMessage(const FSHAICommsMessage& Message);

	/** Determine the appropriate hand signal for a message type. */
	ESHHandSignal DetermineHandSignal(const FSHAICommsMessage& Message) const;

	/** Clear expired hand signal. */
	void TickHandSignal(float DeltaTime);

	/** All received/sent messages (capped at MaxHistorySize). */
	TArray<FSHAICommsMessage> MessageHistory;

	/** Currently displayed hand signal. */
	ESHHandSignal CurrentHandSignal = ESHHandSignal::HoldPosition;

	/** Whether a hand signal animation should be playing. */
	bool bIsSignaling = false;

	/** Time remaining on current hand signal. */
	float HandSignalTimeRemaining = 0.0f;

	/** Grant access to ReceiveMessage from other Echo components. */
	friend class USHPrimordiaEcho;
};
