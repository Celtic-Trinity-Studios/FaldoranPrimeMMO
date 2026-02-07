// Copyright Celtic Trinity Studios, 2026. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;

public class FaldoranPrimeMMOClientTarget : TargetRules
{
	public FaldoranPrimeMMOClientTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Client;
		DefaultBuildSettings = BuildSettingsVersion.V6;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
		ExtraModuleNames.Add("FaldoranPrimeMMO");
	}
}
