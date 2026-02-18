using UnrealBuildTool;
using System.IO;

public class SapientAutoUpdateFramework : ModuleRules
{
    public SapientAutoUpdateFramework(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        bUsePrecompiled = true;
        SapientPrecompiledBinaryUtils.Configure(this, Target, "SapientAutoUpdateFramework");

        PublicDefinitions.Add("SAPIENT_AUTOUPDATE_USE_PRECOMPILED=1");

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

        PublicDependencyModuleNames.AddRange(
            new[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "HTTP",
                "Json",
                "JsonUtilities",
                "SapientCore",
                "Projects",
                "FileUtilities"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
            }
        );
    }
}
