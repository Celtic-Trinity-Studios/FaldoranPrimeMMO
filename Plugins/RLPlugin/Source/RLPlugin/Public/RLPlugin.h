// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

#if ( ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 1 )
#include "AssetRegistry/AssetRegistryModule.h"
#else
#include "AssetRegistryModule.h"
#endif

#include "ObjectTools.h"
#include "PackageTools.h"
#include "CCImportUI.h"
#include "Editor/UnrealEd/Classes/Factories/FbxFactory.h"
#include "Editor/UnrealEd/Public/Editor.h"
#include "Runtime/Engine/Classes/Engine/Selection.h"
#if ( ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 5 )
#include "Materials/MaterialInstanceConstant.h"
#else
#include "Runtime/Engine/Classes/Materials/MaterialInstanceConstant.h"
#endif
#include "Runtime/Engine/Classes/Engine/SkeletalMesh.h"
#include "Runtime/JSon/Public/Dom/JsonObject.h"
#include "RLTextureData.h"
#include "RLMaterialData.h"
#include "RLPhysicCollisionShapeData.h"
#if ( ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION < 4 )
#include "Runtime/Engine/Classes/Animation/Rig.h"
#endif
#include "Runtime/Launch/Resources/Version.h"
#include "RLConstraint.h"
#include "RLJsonParser.h"
#include <functional>

DECLARE_LOG_CATEGORY_EXTERN( LogAutoSetup, Log, All );

class FToolBarBuilder;
class FMenuBuilder;
class UClothingAssetCommon;
class UClothLODDataBase;
class UIKRigDefinition;
class UIKRigController;
class UIKRetargeterController;
class FRLLiveLinkModule;
class FRLRigSetupModule;
#if ( ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 6 )
enum class EFKChainRotationMode : uint8;
#else
enum class ERetargetRotationMode : uint8;
#endif
class CMaterialType
{
public:
    CMaterialType( FString strMaterialName, FString strBoneType = "" );
    FString GetType() { return m_strMaterialType; }
    float GetTilingValue() { return m_fTilingValue; }

private:
    FString m_strMaterialType;
    float m_fTilingValue = 0.0f;
};

enum class EShaderType
{
    Teeth,
    Scalp,
    Eyelash,
    Eye,
    Tearline,
    Occlusion,
    Head,
    Skin,
    Hair,
    GeneralSSS,
    PBR,
    TRA,
    TearlinePlus,
    OcclusionPlus,
};

enum class EWrinkleType 
{
    BaseColor,
    Normal,
    Roughness,
    Flow
};

struct SJsonInfo
{
    FString strGeneration = "";
    FString strBoneType = "NULL";
    bool bSupportShaderSelect = true;
    TMap< FString, RLMaterialData > kMaterialMap;
    TMap< FString, TArray<RLPhysicsCollisionShapeData> > kCollisionShapeMap;
    TMap< FString, TArray< FRLConstraintStruct > > kConstraintMap;
    TMap< FString, TArray< FRLExpStruct > > kExpressionMap;
    FString strExpressionSet = "";
    bool bIsAvatar = false;
};

enum class EImportShaderType
{
    Standard,
    HQ,
    LW,
    CCShader
};

struct AutoSetupSetting
{
    FString strShaderType;
    TArray<FString> kLODPathList;
    bool bIsDragFbx;
};

struct ImportDataInfo
{
    bool bIsAvatar;
    bool bToScene = true;
    bool bMetaHuman = false;
};

class FRLPluginModule : public IModuleInterface
{
public:

