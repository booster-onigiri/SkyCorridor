#pragma once
#include "EWWorld.h"
#include "EWCinemaPlan.h"

namespace EWSkyTheatrePlan
{
    constexpr double Width=10000;
    inline FVector Screen(){return FVector(0,6200,3300);}
    inline FRotator ScreenRotation(){return FRotator(-24,-90,0);}
    inline FVector Entry(){return FVector(0,-2230,110);}
    bool Frame(const EW::ChunkRecipe& R,FTransform& Out);
    void AddInfrastructure(EW::ChunkRecipe& R);
    inline TArray<EWCinemaPlan::Seat> Seats()
    {
        TArray<EWCinemaPlan::Seat> Out;
        for(int32 Row=0;Row<4;++Row)for(double X:{-1550.,-1180.,-810.,-440.,440.,810.,1180.,1550.})
        {const double Y=-100-Row*450.,Z=Row*38.;Out.Add({TEXT("天空の客席"),FVector(X,Y,95+Z),FVector(X,Y+145,91+Z)});}
        return Out;
    }
    inline float AudioGain(const FVector& P)
    {
        if(P.Z<0 || P.Z>3000 || FMath::Abs(P.X)>=2350 || FMath::Abs(P.Y)>=2350)return 0;
        return float(FMath::Clamp(FMath::Min(2350-FMath::Abs(P.X),2350-FMath::Abs(P.Y))/250.,0.,1.));
    }
}
