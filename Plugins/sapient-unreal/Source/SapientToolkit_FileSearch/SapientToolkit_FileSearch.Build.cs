// Copyright 2024 - 2025 Sapient Technology, Inc.

using UnrealBuildTool;

public class SapientToolkit_FileSearch : ModuleRules
{
	public SapientToolkit_FileSearch(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		bUsePrecompiled = true;
        SapientPrecompiledBinaryUtils.Configure(this, Target, "SapientToolkit_FileSearch");

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"SapientCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"SapientToolkit",
			"Json",
			"JsonUtilities"
		});

		// Define API macro for module exports
		PublicDefinitions.Add("SAPIENTTOOLKIT_FILESEARCH_EXPORTS=1");

		// Add test framework dependency for non-shipping builds
		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				"SapientTestFramework"  // Required for tests
			});
		}
	}
}
