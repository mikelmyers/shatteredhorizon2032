// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHPrimordiaEcho.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogSH_Echo, Log, All);

// -----------------------------------------------------------------------
//  Constructor
// -----------------------------------------------------------------------

USHPrimordiaEcho::USHPrimordiaEcho()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.TickInterval = 0.1f; // 10 Hz is sufficient for signal ticking
}

// -----------------------------------------------------------------------
//  UActorComponent overrides
// -----------------------------------------------------------------------

void USHPrimordiaEcho::BeginPlay()
{
	Super::BeginPlay();
	UE_LOG(LogSH_Echo, Verbose, TEXT("Echo component initialized on %s."),
		*GetOwner()->GetName());
}

void USHPrimordiaEcho::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	TickHandSignal(DeltaTime);
}

// -----------------------------------------------------------------------
//  Broadcasting
// -----------------------------------------------------------------------

void USHPrimordiaEcho::BroadcastMessage(const FSHAICommsMessage& Message)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	// Stamp the message
	FSHAICommsMessage StampedMessage = Message;
	StampedMessage.Sender = Owner;
	StampedMessage.Timestamp = World->GetTimeSeconds();
	StampedMessage.HandSignal = DetermineHandSignal(StampedMessage);

	// Show our own hand signal
	CurrentHandSignal = StampedMessage.HandSignal;
	bIsSignaling = true;
	HandSignalTimeRemaining = HandSignalDuration;

	// Store in our own history
	MessageHistory.Add(StampedMessage);
	if (MessageHistory.Num() > MaxHistorySize)
	{
		MessageHistory.RemoveAt(0, MessageHistory.Num() - MaxHistorySize);
	}

	// Broadcast to all Echo components within range
	const FVector OwnerLocation = Owner->GetActorLocation();
	const float RangeSquared = CommunicationRange * CommunicationRange;

	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* OtherActor = *It;
		if (OtherActor == Owner || !OtherActor)
		{
			continue;
		}

		// Check range
		const float DistSq = FVector::DistSquared(OwnerLocation, OtherActor->GetActorLocation());
		if (DistSq > RangeSquared)
		{
			continue;
		}

		// Find Echo component on the target
		USHPrimordiaEcho* OtherEcho = OtherActor->FindComponentByClass<USHPrimordiaEcho>();
		if (OtherEcho)
		{
			OtherEcho->ReceiveMessage(StampedMessage);
		}
	}

	UE_LOG(LogSH_Echo, Verbose, TEXT("%s broadcast %s message (enemies=%d, threat=%.2f)."),
		*Owner->GetName(),
		*UEnum::GetValueAsString(StampedMessage.MessageType),
		StampedMessage.EnemyCount,
		StampedMessage.ThreatLevel);
}

// -----------------------------------------------------------------------
//  Queries
// -----------------------------------------------------------------------

TArray<FSHAICommsMessage> USHPrimordiaEcho::GetRecentMessages(int32 Count) const
{
	const int32 StartIndex = FMath::Max(0, MessageHistory.Num() - Count);
	TArray<FSHAICommsMessage> Result;
	for (int32 i = StartIndex; i < MessageHistory.Num(); ++i)
	{
		Result.Add(MessageHistory[i]);
	}
	return Result;
}

// -----------------------------------------------------------------------
//  Internal
// -----------------------------------------------------------------------

void USHPrimordiaEcho::ReceiveMessage(const FSHAICommsMessage& Message)
{
	// Store in history
	MessageHistory.Add(Message);
	if (MessageHistory.Num() > MaxHistorySize)
	{
		MessageHistory.RemoveAt(0, MessageHistory.Num() - MaxHistorySize);
	}

	// Fire delegate
	OnMessageReceived.Broadcast(Message);
}

ESHHandSignal USHPrimordiaEcho::DetermineHandSignal(const FSHAICommsMessage& Message) const
{
	switch (Message.MessageType)
	{
	case ESHAICommsMessageType::Contact:
		return ESHHandSignal::PointTarget;

	case ESHAICommsMessageType::OrderAck:
		return ESHHandSignal::MoveForward;

	case ESHAICommsMessageType::StatusReport:
		return ESHHandSignal::FormUp;

	case ESHAICommsMessageType::RequestSupport:
		return ESHHandSignal::FormUp;

	case ESHAICommsMessageType::Warning:
		return ESHHandSignal::TakeCover;

	default:
		return ESHHandSignal::HoldPosition;
	}
}

void USHPrimordiaEcho::TickHandSignal(float DeltaTime)
{
	if (!bIsSignaling)
	{
		return;
	}

	HandSignalTimeRemaining -= DeltaTime;
	if (HandSignalTimeRemaining <= 0.0f)
	{
		bIsSignaling = false;
		HandSignalTimeRemaining = 0.0f;
	}
}
