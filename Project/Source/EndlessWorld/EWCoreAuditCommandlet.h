#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "EWCoreAuditCommandlet.generated.h"

UCLASS()
class ENDLESSWORLD_API UEWCoreAuditCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UEWCoreAuditCommandlet();
    virtual int32 Main(const FString& Params) override;
};
