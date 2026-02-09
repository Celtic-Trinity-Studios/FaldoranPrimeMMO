#pragma once
#include "Engine/Engine.h"
#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Curves/CurveFloat.h"
#include "RLStruct.generated.h"
/** Please add a struct description */
USTRUCT( BlueprintType )
struct FSectionIndexListStruct
{
	GENERATED_BODY()
public:
	/** Please add a variable description */
	UPROPERTY( BlueprintReadWrite, EditAnywhere, meta = ( DisplayName = "SectionIndexList" ), Category = "Struct" )
	TArray<int32> SectionIndexList;
};