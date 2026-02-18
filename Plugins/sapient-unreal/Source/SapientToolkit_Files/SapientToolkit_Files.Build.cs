// Copyright 2024 - 2025 Sapient Technology, Inc.

using UnrealBuildTool;

public class SapientToolkit_Files : ModuleRules
{
    public SapientToolkit_Files(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.CPlusPlus;
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        bUsePrecompiled = true;
        SapientPrecompiledBinaryUtils.Configure(this, Target, "SapientToolkit_Files");

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "SapientCore",
            "SapientToolkit"
        });

        // File operation dependencies
        // OPTIONAL MODULE: Can be disabled if these APIs are incompatible
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UnrealEd",
            "SourceControl",
            "AssetRegistry",
            "Json",
            "JsonUtilities"
        });

        // Optional test framework dependency
        if (Target.Configuration != UnrealTargetConfiguration.Shipping)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "SapientTestFramework"  // Required for tests
            });
        }
    }
}
