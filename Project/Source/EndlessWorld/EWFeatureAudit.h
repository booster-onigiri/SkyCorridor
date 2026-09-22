#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWFeatureAudit.generated.h"
UCLASS()
class ENDLESSWORLD_API AEWFeatureAudit:public AActor
{
    GENERATED_BODY()
public:
    AEWFeatureAudit();
    virtual void BeginPlay()override;
    virtual void Tick(float Delta)override;
private:
    int32 Phase=0,Step=0,Button=-1,NavCount=0,JumpBefore=0,RecordsBefore=0;
    double Started=0,PhaseStart=0,NextAction=0,MaxJumpZ=0;
    bool bPad=false,bFinished=false,bPressed=false;
    FVector Before=FVector::ZeroVector;FRotator LookBefore=FRotator::ZeroRotator;
    FString ReportPath,FocusBefore;
    struct Mode{int32 SR=0,FG=0;bool RR=false;};
    TArray<Mode> Modes;
    TArray<TSharedPtr<class FJsonValue>> Checks,Samples;
    void Finish(const FString& Failure=FString());
    void Check(bool Pass,const FString& Name);
};
