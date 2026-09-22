#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWAeroYacht.generated.h"
class AEWCharacter;
class UStaticMeshComponent;
class UBoxComponent;
class UPointLightComponent;
class UWidgetComponent;
class UMaterialInstanceDynamic;
class ACameraActor;

UCLASS()
class ENDLESSWORLD_API AEWAeroYacht:public AActor
{
    GENERATED_BODY()
public:
    AEWAeroYacht();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    UPROPERTY(BlueprintReadOnly) int32 Index=0;
    bool Contains(const AEWCharacter* P) const;
    bool AtDock() const;
    bool CanBoard() const;
    bool Ready() const{return bReady;}
    double Clock() const;
    FString Name() const;
    FString Hint(const AEWCharacter* P) const;
    TSharedRef<class FJsonObject> Evidence() const;
    UFUNCTION(BlueprintCallable) FString YachtEvidence() const;
    UFUNCTION(BlueprintCallable) bool PreviewYacht(int32 View,double Hour);
    bool AuditTime(double Seconds);
    bool SetCinematicClock(double Seconds);
    bool CinematicDeckFrame(FTransform& Out) const;
    void ReleaseCinematicClock();
    void AuditRun(bool Run){bAdvanceOverride=Run;}
    UStaticMeshComponent* WalkingBody() const{return Collision;}
private:
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Meshes;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Collision;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Door;
    UPROPERTY() TObjectPtr<UBoxComponent> DoorBody;
    UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> Lights;
    UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> DeckLamps;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Glow;
    UPROPERTY() TObjectPtr<ACameraActor> PreviewCamera;
    double OverrideClock=-1;
    bool bReady=false,bWasOpen=false;
    bool bAdvanceOverride=false;
    bool bCinematicClock=false,bApplyingCinematicClock=false,bPreviousAdvance=false;
    double PreviousCinematicClock=-1;
    int32 PreviewView=-1;
    void UpdatePreview();
};

UCLASS()
class ENDLESSWORLD_API AEWAeroPort:public AActor
{
    GENERATED_BODY()
public:
    AEWAeroPort();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    FString Status() const;
    TSharedRef<class FJsonObject> Evidence() const;
private:
    UPROPERTY() TObjectPtr<USceneComponent> BridgePivot;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Bridge;
    UPROPERTY() TObjectPtr<UBoxComponent> BridgeBody;
    UPROPERTY() TArray<TObjectPtr<UBoxComponent>> BridgeRailBodies;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Gate;
    UPROPERTY() TObjectPtr<UBoxComponent> GateBody;
    UPROPERTY() TObjectPtr<UWidgetComponent> Sign;
    float Extension=0;
    bool bOpen=false,bReady=false;
};
