#include "EWOuterWater.h"
#include "EWSky92Plan.h"

namespace EWOuterWater
{
bool Contains(const EW::WorldDescriptor& W,EW::ChunkCoord C)
{return W.Seed==EW::WorldDescriptor::ReferenceWorld().Seed && C.X>=3 && C.X<=6 && C.Y>=-1 && C.Y<=2;}
double Ground(const EW::WorldDescriptor& W,EW::ChunkCoord C)
{
    const double D=FMath::FloorToDouble(EW::HeightAt(W,0,0)/450.)*450.;
    return D+FMath::RoundToDouble((EW::HeightAt(W,6400,6400)-D)/1800.)*1800.-(C.X-3)*450.;
}
double WaterHeight(const EW::WorldDescriptor& W,EW::ChunkCoord C){return Ground(W,C)-600;}
namespace
{
void Part(EW::ChunkRecipe& R,const TCHAR* Name,FVector P,FVector Scale=FVector::OneVector,double Yaw=0,uint8 Detail=0)
{R.Parts.Add({FName(Name),FTransform(FRotator(0,Yaw,0),P,Scale),Detail});}
void Box(EW::ChunkRecipe& R,FVector Centre,FVector Half,bool Floor=false,FQuat Q=FQuat::Identity)
{R.Colliders.Add({FTransform(Q,Centre),Half,Floor});}
void Portals(EW::ChunkRecipe& R)
{
    if(!R.Portals.IsEmpty())return;
    for(int32 D=0;D<4;++D)
    {
        const int32 Axis=D<2?0:1;auto C=R.Coord;if(D==1)++C.X;if(D==3)++C.Y;
        const double Offset=4800+R.World.Number("portal-offset",{Axis,C.X,C.Y})%3201;
        FVector P=Axis==0?FVector(D==0?0:EW::ChunkSize,Offset,0):FVector(Offset,D==2?0:EW::ChunkSize,0);
        P.Z=EW::HeightAt(R.World,R.Coord.X*EW::ChunkSize+int64(P.X),R.Coord.Y*EW::ChunkSize+int64(P.Y));
        R.Portals.Add({Axis,C,P,double(550+R.World.Number("portal-width",{Axis,C.X,C.Y})%151)});
    }
}
// Paths and collision share the same exact floor plane. The stone skin is 2 cm
// below that plane to keep a bridge/plaza junction free from coplanar flicker.
void Road(EW::ChunkRecipe& R,FVector A,FVector B,double Width=700,bool Draw=true,bool Rails=true)
{
    if(FVector::Dist2D(A,B)<1)return;
    R.Paths.Add({A,B,Width});if(!Draw)return;
    const FVector D=B-A;const double L=D.Size();const FQuat Q=FRotationMatrix::MakeFromXZ(D.GetSafeNormal(),FVector::UpVector).ToQuat();
    Box(R,(A+B)*.5-Q.GetUpVector()*25,FVector(L*.5+2,Width*.5,25),true,Q);
    const int32 N=FMath::Max(1,FMath::CeilToInt(L/2400.));
    for(int32 I=0;I<N;++I)
    {
        const auto C=FMath::Lerp(A,B,(I+.5)/N);
        if(Rails)R.Parts.Add({TEXT("Cascade86Bridge"),FTransform(Q,C,FVector(L/N/2400.,Width/800.,1)),0});
        else R.Parts.Add({TEXT("Wall"),FTransform(Q,C-Q.GetUpVector()*25,FVector(L/N/100.,Width/100.,.5)),0});
        if(Rails)
        {
            const double RailLength=L/N*20./24.;
            for(double S:{-1.,1.})Box(R,C+Q.GetRightVector()*S*Width*.48125+Q.GetUpVector()*60,FVector(RailLength*.5,12,60),false,Q);
        }
    }
}
struct Approach {FVector Inside,Branch,Apron;};
Approach Access(const EW::ChunkRecipe& R,int32 Side)
{
    const auto P=R.Portals[Side].Position;const auto H=R.Hub;
    FVector I=P;if(Side<2)I.X+=Side==0?900:-900;else I.Y+=Side==2?900:-900;
    const FVector Along=(I-H).GetSafeNormal2D();
    FVector A=H+Along*(1600./FMath::Max(FMath::Abs(Along.X),FMath::Abs(Along.Y)));A.Z=H.Z;
    I.Z=FMath::Lerp(P.Z,H.Z,900./(900.+FVector::Dist2D(I,A)));
    const FVector Branch=FMath::Lerp(I,A,.50);
    return {I,Branch,A};
}
void Marker(EW::ChunkRecipe& R,int32 Kind,FVector P,const FString& Id)
{
    EW::PlaceBookmark B;B.WorldCode=R.World.Code();B.Coord=R.Coord;B.Kind=Kind;B.Id=Id;
    B.Name=Name(Kind);B.LocalPosition=P+FVector(0,0,100);B.Yaw=0;R.Places.Add(B);
    Part(R,TEXT("MooringMast"),P+FVector(-440,350,0),FVector(.13,.13,.10),0,2);
}
}
TArray<FVector> UpperRoute(const EW::ChunkRecipe& R)
{
    const auto West=Access(R,0).Branch,East=Access(R,1).Branch;const double Z=R.Hub.Z+900;
    return {West,FVector((EWSky92Plan::Landmark(R.Coord)==0 || EWSky92Plan::Landmark(R.Coord)==1)?4800:West.X,10800,Z),FVector(5200,10800,Z),FVector(6400,10800,Z),FVector(7600,10800,Z),FVector(East.X,10800,Z),East};
}
void Build(EW::ChunkRecipe& R)
{
    if(!Contains(R.World,R.Coord))return;
    R.bCascadeCity=true;R.Hub=FVector(6400,6400,Ground(R.World,R.Coord));Portals(R);
    const double H=R.Hub.Z,W=H-600;
    Part(R,TEXT("Cascade86Water"),FVector(0,0,W));
    Part(R,TEXT("Cascade86Square"),FVector(6400,6400,W-900),FVector(4,4,1));
    Part(R,TEXT("Cascade86Square"),R.Hub);
    Box(R,R.Hub-FVector(0,0,50),FVector(1600,1600,50),true);
    for(double X:{-1200.,1200.})for(double Y:{-1200.,1200.})
        Part(R,TEXT("Wall"),R.Hub+FVector(X,Y,-790),FVector(1.8,1.8,14.2));
    for(int32 Side=0;Side<4;++Side)
    {
        const auto A=Access(R,Side);const auto P=R.Portals[Side];
        Road(R,P.Position,A.Inside,P.Width);Road(R,A.Inside,A.Branch,P.Width);
        Road(R,A.Branch,A.Apron,P.Width);Road(R,A.Apron,R.Hub,P.Width,false);
    }
    const auto U=UpperRoute(R);
    // Finish climbing at the near edge of the transverse deck, not its centre.
    // Otherwise the last 3 m of slope runs underneath its vertical slab face.
    const FVector WestLip(U[1].X,10500,H+900),EastLip(U[5].X,10500,H+900);
    Road(R,U[0],WestLip,600);Road(R,WestLip,U[1],600,false);
    const bool NewWestLanding=EWSky92Plan::Landmark(R.Coord)==0 || EWSky92Plan::Landmark(R.Coord)==1;
    Road(R,U[1],FVector(5000,10800,H+900),600,true,!NewWestLanding);
    Road(R,FVector(7800,10800,H+900),U[5],600);
    Road(R,U[5],EastLip,600,false);Road(R,EastLip,U[6],600);
    Part(R,TEXT("Cascade86Belvedere"),FVector(6400,10800,H+900));
    Box(R,FVector(6400,10800,H+868),FVector(1400,1200,32),true);
    // The square's rear and front balustrades leave the two central views open.
    for(double S:{-1.,1.})
    {
        // Side approaches meet the terrace at its exact edge. Keep those sides
        // open at y=10800 and only protect the remainder of the outer rim.
        for(double T:{-1.,1.})Box(R,FVector(6400+S*1360,10800+T*770,H+960),FVector(14,370,60));
        for(double X:{-900.,900.})Box(R,FVector(6400+X,10800+S*1160,H+960),FVector(400,14,60));
    }
    // Sanctuary can be walked through along both main axes.
    Part(R,TEXT("Cascade86Sanctum"),R.Hub);
    for(double X:{-700.,700.})for(double Y:{-700.,700.})Box(R,R.Hub+FVector(X,Y,400),FVector(74,74,400));
    Box(R,R.Hub+FVector(0,0,1480),FVector(1000,1000,180));
    const FVector Towers[]={FVector(2300,2300,H),FVector(10300,2300,H),FVector(1000,11200,H),FVector(11800,11200,H)};
    const uint64 Seed=R.World.Number("cascade86-towers",{R.Coord.X,R.Coord.Y});
    for(int32 I=0;I<4;++I)
    {
        const int L=EWSky92Plan::Landmark(R.Coord);
        if(I==2 && (L==0 || L==1 || (R.Coord.X+R.Coord.Y)%2==0))continue;
        const int32 Type=int32((Seed>>(I*7))%3);const double Size=I<2?.77:.48;
        const double Vertical=.78+double((Seed>>(I*5))%5)*.075;
        Part(R,*FString::Printf(TEXT("Cascade88Palace%d"),Type),Towers[I],FVector(Size,Size,Vertical),I*90);
        Part(R,*FString::Printf(TEXT("Cascade88Glass%d"),Type),Towers[I],FVector(Size,Size,Vertical),I*90);
        // The stepped tower starts above the water. Its masonry footing reaches
        // the basin bed, so neither the tower nor its cistern floats over a gap.
        Part(R,TEXT("Wall"),Towers[I]-FVector(0,0,750),FVector(24*Size,24*Size,15));
        Box(R,Towers[I]+FVector(0,0,9000*Vertical),FVector(1200*Size,1200*Size,9000*Vertical));
        Part(R,TEXT("Cascade86Garden"),Towers[I]+FVector(0,0,700),FVector(1.3),I*71,1);
    }
    for(const auto& F:Falls(R.World,R.Coord))
    {
        const double Top=F.Top;const FVector Basin((F.X0+F.X1)*.5,F.StartY-880,Top);
        Part(R,TEXT("Cascade88Cistern"),Basin);
        Part(R,TEXT("Cascade86SourceWater"),Basin+FVector(-1070,-870,8));
        Part(R,F.Top-F.Bottom>4000?TEXT("Cascade86Fall60"):TEXT("Cascade86Fall36"),FVector((F.X0+F.X1)*.5,F.StartY,F.Bottom),FVector(1,1,1),180);
        Part(R,TEXT("Cascade86Foam"),FVector((F.X0+F.X1)*.5,F.EndY,W+4));
    }
    if(R.Coord.X<6)
    {
        Part(R,TEXT("Cascade86Weir"),FVector(12800,6400,W-450),FVector::OneVector,90);
        Part(R,TEXT("Wall"),FVector(12765,6400,W-230),FVector(.70,128,4.4));
    }
    // Retaining edges close the outer basins; only internal eastward drops
    // spill. All old portal decks cross well above these low stone rims.
    if(R.Coord.X==3)Part(R,TEXT("Wall"),FVector(0,6400,W-450),FVector(.9,128,10));
    if(R.Coord.X==6)Part(R,TEXT("Wall"),FVector(12800,6400,W-450),FVector(.9,128,10));
    if(R.Coord.Y==-1)Part(R,TEXT("Wall"),FVector(6400,0,W-450),FVector(128,.9,10));
    if(R.Coord.Y==2)Part(R,TEXT("Wall"),FVector(6400,12800,W-450),FVector(128,.9,10));
    // A few drowned arches give the water visible depth instead of a flat blue floor.
    for(const FVector P:{FVector(3300,8500,W-420),FVector(9300,8400,W-370),FVector(6400,2600,W-600)})
        Part(R,TEXT("Cascade86Sanctum"),P,FVector(.32),35);
    for(const FVector P:{FVector(4950,4800,H),FVector(7850,4900,H),FVector(5200,11800,H+900),FVector(7600,11800,H+900)})
        Part(R,TEXT("Cascade86Garden"),P,FVector(.65),int32(P.X)%171,1);
    if(R.Coord==EW::ChunkCoord{5,1})Part(R,TEXT("Cascade86Halo"),R.Hub+FVector(0,0,1600),FVector(.75),90);
    Marker(R,15,R.Hub+FVector(0,200,0),TEXT("cascade86/")+R.Coord.Text()+TEXT("/sanctum"));
    Marker(R,16,FVector(6400,10800,H+900),TEXT("cascade86/")+R.Coord.Text()+TEXT("/belvedere"));
    EWSky92Plan::Build(R);
}
void Skyline(const EW::WorldDescriptor& W,EW::ChunkCoord C,TArray<EW::Part>& Out)
{if(Contains(W,C)){EW::ChunkRecipe R;R.World=W;R.Coord=C;Build(R);for(const auto& P:R.Parts)if(P.Detail==0)Out.Add(P);}}
EW::PlaceBookmark Entrance(const EW::WorldDescriptor& W)
{
    EW::ChunkRecipe R;R.World=W;R.Coord={3,0};R.Hub=FVector(6400,6400,Ground(W,R.Coord));Portals(R);
    EW::PlaceBookmark P;P.WorldCode=W.Code();P.Coord=R.Coord;P.Kind=15;P.Id=TEXT("cascade86/entrance");P.Name=TEXT("白塔の水都・西の入口");
    P.LocalPosition=Access(R,0).Inside+FVector(0,0,100);P.Yaw=0;return P;
}
TArray<EW::WaterCityFall> Falls(const EW::WorldDescriptor& W,EW::ChunkCoord C)
{
    TArray<EW::WaterCityFall> Out;if(!Contains(W,C))return Out;const double Base=Ground(W,C);
    for(int32 I=0;I<2;++I)
    {
        EW::WaterCityFall F;const double X=I==0?2300:10300;
        F.Id=TEXT("cascade86/")+C.Text()+FString::Printf(TEXT("/fall%d"),I);F.Group=TEXT("Cascade86");
        F.X0=X-500;F.X1=X+500;F.StartY=3480;F.EndY=3510;F.Bottom=Base-600;F.Top=F.Bottom+(I==0?6000:3600);Out.Add(F);
    }
    return Out;
}
TArray<FVector> Lamps(const EW::ChunkRecipe& R)
{
    TArray<FVector> Out;if(!R.bCascadeCity)return Out;
    for(int32 Side=0;Side<4;++Side){const auto A=Access(R,Side);Out.Add(A.Branch+FVector(0,330,0));}
    Out.Add(R.Hub+FVector(-1400,-1000,0));Out.Add(R.Hub+FVector(1400,1000,0));
    Out.Add(FVector(5400,11600,R.Hub.Z+900));Out.Add(FVector(7400,11600,R.Hub.Z+900));
    Out.Add(FVector(5400,9950,R.Hub.Z+900));Out.Add(FVector(7400,9950,R.Hub.Z+900));Out.Append(EWSky92Plan::Lamps(R));return Out;
}
FString Name(int32 Kind)
{if(Kind>=17 && Kind<=19)return EWSky92Plan::Name(Kind-17);return Kind==16?TEXT("白塔の水都・滝見の回廊"):TEXT("白塔の水都・水鏡の聖堂");}
FString Description(int32 Kind)
{if(Kind>=17 && Kind<=19)return EWSky92Plan::Directions(Kind-17);return Kind==16?TEXT("蔦の絡む石の回廊を上がると、重なる水面、白い尖塔と滝が見渡せる。東へ向かうほど水盤は一段ずつ低くなる。"):
    TEXT("都市の東の外縁に広がる水の街。開いた石の聖堂を抜け、橋の下の柱や水底の遺構を探してみよう。");}
}