    /** IModuleInterface implementation */
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    /** This function will be bound to Command. */
    void SetupAll( const TArray<FAssetData>& kAssetDatas,
                   AutoSetupSetting& kAutoSetupSetting,
                   const bool bCreateLiveLink,
                   const bool bCreateControlRig );
    void PluginButtonClicked();
    void AutoSetup( const TArray<FAssetData>& kAssetDatas,
                    FString& strShaderType,
                    const TArray<FString>& kLODPathList,
                    const bool bIsDragFbx,
                    ImportDataInfo& kImportDataInfo );
    FString GetSavePath();
    void WriteCSV( const FString& strSavePath, const TArray<TArray<FString>>& kData );
    void ExportFacialExpressionToCSV( const FString& strSaveFolder, const TArray<FAssetData>& kSelectedAssets );
    bool CheckAutoSetupVersionPass( FString strJsonFilePath );
    FString GetBoneType( const FAssetData& kAssetData );
    bool SetupControlRig( const TArray<USkeletalMesh*>& kSkeletalMeshList );

private:
    
    enum ELODLevel
    {
        LOD0,
        LOD1,
        LOD2,
    };
    // IK Rig
    enum EOperation
    {
        Add,
        Remove,
        Modify
    };
    struct BoneChain
    {
        FName strChainName;
        FName strStartBoneName = NAME_None;
        FName strEndBoneName = NAME_None;
        FName strGoalName = NAME_None;
    };
    struct ModifyBoneChain
    {
        EOperation eModifyOperation;
        BoneChain kBoneChain;
    };
    struct GoalBone
    {
        FName strGoalName;
        FName strParentBoneName;
    };
    struct FKChainSetting
    {
        FName strTargetChainName;
#if ( ENGINE_MAJOR_VERSION >= 5 && ENGINE_MINOR_VERSION >= 6 )
        EFKChainRotationMode eRotationMode;
#else
        ERetargetRotationMode eRotationMode;
#endif
    };

    struct PinBoneSetting
    {
        FName strBoneToCopyFrom;
        FName strBoneToCopyTo;
    };

    struct IKRigSetting
    {
        TArray<ModifyBoneChain> kBoneChainList;
        TArray<GoalBone> kReparentGoalInfoList;
        TArray<FKChainSetting> kFKChainSettingList;
        TArray<PinBoneSetting> kPinBoneSettingList;
        TArray<FName> kEnableAlignPoleVectorChainNameList;
    };

    struct PhysicConstraintSetting
    {
        FTwistConstraint kTwistConstraint;
        FConeConstraint kConeConstraint;
        struct TargetVelocity
        {
            struct AngularDrive
            {
                bool bEnableSlerp;
                bool bEnableTwist;
                bool bEnableSwing;
            };
            AngularDrive kAngularDrive;
            float fDumping;
        };

        TargetVelocity kTargetVelocity;
    };

    using PhysicSettingMap = TMap<FName, PhysicConstraintSetting>;

    void AddToolbarExtension( FToolBarBuilder& kBuilder );
    void AddMenuExtension( FMenuBuilder& kBuilder );
    void AddMenuEntryInRightClick();
    TSharedRef<FExtender> ExtendAssetContextMenu( const TArray<FAssetData>& SelectedAssets );
    TSharedRef<SWidget> AddToolbarSubAction( TSharedPtr<class FUICommandList> spCommands );
    void DisableFBXImportInterchange();
    void ImportFbx();
    UAutomatedAssetImportData* GetImportDataByJson( const FString& strImportSettingFile );
    void LaunchURL( FString strWebID );

