#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "EWResidenceAuditCommandlet.generated.h"

UCLASS()
class ENDLESSWORLD_API UEWResidenceAuditCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UEWResidenceAuditCommandlet();
    virtual int32 Main(const FString& Params) override;
};
