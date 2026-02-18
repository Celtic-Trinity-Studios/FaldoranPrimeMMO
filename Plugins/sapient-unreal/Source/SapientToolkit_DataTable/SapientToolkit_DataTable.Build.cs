// Copyright 2024 - 2025 Sapient Technology, Inc.

using UnrealBuildTool;

public class SapientToolkit_DataTable : ModuleRules
{
	public SapientToolkit_DataTable(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.CPlusPlus;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		bUsePrecompiled = true;
        SapientPrecompiledBinaryUtils.Configure(this, Target, "SapientToolkit_DataTable");

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"SapientCore",
			"SapientToolkit",
			"SapientToolkit_Struct",
            "SapientTestFramework"
        });

		// DataTable manipulation dependencies
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
			"DataTableEditor",
			"SapientToolkit_DataAsset"
		});
	}
}
