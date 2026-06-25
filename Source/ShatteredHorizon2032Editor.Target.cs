// Copyright 2026. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class ShatteredHorizon2032EditorTarget : TargetRules
{
	public ShatteredHorizon2032EditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		bOverrideBuildEnvironment = true;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;

		ExtraModuleNames.AddRange(new string[]
		{
			"ShatteredHorizon2032"
		});
	}
}
