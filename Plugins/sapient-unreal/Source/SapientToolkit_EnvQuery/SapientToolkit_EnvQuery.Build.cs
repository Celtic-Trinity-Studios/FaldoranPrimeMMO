// Copyright 2024 - 2025 Sapient Technology, Inc.

using UnrealBuildTool;

public class SapientToolkit_EnvQuery : ModuleRules
{
	public SapientToolkit_EnvQuery(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.CPlusPlus;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		bUsePrecompiled = true;
        SapientPrecompiledBinaryUtils.Configure(this, Target, "SapientToolkit_EnvQuery");

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"SapientCore",
			"SapientToolkit",
			"SapientTestFramework"
		});

		// EnvQuery specific dependencies
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"AssetRegistry",
			"AssetTools",
			"Json",
			"JsonUtilities",
			"AIModule",           // For Environment Query System
			"GameplayTasks"       // EnvQuery dependency
		});
	}
}
