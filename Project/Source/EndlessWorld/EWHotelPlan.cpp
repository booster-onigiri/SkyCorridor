#include "EWHotelPlan.h"
#include "EWHotel83Data.h"

FString EWHotelPlan::Name(int32 I)
{
    static const TCHAR* N[]={TEXT("凪の和邸"),TEXT("雲のギャラリー"),TEXT("水庭のサロン"),TEXT("琥珀のオアシス"),TEXT("真珠のサロン"),TEXT("夜景の書斎"),TEXT("光彩のロフト"),TEXT("湖雲のロッジ")};
    return I>=0 && I<8?N[I]:TEXT("上層レジデンス");
}
FString EWHotelPlan::Description(int32 I)
{
    static const TCHAR* N[]={
        TEXT("木の湯船、障子、畳の読書席。静かな和のスイート。"),
        TEXT("暮らしの中に彫刻とアートを置く、緑豊かなギャラリー。"),
        TEXT("水庭と木の格子、深い緑。リゾートのように過ごす部屋。"),
        TEXT("砂色の石と天蓋、琥珀色の灯りに包まれる隠れ家。"),
        TEXT("真珠色の壁と細い金の装飾、シャンデリアのサロン。"),
        TEXT("濃い木、本棚、暖炉。都市の夜景と読書を楽しむ部屋。"),
        TEXT("立体的な窓枠と鮮やかな織物。光とアートのロフト。"),
        TEXT("梁のある天井、石の暖炉、雲を望む山荘のリビング。")};
    return I>=0 && I<8?N[I]:FString();
}
const EW::InteriorRoom* EWHotelPlan::Find(const EW::ChunkRecipe& R,int32 I)
{return R.Interiors.FindByPredicate([I](const EW::InteriorRoom& V){return V.Kind==11+I;});}
void EWHotelPlan::AddInfrastructure(EW::ChunkRecipe& R)
{
    if(R.Coord!=EW::ChunkCoord{0,0} || R.World.Seed!=EW::WorldDescriptor::ReferenceWorld().Seed)return;
    TArray<EW::CityWalkFloor> Roofs;
    for(const auto& F:R.CityFloors)if(F.bRoof)Roofs.Add(F);
    Roofs.Sort([](const auto& A,const auto& B){const double D=A.Transform.GetLocation().Z-B.Transform.GetLocation().Z;return FMath::Abs(D)>.01?D>0:A.Column<B.Column;});
    // The tallest original tower is reserved for the open-air theatre.
    if(Roofs.Num()<3)return;
    for(int32 Tower=0;Tower<2;++Tower)
    {
        const auto Roof=Roofs[Tower+1];const FString Id=FString::Printf(TEXT("0,0/%d"),Roof.Column);
        auto* Lift=R.Lifts.FindByPredicate([&](const auto& L){return L.Id==Id;});if(!Lift || Lift->Stops.IsEmpty())continue;
        const auto Original=Lift->Stops.Last();
        for(int32 Level=0;Level<2;++Level)
        {
            // The copper cupolas reach 12 m, above the old roof collision boxes.
            const FTransform Deck(Roof.Transform.GetLocation()+FVector(0,0,1450+Level*500));
            auto Part=[&](FName Mesh,FVector P,FVector Scale=FVector(1),double Yaw=0.)
            {R.Parts.Add({Mesh,FTransform(FRotator(0,Yaw,0),Deck.TransformPosition(P),Scale),0});};
            auto Box=[&](FVector P,FVector E,bool Floor=false)
            {R.Colliders.Add({FTransform(Deck.TransformPosition(P)),E,Floor});};
            // The wider sky deck must leave a real shaft for the existing car.
            // Four rectangles subtract its well instead of passing a solid slab through it.
            const FVector Car=Deck.InverseTransformPosition(FVector(Lift->Cabin.X,Lift->Cabin.Y,Deck.GetLocation().Z));
            const double X0=FMath::Clamp(Car.X-235.,-2350.,2350.),X1=FMath::Clamp(Car.X+235.,-2350.,2350.);
            const double Y0=FMath::Clamp(Car.Y-235.,-2350.,2350.),Y1=FMath::Clamp(Car.Y+235.,-2350.,2350.);
            auto Slab=[&](double A,double B,double C,double D){if(C<=A || D<=B)return;
                Part(TEXT("StoneDeck"),FVector((A+C)*.5,(B+D)*.5,-4),FVector((C-A)/100.,(D-B)/100.,1));
                Box(FVector((A+C)*.5,(B+D)*.5,-20),FVector((C-A)*.5,(D-B)*.5,16),true);};
            Slab(-2350,-2350,X0,2350);Slab(X1,-2350,2350,2350);Slab(X0,-2350,X1,Y0);Slab(X0,Y1,X1,2350);
            Part(TEXT("Hotel83Hall"),FVector(0,0,-4));
            for(double Y:{-1980.,1980.})Box(FVector(0,Y,41),FVector(300,40,45));
            for(int32 Side=0;Side<2;++Side)
            {
                const int32 I=Tower*4+Level*2+Side;const double Yaw=Side==0?90:-90;
                const FTransform T(FRotator(0,Yaw,0),Deck.TransformPosition(FVector(Side==0?-850:850,0,0)));
                R.Interiors.Add({T,11+I,I});
                for(const FName Mesh:{FName(*FString::Printf(TEXT("Hotel83Shell%d"),I)),FName(*FString::Printf(TEXT("Hotel83Furniture%d"),I)),FName(TEXT("Hotel83Glass"))})R.Parts.Add({Mesh,T,0});
                auto RoomBox=[&](FVector P,FVector E,bool Floor){R.Colliders.Add({FTransform(T.GetRotation(),T.TransformPosition(P)),E,Floor});};
                RoomBox(FVector(0,0,-2),FVector(752,602,2),true);
                RoomBox(FVector(0,800,-2.55),FVector(760,200,2),true);
                for(const auto& B:EWHotel83Data::Bodies(11+I))RoomBox(B.P,B.E,B.Floor);
            }
            auto Stop=Original;const double Dz=Deck.GetLocation().Z-Stop.Height;
            Stop.Height+=Dz;Stop.Landing.Z+=Dz;Stop.Entry.Z+=Dz;Stop.Threshold.Z+=Dz;Stop.Floor+=2+Level;
            Stop.Label=FString::Printf(TEXT("雲上レジデンス %s　%d–%d号室"),Level==0?TEXT("下層"):TEXT("上層"),101+Tower*4+Level*2,102+Tower*4+Level*2);
            Lift->Stops.Add(Stop);
            const FVector A=Deck.InverseTransformPosition(Stop.Entry),B=Deck.InverseTransformPosition(Stop.Threshold),D=B-A;const FRotator Q=D.Rotation();
            Part(TEXT("StoneDeck"),(A+B)*.5,FVector(D.Size()/100.,3.,1),Q.Yaw);
            R.Colliders.Add({FTransform(Q,Deck.TransformPosition((A+B)*.5-FVector(0,0,16))),FVector(D.Size()*.5,150,16),true});
            const bool XSide=FMath::Abs(A.X)>FMath::Abs(A.Y);const double Opening=XSide?A.Y:A.X;
            const int32 OpenSide=XSide?(A.X>0?1:3):(A.Y>0?2:0);
            // Guard the three sides of the shaft; the car's normal gate guards boarding.
            const FVector Toward=(A-Car).GetSafeNormal2D();
            for(int32 Edge=0;Edge<4;++Edge)
            {
                const FRotator Rotation(0,Edge*90.,0);const FVector Out=Rotation.RotateVector(FVector(0,-1,0));
                if(FVector::DotProduct(Out,Toward)>.5)continue;
                const FVector Pos=Car+Out*242+FVector(0,0,-4);
                Part(TEXT("StoneRail"),Pos,FVector(1.21,.65,1.1),Rotation.Yaw);
                R.Colliders.Add({FTransform(Rotation,Deck.TransformPosition(Pos+FVector(0,0,60))),FVector(242,10,60),false});
            }
            for(int32 Edge=0;Edge<4;++Edge)
            {
                const FRotator Rotation(0,Edge*90.,0);const double Gap=(Edge==2||Edge==3)?-Opening:Opening;
                auto Rail=[&](double Start,double End){if(End<=Start)return;const FVector Pos=Rotation.RotateVector(FVector((Start+End)*.5,-2325,-4));
                    Part(TEXT("StoneRail"),Pos,FVector((End-Start)/400.,.65,1.1),Rotation.Yaw);
                    R.Colliders.Add({FTransform(Rotation,Deck.TransformPosition(Pos+FVector(0,0,60))),FVector((End-Start)*.5,10,60),false});};
                TArray<FVector2D> Spans{FVector2D(-2325,2325)};
                auto Cut=[&](double C){TArray<FVector2D> Next;for(const auto S:Spans){if(C-175>S.X)Next.Add({S.X,FMath::Min(S.Y,C-175)});if(C+175<S.Y)Next.Add({FMath::Max(S.X,C+175),S.Y});}Spans=Next;};
                if(Edge==OpenSide)Cut(Gap);
                if(Edge==1)Cut(Tower==0?1700:-1700); // upper-line hotel concourse
                for(const auto S:Spans)Rail(S.X,S.Y);
            }
        }
        // A mast extension has the same cross section as the existing lift.
        R.Parts.Add({TEXT("UrbanLiftMast"),FTransform(Lift->Rotation,FVector(Lift->Cabin.X,Lift->Cabin.Y,Original.Height),FVector(1,1,.65)),1});
    }
}
