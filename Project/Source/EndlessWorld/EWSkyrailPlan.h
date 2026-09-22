#pragma once
#include "EWWorld.h"
class FJsonObject;
namespace EWSkyrailPlan
{
constexpr int32 Stations=3; // maximum platforms per line
constexpr double LineY=6400, FloorZ=3150, UpperZ=30000, Dwell=14;
inline int32 Count(int32 Line=0){return Line?3:2;}
inline int32 PhaseCount(int32 Line=0){return 4*(Count(Line)-1);}
inline double Height(int32 Line=0){return Line?UpperZ:FloorZ;}
inline double TrackY(double X)
{
    auto Ease=[](double T){T=FMath::Clamp(T,0.,1.);return T*T*(3.-2.*T);};
    return LineY+220.*Ease((X-12300.)/2900.)*(1.-Ease((X-18800.)/1700.));
}
inline FTransform TrackPose(double X,int32 Line=0)
{return FTransform(FRotator(0,FMath::RadiansToDegrees(FMath::Atan2(TrackY(X+1)-TrackY(X-1),2.)),0),FVector(X,TrackY(X),Height(Line)));}
inline double StopX(int32 I,int32 Line=0){return Line?(I==0?5000:I==1?10000:22500):(I==0?10000:42800);}
inline FVector Stop(int32 I,int32 Line=0){return FVector(StopX(I,Line),LineY,Height(Line));}
inline EW::ChunkCoord StopChunk(int32 I,int32 Line=0){return {int64(StopX(I,Line)/EW::ChunkSize),0};}
inline FVector Boarding(int32 I,int32 Line=0){return Stop(I,Line)+FVector(-560,350,91);}
inline FString Name(int32 I,int32 Line=0){return Line?(I==0?TEXT("雲上ホテル"):I==1?TEXT("天空シアター"):TEXT("アウレリア空中港")):(I==0?TEXT("時計広場"):TEXT("白塔の水都"));}
inline FString LineName(int32 Line){return Line?TEXT("上層線 / ホテル・シアター・空港"):TEXT("水都線 / 時計広場・白塔の水都");}
inline FString LiftId(int32 I,int32 Line=0){return Line?(I==0?TEXT("skyrail88/hotel"):I==1?TEXT("skyrail78/0"):TEXT("skyrail88/airport")):(I==0?TEXT("skyrail78/0"):TEXT("skyrail78/2"));}
inline int32 Departure(int32 Phase,int32 Line=0){const int32 N=Count(Line),K=(Phase/2)%(2*(N-1));return K<N?K:2*(N-1)-K;}
inline int32 Destination(int32 Phase,int32 Line=0){return Departure((Phase+2)%PhaseCount(Line),Line);}
inline double Duration(int32 Phase,int32 Line=0){return Phase%2?FMath::Abs(StopX(Destination(Phase,Line),Line)-StopX(Departure(Phase,Line),Line))/1300.+3.:Dwell;}
inline double Progress(double Elapsed,double Duration)
{
    const double T=FMath::Clamp(Elapsed,0.,Duration),Ramp=FMath::Min(3.,Duration*.4),V=1./(Duration-Ramp);
    if(T<Ramp)return .5*V*T*T/Ramp;
    if(T>Duration-Ramp){const double Left=Duration-T;return 1.-.5*V*Left*Left/Ramp;}
    return V*(T-Ramp*.5);
}
inline bool Enabled(const EW::WorldDescriptor& W){return W.Seed==EW::WorldDescriptor::ReferenceWorld().Seed;}
// Global floor-level waypoints shared by the walkways, guidance and physical audit.
TArray<FVector> HotelPath(const EW::ChunkRecipe& R,int32 Room);
TArray<FVector> TheatrePath(const EW::ChunkRecipe& R);
TArray<FVector> AirportPath(const EW::ChunkRecipe& R);
void AddInfrastructure(EW::ChunkRecipe& R);
TSharedRef<FJsonObject> AuditRoute();
}
