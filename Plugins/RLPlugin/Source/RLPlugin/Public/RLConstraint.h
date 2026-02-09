
#pragma once
#include "Engine/Engine.h"
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Curves/CurveFloat.h"
#include "RLConstraint.generated.h"


USTRUCT( BlueprintType )
struct FRLBoneStruct
{
    GENERATED_BODY()
    UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Struct" )
    FName BoneName;
    UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Struct" )
    FTransform Transform;
};

USTRUCT( BlueprintType )
struct FRLExpStruct
{
    GENERATED_BODY()
    UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Struct" )
    FName ExpName;
    UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Struct" )
    TArray< FRLBoneStruct > Bones;
}; 

USTRUCT( BlueprintType )
struct FRLConstraintStruct
{
    GENERATED_BODY()
    UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Struct" )
    FName Name;
    UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Struct" )
    TArray< FName > SourceChannels;
    UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Struct" )
    FName TargetChannel;
    UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Struct" )
    FString CurveMode;
    UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Struct" )
    FRuntimeFloatCurve Curve;
    UPROPERTY( EditAnywhere, BlueprintReadWrite, Category = "Struct" )
    FString Mode;
};