#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWCity82Audit.generated.h"
UCLASS()
class ENDLESSWORLD_API AEWCity82Audit : public AActor
{
    GENERATED_BODY()
public:
    AEWCity82Audit();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
private:
    double Started=0,Stage=0;
    int32 Phase=0;
    bool Finished=false;
    FString Report;
    FVector ShipBefore=FVector::ZeroVector;
    TArray<TSharedPtr<class FJsonValue>> Checks;
    UPROPERTY() TObjectPtr<class ACameraActor> Camera;
    void Capture(const FString& Name);
    bool Check(bool OK,const FString& Name);
    void Finish(const FString& Error=FString());
};
