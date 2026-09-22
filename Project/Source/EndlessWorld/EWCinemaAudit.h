#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWCinemaAudit.generated.h"
class FEWCinemaMeter;
UCLASS()
class ENDLESSWORLD_API AEWCinemaAudit:public AActor
{
    GENERATED_BODY()
public:
    AEWCinemaAudit();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    int32 Phase=0,Waypoint=1,SeatIndex=0,AudioCase=0,DayCase=0;
    double Started=0,Stage=0,SegmentStarted=0;
    bool Finished=false,Clicked=false,Measuring=false;
    FString Report;
    FTransform Auditorium;
    TArray<TSharedPtr<class FJsonValue>> Checks,AudioCases,DayCases;
    TSharedPtr<FEWCinemaMeter,ESPMode::ThreadSafe> Meter;
    bool Check(bool Value,const FString& Name);
    void Finish(const FString& Error=FString());
    void Capture(const FString& Name);
    void PlaceListener(FVector Local,float Yaw);
};
