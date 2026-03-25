// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "AI/Primordia/SHPrimordiaClient.h"
#include "WebSocketsModule.h"
#include "IWebSocket.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/CRC.h"

DEFINE_LOG_CATEGORY_STATIC(LogPrimordiaClient, Log, All);

// -----------------------------------------------------------------------
USHPrimordiaClient::USHPrimordiaClient()
{
}

// -----------------------------------------------------------------------
//  Initialization / Lifecycle
// -----------------------------------------------------------------------

void USHPrimordiaClient::Initialize(const FString& InEndpointURL, const FString& InAPIKey)
{
	if (bInitialized)
	{
		UE_LOG(LogPrimordiaClient, Warning, TEXT("PrimordiaClient already initialized — ignoring duplicate call."));
		return;
	}

	EndpointURL = InEndpointURL;
	APIKey = InAPIKey;

	// Derive WebSocket URL from REST endpoint if not set explicitly.
	if (WebSocketURL.IsEmpty())
	{
		WebSocketURL = EndpointURL;
		WebSocketURL.ReplaceInline(TEXT("https://"), TEXT("wss://"));
		WebSocketURL.ReplaceInline(TEXT("http://"), TEXT("ws://"));
		if (!WebSocketURL.EndsWith(TEXT("/")))
		{
			WebSocketURL += TEXT("/");
		}
		WebSocketURL += TEXT("ws");
	}

	bInitialized = true;
	UE_LOG(LogPrimordiaClient, Log, TEXT("PrimordiaClient initialized — REST: %s  WS: %s"), *EndpointURL, *WebSocketURL);
}

void USHPrimordiaClient::Shutdown()
{
	CloseWebSocket();
	SetAuthState(ESHPrimordiaAuthState::Disconnected);
	SetConnectionHealth(ESHPrimordiaConnectionHealth::Lost);
	bInitialized = false;
	SessionToken.Empty();
	SessionId.Empty();
	UE_LOG(LogPrimordiaClient, Log, TEXT("PrimordiaClient shut down."));
}

void USHPrimordiaClient::Tick(float DeltaSeconds)
{
	if (!bInitialized)
	{
		return;
	}

	// --- Heartbeat ---
	if (AuthState == ESHPrimordiaAuthState::Authenticated)
	{
		HeartbeatAccumulator += DeltaSeconds;
		TimeSinceLastPong += DeltaSeconds;

		if (HeartbeatAccumulator >= HeartbeatInterval)
		{
			SendHeartbeat();
			HeartbeatAccumulator = 0.f;
		}

		EvaluateConnectionHealth();
	}

	// --- Reconnection ---
	if (bReconnecting)
	{
		ReconnectAccumulator += DeltaSeconds;
		if (ReconnectAccumulator >= ReconnectDelay)
		{
			ReconnectAccumulator = 0.f;
			AttemptReconnect();
		}
	}
}

// -----------------------------------------------------------------------
//  Authentication
// -----------------------------------------------------------------------

void USHPrimordiaClient::Authenticate()
{
	if (!bInitialized)
	{
		UE_LOG(LogPrimordiaClient, Error, TEXT("Cannot authenticate — client not initialized."));
		return;
	}

	SetAuthState(ESHPrimordiaAuthState::Authenticating);

	FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(EndpointURL + TEXT("/auth/session"));
	Request->SetVerb(TEXT("POST"));
	SetCommonHeaders(Request);

	// Body: API key + metadata
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("api_key"), APIKey);
	Body->SetStringField(TEXT("client_version"), TEXT("SH2032_1.0"));
	Body->SetStringField(TEXT("game_session"), FGuid::NewGuid().ToString());

	FString BodyStr;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyStr);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);
	Request->SetContentAsString(BodyStr);

	Request->OnProcessRequestComplete().BindUObject(this, &USHPrimordiaClient::HandleAuthResponse);
	Request->ProcessRequest();

	UE_LOG(LogPrimordiaClient, Log, TEXT("Authentication request sent."));
}

// -----------------------------------------------------------------------
//  Data transfer
// -----------------------------------------------------------------------

