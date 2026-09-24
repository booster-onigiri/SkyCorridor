#include "EWSkyTheatrePlan.h"

bool EWSkyTheatrePlan::Frame(const EW::ChunkRecipe& R,FTransform& Out)
{
    const auto* P=R.Parts.FindByPredicate([](const EW::Part& V){return V.Mesh==TEXT("SkyTheatre87Deck");});
    if(!P)return false;Out=P->Transform;return true;
}
void EWSkyTheatrePlan::AddInfrastructure(EW::ChunkRecipe& R)
{
    if(R.Coord!=EW::ChunkCoord{0,0} || R.World.Seed!=EW::WorldDescriptor::ReferenceWorld().Seed)return;
    const EW::CityWalkFloor* Roof=nullptr;
    for(const auto& F:R.CityFloors)if(F.bRoof && (!Roof || F.Transform.GetLocation().Z>Roof->Transform.GetLocation().Z))Roof=&F;
    if(!Roof)return;
    // A new open deck clears the existing crown, retaining every old roof and save location.
    const FTransform T(Roof->Transform.GetLocation()+FVector(0,0,850));
    auto Part=[&](const TCHAR* Name,FVector P,FVector Scale=FVector(1),double Yaw=0.)
    {R.Parts.Add({FName(Name),FTransform(FRotator(0,Yaw,0),T.TransformPosition(P),Scale),0});};
    auto Box=[&](FVector P,FVector E,bool Floor=false)
    {R.Colliders.Add({FTransform(T.TransformPosition(P)),E,Floor});};
    Part(TEXT("SkyTheatre87Deck"),FVector::ZeroVector);Part(TEXT("SkyTheatre82Seats"),FVector::ZeroVector);
    Box(FVector(0,0,-16),FVector(2350,2350,16),true);
    for(int32 I=0;I<6;++I){double Edge=-290-I*225.,Z=(I+1)*19.;Box(FVector(0,(-1830+Edge)*.5,Z-9.5),FVector(2100,(1830+Edge)*.5,9.5),true);}
    // Rear entrance climbs the seating terrace in normal 19 cm steps.
    for(int32 I=0;I<6;++I)
    {
        const double Start=-2270+I*65.,End=-1830,Z=(I+1)*19.;
        Part(TEXT("StoneDeck"),FVector(0,(Start+End)*.5,Z),FVector(2.6,(End-Start)/100.,1));
        Box(FVector(0,(Start+End)*.5,Z-16),FVector(130,(End-Start)*.5,16),true);
    }
    for(const auto& S:Seats())
    {Box(S.Position+FVector(0,-10,-57),FVector(125,65,38));Box(S.Position+FVector(0,-62,0),FVector(125,12,42));}
    // One floating panel, with collision matching the same tilted actor frame.
    const FTransform Panel(ScreenRotation(),T.TransformPosition(Screen()));
    R.Colliders.Add({FTransform(Panel.GetRotation(),Panel.TransformPosition(FVector(-35,0,0))),FVector(35,Width*.5+30,Width*9./32.+20),false});
    EW::LiftSpec* Lift=nullptr;
    const FString Id=FString::Printf(TEXT("0,0/%d"),Roof->Column);
    for(auto& L:R.Lifts)if(L.Id==Id)Lift=&L;
    if(!Lift)return;
    const auto Previous=Lift->Stops.Last();auto Stop=Previous;const double Dz=T.GetLocation().Z-Stop.Height;
    Stop.Height+=Dz;Stop.Landing.Z+=Dz;Stop.Entry.Z+=Dz;Stop.Threshold.Z+=Dz;Stop.Floor+=2;
    Stop.Label=TEXT("天空シアター・最上階");Lift->Stops.Add(Stop);
    const FVector A=T.InverseTransformPosition(Stop.Entry),B=T.InverseTransformPosition(Stop.Threshold);
    const FVector D=B-A;const FRotator Q=D.Rotation();
    Part(TEXT("StoneDeck"),(A+B)*.5,FVector(D.Size()/100.,3.,1),Q.Yaw);
    R.Colliders.Add({FTransform(Q,T.TransformPosition((A+B)*.5-FVector(0,0,16))),FVector(D.Size()*.5,150,16),true});
    // Physical rail openings follow this lift's actual entry side and position.
    const bool XSide=FMath::Abs(A.X)>FMath::Abs(A.Y);const double Opening=XSide?A.Y:A.X;
    const int32 OpenSide=XSide?(A.X>0?1:3):(A.Y>0?2:0);
    for(int32 Side=0;Side<4;++Side)
    {
        const double Yaw=Side*90.;const FRotator Rota(0,Yaw,0);
        const double Centre=(Side==2||Side==3)?-Opening:Opening;
        auto Rail=[&](double Start,double End)
        {
            if(End<=Start)return;const FVector Pos=Rota.RotateVector(FVector((Start+End)*.5,-2325,0));
            Part(TEXT("StoneRail"),Pos,FVector((End-Start)/400.,.65,1.1),Yaw);
            R.Colliders.Add({FTransform(Rota,T.TransformPosition(Pos+FVector(0,0,60))),FVector((End-Start)*.5,10,60),false});
        };
        TArray<FVector2D> Spans{FVector2D(-2325,2325)};
        auto Cut=[&](double C){TArray<FVector2D> Next;for(const auto S:Spans){if(C-175>S.X)Next.Add({S.X,FMath::Min(S.Y,C-175)});if(C+175<S.Y)Next.Add({FMath::Max(S.X,C+175),S.Y});}Spans=Next;};
        if(Side==OpenSide)Cut(Centre);
        if(Side==0)Cut(11700-T.GetLocation().X); // station express lift
        for(const auto S:Spans)Rail(S.X,S.Y);
    }
    // Keep the skyline open above the floating lift.
}
