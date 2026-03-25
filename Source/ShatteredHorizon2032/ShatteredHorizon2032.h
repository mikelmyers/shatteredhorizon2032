// Copyright 2026. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

// Module API macro
#ifndef SHATTEREDHORIZON2032_API
	#define SHATTEREDHORIZON2032_API DLLEXPORT
#endif

DECLARE_LOG_CATEGORY_EXTERN(LogShatteredHorizon, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogSH_Combat, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogSH_Squad, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogSH_AI, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogSH_Network, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogSH_Vehicle, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogSH_Audio, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogSH_Drone, Log, All);
DECLARE_LOG_CATEGORY_EXTERN(LogSH_EW, Log, All);

// Convenience macros for logging
#define SH_LOG(CategoryName, Verbosity, Format, ...) \
	UE_LOG(CategoryName, Verbosity, TEXT(Format), ##__VA_ARGS__)

#define SH_WARNING(CategoryName, Format, ...) \
	UE_LOG(CategoryName, Warning, TEXT(Format), ##__VA_ARGS__)

#define SH_ERROR(CategoryName, Format, ...) \
	UE_LOG(CategoryName, Error, TEXT(Format), ##__VA_ARGS__)

// Forward-declare custom collision channels
#define SH_COLLISION_PROJECTILE		ECC_GameTraceChannel1
#define SH_COLLISION_WEAPON_TRACE	ECC_GameTraceChannel2
#define SH_COLLISION_INTERACTION	ECC_GameTraceChannel3
#define SH_COLLISION_VEHICLE		ECC_GameTraceChannel4
#define SH_COLLISION_DRONE			ECC_GameTraceChannel5
