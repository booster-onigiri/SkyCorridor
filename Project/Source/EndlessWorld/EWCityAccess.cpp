#include "EWCityAccess.h"
#include "EWWaterCity.h"
#include "EWInteriors.h"

namespace EW
{
namespace
{
void PartAt(ChunkRecipe& R, FName Mesh, const FTransform& T, uint8 Detail=1)
{ R.Parts.Add({Mesh,T,Detail}); }
void BoxAt(ChunkRecipe& R,const FTransform& T,FVector Centre,FVector Extent,bool Floor=false)
{
    R.Colliders.Add({FTransform(T.GetRotation(),T.TransformPosition(Centre)),Extent*T.GetScale3D().GetAbs(),Floor});
}
void Guard(ChunkRecipe& R,const FTransform& T,double A,double B,double Y,bool Visible=true)
{
    if (B-A<2) return;
    const FVector P((A+B)*.5,Y,0);
    if (Visible) PartAt(R,TEXT("StoneRail"),FTransform(T.GetRotation(),T.TransformPosition(P),
        FVector((B-A)/400.,1,1.10)*T.GetScale3D()));
    BoxAt(R,T,P+FVector(0,0,62),FVector((B-A)*.5,12,62));
}
void ConsolidateGuards(ChunkRecipe& R,const FTransform& T,int32 First)
{
    // Joining touching rail boxes preserves their exact solid union and every
    // bridge/lift opening, while keeping furnished chunks inside the body budget.
    TArray<FVector2D> Ranges;
    for(int32 I=First;I<R.Colliders.Num();++I)
    {
        const auto& C=R.Colliders[I];const double X=T.InverseTransformPosition(C.Transform.GetLocation()).X;
        const double Half=C.Extent.X/FMath::Abs(T.GetScale3D().X);Ranges.Add({X-Half,X+Half});
    }
    R.Colliders.SetNum(First,EAllowShrinking::No);Ranges.Sort([](auto A,auto B){return A.X<B.X;});
    TArray<FVector2D> Joined;
    for(const auto Range:Ranges)
        if(!Joined.IsEmpty() && Range.X<=Joined.Last().Y+.001)Joined.Last().Y=FMath::Max(Joined.Last().Y,Range.Y);
        else Joined.Add(Range);
    for(const auto Range:Joined)BoxAt(R,T,FVector((Range.X+Range.Y)*.5,-2385,62),FVector((Range.Y-Range.X)*.5,12,62));
}
void Deck(ChunkRecipe& R,FVector A,FVector B,double Width,bool Rails=false,bool Ground=false)
{
    const FVector Delta=B-A;
    if (Delta.Size()<1) return;
    const FQuat Q=FRotationMatrix::MakeFromXZ(Delta.GetSafeNormal(),FVector::UpVector).ToQuat();
    const FVector Normal=Q.GetUpVector();
    PartAt(R,TEXT("StoneDeck"),FTransform(Q,(A+B)*.5,FVector((Delta.Size()+3)/100.,Width/100.,1)),0);
    R.Colliders.Add({FTransform(Q,(A+B)*.5-Normal*20),FVector(Delta.Size()*.5+2,Width*.5,20),true});
    if (Ground) R.Paths.Add({A,B,Width});
    if (Rails)
    {
        const auto Boundary=[](FVector P){return P.X<1 || P.Y<1 || P.X>ChunkSize-1 || P.Y>ChunkSize-1;};
        const double TrimA=Ground && !Boundary(A)?450.:0., TrimB=Ground && !Boundary(B)?450.:0.;
        if (Delta.Size()>TrimA+TrimB+10)
        {
            const FTransform T(Q,(A+B)*.5+Delta.GetSafeNormal()*(TrimA-TrimB)*.5);
            const double Half=(Delta.Size()-TrimA-TrimB)*.5;
            for (double S:{-1.,1.}) Guard(R,T,-Half,Half,S*(Width*.5-15));
        }
    }
}
bool IsBlock(const FString& Name){return Name.StartsWith(TEXT("UrbanBlock_")) || Name.StartsWith(TEXT("UrbanLibrary_"));}
bool IsBridge(const FString& Name){return Name.StartsWith(TEXT("UrbanBridge_")) || Name.StartsWith(TEXT("UrbanPromenade_"));}
double BridgeHalfWidth(const FString& Name)
{return Name.StartsWith(TEXT("UrbanPromenade_"))?350.:FCString::Atoi(*Name.Right(1))%2?260.:370.;}
FTransform SideTransform(const FTransform& T,int32 Side)
{
    const FVector S=T.GetScale3D();
    return FTransform(T.GetRotation()*FRotator(0,Side*90.,0).Quaternion(),T.GetLocation(),Side%2?FVector(S.Y,S.X,S.Z):S);
}

void PathOpening(const FTransform& T,const PathSegment& P,double Limit,TArray<FVector2D>& Open)
{
    const FVector N=FVector::CrossProduct((P.B-P.A).GetSafeNormal2D(),FVector::UpVector)*(P.Width*.5);
    double X[2];
    for(int32 Edge=0;Edge<2;++Edge)
    {
        const FVector A=T.InverseTransformPosition(P.A+N*(Edge?1.:-1.)),B=T.InverseTransformPosition(P.B+N*(Edge?1.:-1.));
        if(FMath::Abs(B.Y-A.Y)<.001)return;
        const double U=(-2385-A.Y)/(B.Y-A.Y);if(U<0 || U>1)return;
        X[Edge]=FMath::Lerp(A.X,B.X,U);
    }
    if(FMath::Max(X[0],X[1])>-Limit && FMath::Min(X[0],X[1])<Limit)
        Open.Add(FVector2D(FMath::Max(-Limit,FMath::Min(X[0],X[1])-12),FMath::Min(Limit,FMath::Max(X[0],X[1])+12)));
}

void ResidenceCollision(ChunkRecipe& R,const ResidenceSpec& S)
{
    const FTransform& T=S.Frame;
    auto Solid=[&](FVector Lo,FVector Hi,bool Floor=false)
    {
        if((Hi-Lo).GetMin()>.01)BoxAt(R,T,(Lo+Hi)*.5,(Hi-Lo)*.5,Floor);
    };
    // Partition only the old occupied box. The open pocket is x[0,900],
    // y[-1800,-1200], z[0,340] in the selected facade's floor frame.
    Solid(FVector(-1800,-1800,-S.FloorOffset),FVector(1800,1800,-24));
    Solid(FVector(-1800,-1800,340),FVector(1800,1800,3580-S.FloorOffset));
    Solid(FVector(-1800,-1800,0),FVector(0,1800,340));
    Solid(FVector(900,-1800,0),FVector(1800,1800,340));
    Solid(FVector(0,-1200,0),FVector(900,1800,340));
    Solid(FVector(0,-1800,-24),FVector(900,-1200,0),true);
    // Door bay x=225 is clear from the public arcade. Window bay x=675
    // has a visible sill and brass guard, not a body-sized open drop.
    for(const FVector2D Range:{FVector2D(0,100),FVector2D(350,550),FVector2D(800,900)})
        Solid(FVector(Range.X,-1820,0),FVector(Range.Y,-1660,340));
    Solid(FVector(100,-1820,285),FVector(350,-1660,340));
    Solid(FVector(550,-1820,0),FVector(800,-1660,55));
    Solid(FVector(550,-1820,315),FVector(800,-1660,340));
    BoxAt(R,T,FVector(675,-1810,83),FVector(125,12,28));
    BoxAt(R,T,FVector(650,-1710,44),FVector(110,38,44));
    for(const FVector P:{FVector(85,-1300,32),FVector(855,-1620,32)})
        BoxAt(R,T,P,FVector(32,32,32));
    BoxAt(R,T,FVector(-225,-1980,70),FVector(75,40,70));
}
}

bool IsRainWindowDistrict(const WorldDescriptor& W,ChunkCoord C)
{ return C==ChunkCoord{0,0} && W.Seed==WorldDescriptor::ReferenceWorld().Seed; }

bool DescribeRainWindow(const ChunkRecipe& R,const Part& Block,ResidenceSpec& Out)
{
    if(!IsRainWindowDistrict(R.World,R.Coord) || Block.Mesh!=TEXT("UrbanBlock_RainWindow"))return false;
    Out=ResidenceSpec();Out.BlockTransform=Block.Transform;
    const double Datum=FMath::FloorToDouble(HeightAt(R.World,0,0)/450.)*450.;
    const int32 Level=FMath::RoundToInt((Block.Transform.GetLocation().Z-Datum)/3600.);
    const uint64 V=R.World.Number("urban-storeys",{0,0,0,1,Level});
    Out.OriginalMesh=FName(*FString::Printf(TEXT("UrbanBlock_%d"),int32(V%8)));
    Out.FloorOffset=R.Hub.Z-Block.Transform.GetLocation().Z;
    double Best=-DBL_MAX;
    for(int32 Side=0;Side<4;++Side)
    {
        const FTransform T=SideTransform(Block.Transform,Side);
        const double Score=FVector::DotProduct((R.Hub-T.GetLocation()).GetSafeNormal2D(),T.TransformVectorNoScale(FVector(0,-1,0)));
        if(Score>Best){Best=Score;Out.OpeningSide=Side;Out.Frame=T;}
    }
    Out.Frame.AddToTranslation(FVector(0,0,Out.FloorOffset));
    Out.Entry=Out.Frame.TransformPosition(FVector(225,-2090,88));
    Out.Valve=Out.Frame.TransformPosition(FVector(-225,-2040,115));
    Out.Seat=Out.Frame.TransformPosition(FVector(600,-1740,130));
    Out.Stand=Out.Frame.TransformPosition(FVector(675,-1320,88));
    const Part* Clock=R.Parts.FindByPredicate([](const Part& P){return P.Mesh==TEXT("ClockTower");});
    if(!Clock)return false;
    // build_kit.py's west clock hand: 223 cm out, 1742 cm up before the
    // existing landmark's scale. This is an actual mesh point, not open sky.
    Out.ViewTarget=Clock->Transform.TransformPosition(FVector(-223,0,1742));
    const FRotator View=(Out.ViewTarget-(Out.Seat+FVector(0,0,47))).Rotation();
    Out.ViewYaw=View.Yaw;Out.ViewPitch=View.Pitch;
    return FMath::Abs(Out.FloorOffset)<.01 || FMath::Abs(Out.FloorOffset-1800.)<.01;
}

void BuildCityGround(ChunkRecipe& R)
{
    // The narrow central cross and perimeter lanes clear all four building bodies.
    const FVector H=R.Hub;
    TArray<FVector> Ring;
    for (const FVector P:{FVector(600,0,0),FVector(600,600,0),FVector(0,600,0),FVector(-600,600,0),
        FVector(-600,0,0),FVector(-600,-600,0),FVector(0,-600,0),FVector(600,-600,0)}) Ring.Add(H+P);
    for (int32 I=0;I<Ring.Num();++I) Deck(R,Ring[I],Ring[(I+1)%Ring.Num()],440,false,true);
    for (int32 Side=0;Side<4;++Side) Guard(R,FTransform(FRotator(0,Side*90.,0),H),-380,380,-380);
    Deck(R,H+FVector(0,-1550,0),H+FVector(0,-600,0),700,false,true);
    for (const auto& P:R.Portals)
    {
        FVector In=P.Position,Turn=P.Position,End=H;
        if(P.Axis==0){In.X=P.Position.X<1?100:ChunkSize-100;Turn=FVector(In.X,H.Y,0);End.X+=P.Position.X<1?-600:600;}
        else {In.Y=P.Position.Y<1?100:ChunkSize-100;Turn=FVector(H.X,In.Y,0);End.Y+=P.Position.Y<1?-600:600;}
        FVector Apron=End;
        if(P.Axis==0)Apron.X=P.Position.X<1?3000:9800;else Apron.Y=P.Position.Y<1?3000:9800;
        Apron.Z=H.Z;
        const double L1=FVector::Dist2D(P.Position,In),L2=FVector::Dist2D(In,Turn),L3=FVector::Dist2D(Turn,Apron);
        In.Z=FMath::Lerp(P.Position.Z,H.Z,L1/(L1+L2+L3));
        Turn.Z=FMath::Lerp(P.Position.Z,H.Z,(L1+L2)/(L1+L2+L3));
        Deck(R,P.Position,In,P.Width,true,true);Deck(R,In,Turn,P.Width,true,true);
        // Finish the ramp before the flat square edge, avoiding a raised lip.
        Deck(R,Turn,Apron,P.Width,false,true);Deck(R,Apron,End,P.Width,false,true);
    }
}

void BuildCityAccess(ChunkRecipe& R)
{
    if(R.Region!=RegionKind::City)return;
    const TArray<PathSegment> WaterPaths=WaterCityAccessPaths(R.World,R.Coord,true);
    const TArray<Part> Original=R.Parts;
    TArray<FVector> Centres;
    TArray<Part> Bridges;
    TArray<TPair<FTransform,int32>> GroundRings;
    for(const auto& P:Original)
    {
        const FString Name=P.Mesh.ToString();
        if(IsBlock(Name))
        {
            if(!Centres.ContainsByPredicate([&](const FVector& V){return FVector::DistSquared2D(V,P.Transform.GetLocation())<1;}))
                Centres.Add(P.Transform.GetLocation());
        }
        if(IsBridge(Name)) Bridges.Add(P);
    }
    // Incoming bridges belong to the western/southern chunk. They must also open
    // the matching rail on this side of that shared boundary.
    for(const ChunkCoord Offset:{ChunkCoord{-1,0},ChunkCoord{0,-1}})
    {
        TArray<Part> Neighbor;const ChunkCoord C=R.Coord+Offset;
        CityVolumeParts(R.World,C,Neighbor);
        for(auto P:Neighbor) if(IsBridge(P.Mesh.ToString()))
        { P.Transform.AddToTranslation(FVector(Offset.X*ChunkSize,Offset.Y*ChunkSize,0));Bridges.Add(P); }
    }
    for(int32 Column=0;Column<Centres.Num();++Column)
    {
        TArray<Part> Blocks,Roofs,Terraces;
        for(const auto& P:Original)
        {
            if(FVector::DistSquared2D(P.Transform.GetLocation(),Centres[Column])>1)continue;
            const FString Name=P.Mesh.ToString();
            if(IsBlock(Name))Blocks.Add(P);
            else if(Name.StartsWith(TEXT("UrbanGarden_")) || Name.StartsWith(TEXT("UrbanCrown_")))Roofs.Add(P);
            else if(Name.StartsWith(TEXT("UrbanTerrace_")))Terraces.Add(P);
        }
        Blocks.Sort([](const Part&A,const Part&B){return A.Transform.GetLocation().Z<B.Transform.GetLocation().Z;});
        if(Blocks.IsEmpty())continue;
        const FTransform Base=Blocks[0].Transform;
        int32 LiftSide=0;double Best=-DBL_MAX;
        for(int32 Side=0;Side<4;++Side)
        {
            const FVector Out=SideTransform(Base,Side).TransformVectorNoScale(FVector(0,-1,0));
            const double Score=FVector::DotProduct((R.Hub-Base.GetLocation()).GetSafeNormal2D(),Out);
            if(Score>Best){Best=Score;LiftSide=Side;}
        }
        const FTransform Facing=SideTransform(Base,LiftSide);
        const double EntranceX=FVector::DotProduct(R.Hub-Base.GetLocation(),Facing.TransformVectorNoScale(FVector(1,0,0)))>=0?1600.:-1600.;
        LiftSpec Lift;Lift.Id=FString::Printf(TEXT("%s/%d"),*R.Coord.Text(),Column);
        Lift.Outward=Facing.TransformVectorNoScale(FVector(0,-1,0));Lift.Rotation=Facing.Rotator();
        Lift.Cabin=Facing.TransformPosition(FVector(EntranceX,-2385,0))+Lift.Outward*350.;
        Lift.Cabin.Z=0;
        const FVector Door=Lift.Cabin-Lift.Outward*200.;
        TArray<CityWalkFloor> Floors;
        for(const auto& P:Blocks)
        {
            // Furnished rooms open from the existing public gallery datum.
            ResidenceSpec Residence;
            if(DescribeRainWindow(R,P,Residence))
            { ResidenceCollision(R,Residence);R.Residences.Add(Residence);EWInteriors::AddRain(R,Residence); }
            else if(!EWInteriors::AddBlock(R,P))BoxAt(R,P.Transform,FVector(0,0,1790),FVector(1800,1800,1790));
            for(double Z:{0.,1800.})
            { auto T=P.Transform;T.AddToTranslation(FVector(0,0,Z));Floors.Add({Column,T,false}); }
            if(P.Mesh.ToString().StartsWith(TEXT("UrbanLibrary_")))
                for(int32 Side=0;Side<4;++Side)for(double X:{-1100.,1100.})
                    BoxAt(R,SideTransform(P.Transform,Side),FVector(X,-2210,1850),FVector(325,85,50));
        }
        for(const auto& P:Roofs)
        {
            auto T=P.Transform;
            const bool Crown=P.Mesh.ToString().StartsWith(TEXT("UrbanCrown_"));
            if(Crown)T.AddToTranslation(FVector(0,0,45));
            Floors.Add({Column,T,true});
            BoxAt(R,T,FVector(0,0,-20),Crown?FVector(1900,1900,20):FVector(2075,2075,20),true);
            if(Crown)
            {
                for(double X:{-900.,900.})for(double Y:{-900.,900.})
                    BoxAt(R,T,FVector(X,Y,275),FVector(550,500,295));
                for(double X:{-1700.,1700.})for(double Y:{-1700.,1700.})
                    BoxAt(R,T,FVector(X,Y,285),FVector(42,42,300));
            }
            else
            {
                for(int32 Side=0;Side<4;++Side)for(double X:{-1400.,-500.,500.,1400.})
                    BoxAt(R,SideTransform(T,Side),FVector(X,-1860,61.25),FVector(265,102.5,61.25),true);
                // The original garden FBX reflects Blender Y. Keep asymmetric
                // solids in the imported frame, including the open pavilion.
                for(double X:{-1200.,0.})for(double Y:{-500.,-1700.})
                {
                    BoxAt(R,T,FVector(X,Y,29),FVector(37.4,37.4,19),true);
                    BoxAt(R,T,FVector(X,Y,271),FVector(22,22,223));
                    BoxAt(R,T,FVector(X,Y,512),FVector(35.2,35.2,18));
                }
                // Individual copper strips follow the visible curved canopy;
                // a single flat box used to block empty air and miss its crown.
                for(int32 I=0;I<25;++I)
                {
                    const double U=(I-12)/12.;
                    BoxAt(R,T,FVector(-600+U*700,-1100,565+160*(1-U*U)),FVector(1400./48.+2.75,750,10),true);
                }
                for(FVector P0:{FVector(1100,-1100,0),FVector(-1200,1000,0)})
                {
                    BoxAt(R,T,P0+FVector(0,0,55),FVector(270,270,55),true);
                    BoxAt(R,T,P0+FVector(20,0,205),FVector(24,24,95));
                }
            }
        }
        for(const auto& P:Terraces)for(int32 Side=0;Side<4;++Side)for(double X:{-1480.,-650.,650.,1480.})
            BoxAt(R,SideTransform(P.Transform,Side),FVector(X,-1860,61.5),FVector(175,102.5,61.5));
        auto AddLanding=[&](double Z,const FVector& Entry,int32 Floor,const FString& Label)
        {
            const FVector Car(Lift.Cabin.X,Lift.Cabin.Y,Z);
            const FVector TowardEntry=(Entry-Car).GetSafeNormal2D();
            const FRotator Boarding=FRotationMatrix::MakeFromYZ(TowardEntry,FVector::UpVector).Rotator();
            const FVector D=Car+TowardEntry*200.;
            Deck(R,Entry,D,280,true);
            // The door collider and moving cabin are owned by AEWLift.
            const FVector Landing=D+TowardEntry*180.;
            const FVector Tangent=Boarding.RotateVector(FVector(1,0,0));
            PartAt(R,TEXT("UrbanLiftSign"),FTransform(Boarding,Landing+Tangent*180.),2);
            R.Colliders.Add({FTransform(Boarding,Landing+Tangent*180.+FVector(0,0,60)),FVector(58,35,60),false});
            Lift.Stops.Add({Z,Landing,Label,Floor,Entry,D,Boarding});
        };
        for(const auto& F:Floors)
        {
            const FTransform T=F.Transform;const double Z=T.GetLocation().Z;
            const bool Ground=FMath::Abs(Z-R.Hub.Z)<1;
            R.CityFloors.Add(F);PartAt(R,Ground?TEXT("UrbanWalkRingFloor"):TEXT("UrbanWalkRing"),T,0);
            if(Ground)GroundRings.Add({T,LiftSide+(EntranceX<0?4:0)});
            for(double S:{-1.,1.})
            {
                BoxAt(R,T,FVector(0,S*2100,-18),FVector(2400,300,18),true);
                BoxAt(R,T,FVector(S*2100,0,-18),FVector(300,1800,18),true);
            }
            for(int32 Side=0;Side<4;++Side)
            {
                if(Ground)continue;
                const FTransform ST=SideTransform(T,Side);
                const int32 FirstGuard=R.Colliders.Num();
                for(const FVector2D Range:{FVector2D(-2400,-1780),FVector2D(-1420,-800),FVector2D(800,1420),FVector2D(1780,2400)})
                    Guard(R,ST,Range.X,Range.Y,-2385,false);
                TArray<FVector2D> Openings;
                for(const auto& B:Bridges)
                {
                    if(FMath::Abs(B.Transform.GetLocation().Z-Z)>50)continue;
                    const double Half=BridgeHalfWidth(B.Mesh.ToString());
                    double Xs[2];bool Cross=true;
                    for(int32 Edge=0;Edge<2;++Edge)
                    {
                        const FVector A=ST.InverseTransformPosition(B.Transform.TransformPosition(FVector(-3200,Edge?Half:-Half,0)));
                        const FVector E=ST.InverseTransformPosition(B.Transform.TransformPosition(FVector(3200,Edge?Half:-Half,0)));
                        if(FMath::Abs(E.Y-A.Y)<.01){Cross=false;break;}
                        const double U=(-2385-A.Y)/(E.Y-A.Y);
                        if(U<0 || U>1 || FMath::Abs(FMath::Lerp(A.Z,E.Z,U))>8){Cross=false;break;}Xs[Edge]=FMath::Lerp(A.X,E.X,U);
                    }
                    if(Cross && FMath::Max(Xs[0],Xs[1])>-800 && FMath::Min(Xs[0],Xs[1])<800)
                        Openings.Add(FVector2D(FMath::Max(-800.,FMath::Min(Xs[0],Xs[1])-12),FMath::Min(800.,FMath::Max(Xs[0],Xs[1])+12)));
                }
                // Water bridges use only the existing central opening in the
                // ring mesh. The fixed corner ranges above remain untouched.
                for(const auto& P:WaterPaths)if(FMath::Abs(P.A.Z-Z)<1 && FMath::Abs(P.B.Z-Z)<1)
                    PathOpening(ST,P,800,Openings);
                Openings.Sort([](auto A,auto B){return A.X<B.X;});double Cursor=-800;
                for(auto O:Openings){Guard(R,ST,Cursor,O.X,-2385);Cursor=FMath::Max(Cursor,O.Y);}
                Guard(R,ST,Cursor,800,-2385);
                for(double X:{-1600.,1600.})if(Side!=LiftSide || X!=EntranceX)Guard(R,ST,X-180,X+180,-2385);
                ConsolidateGuards(R,ST,FirstGuard);
            }
            const FVector Entry=SideTransform(T,LiftSide).TransformPosition(FVector(EntranceX,-2300,0));
            const double Datum=FMath::FloorToDouble(HeightAt(R.World,0,0)/450.)*450.;
            const int32 Floor=FMath::RoundToInt((Z-Datum)/450.);
            AddLanding(Z,Entry,Floor,Ground?FString::Printf(TEXT("出発広場　%+d 階"),Floor):F.bRoof?FString::Printf(TEXT("屋上庭園　%+d 階"),Floor):FString::Printf(TEXT("%+d 階　空中回廊"),Floor));
        }
        const double GroundZ=R.Hub.Z;
        {
            FVector End=Door-Lift.Outward*350.;End.Z=GroundZ;
            FVector Tangent=Lift.Rotation.RotateVector(FVector(1,0,0));
            if(FVector::DotProduct(R.Hub-End,Tangent)<0)Tangent=-Tangent;
            const FVector Corner=End+Tangent*600.;
            FVector Cross=Corner;
            if(FMath::Abs(Lift.Outward.X)>.5)Cross.X=R.Hub.X;else Cross.Y=R.Hub.Y;
            FVector Start=R.Hub;
            if(FMath::Abs(Lift.Outward.X)>.5)Start.Y=Cross.Y>R.Hub.Y?R.Hub.Y+600:R.Hub.Y-600;
            else Start.X=Cross.X>R.Hub.X?R.Hub.X+600:R.Hub.X-600;
            // Central-cross connection, never a diagonal through a residence.
            // Approach from the side, leaving the moving car's back and side
            // walls outside the public route to its boarding door.
            Deck(R,Start,Cross,400,false,true);Deck(R,Cross,Corner,400,false,true);Deck(R,Corner,End,400,false,true);
            if(!Lift.Stops.ContainsByPredicate([&](const LiftStop&S){return FMath::Abs(S.Height-GroundZ)<100.;}))
                AddLanding(GroundZ,End,0,TEXT("出発広場　地上連絡路"));
        }
        Lift.Stops.Sort([](const LiftStop&A,const LiftStop&B){return A.Height<B.Height;});
        for(double Z=Lift.Stops[0].Height;Z<Lift.Stops.Last().Height;Z+=3600)
            PartAt(R,TEXT("UrbanLiftMast"),FTransform(Lift.Rotation,FVector(Lift.Cabin.X,Lift.Cabin.Y,Z)),1);
        R.Lifts.Add(MoveTemp(Lift));
    }
    // Ground guards are cut against every completed street/spur, including
    // junctions far from the central opening used by the upper air bridges.
    TArray<PathSegment> GroundPaths=R.Paths;GroundPaths.Append(WaterPaths);
    for(const auto& Ring:GroundRings)for(int32 Side=0;Side<4;++Side)
    {
        const FTransform T=SideTransform(Ring.Key,Side);TArray<FVector2D> Open;
        if(Side==Ring.Value%4){const double X=Ring.Value>=4?-1600.:1600.;Open.Add(FVector2D(X-180,X+180));}
        for(const auto& P:GroundPaths)
        {
            if(FMath::Abs(P.A.Z-R.Hub.Z)>1 || FMath::Abs(P.B.Z-R.Hub.Z)>1)continue;
            const FVector N=FVector::CrossProduct((P.B-P.A).GetSafeNormal2D(),FVector::UpVector)*(P.Width*.5);
            double X[2];bool Cross=true;
            for(int32 Edge=0;Edge<2;++Edge)
            {
                const FVector A=T.InverseTransformPosition(P.A+N*(Edge?1.:-1.)),B=T.InverseTransformPosition(P.B+N*(Edge?1.:-1.));
                if(FMath::Abs(B.Y-A.Y)<.001){Cross=false;break;}
                const double U=(-2385-A.Y)/(B.Y-A.Y);
                if(U<0 || U>1){Cross=false;break;}X[Edge]=FMath::Lerp(A.X,B.X,U);
            }
            if(Cross && FMath::Max(X[0],X[1])>-2400 && FMath::Min(X[0],X[1])<2400)
                Open.Add(FVector2D(FMath::Max(-2400.,FMath::Min(X[0],X[1])-15),FMath::Min(2400.,FMath::Max(X[0],X[1])+15)));
        }
        Open.Sort([](auto A,auto B){return A.X<B.X;});double Cursor=-2400;
        for(auto O:Open){Guard(R,T,Cursor,O.X,-2385);Cursor=FMath::Max(Cursor,O.Y);}Guard(R,T,Cursor,2400,-2385);
    }
    for(const auto& P:Original)if(IsBridge(P.Mesh.ToString()))
    {
        const FString Name=P.Mesh.ToString();const double Half=BridgeHalfWidth(Name);
        BoxAt(R,P.Transform,FVector(0,0,-18),FVector(3200,Half,18),true);
        for(double S:{-1.,1.}) BoxAt(R,P.Transform,FVector(0,S*(Half-10),60),FVector(3200,14,60));
        const int32 Variant=FCString::Atoi(*Name.Right(1));
        if(Name.StartsWith(TEXT("UrbanPromenade_")))
        {
            for(int32 X=-2800;X<=2800;X+=700)for(double Y:{-350.,350.})
                BoxAt(R,P.Transform,FVector(X,Y,220),FVector(18,18,220));
            if(Variant==0)BoxAt(R,P.Transform,FVector(0,0,470),FVector(3200,Half+50,25));
        }
        else if(Variant%2)
        {
            for(int32 X=-3000;X<=3000;X+=500)for(double S:{-1.,1.})
                BoxAt(R,P.Transform,FVector(X,S*(Half-28),177.5),FVector(14,14,177.5));
            BoxAt(R,P.Transform,FVector(0,0,395),FVector(3200,Half+40,19));
        }
    }
}
}
