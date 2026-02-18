// Copyright 2024 - 2025 Sapient Technology, Inc.

using UnrealBuildTool;

public class SapientToolkit_BehaviorTree : ModuleRules
{
    public SapientToolkit_BehaviorTree(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.CPlusPlus;
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        bUsePrecompiled = true;
        SapientPrecompiledBinaryUtils.Configure(this, Target, "SapientToolkit_BehaviorTree");

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "SapientCore",
            "SapientToolkit",
            "SapientTestFramework"
        });

        // BehaviorTree dependencies
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UnrealEd",
            "AIModule",
            "AIGraph",
            "GameplayTasks",
            "BehaviorTreeEditor",
            "PropertyEditor",
            "Json",
            "JsonUtilities"
        });
    }
}
