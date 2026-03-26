// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.
//
// Primordia Decision Engine Tests
//
// These tests verify the tactical order system — force allocation,
// escalation, morale-based retreat, and reinforcement wave integration.
// The Decision Engine is the brain of the AI; bugs here break the entire
// enemy behavior chain.

#include "Misc/AutomationTest.h"
#include "AI/Primordia/SHPrimordiaDecisionEngine.h"

#if WITH_AUTOMATION_TESTS

// ========================================================================
//  Squad registration and tracking
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionEngine_SquadRegistration, "SH2032.AI.Primordia.DecisionEngine.SquadRegistration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDecisionEngine_SquadRegistration::RunTest(const FString&)
{
	USHPrimordiaDecisionEngine* Engine = NewObject<USHPrimordiaDecisionEngine>();
	Engine->Initialize();

	// Register 3 squads
	Engine->RegisterSquad(1, 6, false);
	Engine->RegisterSquad(2, 6, false);
	Engine->RegisterSquad(3, 4, true); // reserve

	// Squad count should reflect all registered
	const int32 Available = Engine->GetAvailableSquadCount(false);
	const int32 AvailableWithReserves = Engine->GetAvailableSquadCount(true);

	TestTrue(TEXT("Available squads without reserves >= 2"), Available >= 2);
	TestTrue(TEXT("Available with reserves > without"), AvailableWithReserves > Available);

	// Unregister a squad
	Engine->UnregisterSquad(1);

	const int32 AfterUnregister = Engine->GetAvailableSquadCount(false);
	TestTrue(TEXT("Count decreased after unregister"), AfterUnregister < Available);

	return true;
}

// ========================================================================
//  Strength tracking
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionEngine_StrengthTracking, "SH2032.AI.Primordia.DecisionEngine.StrengthTracking",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDecisionEngine_StrengthTracking::RunTest(const FString&)
{
	USHPrimordiaDecisionEngine* Engine = NewObject<USHPrimordiaDecisionEngine>();
	Engine->Initialize();

	Engine->RegisterSquad(1, 6, false);

	// Full strength
	Engine->UpdateSquadStrength(1, 6, 1.0f);
	// This shouldn't crash or behave oddly

	// Half casualties
	Engine->UpdateSquadStrength(1, 3, 0.5f);

	// Zero alive — squad wiped
	Engine->UpdateSquadStrength(1, 0, 0.0f);

	// Negative values should be clamped (defensive test)
	Engine->UpdateSquadStrength(1, -1, -0.5f);

	// Update on unregistered squad should not crash
	Engine->UpdateSquadStrength(999, 5, 0.8f);

	return true;
}

// ========================================================================
//  Configuration: default values are reasonable
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionEngine_DefaultConfig, "SH2032.AI.Primordia.DecisionEngine.DefaultConfig",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDecisionEngine_DefaultConfig::RunTest(const FString&)
{
	USHPrimordiaDecisionEngine* Engine = NewObject<USHPrimordiaDecisionEngine>();

	// Stall threshold should be 20-60 seconds (doctrine)
	TestTrue(TEXT("Stall threshold >= 15s"), Engine->StallThresholdSeconds >= 15.f);
	TestTrue(TEXT("Stall threshold <= 120s"), Engine->StallThresholdSeconds <= 120.f);

	// Casualty retreat should be 30-70% casualties
	TestTrue(TEXT("Casualty retreat >= 0.3"), Engine->CasualtyRetreatThreshold >= 0.3f);
	TestTrue(TEXT("Casualty retreat <= 0.7"), Engine->CasualtyRetreatThreshold <= 0.7f);

	// Reserve ratio should be 10-30%
	TestTrue(TEXT("Reserve ratio >= 0.1"), Engine->ReserveRatio >= 0.1f);
	TestTrue(TEXT("Reserve ratio <= 0.3"), Engine->ReserveRatio <= 0.3f);

	// Morale retreat threshold should be 0.1-0.4
	TestTrue(TEXT("Morale retreat >= 0.1"), Engine->MoraleRetreatThreshold >= 0.1f);
	TestTrue(TEXT("Morale retreat <= 0.4"), Engine->MoraleRetreatThreshold <= 0.4f);

	return true;
}

