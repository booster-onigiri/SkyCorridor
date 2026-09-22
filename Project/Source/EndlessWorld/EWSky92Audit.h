#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWWorld.h"
#include "EWSky92Audit.generated.h"
UCLASS()
class ENDLESSWORLD_API AEWSky92Audit : public AActor
{
    GENERATED_BODY()
public:
    AEWSky92Audit();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
private:
    struct Step {FVector Local;int32 Action=0;};
    TArray<Step> Route;TArray<TSharedPtr<class FJsonValue>> Checks;
    FString Report;int32 Phase=0,App=0,Site=0,Point=0;
    double Started=0,Stage=0,Progress=0,Best=DBL_MAX,Walked=0;
    FVector Last;EW::ChunkCoord LastCoord;bool Done=false;
    void Next(int32 Value);
    bool Check(bool Pass,const FString& Label);
    void Finish(const FString& Error=FString());
    void Prepare(class UEWGameInstance* G);
};