    // Apply CC Textures for Meta Human
    TSharedRef< FExtender > CreateContentBrowserAssetSelectionMenu( const TArray< FAssetData >& kSelectedAssets );
    void ApplyCCTexturesToMH( FString strAssetPath );
    bool IsMetaHumanBP( UBlueprint* pBlueprint );
    USkeletalMeshComponent* GetMetaHumanSkeletalMeshByPart( UBlueprint* pBlueprint, const FName& strPart );
    UTexture* LoadTexture( const FString& strTexturePath, bool bsRGB = false );
    void CheckWorldGridMaterial(FString &strFbxName, FString &rootGamePath, FString ccMaterialFolderGamePath, UStaticMesh *pMesh, bool isMaterialInstance);
    bool CheckShaderTypeChange(FString &strfbxName, FString shaderType, USkeletalMesh *mesh, UMaterial* material, UMaterialInterface* MaterialInterface, RLMaterialData* pMaterialData, int slotID, FString &rootGamePath, bool isMaterialInstance, FString strBoneType );
    void DeleteTextureFile( const FString& strDeleteFloderPath, const FString& strCheckFolderPath );
    void ShowInfo( const FString& strMessage, const float& fExpireDuration );
    USkeleton* GetAssetSkeleton( const FAssetData& kAssetData );
    void CreateTexturesPathList( const FString &strRootGamePath, TArray <FString> &kTexturesPathList );
    void RemoveInvalidTexture( TMap< FString, RLMaterialData >& kMaterialMap, const FString& strTexturePath, const FString& strFbmTexturePath, TArray< FString >& kTexturesPathList );
    bool CheckTextureShouldImport( const FString& strFilePath, bool bPbr );
    bool VerifyTextureFilePath( const FString& strRootGamePath,
                                const TArray< FString >& kTexturesPathList,
                                const FString& strTextureGamePath,
                                const FString& strFbmTextureGamePath );
#if ENGINE_MAJOR_VERSION <= 4 && ENGINE_MINOR_VERSION == 24
    bool RLPluginImportToLodInternal( USkeletalMesh* SourceMesh, int32 SourceLodIndex, int32 SourceSectionIndex, UClothingAssetCommon* DestAsset, UClothLODDataBase* DestLod, UClothLODDataBase* InParameterRemapSource );
#endif
    bool SetupBoneRetargetingMode( USkeleton* pAvatarSkeleton );
    ELODLevel GetLODLevel( USkeleton* pAvatarSkeleton );
    FString GetMaterialPackagePath( const FString& strRootGamePath, const FString& strMaterialName, const FString& strFbxName, bool bIsMaterialInstance );
    void ReplaceMaterial( UMaterialInterface* pMaterialInterface, const FString &strPackagePath, std::function< void( UMaterialInterface* ) > fnReplaceMaterial );
    void DeleteUnrealMaterial( const FString &strSourceFolderPath, const FString &strTargetFolderPath, const FString &strFbxName, const FString &strRootGamePath, bool bIsMaterialInstance, std::function< void() > fnReplaceMeshMaterial );
    void DeleteDefaultSkeletalMaterial( const FString &strSourceFolderPath, const FString &strTargetFolderPath, const FString &strFbxName, const FString &strRootGamePath, USkeletalMesh *pMesh, bool bIsMaterialInstance );
    void DeleteDefaultStaticMaterial( const FString &strSourceFolderPath, const FString &strTargetFolderPath, const FString &strFbxName, const FString &strRootGamePath, UStaticMesh *pMesh, bool bIsMaterialInstance );

    void MoveTextureFile( FString& strTargetFolderPath );
    void findLODGroupMaterialJson(TMap< FString, RLMaterialData > &kMaterialMap, FString &strFbxName, FString &strRootGamePath, USkeletalMesh *kMesh, bool bIsMaterialInstance);
    void findLODGroupMaterial(FString &rootGamePath, USkeletalMesh *mesh );
    void SetOrmTextureSetting( UMaterialInstanceConstant* pMaterialInstance, const FString& strTexturePath, const bool bSkin = false );
    void ProcessMaterialInstanceConstant( UMaterialInstanceConstant*& pMaterialInstance, UMaterial* pMaterial, UMaterialInterface* pMeshMaterialInterface, UMaterialInterface* pParentInterface, const FString& strCCMaterialFolderPath, std::function< void( UMaterialInstanceConstant* ) > fnMaterialSetting );
    void AssignMaterialInstanceJson( UMaterialInstanceConstant*& pInstUMaterialInterface, 
                                     const FString& strMaterialName,
                                     RLMaterialData* pMaterialData,
                                     const TArray <FString>& kTexturesPathList,
                                     UMaterial* pMaterial,
                                     UMaterialInterface* pMaterialInterface,
                                     const FString& strTexturesFilesGamePath,
                                     const FString& strTexturesFilesGamePathFbm,
                                     const FString& strCCMaterialFolderGamePath,
                                     const FString& strSubsurfaceProfilePath,
                                     const FString& strBoneType,
                                     const FString& strShaderType,
                                     USkeleton* pSkeleton );

