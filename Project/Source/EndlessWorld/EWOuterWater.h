#pragma once
#include "EWWorld.h"
#include "EWWaterCity.h"

namespace EWOuterWater
{
ENDLESSWORLD_API bool Contains(const EW::WorldDescriptor& W,EW::ChunkCoord C);
ENDLESSWORLD_API double Ground(const EW::WorldDescriptor& W,EW::ChunkCoord C);
ENDLESSWORLD_API double WaterHeight(const EW::WorldDescriptor& W,EW::ChunkCoord C);
ENDLESSWORLD_API void Build(EW::ChunkRecipe& R);
ENDLESSWORLD_API void Skyline(const EW::WorldDescriptor& W,EW::ChunkCoord C,TArray<EW::Part>& Out);
ENDLESSWORLD_API EW::PlaceBookmark Entrance(const EW::WorldDescriptor& W);
ENDLESSWORLD_API TArray<FVector> UpperRoute(const EW::ChunkRecipe& R);
ENDLESSWORLD_API TArray<EW::WaterCityFall> Falls(const EW::WorldDescriptor& W,EW::ChunkCoord C);
ENDLESSWORLD_API TArray<FVector> Lamps(const EW::ChunkRecipe& R);
ENDLESSWORLD_API FString Name(int32 Kind);
ENDLESSWORLD_API FString Description(int32 Kind);
}
