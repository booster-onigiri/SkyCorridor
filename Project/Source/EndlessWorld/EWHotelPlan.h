#pragma once
#include "EWWorld.h"
namespace EWHotelPlan
{
inline bool IsSuite(int32 Kind){return Kind>=11 && Kind<=18;}
FString Name(int32 Index);
FString Description(int32 Index);
void AddInfrastructure(EW::ChunkRecipe& Recipe);
const EW::InteriorRoom* Find(const EW::ChunkRecipe& Recipe,int32 Index);
inline TArray<FVector> Route(){return {
    FVector(0,-740,-4),FVector(0,-300,0),FVector(-120,-300,0),FVector(-120,180,0),FVector(0,180,0),
    FVector(0,-100,0),FVector(555,-100,0),FVector(555,-270,0),FVector(555,-100,0),FVector(665,-100,0),FVector(665,190,0),FVector(665,-100,0),FVector(0,-100,0),
    FVector(0,490,0),FVector(0,750,-.55),FVector(0,490,0),FVector(0,-300,0),FVector(0,-740,-4)};}
}
