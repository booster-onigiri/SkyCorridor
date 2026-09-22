#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWRRHDRRuntimeAudit.generated.h"

UCLASS()
class ENDLESSWORLD_API AEWRRHDRRuntimeAudit : public AActor
{
    GENERATED_BODY()
public:
    AEWRRHDRRuntimeAudit();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
private:
    struct FMode { int32 SR, FG; bool RR, HDR; int32 Peak = 1000, Brightness = 100; };
    TArray<FMode> Modes;
    FString ReportPath, StreamPath;
    TArray<TSharedPtr<class FJsonValue>> Samples, Checks;
    TMap<FString, int32> OriginalDenoisers;
    double Started = 0, NextAction = 0, StageStarted = 0;
    int32 Phase = 0, Index = 0, Collected = 0;
    bool bFinished = false, bResumeOnly = false, bHDRSupported = false, bRRSupported = false;
    double BaselineEvaluations = 0, BaselinePresents = 0;
    bool Check(bool Passed, const FString& Name);
    void Finish(const FString& Failure = FString());
};
