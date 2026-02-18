// Copyright 2024 - 2025 Sapient Technology, Inc.

using UnrealBuildTool;

public class SapientToolkit_Struct : ModuleRules
{
	public SapientToolkit_Struct(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.CPlusPlus;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		bUsePrecompiled = true;
        SapientPrecompiledBinaryUtils.Configure(this, Target, "SapientToolkit_Struct");

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"SapientCore",
			"SapientToolkit",
            "SapientTestFramework"
        });

		// Struct manipulation dependencies
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"AssetRegistry",
			"Json",
			"JsonUtilities",
			"AssetTools",
			"Kismet",
			"BlueprintGraph",
			"SapientToolkit_DataAsset"
		});
	}
}