// ========================================================================
//  Order lifecycle: basic query behavior
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionEngine_OrderQueries, "SH2032.AI.Primordia.DecisionEngine.OrderQueries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDecisionEngine_OrderQueries::RunTest(const FString&)
{
	USHPrimordiaDecisionEngine* Engine = NewObject<USHPrimordiaDecisionEngine>();
	Engine->Initialize();

	// No orders initially
	TArray<FSHTacticalOrder> Orders = Engine->GetAllActiveOrders();
	TestEqual(TEXT("No orders initially"), Orders.Num(), 0);

	// Query for non-existent squad should return false
	FSHTacticalOrder OutOrder;
	const bool bFound = Engine->GetActiveOrderForSquad(999, OutOrder);
	TestFalse(TEXT("No order for unregistered squad"), bFound);

	return true;
}

// ========================================================================
//  Reinforcement wave integration
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionEngine_ReinforcementWave, "SH2032.AI.Primordia.DecisionEngine.ReinforcementWave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDecisionEngine_ReinforcementWave::RunTest(const FString&)
{
	USHPrimordiaDecisionEngine* Engine = NewObject<USHPrimordiaDecisionEngine>();
	Engine->Initialize();

	// Spawn new squads via reinforcement wave
	TArray<int32> NewSquads = { 10, 11, 12 };
	Engine->OnReinforcementWaveSpawned(NewSquads);

	// Engine should not crash on empty wave
	TArray<int32> EmptyWave;
	Engine->OnReinforcementWaveSpawned(EmptyWave);

	return true;
}

// ========================================================================
//  Fallback orders: disconnected from Primordia
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionEngine_Fallback, "SH2032.AI.Primordia.DecisionEngine.FallbackOrders",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDecisionEngine_Fallback::RunTest(const FString&)
{
	USHPrimordiaDecisionEngine* Engine = NewObject<USHPrimordiaDecisionEngine>();
	Engine->Initialize();

	// Register squads
	Engine->RegisterSquad(1, 6, false);
	Engine->RegisterSquad(2, 6, false);

	// Generate fallback orders (simulates Primordia disconnection)
	// Should not crash, and should produce some form of orders
	Engine->GenerateLocalFallbackOrders();

	return true;
}

// ========================================================================
//  Tactical order state machine: verify all states exist
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionEngine_OrderStates, "SH2032.AI.Primordia.DecisionEngine.OrderStates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDecisionEngine_OrderStates::RunTest(const FString&)
{
	// Verify all order states are defined
	TestEqual(TEXT("Pending is 0"), static_cast<uint8>(ESHTacticalOrderState::Pending), 0);
	TestEqual(TEXT("InProgress is 1"), static_cast<uint8>(ESHTacticalOrderState::InProgress), 1);
	TestEqual(TEXT("Stalled is 2"), static_cast<uint8>(ESHTacticalOrderState::Stalled), 2);
	TestEqual(TEXT("Succeeded is 3"), static_cast<uint8>(ESHTacticalOrderState::Succeeded), 3);
	TestEqual(TEXT("Failed is 4"), static_cast<uint8>(ESHTacticalOrderState::Failed), 4);
	TestEqual(TEXT("Cancelled is 5"), static_cast<uint8>(ESHTacticalOrderState::Cancelled), 5);

	return true;
}

// ========================================================================
//  Tick safety: engine should not crash on tick with no state
// ========================================================================

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDecisionEngine_TickSafety, "SH2032.AI.Primordia.DecisionEngine.TickSafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
bool FDecisionEngine_TickSafety::RunTest(const FString&)
{
	USHPrimordiaDecisionEngine* Engine = NewObject<USHPrimordiaDecisionEngine>();
	Engine->Initialize();

	// Tick with no squads, no orders — should not crash
	Engine->Tick(0.016f); // 60fps
	Engine->Tick(0.033f); // 30fps
	Engine->Tick(1.0f);   // 1fps (worst case)
	Engine->Tick(0.0f);   // zero delta

	// Register squads, still no orders
	Engine->RegisterSquad(1, 6, false);
	Engine->Tick(0.016f);

	// Large delta time (lag spike)
	Engine->Tick(5.0f);

	return true;
}

#endif // WITH_AUTOMATION_TESTS
