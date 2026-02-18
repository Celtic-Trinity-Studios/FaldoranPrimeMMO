// Copyright © 2025 Sapient Technology, Inc.

using UnrealBuildTool;

public class AssetIntelligence : ModuleRules
{
	public AssetIntelligence(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		bUsePrecompiled = true;
        SapientPrecompiledBinaryUtils.Configure(this, Target, "AssetIntelligence");

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"Json",
				"JsonUtilities",
				"SapientTestFramework"  // For toolkit test registration
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"AssetRegistry",
				"ApplicationCore",
				"UMG",
				"EnhancedInput",
				"DeveloperSettings",
				// Sapient toolkit dependencies for tool registration
				"SapientCore",
				"SapientToolkit"
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"UMGEditor",
				"UnrealEd",
				// For Editor-only Blueprint search (FiB and related utilities)
				"BlueprintGraph",
				"Kismet",
				"AIModule",
				"Niagara"
			}
			);
		}
	}
}