    void AssignGeneralSss( RLMaterialData* pMaterialData, 
                           const TArray< FString >& kTexturesPathList, 
                           const FString& strTexturesFilesGamePathFbm,
                           const FString& strMaterialName, 
                           const FString& strTexturesFilesGamePath,
                           const bool bIsHQSkin,
                           const FString& strSubsurfaceProfilePath,
                           UMaterialInstanceConstant*& pInstUMaterialInterface, 
                           UMaterial* pMaterial, 
                           UMaterialInterface* pMaterialInterface, 
                           const FString& strCCMaterialFolderGamePath );

    void PhysicIniPaser( FString iniPath );
    FString GetTexturePath( RLMaterialData *pMaterialData, const FString& strKey, const FString& strTexturesFolderPath, const FString& strMaterialName );
    void SetTextureParameter( RLMaterialData *pMaterialData, const FString& strKey, UMaterialInstanceConstant* pMaterialInstance );

    void UpdateStaticParameter( UMaterialInstanceConstant* pMaterialInstance, const FString& strParameter, bool bEnable, bool bMarkChanged );
    void SetMultiUvIndex( RLMaterialData* pMaterialData, UMaterialInstanceConstant* pMaterialInstance );
    void SetBaseColor( RLMaterialData* pMaterialData, UMaterialInstanceConstant* pMaterialInstance, const TArray< FString >& kTexturesPathList, const TArray< FString >& kTexturesFilesGamePaths, const FString& strMaterialName, const bool bSetWrinkle );
    void SetNormal( RLMaterialData* pMaterialData, UMaterialInstanceConstant* pMaterialInstance, const TArray< FString >& kTexturesPathList, const TArray< FString >& kTexturesFilesGamePaths, const FString& strMaterialName, const bool bSetWrinkle );
    void SetSpecular( RLMaterialData* pMaterialData, UMaterialInstanceConstant* pMaterialInstance, const TArray< FString >& kTexturesPathList, const TArray< FString >& kTexturesFilesGamePaths, const FString& strMaterialName, const bool bIsPBR );
    void SetOpacity( RLMaterialData* pMaterialData, UMaterialInstanceConstant* pMaterialInstance, const TArray< FString >& kTexturesPathList, const TArray< FString >& kTexturesFilesGamePaths, const FString& strMaterialName );
    void SetOpacityAdv( RLMaterialData* pMaterialData, UMaterialInstanceConstant* pMaterialInstance );
    void SetGlow( RLMaterialData* pMaterialData, UMaterialInstanceConstant* pMaterialInstance, const TArray< FString >& kTexturesPathList, const TArray< FString >& kTexturesFilesGamePaths, const FString& strMaterialName );
    void SetBlend( RLMaterialData* pMaterialData, UMaterialInstanceConstant* pMaterialInstance, const TArray< FString >& kTexturesPathList, const TArray< FString >& kTexturesFilesGamePaths, const FString& strMaterialName );
    void SetBlendToHairDepthMap( RLMaterialData *pMaterialData, UMaterialInstanceConstant* pMaterialInstance, const TArray< FString >& kTexturesPathList, const TArray< FString >& kTexturesFilesGamePaths, const FString& strMaterialName );
    void SetDisplacement( RLMaterialData* pMaterialData, UMaterialInstanceConstant* pMaterialInstance, const TArray< FString >& kTexturesPathList, const TArray< FString >& kTexturesFilesGamePaths, const FString& strMaterialName );
    void SetAO( RLMaterialData* pMaterialData, UMaterialInstanceConstant* pMaterialInstance, const TArray< FString >& kTexturesPathList, const TArray< FString >& kTexturesFilesGamePaths, const FString& strMaterialName );
    void SetRoughness( RLMaterialData* pMaterialData, UMaterialInstanceConstant* pMaterialInstance, const TArray< FString >& kTexturesPathList, const TArray< FString >& kTexturesFilesGamePaths, const FString& strMaterialName, const bool bIsPBR, const bool bSetWrinkle );
    void SetMetallic( RLMaterialData* pMaterialData, UMaterialInstanceConstant* pMaterialInstance, const TArray< FString >& kTexturesPathList, const TArray< FString >& kTexturesFilesGamePaths, const FString& strMaterialName, const bool bIsPBR );
    void CreateCollisionShape(FName strBoneName, FVector kBoundMin, FVector kBoundMax, FVector kScale, FVector kOffset, UBodySetup* pBodySetup, int nShapeType, int nBoundAxis);
    void CreateCollisionShapeFromData( RLPhysicsCollisionShapeData* pCollisionShapeData, UBodySetup* pBodySetup, const FTransform* kBoneParentWorldTransform );
    void CreateConstraint(FName strBoneName, 
                           int nBoneID, 
                           USkeletalMesh* pMesh, 
                           UPhysicsAsset* pPhysicsAsset,
                           const bool bIsStandardAvatar = false,
                           const PhysicSettingMap& kPhysicSettingMap = {} );
    static bool SetShaderTextureSrgbCompression( UTexture* pTexture, FString strName );
    void SetShaderData( RLShaderData* pShaderData, UMaterialInstanceConstant* pMaterialInstance, const FString& strFolder );
    void SetScatter( const RLScatter* pScatter, UMaterialInstanceConstant* pMaterialInstance, const FString& strMaterialName, FString strSubsurfaceProfilePath, EShaderType eShaderType );

