#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWUpperRailAudit.generated.h"
UCLASS()
class ENDLESSWORLD_API AEWUpperRailAudit:public AActor
{
    GENERATED_BODY()
public:
    AEWUpperRailAudit();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
private:
    struct Step {int Kind=0,A=0,B=0;FString Name,Id;TArray<FVector> Path;};
    TArray<Step> Steps;
    TArray<TSharedPtr<class FJsonValue>> Checks;
    double Started=0,Stage=0,Progress=0,Best=DBL_MAX,Walked=0;FVector Last;
    int Index=-2,Point=0,Sub=0;bool Done=false;
    FString Report;
    void BuildSteps();
    void Next();
    void Finish(const FString& Error=FString());
};
