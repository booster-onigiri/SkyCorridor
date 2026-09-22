#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWHighlightAudit.generated.h"
// Opt-in, silent, isolated renderer comparison. Never runs in normal play.
UCLASS()
class ENDLESSWORLD_API AEWHighlightAudit:public AActor
{
    GENERATED_BODY()
public:
    AEWHighlightAudit();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    void Finish(const FString& Error=FString());
    void CaptureSDR(int32 Width,int32 Height,const TArray<FColor>& Pixels);
    void CaptureHDR(int32 Width,int32 Height,const TArray<FLinearColor>& Pixels);
    void WritePNG(int32 Width,int32 Height,const TArray<FColor>& Pixels);
    FString Report,Pending;
    UPROPERTY() TObjectPtr<class ACameraActor> Camera;
    FDelegateHandle SDRHandle,HDRHandle;
    TSharedPtr<class FJsonObject> Shot;
    TArray<TSharedPtr<class FJsonValue>> Captures;
    int32 Index=0,Phase=0;
    double Started=0,Stage=0;
    bool Done=false,WantHDR=false;
};
