#pragma once
#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN( LogGroom, Log, All );
class UGroomAsset;
class UGroomBindingAsset;
class USkeletalMesh;

class FRLGroom
{
public:
    static bool CreateCCGroomBinding( const TArray<FAssetData>& kSelectedAssets );
    static bool UpdateGroomBindingTargetMesh( USkeletalMesh* pTargetMesh );
private:
    static bool RebindGroom( UGroomBindingAsset* pGroomBindingAsset, 
                             const FString& strTargetGamePath, 
                             USkeletalMesh* pBindingMeshCC, 
                             USkeletalMesh* pBindingMeshMHA );
    static UGroomAsset* CreateDeformedGroom( UGroomBindingAsset* pBindingAsset,
                                             const FString& strTargetPath );
    static bool GetGroomBindingAsset( const FString& strGroomFolderPath, TArray<UGroomBindingAsset*>& kGroomBindingList );
    
};