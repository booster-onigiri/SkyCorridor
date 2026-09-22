#include "EWExplorationPlan.h"
#include "EWExplore85Data.h"
#include "EWGameInstance.h"
#include "EWChunkManager.h"
#include "EWCharacter.h"
#include "Kismet/GameplayStatics.h"

FString EWExplorationPlan::Name(int32 I)
{
    static const TCHAR* N[]={TEXT("水庭のカフェ"),TEXT("雲待ちのラウンジ"),TEXT("空の大図書館"),TEXT("旅の地図室"),TEXT("光の美術館"),TEXT("硝子の植物園")};
    return I>=0 && I<6?N[I]:TEXT("街の公共施設");
}
FString EWExplorationPlan::Description(int32 I)
{
    static const TCHAR* N[]={
        TEXT("水盤と木のカウンターを囲む喫茶室。窓際に座り、回廊を行き交う景色を眺める。"),
        TEXT("布の天蓋、柔らかな椅子、静かな水面。散策の合間に立ち寄る空の休憩所。"),
        TEXT("高さ18mの吹き抜けと壁を埋める本棚。階段の先の閲覧回廊から、書庫全体を見渡せる。"),
        TEXT("まだ名のない島々の模型と、大きな地図のある書斎。都市を小さく見つめ直す場所。"),
        TEXT("白い展示壁を巡り、色の風景画や金属の彫刻に出会う。街の光も展示の一部になる美術館。"),
        TEXT("ステンレスの斜めの柱と大きなガラス壁。緑と水、都市の風景を重ねる空中植物園。")};
    return I>=0 && I<6?N[I]:FString();
}
FString EWExplorationPlan::Directions(int32 I)
{
    static const TCHAR* N[]={TEXT("時計広場から南西の塔へ・広場と同じ階"),TEXT("カフェの塔の昇降機で、カフェから1つ上の停車階へ"),
        TEXT("広場の南東の塔・昇降機の「空の大図書館」へ"),TEXT("大図書館の塔で、さらに1つ上の停車階へ"),
        TEXT("映画館の塔・昇降機の「光の美術館」へ"),TEXT("美術館の塔で、さらに1つ上の停車階へ")};
    return I>=0 && I<6?N[I]:FString();
}
const EW::InteriorRoom* EWExplorationPlan::Find(const EW::ChunkRecipe& R,int32 I)
{return R.Interiors.FindByPredicate([I](const auto& Room){return Room.Kind==19+I;});}
bool EWExplorationPlan::AddBlock(EW::ChunkRecipe& R,const EW::Part& P)
{
    if(R.Coord!=EW::ChunkCoord{0,0} || R.World.Seed!=EW::WorldDescriptor::ReferenceWorld().Seed)return false;
    const auto V=P.Transform.GetLocation();int32 Pair=-1;
    if(V.X<6400 && V.Y<6400 && FMath::Abs(V.Z-R.Hub.Z)<1)Pair=0;
    if(V.X>6400 && V.Y<6400 && FMath::Abs(V.Z-R.Hub.Z-3600)<1)Pair=1;
    if(V.X>6400 && V.Y>6400 && FMath::Abs(V.Z-R.Hub.Z-3600)<1)Pair=2;
    if(Pair<0)return false;
    R.Parts.RemoveAll([&](const auto& Old){return Old.Mesh==P.Mesh && Old.Transform.Equals(P.Transform);});
    for(int32 Level=0;Level<2;++Level)
    {
        const int32 I=Pair*2+Level;auto T=P.Transform;T.AddToTranslation(FVector(0,0,Level*1800));
        R.Interiors.Add({T,19+I,I});
        for(const TCHAR* Group:{TEXT("Shell"),TEXT("Furniture"),TEXT("Glass")})
            R.Parts.Add({FName(*FString::Printf(TEXT("Explore85%s%d"),Group,I)),T,0});
        for(const auto& B:EWExplore85Data::Bodies(19+I))
            R.Colliders.Add({FTransform(T.GetRotation(),T.TransformPosition(B.P)),B.E*T.GetScale3D().GetAbs(),B.Floor});
    }
    return true;
}
EW::PlaceBookmark EWExplorationPlan::Bookmark(const EW::ChunkRecipe& R,const EW::InteriorRoom& Room,bool Entry)
{
    EW::PlaceBookmark P;const int32 I=Room.Kind-19;P.WorldCode=R.World.Code();P.Coord=R.Coord;
    P.Id=FString::Printf(TEXT("exploration85/%d"),I);P.Kind=9+I;P.Name=Name(I);
    P.LocalPosition=Room.Frame.TransformPosition(Entry?FVector(0,-2100,100):FVector(0,500,100));
    P.Yaw=Room.Frame.Rotator().Yaw+90;return P;
}
void EWExplorationPlan::Finish(EW::ChunkRecipe& R)
{
    for(const auto& Room:R.Interiors)if(IsPublic(Room.Kind))
    {
        R.Places.Add(Bookmark(R,Room));
        for(auto& Lift:R.Lifts)if(FVector::Dist2D(FVector(Lift.Cabin.X,Lift.Cabin.Y,0),Room.Frame.GetLocation())<3500)
            for(auto& Stop:Lift.Stops)if(FMath::Abs(Stop.Height-Room.Frame.GetLocation().Z)<1)
                Stop.Label=Name(Room.Kind-19)+TEXT("　")+Stop.Label;
    }
}
namespace
{
const EW::InteriorRoom* Current(const UEWGameInstance* G,FVector& Local)
{
    const auto* M=G?G->Manager.Get():nullptr;const auto* P=G?UGameplayStatics::GetPlayerCharacter(G,0):nullptr;
    const auto R=M?M->RecipeAt(M->PlayerCoord()):nullptr;if(!R || !P)return nullptr;
    for(const auto& Room:R->Interiors)if(EWExplorationPlan::IsPublic(Room.Kind))
    {
        Local=Room.Frame.InverseTransformPosition(M->ToLocal(R->Coord,P->GetActorLocation()));
        if(Local.Z>-50 && Local.Z<1000 && FMath::Abs(Local.X)<1800 && FMath::Abs(Local.Y)<2200)return &Room;
    }
    return nullptr;
}
}
FString EWExplorationPlan::Hint(const UEWGameInstance* G)
{
    FVector Local;const auto* Room=Current(G,Local);if(!Room)return {};
    const auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(G,0));
    if(P && P->IsSeated())return Name(Room->Kind-19)+TEXT("　E / WASD 立ち上がる　マウス 見回す");
    if(FVector::Dist(Local,FVector(600,-1450,90))<190)return Name(Room->Kind-19)+TEXT("　窓辺でひと休み　E 座る");
    const auto R=G->Manager->RecipeAt(G->Manager->PlayerCoord());
    const auto Place=Bookmark(*R,*Room);
    if(FVector::Dist(Local,FVector(0,500,100))<450)
        return Name(Room->Kind-19)+(G->IsRecorded(Place)?TEXT("　発見済み　Tab 図鑑"):TEXT("　E 発見を記録する"));
    return Name(Room->Kind-19)+(Local.Y<-1800?TEXT("　このまま歩いて入れます"):TEXT("　館内の案内台を探してみよう"));
}
bool EWExplorationPlan::Sit(UEWGameInstance* G)
{
    FVector Local;const auto* Room=Current(G,Local);if(!Room || FVector::Dist(Local,FVector(600,-1450,90))>=190)return false;
    auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(G,0));if(!P)return false;
    const auto* M=G->Manager.Get();const auto C=M->PlayerCoord();
    auto T=[&](FVector V){return FTransform(FRotator(0,Room->Frame.Rotator().Yaw-90,0),M->ToRender(C,Room->Frame.TransformPosition(V)));};
    return P->SitAtTransform(G->Manager.Get(),T(FVector(600,-1300,98)),T(FVector(600,-1450,90)));
}
