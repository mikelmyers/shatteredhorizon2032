// Copyright 2026 Shattered Horizon Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SHGameplayTypes.generated.h"

/** Fire mode for the current weapon. */
UENUM(BlueprintType)
enum class ESHFireMode : uint8
{
	Semi   UMETA(DisplayName = "Semi-Auto"),
	Burst  UMETA(DisplayName = "Burst"),
	Auto   UMETA(DisplayName = "Full Auto")
};

/** Shared player/weapon stance representation. */
UENUM(BlueprintType)
enum class ESHStance : uint8
{
	Standing,
	Crouching,
	Prone
};

/** Lean direction. */
UENUM(BlueprintType)
enum class ESHLeanState : uint8
{
	None,
	Left,
	Right
};
