#pragma once
#include "EWWorld.h"

struct FEWMemoryPlace
{
    EW::PlaceBookmark Place;
    FString Area, Moment, Record, Direction;
    FVector Approach=FVector::ZeroVector;
};
namespace EWMemoryPlan
{
    TArray<FEWMemoryPlace> Build();
}
