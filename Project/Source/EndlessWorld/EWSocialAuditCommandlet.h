#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "EWSocialAuditCommandlet.generated.h"
UCLASS()
class ENDLESSWORLD_API UEWSocialAuditCommandlet:public UCommandlet
{
    GENERATED_BODY()
public:
    UEWSocialAuditCommandlet();
    virtual int32 Main(const FString& Params) override;
};
