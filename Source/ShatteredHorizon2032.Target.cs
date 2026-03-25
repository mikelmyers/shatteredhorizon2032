// Copyright 2026. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class ShatteredHorizon2032Target : TargetRules
{
	public ShatteredHorizon2032Target(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.V4;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4;

		ExtraModuleNames.AddRange(new string[]
		{
			"ShatteredHorizon2032"
		});

		// Shipping build optimizations
		if (Configuration == UnrealTargetConfiguration.Shipping)
		{
			bUseLoggingInShipping = false;
			bUseChecksInShipping = false;
		}

		// Enable Iris for profiling in non-shipping builds
		if (Configuration != UnrealTargetConfiguration.Shipping)
		{
			bWithPerfCounters = true;
		}

		// Platform-specific settings
		WindowsPlatform.bStrictConformanceMode = true;
	}
}
