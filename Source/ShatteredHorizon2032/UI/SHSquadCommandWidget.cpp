// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#include "SHSquadCommandWidget.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/WorldSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "CollisionQueryParams.h"
#include "Squad/SHSquadOrder.h"

DEFINE_LOG_CATEGORY(LogSH_SquadCommand);

// ======================================================================
//  Construction
// ======================================================================

USHSquadCommandWidget::USHSquadCommandWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, LastUsedOrderType(ESHOrderType::MoveToPosition)
{
	OptionFont = FCoreStyle::GetDefaultFontStyle("Bold", 14);
}

// ======================================================================
//  Widget lifecycle
// ======================================================================

void USHSquadCommandWidget::NativeConstruct()
{
	Super::NativeConstruct();

	// Build default menu if not configured in editor
	if (DefaultMenuOptions.Num() == 0)
	{
		BuildDefaultMenu();
	}
}

void USHSquadCommandWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);

	if (!bIsOpen) return;

	// Continuously update context while open (player may move aim)
	DetermineContext();
	ShowOrderPreview();
}

FReply USHSquadCommandWidget::NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
	if (bIsOpen)
	{
		UpdateHighlightFromMouse(InMouseEvent.GetScreenSpacePosition(), InGeometry);
	}

	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

// ======================================================================
//  Open / close
// ======================================================================

void USHSquadCommandWidget::OpenCommandMenu()
{
	if (bIsOpen) return;

	bIsOpen = true;
	bInSubMenu = false;
	HighlightedIndex = -1;
	MenuStack.Empty();

	// Determine context from crosshair trace
	DetermineContext();

	// Build context-appropriate menu
	BuildMenuForContext(CurrentContext);

	// Show the widget
	SetVisibility(ESlateVisibility::Visible);

	// Apply time slowdown
	ApplyTimeSlowdown(true);

	// Capture mouse to the widget center
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		PC->SetShowMouseCursor(true);

		// Center mouse on widget
		FVector2D ViewportSize;
		GEngine->GameViewport->GetViewportSize(ViewportSize);
		PC->SetMouseLocation(ViewportSize.X * 0.5f, ViewportSize.Y * 0.5f);
	}

	UE_LOG(LogSH_SquadCommand, Log, TEXT("Command menu opened. Context: %d"), static_cast<int32>(CurrentContext));
}

void USHSquadCommandWidget::CloseCommandMenu()
{
	if (!bIsOpen) return;

	// Issue command if something is highlighted
	if (HighlightedIndex >= 0 && HighlightedIndex < ActiveMenuOptions.Num())
	{
		IssueSelectedCommand();
	}

	bIsOpen = false;
	bInSubMenu = false;

	SetVisibility(ESlateVisibility::Collapsed);
	HideOrderPreview();

	// Restore time
	ApplyTimeSlowdown(false);

	// Release mouse
	APlayerController* PC = GetOwningPlayer();
	if (PC)
	{
		PC->SetShowMouseCursor(false);
	}

	UE_LOG(LogSH_SquadCommand, Log, TEXT("Command menu closed."));
}

void USHSquadCommandWidget::QuickCommand()
{
	// Issue the last used command at current aim point
	DetermineContext();

	OnSquadCommandIssued.Broadcast(LastUsedOrderType, CommandTargetLocation, CommandTargetActor.Get());

	UE_LOG(LogSH_SquadCommand, Log, TEXT("Quick command: OrderType=%d at %s"),
		static_cast<int32>(LastUsedOrderType), *CommandTargetLocation.ToString());
}

// ======================================================================
//  Selection
// ======================================================================

void USHSquadCommandWidget::ConfirmSelection()
{
	if (!bIsOpen) return;
	if (HighlightedIndex < 0 || HighlightedIndex >= ActiveMenuOptions.Num()) return;

	const FSHRadialMenuOption& Selected = ActiveMenuOptions[HighlightedIndex];

	if (!Selected.bIsAvailable) return;

	if (Selected.bIsSubMenu && Selected.SubOptions.Num() > 0)
	{
		// Push current menu onto stack and enter sub-menu
		MenuStack.Add(ActiveMenuOptions);
		ActiveMenuOptions = Selected.SubOptions;
		bInSubMenu = true;
		HighlightedIndex = -1;

		UE_LOG(LogSH_SquadCommand, Verbose, TEXT("Entered sub-menu: %s (%d options)"),
			*Selected.Label.ToString(), Selected.SubOptions.Num());
	}
	else
	{
		IssueSelectedCommand();
		CloseCommandMenu();
	}
}

