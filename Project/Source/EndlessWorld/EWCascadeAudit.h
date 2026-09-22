#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWWorld.h"
#include "EWCascadeAudit.generated.h"

UCLASS()
class ENDLESSWORLD_API AEWCascadeAudit:public AActor
{
    GENERATED_BODY()
public:
    AEWCascadeAudit();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
private:
    void Finish(const FString& Error=FString());
    bool Check(bool Pass,const FString& Name);
    struct Step {EW::ChunkCoord Coord;FVector Position;int32 Kind=0;};
    TArray<Step> Route;
    TArray<TSharedPtr<class FJsonValue>> Checks,Captures;
    UPROPERTY() TObjectPtr<class ACameraActor> Camera;
    FString Report;
    int32 Phase=0,Point=0,Photo=0,Falls=0,InitialRecords=0;
    double Started=0,Stage=0,Progress=0,Best=DBL_MAX,Walked=0;
    FVector Last,Recovery;
    EW::ChunkCoord LastCoord;
    bool Done=false,Resume=false;
};
