// Copyright Epic Games, Inc. All Rights Reserved.
// Copyright 2024 - 2025 Sapient Technology, Inc.

using System.IO;
using UnrealBuildTool;

public class SapientUI : ModuleRules
{
	public SapientUI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // Binary distribution settings
		bUsePrecompiled = true;
        SapientPrecompiledBinaryUtils.Configure(this, Target, "SapientUI");

		// Optimization settings
#if UE_5_6_OR_LATER
		CppCompileWarningSettings.UndefinedIdentifierWarningLevel = WarningLevel.Off;
#elif UE_5_5_OR_LATER
		UndefinedIdentifierWarningLevel = WarningLevel.Off;
#endif

		PublicIncludePaths.AddRange(
			new[]
			{
				Path.Combine(ModuleDirectory, "Public")
			}
		);

		PrivateIncludePaths.AddRange(
			new[]
			{
				Path.Combine(ModuleDirectory, "Private")
			}
		);

        // Core module dependencies
        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "HTTP",
                "Json",
                "JsonUtilities",
                "SapientCore",       // For Agent API (binary module)
                "SapientToolkit"     // For AgentBuilder (source module)
            }
        );

        // Editor-specific dependencies
        PrivateDependencyModuleNames.AddRange(
            new[]
            {
                "BlueprintEditorLibrary",
                "Projects",
                "Slate",
                "SlateCore",
                "InputCore",         // For keyboard/mouse input
                "EditorStyle",       // For editor styling
                "EditorWidgets",     // For editor widgets
                "ToolMenus",
                "UnrealEd",
                "DesktopPlatform",
                "WorkspaceMenuStructure",  // For tab registration
                "Settings",          // For ISettingsModule
                "ApplicationCore",   // For FPlatformApplicationMisc::ClipboardCopy
                "SapientAutoDownloadFramework"  // For manual update trigger
            }
        );

        // Add definitions for DLL management system
        PublicDefinitions.Add("WITH_SAPIENT_DLL_MANAGEMENT=1");
    }
}
