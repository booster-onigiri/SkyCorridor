#include "EWSky92Plan.h"
#include "EWSky92Data.h"
#include "EWOuterWater.h"
#include "EWLocalization.h"
#include "Components/LocalFogVolumeComponent.h"
#include "GameFramework/Actor.h"

namespace EWSky92Plan
{
EW::ChunkCoord Coord(int32 I){return I==0?EW::ChunkCoord{3,0}:I==1?EW::ChunkCoord{4,0}:EW::ChunkCoord{5,1};}
int32 Landmark(EW::ChunkCoord C){for(int I=0;I<3;++I)if(C==Coord(I))return I;return INDEX_NONE;}
FTransform Frame(const EW::WorldDescriptor& W,int32 I)
{
    const double H=EWOuterWater::Ground(W,Coord(I));
    return I==2?FTransform(FRotator(0,180,0),FVector(6400,6400,H+32000)):
        FTransform(FRotator(0,90,0),FVector(2700,10800,H+(I==0?900:19000)));
}
FString Name(int32 I){return I==0?TEXT("円環商店街"):I==1?TEXT("雲上の回廊院"):TEXT("空に眠る城");}
FString Directions(int32 I)
{
    const TCHAR* D[]={TEXT("水都駅の昇降機で広場へ。北側の滝見の回廊を上がり、左のガラス塔へ。"),
        TEXT("円環商店街のある広場から橋を東へ一つ渡る。北の滝見の回廊にある昇降機で「雲上の回廊院」へ。"),
        TEXT("回廊院の広場から東へ一つ、さらに北へ一つ橋を渡る。滝見の回廊の奥の昇降機で「空に眠る城」へ。")};
    return EWL::Translate(D[FMath::Clamp(I,0,2)]);
}
namespace
{
void Part(EW::ChunkRecipe& R,FName Name,const FTransform& F){R.Parts.Add({Name,F,0});}
void Bridge(EW::ChunkRecipe& R,FVector A,FVector B,double Width=600)
{
    const FVector D=B-A;const double Length=D.Size();if(Length<1)return;
    const FQuat Q=FRotationMatrix::MakeFromXZ(D.GetSafeNormal(),FVector::UpVector).ToQuat();
    R.Paths.Add({A,B,Width});R.Colliders.Add({FTransform(Q,(A+B)*.5-FVector(0,0,25)),FVector(Length*.5,Width*.5,25),true});
    const int N=FMath::Max(1,FMath::CeilToInt(Length/2400.));
    for(int J=0;J<N;++J)
    {
        const auto P=FMath::Lerp(A,B,(J+.5)/N);
        Part(R,TEXT("Wall"),FTransform(Q,P-FVector(0,0,25),FVector(Length/N/100.,Width/100.,.5)));
        const double GuardLength=FMath::Max(0.,Length/N-200);
        for(double S:{-1.,1.})if(GuardLength>0)
        {
            const auto Centre=P+Q.GetRightVector()*Width*.48*S;
            R.Colliders.Add({FTransform(Q,Centre+FVector(0,0,60)),FVector(GuardLength*.5,10,60),false});
            for(double Z:{22.,112.})Part(R,TEXT("Wall"),FTransform(Q,Centre+FVector(0,0,Z),FVector(GuardLength/100.,.12,.12)));
            const int Posts=FMath::Max(1,FMath::CeilToInt(GuardLength/100.));
            for(int K=0;K<=Posts;++K)Part(R,TEXT("Wall"),FTransform(Q,Centre+Q.GetForwardVector()*GuardLength*(double(K)/Posts-.5)+FVector(0,0,56),FVector(.09,.09,1.12)));
        }
    }
}
template<size_t N> void Model(EW::ChunkRecipe& R,FName Name,const FTransform& F,const EWSky92Data::Shape (&Shapes)[N])
{
    Part(R,Name,F);
    for(const auto& B:Shapes)R.Colliders.Add({FTransform(F.GetRotation()*FRotator(0,B.Yaw,0).Quaternion(),F.TransformPosition(B.P)),B.Half,B.Floor});
}
void Lift(EW::ChunkRecipe& R,int I,FVector Cabin,double Bottom,double Top,FRotator Q)
{
    EW::LiftSpec L;L.Id=TEXT("sky92/")+R.Coord.Text()+TEXT("/lift");L.Cabin=Cabin;L.Rotation=Q;L.Outward=Q.RotateVector(FVector(0,1,0));
    for(int Floor=0;Floor<2;++Floor)
    {
        EW::LiftStop S;S.Height=Floor?Top:Bottom;S.Floor=Floor;S.Label=Floor?Name(I):TEXT("水都・滝見の回廊");S.BoardingRotation=Q;
        const auto P=FVector(Cabin.X,Cabin.Y,S.Height);S.Landing=P+L.Outward*320;S.Entry=S.Landing;S.Threshold=P+L.Outward*190;L.Stops.Add(S);
    }
    for(double Side:{-1.,1.})Part(R,TEXT("Wall"),FTransform(FQuat::Identity,FVector(Cabin.X+220,Cabin.Y+Side*205,(Bottom+Top+330)*.5)));
    // Slender guide rails only; the landmark itself retains its floating silhouette.
    for(int J=R.Parts.Num()-2;J<R.Parts.Num();++J)R.Parts[J].Transform.SetScale3D(FVector(.09,.09,(Top+330-Bottom)/100.));
    R.Lifts.Add(L);
}
}
void Build(EW::ChunkRecipe& R)
{
    if(!R.bCascadeCity)return;const int I=Landmark(R.Coord);const double H=R.Hub.Z;
    if(I==0)
    {
        const auto F=Frame(R.World,I);Model(R,TEXT("Sky92Market"),F,EWSky92Data::Sky92Market);
        Bridge(R,FVector(4450,10800,H+900),FVector(4800,10800,H+900));
    }
    else if(I==1)
    {
        const auto F=Frame(R.World,I);Model(R,TEXT("Sky92Cloister"),F,EWSky92Data::Sky92Cloister);
        Lift(R,I,FVector(5300,10800,0),H+900,H+19000,FRotator(0,90,0));
        Bridge(R,FVector(4400,10800,H+19000),FVector(5110,10800,H+19000));
    }
    else if(I==2)
    {
        const auto F=Frame(R.World,I);Model(R,TEXT("Sky92Castle"),F,EWSky92Data::Sky92Castle);
        Lift(R,I,FVector(6400,12300,0),H+900,H+32000,FRotator(0,180,0));
        Bridge(R,FVector(6400,12000,H+900),FVector(6400,12110,H+900),400);
        Bridge(R,FVector(6400,10150,H+32000),FVector(6400,12110,H+32000));
    }
    if(I!=INDEX_NONE)
    {
        EW::PlaceBookmark P;P.WorldCode=R.World.Code();P.Coord=R.Coord;P.Kind=17+I;P.Name=Name(I);
        P.Id=TEXT("sky92/")+R.Coord.Text()+TEXT("/landmark");
        P.LocalPosition=Frame(R.World,I).TransformPosition(I==2?FVector(0,-3200,100):FVector(0,-1400,100));
        P.Yaw=I==2?-90:180;R.Places.Add(P);
    }
    // A varied secondary skyline replaces selected repeated corner palaces.
    if(I!=0 && I!=1 && (R.Coord.X+R.Coord.Y)%2==0)
        Part(R,TEXT("Sky92ConePavilion"),FTransform(FRotator(0,35,0),FVector(1300,11300,H),FVector(.85,.85,1.45)));
}
TArray<FVector> Lamps(const EW::ChunkRecipe& R)
{
    TArray<FVector> Out;int I=Landmark(R.Coord);if(I==INDEX_NONE)return Out;const auto F=Frame(R.World,I);
    if(I==0){for(FVector P:{FVector(-650,-1520,0),FVector(650,-1520,0),FVector(-1500,400,0),FVector(1500,400,0)})Out.Add(F.TransformPosition(P));}
    if(I==1){for(FVector P:{FVector(-600,-800,0),FVector(600,-800,0),FVector(-700,600,0),FVector(700,600,0)})Out.Add(F.TransformPosition(P));}
    if(I==2){for(FVector P:{FVector(-750,-2800,0),FVector(750,-2800,0),FVector(-1400,-1100,0),FVector(1400,-1100,0),FVector(-600,600,0),FVector(600,600,0)})Out.Add(F.TransformPosition(P));}
    return Out;
}
void Clouds(USceneComponent* Parent,const EW::WorldDescriptor& W,EW::ChunkCoord C)
{
    int I=Landmark(C);if(!Parent || I<1)return;const auto F=Frame(W,I);
    // Local participating media lit by the real sky/sun; no emissive cloud shell.
    for(int J=0;J<7;++J)
    {
        const double A=J*2*PI/7,R=I==1?1500:3100,Radius=I==1?1300:1900;
        auto* Fog=NewObject<ULocalFogVolumeComponent>(Parent->GetOwner());Fog->SetupAttachment(Parent);Fog->SetMobility(EComponentMobility::Movable);
        Fog->SetRelativeLocation(F.TransformPosition(FVector(FMath::Cos(A)*R,FMath::Sin(A)*R,-Radius*.65-200)));
        // UE combines radial and height coverage multiplicatively. Zero height
        // extinction also removes radial fog; keep both channels nonzero.
        Fog->SetRelativeScale3D(FVector(Radius/500.));Fog->RadialFogExtinction=.8f;
        Fog->HeightFogExtinction=.12f;Fog->HeightFogFalloff=100.f;
        Fog->FogAlbedo=FLinearColor(.82f,.88f,.91f,1);Fog->FogPhaseG=.15f;Fog->RegisterComponent();
    }
}
}
