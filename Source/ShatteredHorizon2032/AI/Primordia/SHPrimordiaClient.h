// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Http.h"
#include "SHPrimordiaClient.generated.h"

class IWebSocket;

// -----------------------------------------------------------------------
//  Data structures for Primordia communication
// -----------------------------------------------------------------------

/** Authentication state for the Primordia session. */
UENUM(BlueprintType)
enum class ESHPrimordiaAuthState : uint8
{
	Disconnected    UMETA(DisplayName = "Disconnected"),
	Authenticating  UMETA(DisplayName = "Authenticating"),
	Authenticated   UMETA(DisplayName = "Authenticated"),
	SessionExpired  UMETA(DisplayName = "Session Expired"),
	Error           UMETA(DisplayName = "Error")
};

/** Connection health status. */
UENUM(BlueprintType)
enum class ESHPrimordiaConnectionHealth : uint8
{
	Healthy,
	Degraded,
	Lost
};

/** A single tactical directive returned by Primordia. */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHPrimordiaTacticalDirective
{
	GENERATED_BODY()

	/** Unique directive identifier for acknowledgment. */
	UPROPERTY(BlueprintReadOnly)
	FString DirectiveId;

	/** High-level order type, e.g. "probe", "mass_assault", "feint", "retreat", "hold". */
	UPROPERTY(BlueprintReadOnly)
	FString OrderType;

	/** World-space target location / objective center. */
	UPROPERTY(BlueprintReadOnly)
	FVector TargetLocation = FVector::ZeroVector;

	/** Secondary location for multi-axis maneuvers (feint origin, flanking waypoint, etc.). */
	UPROPERTY(BlueprintReadOnly)
	FVector SecondaryLocation = FVector::ZeroVector;

	/** Proportion of available forces to commit (0..1). */
	UPROPERTY(BlueprintReadOnly)
	float ForceAllocationRatio = 0.f;

	/** Desired execution timestamp (server game-time seconds). */
	UPROPERTY(BlueprintReadOnly)
	float ExecutionTime = 0.f;

	/** Priority weight (higher = execute first). */
	UPROPERTY(BlueprintReadOnly)
	int32 Priority = 0;

	/** Sector tag / named zone this directive concerns. */
	UPROPERTY(BlueprintReadOnly)
	FName SectorTag;

	/** Squad IDs explicitly assigned (empty = decision engine decides). */
	UPROPERTY(BlueprintReadOnly)
	TArray<int32> AssignedSquadIds;

	/** Free-form parameters blob from Primordia (key/value). */
	UPROPERTY(BlueprintReadOnly)
	TMap<FString, FString> Parameters;
};

/** Container for a batch of directives from a single Primordia response. */
USTRUCT(BlueprintType)
struct SHATTEREDHORIZON2032_API FSHPrimordiaDirectiveBatch
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	FString BatchId;

	UPROPERTY(BlueprintReadOnly)
	double ServerTimestamp = 0.0;

	UPROPERTY(BlueprintReadOnly)
	TArray<FSHPrimordiaTacticalDirective> Directives;
};

// -----------------------------------------------------------------------
//  Delegates
// -----------------------------------------------------------------------

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPrimordiaDirectivesReceived, const FSHPrimordiaDirectiveBatch&, Batch);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPrimordiaConnectionChanged, ESHPrimordiaConnectionHealth, NewHealth);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPrimordiaAuthStateChanged, ESHPrimordiaAuthState, NewState);

/**
 * USHPrimordiaClient
 *
 * Network client responsible for communicating with the external Primordia
 * AI service.  Sends delta-compressed battlefield state snapshots and
 * receives tactical directives that feed the Decision Engine.
 *
 * Lifecycle:
 *   1. Created & configured by the Game Mode (or subsystem).
 *   2. Authenticate() establishes a session.
 *   3. PushBattlefieldState() sends delta updates at a configurable cadence.
 *   4. Directives arrive either via the WebSocket push channel or HTTP poll.
 *   5. If the connection is lost, the client signals fallback mode so that
 *      local AI can take over seamlessly.
 */
UCLASS(BlueprintType, Blueprintable)
class SHATTEREDHORIZON2032_API USHPrimordiaClient : public UObject
{
	GENERATED_BODY()

public:
	USHPrimordiaClient();

	// ------------------------------------------------------------------
	//  Initialization / Lifecycle
	// ------------------------------------------------------------------

	/** Initialize the client with endpoint and credentials. Call once. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia")
	void Initialize(const FString& InEndpointURL, const FString& InAPIKey);

	/** Tear down connections. Called automatically on destruction, but can be invoked early. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia")
	void Shutdown();

	/** Drive internal timers — must be called from owning object's Tick. */
	void Tick(float DeltaSeconds);

	// ------------------------------------------------------------------
	//  Authentication
	// ------------------------------------------------------------------

	/** Begin async authentication handshake. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia")
	void Authenticate();

	UFUNCTION(BlueprintPure, Category = "SH|Primordia")
	ESHPrimordiaAuthState GetAuthState() const { return AuthState; }

	// ------------------------------------------------------------------
	//  Data transfer
	// ------------------------------------------------------------------

	/**
	 * Push a battlefield state snapshot.  The client automatically
	 * computes a delta against the last acknowledged snapshot and sends
	 * only the changed fields.
	 *
	 * @param StateJson  Full state JSON object; the client handles delta logic.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia")
	void PushBattlefieldState(const FString& StateJson);

	/** Explicitly request tactical directives (HTTP poll fallback). */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia")
	void RequestDirectives();

