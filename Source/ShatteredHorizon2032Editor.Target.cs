// Copyright 2026. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class ShatteredHorizon2032EditorTarget : TargetRules
{
	public ShatteredHorizon2032EditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V4;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_4;

		ExtraModuleNames.AddRange(new string[]
		{
			"ShatteredHorizon2032"
		});
	}
}