void USHSquadCommandWidget::NavigateBack()
{
	if (!bInSubMenu || MenuStack.Num() == 0) return;

	ActiveMenuOptions = MenuStack.Pop();
	bInSubMenu = (MenuStack.Num() > 0);
	HighlightedIndex = -1;
}

// ======================================================================
//  Context determination
// ======================================================================

void USHSquadCommandWidget::DetermineContext()
{
	APlayerController* PC = GetOwningPlayer();
	if (!PC) return;

	APawn* Pawn = PC->GetPawn();
	if (!Pawn) return;

	UWorld* World = GetWorld();
	if (!World) return;

	FVector Start;
	FRotator Direction;
	PC->GetPlayerViewPoint(Start, Direction);
	FVector End = Start + Direction.Vector() * ContextTraceDistance;

	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CommandContext), true, Pawn);

	CurrentContext = ESHCommandContext::Default;
	CommandTargetLocation = End;
	CommandTargetActor = nullptr;

	if (World->LineTraceSingleByChannel(Hit, Start, End, ECC_Visibility, Params))
	{
		CommandTargetLocation = Hit.ImpactPoint;
		CommandTargetActor = Hit.GetActor();

		if (AActor* HitActor = Hit.GetActor())
		{
			// Determine context from hit actor tags
			if (HitActor->ActorHasTag(TEXT("Enemy")))
			{
				CurrentContext = ESHCommandContext::Enemy;
			}
			else if (HitActor->ActorHasTag(TEXT("Building")) || HitActor->ActorHasTag(TEXT("Structure")))
			{
				CurrentContext = ESHCommandContext::Building;
			}
			else if (HitActor->ActorHasTag(TEXT("Vehicle")))
			{
				CurrentContext = ESHCommandContext::Vehicle;
			}
			else if (HitActor->ActorHasTag(TEXT("Friendly")) || HitActor->ActorHasTag(TEXT("SquadMember")))
			{
				CurrentContext = ESHCommandContext::Friendly;
			}
			else if (HitActor->ActorHasTag(TEXT("Objective")))
			{
				CurrentContext = ESHCommandContext::Objective;
			}
		}
	}
}

// ======================================================================
//  Menu building
// ======================================================================

void USHSquadCommandWidget::BuildMenuForContext(ESHCommandContext Context)
{
	ActiveMenuOptions.Empty();

	switch (Context)
	{
	case ESHCommandContext::Enemy:		BuildEnemyContextMenu(); break;
	case ESHCommandContext::Building:	BuildBuildingContextMenu(); break;
	case ESHCommandContext::Vehicle:		BuildVehicleContextMenu(); break;
	case ESHCommandContext::Friendly:	BuildFriendlyContextMenu(); break;
	default:							BuildDefaultMenu(); break;
	}
}

