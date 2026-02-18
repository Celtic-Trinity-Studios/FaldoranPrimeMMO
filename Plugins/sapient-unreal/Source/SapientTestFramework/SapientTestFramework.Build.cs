// Copyright 2024 - 2025 Sapient Technology, Inc.

using UnrealBuildTool;

public class SapientTestFramework : ModuleRules
{
	public SapientTestFramework(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.CPlusPlus;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		bUsePrecompiled = true;
        SapientPrecompiledBinaryUtils.Configure(this, Target, "SapientTestFramework");

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"SapientCore",  // Core interfaces and types
			"SapientToolkit"  // For AssetOperationBase (used by AssetToolTestBase)
		});

	PrivateDependencyModuleNames.AddRange(new string[]
	{
		"Core",
		"CoreUObject",
		"Engine",
		"UnrealEd",
		"Json",
		"JsonUtilities",
		"AssetRegistry",
		"AssetTools",       // For asset management in AssetToolTestBase
		"DataTableEditor",  // For DataTable utilities in AssetToolTestBase
		"Kismet",           // For BlueprintEditorUtils
		"BlueprintGraph",   // For EdGraphSchema_K2
		"UMG",              // For Widget Blueprint support (UWidgetBlueprint, UWidgetTree)
		"UMGEditor"         // For Widget Blueprint editor utilities
	});

		// Optional: Only compile in editor with automation tests
		if (Target.Configuration != UnrealTargetConfiguration.Shipping)
		{
			PrivateDependencyModuleNames.Add("AutomationController");
		}
	}
}
