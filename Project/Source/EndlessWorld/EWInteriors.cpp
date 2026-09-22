#include "EWInteriors.h"
#include "EWInteriorPlan.h"
#include "EWHotelPlan.h"
#include "EWExplorationPlan.h"
#include "EWExplore85Data.h"
#include "EWHotel83Data.h"
#include "EWCinemaPlan.h"
#include "EWDayCycle.h"
#include "EWChunkManager.h"
#include "EWGameInstance.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Dom/JsonObject.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"

namespace
{
void Box(EW::ChunkRecipe& R,const FTransform& T,FVector Lo,FVector Hi,bool Floor=false)
{
    if((Hi-Lo).GetMin()>.01)R.Colliders.Add({FTransform(T.GetRotation(),T.TransformPosition((Lo+Hi)*.5)),
        (Hi-Lo)*.5*T.GetScale3D().GetAbs(),Floor});
}
FTransform Side(const FTransform& T,int32 Index)
{
    const auto S=T.GetScale3D();
    return FTransform(T.GetRotation()*FRotator(0,Index*90.,0).Quaternion(),T.GetLocation(),Index%2?FVector(S.Y,S.X,S.Z):S);
}
void Furniture(EW::ChunkRecipe& R,const EW::InteriorRoom& Room)
{
    for(const auto& B:EWInteriorPlan::Furniture(Room.Kind))Box(R,Room.Frame,B.Centre-B.Extent,B.Centre+B.Extent);
}
void Apartment(EW::ChunkRecipe& R,const FTransform& T,int32 Kind)
{
    static const TCHAR* Names[]={TEXT("InteriorLiving"),TEXT("InteriorBedroom"),TEXT("InteriorStudy"),TEXT("InteriorDining"),
        TEXT("InteriorLibrary"),TEXT("InteriorRain"),TEXT("InteriorCafe"),TEXT("InteriorBotanical"),TEXT("InteriorAtelier"),TEXT("InteriorMusic")};
    const FName Shell=Kind==1 || Kind==9?TEXT("InteriorShellRose"):Kind==7?TEXT("InteriorShellSage"):
        Kind==2?TEXT("InteriorShellBlue"):TEXT("InteriorShell");
    R.Interiors.Add({T,Kind,0});R.Parts.Add({Shell,T,2});R.Parts.Add({FName(Names[Kind]),T,2});
    R.Parts.Add({TEXT("InteriorEntry"),T,2});
    Box(R,T,FVector(0,-1800,0),FVector(12,-1200,340));
    Box(R,T,FVector(888,-1800,0),FVector(900,-1200,340));
    Box(R,T,FVector(12,-1212,0),FVector(888,-1200,340));
    for(const FVector2D Range:{FVector2D(0,100),FVector2D(350,550),FVector2D(800,900)})
        Box(R,T,FVector(Range.X,-1820,0),FVector(Range.Y,-1660,340));
    Box(R,T,FVector(100,-1820,285),FVector(350,-1660,340));
    Box(R,T,FVector(550,-1820,0),FVector(800,-1660,55));
    Box(R,T,FVector(550,-1820,315),FVector(800,-1660,340));
    Box(R,T,FVector(550,-1822,55),FVector(800,-1798,111));
    Furniture(R,R.Interiors.Last());
}
void Storey(EW::ChunkRecipe& R,const FTransform& T,int32 Variant,int32 Level)
{
    // Subtract four rooms from one shared solid floor plan. Adding four copies
    // of the old solid building would fill the adjacent rooms back in.
    struct Rect{double X0,Y0,X1,Y1;};
    TArray<Rect> Solid;Solid.Add({-1800,-1800,1800,1800});
    const Rect Rooms[]={{0,-1800,900,-1200},{1200,0,1800,900},{-900,1200,0,1800},{-1800,-900,-1200,0}};
    for(const auto V:Rooms)
    {
        TArray<Rect> Next;
        for(const auto A:Solid)
        {
            const double X0=FMath::Max(A.X0,V.X0),X1=FMath::Min(A.X1,V.X1);
            const double Y0=FMath::Max(A.Y0,V.Y0),Y1=FMath::Min(A.Y1,V.Y1);
            if(X0>=X1 || Y0>=Y1){Next.Add(A);continue;}
            if(A.X0<X0)Next.Add({A.X0,A.Y0,X0,A.Y1});
            if(X1<A.X1)Next.Add({X1,A.Y0,A.X1,A.Y1});
            if(A.Y0<Y0)Next.Add({X0,A.Y0,X1,Y0});
            if(Y1<A.Y1)Next.Add({X0,Y1,X1,A.Y1});
        }
        Solid=MoveTemp(Next);
    }
    Box(R,T,FVector(-1800,-1800,-24),FVector(1800,1800,0),true);
    Box(R,T,FVector(-1800,-1800,340),FVector(1800,1800,1776));
    for(const auto A:Solid)Box(R,T,FVector(A.X0,A.Y0,0),FVector(A.X1,A.Y1,340));
    static constexpr int32 Kinds[]={0,1,2,3,6,7,8,9};
    for(int32 S=0;S<4;++S)Apartment(R,Side(T,S),Kinds[(Variant+S*2+Level*3)%8]);
}
}
namespace EWInteriors
{
bool AddBlock(EW::ChunkRecipe& R,const EW::Part& P)
{
    if(EWExplorationPlan::AddBlock(R,P))return true;
    const FString Name=P.Mesh.ToString();
    const bool Library=Name.StartsWith(TEXT("UrbanLibrary_"));
    if(!Library && (!Name.StartsWith(TEXT("UrbanBlock_")) || Name==TEXT("UrbanBlock_RainWindow")))return false;
    const int32 Variant=FCString::Atoi(*Name.Right(1));
    if(!Library)Storey(R,P.Transform,Variant,0);
    else
    {
        const bool Cinema=R.World.Seed==EW::WorldDescriptor::ReferenceWorld().Seed && R.Coord==EW::ChunkCoord{0,0}
            && P.Transform.GetLocation().X>6400 && P.Transform.GetLocation().Y>6400
            && FMath::Abs(P.Transform.GetLocation().Z-R.Hub.Z)<1;
        const auto RoomFrame=Cinema?Side(P.Transform,2):P.Transform;
        R.Interiors.Add({RoomFrame,Cinema?10:4,Variant});
        if(Cinema)
        {
            for(const FName Mesh:{FName(TEXT("CinemaShell")),FName(TEXT("CinemaFurniture")),FName(TEXT("CinemaDetails"))})
                R.Parts.Add({Mesh,RoomFrame,2});
            for(const auto& B:EWCinemaPlan::Boxes())Box(R,RoomFrame,B.Centre-B.Extent,B.Centre+B.Extent,B.Floor);
        }
        else R.Parts.Add({TEXT("InteriorLibrary"),P.Transform,2});
        Box(R,P.Transform,FVector(-1800,-1800,-24),FVector(1800,1800,0),true);
        // Each source facade's one open arch maps through FBX's Y reflection.
        for(int32 SourceSide=0;SourceSide<4;++SourceSide)
        {
            const int32 Bay=(3-((Variant+SourceSide)*17)%3)%3;
            const double X=1150.-Bay*1150.;const auto T=Side(P.Transform,(2-SourceSide+4)%4);
            Box(R,T,FVector(-1800,-1810,0),FVector(X-400,-1610,1800));
            Box(R,T,FVector(X+400,-1810,0),FVector(1800,-1610,1800));
            Box(R,T,FVector(X-400,-1810,950),FVector(X+400,-1610,1800));
        }
        if(!Cinema)Furniture(R,R.Interiors.Last());
    }
    auto Upper=P.Transform;Upper.AddToTranslation(FVector(0,0,1800));
    Storey(R,Upper,Variant,1);return true;
}
void AddRain(EW::ChunkRecipe& R,const EW::ResidenceSpec& S)
{
    R.Interiors.Add({S.Frame,5,0});R.Parts.Add({TEXT("InteriorRain"),S.Frame,2});Furniture(R,R.Interiors.Last());
}
FString Name(int32 Kind)
{
    if(EWExplorationPlan::IsPublic(Kind))return EWExplorationPlan::Name(Kind-19);
    if(EWHotelPlan::IsSuite(Kind))return EWHotelPlan::Name(Kind-11);
    static const TCHAR* Names[]={TEXT("居間"),TEXT("寝室"),TEXT("書斎"),TEXT("食堂"),TEXT("図書館"),TEXT("雨継ぎの窓辺"),
        TEXT("水辺の喫茶室"),TEXT("植物の温室"),TEXT("画家のアトリエ"),TEXT("音楽室"),TEXT("水鏡の映写室")};
    return Kind>=0 && Kind<11?Names[Kind]:TEXT("部屋");
}
FString NearbyText(const EW::ChunkRecipe& R,const FVector& Position)
{
    for(const auto& Room:R.Interiors)
    {
        if(FMath::Abs(Position.Z-Room.Frame.GetLocation().Z-90)>130)continue;
        const auto P=Room.Frame.InverseTransformPosition(Position);
        if(EWHotelPlan::IsSuite(Room.Kind))
        {if(FMath::Abs(P.X)<755 && P.Y>-805 && P.Y<1010)return Name(Room.Kind)+TEXT("　雲を望む上層の客室");continue;}
        if(Room.Kind==10)
        {if(FMath::Abs(P.X)<1550 && P.Y>-2150 && P.Y<1550)return Name(10)+TEXT("　32席の小さな映画館");continue;}
        if(Room.Kind==4)
        {if(FMath::Abs(P.X)<1650 && FMath::Abs(P.Y)<1650)return Name(4)+TEXT("　空の見える閲覧室");continue;}
        const bool Inside=P.X>30 && P.X<870 && P.Y>-1760 && P.Y<-1220;
        const bool Entrance=P.X>75 && P.X<375 && P.Y>-2320 && P.Y<=-1760;
        if(Inside || Entrance)return Name(Room.Kind)+(Entrance?TEXT("　このまま歩いて入れます"):TEXT("　回廊へ自由に出入りできます"));
    }
    return {};
}
TArray<FVector> Route(const EW::InteriorRoom& Room)
{
    TArray<FVector> Points;
    if(EWExplorationPlan::IsPublic(Room.Kind))Points=EWExplore85Data::Route(Room.Kind);
    else if(EWHotelPlan::IsSuite(Room.Kind))Points=EWHotelPlan::Route();
    else if(Room.Kind==10)Points=EWCinemaPlan::Route();
    else if(Room.Kind==4)
    {
        const int32 SourceSide=2, Bay=(3-((Room.Variant+SourceSide)*17)%3)%3;
        const double X=1150.-Bay*1150.;
        Points={FVector(X,-2100,0),FVector(X,-1450,0),FVector(0,-1450,0),FVector(0,0,0)};
    }
    else if(Room.Kind==5)Points={FVector(225,-2100,0),FVector(225,-1450,0),FVector(450,-1450,0),FVector(675,-1600,0)};
    else if(Room.Kind==6)Points={FVector(225,-2100,0),FVector(225,-1520,0),FVector(450,-1520,0),FVector(525,-1470,0)};
    else if(Room.Kind==7)Points={FVector(225,-2100,0),FVector(225,-1585,0),FVector(675,-1585,0)};
    else Points={FVector(225,-2100,0),FVector(225,-1520,0),FVector(450,-1550,0),FVector(675,-1600,0)};
    for(auto& P:Points)P=Room.Frame.TransformPosition(P);
    return Points;
}
}

