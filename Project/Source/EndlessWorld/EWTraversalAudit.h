#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWWorld.h"
#include "EWTraversalAudit.generated.h"
class AEWCharacter;
class AEWChunkManager;
class AEWLift;
struct FEWTraversalCase
{
    FString Name;TArray<FVector> Points;int32 Kind=0,Column=-1;FTransform Floor;
    int32 Stop=INDEX_NONE;
};
UCLASS()
class ENDLESSWORLD_API AEWTraversalAudit:public AActor
{
    GENERATED_BODY()
public:
    AEWTraversalAudit();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
private:
    AEWCharacter* Player()const;
    AEWChunkManager* Manager()const;
    AEWLift* Lift(int32 Column)const;
    void BuildCases();
    void BeginCase();
    bool WalkTo(FVector Target);
    void SetupAt(FVector FloorPosition);
    void Event(const FString& Type,const FString& Text);
    void Finish(const FString& Failure=FString());
    void PassCase();
    int32 Phase=0,CaseIndex=0,PointIndex=0,InitialFalls=0,LiftLeg=0;
    int32 ExpectedRecoveries=0;
    double Started=0,StageStart=0,ProgressTime=0,BestDistance=DBL_MAX,Walked=0,Ridden=0;
    FVector LastPosition=FVector::ZeroVector;
    bool bMeasure=false,bFinished=false,bResume=false,bResumeRequested=false;
    bool bVertical=false,bBelowDeparture=false;
    double LowestDropZ=DBL_MAX;
    double ExpectedZ=0;
    FString ReportPath,StreamPath;
    TArray<FEWTraversalCase> Cases;
    TArray<TSharedPtr<class FJsonValue>> Results;
    EW::PlaceBookmark HighBookmark;
};