void USHPrimordiaClient::PushBattlefieldState(const FString& StateJson)
{
	if (AuthState != ESHPrimordiaAuthState::Authenticated)
	{
		UE_LOG(LogPrimordiaClient, Verbose, TEXT("Skipping state push — not authenticated."));
		return;
	}

	const FString DeltaJson = ComputeStateDelta(StateJson);

	// Prefer WebSocket if available
	if (WebSocket.IsValid() && WebSocket->IsConnected())
	{
		TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
		Envelope->SetStringField(TEXT("type"), TEXT("state_update"));
		Envelope->SetStringField(TEXT("session_id"), SessionId);
		Envelope->SetStringField(TEXT("payload"), DeltaJson);
		Envelope->SetNumberField(TEXT("timestamp"), FPlatformTime::Seconds());

		FString EnvelopeStr;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&EnvelopeStr);
		FJsonSerializer::Serialize(Envelope.ToSharedRef(), Writer);

		WebSocket->Send(EnvelopeStr);
	}
	else
	{
		// HTTP fallback
		FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
		Request->SetURL(EndpointURL + TEXT("/state"));
		Request->SetVerb(TEXT("POST"));
		SetCommonHeaders(Request);
		Request->SetContentAsString(DeltaJson);
		Request->OnProcessRequestComplete().BindUObject(this, &USHPrimordiaClient::HandleStatePushResponse);
		Request->ProcessRequest();
	}

	// Update cached state
	PreviousStateJson = StateJson;
	PreviousStateHash = FCrc::StrCrc32(*StateJson);
}

void USHPrimordiaClient::RequestDirectives()
{
	if (AuthState != ESHPrimordiaAuthState::Authenticated)
	{
		return;
	}

	FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
	Request->SetURL(FString::Printf(TEXT("%s/directives?session=%s"), *EndpointURL, *SessionId));
	Request->SetVerb(TEXT("GET"));
	SetCommonHeaders(Request);
	Request->OnProcessRequestComplete().BindUObject(this, &USHPrimordiaClient::HandleDirectivePollResponse);
	Request->ProcessRequest();
}

void USHPrimordiaClient::AcknowledgeBatch(const FString& BatchId)
{
	if (AuthState != ESHPrimordiaAuthState::Authenticated)
	{
		return;
	}

	if (WebSocket.IsValid() && WebSocket->IsConnected())
	{
		TSharedPtr<FJsonObject> Msg = MakeShared<FJsonObject>();
		Msg->SetStringField(TEXT("type"), TEXT("ack"));
		Msg->SetStringField(TEXT("batch_id"), BatchId);

		FString MsgStr;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&MsgStr);
		FJsonSerializer::Serialize(Msg.ToSharedRef(), Writer);
		WebSocket->Send(MsgStr);
	}
	else
	{
		FHttpRequestRef Request = FHttpModule::Get().CreateRequest();
		Request->SetURL(EndpointURL + TEXT("/directives/ack"));
		Request->SetVerb(TEXT("POST"));
		SetCommonHeaders(Request);

		FString Body = FString::Printf(TEXT("{\"batch_id\":\"%s\",\"session_id\":\"%s\"}"), *BatchId, *SessionId);
		Request->SetContentAsString(Body);
		Request->ProcessRequest();
	}
}

// -----------------------------------------------------------------------
//  Internal — HTTP Response Handlers
// -----------------------------------------------------------------------

void USHPrimordiaClient::HandleAuthResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	if (!bConnectedSuccessfully || !Response.IsValid() || Response->GetResponseCode() != 200)
	{
		const int32 Code = Response.IsValid() ? Response->GetResponseCode() : 0;
		UE_LOG(LogPrimordiaClient, Error, TEXT("Authentication failed — HTTP %d"), Code);
		SetAuthState(ESHPrimordiaAuthState::Error);
		bReconnecting = true;
		return;
	}

	TSharedPtr<FJsonObject> Json;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Response->GetContentAsString());
	if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
	{
		UE_LOG(LogPrimordiaClient, Error, TEXT("Authentication response: malformed JSON."));
		SetAuthState(ESHPrimordiaAuthState::Error);
		return;
	}

	SessionToken = Json->GetStringField(TEXT("token"));
	SessionId = Json->GetStringField(TEXT("session_id"));

	SetAuthState(ESHPrimordiaAuthState::Authenticated);
	RetryCount = 0;
	bReconnecting = false;
	TimeSinceLastPong = 0.f;

	UE_LOG(LogPrimordiaClient, Log, TEXT("Authenticated with Primordia — session %s"), *SessionId);

	// Open the real-time WebSocket channel now that we have a session.
	OpenWebSocket();
}

void USHPrimordiaClient::HandleStatePushResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	if (!bConnectedSuccessfully || !Response.IsValid())
	{
		UE_LOG(LogPrimordiaClient, Warning, TEXT("State push failed (no response)."));
		return;
	}

	if (Response->GetResponseCode() == 200)
	{
		// Some responses may piggy-back directives.
		FSHPrimordiaDirectiveBatch Batch;
		if (ParseDirectiveBatch(Response->GetContentAsString(), Batch) && Batch.Directives.Num() > 0)
		{
			OnDirectivesReceived.Broadcast(Batch);
		}
	}
	else
	{
		UE_LOG(LogPrimordiaClient, Warning, TEXT("State push returned HTTP %d"), Response->GetResponseCode());
	}
}

