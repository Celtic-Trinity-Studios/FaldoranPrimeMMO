// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class FaldoranPrimeMMOEditorTarget : TargetRules
{
	public FaldoranPrimeMMOEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		ExtraModuleNames.Add("FaldoranPrimeMMO");
	}
}