void USHSquadCommandWidget::BuildDefaultMenu()
{
	ActiveMenuOptions.Empty();

	auto AddOption = [this](ESHOrderType Type, const FString& Label, const FString& Tip, bool bSubMenu = false)
	{
		FSHRadialMenuOption Opt;
		Opt.OrderType = Type;
		Opt.Label = FText::FromString(Label);
		Opt.Tooltip = FText::FromString(Tip);
		Opt.bIsSubMenu = bSubMenu;
		Opt.bIsAvailable = true;
		ActiveMenuOptions.Add(Opt);
		return &ActiveMenuOptions.Last();
	};

	AddOption(ESHOrderType::MoveToPosition,		TEXT("Move"),		TEXT("Order squad to move to location"));
	AddOption(ESHOrderType::HoldPosition,		TEXT("Hold"),		TEXT("Order squad to hold current position"));
	AddOption(ESHOrderType::ProvideOverwatch,	TEXT("Overwatch"),	TEXT("Set up overwatch on area"));
	AddOption(ESHOrderType::Suppress,			TEXT("Suppress"),	TEXT("Lay down suppressive fire"));
	AddOption(ESHOrderType::FlankLeft,			TEXT("Flank Left"),	TEXT("Flank enemy from the left"));
	AddOption(ESHOrderType::FlankRight,			TEXT("Flank Right"),TEXT("Flank enemy from the right"));
	AddOption(ESHOrderType::FallBack,			TEXT("Fall Back"),	TEXT("Retreat to previous position"));
	AddOption(ESHOrderType::Rally,				TEXT("Rally"),		TEXT("Regroup on squad leader"));

	// Formation sub-menu
	FSHRadialMenuOption FormationOpt;
	FormationOpt.Label = FText::FromString(TEXT("Formation"));
	FormationOpt.Tooltip = FText::FromString(TEXT("Change squad formation"));
	FormationOpt.bIsSubMenu = true;
	FormationOpt.bIsAvailable = true;
	FormationOpt.OrderType = ESHOrderType::None;
	BuildFormationSubMenu(FormationOpt);
	ActiveMenuOptions.Add(FormationOpt);

	// ROE sub-menu
	FSHRadialMenuOption ROEOpt;
	ROEOpt.Label = FText::FromString(TEXT("ROE"));
	ROEOpt.Tooltip = FText::FromString(TEXT("Set rules of engagement"));
	ROEOpt.bIsSubMenu = true;
	ROEOpt.bIsAvailable = true;
	ROEOpt.OrderType = ESHOrderType::None;
	BuildROESubMenu(ROEOpt);
	ActiveMenuOptions.Add(ROEOpt);

	// Store as default for fallback
	if (DefaultMenuOptions.Num() == 0)
	{
		DefaultMenuOptions = ActiveMenuOptions;
	}
}

void USHSquadCommandWidget::BuildEnemyContextMenu()
{
	ActiveMenuOptions.Empty();

	auto Add = [this](ESHOrderType Type, const FString& Label, const FString& Tip)
	{
		FSHRadialMenuOption Opt;
		Opt.OrderType = Type;
		Opt.Label = FText::FromString(Label);
		Opt.Tooltip = FText::FromString(Tip);
		Opt.bIsAvailable = true;
		ActiveMenuOptions.Add(Opt);
	};

	Add(ESHOrderType::Suppress,			TEXT("Suppress"),		TEXT("Suppress this target"));
	Add(ESHOrderType::MarkTarget,		TEXT("Mark Target"),	TEXT("Mark enemy for squad awareness"));
	Add(ESHOrderType::FlankLeft,		TEXT("Flank Left"),		TEXT("Flank target from left"));
	Add(ESHOrderType::FlankRight,		TEXT("Flank Right"),	TEXT("Flank target from right"));
	Add(ESHOrderType::FreeFire,			TEXT("Fire At Will"),	TEXT("Engage all visible enemies"));
	Add(ESHOrderType::CeaseFire,		TEXT("Cease Fire"),		TEXT("Stop engaging"));
	Add(ESHOrderType::ProvideOverwatch,	TEXT("Overwatch"),		TEXT("Watch this area"));
	Add(ESHOrderType::CoverMe,			TEXT("Cover Me"),		TEXT("Provide covering fire while I move"));
}

void USHSquadCommandWidget::BuildBuildingContextMenu()
{
	ActiveMenuOptions.Empty();

	auto Add = [this](ESHOrderType Type, const FString& Label, const FString& Tip)
	{
		FSHRadialMenuOption Opt;
		Opt.OrderType = Type;
		Opt.Label = FText::FromString(Label);
		Opt.Tooltip = FText::FromString(Tip);
		Opt.bIsAvailable = true;
		ActiveMenuOptions.Add(Opt);
	};

	Add(ESHOrderType::StackUp,			TEXT("Stack Up"),			TEXT("Stack up on entry point"));
	Add(ESHOrderType::BreachAndClear,	TEXT("Breach & Clear"),		TEXT("Breach door and clear room"));
	Add(ESHOrderType::MoveToPosition,	TEXT("Enter Building"),		TEXT("Move into building"));
	Add(ESHOrderType::HoldPosition,		TEXT("Hold Entrance"),		TEXT("Hold this entrance"));
	Add(ESHOrderType::ProvideOverwatch,	TEXT("Watch Windows"),		TEXT("Watch building windows"));
	Add(ESHOrderType::Suppress,			TEXT("Suppress Building"),	TEXT("Suppress the building"));
}