void USHPrimordiaClient::HandleDirectivePollResponse(FHttpRequestPtr Request, FHttpResponsePtr Response, bool bConnectedSuccessfully)
{
	if (!bConnectedSuccessfully || !Response.IsValid() || Response->GetResponseCode() != 200)
	{
		return;
	}

	FSHPrimordiaDirectiveBatch Batch;
	if (ParseDirectiveBatch(Response->GetContentAsString(), Batch) && Batch.Directives.Num() > 0)
	{
		OnDirectivesReceived.Broadcast(Batch);
		AcknowledgeBatch(Batch.BatchId);
	}
}

// -----------------------------------------------------------------------
//  Internal — WebSocket
// -----------------------------------------------------------------------

void USHPrimordiaClient::OpenWebSocket()
{
	if (!FModuleManager::Get().IsModuleLoaded(TEXT("WebSockets")))
	{
		FModuleManager::Get().LoadModule(TEXT("WebSockets"));
	}

	const FString Protocol = TEXT("wss");
	TMap<FString, FString> Headers;
	Headers.Add(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *SessionToken));
	Headers.Add(TEXT("X-Session-Id"), SessionId);

	WebSocket = FWebSocketsModule::Get().CreateWebSocket(WebSocketURL, Protocol, Headers);

	WebSocket->OnConnected().AddUObject(this, &USHPrimordiaClient::OnWebSocketConnected);
	WebSocket->OnConnectionError().AddUObject(this, &USHPrimordiaClient::OnWebSocketConnectionError);
	WebSocket->OnClosed().AddUObject(this, &USHPrimordiaClient::OnWebSocketClosed);
	WebSocket->OnMessage().AddUObject(this, &USHPrimordiaClient::OnWebSocketMessage);

	WebSocket->Connect();
}

void USHPrimordiaClient::CloseWebSocket()
{
	if (WebSocket.IsValid())
	{
		if (WebSocket->IsConnected())
		{
			WebSocket->Close();
		}
		WebSocket.Reset();
	}
}

void USHPrimordiaClient::OnWebSocketConnected()
{
	UE_LOG(LogPrimordiaClient, Log, TEXT("WebSocket connected to Primordia."));
	SetConnectionHealth(ESHPrimordiaConnectionHealth::Healthy);
	TimeSinceLastPong = 0.f;
}

void USHPrimordiaClient::OnWebSocketConnectionError(const FString& Error)
{
	UE_LOG(LogPrimordiaClient, Error, TEXT("WebSocket error: %s"), *Error);
	SetConnectionHealth(ESHPrimordiaConnectionHealth::Degraded);
}

void USHPrimordiaClient::OnWebSocketClosed(int32 StatusCode, const FString& Reason, bool bWasClean)
{
	UE_LOG(LogPrimordiaClient, Warning, TEXT("WebSocket closed (%d): %s  clean=%d"), StatusCode, *Reason, bWasClean);

	if (AuthState == ESHPrimordiaAuthState::Authenticated)
	{
		// Attempt to reconnect WebSocket only.
		bReconnecting = true;
		ReconnectAccumulator = 0.f;
	}
}

void USHPrimordiaClient::OnWebSocketMessage(const FString& Message)
{
	TimeSinceLastPong = 0.f; // Any message counts as proof-of-life.

	TSharedPtr<FJsonObject> Json;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
	if (!FJsonSerializer::Deserialize(Reader, Json) || !Json.IsValid())
	{
		UE_LOG(LogPrimordiaClient, Warning, TEXT("Received malformed WS message."));
		return;
	}

	const FString Type = Json->GetStringField(TEXT("type"));

	if (Type == TEXT("pong"))
	{
		const double SendTime = Json->GetNumberField(TEXT("client_time"));
		LastRoundTripMs = static_cast<float>((FPlatformTime::Seconds() - SendTime) * 1000.0);
	}
	else if (Type == TEXT("directives"))
	{
		FSHPrimordiaDirectiveBatch Batch;
		if (ParseDirectiveBatch(Message, Batch))
		{
			OnDirectivesReceived.Broadcast(Batch);
			AcknowledgeBatch(Batch.BatchId);
		}
	}
	else if (Type == TEXT("session_expired"))
	{
		UE_LOG(LogPrimordiaClient, Warning, TEXT("Session expired — re-authenticating."));
		SetAuthState(ESHPrimordiaAuthState::SessionExpired);
		Authenticate();
	}
}

