#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWSkyrailAudit.generated.h"
class AEWLift;
class ACameraActor;
UCLASS()
class ENDLESSWORLD_API AEWSkyrailAudit:public AActor
{
    GENERATED_BODY()
public:
    AEWSkyrailAudit();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
private:
    double Started=0,Stage=0;int32 Phase=0,TourPoint=0;bool Finished=false,Crossed=false,MiddleStop=false,Resume=false;
    FString Report;
    TArray<TSharedPtr<class FJsonValue>> Checks;
    UPROPERTY() TWeakObjectPtr<AEWLift> Lift;
    UPROPERTY() TObjectPtr<ACameraActor> Camera;
    void Finish(const FString& Error=FString());
    bool Check(bool OK,const FString& Name);
    void Capture(const FString& Name);
};
