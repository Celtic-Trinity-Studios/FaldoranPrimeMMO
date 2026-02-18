using UnrealBuildTool;
using System.IO;

public class SapientAutoDownloadFramework : ModuleRules
{
    public SapientAutoDownloadFramework(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        bUsePrecompiled = true;
        SapientPrecompiledBinaryUtils.Configure(this, Target, "SapientAutoDownloadFramework");

        PublicDefinitions.Add("SAPIENT_AUTODOWNLOAD_USE_PRECOMPILED=1");

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
    }
}