void USHSquadCommandWidget::BuildVehicleContextMenu()
{
	ActiveMenuOptions.Empty();

	auto Add = [this](ESHOrderType Type, const FString& Label, const FString& Tip)
	{
		FSHRadialMenuOption Opt;
		Opt.OrderType = Type;
		Opt.Label = FText::FromString(Label);
		Opt.Tooltip = FText::FromString(Tip);
		Opt.bIsAvailable = true;
		ActiveMenuOptions.Add(Opt);
	};

	Add(ESHOrderType::MarkTarget,		TEXT("Mark Vehicle"),	TEXT("Mark vehicle for squad"));
	Add(ESHOrderType::Suppress,			TEXT("Engage Vehicle"),	TEXT("Focus fire on vehicle"));
	Add(ESHOrderType::MoveToPosition,	TEXT("Take Cover"),		TEXT("Use as cover"));
	Add(ESHOrderType::FlankLeft,		TEXT("Flank Left"),		TEXT("Flank vehicle from left"));
	Add(ESHOrderType::FlankRight,		TEXT("Flank Right"),	TEXT("Flank vehicle from right"));
	Add(ESHOrderType::FallBack,			TEXT("Fall Back"),		TEXT("Retreat from vehicle"));
}

void USHSquadCommandWidget::BuildFriendlyContextMenu()
{
	ActiveMenuOptions.Empty();

	auto Add = [this](ESHOrderType Type, const FString& Label, const FString& Tip)
	{
		FSHRadialMenuOption Opt;
		Opt.OrderType = Type;
		Opt.Label = FText::FromString(Label);
		Opt.Tooltip = FText::FromString(Tip);
		Opt.bIsAvailable = true;
		ActiveMenuOptions.Add(Opt);
	};

	Add(ESHOrderType::Medic,			TEXT("Medic!"),			TEXT("Request medical attention for this member"));
	Add(ESHOrderType::Rally,			TEXT("Rally On Me"),	TEXT("Regroup on your position"));
	Add(ESHOrderType::CoverMe,			TEXT("Cover Me"),		TEXT("Request covering fire"));
	Add(ESHOrderType::ProvideOverwatch,	TEXT("Overwatch"),		TEXT("Provide overwatch"));
	Add(ESHOrderType::HoldPosition,		TEXT("Hold Here"),		TEXT("Hold current position"));
}

void USHSquadCommandWidget::BuildFormationSubMenu(FSHRadialMenuOption& ParentOption)
{
	auto Add = [&ParentOption](const FString& Label)
	{
		FSHRadialMenuOption Sub;
		Sub.OrderType = ESHOrderType::MoveToPosition; // Formation change combined with move
		Sub.Label = FText::FromString(Label);
		Sub.bIsAvailable = true;
		ParentOption.SubOptions.Add(Sub);
	};

	Add(TEXT("Wedge"));
	Add(TEXT("Line Abreast"));
	Add(TEXT("Column"));
	Add(TEXT("Echelon Left"));
	Add(TEXT("Echelon Right"));
	Add(TEXT("Staggered Column"));
}

void USHSquadCommandWidget::BuildROESubMenu(FSHRadialMenuOption& ParentOption)
{
	auto Add = [&ParentOption](ESHOrderType Type, const FString& Label)
	{
		FSHRadialMenuOption Sub;
		Sub.OrderType = Type;
		Sub.Label = FText::FromString(Label);
		Sub.bIsAvailable = true;
		ParentOption.SubOptions.Add(Sub);
	};

	Add(ESHOrderType::CeaseFire,	TEXT("Hold Fire"));
	Add(ESHOrderType::CoverMe,		TEXT("Return Fire Only"));
	Add(ESHOrderType::FreeFire,		TEXT("Fire At Will"));
	Add(ESHOrderType::FreeFire,		TEXT("Weapons Free"));
}

// ======================================================================
//  Highlight from mouse position
// ======================================================================

