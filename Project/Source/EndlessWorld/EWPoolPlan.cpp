#include "EWPoolPlan.h"
#include "EWPool90Data.h"

TOptional<EWPoolPlan::Pool> EWPoolPlan::Describe(const EW::ChunkRecipe& R)
{
    if(R.World.Seed!=EW::WorldDescriptor::ReferenceWorld().Seed)return {};
    const EW::ChunkCoord Sites[]={{0,1},{1,1},{-1,0}};
    const TCHAR* Names[]={TEXT("北の水鏡プール"),TEXT("雲を眺めるプール"),TEXT("夕凪のプール")};
    int32 Index=INDEX_NONE;for(int32 I=0;I<3;++I)if(R.Coord==Sites[I])Index=I;
    if(Index==INDEX_NONE)return {};
    const EW::Part* Garden=nullptr;
    for(const auto& Part:R.Parts)if(Part.Mesh.ToString().StartsWith(TEXT("UrbanGarden_")) &&
        (!Garden || Part.Transform.GetLocation().Z>Garden->Transform.GetLocation().Z))Garden=&Part;
    if(!Garden)return {};
    Pool P;P.Coord=R.Coord;P.Index=Index;P.Name=Names[Index];
    P.Frame=Garden->Transform;P.Frame.SetLocation(Garden->Transform.TransformPosition(FVector(650,650,0)));
    for(const auto& Floor:R.CityFloors)if(Floor.bRoof && Floor.Transform.GetLocation().Equals(Garden->Transform.GetLocation(),1))P.Column=Floor.Column;
    return P;
}
TArray<EWPoolPlan::Pool> EWPoolPlan::All()
{
    TArray<Pool> Out;const auto W=EW::WorldDescriptor::ReferenceWorld();
    for(EW::ChunkCoord C:{EW::ChunkCoord{0,1},EW::ChunkCoord{1,1},EW::ChunkCoord{-1,0}})
    {const auto R=EW::GenerateChunk(W,C);if(auto P=Describe(R))Out.Add(*P);}
    return Out;
}
void EWPoolPlan::AddInfrastructure(EW::ChunkRecipe& R)
{
    const auto P=Describe(R);if(!P)return;
    for(const TCHAR* Part:{TEXT("Pool90Stone"),TEXT("Pool90Water"),TEXT("Pool90Glass"),TEXT("Pool90Details")})
        R.Parts.Add({FName(Part),P->Frame,0});
    for(const auto& B:EWPool90Data::Bodies())
        R.Colliders.Add({FTransform(P->Frame.GetRotation()*FRotator(0,B.Yaw,0).Quaternion(),P->Frame.TransformPosition(B.P)),B.E*P->Frame.GetScale3D(),B.Floor});
    const FString Id=FString::Printf(TEXT("%s/%d"),*R.Coord.Text(),P->Column);
    for(auto& L:R.Lifts)if(L.Id==Id)for(auto& S:L.Stops)
        if(FMath::Abs(S.Height-P->Frame.GetLocation().Z)<1)S.Label=FString::Printf(TEXT("屋上庭園・%s"),*P->Name);
}
EW::PlaceBookmark EWPoolPlan::Arrival(const Pool& P)
{
    EW::PlaceBookmark B;B.WorldCode=EW::WorldDescriptor::ReferenceWorld().Code();B.Coord=P.Coord;
    B.Id=TEXT("pool90/")+FString::FromInt(P.Index);B.Name=P.Name;B.Kind=0;
    B.LocalPosition=P.Frame.TransformPosition(FVector(-1490,0,100));B.Yaw=P.Frame.Rotator().Yaw;return B;
}
