// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class RLPlugin : ModuleRules
{
    public RLPlugin(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
        // for CC_ControlRig
        OptimizeCode = ModuleRules.CodeOptimization.Never;

        bUsePrecompiled = true;
        PrivateIncludePaths.AddRange(
            new string[] {
                "RLLiveLinkEditor/Private",
                "RLLiveLink/Public",
                "CC_ControlRig/Public",
                "RLPlugin/Public",
                "RLPlugin/Private",
                "Shared",
                "RLGroom/Public",
            }
            );
        PublicIncludePaths.AddRange(
            new string[] {
                // ... add public include paths required here ...
            }
            );


        PrivateIncludePaths.AddRange(
            new string[] {
                // ... add other private include paths required here ...
            }
            );


        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                // for RLLiveLink
                "Networking",
                "Sockets",
                "Json",
                "JsonUtilities",
                "LiveLinkInterface",
                "Engine",
                // for RLLiveLinkEditor
                "CoreUObject",
                "UnrealEd",
                "Projects",
                "DetailCustomizations",
                "BlueprintGraph"
            }
            );


        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "Projects",
                "InputCore",
                "UnrealEd",
                "LevelEditor",
                "CoreUObject",
                "RenderCore",
                "Engine",
                "Slate",
                "SlateCore",
                "ContentBrowser",
                "DesktopPlatform",
                "MaterialEditor",
                "ImageWrapper",
                "EditorStyle",
                "RawMesh",
                "ClothingSystemEditorInterface",
                "SkeletalMeshEditor",
                "Json",
                "JsonUtilities",
                "SkeletalMeshUtilitiesCommon",
                "ClothingSystemRuntimeCommon",
                "PropertyEditor",
                // for RLLiveLink
                "Core",
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "Projects",
                "InputCore",
                "UnrealEd",
                "LevelEditor",
                "ContentBrowser",
                "DesktopPlatform",
                "ImageWrapper",
                "EditorStyle",
                "RawMesh",
                "BlueprintGraph",
                "ApplicationCore",
                "CinematicCamera",
                "FBX",
                "LevelSequence",
                "MovieScene",
                "LevelSequenceEditor",
                "Sequencer",
                "MovieSceneTracks",
                "MovieSceneTools",
                "UMG",
                "CinematicCamera",
                "MovieSceneTracks",
                // for RLLiveLinkEditor
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "WorkspaceMenuStructure",
                "EditorStyle",
                "SlateCore",
                "Slate",
                "InputCore",
                // for CC_ControlRig
                "Projects",
                "InputCore",
                "EditorFramework",
                "UnrealEd",
                "ToolMenus",
                "CoreUObject",
                "Engine",
                "Slate",
                "EditorScriptingUtilities",
                "SlateCore",
                "ControlRigDeveloper",
                "ControlRig",
            }
            );
        if( Target.Version.MajorVersion == 4 && Target.Version.MinorVersion <= 23 )
        {
            PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "ClothingSystemRuntime"
            } );
        }
        if( Target.Version.MajorVersion >= 5 || ( Target.Version.MajorVersion == 4 && Target.Version.MinorVersion >= 24 ) )
        {
            PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "ClothingSystemEditor",
                "ClothingSystemRuntimeCommon",
                "SkeletalMeshUtilitiesCommon",
                "ClothingSystemRuntimeInterface",
                "ClothingSystemRuntimeNv",
            } );
        }
        if ( Target.Version.MajorVersion >= 5 )
        {
            PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "PhysicsCore",
                "PhysicsUtilities",
                "ChaosCloth",
                "IKRigEditor",
                "IKRig",
                "HairStrandsCore",
                "SkeletalMeshDescription",
                "StaticMeshDescription",
                "MeshDescription",
                "AssetTools"
            } );
        }
        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
                // ... add any modules that your module loads dynamically here ...
            }
            );
        PublicDefinitions.AddRange(
            new string[]
            {
                // for RLLiveLink
                "_WIN32_WINNT_WIN10_TH2",
                "_WIN32_WINNT_WIN10_RS1",
                "_WIN32_WINNT_WIN10_RS2",
                "_WIN32_WINNT_WIN10_RS3",
                "_WIN32_WINNT_WIN10_RS4",
                "_WIN32_WINNT_WIN10_RS5",
            }
            );
    }
}

