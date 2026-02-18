// Copyright 2024 - 2025 Sapient Technology, Inc.

using UnrealBuildTool;

public class SapientToolkit_Enum : ModuleRules
{
	public SapientToolkit_Enum(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.CPlusPlus;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		bUsePrecompiled = true;
        SapientPrecompiledBinaryUtils.Configure(this, Target, "SapientToolkit_Enum");

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"SapientCore",
			"SapientToolkit",
			"SapientTestFramework"
		});

		// Enum dependencies
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
			"Kismet"
		});
	}
}
