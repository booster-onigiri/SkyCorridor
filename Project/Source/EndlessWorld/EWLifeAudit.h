#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWLifeAudit.generated.h"

// Explicitly opted-in packaged-game smoke test. All catches use the real wait/reel
// loop; the launcher must supply an isolated save directory and a fresh report.
UCLASS()
class ENDLESSWORLD_API AEWLifeAudit : public AActor
{
    GENERATED_BODY()
public:
    AEWLifeAudit();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
private:
    double Started=0,Next=0,ClockStarted=0,InitialHour=0;
    int32 Phase=0,Site=0,Before=0,ExpectedBefore=-1;
    bool Finished=false,Passed=true;
    FString Report,PreviousPhoto;
    TArray<TSharedPtr<class FJsonValue>> Checks;
    void Check(bool OK,const FString& Name);
    void Finish(const FString& Failure=FString());
};
