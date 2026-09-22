#pragma once
#include "CoreMinimal.h"
#include "EWWorld.h"
#include "GameFramework/Actor.h"
#include "EWInteriors.generated.h"
class UPointLightComponent;
class USpotLightComponent;
namespace EWInteriors
{
    bool AddBlock(EW::ChunkRecipe& Recipe,const EW::Part& Block);
    void AddRain(EW::ChunkRecipe& Recipe,const EW::ResidenceSpec& Residence);
    TArray<FVector> Route(const EW::InteriorRoom& Room);
    FString Name(int32 Kind);
    FString NearbyText(const EW::ChunkRecipe& Recipe,const FVector& Position);
}

// A fixed pool follows nearby rooms; thousands of streamed homes do not each
// allocate ticking lights or shadow maps.
UCLASS()
class ENDLESSWORLD_API AEWInteriorLighting:public AActor
{
    GENERATED_BODY()
public:
    AEWInteriorLighting();
    virtual void Tick(float DeltaSeconds) override;
    TSharedRef<class FJsonObject> Evidence() const;
private:
    UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> Lights;
    UPROPERTY() TArray<TObjectPtr<USpotLightComponent>> PendantLights;
    int32 ActiveCafePendants=0;
};
