// Copyright 2026. All Rights Reserved.

using UnrealBuildTool;

public class ShatteredHorizon2032Tests : ModuleRules
{
	public ShatteredHorizon2032Tests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;
		bEnableExceptions = true;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"ShatteredHorizon2032",
			"AIModule",
			"NavigationSystem",
			"GameplayTasks",
			"EnhancedInput",
			"Niagara",
			"CommonUI",
			"PhysicsCore"
		});
	}
}
