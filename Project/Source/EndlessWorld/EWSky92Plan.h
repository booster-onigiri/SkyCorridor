#pragma once
#include "EWWorld.h"
class USceneComponent;
namespace EWSky92Plan
{
ENDLESSWORLD_API int32 Landmark(EW::ChunkCoord C);
ENDLESSWORLD_API EW::ChunkCoord Coord(int32 I);
ENDLESSWORLD_API FTransform Frame(const EW::WorldDescriptor& W,int32 I);
ENDLESSWORLD_API FString Name(int32 I);
ENDLESSWORLD_API FString Directions(int32 I);
ENDLESSWORLD_API void Build(EW::ChunkRecipe& R);
ENDLESSWORLD_API TArray<FVector> Lamps(const EW::ChunkRecipe& R);
ENDLESSWORLD_API void Clouds(USceneComponent* Parent,const EW::WorldDescriptor& W,EW::ChunkCoord C);
}
