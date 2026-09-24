#include "EWSkyrailPlan.h"
#include "EWHotelPlan.h"
#include "EWSkyTheatrePlan.h"
#include "EWAeroYachtPlan.h"

TArray<FVector> EWSkyrailPlan::HotelPath(const EW::ChunkRecipe& R,int32 Room)
{
    const auto* V=EWHotelPlan::Find(R,Room);if(!V)return {};
    const FVector Deck=V->Frame.GetLocation()+FVector(Room%2?-850:850,0,0);
    const double Z=Deck.Z-4,Y=Deck.Y+(Room<4?1700:-1700);
    return {FVector(6380,6820,Z),FVector(6100,6820,Z),FVector(6100,Y,Z),FVector(Deck.X+2350,Y,Z),
        FVector(Deck.X,Y,Z),FVector(Deck.X,Deck.Y,Z),V->Frame.TransformPosition(FVector(0,-740,0))};
}
TArray<FVector> EWSkyrailPlan::TheatrePath(const EW::ChunkRecipe& R)
{
    FTransform T;if(!EWSkyTheatrePlan::Frame(R,T))return {};
    const auto C=T.GetLocation();const double X=11700;
    return {FVector(11700,7140,C.Z),FVector(X,C.Y-2350,C.Z),FVector(X,C.Y-2150,C.Z),
        FVector(X,C.Y-1700,C.Z),FVector(X,C.Y+450,C.Z),FVector(C.X,C.Y+450,C.Z)};
}
TArray<FVector> EWSkyrailPlan::AirportPath(const EW::ChunkRecipe& R)
{
    const auto T=EWAeroYachtPlan::Design().Terminal;
    return {FVector(24180,5400,T.Z),FVector(T.X+2450,5400,T.Z),FVector(T.X+2450,T.Y+730,T.Z),
        FVector(T.X+1800,T.Y+730,T.Z),FVector(T.X+800,T.Y+730,T.Z),T+FVector(0,730,0)};
}

