#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWAirship.generated.h"
class UStaticMeshComponent;
class UMaterialInstanceDynamic;
UCLASS()
class ENDLESSWORLD_API AEWAirship:public AActor
{
    GENERATED_BODY()
public:
    AEWAirship();
    UPROPERTY(BlueprintReadOnly) int32 FleetIndex=0;
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    TSharedRef<class FJsonObject> Evidence() const;
private:
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Meshes;
    UPROPERTY() TArray<TObjectPtr<UStaticMeshComponent>> Props;
    UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> Glow;
    double Started=0,CruiseZ=0;
    bool Ready=false;
};
