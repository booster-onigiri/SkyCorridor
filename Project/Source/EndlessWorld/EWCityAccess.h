#pragma once
#include "EWWorld.h"
namespace EW
{
void BuildCityGround(ChunkRecipe& Recipe);
void BuildCityAccess(ChunkRecipe& Recipe);
bool IsRainWindowDistrict(const WorldDescriptor& World, ChunkCoord Coord);
bool DescribeRainWindow(const ChunkRecipe& Recipe, const Part& Block, ResidenceSpec& Out);
}
