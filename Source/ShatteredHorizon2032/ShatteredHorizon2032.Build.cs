// Copyright 2026. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class ShatteredHorizon2032 : ModuleRules
{
	public ShatteredHorizon2032(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		IWYUSupport = IWYUSupport.Full;
		CppStandard = CppStandardVersion.Cpp20;
		PublicIncludePaths.Add(ModuleDirectory);
		PrivateIncludePaths.Add(ModuleDirectory);

		// Precompiled header
		PrivatePCHHeaderFile = "ShatteredHorizon2032.h";

		// Core Engine modules
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"Slate",
			"SlateCore",
			"UMG",
			"RenderCore",
			"RHI",
			"PhysicsCore",
			"Chaos",
			"ChaosVehiclesCore",
			"NetCore"
		});

		// Gameplay framework
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// Enhanced Input
			"EnhancedInput",

			// AI & Navigation
			"AIModule",
			"NavigationSystem",
			"GameplayTasks",

			// Gameplay Ability System
			"GameplayAbilities",
			"GameplayTags",

			// Mass Entity (crowd/AI simulation)
			"MassEntity",
			"MassCommon",
			"MassActors",
			"MassSpawner",
			"MassGameplayDebug",

			// Animation
			"AnimGraphRuntime",
			"MotionWarping",
			"AnimationWarpingRuntime",
			"ContextualAnimation",

			// VFX & Audio
			"Niagara",
			"NiagaraCore",
			"MetasoundEngine",
			"MetasoundFrontend",
			"AudioMixer",
			"AudioExtensions",

			// Rendering & World
			"Landscape",
			"Foliage",
			"Water",
			"PCG",
			"GeometryCore",
			"GeometryCollectionEngine",

			// Networking
			"OnlineSubsystem",
			"OnlineSubsystemUtils",
			"NetCore",

			// UI
			"CommonUI",
			"CommonInput",

			// Modular Gameplay
			"ModularGameplay",
			"GameFeatures",

			// Utilities
			"DeveloperSettings",
			"PropertyPath",
			"StructUtils",
			"Json",
			"JsonUtilities",
			"HTTP",
			"WebSockets"
		});

		// Editor-only modules
		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"UnrealEd",
				"EditorFramework",
				"EditorSubsystem"
			});
		}

		// Platform-specific modules
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PublicDependencyModuleNames.Add("D3D12RHI");
		}

		// Enable exceptions for third-party library integration
		bEnableExceptions = true;

		// Optimize for non-editor builds
		OptimizeCode = Target.bBuildEditor
			? CodeOptimization.Never
			: CodeOptimization.Always;
	}
}
