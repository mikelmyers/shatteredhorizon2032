// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameFramework/GameModeBase.h"
#include "SHGameSessionManager.generated.h"

class USHMainMenuWidget;
class USHLoadoutWidget;
class USHLoadingScreenWidget;
class USHAfterActionWidget;
class USHPauseMenuWidget;
class USHProgressionSystem;
class UCommonActivatableWidget;
class UCommonActivatableWidgetContainerBase;
struct FSHEngagementRecord;
struct FSHXPBreakdown;

DECLARE_LOG_CATEGORY_EXTERN(LogSH_Session, Log, All);

// ─────────────────────────────────────────────────────────────────────
//  Enums
// ─────────────────────────────────────────────────────────────────────

/** States representing the full game session flow. */
UENUM(BlueprintType)
enum class ESHSessionState : uint8
{
	MainMenu		UMETA(DisplayName = "Main Menu"),
	MissionSelect	UMETA(DisplayName = "Mission Select"),
	LoadoutScreen	UMETA(DisplayName = "Loadout Screen"),
	Briefing		UMETA(DisplayName = "Briefing"),
	Loading			UMETA(DisplayName = "Loading"),
	InMission		UMETA(DisplayName = "In Mission"),
	Paused			UMETA(DisplayName = "Paused"),
	AfterAction		UMETA(DisplayName = "After Action"),
	CoopLobby		UMETA(DisplayName = "Co-op Lobby")
};

// ─────────────────────────────────────────────────────────────────────
//  Structs
// ─────────────────────────────────────────────────────────────────────

/** Configuration for a single mission instance. */
USTRUCT(BlueprintType)
struct FSHMissionConfig
{
	GENERATED_BODY()

	/** Unique identifier for this mission. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	FName MissionId;

	/** Localised display name. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	FText MissionName;

	/** Map asset path (e.g. "/Game/Maps/TaoyuanBeach"). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	FString MapName;

	/** Difficulty level. 0 = Normal, higher = harder. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission", meta = (ClampMin = "0", ClampMax = "4"))
	int32 DifficultyLevel = 0;

	/** Whether this mission supports cooperative play. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	bool bIsCoop = false;

	/** Maximum number of players in co-op. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission", meta = (ClampMin = "1", ClampMax = "4", EditCondition = "bIsCoop"))
	int32 MaxPlayers = 1;

	/** Game mode class to use for this mission. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|Mission")
	TSubclassOf<AGameModeBase> GameModeClass;

	bool IsValid() const { return !MissionId.IsNone() && !MapName.IsEmpty(); }
};

// ─────────────────────────────────────────────────────────────────────
//  Delegates
// ─────────────────────────────────────────────────────────────────────

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnSessionStateChanged,
	ESHSessionState, OldState,
	ESHSessionState, NewState);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOnMissionEnded,
	bool, bVictory,
	const FSHMissionConfig&, MissionConfig);

// ─────────────────────────────────────────────────────────────────────
//  USHGameSessionManager
// ─────────────────────────────────────────────────────────────────────

/**
 * USHGameSessionManager
 *
 * Game instance subsystem that orchestrates the full game session flow:
 *
 *   MainMenu → MissionSelect → Loadout → Briefing → Loading →
 *   InMission → AfterAction → MainMenu/MissionSelect
 *
 * Acts as the glue layer between save, progression, UI widgets, and
 * game modes. Does not implement gameplay logic — it manages transitions,
 * widget stacks, level travel, and inter-system handoffs.
 *
 * Widget management uses CommonUI activatable widget stacks. Each state
 * transition pushes the appropriate widget and deactivates the previous one.
 */
UCLASS()
class SHATTEREDHORIZON2032_API USHGameSessionManager : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override { return true; }

	// ------------------------------------------------------------------
	//  State machine
	// ------------------------------------------------------------------

	/**
	 * Main state machine driver. Validates the transition, tears down
	 * the current state, enters the new state, and broadcasts
	 * OnSessionStateChanged.
	 *
	 * @return true if the transition was accepted.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Session")
	bool TransitionTo(ESHSessionState NewState);

	/** Get the current session state. */
	UFUNCTION(BlueprintPure, Category = "SH|Session")
	ESHSessionState GetCurrentState() const { return CurrentState; }

