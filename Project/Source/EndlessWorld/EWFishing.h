#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWFishingModel.h"
#include "EWSocialStore.h"
#include "EWFishing.generated.h"

class UStaticMesh;
class UStaticMeshComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UWidgetComponent;
class AEWCharacter;

UCLASS()
class ENDLESSWORLD_API AEWFishing : public AActor
{
    GENERATED_BODY()
public:
    AEWFishing();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    bool Interact();
    FString Hint() const;
    void Cancel();
    void RetrySave();
    bool FlushPending();
    void CastAgain();
    void OpenJournal();
    void VisitSpot(int32 Site);
    void TogglePatient();
    void DisplayFish(const FString& Id);
    bool Patient() const {return bPatient;}
    bool Saved() const {return bSaved;}
    bool NewSpecies() const {return bNewSpecies;}
    bool NewRecord() const {return bNewRecord;}
    bool Busy() const {return FishingCast.Active();}
    const EWFishing::FCatch& LastCatch() const {return FishingCast.Result();}
    EWFishing::EPhase Phase() const {return FishingCast.Phase();}
    const TArray<EWFishing::FRecord>& Records() const {return Collection;}
    FString DisplayedFish() const {return StoreDisplay;}
    FString SaveError() const {return Store?Store->Error():TEXT("生活データを開けません。");}
    double GameHour() const;
    UFUNCTION(BlueprintCallable, Category="Fishing verification") FString FishingEvidence() const;
    UFUNCTION(BlueprintCallable, Category="Fishing verification") bool PreviewFishing(int32 Site);
    UFUNCTION(BlueprintCallable, Category="Fishing verification") bool FishingAction(const FString& Action);
private:
    FEWSocialStore* Store=nullptr; // owned by the local social-session actor, including offline play
    EWFishing::FCast FishingCast;
    FRandomStream Random;
    TArray<EWFishing::FRecord> Collection;
    FString StoreDisplay;
    int32 ActiveSite=-1,ArrivalSite=-1;
    bool bReady=false,bPatient=true,bSaved=false,bNewSpecies=false,bNewRecord=false;
    UPROPERTY() TObjectPtr<USceneComponent> Root;
    UPROPERTY() TObjectPtr<UStaticMesh> Cube;
    UPROPERTY() TObjectPtr<UStaticMesh> Sphere;
    UPROPERTY() TObjectPtr<UStaticMesh> Cylinder;
    UPROPERTY() TObjectPtr<UStaticMesh> Cone;
    UPROPERTY() TObjectPtr<UMaterialInterface> Surface;
    UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Materials;
    UPROPERTY() TArray<TObjectPtr<USceneComponent>> SiteRoots;
    UPROPERTY() TObjectPtr<USceneComponent> Tackle;
    UPROPERTY() TObjectPtr<USceneComponent> DisplayRoot;
    UPROPERTY() TObjectPtr<USceneComponent> DisplayFishRoot;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Rod;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Line;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> Bobber;
    UPROPERTY() TObjectPtr<UStaticMeshComponent> DisplayBody;
    UPROPERTY() TArray<TObjectPtr<UWidgetComponent>> Signs;
    UStaticMeshComponent* Shape(USceneComponent* Parent,UStaticMesh* Mesh,FVector Position,FVector Size,FLinearColor Colour);
    void Beam(UStaticMeshComponent* Mesh,FVector A,FVector B,double Radius);
    void BuildScene();
    void BuildFish(USceneComponent* Parent,const EWFishing::FSpecies& Fish,double Length);
    void UpdateDisplay();
    void SaveLanded();
    int32 Nearby() const;
    AEWCharacter* Player() const;
};
