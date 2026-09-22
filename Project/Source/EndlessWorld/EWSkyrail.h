#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWSkyrail.generated.h"
class AEWCharacter;
class UStaticMeshComponent;
class UPointLightComponent;
class UWidgetComponent;
class UMaterialInstanceDynamic;
class ACameraActor;
class UBoxComponent;

UCLASS()
class ENDLESSWORLD_API AEWSkyrail : public AActor
{
    GENERATED_BODY()
public:
    AEWSkyrail();
    UPROPERTY(EditAnywhere,BlueprintReadWrite,Category="Rail") int32 Line=0;
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    bool Interact(AEWCharacter* P);
    FString Hint(const AEWCharacter* P) const;
    bool IsRider(const AEWCharacter* P) const;
    void ReleaseRider();
    int32 AtStation() const;
    int32 Destination() const;
    bool IsReady() const {return bReady;}
    int32 NearbyStation(const AEWCharacter* P) const;
    FVector BoardingPosition(int32 Station) const;
    FVector LiftEntry(int32 Station) const;
    FString StationText(int32 Station) const;
    TSharedRef<class FJsonObject> Evidence() const;
    // Isolated cinematic mode only; normal play cannot set or freeze the clock.
    bool SetCinematicClock(double Seconds);
    bool CinematicCarFrame(int32 Car, FTransform& Out) const;
    void ReleaseCinematicClock();
    UFUNCTION(BlueprintCallable,Category="Rail Preview") bool PreviewStation(int32 Station,bool Exterior);
    UFUNCTION(BlueprintCallable,Category="Rail Preview") FString RailEvidence() const;
    int32 Boardings=0,Arrivals=0,Alightings=0;
private:
    UPROPERTY() TObjectPtr<USceneComponent> Train;
    UPROPERTY() TArray<TObjectPtr<USceneComponent>> CarFrames;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Cars;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Doors;
    UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> Lights;
    UPROPERTY() TArray<TObjectPtr<UWidgetComponent>> Signs;
    UPROPERTY() TArray<TObjectPtr<UWidgetComponent>> Guides;
    TArray<FVector> GuidePositions;
    bool bGuidesBuilt=false;
    void BuildGuides();
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> BodyDetails;
    UPROPERTY() TArray<TObjectPtr<UBoxComponent>> Collision;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Glow;
    UPROPERTY() TWeakObjectPtr<AEWCharacter> Rider;
    UPROPERTY() TObjectPtr<ACameraActor> PreviewCamera;
    int32 Phase=0,BoardedAt=0,RiderCar=0;
    double Elapsed=0,DoorOpen=0,Distance=0;
    double Speed=0,PeakSpeed=0;
    bool bReady=false;
    bool bCinematicClock=false,bApplyingCinematicClock=false;
    FVector RenderOffset=FVector::ZeroVector;
    FTransform StandAt(int32 Station) const;
};
