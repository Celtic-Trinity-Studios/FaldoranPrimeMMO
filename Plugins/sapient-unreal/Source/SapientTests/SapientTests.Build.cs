// Copyright 2024 - 2025 Sapient Technology, Inc.

using UnrealBuildTool;

public class SapientTests : ModuleRules
{
    public SapientTests(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.CPlusPlus;
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        bUsePrecompiled = true;
        SapientPrecompiledBinaryUtils.Configure(this, Target, "SapientTests");

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "SapientCore",
            "SapientToolkit",
            "SapientTestFramework",   // For test framework infrastructure
            "SapientUI"               // For ViewModel tests
            // Removed toolkit dependencies - tests should only test core modules
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UnrealEd",
            "Json",
            "JsonUtilities",
            "Slate",
            "SlateCore",
            "InputCore",
            "AssetRegistry",      // For asset searching
            "AssetTools",         // For asset management
            "DataTableEditor",    // For DataTable utilities
            "EnhancedInput",      // For InputMappingContext tests
            "KismetCompiler",     // For enum editor utilities
            "AIModule",            // For BehaviorTree tests
            "Projects",
            "SapientAutoDownloadFramework"
        });

        // Optional toolkit modules (tests will check if available)
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                // These are optional - tests will check module availability
                // "SapientToolkit_Blueprint",
                "SapientToolkit_BehaviorTree",  // For BehaviorTree undo tests
                // "SapientToolkit_Assets",
                // "SapientToolkit_Files",
                "SapientToolkit_Enum",
                "SapientToolkit_Struct",
                "SapientToolkit_FileSearch"  // For FileSearch async parity tests
            });
        }
    }
}
