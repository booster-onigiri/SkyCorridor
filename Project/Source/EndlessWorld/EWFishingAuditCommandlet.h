#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "EWFishingAuditCommandlet.generated.h"
UCLASS()
class ENDLESSWORLD_API UEWFishingAuditCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UEWFishingAuditCommandlet();
    virtual int32 Main(const FString& Params) override;
};