	/** Acknowledge receipt of a directive batch so Primordia can track execution. */
	UFUNCTION(BlueprintCallable, Category = "SH|Primordia")
	void AcknowledgeBatch(const FString& BatchId);

	// ------------------------------------------------------------------
	//  Connection health
	// ------------------------------------------------------------------

	UFUNCTION(BlueprintPure, Category = "SH|Primordia")
	ESHPrimordiaConnectionHealth GetConnectionHealth() const { return ConnectionHealth; }

	UFUNCTION(BlueprintPure, Category = "SH|Primordia")
	bool IsConnected() const { return ConnectionHealth != ESHPrimordiaConnectionHealth::Lost; }

	UFUNCTION(BlueprintPure, Category = "SH|Primordia")
	float GetLatencyMs() const { return LastRoundTripMs; }

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Primordia")
	FOnPrimordiaDirectivesReceived OnDirectivesReceived;

	UPROPERTY(BlueprintAssignable, Category = "SH|Primordia")
	FOnPrimordiaConnectionChanged OnConnectionHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "SH|Primordia")
	FOnPrimordiaAuthStateChanged OnAuthStateChanged;

	// ------------------------------------------------------------------
	//  Configuration (exposed for tuning in editor / ini)
	// ------------------------------------------------------------------

	/** Base URL for the Primordia REST API, e.g. "https://primordia.example.com/api/v1". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Primordia|Config")
	FString EndpointURL;

	/** WebSocket URL (derived from EndpointURL if not set explicitly). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Primordia|Config")
	FString WebSocketURL;

	/** API key / bearer token. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Primordia|Config")
	FString APIKey;

	/** Minimum interval between state pushes (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Primordia|Config", meta = (ClampMin = "0.5", ClampMax = "30.0"))
	float StatePushInterval = 2.0f;

	/** Heartbeat ping interval (seconds). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Primordia|Config", meta = (ClampMin = "1.0", ClampMax = "60.0"))
	float HeartbeatInterval = 5.0f;

	/** Seconds without a pong before declaring connection lost. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Primordia|Config")
	float HeartbeatTimeoutSeconds = 15.0f;

	/** Maximum number of retry attempts before falling back to local AI. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Primordia|Config")
	int32 MaxRetryAttempts = 3;

	/** Seconds between reconnect attempts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Primordia|Config")
	float ReconnectDelay = 5.0f;

private:
	// ------------------------------------------------------------------
	//  Internal helpers
	// ------------------------------------------------------------------

	/** Build common HTTP headers (auth, content-type, session). */
	void SetCommonHeaders(FHttpRequestRef Request) const;

	/** Create and open the WebSocket channel. */
	void OpenWebSocket();

	/** Close the WebSocket channel. */
	void CloseWebSocket();

	/** Compute a JSON delta between PreviousStateHash and the new state. */
	FString ComputeStateDelta(const FString& FullStateJson);

	/** Parse a directive batch from a JSON string. */
	bool ParseDirectiveBatch(const FString& Json, FSHPrimordiaDirectiveBatch& OutBatch) const;

	/** Handle successful authentication response. */
	void HandleAuthResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);

	/** Handle state push response. */
	void HandleStatePushResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);

	/** Handle directive poll response. */
	void HandleDirectivePollResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully);

	/** WebSocket message callbacks. */
	void OnWebSocketConnected();
	void OnWebSocketConnectionError(const FString& Error);
	void OnWebSocketClosed(int32 StatusCode, const FString& Reason, bool bWasClean);
	void OnWebSocketMessage(const FString& Message);

	/** Heartbeat / connection monitoring. */
	void SendHeartbeat();
	void EvaluateConnectionHealth();

	/** Transition to a new auth state and broadcast. */
	void SetAuthState(ESHPrimordiaAuthState NewState);

	/** Transition to a new connection health and broadcast. */
	void SetConnectionHealth(ESHPrimordiaConnectionHealth NewHealth);

	/** Attempt reconnection. */
	void AttemptReconnect();

	// ------------------------------------------------------------------
	//  State
	// ------------------------------------------------------------------

	ESHPrimordiaAuthState AuthState = ESHPrimordiaAuthState::Disconnected;
	ESHPrimordiaConnectionHealth ConnectionHealth = ESHPrimordiaConnectionHealth::Lost;

	/** Session token returned by Primordia after authentication. */
	FString SessionToken;

	/** Session unique identifier. */
	FString SessionId;

	/** Hash of the last successfully acknowledged state snapshot (for delta computation). */
	uint32 PreviousStateHash = 0;

	/** The last full state JSON we sent (used for delta comparison). */
	FString PreviousStateJson;

	/** Round-trip latency of the most recent successful request. */
	float LastRoundTripMs = 0.f;

	/** Time since last heartbeat was sent. */
	float HeartbeatAccumulator = 0.f;

	/** Time since last pong received. */
	float TimeSinceLastPong = 0.f;

	/** Time since last state push. */
	float StatePushAccumulator = 0.f;

	/** Retry counter for reconnects. */
	int32 RetryCount = 0;

	/** Accumulator for reconnect delay. */
	float ReconnectAccumulator = 0.f;

	/** Whether we are actively trying to reconnect. */
	bool bReconnecting = false;

	/** Whether the client has been initialized. */
	bool bInitialized = false;

	/** Active WebSocket connection. */
	TSharedPtr<IWebSocket> WebSocket;
};