AEWInteriorLighting::AEWInteriorLighting()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickInterval=.15f;
    SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("InteriorLightRoot")));
    for(int32 I=0;I<8;++I)
    {
        auto* L=CreateDefaultSubobject<UPointLightComponent>(*FString::Printf(TEXT("RoomLight_%d"),I));
        L->SetupAttachment(RootComponent);L->SetMobility(EComponentMobility::Movable);
        L->SetIntensityUnits(ELightUnits::Lumens);L->SetLightColor(FLinearColor(1,.80,.56));
        L->SetSourceRadius(22);L->SetSoftSourceRadius(35);L->SetCastShadows(true);
        L->SetVolumetricScatteringIntensity(0);L->SetVisibility(false);Lights.Add(L);
    }
    // Downward light follows the cafe and library's visible pendant shades.
    // These replace selected point slots; the active budget stays at eight.
    for(int32 I=0;I<8;++I)
    {
        auto* L=CreateDefaultSubobject<USpotLightComponent>(*FString::Printf(TEXT("PublicPendant_%d"),I));
        L->SetupAttachment(RootComponent);L->SetMobility(EComponentMobility::Movable);
        L->SetIntensityUnits(ELightUnits::Lumens);L->SetInnerConeAngle(42);L->SetOuterConeAngle(68);
        L->SetSourceRadius(12);L->SetSoftSourceRadius(18);L->SetCastShadows(true);
        L->SetIndirectLightingIntensity(.65f);L->SetVolumetricScatteringIntensity(0);
        L->SetIntensity(0);L->SetVisibility(false);PendantLights.Add(L);
    }
}
void AEWInteriorLighting::Tick(float Delta)
{
    Super::Tick(Delta);const auto* G=GetGameInstance<UEWGameInstance>();const auto* M=G?G->Manager.Get():nullptr;
    const auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0);
    if(!M || !Camera)return;
    struct Candidate{FVector Position;float Lumens,Radius;double Distance;FLinearColor Colour;bool Street=false,Pendant=false;FRotator Rotation=FRotator::ZeroRotator;int32 RoomKind=-1;};TArray<Candidate> Candidates;
    const FVector Eye=Camera->GetCameraLocation();const auto Centre=M->PlayerCoord();
    for(int32 Y=-1;Y<=1;++Y)for(int32 X=-1;X<=1;++X)
    {
        const auto Coord=Centre+EW::ChunkCoord{X,Y};const auto R=M->RecipeAt(Coord);if(!R)continue;
        // Reuse the same eight-light budget outside. Each light belongs to an
        // authored lantern, and is shadowed so it cannot brighten closed rooms.
        if(G->DayCycle && G->DayCycle->StreetLightScale()>.0001f)
            for(const auto& Part:R->Parts)if(Part.Mesh==TEXT("Lamp"))
            {
                const FVector Pos=M->ToRender(Coord,Part.Transform.TransformPosition(FVector(0,0,376)));
                const double Distance=FVector::DistSquared(Pos,Eye);
                if(Distance<25000000. && FMath::Abs(Pos.Z-Eye.Z)<1000)
                    Candidates.Add({Pos,350000,1400,Distance,FLinearColor(1.f,.67f,.34f),true});
            }
        for(const auto& Room:R->Interiors)
        {
            const auto Local=Room.Frame.InverseTransformPosition(M->ToLocal(Coord,Eye));
            if(EWExplorationPlan::IsPublic(Room.Kind))
            {
                if(Local.Z<-200 || Local.Z>1800 || FMath::Abs(Local.X)>2350 || FMath::Abs(Local.Y)>2350)continue;
                for(const FVector L:EWExplore85Data::Lights(Room.Kind))
                {
                    const bool Cafe=Room.Kind==19;
                    const bool Pendant=Cafe || Room.Kind==21;
                    // Generated emitters are 65 cm below each shade. Move the
                    // smaller cafe source to 14 cm below its luminous disc.
                    const auto Pos=M->ToRender(Coord,Room.Frame.TransformPosition(L+FVector(0,0,Pendant?50:0)));
                    // Convert the old omni flux by the spot's solid angle.
                    // Keeping full omni lumens here would triple axial light.
                    const float Flux=Pendant?1300000.f*(1.f-FMath::Cos(FMath::DegreesToRadians(68.f)))*.5f*(Cafe?1.45f:1.30f):1300000.f;
                    const auto Colour=Cafe?FLinearColor(1,.78,.52):Room.Kind==24?FLinearColor(.83,1,.90):FLinearColor(1,.82,.60);
                    const auto Aim=Room.Frame.TransformVectorNoScale(FVector(0,0,-1)).Rotation();
                    Candidates.Add({Pos,Flux,2300,FVector::DistSquared(Pos,Eye),Colour,false,Pendant,Aim,Room.Kind});
                }
            }
            else if(EWHotelPlan::IsSuite(Room.Kind))
            {
                if(Local.Z<-120 || Local.Z>520 || FMath::Abs(Local.X)>1000 || Local.Y<-1000 || Local.Y>1150)continue;
                for(const FVector L:EWHotel83Data::Lights(Room.Kind))
                {
                    const auto Pos=M->ToRender(Coord,Room.Frame.TransformPosition(L));
                    const auto Colour=Room.Kind==14?FLinearColor(1,.67,.39):FLinearColor(1,.85,.67);
                    Candidates.Add({Pos,350000,1050,FVector::DistSquared(Pos,Eye),Colour});
                }
            }
            else if(Room.Kind==4)
            {
                if(FMath::Abs(Local.Z)>1650 || FMath::Abs(Local.X)>2200 || FMath::Abs(Local.Y)>2200)continue;
                for(double LX:{-1200.,0.,1200.})for(double LY:{-1200.,0.,1200.})
                {
                    auto Pos=M->ToRender(Coord,Room.Frame.TransformPosition(FVector(LX,LY,725)));
                    Candidates.Add({Pos,650000,1650,FVector::DistSquared(Pos,Eye),FLinearColor(1,.86,.68)});
                }
            }
            else if(Room.Kind!=5 && Room.Kind!=10 && Local.Z>-350 && Local.Z<680 && Local.X>-500 && Local.X<1400 && Local.Y>-2600 && Local.Y<-900)
                for(double LX:{220.,660.})
                {
                    auto Pos=M->ToRender(Coord,Room.Frame.TransformPosition(FVector(LX,-1460,265)));
                    const auto Colour=Room.Kind==7?FLinearColor(.90,1,.78):Room.Kind==9?FLinearColor(1,.69,.43):FLinearColor(1,.84,.63);
                    Candidates.Add({Pos,150000,780,FVector::DistSquared(Pos,Eye),Colour});
                }
        }
    }
    Candidates.Sort([](const Candidate& A,const Candidate& B){return A.Distance<B.Distance;});
    const int32 Selected=FMath::Min(Lights.Num(),Candidates.Num());
    int32 PointIndex=0,PendantIndex=0;ActiveCafePendants=0;
    for(int32 I=0;I<Selected;++I)
    {
        const auto& C=Candidates[I];UPointLightComponent* L=nullptr;
        if(C.Pendant)
        {
            if(PendantIndex>=PendantLights.Num())continue;
            L=PendantLights[PendantIndex++].Get();L->SetWorldRotation(C.Rotation);
            if(C.RoomKind==19)++ActiveCafePendants;
        }
        else L=Lights[PointIndex++].Get();
        L->SetVisibility(true);L->SetWorldLocation(C.Position);
        const float Scale=G->DayCycle?(C.Street?G->DayCycle->StreetLightScale():G->DayCycle->InteriorLightScale()):1.f;
        L->SetIntensity(C.Lumens*Scale);L->SetAttenuationRadius(C.Radius);L->SetLightColor(C.Colour);
    }
    for(int32 I=PointIndex;I<Lights.Num();++I){Lights[I]->SetVisibility(false);Lights[I]->SetIntensity(0);}
    for(int32 I=PendantIndex;I<PendantLights.Num();++I){PendantLights[I]->SetVisibility(false);PendantLights[I]->SetIntensity(0);}
}
TSharedRef<FJsonObject> AEWInteriorLighting::Evidence() const
{
    auto O=MakeShared<FJsonObject>();int32 Active=0,PendantActive=0;bool Shadows=true;
    for(const auto& L:Lights)if(L && L->IsVisible()){++Active;Shadows&=L->CastShadows;}
    TArray<TSharedPtr<FJsonValue>> Pendants;
    for(const auto& L:PendantLights)if(L && L->IsVisible())
    {
        ++Active;++PendantActive;Shadows&=L->CastShadows;
        auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("position"),L->GetComponentLocation().ToString());
        P->SetStringField(TEXT("direction"),L->GetForwardVector().ToString());P->SetNumberField(TEXT("lumens"),L->Intensity);
        P->SetNumberField(TEXT("source_radius_cm"),L->SourceRadius);P->SetNumberField(TEXT("outer_cone_degrees"),L->OuterConeAngle);
        Pendants.Add(MakeShared<FJsonValueObject>(P));
    }
    O->SetNumberField(TEXT("active_lights"),Active);O->SetNumberField(TEXT("active_cafe_spots"),ActiveCafePendants);
    O->SetNumberField(TEXT("active_public_pendant_spots"),PendantActive);
    O->SetNumberField(TEXT("active_light_budget"),Lights.Num());O->SetBoolField(TEXT("within_active_budget"),Active<=Lights.Num());
    O->SetBoolField(TEXT("all_cast_shadows"),Shadows);O->SetArrayField(TEXT("public_pendants"),Pendants);return O;
}
