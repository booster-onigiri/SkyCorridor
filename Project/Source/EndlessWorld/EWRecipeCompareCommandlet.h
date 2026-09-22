#pragma once
#include "Commandlets/Commandlet.h"
#include "EWRecipeCompareCommandlet.generated.h"
UCLASS()
class UEWRecipeCompareCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UEWRecipeCompareCommandlet();
    virtual int32 Main(const FString& Params) override;
};
