#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWWorld.h"
#include "EWHotelAudit.generated.h"
UCLASS()
class ENDLESSWORLD_API AEWHotelAudit:public AActor
{
    GENERATED_BODY()
public:
    AEWHotelAudit();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
private:
    void Finish(const FString& Error=FString());
    bool Check(bool Pass,const FString& Name);
    FString Report;
    TArray<TSharedPtr<class FJsonValue>> Checks,Captures;
    TArray<EW::InteriorRoom> Rooms;
    TArray<FVector> Route;
    UPROPERTY() TObjectPtr<class ACameraActor> Camera;
    UPROPERTY() TObjectPtr<class AEWLift> Lift;
    int32 Phase=0,Index=0,Point=0,Photo=0,Ride=0,From=0,To=0,Falls=0;
    double Started=0,Stage=0,Progress=0,Best=DBL_MAX,Walked=0;
    FVector Last;
    bool Done=false,Resume=false;
};
