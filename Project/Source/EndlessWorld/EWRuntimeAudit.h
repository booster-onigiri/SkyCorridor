#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWWorld.h"
#include "EWRuntimeAudit.generated.h"

class AEWCharacter;
class AEWChunkManager;
struct FEWAuditBoundary { EW::ChunkCoord C; int32 Axis = 0, From = 0, To = 0; };
struct FEWAuditPoint { EW::ChunkCoord C; FVector Local; };

/** Opt-in verification runner. Uses AddMovementInput and ordinary CharacterMovement.
 *  Inter-case travel is explicitly logged; it never teleports to pass a walking segment. */
UCLASS()
class ENDLESSWORLD_API AEWRuntimeAudit : public AActor
{
    GENERATED_BODY()
public:
    AEWRuntimeAudit();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
private:
    AEWCharacter* Player() const;
    AEWChunkManager* Manager() const;
    void Event(const FString& Type, const FString& Detail);
    void Fail(const FString& Detail);
    void BeginCase();
    void BeginSoak();
    bool MakeRoute(EW::ChunkCoord C, int32 Entry, int32 Exit);
    void Finish();
    TArray<FEWAuditBoundary> Cases;
    TArray<FEWAuditPoint> Route;
    TArray<FString> Failures;
    TArray<float> Frames;
    TArray<TSharedPtr<class FJsonValue>> CaseResults, LoopResults;
    TMap<EW::ChunkCoord, FString> RevisitDigests;
    FString ReportPath, StreamPath;
    EW::ChunkCoord SoakCoord;
    EW::ChunkCoord PreviousCoord;
    FVector PreviousLocal=FVector::ZeroVector;
    bool bHasPreviousPosition=false;
    int32 Phase = 0, CaseIndex = 0, PointIndex = 0, Segment = 0, Step = 0, Loops = 0, WalkedChunks = 0;
    int32 InitialRecoveries = 0, LastRegions = 0;
    int32 CapturedRegions = 0;
    double CaptureAfter = 0;
    int32 PeakResident = 0, PeakJobs = 0, PeakQueue = 0;
    double Started = 0, LastTick = 0, LastSample = 0, StageStarted = 0, ProgressTime = 0;
    double SoakStarted = 0, SoakSeconds = 7200, BestDistance = DBL_MAX, FrameSeconds = 0;
    uint64 PeakWorkingSet = 0, PeakVirtual = 0;
    bool bSoakOnly = false, bFinished = false;
};
