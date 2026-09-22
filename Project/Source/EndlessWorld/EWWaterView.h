#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWPoolPlan.h"
#include "EWWaterView.generated.h"

struct FEWWaterSample
{
    FString Id;
    FTransform Frame;
    FVector Extent=FVector::ZeroVector;
    double Bottom=0,Depth=0;
};
UCLASS()
class ENDLESSWORLD_API AEWWaterView : public AActor
{
    GENERATED_BODY()
public:
    AEWWaterView();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    bool Sample(const FVector& Eye,FEWWaterSample& Out) const;
    UFUNCTION(BlueprintCallable) bool VisitPool(int32 Index);
    UFUNCTION(BlueprintCallable) bool PreviewPool(int32 Index,int32 View,double Hour);
    UFUNCTION(BlueprintCallable) bool PreviewOptics(int32 Mode);
    UFUNCTION(BlueprintCallable) FString WaterEvidence() const;
    TSharedRef<class FJsonObject> Evidence() const;
    const TArray<EWPoolPlan::Pool>& Pools() const{return Sites;}
private:
    UPROPERTY() TObjectPtr<class UPostProcessComponent> Optical;
    UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> Material;
    UPROPERTY() TArray<TObjectPtr<class UPointLightComponent>> Lamps;
    UPROPERTY() TObjectPtr<class ACameraActor> PreviewCamera;
    TArray<EWPoolPlan::Pool> Sites;
    FEWWaterSample Current;
    float Weight=0;
    bool bLoaded=false;
};