// -----------------------------------------------------------------------
//  Internal — Connection Health
// -----------------------------------------------------------------------

void USHPrimordiaClient::SendHeartbeat()
{
	if (WebSocket.IsValid() && WebSocket->IsConnected())
	{
		FString Ping = FString::Printf(TEXT("{\"type\":\"ping\",\"client_time\":%.6f}"), FPlatformTime::Seconds());
		WebSocket->Send(Ping);
	}
}

void USHPrimordiaClient::EvaluateConnectionHealth()
{
	if (TimeSinceLastPong > HeartbeatTimeoutSeconds)
	{
		SetConnectionHealth(ESHPrimordiaConnectionHealth::Lost);
		bReconnecting = true;
		ReconnectAccumulator = 0.f;
	}
	else if (TimeSinceLastPong > HeartbeatTimeoutSeconds * 0.5f)
	{
		SetConnectionHealth(ESHPrimordiaConnectionHealth::Degraded);
	}
	else
	{
		SetConnectionHealth(ESHPrimordiaConnectionHealth::Healthy);
	}
}

void USHPrimordiaClient::SetAuthState(ESHPrimordiaAuthState NewState)
{
	if (AuthState != NewState)
	{
		AuthState = NewState;
		OnAuthStateChanged.Broadcast(NewState);
	}
}

void USHPrimordiaClient::SetConnectionHealth(ESHPrimordiaConnectionHealth NewHealth)
{
	if (ConnectionHealth != NewHealth)
	{
		ConnectionHealth = NewHealth;
		OnConnectionHealthChanged.Broadcast(NewHealth);
	}
}

void USHPrimordiaClient::AttemptReconnect()
{
	if (RetryCount >= MaxRetryAttempts)
	{
		UE_LOG(LogPrimordiaClient, Error, TEXT("Max reconnect attempts reached — falling back to local AI."));
		SetConnectionHealth(ESHPrimordiaConnectionHealth::Lost);
		bReconnecting = false;
		return;
	}

	RetryCount++;
	UE_LOG(LogPrimordiaClient, Log, TEXT("Reconnect attempt %d / %d"), RetryCount, MaxRetryAttempts);

	// If the session is still valid, just reconnect WebSocket.
	if (AuthState == ESHPrimordiaAuthState::Authenticated && !SessionToken.IsEmpty())
	{
		CloseWebSocket();
		OpenWebSocket();
	}
	else
	{
		Authenticate();
	}
}

// -----------------------------------------------------------------------
//  Internal — Serialization Helpers
// -----------------------------------------------------------------------

void USHPrimordiaClient::SetCommonHeaders(FHttpRequestRef Request) const
{
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetHeader(TEXT("Accept"), TEXT("application/json"));
	Request->SetHeader(TEXT("X-API-Key"), APIKey);

	if (!SessionToken.IsEmpty())
	{
		Request->SetHeader(TEXT("Authorization"), FString::Printf(TEXT("Bearer %s"), *SessionToken));
	}
	if (!SessionId.IsEmpty())
	{
		Request->SetHeader(TEXT("X-Session-Id"), SessionId);
	}
}

