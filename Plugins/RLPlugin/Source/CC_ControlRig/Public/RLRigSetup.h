// Copyright 2024 The Reallusion Authors. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "RLRigSetupDefine.h"

DECLARE_LOG_CATEGORY_EXTERN( LogCCControlRig, Log, All );

class FToolBarBuilder;
class FMenuBuilder;
class ASkeletalMeshActor;

class FRLRigSetupModule
{
public:
	enum ESkeletalMeshPart : uint8
	{
		Face = 0,
		Body
	};

	FRLRigSetupModule();
	~FRLRigSetupModule();
	
	bool PluginButtonClicked();
	bool SetupControlRig( const TArray<USkeletalMesh*>& kSkeletalMeshList );
	bool GetSkeletalMeshFromViewport( TArray<USkeletalMesh*>& kSkeletalMeshList );
	bool GetSkeletalMeshFromContentBrowser( TArray<USkeletalMesh*>& kSkeletalMeshList );
private:
	// Main Function
	UBlueprint* CreateRigBlueprint( USkeletalMesh* pSkeletalMesh,
							        USkeletalMesh* pFaceSkeletalMesh,
							        USkeletalMesh* pBodySkeletalMesh,
							        const FString& strFaceAnimBlueprintPackagePath,
							        const FString& strFaceAnimBlueprintRegularPath,
							        const FString& strBodyAnimBlueprintPackagePath,
							        const FString& strBodyAnimBlueprintRegularPath,
							        const FString& strRigBlueprintPackagePath,
							        const FString& strRigBlueprintRegularPath );
	UBlueprint* CreateRigBlueprintWithLiveLink( USkeletalMesh* pSkeletalMesh,
										        USkeletalMesh* pFaceSkeletalMesh,
										        USkeletalMesh* pBodySkeletalMesh,
										        const FString& strFaceAnimBlueprintPackagePath,
										        const FString& strFaceAnimBlueprintRegularPath,
										        const FString& strBodyAnimBlueprintPackagePath,
										        const FString& strBodyAnimBlueprintRegularPath,
										        const FString& strRigBlueprintPackagePath,
										        const FString& strRigBlueprintRegularPath );
	bool CreateBodyAndFaceAsset( ASkeletalMeshActor* pActor,
								 USkeletalMesh*& pFaceSkeletalMesh,
								 USkeletalMesh*& pBodySkeletalMesh );
	bool CreateBodyAndFaceAsset( USkeletalMesh* pSkeletalMesh,
								 const FString strMainName,
								 const FString& strCreateTargetPackagePath,
								 const FString& strCreateTargetRegularPath,
								 USkeletalMesh*& pFaceSkeletalMesh,
								 USkeletalMesh*& pBodySkeletalMesh,
								 const bool bDisableMeshSection,
								 TMap<ESkeletalMeshPart, TMap<int, TArray< int >>>& kSectionIndexToHideMap );
	bool SetupWrinkleBlueprint( const FString& strTargetGamePath,
								const FString& strMainName,
								FString& strWrinkleBPTargetName,
								USkeletalMesh* pSkeletalMesh );
	bool BuildWrinkleBlueprint( const FString& strTargetGamePath,
								const FString& strWrinkleBPName,
								FString& strWrinkleBPTargetName,
								USkeletalMesh* pSkeletalMesh );

	//Utility
	FString GetRigPluginPath();
	uint32 GetSelectedActors( UClass* pFilterClass, TArray<UObject*>& kOutSelectedObjects );
	FString GetActorPackagePath( ASkeletalMeshActor* pActor );
	FString GetActorName( ASkeletalMeshActor* pActor );
	FString GetActorRLContentRegularPath( ASkeletalMeshActor* pActor );
	FString GetSkeletalMeshFilePath( ASkeletalMeshActor* pActor );
	bool IsSectionUsedHeadBone( const FSkelMeshSection& kSection, 
								const TSet<int32>& kHeadBoneIndices,
								const TSet<int32>& kSpineBoneIndices,
								const FSkinWeightVertexBuffer& kSkinWeightBuffer );

	UAnimBlueprint* CreateAnimationBlueprintTemplate( const FString& strTargetPackagePath,
													  const FString& strTargetRegularPath,
													  const FString& strTemplateName );
	UBlueprint* CreateBlueprintFromTemplate( const FString& strTargetPackagePath,
									 		 const FString& strTargetRegularPath,
											 const FString& strSourcePath,
											 const FString& strTemplateName,
											 const FString& strTargetName );
	UObject* CreateAnimationRigTemplate( const FString& strTemplateName );
	USkeletalMeshComponent* GetSkeletalMeshByPart( UBlueprint* pBlueprint, const FString& strPart );
	USkeletalMesh* CopySkeletalMeshFile( ASkeletalMeshActor* pActor,
									     const FString& strTemplateName );
	USkeletalMesh* CopySkeletalMeshFile( USkeletalMesh* pSkeletalMesh,
										 const FString& strActorGamePath,
										 const FString& strTemplateName );
	void CopyRigPickerFolderToRigFolder();
	void RemoveSkeletalMeshPart( USkeletalMesh* pSkeletalMesh,
								 ESkeletalMeshPart ePart,
								 const bool bDisableMeshSection,
								 TMap<int, TArray< int >>& kSectionIndexToHideMap );
	void DeleteControlRigAssets( const FString& strAssetPackagePath, const FString& strAssetName );
	bool ReAssignAnimationBlueprintSkeleton( UAnimBlueprint* pAnimBlueprint, USkeleton* pSkeleton );
	void ShowDisableLiveLinkActiveMessage();
	void ShowSkeltonNotCompatiableMessage( FString strSkeletonName );
	bool CopyRigControlFolder();
	void SetMHALib();
	bool IsCC3PlusSkeleton( USkeleton* pSkeleton );
private:

	TArray< FName > m_kHeadBoneNames ={ CC_HEAD, CC_FACIAL, CC_JAW_ROOT, CC_UPPER_JAW, CC_TEETH_01, CC_TEETH_02,
										CC_TONGUE_01, CC_TONGUE_02, CC_TONGUE_03, CC_LEFT_EYE, CC_RIGHT_EYE, };
	TArray< FName > m_kUE5SpineBoneNames = { CC_SPINE_01, CC_SPINE_02 };
	TArray< FName > m_kUE4SpineBoneNames = { CC_SPINE_01 };
};
