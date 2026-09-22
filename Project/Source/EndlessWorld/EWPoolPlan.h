#pragma once
#include "EWWorld.h"

namespace EWPoolPlan
{
struct Pool
{
    EW::ChunkCoord Coord;
    int32 Index=0,Column=0;
    FTransform Frame;
    FString Name;
};
constexpr double Surface=238,Bottom=18;
constexpr double HalfX=800,HalfY=450;
ENDLESSWORLD_API TOptional<Pool> Describe(const EW::ChunkRecipe& Recipe);
ENDLESSWORLD_API TArray<Pool> All();
ENDLESSWORLD_API void AddInfrastructure(EW::ChunkRecipe& Recipe);
ENDLESSWORLD_API EW::PlaceBookmark Arrival(const Pool& P);
}