FString USHPrimordiaClient::ComputeStateDelta(const FString& FullStateJson)
{
	const uint32 NewHash = FCrc::StrCrc32(*FullStateJson);

	// If first push or hash differs, determine changed fields.
	if (PreviousStateHash == 0 || PreviousStateHash != NewHash)
	{
		// Parse both JSONs and emit only changed top-level keys.
		TSharedPtr<FJsonObject> NewObj;
		TSharedRef<TJsonReader<>> NewReader = TJsonReaderFactory<>::Create(FullStateJson);
		FJsonSerializer::Deserialize(NewReader, NewObj);

		TSharedPtr<FJsonObject> OldObj;
		if (!PreviousStateJson.IsEmpty())
		{
			TSharedRef<TJsonReader<>> OldReader = TJsonReaderFactory<>::Create(PreviousStateJson);
			FJsonSerializer::Deserialize(OldReader, OldObj);
		}

		if (!NewObj.IsValid())
		{
			return FullStateJson; // Fallback — send everything.
		}

		TSharedPtr<FJsonObject> Delta = MakeShared<FJsonObject>();
		Delta->SetBoolField(TEXT("is_delta"), true);
		Delta->SetStringField(TEXT("session_id"), SessionId);

		for (const auto& Pair : NewObj->Values)
		{
			bool bChanged = true;

			if (OldObj.IsValid() && OldObj->HasField(Pair.Key))
			{
				// Serialize both values to string and compare — simple but effective for delta.
				FString NewValStr, OldValStr;
				TSharedRef<TJsonWriter<TCondensedJsonPrintPolicy<TCHAR>>> NewWriter = TJsonWriterFactory<TCondensedJsonPrintPolicy<TCHAR>>::Create(&NewValStr);
				TSharedRef<TJsonWriter<TCondensedJsonPrintPolicy<TCHAR>>> OldWriter = TJsonWriterFactory<TCondensedJsonPrintPolicy<TCHAR>>::Create(&OldValStr);

				FJsonSerializer::Serialize(Pair.Value, Pair.Key, NewWriter);
				FJsonSerializer::Serialize(OldObj->Values[Pair.Key], Pair.Key, OldWriter);

				bChanged = (NewValStr != OldValStr);
			}

			if (bChanged)
			{
				Delta->SetField(Pair.Key, Pair.Value);
			}
		}

		FString DeltaStr;
		TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&DeltaStr);
		FJsonSerializer::Serialize(Delta.ToSharedRef(), Writer);
		return DeltaStr;
	}

	// Nothing changed — send a minimal keepalive.
	return FString::Printf(TEXT("{\"is_delta\":true,\"session_id\":\"%s\",\"no_change\":true}"), *SessionId);
}

bool USHPrimordiaClient::ParseDirectiveBatch(const FString& Json, FSHPrimordiaDirectiveBatch& OutBatch) const
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return false;
	}

	OutBatch.BatchId = Root->GetStringField(TEXT("batch_id"));
	OutBatch.ServerTimestamp = Root->GetNumberField(TEXT("timestamp"));

	const TArray<TSharedPtr<FJsonValue>>* DirectivesArray = nullptr;
	if (!Root->TryGetArrayField(TEXT("directives"), DirectivesArray))
	{
		return false;
	}

	for (const TSharedPtr<FJsonValue>& Val : *DirectivesArray)
	{
		const TSharedPtr<FJsonObject>& Obj = Val->AsObject();
		if (!Obj.IsValid())
		{
			continue;
		}

		FSHPrimordiaTacticalDirective Directive;
		Directive.DirectiveId = Obj->GetStringField(TEXT("id"));
		Directive.OrderType = Obj->GetStringField(TEXT("order_type"));
		Directive.Priority = static_cast<int32>(Obj->GetIntegerField(TEXT("priority")));
		Directive.ForceAllocationRatio = static_cast<float>(Obj->GetNumberField(TEXT("force_ratio")));
		Directive.ExecutionTime = static_cast<float>(Obj->GetNumberField(TEXT("exec_time")));

		// Target location
		const TSharedPtr<FJsonObject>* LocObj = nullptr;
		if (Obj->TryGetObjectField(TEXT("target"), LocObj))
		{
			Directive.TargetLocation.X = (*LocObj)->GetNumberField(TEXT("x"));
			Directive.TargetLocation.Y = (*LocObj)->GetNumberField(TEXT("y"));
			Directive.TargetLocation.Z = (*LocObj)->GetNumberField(TEXT("z"));
		}

		if (Obj->TryGetObjectField(TEXT("secondary"), LocObj))
		{
			Directive.SecondaryLocation.X = (*LocObj)->GetNumberField(TEXT("x"));
			Directive.SecondaryLocation.Y = (*LocObj)->GetNumberField(TEXT("y"));
			Directive.SecondaryLocation.Z = (*LocObj)->GetNumberField(TEXT("z"));
		}

		Obj->TryGetStringField(TEXT("sector"), Directive.SectorTag);

		// Assigned squads
		const TArray<TSharedPtr<FJsonValue>>* SquadArr = nullptr;
		if (Obj->TryGetArrayField(TEXT("squads"), SquadArr))
		{
			for (const auto& SVal : *SquadArr)
			{
				Directive.AssignedSquadIds.Add(static_cast<int32>(SVal->AsNumber()));
			}
		}

		// Parameters
		const TSharedPtr<FJsonObject>* ParamsObj = nullptr;
		if (Obj->TryGetObjectField(TEXT("params"), ParamsObj))
		{
			for (const auto& P : (*ParamsObj)->Values)
			{
				Directive.Parameters.Add(P.Key, P.Value->AsString());
			}
		}

		OutBatch.Directives.Add(MoveTemp(Directive));
	}

	return true;
}
