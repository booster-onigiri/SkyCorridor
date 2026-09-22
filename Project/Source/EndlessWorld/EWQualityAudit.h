#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWQualityAudit.generated.h"

UCLASS()
class ENDLESSWORLD_API AEWQualityAudit : public AActor
{
    GENERATED_BODY()
public:
    AEWQualityAudit();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
private:
    void Finish(const FString& Error=FString());
    double Started=0,Stage=0,LastResume=0;
    int32 Phase=0;
    bool Finished=false;
    FString Report,URL;
};
