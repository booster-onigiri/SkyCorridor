#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "EWWaterCityAuditCommandlet.generated.h"

UCLASS()
class ENDLESSWORLD_API UEWWaterCityAuditCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UEWWaterCityAuditCommandlet();
    virtual int32 Main(const FString& Params) override;
};
