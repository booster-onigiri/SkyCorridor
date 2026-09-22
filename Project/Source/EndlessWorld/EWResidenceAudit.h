#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWWorld.h"
#include "EWResidenceAudit.generated.h"
class AEWCharacter;
class AEWChunkManager;
class AEWResidence;

UCLASS()
class ENDLESSWORLD_API AEWResidenceAudit : public AActor
{
    GENERATED_BODY()
public:
    AEWResidenceAudit();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
private:
    AEWCharacter* Player() const;
    AEWChunkManager* Manager() const;
    AEWResidence* Residence() const;
    void Event(const FString& Type,const FString& Detail);
    void WaitingEvidence();
    void Finish(const FString& Failure=FString());
    void SetupAt(EW::ChunkCoord Coord,FVector CapsuleCentre);
    void BeginWalk(const FString& Name,const TArray<FVector>& Points,int32 Next);
    void AimAt(FVector ChunkPoint,int32 Next);
    void Photo(const FString& Name,FVector ChunkTarget,int32 Next);
    bool VerifyState(int32 Expected);
    bool InteractWith(bool Seat);
    bool GroundReady(EW::ChunkCoord Coord) const;
    EW::ResidenceSpec Spec;
    EW::PlaceBookmark StandingBookmark;
    EW::ChunkCoord FarCoord;
    TWeakObjectPtr<AEWResidence> RetiredResidence;
    TArray<FVector> Route;
    TArray<TSharedPtr<class FJsonValue>> Evidence,Photos;
    FString ReportPath,StreamPath,RouteName,PhotoPath;
    int32 Phase=0,NextPhase=0,Point=0,InitialFalls=0,InitialRebases=0;
    double Started=0,StageStart=0,WaitUntil=0,BestDistance=DBL_MAX,ProgressAt=0,Walked=0,LastWaitingEvidence=0;
    FVector LastPosition=FVector::ZeroVector;
    bool bFinished=false,bResume=false,bSkipPhotos=false,bCaptured=false;
};
