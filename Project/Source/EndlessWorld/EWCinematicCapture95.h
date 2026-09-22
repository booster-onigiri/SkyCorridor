#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FrameGrabber.h"
#include "EWCinematicCapture95.generated.h"

class ACameraActor;
class AEWSkyrail;
class AEWLift;
class AEWAeroYacht;
struct FCinematicPayload95;

// Explicit opt-in, standalone-game viewport capture. Never spawned in normal play.
UCLASS()
class ENDLESSWORLD_API AEWCinematicCapture95 : public AActor
{
    GENERATED_BODY()
public:
    AEWCinematicCapture95();
    virtual ~AEWCinematicCapture95();
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
private:
    bool PrepareShot();
    bool ApplyCameraOverride(float& FOV);
    void Pose(int32 Frame);
    void DrainFrames();
    void QueueFrame(int32 Frame);
    void CaptureHDR(int32 W,int32 H,const TArray<FLinearColor>& Pixels);
    void RecordPayload(const FCinematicPayload95& Payload);
    void Finish(const FString& Error = FString());
    UPROPERTY() TObjectPtr<ACameraActor> Camera;
    UPROPERTY() TObjectPtr<AEWSkyrail> FilmedTrain;
    UPROPERTY() TObjectPtr<AEWLift> FilmedLift;
    UPROPERTY() TObjectPtr<AEWAeroYacht> FilmedYacht;
    FDelegateHandle HDRHandle;
    TSharedPtr<FCinematicPayload95,ESPMode::ThreadSafe> PendingHDR;
    TUniquePtr<FFrameGrabber> Grabber;
    FString Output, Name, Source, CameraOverridePath, LiftId;
    FVector EyeStart, EyeEnd, AimStart, AimEnd;
    FVector StreamBookmark;
    int64 ChunkX=0,ChunkY=0;
    int32 TrainLine=-1,TrainCar=0;
    double TrainClockStart=0,TrainClockNow=0;
    bool bTrainCamera=false,bLiftCamera=false,bYachtCamera=false,bChunkAim=false;
    bool bHDRCapture=false,bCinematic96=false;
    int32 LiftFrom=0,LiftTo=0,YachtIndex=-1;
    double LiftClockStart=0,YachtClockStart=0,VehicleClockNow=0;
    int32 Shot=0, Phase=0, Requested=0, Written=0, Frames=240, FPS=30;
    int32 WarmupRequested=0, WarmupRendered=0;
    int32 Width=1920, Height=1080;
    double Started=0, Stage=0, Warmup=12, Timeout=420, PreviousFixedDelta=0;
    bool Done=false, PreviousFixed=false, OwnsFixed=false;
    bool CameraOverrideApplied=false;
    TSet<int32> Received;
    TArray<TSharedPtr<class FJsonValue>> FrameEvidence;
    TArray<TSharedPtr<class FJsonValue>> PoseEvidence;
};
