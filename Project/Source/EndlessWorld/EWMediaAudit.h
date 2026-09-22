#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWMediaAudit.generated.h"
class FEWMonitorMeter;
class AEWLift;
class UWidgetComponent;
class ACameraActor;
UCLASS()
class ENDLESSWORLD_API AEWMediaAudit:public AActor
{
    GENERATED_BODY()
public:
    AEWMediaAudit();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    int32 Phase=0,AudioCase=0;
    double Started=0,Stage=0,GroundZ=0,PeakZ=0,SinglePeak=0,SecondVelocity=0;
    bool Finished=false,Measuring=false;
    FString Report;
    TArray<TSharedPtr<class FJsonValue>> Checks,Samples,ClockSamples;
    TSharedPtr<FEWMonitorMeter,ESPMode::ThreadSafe> Meter;
    TSharedPtr<FEWMonitorMeter,ESPMode::ThreadSafe> MainMeter;
    TSharedPtr<class FJsonObject> BaselineOutput,UnprobedOutput;
    UPROPERTY() TWeakObjectPtr<AEWLift> GateLift;
    int32 GateStop=0;
    UPROPERTY() TWeakObjectPtr<UWidgetComponent> ClockCapture;
    UPROPERTY() TObjectPtr<ACameraActor> FloorCamera;
    bool Check(bool Value,const FString& Name);
    void Finish(const FString& Error=FString());
};
