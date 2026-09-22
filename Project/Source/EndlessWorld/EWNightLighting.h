#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWNightLighting.generated.h"
class UPointLightComponent;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;

struct FEWNightFixture
{
    FString Id;
    FVector Position;
    FRotator Rotation;
    float Lumens=0,Radius=0;
    bool Spot=false;
};
struct FEWNightLightSlot
{
    FString Id;
    float Fade=0;
};

// Fixtures follow the existing public floors and bridge ribs. No route, save
// code, collider or generated building is changed by this presentation layer.
UCLASS()
class ENDLESSWORLD_API AEWNightLighting:public AActor
{
    GENERATED_BODY()
public:
    AEWNightLighting();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    UFUNCTION(BlueprintCallable,Category="Lighting Preview") bool PreviewNightLights(float Gain);
    TSharedRef<class FJsonObject> Evidence() const;
private:
    UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> Lights;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Lanterns;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Housings;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Lenses;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Glow;
    TArray<FEWNightFixture> Fixtures;
    TArray<FEWNightLightSlot> Slots;
    FString CacheKey;
    float PreviewGain=1,LastNightScale=0;
    int32 Rebuilds=0;
};
