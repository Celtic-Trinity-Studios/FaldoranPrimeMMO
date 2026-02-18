// Copyright 2024 - 2025 Sapient Technology, Inc.

using UnrealBuildTool;

public class SapientToolkit_Compilation : ModuleRules
{
	public SapientToolkit_Compilation(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.CPlusPlus;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		bUsePrecompiled = true;
        SapientPrecompiledBinaryUtils.Configure(this, Target, "SapientToolkit_Compilation");

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"SapientCore",
			"SapientToolkit",
			"SapientTestFramework"  // For dynamic test registration
		});

		// Compilation toolkit dependencies
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"DeveloperSettings",    // For USapientCompilationSettings
			"HotReload",           // For IHotReloadInterface
			"Projects",             // For module detection
			"Json",
			"JsonUtilities",
			"DesktopPlatform",     // For platform utilities
			"Settings",            // For ISettingsModule - required for Project Settings registration
			"ApplicationCore"      // For FPlatformApplicationMisc::PumpMessages
		});

		// Platform-specific dependencies
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			PrivateDependencyModuleNames.Add("LiveCoding");  // Windows-only LiveCoding
		}

		// Include paths for Unreal Engine compilation APIs
		PublicIncludePaths.AddRange(new string[]
		{
			// Add public include paths if needed
		});

		PrivateIncludePaths.AddRange(new string[]
		{
			// Add private include paths if needed
		});
	}
}