    void GetShaderRenameMap( FString strMaterialName, EShaderType eType, RLShaderData* pShaderData, ENodeType eNodeType, TMap< FString, FString >& kRenameMap );
    void ParseJson( const FString& strJsonFilePath,  SJsonInfo& kJInfo );
    void ParseJsonShaderData( TSharedPtr< RLMaterialData > spMaterialData, TSharedPtr< FJsonObject > spJsonObject, FString strMaterialName, FString strBoneType );
    void ParseJsonScatterData( TSharedPtr< RLMaterialData > spMaterialData, TSharedPtr< FJsonObject > spJsonObject );
    void ParseJsonPhysicsData( TSharedPtr<FJsonObject> spPhysicsRoot, TMap< FString, RLMaterialData >& kMaterialMap, TMap< FString, TArray<RLPhysicsCollisionShapeData> >& kCollisionShapeMap, bool bIsCCStdBoneAxis );
    void ParseJsonForPhysicConstraint( const FString& strJsonFilePath, PhysicSettingMap& kPhysicConstraint );
    void ParseJsonForIKRig( const FString& strJsonFilePath, 
                            IKRigSetting& kIKRigSetting );
    void ConvertEyeMaterialName( FString& strMaterialName );


    UObject* CreatePhysicsAsset( USkeletalMesh* pMesh );

