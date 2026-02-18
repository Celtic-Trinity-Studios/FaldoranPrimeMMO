// Copyright 2024 - 2025 Sapient Technology, Inc.

using UnrealBuildTool;

public class SapientToolkit : ModuleRules
{
    public SapientToolkit(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.CPlusPlus;
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        bUsePrecompiled = true;
        SapientPrecompiledBinaryUtils.Configure(this, Target, "SapientToolkit");

        // Depends on SapientCore for interfaces
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "SapientCore"
        });

        // Minimal Unreal dependencies - this base toolkit should always compile
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "SapientAutoDownloadFramework",
            "UnrealEd",          // For FileHelpers, ObjectTools, PackageTools (AssetOperationBase)
            "AssetRegistry",     // For AssetRegistryModule (AssetOperationBase)
            "AssetTools",        // For AssetToolsModule, IAssetTools (AssetOperationBase)
            "BlueprintGraph",    // For UEdGraphSchema_K2 (AssetOperationBase pin type utilities)
            "EnhancedInput",     // For shared EnhancedInputJsonUtils
            "HTTP",
            "Json",
            "JsonUtilities",
            "Projects",
            "SourceControl",
            "DirectoryWatcher"   // For FileChangeListener (P3-02)
        });

        PublicDefinitions.Add("SAPIENTTOOLKIT_EXPORTS=1");
    }
}
