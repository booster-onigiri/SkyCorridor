#pragma once
#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "GameFramework/Actor.h"
#include "EWWorld.h"
#include "EWInteriorAudit.generated.h"
class ACameraActor;

UCLASS()
class ENDLESSWORLD_API UEWInteriorAuditCommandlet:public UCommandlet
{
    GENERATED_BODY()
public:
    UEWInteriorAuditCommandlet();
    virtual int32 Main(const FString& Params) override;
};

UCLASS()
class ENDLESSWORLD_API AEWInteriorAudit:public AActor
{
    GENERATED_BODY()
public:
    AEWInteriorAudit();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
private:
    void Finish(const FString& Failure=FString());
    FString Report,PhotoPath;
    TArray<EW::InteriorRoom> Rooms;
    TArray<FVector> Route;
    TArray<TSharedPtr<class FJsonValue>> Photos,Results;
    UPROPERTY() TObjectPtr<ACameraActor> Camera;
    FVector LastPosition;
    int32 Phase=0,RoomIndex=0,Point=0,InitialFalls=0,MonitorSide=0;
    double Started=0,Stage=0,WaitUntil=0,Walked=0,Best=DBL_MAX,ProgressAt=0;
    bool Done=false,SkipPhotos=false,VisualOnly=false,Captured=false;
};
