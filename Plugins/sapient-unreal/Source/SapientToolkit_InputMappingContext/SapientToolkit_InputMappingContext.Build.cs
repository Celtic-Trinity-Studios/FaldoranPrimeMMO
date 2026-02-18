// Copyright 2024 - 2025 Sapient Technology, Inc.

using UnrealBuildTool;

public class SapientToolkit_InputMappingContext : ModuleRules
{
	public SapientToolkit_InputMappingContext(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.CPlusPlus;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		bUsePrecompiled = true;
        SapientPrecompiledBinaryUtils.Configure(this, Target, "SapientToolkit_InputMappingContext");

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"SapientCore",
			"SapientToolkit",
			"SapientToolkit_DataAsset",  // Can use base DataAsset utilities
			"SapientTestFramework"  // For test infrastructure
		});

		// Asset manipulation and EnhancedInput dependencies
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"AssetRegistry",
			"AssetTools",
			"EditorSubsystem",
			"Json",
			"JsonUtilities",
			"InputCore",  // Required for FKey
			"EnhancedInput"  // Required for UInputMappingContext
		});
	}
}
