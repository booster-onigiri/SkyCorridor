#pragma once
#include "EWWorld.h"

namespace EWLighting
{
// Camera-only inspections share actual gallery geometry with traversal tests.
inline FTransform PreviewView(const EW::ChunkRecipe& Recipe,int32 Index)
{
    const FVector Offsets[]={FVector(0,-1250,164),FVector(0,-2250,164),FVector(0,-1250,164),FVector(-1650,-650,164),FVector(0,-1250,164)};
    const FRotator Rotations[]={FRotator(12,35,0),FRotator(13,90,0),FRotator(12,35,0),FRotator(2,195,0),FRotator(34,270,0)};
    Index=FMath::Clamp(Index,0,4);FVector Eye=Recipe.Hub+Offsets[Index];FRotator Rotation=Rotations[Index];
    if(Index==2)
    {
        const EW::CityWalkFloor* Floor=nullptr;
        for(const auto& Candidate:Recipe.CityFloors)
            if(Candidate.Column==0 && Candidate.Transform.GetLocation().Z>=Recipe.Hub.Z+3600 &&
                (!Floor || Candidate.Transform.GetLocation().Z<Floor->Transform.GetLocation().Z))Floor=&Candidate;
        if(Floor)
        {
            double Nearest=DBL_MAX;
            for(double X:{-2150.,2150.})for(double Y:{-2150.,2150.})
            {
                const FVector Corner=Floor->Transform.TransformPosition(FVector(X,Y,164));
                const double Distance=FVector::DistSquared2D(Corner,Recipe.Hub);
                if(Distance<Nearest){Nearest=Distance;Eye=Corner;}
            }
            Rotation=(FVector(Recipe.Hub.X,Recipe.Hub.Y,Eye.Z-550)-Eye).Rotation();
        }
    }
    return FTransform(Rotation,Eye);
}
}
