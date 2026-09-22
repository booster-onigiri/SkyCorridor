#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWTemporalAudit.generated.h"

UCLASS()
class ENDLESSWORLD_API AEWTemporalAudit : public AActor
{
    GENERATED_BODY()
public:
    AEWTemporalAudit();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
private:
    UPROPERTY() TObjectPtr<class ACameraActor> Camera;
    FString Output, Scene = TEXT("city"), OnlyMode;
    FVector Eye = FVector::ZeroVector, Aim = FVector::ZeroVector;
    FRotator View = FRotator::ZeroRotator;
    int32 Phase = 0, Mode = 0;
    double Started = 0, PhaseAt = 0, LastSample = 0;
    bool bFinished = false, bGeneratedOnly = false;
    float FastPanSeconds = 10.f;
    void Event(const FString& Name);
    void Finish(const FString& Failure = FString());
};
