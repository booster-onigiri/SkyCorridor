#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWPoolAudit.generated.h"
UCLASS()
class ENDLESSWORLD_API AEWPoolAudit:public AActor
{
    GENERATED_BODY()
public:
    AEWPoolAudit();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
private:
    FString Report;TArray<TSharedPtr<class FJsonValue>> Checks;
    int32 Phase=0,Index=0,Leg=0;double Started=0,Stage=0,Walked=0;bool Done=false,Resume=false;
    FTransform Garden;FVector Last=FVector::ZeroVector;
    bool Check(bool Pass,const FString& Name);
    void Next(int32 Value);
    void Finish(const FString& Error=FString());
    void Capture(const FString& Name);
};
