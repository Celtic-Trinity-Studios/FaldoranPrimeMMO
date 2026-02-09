#pragma once
#include "RLConstraint.h"

class URLJsonParser : public UObject
{

public:
    static void ParseExpressionSet(
        TSharedPtr<FJsonObject> spMainRoot,
        FString& strExpressionSet );

    static void ParseExpressionData(
        TSharedPtr<FJsonObject> spMainRoot,
        TArray<FRLExpStruct>& kExpressionList );

    static void ParseConstraintData(
        TSharedPtr<FJsonObject> spMainRoot,
        TArray<FRLConstraintStruct>& kConstraintList );

private:
    static void ParseBones(
        TSharedPtr<FJsonObject> spBones,
        TArray<FRLBoneStruct>& kBoneList );

    static void ParseTransform(
        TSharedPtr<FJsonObject> kBoneObject,
        FTransform& kTransform );

    static void ParseCurve(
        TArray<TSharedPtr<FJsonValue>> kCurves,
        FRuntimeFloatCurve& Curve );
};