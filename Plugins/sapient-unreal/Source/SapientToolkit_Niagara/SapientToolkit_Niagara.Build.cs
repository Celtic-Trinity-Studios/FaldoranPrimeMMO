// Copyright 2024 - 2025 Sapient Technology, Inc.

using UnrealBuildTool;

public class SapientToolkit_Niagara : ModuleRules
{
	public SapientToolkit_Niagara(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.CPlusPlus;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		bUsePrecompiled = true;
        SapientPrecompiledBinaryUtils.Configure(this, Target, "SapientToolkit_Niagara");

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"SapientCore",
			"SapientToolkit",
			"SapientTestFramework"  // For test infrastructure
		});

		// Niagara read-only dependencies (minimized)
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"Json",
			"JsonUtilities",
			"Niagara"               // Required for UNiagaraSystem and related types
		});
	}
}
