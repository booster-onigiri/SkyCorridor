#pragma once
#include "CoreMinimal.h"
#include "EWWorld.h"
class AActor;
class USceneComponent;
class UWidgetComponent;
class FJsonObject;

namespace EWClock
{
    bool IsCentralPlaza(const EW::ChunkRecipe& Recipe);
    FVector ScreenCentre(const EW::ChunkRecipe& Recipe);
    FVector AnglesAt(const FDateTime& LocalTime);
    void AddFaces(AActor* Owner,USceneComponent* Parent,const EW::Part& Part,TArray<TObjectPtr<UWidgetComponent>>& Out);
    TSharedRef<FJsonObject> FaceEvidence(const UWidgetComponent* Face);
}
