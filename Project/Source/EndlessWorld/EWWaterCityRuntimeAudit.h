#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWWorld.h"
#include "EWWaterCityRuntimeAudit.generated.h"
class AEWCharacter;
class AEWChunkManager;
class AEWWaterCity;
class ACameraActor;
class UInstancedStaticMeshComponent;

struct FEWWaterCityRoute
{
    FString Name;
    TArray<FVector> Feet;
};
struct FEWWaterCityPhoto
{
    FString Name;
    FVector Eye,Target;
};

UCLASS()
class ENDLESSWORLD_API AEWWaterCityRuntimeAudit : public AActor
{
    GENERATED_BODY()
public:
    AEWWaterCityRuntimeAudit();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
private:
    AEWCharacter* Player() const;
    AEWChunkManager* Manager() const;
    bool Ready() const;
    bool VerifyComponents(const FString& Label);
    void Event(const FString& Kind,const FString& Detail);
    void Finish(const FString& Failure=FString());
    void Setup(EW::ChunkCoord Coord,FVector Feet);
    void BeginRoute();
    void BeginPhoto();
    void RestorePlayerCamera();
    FString ReportPath,StreamPath,PhotoPath;
    TArray<FEWWaterCityRoute> Routes;
    TArray<FEWWaterCityPhoto> Views;
    TArray<TSharedPtr<class FJsonValue>> Evidence,RouteResults,Photos;
    TArray<TWeakObjectPtr<AEWWaterCity>> RetiredCities;
    TWeakObjectPtr<UInstancedStaticMeshComponent> MotionSpray;
    FTransform FirstSprayTransform;
    UPROPERTY() TObjectPtr<ACameraActor> PhotoCamera;
    EW::ChunkCoord FarCoord;
    EW::PlaceBookmark SavedUpper;
    FVector LastPosition=FVector::ZeroVector;
    int32 Phase=0,RouteIndex=0,PointIndex=0,PhotoIndex=0,InitialFalls=0,InitialRebases=0;
    double Started=0,StageStart=0,WaitUntil=0,BestDistance=DBL_MAX,ProgressAt=0,Walked=0,RouteStarted=0,RouteWalked=0;
    bool bFinished=false,bCaptured=false,bSkipPhotos=false,bResumeOnly=false,bMotionSeen=false,bVisualOnly=false;
};