	/** Get the state that was active before the current one. */
	UFUNCTION(BlueprintPure, Category = "SH|Session")
	ESHSessionState GetPreviousState() const { return PreviousState; }

	// ------------------------------------------------------------------
	//  Campaign flow
	// ------------------------------------------------------------------

	/**
	 * Start a new campaign. Creates a fresh campaign save, sets the
	 * difficulty, and transitions to Briefing for the first mission.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Session")
	void StartNewCampaign(int32 DifficultyLevel);

	/**
	 * Continue a previously saved campaign. Loads the save from the
	 * specified slot and transitions to Briefing for the next mission.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Session")
	bool ContinueCampaign(int32 SaveSlot);

	// ------------------------------------------------------------------
	//  Mission lifecycle
	// ------------------------------------------------------------------

	/**
	 * Start a mission with the given configuration. Transitions through
	 * Loading, opens the level, and enters InMission state.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Session")
	void StartMission(const FSHMissionConfig& Config);

	/**
	 * End the current mission. Gathers engagement records from player
	 * state, computes XP awards, pushes results to progression, and
	 * transitions to AfterAction.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Session")
	void EndMission(bool bVictory);

	/** Restart the current mission from the beginning. */
	UFUNCTION(BlueprintCallable, Category = "SH|Session")
	void RestartMission();

	/** Return to the main menu, cleaning up the current session. */
	UFUNCTION(BlueprintCallable, Category = "SH|Session")
	void ReturnToMainMenu();

	// ------------------------------------------------------------------
	//  Pause management
	// ------------------------------------------------------------------

	/** Pause the game and transition to Paused state. */
	UFUNCTION(BlueprintCallable, Category = "SH|Session")
	void PauseGame();

	/** Resume gameplay and return to InMission state. */
	UFUNCTION(BlueprintCallable, Category = "SH|Session")
	void ResumeGame();

	// ------------------------------------------------------------------
	//  Mission config access
	// ------------------------------------------------------------------

	/** Get the configuration for the currently active (or most recent) mission. */
	UFUNCTION(BlueprintPure, Category = "SH|Session")
	const FSHMissionConfig& GetCurrentMissionConfig() const { return CurrentMissionConfig; }

	/** Whether a mission is currently in progress. */
	UFUNCTION(BlueprintPure, Category = "SH|Session")
	bool IsMissionActive() const { return CurrentState == ESHSessionState::InMission || CurrentState == ESHSessionState::Paused; }

	// ------------------------------------------------------------------
	//  Widget management
	// ------------------------------------------------------------------

	/**
	 * Set the CommonUI widget container stack used for session UI.
	 * Must be called before any state transitions that display widgets.
	 */
	UFUNCTION(BlueprintCallable, Category = "SH|Session|UI")
	void SetWidgetStack(UCommonActivatableWidgetContainerBase* InWidgetStack);

	// ------------------------------------------------------------------
	//  Widget class configuration
	// ------------------------------------------------------------------

	/** Widget class for the main menu screen. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Session|UI")
	TSubclassOf<USHMainMenuWidget> MainMenuWidgetClass;

	/** Widget class for the loadout screen. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Session|UI")
	TSubclassOf<USHLoadoutWidget> LoadoutWidgetClass;

	/** Widget class for the loading screen. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Session|UI")
	TSubclassOf<USHLoadingScreenWidget> LoadingScreenWidgetClass;

	/** Widget class for the after-action report. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Session|UI")
	TSubclassOf<USHAfterActionWidget> AfterActionWidgetClass;

	/** Widget class for the pause menu. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Session|UI")
	TSubclassOf<USHPauseMenuWidget> PauseMenuWidgetClass;

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Map to load when returning to the main menu. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Session")
	FString MainMenuMapName = TEXT("/Game/Maps/MainMenu");

	/** Name prefix for campaign save slots. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Session")
	FString CampaignSavePrefix = TEXT("Campaign");

	/** Default first mission config used when starting a new campaign. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Session")
	FSHMissionConfig DefaultFirstMission;

	/** XP multiplier per difficulty level (index 0 = Normal). */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Session|XP")
	TArray<float> DifficultyXPMultipliers;

