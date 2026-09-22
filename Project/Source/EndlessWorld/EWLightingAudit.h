#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWLightingAudit.generated.h"

UCLASS()
class ENDLESSWORLD_API AEWLightingAudit:public AActor
{
    GENERATED_BODY()
public:
    AEWLightingAudit();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    UPROPERTY() TObjectPtr<class ACameraActor> Camera;
    FString Report,Pending;
    int32 Index=0,Phase=0;
    double Started=0,Stage=0;
    bool Done=false,HDR=false,Resume=false;
    FTransform Garden;
    TSharedPtr<class FJsonObject> Shot;
    TArray<TSharedPtr<class FJsonValue>> Captures;
    FDelegateHandle SDRHandle,HDRHandle;
    void Finish(const FString& Error=FString());
    void WritePNG(int32 W,int32 H,const TArray<FColor>& Pixels);
    void CaptureSDR(int32 W,int32 H,const TArray<FColor>& Pixels);
    void CaptureHDR(int32 W,int32 H,const TArray<FLinearColor>& Pixels);
};
