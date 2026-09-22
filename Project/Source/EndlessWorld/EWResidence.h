#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWWorld.h"
#include "EWResidence.generated.h"

class AEWCharacter;
class UAudioComponent;
class UStaticMeshComponent;
class USoundWaveProcedural;
class UPointLightComponent;

enum class EEWResidenceAction : uint8 { None, Valve, Seat };

UCLASS()
class ENDLESSWORLD_API AEWResidence : public AActor
{
    GENERATED_BODY()
public:
    AEWResidence();
    bool Initialize(const EW::ResidenceSpec& InSpec,const FString& InWorldCode);
    static void AppendAssetPaths(TArray<FSoftObjectPath>& Paths);
    EEWResidenceAction Nearby(const AEWCharacter* Player,double& DistanceSquared) const;
    FString Hint(EEWResidenceAction Action) const;
    bool ToggleWater(FString& OutMessage);
    FTransform SeatTransform() const;
    FTransform StandTransform() const;
    int32 FlowMode() const { return Mode; }
    bool StateReady() const { return bStateReady; }
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    EW::ResidenceSpec Spec;
private:
    UPROPERTY() TObjectPtr<USceneComponent> Root;
    UPROPERTY() TObjectPtr<USceneComponent> Frame;
    UPROPERTY() TObjectPtr<USceneComponent> HandlePivot;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> EavesWater;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> WindowWater;
    UPROPERTY() TObjectPtr<UAudioComponent> EavesSound;
    UPROPERTY() TObjectPtr<UAudioComponent> WindowSound;
    UPROPERTY() TObjectPtr<UAudioComponent> HandleSound;
    UPROPERTY() TObjectPtr<USoundWaveProcedural> HandleWave;
    UPROPERTY() TArray<TObjectPtr<USoundWaveProcedural>> SoundWaves;
    UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> RoomLights;
    FTimerHandle LightingTimer;
    FString WorldCode,StateError;
    int32 Mode=0;
    bool bStateReady=false;
    double HandleAngle=-42.,TargetAngle=-42.,ClickRemaining=0;
    TArray<uint8> ClickPCM;
    void ApplyState(bool Animate);
    void UpdateLighting();
    UAudioComponent* CreateWaterSound(FVector Position,bool Close);
};
