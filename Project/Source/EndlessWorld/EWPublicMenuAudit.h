#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWPublicMenuAudit.generated.h"

// Opt-in, isolated-save audit of the normal public UI. Never starts a service.
UCLASS()
class ENDLESSWORLD_API AEWPublicMenuAudit : public AActor
{
    GENERATED_BODY()
public:
    AEWPublicMenuAudit();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
private:
    int32 Phase = 0;
    double Started = 0, Stage = 0;
    bool Finished = false, Failed = false;
    FString Report;
    TArray<TSharedPtr<class FJsonValue>> Checks, Buttons;
    void Check(bool Pass, const FString& Name);
    void InspectMain(bool Paused);
    void InspectPhone();
    FString ImagePath(const TCHAR* Name) const;
    void Finish(const FString& Error = FString());
};
