#include "EWAeroYachtPlan.h"
#include "EWSkyTheatrePlan.h"

const EWAeroYachtPlan::Layout& EWAeroYachtPlan::Design()
{
    static const Layout Value=[]
    {
        Layout D;const auto W=EW::WorldDescriptor::ReferenceWorld();double TheatreRoof=0;
        // Derive a conservative altitude from every authored roof in the flight
        // envelope. CityVolumeParts does not recursively generate infrastructure.
        for(int Y=-2;Y<=5;++Y)for(int X=-2;X<=4;++X)
        {
            TArray<EW::Part> Parts;EW::CityVolumeParts(W,{X,Y},Parts);
            for(const auto& P:Parts)
            {
                const FString Name=P.Mesh.ToString();const bool Crown=Name.StartsWith(TEXT("UrbanCrown_"));
                if(!Crown && !Name.StartsWith(TEXT("UrbanGarden_")))continue;
                const FVector V=P.Transform.GetLocation()+FVector(0,0,Crown?45:0);
                D.Ceiling=FMath::Max(D.Ceiling,V.Z+1500.);
                if(X==0 && Y==0)TheatreRoof=FMath::Max(TheatreRoof,V.Z);
                if(X==1 && Y==0 && V.Z>D.OldRoof){D.OldRoof=V.Z;D.Terminal=V+FVector(EW::ChunkSize,0,0);}
            }
        }
        const double ScreenTop=TheatreRoof+850+EWSkyTheatrePlan::Screen().Z+
            EWSkyTheatrePlan::Width*9./32.*FMath::Cos(FMath::DegreesToRadians(24.))+400;
        D.Ceiling=FMath::Max(D.Ceiling,ScreenTop);
        D.Terminal.Z=D.Ceiling+2100;D.Ship=D.Terminal+FVector(2400,5600,0);return D;
    }();return Value;
}
double EWAeroYachtPlan::Phase(double Seconds,int32 Index)
{return FMath::Fmod(FMath::Fmod(Seconds+Index*Period/Ships,Period)+Period,Period);}
FTransform EWAeroYachtPlan::Pose(double Seconds,int32 Index)
{
    const double Time=Phase(Seconds,Index);const FVector Dock=Design().Ship;
    if(Time<Dwell)return FTransform(Dock);
    const double T=FMath::Clamp((Time-Dwell)/Journey,0.,1.);
    // Ease only the first/last 14.7 seconds. Easing the whole orbit compressed
    // both vessels around the port while one waited, despite their phase offset.
    constexpr double Ramp=.06;
    const auto Entry=[](double U){const double Q=U/Ramp;return Ramp*(Q*Q*Q-.5*Q*Q*Q*Q)/(1-Ramp);};
    const double Distance=T<Ramp?Entry(T):(T>1-Ramp?1-Entry(1-T):(T-Ramp*.5)/(1-Ramp));
    const double A=2*PI*Distance;
    const FVector P=Dock+FVector(15500*FMath::Sin(A),12500*(1-FMath::Cos(A)),0);
    const FRotator Yaw(0,FMath::RadiansToDegrees(FMath::Atan2(12500*FMath::Sin(A),15500*FMath::Cos(A))),0);
    return FTransform(Yaw,P);
}
EW::PlaceBookmark EWAeroYachtPlan::Arrival()
{
    EW::PlaceBookmark B;B.WorldCode=EW::WorldDescriptor::ReferenceWorld().Code();B.Coord={1,0};
    B.Id=TEXT("aero87/skyport");B.Name=TEXT("アウレリア空中港");B.Kind=0;B.Yaw=90;
    B.LocalPosition=Design().Terminal-FVector(EW::ChunkSize,0,0)+FVector(0,600,99);return B;
}
void EWAeroYachtPlan::AddInfrastructure(EW::ChunkRecipe& R)
{
    if(R.Coord!=EW::ChunkCoord{1,0} || R.World.Seed!=EW::WorldDescriptor::ReferenceWorld().Seed)return;
    const EW::CityWalkFloor* Roof=nullptr;
    for(const auto& F:R.CityFloors)if(F.bRoof && (!Roof || F.Transform.GetLocation().Z>Roof->Transform.GetLocation().Z))Roof=&F;
    if(!Roof)return;
    const FString Id=FString::Printf(TEXT("1,0/%d"),Roof->Column);
    auto* Lift=R.Lifts.FindByPredicate([&](const auto& L){return L.Id==Id;});if(!Lift || Lift->Stops.IsEmpty())return;
    const FVector Origin=Design().Terminal-FVector(EW::ChunkSize,0,0);const FTransform T(Origin);
    auto Part=[&](FName Name,FVector P,FVector Scale=FVector(1),double Yaw=0.)
    {R.Parts.Add({Name,FTransform(FRotator(0,Yaw,0),T.TransformPosition(P),Scale),0});};
    auto Box=[&](FVector P,FVector E,bool Floor=false,double Yaw=0.)
    {R.Colliders.Add({FTransform(FRotator(0,Yaw,0),T.TransformPosition(P)),E,Floor});};
    Part(TEXT("Aero87Terminal"),FVector::ZeroVector);
    Box(FVector(0,0,-20),FVector(1800,1800,20),true);
    Box(FVector(0,2600,-12),FVector(175,800,12),true);
    Box(FVector(0,0,520),FVector(1700,1400,14));
    for(double X:{-1640.,1640.})for(double Y:{-1200.,1200.})Box(FVector(X,Y,255),FVector(20,20,255));
    for(double X:{-1200.,1200.})for(double Y:{-400.,400.})Box(FVector(X,Y,65),FVector(120,55,65));
    for(double X:{-170.,170.})Box(FVector(X,2600,56),FVector(6,800,56));
    auto Stop=Lift->Stops.Last();const double Previous=Stop.Height,Dz=Origin.Z-Stop.Height;
    Stop.Height=Origin.Z;Stop.Entry.Z+=Dz;Stop.Landing.Z+=Dz;Stop.Threshold.Z+=Dz;
    Stop.Floor+=FMath::CeilToInt(Dz/450.);Stop.Label=TEXT("アウレリア空中港・最上階");Lift->Stops.Add(Stop);
    const FVector A=T.InverseTransformPosition(Stop.Entry);const FVector B=FVector::ZeroVector;const FVector V=B-A;
    const double Outer=FMath::Max(FMath::Abs(A.X),FMath::Abs(A.Y));
    const FVector Rim=Outer>1780?A*(1780/Outer):A;
    const FVector Outside=A-Rim;
    if(Outside.Size2D()>1)Part(TEXT("StoneDeck"),(A+Rim)*.5-FVector(0,0,2),FVector(Outside.Size()/100.,3.,1),Outside.Rotation().Yaw);
    Box((A+B)*.5-FVector(0,0,16),FVector(V.Size()*.5,150,16),true,V.Rotation().Yaw);
    const FVector Door=T.InverseTransformPosition(Stop.Threshold),Landing=Door-A;
    Part(TEXT("StoneDeck"),(A+Door)*.5-FVector(0,0,2),FVector((Landing.Size()+12)/100.,3.,1),Landing.Rotation().Yaw);
    Box((A+Door)*.5-FVector(0,0,16),FVector(Landing.Size()*.5+6,150,16),true,Landing.Rotation().Yaw);
    for(const auto Ends:{TPair<FVector,FVector>(Rim,A),TPair<FVector,FVector>(A,Door)})
    {
        const FVector D=Ends.Value-Ends.Key;const FVector Side=FVector::CrossProduct(D.GetSafeNormal2D(),FVector::UpVector);
        for(double S:{-1.,1.})if(D.Size2D()>1)
        {
            const FVector Centre=(Ends.Key+Ends.Value)*.5+Side*145*S;
            Part(TEXT("StoneRail"),Centre,FVector(D.Size()/400.,.5,1),D.Rotation().Yaw);
            Box(Centre+FVector(0,0,56),FVector(D.Size()*.5,6,56),false,D.Rotation().Yaw);
        }
    }
    const bool XS=FMath::Abs(A.X)>FMath::Abs(A.Y);const int OpenSide=XS?(A.X>0?1:3):(A.Y>0?2:0);
    const double Opening=(XS?A.Y:A.X)*1775/Outer;
    for(int Side=0;Side<4;++Side)
    {
        const double Yaw=Side*90.;const FRotator Q(0,Yaw,0);
        TArray<FVector2D> Spans{FVector2D(-1775,1775)};
        auto Cut=[&](double C,double W)
        {
            TArray<FVector2D> Next;for(const auto S:Spans){if(C-W>S.X)Next.Add({S.X,FMath::Min(S.Y,C-W)});if(C+W<S.Y)Next.Add({FMath::Max(S.X,C+W),S.Y});}Spans=Next;
        };
        if(Side==2)Cut(0,190); // north pier
        if(Side==1)Cut(730,175); // upper-line station walkway
        if(Side==OpenSide)Cut((Side==2||Side==3)?-Opening:Opening,185);
        for(const auto S:Spans)if(S.Y-S.X>5)
        {
            const FVector Pos=Q.RotateVector(FVector((S.X+S.Y)*.5,-1775,0));
            Part(TEXT("StoneRail"),Pos,FVector((S.Y-S.X)/400.,.65,1),Yaw);
            Box(Pos+FVector(0,0,60),FVector((S.Y-S.X)*.5,10,60),false,Yaw);
        }
    }
    for(double Z=Previous;Z<Stop.Height;Z+=3600)
        R.Parts.Add({TEXT("UrbanLiftMast"),FTransform(Lift->Rotation,FVector(Lift->Cabin.X,Lift->Cabin.Y,Z)),0});
    // Slender structural columns connect the terminal to the old rooftop.
    for(double X:{-1400.,1400.})for(double Y:{-1400.,1400.})
        Part(TEXT("Wall"),FVector(X,Y,-(Origin.Z-Design().OldRoof)*.5),FVector(.6,.6,(Origin.Z-Design().OldRoof)/100.));
}
