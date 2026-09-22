#pragma once
#include "EWWorld.h"
namespace EWAeroYachtPlan
{
    constexpr double Dwell=55,Journey=245,Period=Dwell+Journey;
    constexpr int32 Ships=2;
    constexpr double GangwayStart=3400;
    struct Layout { FVector Terminal,Ship; double Ceiling=0,OldRoof=0; };
    const Layout& Design();
    void AddInfrastructure(EW::ChunkRecipe& R);
    EW::PlaceBookmark Arrival();
    FTransform Pose(double Seconds,int32 Index);
    double Phase(double Seconds,int32 Index);
    inline bool Docked(double Seconds,int32 Index){return Phase(Seconds,Index)<Dwell;}
    inline bool GangwayOpen(double Seconds,int32 Index){const double T=Phase(Seconds,Index);return T>=3 && T<Dwell-7;}
    inline FVector Door(){return FVector(-2400,-1975,0);}
}
