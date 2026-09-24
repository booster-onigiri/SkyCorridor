#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWArtStudy.generated.h"

class ACameraActor;

// Optional authoring-only camera study of the actual rendered world.
// It is never spawned by an ordinary player launch or counted as movement QA.
UCLASS()
class ENDLESSWORLD_API AEWArtStudy : public AActor
{
    GENERATED_BODY()
public:
    AEWArtStudy();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
private:
    UPROPERTY() TObjectPtr<ACameraActor> Camera;
    FString OutputPrefix;
    FString CapturePath;
    double Started=0, ReadyAt=0, CapturedAt=0;
    int32 ViewIndex=0, SelectedViewIndex=-1;
    bool bDestinationSet=false, bCameraSet=false, bFinished=false;
    TArray<TSharedPtr<class FJsonValue>> Views;
    void Finish(const FString& Failure=FString());
};