    FString GetMaterialTypeByName( FString strName );
    EShaderType GetShaderType( RLMaterialData* pMaterialData, FString strMaterialName, FString strBoneType );
    EShaderType GetPbrShaderType( const RLShaderData* pShaderData, FString strMaterialName, FString strBoneType );
    void UpdateWorld( int32 BoneIndex, FTransform ParentWorld, USkeleton* pSkeleton, const TArray<FTransform>& kBoneTransform, TArray<FTransform>& kBoneWorld );
    void CreatePhysicCollisionShape( UPhysicsAsset* pPhysicsAsset,
                                     USkeletalMesh* pMesh,
                                     USkeleton* pSkeleton,
                                     const TMap< FString, TArray<RLPhysicsCollisionShapeData> >& kCollisionShapeMap,
                                     const bool bIsStandardAvatar = false );
    void CreatePhysicSoftCloth( UPhysicsAsset* pPhysicsAsset, 
                                USkeletalMesh* pMesh, 
                                const FString& strBoneType,
                                TMap< FString, RLMaterialData >& kMaterialMap );

    void ParseJsonWrinkleData( TSharedPtr< RLMaterialData > spMaterialData, TSharedPtr<FJsonObject> spJsonObject );
    void SetWrinkleTexture( RLMaterialData* pMaterialData, UMaterialInstanceConstant* pMaterialInstance, const EWrinkleType& eType,
                            const TArray< FString >& kTexturesPathList, const TArray< FString >& kTexturesFilesGamePaths, 
                            const FString& strMaterialName, const FString& strDefaultTexturePath );
    bool SetWrinkleWeights( RLMaterialData* pMaterialData, UMaterialInstanceConstant* pMaterialInstance );
    void SetWrinkleAnimCurves( RLMaterialData* pMaterialData, USkeleton* pSkeleton );
    bool BuildWrinkleBlueprint( const FString& strRootGamePath, USkeleton* pSkeleton, TMap< FString, TArray< FRLConstraintStruct > >& kConstraintMap,
        TMap< FString, TArray< FRLExpStruct > >& kBoneMap, bool bIsMHAProfile );
    bool ReAssignSequenceSkeleton( const TArray<FAssetData>& kAssetsToRetarget, USkeleton* pSkeleton );
    bool ReAssignAnimationBlueprintSkeleton( UAnimBlueprint* pAnimBlueprint, USkeleton* pSkeleton );
    void SaveModifiedAssets( const TArray<FAssetData>& kAssetDatas );
    void CompileAndSaveBlueprint( UBlueprint* pBlueprint );
    UIKRigDefinition* CreateIKRigFromSkeletalMesh( USkeletalMesh* pSkeleton );
    bool SetupIKRigController( UIKRigDefinition* pIKRig,
                               const TArray<ModifyBoneChain>& kBoneChainList,
                               const TArray<GoalBone>& kReparentGoalInfoList );
    bool ModifyIKChainBone( UIKRigController* pIkRigController, const ModifyBoneChain& kModifyBoneChainList );
    UIKRetargeterController* CreateNewIKRetargeterFromIKRig( UIKRigDefinition* pIKRig );
    bool SetupIKRetargetFKChainsOp( UIKRetargeterController* pRetargetController,
                                    const TArray<FKChainSetting>& kFKChainSettingList );
    bool SetupIKRetargetPinBoneOp( UIKRetargeterController* pRetargetController,
                                   const TArray<PinBoneSetting>& kPinBoneSettingList );
    bool SetupIKRetargetAlignPoleVectorOp( UIKRetargeterController* pRetargetController, const TArray<FName>& kEnableChainList );
    bool AddRetargetOp( UIKRetargeterController* pRetargetController );
    bool AddExpAnimCurves( USkeleton* pSkeleton, const TArray< FString >& kAddHDExpCurvesList );
    void MoveMotionAsset( const FString& strRootPath );
    
private:
    TSharedPtr< class FUICommandList > m_kPluginCommands;
    static URig* m_pEngineHumanoidRig;
    bool m_bIsMaterialInstance = true;
    FString m_strContentBrowserSelectAsset = "";
    TSharedPtr< FRLLiveLinkModule > m_spRLLiveLinkModule;
    TSharedPtr< FRLRigSetupModule > m_spRLRigSetupModule;
};
