// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "SHSquadCommandWidget.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSH_SquadCommand, Log, All);

struct FSHSquadOrder;
enum class ESHOrderType : uint8;

/** A single option in the radial menu. */
USTRUCT(BlueprintType)
struct FSHRadialMenuOption
{
	GENERATED_BODY()

	/** Display label. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SquadCommand")
	FText Label;

	/** Icon texture for this option. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SquadCommand")
	TObjectPtr<UTexture2D> Icon = nullptr;

	/** The order type this option issues. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SquadCommand")
	ESHOrderType OrderType;

	/** Whether this option opens a sub-menu instead of issuing directly. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SquadCommand")
	bool bIsSubMenu = false;

	/** Sub-menu options (if bIsSubMenu). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SquadCommand", meta = (EditCondition = "bIsSubMenu"))
	TArray<FSHRadialMenuOption> SubOptions;

	/** Whether this option is currently available (context-sensitive). */
	UPROPERTY(BlueprintReadOnly, Category = "SquadCommand")
	bool bIsAvailable = true;

	/** Tooltip text. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SquadCommand")
	FText Tooltip;
};

/** Context type determined by what the player is looking at. */
UENUM(BlueprintType)
enum class ESHCommandContext : uint8
{
	Default			UMETA(DisplayName = "Default"),			// Open terrain
	Enemy			UMETA(DisplayName = "Enemy"),			// Looking at enemy
	Building		UMETA(DisplayName = "Building"),		// Looking at a structure
	Vehicle			UMETA(DisplayName = "Vehicle"),			// Looking at a vehicle
	Friendly		UMETA(DisplayName = "Friendly"),		// Looking at a squad member
	Objective		UMETA(DisplayName = "Objective")		// Looking at objective
};

/** Formation types for sub-menu. */
UENUM(BlueprintType)
enum class ESHFormationType : uint8
{
	Wedge		UMETA(DisplayName = "Wedge"),
	Line		UMETA(DisplayName = "Line"),
	Column		UMETA(DisplayName = "Column"),
	Echelon		UMETA(DisplayName = "Echelon"),
	Staggered	UMETA(DisplayName = "Staggered Column")
};

/** Rules of engagement for sub-menu. */
UENUM(BlueprintType)
enum class ESHRulesOfEngagement : uint8
{
	HoldFire	UMETA(DisplayName = "Hold Fire"),
	ReturnFire	UMETA(DisplayName = "Return Fire Only"),
	FireAtWill	UMETA(DisplayName = "Fire At Will"),
	Weapons Free UMETA(DisplayName = "Weapons Free")
};

/** Delegate fired when a command is selected. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnSquadCommandIssued, ESHOrderType, OrderType, FVector, TargetLocation, AActor*, TargetActor);

/** Delegate fired when formation changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnFormationChanged, ESHFormationType, NewFormation);

/** Delegate fired when ROE changes. */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnROEChanged, ESHRulesOfEngagement, NewROE);

/**
 * USHSquadCommandWidget
 *
 * Radial wheel for issuing squad orders. Activated by holding a key/button.
 * Context-sensitive — shows different options based on what the player aims at.
 * Features sub-menus for formation and ROE selection. Time slows slightly
 * while the menu is open to allow tactical decision-making under fire.
 */
UCLASS()
class SHATTEREDHORIZON2032_API USHSquadCommandWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	USHSquadCommandWidget(const FObjectInitializer& ObjectInitializer);

	// ------------------------------------------------------------------
	//  UUserWidget overrides
	// ------------------------------------------------------------------
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

	// ------------------------------------------------------------------
	//  Open / close
	// ------------------------------------------------------------------

	/** Open the command wheel. Determines context from player's aim. */
	UFUNCTION(BlueprintCallable, Category = "SH|SquadCommand")
	void OpenCommandMenu();

	/** Close the menu and issue the selected command (if any). */
	UFUNCTION(BlueprintCallable, Category = "SH|SquadCommand")
	void CloseCommandMenu();

	/** Quick-tap: issue the last used command at the current aim point. */
	UFUNCTION(BlueprintCallable, Category = "SH|SquadCommand")
	void QuickCommand();

	/** Whether the command menu is currently open. */
	UFUNCTION(BlueprintPure, Category = "SH|SquadCommand")
	bool IsMenuOpen() const { return bIsOpen; }

	// ------------------------------------------------------------------
	//  Selection
	// ------------------------------------------------------------------

	/** Confirm the currently highlighted option. */
	UFUNCTION(BlueprintCallable, Category = "SH|SquadCommand")
	void ConfirmSelection();

	/** Navigate back from a sub-menu. */
	UFUNCTION(BlueprintCallable, Category = "SH|SquadCommand")
	void NavigateBack();

	/** Get the currently highlighted option index (-1 if none). */
	UFUNCTION(BlueprintPure, Category = "SH|SquadCommand")
	int32 GetHighlightedIndex() const { return HighlightedIndex; }

	// ------------------------------------------------------------------
	//  Context
	// ------------------------------------------------------------------

	/** Get the current command context. */
	UFUNCTION(BlueprintPure, Category = "SH|SquadCommand")
	ESHCommandContext GetCurrentContext() const { return CurrentContext; }

	/** Get the world location the command will target. */
	UFUNCTION(BlueprintPure, Category = "SH|SquadCommand")
	FVector GetCommandTargetLocation() const { return CommandTargetLocation; }

	/** Get the actor the command targets (may be null). */
	UFUNCTION(BlueprintPure, Category = "SH|SquadCommand")
	AActor* GetCommandTargetActor() const { return CommandTargetActor.Get(); }

	// ------------------------------------------------------------------
	//  Squad status (shown while menu is open)
	// ------------------------------------------------------------------

	/** Update squad member info displayed in the command widget. */
	UFUNCTION(BlueprintCallable, Category = "SH|SquadCommand")
	void UpdateSquadMemberInfo(const TArray<FText>& Names, const TArray<float>& HealthFractions, const TArray<FText>& CurrentOrders);

	// ------------------------------------------------------------------
	//  Delegates
	// ------------------------------------------------------------------

	UPROPERTY(BlueprintAssignable, Category = "SH|SquadCommand")
	FOnSquadCommandIssued OnSquadCommandIssued;

	UPROPERTY(BlueprintAssignable, Category = "SH|SquadCommand")
	FOnFormationChanged OnFormationChanged;

	UPROPERTY(BlueprintAssignable, Category = "SH|SquadCommand")
	FOnROEChanged OnROEChanged;

