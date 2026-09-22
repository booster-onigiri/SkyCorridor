#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWAeroAudit.generated.h"
UCLASS()
class ENDLESSWORLD_API AEWAeroAudit:public AActor
{
    GENERATED_BODY()
public:
    AEWAeroAudit();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
private:
    bool Check(bool Pass,const FString& Name);
    void Finish(const FString& Error=FString());
    void Capture(const FString& Name);
    void Next(int32 Value);
    void Clock(double Value,bool Run=false);
    bool Walk(const FVector& Local,bool OnShip,double Timeout=30.);
    UPROPERTY() TObjectPtr<class ACameraActor> Camera;
    UPROPERTY() TObjectPtr<class AEWLift> PortLift;
    TArray<TSharedPtr<class FJsonValue>> Checks;
    FString Report;
    int32 Phase=0,Point=0,ShipIndex=0,Rebases=0,Records=0;
    double Started=0,Stage=0,Progress=0,Best=DBL_MAX,FlightMaxDrift=0,PathMetres=0;
    bool Done=false,Resume=false;
    FVector LastPoint,FlightAnchor;
};
