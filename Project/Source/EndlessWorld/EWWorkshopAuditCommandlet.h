#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "EWWorkshopAuditCommandlet.generated.h"
UCLASS()
class ENDLESSWORLD_API UEWWorkshopAuditCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UEWWorkshopAuditCommandlet();
    virtual int32 Main(const FString& Params) override;
};
