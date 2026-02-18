// Copyright 2024 - 2025 Sapient Technology, Inc.

using UnrealBuildTool;

public class SapientToolkit_Blueprint : ModuleRules
{
    public SapientToolkit_Blueprint(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.CPlusPlus;
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        bUsePrecompiled = true;
        SapientPrecompiledBinaryUtils.Configure(this, Target, "SapientToolkit_Blueprint");

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "SapientCore",
            "SapientToolkit",
            "SapientTestFramework",
            "AssetIntelligence"
        });

        // All Blueprint-specific dependencies
        // OPTIONAL MODULE: Can be disabled if these APIs are incompatible
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "UnrealEd",
            "BlueprintGraph",
            "KismetCompiler",
            "Kismet",
            "GraphEditor",
            "AnimGraph",
            "UMG",
            "UMGEditor",
            "MovieScene",
            "PropertyEditor",
            "EditorSubsystem",
            "SubobjectDataInterface",
            "Json",
            "JsonUtilities",
            "SapientTestFramework",
            "GameplayTags",
            "GameplayTagsEditor",
            "Projects",
            "ApplicationCore"
        });

        // AnimationBlueprintLibrary is editor-only, conditionally link it
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "AnimationBlueprintLibrary",
                "AnimGraph"
            });
        }
    }
}
