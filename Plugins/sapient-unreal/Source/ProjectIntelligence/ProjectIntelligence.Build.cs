// Copyright © 2025 Sapient Technology, Inc.

using UnrealBuildTool;

public class ProjectIntelligence : ModuleRules
{
	public ProjectIntelligence(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		bUsePrecompiled = true;
        SapientPrecompiledBinaryUtils.Configure(this, Target, "ProjectIntelligence");

	PublicDependencyModuleNames.AddRange(
		new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"AIModule",          // For BehaviorTree support
			"Json",
			"JsonUtilities",
			"SapientTestFramework"  // For test macros and FGenericToolTestBase
		}
	);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"SapientCore",           // For CodeIntelligence
				"SapientToolkit",        // For FSapientToolkitModule::GetToolExecutor
				"AssetIntelligence",     // For asset queries
				"AssetRegistry"
			}
		);

		if (Target.bBuildEditor)
		{
			PrivateDependencyModuleNames.AddRange(
				new string[]
				{
					"UnrealEd",
					"SourceCodeAccess"  // For FSourceCodeNavigation
				}
			);
		}
	}
}

