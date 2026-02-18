// Copyright 2024 - 2025 Sapient Technology, Inc.

using UnrealBuildTool;

public class SapientToolkit_Blackboard : ModuleRules
{
    public SapientToolkit_Blackboard(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.CPlusPlus;
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        bUsePrecompiled = true;
        SapientPrecompiledBinaryUtils.Configure(this, Target, "SapientToolkit_Blackboard");

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "SapientCore",
            "SapientToolkit"
        });

        // Blackboard-specific dependencies
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UnrealEd",
            "AIModule",
            "Json",
            "JsonUtilities"
        });

        // Add test framework dependency for non-shipping builds
        if (Target.Configuration != UnrealTargetConfiguration.Shipping)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "SapientTestFramework"
            });
        }
    }
}

