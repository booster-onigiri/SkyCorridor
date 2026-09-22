#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWWorld.h"
#include "EWLift.generated.h"
class AEWCharacter;
class UBoxComponent;
class UInstancedStaticMeshComponent;

UCLASS()
class ENDLESSWORLD_API AEWLift:public AActor
{
    GENERATED_BODY()
public:
    AEWLift();
    void Initialize(const EW::LiftSpec& InSpec,double PlayerHeight);
    virtual void Tick(float Delta) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    bool Nearby(const AEWCharacter* Player,int32& Stop,bool& OnCar) const;
    bool Ride(AEWCharacter* Player,int32 Stop);
    bool Call(int32 Stop);
    bool IsMoving() const {return TargetStop!=INDEX_NONE;}
    int32 FloorIndex() const {return CurrentStop;}
    FVector CarPosition() const;
    // Isolated film96 only: reuse the actual cabin easing and landing orientation.
    bool SetCinematicTrip(int32 FromStop,int32 ToStop,double Seconds);
    bool CinematicCabinFrame(FTransform& Out) const;
    void ReleaseCinematicTrip();
    FVector LandingPosition(int32 Stop) const;
    FString Hint(const AEWCharacter* Player) const;
    EW::LiftSpec Spec;
    int32 CompletedRides=0,CompletedCalls=0;
private:
    UPROPERTY() TObjectPtr<USceneComponent> Root;
    UPROPERTY() TObjectPtr<USceneComponent> Cabin;
    UPROPERTY() TObjectPtr<UBoxComponent> Floor;
    UPROPERTY() TObjectPtr<UBoxComponent> CarGate;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> GateMeshes;
    UPROPERTY() TArray<TObjectPtr<UBoxComponent>> Doors;
    UPROPERTY() TWeakObjectPtr<AEWCharacter> Rider;
    int32 CurrentStop=INDEX_NONE,TargetStop=INDEX_NONE;
    double StartHeight=0,TargetHeight=0,Elapsed=0,Duration=0;
    bool bCinematicTrip=false,bApplyingCinematicTrip=false;
    FQuat StartRotation=FQuat::Identity,TargetRotation=FQuat::Identity;
    struct FGatePiece { int32 Instance, Stop; FVector Centre, Size; };
    TArray<FGatePiece> GatePieces;
    void AddGate(int32 Stop, double HalfWidth);
    void DrawGates(bool CarOnly=false);
    void UpdateDoors();
};
