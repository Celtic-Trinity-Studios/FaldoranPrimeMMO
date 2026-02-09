#pragma once
#include "CoreMinimal.h"

class USkeleton;
class RLToolKit
{
public:
    enum UnrealGeneration
    {
        None,
        UE4,
        UE5,
    };
    static UnrealGeneration CheckBoneStructureGeneration( USkeleton* pAvatarSkeleton );

    // strFilePath is the path with Project Content Directory
    static uint32 GetFileSerialNumber( const FString& strFilePath );
    static void GetUniqueAssetName( const FString& strTargetGamePath, FString& strAssetName );
};