	// ------------------------------------------------------------------
	//  XP award tuning
	// ------------------------------------------------------------------

	UPROPERTY(EditDefaultsOnly, Category = "SH|Session|XP")
	int32 XPPerKill = 50;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Session|XP")
	int32 XPPerAssist = 20;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Session|XP")
	int32 XPPerVehicleKill = 150;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Session|XP")
	int32 XPPerObjective = 200;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Session|XP")
	int32 XPPerRevive = 75;

	UPROPERTY(EditDefaultsOnly, Category = "SH|Session|XP")
	int32 XPVictoryBonus = 500;

	/** Accuracy threshold (0-1) above which an accuracy XP bonus is awarded. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Session|XP", meta = (ClampMin = "0", ClampMax = "1"))
	float AccuracyBonusThreshold = 0.35f;

	/** XP awarded for exceeding the accuracy threshold. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|Session|XP")
	int32 AccuracyBonusXP = 100;

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|Session")
	FOnSessionStateChanged OnSessionStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "SH|Session")
	FOnMissionEnded OnMissionEnded;

private:
	// ------------------------------------------------------------------
	//  State management internals
	// ------------------------------------------------------------------

	/** Validate whether transitioning from CurrentState to NewState is legal. */
	bool IsTransitionValid(ESHSessionState From, ESHSessionState To) const;

	/** Tear down the current state (hide widgets, stop systems). */
	void ExitState(ESHSessionState State);

	/** Enter the new state (show widgets, start systems). */
	void EnterState(ESHSessionState State);

	// ------------------------------------------------------------------
	//  Widget helpers
	// ------------------------------------------------------------------

	/** Push a widget of the given class onto the widget stack. Returns the instance. */
	UCommonActivatableWidget* PushWidget(TSubclassOf<UCommonActivatableWidget> WidgetClass);

	/** Deactivate and remove the current active session widget. */
	void ClearActiveWidget();

	// ------------------------------------------------------------------
	//  Mission helpers
	// ------------------------------------------------------------------

	/** Perform level travel to the specified map. */
	void TravelToMap(const FString& MapPath);

	/** Run cleanup between missions: GC, subsystem resets. */
	void PerformInterMissionCleanup();

	/** Gather engagement stats from the local player state and compute XP. */
	FSHXPBreakdown ComputeXPAward(const FSHEngagementRecord& Record, bool bVictory) const;

	/** Get the XP multiplier for the current difficulty. */
	float GetDifficultyMultiplier() const;

	/** Build the save slot name for a given slot index. */
	FString GetSaveSlotName(int32 SlotIndex) const;

	/** Get the progression system from the game instance. */
	USHProgressionSystem* GetProgressionSystem() const;

	// ------------------------------------------------------------------
	//  Runtime state
	// ------------------------------------------------------------------

	/** Current session state. */
	UPROPERTY()
	ESHSessionState CurrentState = ESHSessionState::MainMenu;

	/** Previous session state (before the last transition). */
	UPROPERTY()
	ESHSessionState PreviousState = ESHSessionState::MainMenu;

	/** State to return to when unpausing. */
	UPROPERTY()
	ESHSessionState StateBeforePause = ESHSessionState::InMission;

	/** Current (or most recent) mission configuration. */
	UPROPERTY()
	FSHMissionConfig CurrentMissionConfig;

	/** Active save slot index for the current campaign. */
	int32 ActiveSaveSlot = 0;

	/** Whether the last mission was a victory (cached for AAR). */
	bool bLastMissionVictory = false;

	/** Elapsed real-time seconds for the current mission. */
	float MissionElapsedTime = 0.f;

	/** CommonUI widget stack for session-level UI. */
	UPROPERTY()
	TObjectPtr<UCommonActivatableWidgetContainerBase> WidgetStack = nullptr;

	/** Currently active session widget (main menu, loadout, AAR, etc.). */
	UPROPERTY()
	TObjectPtr<UCommonActivatableWidget> ActiveWidget = nullptr;
};
