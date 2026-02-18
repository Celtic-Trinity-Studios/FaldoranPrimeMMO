// Copyright 2024 - 2025 Sapient Technology, Inc.

using UnrealBuildTool;
using System;
using System.IO;

public class SapientCore : ModuleRules
{
    public SapientCore(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.CPlusPlus;
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // ========================================================================
        // Precompiled Binary Configuration
        // ========================================================================
        // TEMPORARILY DISABLED to rebuild the DLL
        // TODO: Set back to true after rebuild
        bUsePrecompiled = true;
        SapientPrecompiledBinaryUtils.Configure(this, Target, "SapientCore");

        System.Console.WriteLine("SapientCore: Compiling from source (rebuilding DLL)");

        // CRITICAL: Minimal dependencies - NO UnrealEd, NO BlueprintGraph, etc.
        // This allows the module to be shipped as a binary
        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",  // ONLY for basic types like FString, TArray, TMap
            "CoreUObject",  // For TWeakObjectPtr<UObject> in FToolResult
            "Json",
            "JsonUtilities",
        });

        // For HTTP communication, JSON, and UObject support
        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "HTTP",
            "Projects",  // For IPluginManager (dynamic plugin path resolution)
            "AssetRegistry"  // For SafeAssetLoader
        });

        // Aggressive optimization for binary distribution
        OptimizeCode = CodeOptimization.Always;

        // Unity build disabled due to QueryEngine.cpp namespace visibility issues
        // The file uses `using namespace QueryEngine;` before the namespace block opens,
        // which creates symbol emission issues in unity builds where Tools.cpp is
        // included before QueryEngine.cpp in the same translation unit
        bUseUnity = false;

        // Enable exceptions - required for CodeIntelligence error handling
        bEnableExceptions = true;
        bUseRTTI = false;

        // Export symbols for shared library
        PublicDefinitions.Add("SAPIENTCORE_EXPORTS=1");

        // Logging configuration (0 = full logging, 1 = minimal logging for shipping)
        PublicDefinitions.Add("SAPIENTCORE_UE_BUILD_SHIPPING=1");

        // ========================================================================
        // CodeIntelligence Integration
        // ========================================================================

        // C++20 required for UE 5.5+ compatibility and CodeIntelligence features
        CppStandard = CppStandardVersion.Cpp20;

        // Add CodeIntelligence public include paths
        PublicIncludePaths.AddRange(new string[]
        {
            Path.Combine(ModuleDirectory, "Public"),
            Path.Combine(ModuleDirectory, "Public/CodeIntelligence"),
            Path.Combine(ModuleDirectory, "Public/CodeIntel")
        });

        // Setup ThirdParty dependencies
        SetupThirdPartyDependencies(Target);

        // Setup runtime dependencies to ensure DLLs are copied
        SetupRuntimeDependencies(Target);
    }

    private string GetEngineVersionFromEditor(ReadOnlyTargetRules Target)
    {
        try
        {
            // Get engine version directly from the build system
            int Major = Target.Version.MajorVersion;
            int Minor = Target.Version.MinorVersion;
            return $"{Major}.{Minor}";
        }
        catch (Exception Ex)
        {
            System.Console.WriteLine($"Warning: Failed to read EngineVersion from editor: {Ex.Message}");
        }

        // Fallback to default version if reading fails
        return "5.6"; // Default fallback
    }

    private void SetupRuntimeDependencies(ReadOnlyTargetRules Target)
    {
        string PluginPath = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../"));
        string BinariesPath = Path.Combine(PluginPath, "Binaries", Target.Platform.ToString());

        // Ensure the SapientCore DLL is copied to the output directory
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            string DllName, PdbName;
            
            if (Target.Configuration == UnrealTargetConfiguration.DebugGame)
            {
                DllName = "UnrealEditor-SapientCore-Win64-DebugGame.dll";
                PdbName = "UnrealEditor-SapientCore-Win64-DebugGame.pdb";
            }
            else
            {
                DllName = "UnrealEditor-SapientCore.dll";
                PdbName = "UnrealEditor-SapientCore.pdb";
            }
            
            string DllPath = Path.Combine(BinariesPath, DllName);

            // Add runtime dependency so UBT knows to copy this DLL
            RuntimeDependencies.Add(DllPath, StagedFileType.NonUFS);

            // Also add PDB for debugging
            string PdbPath = Path.Combine(BinariesPath, PdbName);
            if (File.Exists(PdbPath))
            {
                RuntimeDependencies.Add(PdbPath, StagedFileType.DebugNonUFS);
            }
        }
        else if (Target.Platform == UnrealTargetPlatform.Mac)
        {
            string DylibName = "UnrealEditor-SapientCore.dylib";
            string DylibPath = Path.Combine(BinariesPath, DylibName);
            RuntimeDependencies.Add(DylibPath, StagedFileType.NonUFS);
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            string SoName = "UnrealEditor-SapientCore.so";
            string SoPath = Path.Combine(BinariesPath, SoName);
            RuntimeDependencies.Add(SoPath, StagedFileType.NonUFS);
        }
    }

    private void SetupThirdPartyDependencies(ReadOnlyTargetRules Target)
    {
        string ThirdPartyPath = Path.Combine(ModuleDirectory, "ThirdParty");

        // ========================================================================
        // Tree-sitter (C++ parser)
        // ========================================================================
        SetupTreeSitter(Target, ThirdPartyPath);

        // ========================================================================
        // RapidJSON (Header-only JSON library)
        // ========================================================================
        SetupRapidJSON(Target, ThirdPartyPath);

        // ========================================================================
        // Blake3 (Fast cryptographic hashing)
        // ========================================================================
        SetupBlake3(Target, ThirdPartyPath);
    }

    private void SetupTreeSitter(ReadOnlyTargetRules Target, string ThirdPartyPath)
    {
        string TreeSitterPath = Path.Combine(ThirdPartyPath, "TreeSitter");

        // Add include paths
        PublicIncludePaths.Add(Path.Combine(TreeSitterPath, "Include"));
        PublicIncludePaths.Add(Path.Combine(TreeSitterPath, "Grammars"));
        // Add C++ grammar directory so that "tree_sitter/parser.h" resolves correctly
        PublicIncludePaths.Add(Path.Combine(TreeSitterPath, "Grammars", "cpp"));

        // Add definitions
        PublicDefinitions.Add("TREE_SITTER_HIDE_SYMBOLS=1");

        // Link platform-specific pre-built libraries (runtime + grammars)
        // Grammar source files are NOT compiled by UBT - they're pre-built in CodeIntelligence repo
        // to avoid filename conflicts (parser.c, scanner.c in multiple grammars)
        string LibPath = Path.Combine(TreeSitterPath, "Lib", Target.Platform.ToString());

        string[] RequiredLibs;
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            RequiredLibs = new string[]
            {
                "tree-sitter.lib",
                "tree-sitter-cpp.lib",
                "tree-sitter-c-sharp.lib",
                "tree-sitter-json.lib"
            };
        }
        else // Mac, Linux
        {
            RequiredLibs = new string[]
            {
                "libtree-sitter.a",
                "libtree-sitter-cpp.a",
                "libtree-sitter-c-sharp.a",
                "libtree-sitter-json.a"
            };
        }

        foreach (string LibName in RequiredLibs)
        {
            string LibFile = Path.Combine(LibPath, LibName);
            if (File.Exists(LibFile))
            {
                PublicAdditionalLibraries.Add(LibFile);
            }
            else
            {
                System.Console.WriteLine("WARNING: " + LibName + " not found at: " + LibFile);
                System.Console.WriteLine("See ThirdParty/TreeSitter/README.md for installation instructions");
            }
        }
    }

    private void SetupRapidJSON(ReadOnlyTargetRules Target, string ThirdPartyPath)
    {
        // RapidJSON is header-only, just add include path
        string RapidJSONPath = Path.Combine(ThirdPartyPath, "RapidJSON");

        // Add include path (header-only library)
        PublicIncludePaths.Add(Path.Combine(RapidJSONPath, "Include"));

        // Add helpful definitions
        PublicDefinitions.Add("RAPIDJSON_HAS_STDSTRING=1");
        PublicDefinitions.Add("RAPIDJSON_HAS_CXX11_RVALUE_REFS=1");
        PublicDefinitions.Add("RAPIDJSON_HAS_CXX11_NOEXCEPT=1");

        // Disable RapidJSON assertions in shipping builds for performance
        if (Target.Configuration == UnrealTargetConfiguration.Shipping)
        {
            PublicDefinitions.Add("RAPIDJSON_ASSERT(x)=");
        }

        // Platform-specific warning suppression
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PublicDefinitions.Add("_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING");
        }
    }

    private void SetupBlake3(ReadOnlyTargetRules Target, string ThirdPartyPath)
    {
        // Blake3 is pre-built in CodeIntelligence repo and stored with TreeSitter libs
        // (shared lib directory for simplicity)
        string Blake3Path = Path.Combine(ThirdPartyPath, "Blake3");
        string LibPath = Path.Combine(ThirdPartyPath, "TreeSitter", "Lib", Target.Platform.ToString());

        // Add include path
        PublicIncludePaths.Add(Path.Combine(Blake3Path, "Include"));

        // Enable SIMD optimizations (platform-specific)
        if (Target.Platform == UnrealTargetPlatform.Win64 ||
            Target.Platform == UnrealTargetPlatform.Mac ||
            Target.Platform == UnrealTargetPlatform.Linux)
        {
            PublicDefinitions.Add("BLAKE3_NO_SSE2=0");
            PublicDefinitions.Add("BLAKE3_NO_SSE41=0");
            PublicDefinitions.Add("BLAKE3_NO_AVX2=0");
            PublicDefinitions.Add("BLAKE3_NO_AVX512=0");
        }

        // Link pre-built library (stored with TreeSitter libs)
        string LibName = Target.Platform == UnrealTargetPlatform.Win64 ? "blake3.lib" : "libblake3.a";
        string LibFile = Path.Combine(LibPath, LibName);

            if (File.Exists(LibFile))
            {
                PublicAdditionalLibraries.Add(LibFile);
            }
            else
            {
            System.Console.WriteLine("WARNING: " + LibName + " not found at: " + LibFile);
            System.Console.WriteLine("Build libraries in CodeIntelligence repo and copy to ThirdParty/TreeSitter/Lib/");
        }
    }
}