void EWSkyrailPlan::AddInfrastructure(EW::ChunkRecipe& R)
{
    if(!Enabled(R.World) || R.Coord.Y!=0 || R.Coord.X<0 || R.Coord.X>3)return;
    const FVector Offset(R.Coord.X*EW::ChunkSize,0,0);
    auto Part=[&](const TCHAR* Mesh,FVector P,FVector Scale=FVector(1),double Yaw=0.)
    {R.Parts.Add({FName(Mesh),FTransform(FRotator(0,Yaw,0),P-Offset,Scale),0});};
    auto Box=[&](FVector P,FVector E,bool Floor=false,double Yaw=0.)
    {R.Colliders.Add({FTransform(FRotator(0,Yaw,0),P-Offset),E,Floor});};
    // Axis-aligned unions produce disjoint paving cells at junctions. They avoid
    // coplanar floors and leave only the external sides guarded, with open ends.
    auto Walkway=[&](const TArray<TArray<FVector>>& Paths,bool Guard=true)
    {
        TArray<FBox2D> Rects;TArray<double> Xs,Ys;TArray<FVector2D> Open;double Z=0;
        for(const auto& Path:Paths)
        {
            if(Path.Num()<2)continue;Z=Path[0].Z;
            Open.Add(FVector2D(Path[0]));Open.Add(FVector2D(Path.Last()));
            for(int K=1;K+1<Path.Num();++K)
            {
                const auto A=(Path[K]-Path[K-1]).GetSafeNormal2D(),B=(Path[K+1]-Path[K]).GetSafeNormal2D();
                if(FMath::Abs(FVector::DotProduct(A,B))>.5 || A.IsNearlyZero() || B.IsNearlyZero())continue;
                const FVector2D P(Path[K]);const FBox2D Corner(P-FVector2D(150,150),P+FVector2D(150,150));
                Rects.Add(Corner);Xs.AddUnique(Corner.Min.X);Xs.AddUnique(Corner.Max.X);Ys.AddUnique(Corner.Min.Y);Ys.AddUnique(Corner.Max.Y);
            }
            for(int K=1;K<Path.Num();++K)
            {
                FVector2D A(Path[K-1]),B(Path[K]);if((A-B).Size()<1)continue;
                const bool AlongX=FMath::Abs(A.X-B.X)>FMath::Abs(A.Y-B.Y);
                FBox2D Rect(FVector2D(FMath::Min(A.X,B.X),FMath::Min(A.Y,B.Y)),FVector2D(FMath::Max(A.X,B.X),FMath::Max(A.Y,B.Y)));
                if(AlongX){Rect.Min.Y-=150;Rect.Max.Y+=150;}else{Rect.Min.X-=150;Rect.Max.X+=150;}
                // Square elbows belong to this same union, not a second slab.
                Rect.Min-=FVector2D(1,1);Rect.Max+=FVector2D(1,1);Rects.Add(Rect);
                Xs.AddUnique(Rect.Min.X);Xs.AddUnique(Rect.Max.X);Ys.AddUnique(Rect.Min.Y);Ys.AddUnique(Rect.Max.Y);
            }
        }
        Xs.Sort();Ys.Sort();auto Inside=[&](FVector2D P){return Rects.ContainsByPredicate([&](const auto& B){return B.IsInside(P);});};
        for(int X=1;X<Xs.Num();++X)for(int Y=1;Y<Ys.Num();++Y)
        {
            const FVector2D C((Xs[X-1]+Xs[X])*.5,(Ys[Y-1]+Ys[Y])*.5),E((Xs[X]-Xs[X-1])*.5,(Ys[Y]-Ys[Y-1])*.5);
            if(!Inside(C))continue;
            Part(TEXT("StoneDeck"),FVector(C,Z),FVector(E.X*2/100.,E.Y*2/100.,1));Box(FVector(C,Z-16),FVector(E.X,E.Y,16),true);
            if(!Guard)continue;
            for(int Side=0;Side<4;++Side)
            {
                const bool Vertical=Side<2;const FVector2D D=Vertical?FVector2D(Side?E.X:-E.X,0):FVector2D(0,Side==2?-E.Y:E.Y);
                const FVector2D P=C+D;const FVector2D N=D.GetSafeNormal();if(Inside(P+N*2))continue;
                if(Open.ContainsByPredicate([&](const auto& V){return (P-V).Size()<230;}))continue;
                const double Length=Vertical?E.Y*2:E.X*2;if(Length<5)continue;
                const double Yaw=Vertical?90.:0.;Part(TEXT("StoneRail"),FVector(P,Z),FVector(Length/400.,.5,1),Yaw);
                Box(FVector(P,Z+56),FVector(Length*.5,6,56),false,Yaw);
            }
        }
    };
    auto LiftStop=[&](EW::LiftSpec& L,FVector Cabin,double Z,const FString& Label,int Floor,FRotator Q)
    {
        EW::LiftStop S;S.Height=Z;S.Floor=Floor;S.Label=Label;S.BoardingRotation=Q;
        const FVector Out=Q.RotateVector(FVector(0,1,0));S.Landing=FVector(Cabin.X,Cabin.Y,Z)+Out*320-Offset;
        S.Entry=S.Landing;S.Threshold=FVector(Cabin.X,Cabin.Y,Z)+Out*190-Offset;L.Stops.Add(S);
    };
    auto FinishLift=[&](EW::LiftSpec L)
    {
        // Express lifts float freely; cabin collision and landing guards are independent.
        R.Lifts.Add(L);
    };
    for(int Line=0;Line<2;++Line)
    {
        for(double X=StopX(0,Line)-1300;X<StopX(Count(Line)-1,Line)+1300;X+=400)
            if(int64(X/EW::ChunkSize)==R.Coord.X){const auto A=TrackPose(X,Line).GetLocation(),B=TrackPose(X+400,Line).GetLocation();Part(TEXT("SkyrailTrack"),(A+B)*.5,FVector((B-A).Size()/400.,1,1),(B-A).Rotation().Yaw);}
        for(double X:{StopX(0,Line)-1280.,StopX(Count(Line)-1,Line)+1280.})if(int64(X/EW::ChunkSize)==R.Coord.X)
            Part(TEXT("SkyrailBuffer"),FVector(X,LineY,Height(Line)),FVector(1),X<StopX(0,Line)?0:180);
        for(int I=0;I<Count(Line);++I)
        {
            if(StopChunk(I,Line)!=R.Coord)continue;const auto S=Stop(I,Line);
            Part(TEXT("SkyrailStation"),S);if(Line || I==1){Part(TEXT("Cascade88StationRibs"),S);Part(TEXT("Cascade88StationGlass"),S);}
            Box(S+FVector(100,420,-14),FVector(1400,250,14),true);
            Box(S+FVector(100,662,66),FVector(1400,8,66));Box(S+FVector(-1292,420,66),FVector(8,250,66));
            for(FVector Segment:{FVector(-970,660,0),FVector(0,960,0),FVector(1070,860,0)})Box(S+FVector(Segment.X,178,62),FVector(Segment.Y*.5,8,62));
            if(Line)continue;
            EW::LiftSpec Lift;Lift.Id=LiftId(I);const FVector Cabin=S+FVector(1700,420,0);Lift.Cabin=Cabin-Offset;Lift.Rotation=FRotator(0,90,0);Lift.Outward=FVector(1,0,0);
            LiftStop(Lift,Cabin,R.Hub.Z+8,I?TEXT("水鏡の聖堂・水辺の広場"):TEXT("時計広場・街の回廊"),0,FRotator::ZeroRotator);
            LiftStop(Lift,Cabin,FloorZ,Name(I)+TEXT("駅・水都線"),1,FRotator(0,90,0));
            // The water-city landing lies wholly on its large square. Side
            // guards here would fence the passenger into a strip of open plaza.
            Walkway({{FVector(Cabin.X,Cabin.Y+190,R.Hub.Z+8),FVector(Cabin.X,Cabin.Y+1070,R.Hub.Z+8)}},I==0);
            if(I==0)
            {
                LiftStop(Lift,Cabin,UpperZ,TEXT("上層線・ホテル／空港方面"),2,FRotator(0,90,0));
                auto Path=TheatrePath(R);if(!Path.IsEmpty())
                {
                    LiftStop(Lift,Cabin,Path[0].Z,TEXT("天空シアター・大画面と客席"),3,FRotator::ZeroRotator);
                    TArray<FVector> Bridge{FVector(Cabin.X,Cabin.Y+190,Path[0].Z),Path[0],Path[1]};Walkway({Bridge});
                }
            }
            FinishLift(Lift);
        }
    }
    if(R.Coord==EW::ChunkCoord{0,0})
    {
        EW::LiftSpec L;L.Id=LiftId(0,1);const FVector C(6700,6820,UpperZ);L.Cabin=C;L.Rotation=FRotator(0,90,0);L.Outward=FVector(1,0,0);
        for(int Level=0;Level<2;++Level)
        {
            const auto A=HotelPath(R,Level*2),B=HotelPath(R,4+Level*2);if(A.IsEmpty() || B.IsEmpty())continue;
            LiftStop(L,C,A[0].Z,Level?TEXT("高級ホテル 103・104・107・108号室"):TEXT("高級ホテル 101・102・105・106号室"),Level,FRotator(0,90,0));
            Walkway({{FVector(C.X-190,C.Y,A[0].Z),A[0],A[1],A[2],A[3]}, {A[1],B[2],B[3]}});
        }
        LiftStop(L,C,UpperZ,TEXT("雲上ホテル駅・シアター／空港方面"),2,FRotator(0,90,0));FinishLift(L);
    }
    if(R.Coord==EW::ChunkCoord{1,0})
    {
        EW::LiftSpec L;L.Id=LiftId(2,1);const FVector C(24500,5400,UpperZ);L.Cabin=C-Offset;L.Rotation=FRotator(0,90,0);L.Outward=FVector(1,0,0);
        LiftStop(L,C,UpperZ,TEXT("空中港駅・ホテル／シアター方面"),0,FRotator(0,90,0));
        const auto Path=AirportPath(R);LiftStop(L,C,Path[0].Z,TEXT("アウレリア空中港・豪華飛行船の搭乗口"),1,FRotator(0,90,0));
        Walkway({{FVector(24000,6820,UpperZ),FVector(24180,6820,UpperZ),FVector(24180,5400,UpperZ),FVector(24310,5400,UpperZ)}});
        Walkway({{FVector(24310,5400,Path[0].Z),Path[0],Path[1],Path[2],Path[3]}});FinishLift(L);
    }
}
