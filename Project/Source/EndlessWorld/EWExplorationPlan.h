#pragma once
#include "EWWorld.h"
class UEWGameInstance;
namespace EWExplorationPlan
{
inline bool IsPublic(int32 Kind){return Kind>=19 && Kind<=24;}
FString Name(int32 Index);
FString Description(int32 Index);
FString Directions(int32 Index);
bool AddBlock(EW::ChunkRecipe& R,const EW::Part& P);
void Finish(EW::ChunkRecipe& R);
const EW::InteriorRoom* Find(const EW::ChunkRecipe& R,int32 Index);
EW::PlaceBookmark Bookmark(const EW::ChunkRecipe& R,const EW::InteriorRoom& Room,bool Entry=false);
FString Hint(const UEWGameInstance* G);
bool Sit(UEWGameInstance* G);
}
