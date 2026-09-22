#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWPresentationAudit.generated.h"

UCLASS()
class ENDLESSWORLD_API AEWPresentationAudit : public AActor
{
    GENERATED_BODY()
public:
    AEWPresentationAudit();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
private:
    struct FCase { FString Name; int32 FG = 0, Cap = 120; bool VSync = false; float WorkMs = 0; };
    TArray<FCase> Cases;
    TArray<TSharedPtr<class FJsonValue>> Checks, Samples;
    FString ReportPath;
    int32 Phase = 0, Step = 0, MenuIndex = 0, RenderCount = 0, PresentedCount = 0, GeneratedRenderCount = 0;
    double Started = 0, NextAction = 0, LastFrame = 0, SampleSeconds = 0;
    bool bFinished = false;
    bool Check(bool Pass, const FString& Name);
    void Finish(const FString& Failure = FString());
};