void USHSquadCommandWidget::UpdateHighlightFromMouse(const FVector2D& ScreenPosition, const FGeometry& Geometry)
{
	if (ActiveMenuOptions.Num() == 0)
	{
		HighlightedIndex = -1;
		return;
	}

	// Get widget center in screen space
	FVector2D LocalSize = Geometry.GetLocalSize();
	FVector2D Center = Geometry.LocalToAbsolute(LocalSize * 0.5f);

	FVector2D Delta = ScreenPosition - Center;
	float Distance = Delta.Size();

	// Dead zone check
	if (Distance < DeadZoneRadius)
	{
		HighlightedIndex = -1;
		return;
	}

	// Compute angle and map to segment
	float Angle = FMath::Atan2(Delta.Y, Delta.X);
	if (Angle < 0.f) Angle += 2.f * PI;

	float SegmentArc = (2.f * PI) / ActiveMenuOptions.Num();

	// Offset so segment 0 is at top (12 o'clock)
	float AdjustedAngle = Angle + (PI * 0.5f) + (SegmentArc * 0.5f);
	if (AdjustedAngle < 0.f) AdjustedAngle += 2.f * PI;
	AdjustedAngle = FMath::Fmod(AdjustedAngle, 2.f * PI);

	HighlightedIndex = FMath::FloorToInt(AdjustedAngle / SegmentArc);
	HighlightedIndex = FMath::Clamp(HighlightedIndex, 0, ActiveMenuOptions.Num() - 1);

	MouseOffset = Delta;
}

// ======================================================================
//  Time dilation
// ======================================================================

void USHSquadCommandWidget::ApplyTimeSlowdown(bool bSlow)
{
	UWorld* World = GetWorld();
	if (!World) return;

	AWorldSettings* Settings = World->GetWorldSettings();
	if (!Settings) return;

	if (bSlow && !bTimeDilated)
	{
		Settings->SetTimeDilation(TimeSlowFactor);
		bTimeDilated = true;
		UE_LOG(LogSH_SquadCommand, Verbose, TEXT("Time dilation applied: %.2f"), TimeSlowFactor);
	}
	else if (!bSlow && bTimeDilated)
	{
		Settings->SetTimeDilation(1.f);
		bTimeDilated = false;
		UE_LOG(LogSH_SquadCommand, Verbose, TEXT("Time dilation restored to 1.0"));
	}
}

// ======================================================================
//  Issue command
// ======================================================================

void USHSquadCommandWidget::IssueSelectedCommand()
{
	if (HighlightedIndex < 0 || HighlightedIndex >= ActiveMenuOptions.Num()) return;

	const FSHRadialMenuOption& Selected = ActiveMenuOptions[HighlightedIndex];
	if (!Selected.bIsAvailable) return;

	LastUsedOrderType = Selected.OrderType;
	LastCommandLocation = CommandTargetLocation;

	OnSquadCommandIssued.Broadcast(Selected.OrderType, CommandTargetLocation, CommandTargetActor.Get());

	UE_LOG(LogSH_SquadCommand, Log, TEXT("Command issued: %s (OrderType=%d) at %s, target=%s"),
		*Selected.Label.ToString(),
		static_cast<int32>(Selected.OrderType),
		*CommandTargetLocation.ToString(),
		CommandTargetActor.IsValid() ? *CommandTargetActor->GetName() : TEXT("none"));
}

// ======================================================================
//  Order preview
// ======================================================================

void USHSquadCommandWidget::ShowOrderPreview()
{
	// In production: spawn or update a decal/widget at CommandTargetLocation
	// showing where the squad will move to, with directional indicators for
	// flank orders, breach points for building orders, etc.
}

void USHSquadCommandWidget::HideOrderPreview()
{
	if (OrderPreviewActor.IsValid())
	{
		OrderPreviewActor->Destroy();
		OrderPreviewActor = nullptr;
	}
}

// ======================================================================
//  Squad info update
// ======================================================================

void USHSquadCommandWidget::UpdateSquadMemberInfo(const TArray<FText>& Names, const TArray<float>& HealthFractions,
												   const TArray<FText>& CurrentOrders)
{
	SquadNames = Names;
	SquadHealths = HealthFractions;
	SquadOrders = CurrentOrders;
}
