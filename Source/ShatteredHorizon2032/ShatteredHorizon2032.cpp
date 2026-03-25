// Copyright 2026. All Rights Reserved.

#include "ShatteredHorizon2032.h"
#include "Modules/ModuleManager.h"

// Define log categories
DEFINE_LOG_CATEGORY(LogShatteredHorizon);
DEFINE_LOG_CATEGORY(LogSH_Combat);
DEFINE_LOG_CATEGORY(LogSH_Squad);
DEFINE_LOG_CATEGORY(LogSH_AI);
DEFINE_LOG_CATEGORY(LogSH_Network);
DEFINE_LOG_CATEGORY(LogSH_Vehicle);
DEFINE_LOG_CATEGORY(LogSH_Audio);
DEFINE_LOG_CATEGORY(LogSH_Drone);
DEFINE_LOG_CATEGORY(LogSH_EW);

/**
 * Shattered Horizon 2032 — Primary Game Module
 *
 * This module initializes the core game systems for a milsim FPS
 * featuring combined-arms combat during the defense of Taoyuan Beach.
 */
class FShatteredHorizon2032Module : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		UE_LOG(LogShatteredHorizon, Log, TEXT("ShatteredHorizon2032 module starting up."));

		// Register any module-level delegates or subsystem initialization here
	}

	virtual void ShutdownModule() override
	{
		UE_LOG(LogShatteredHorizon, Log, TEXT("ShatteredHorizon2032 module shutting down."));

		// Cleanup module-level resources here
	}
};

IMPLEMENT_PRIMARY_GAME_MODULE(FShatteredHorizon2032Module, ShatteredHorizon2032, "ShatteredHorizon2032");