protected:
	// ------------------------------------------------------------------
	//  Internal
	// ------------------------------------------------------------------
	void DetermineContext();
	void BuildMenuForContext(ESHCommandContext Context);
	void BuildDefaultMenu();
	void BuildEnemyContextMenu();
	void BuildBuildingContextMenu();
	void BuildVehicleContextMenu();
	void BuildFriendlyContextMenu();
	void BuildFormationSubMenu(FSHRadialMenuOption& ParentOption);
	void BuildROESubMenu(FSHRadialMenuOption& ParentOption);
	void UpdateHighlightFromMouse(const FVector2D& LocalPosition, const FGeometry& Geometry);
	void ApplyTimeSlowdown(bool bSlow);
	void IssueSelectedCommand();
	void ShowOrderPreview();
	void HideOrderPreview();

	// ------------------------------------------------------------------
	//  Configuration
	// ------------------------------------------------------------------

	/** Radial menu outer radius in pixels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|SquadCommand|Config")
	float WheelRadius = 200.f;

	/** Inner dead zone radius (prevents accidental selection). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|SquadCommand|Config")
	float DeadZoneRadius = 40.f;

	/** Time dilation factor while menu is open (< 1.0 = slow). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|SquadCommand|Config", meta = (ClampMin = "0.1", ClampMax = "1.0"))
	float TimeSlowFactor = 0.3f;

	/** Trace distance for context determination (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|SquadCommand|Config")
	float ContextTraceDistance = 50000.f;

	/** Default menu options — base set always available. */
	UPROPERTY(EditDefaultsOnly, Category = "SH|SquadCommand|Menu")
	TArray<FSHRadialMenuOption> DefaultMenuOptions;

	/** Wheel background color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|SquadCommand|Style")
	FLinearColor WheelBackgroundColor = FLinearColor(0.05f, 0.05f, 0.05f, 0.8f);

	/** Highlighted segment color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|SquadCommand|Style")
	FLinearColor HighlightColor = FLinearColor(0.2f, 0.5f, 0.2f, 0.9f);

	/** Unavailable option color. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|SquadCommand|Style")
	FLinearColor UnavailableColor = FLinearColor(0.3f, 0.3f, 0.3f, 0.4f);

	/** Font for option labels. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SH|SquadCommand|Style")
	FSlateFontInfo OptionFont;

private:
	// ------------------------------------------------------------------
	//  Runtime state
	// ------------------------------------------------------------------

	bool bIsOpen = false;
	bool bInSubMenu = false;

	/** Current menu options (may be a sub-menu). */
	TArray<FSHRadialMenuOption> ActiveMenuOptions;

	/** Parent menu stack for sub-menu navigation. */
	TArray<TArray<FSHRadialMenuOption>> MenuStack;

	/** Currently highlighted segment index. */
	int32 HighlightedIndex = -1;

	/** Current command context. */
	ESHCommandContext CurrentContext = ESHCommandContext::Default;

	/** Command target location in world space. */
	FVector CommandTargetLocation = FVector::ZeroVector;

	/** Command target actor (if aiming at one). */
	TWeakObjectPtr<AActor> CommandTargetActor;

	/** Last used order type for quick-tap. */
	ESHOrderType LastUsedOrderType;

	/** Last used target location for quick-tap. */
	FVector LastCommandLocation = FVector::ZeroVector;

	/** Squad member display data. */
	TArray<FText> SquadNames;
	TArray<float> SquadHealths;
	TArray<FText> SquadOrders;

	/** Mouse position relative to widget center. */
	FVector2D MouseOffset = FVector2D::ZeroVector;

	/** Whether time is currently slowed. */
	bool bTimeDilated = false;

	/** Preview actor for showing where the order will execute. */
	UPROPERTY()
	TWeakObjectPtr<AActor> OrderPreviewActor;
};
