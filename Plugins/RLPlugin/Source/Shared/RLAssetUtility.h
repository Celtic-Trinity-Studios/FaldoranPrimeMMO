#pragma once
#include "CoreMinimal.h"

#include <functional>

struct CSceneTempData
{
    FString strAssetName;
    FTransform kTransform;
    AActor* pParentActor;
    FString strFolderName;
    bool bPilotTarget;
};

struct FAssetRenameData;
class FRLAssetUtility
{
public:
    static bool CreateFolder( const FString& strPath );
    static bool MoveFolder( const TArray<FString>& kAssetFolder, const FString& strTargetFolder );
    static bool MoveAssets( TArray<FAssetData>& kAssets, const FString& strTargetPath );
    static bool MoveAssets( const TArray<FAssetRenameData>& kAssetMoveData );
    static bool RenameAsset( UObject* pAssetObject, const FString& strNewAssetName );
    static bool SaveAssets( const TArray<UObject*>& kAssets );
    static bool DeleteAssets( const TArray<FAssetData>& kObjectList, TArray< CSceneTempData >& kSceneTempData );
    static bool DeleteAssets( const TArray<UObject*>& kObjectList, TArray< CSceneTempData >& kSceneTempData );
    static bool DeleteActorInScene( const FString& strPath, const FString& strTargetName );
    static void GetAssets( const FARFilter& kFilter, TArray<FAssetData>& kObjectList, const FARFilter& kIgnoreObjectFilter = FARFilter() );
};