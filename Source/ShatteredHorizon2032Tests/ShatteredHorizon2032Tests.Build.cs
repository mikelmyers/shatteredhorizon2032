// Copyright 2026. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class ShatteredHorizon2032Tests : ModuleRules
{
	public ShatteredHorizon2032Tests(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;
		bEnableExceptions = true;
		PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "ShatteredHorizon2032"));
		PrivateIncludePaths.Add(Path.Combine(ModuleDirectory, "..", "ShatteredHorizon2032"));

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
			"GameplayAbilities",
			"GameplayTags",
			"EnhancedInput",
			"Niagara",
			"CommonUI",
			"PhysicsCore",
			"Json",
			"JsonUtilities",
			"HTTP",
			"StructUtils"
		});
	}
}
