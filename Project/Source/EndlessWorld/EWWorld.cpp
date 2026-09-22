#include "EWWorld.h"
#include "EWSkyrailPlan.h"
#include "EWSkyTheatrePlan.h"
#include "EWAeroYachtPlan.h"
#include "EWHotelPlan.h"
#include "EWExplorationPlan.h"
#include "EWOuterWater.h"
#include "EWWaterCity.h"
#include "EWCityAccess.h"
#include "EWPoolPlan.h"
#include "EWCraft95.h"
#include "Hash/Blake3.h"
#include "String/LexFromString.h"

namespace EW
{
namespace
{
void Append64(TArray<uint8>& Bytes, uint64 Value)
{
    for (int32 I = 0; I < 8; ++I) Bytes.Add(uint8(Value >> (I * 8)));
}

void AppendString(TArray<uint8>& Bytes, const FString& Value)
{
    FTCHARToUTF8 Text(*Value);
    Append64(Bytes, Text.Length());
    Bytes.Append(reinterpret_cast<const uint8*>(Text.Get()), Text.Length());
}

void AppendVector(TArray<uint8>& Bytes, const FVector& Value)
{
    Append64(Bytes, int64(FMath::RoundToDouble(Value.X * 10000.0)));
    Append64(Bytes, int64(FMath::RoundToDouble(Value.Y * 10000.0)));
    Append64(Bytes, int64(FMath::RoundToDouble(Value.Z * 10000.0)));
}

bool ParseInteger(const FString& Text, int64& Value)
{
    if (Text.IsEmpty() || Text.Len() > 16) return false;
    for (int32 I = 0; I < Text.Len(); ++I)
        if (!(Text[I] >= '0' && Text[I] <= '9') && !(I == 0 && Text[I] == '-' && Text.Len() > 1))
            return false;
    return LexTryParseString(Value, *Text);
}

FTransform SegmentTransform(const FVector& A, const FVector& B)
{
    const FVector Delta = B - A;
    const FRotator Rotation = FRotationMatrix::MakeFromXZ(Delta.GetSafeNormal(), FVector::UpVector).Rotator();
    return FTransform(Rotation, (A + B) * 0.5);
}

void AddPart(ChunkRecipe& R, const TCHAR* Mesh, const FVector& Position, const FVector& Scale = FVector::OneVector,
             double Yaw = 0, uint8 Detail = 1)
{
    R.Parts.Add({FName(Mesh), FTransform(FRotator(0, Yaw, 0), Position, Scale), Detail});
}

void AddBox(ChunkRecipe& R, const FVector& Top, const FVector& Extent, double Yaw, bool Walk)
{
    R.Colliders.Add({FTransform(FRotator(0, Yaw, 0), Top - FVector(0, 0, Extent.Z)), Extent, Walk});
}

void AddFloor(ChunkRecipe& R, const FVector& Top, double X, double Y, double Yaw = 0, const TCHAR* Mesh = TEXT("StoneDeck"))
{
    AddPart(R, Mesh, Top, FVector(X / 100, Y / 100, 1), Yaw, 0);
    AddBox(R, Top, FVector(X * .5, Y * .5, 20), Yaw, true);
}

void AddRoad(ChunkRecipe& R, FVector A, FVector B, double Width, bool Rail = true)
{
    if ((A - B).SizeSquared() < 1) return;
    R.Paths.Add({A, B, Width});
    const FTransform Transform = SegmentTransform(A, B);
    const double Length = (B - A).Size();
    const FVector Normal = Transform.GetRotation().GetUpVector();
    R.Colliders.Add({FTransform(Transform.GetRotation(), Transform.GetLocation() - Normal * 20),
                     FVector(Length * .5 + 3, Width * .5, 20), true});
    const int32 Segments = FMath::Max(1, FMath::CeilToInt(Length / 800));
    for (int32 I = 0; I < Segments; ++I)
    {
        const FVector P = FMath::Lerp(A, B, (I + .5) / Segments);
        const TCHAR* Deck = R.Region == RegionKind::Garden ? TEXT("WoodDeck") :
            R.Region == RegionKind::Crystal ? TEXT("RockDeck") : TEXT("StoneDeck");
        R.Parts.Add({FName(Deck), FTransform(Transform.GetRotation(), P,
                     FVector((Length / Segments + 1) / 100, Width / 100, 1)), 0});
        if (Rail)
        {
            const auto IsBoundary = [](const FVector& Q)
            { return Q.X <= .1 || Q.Y <= .1 || Q.X >= ChunkSize - .1 || Q.Y >= ChunkSize - .1; };
            // Leave open junction aprons, including the full width of a crossing route.
            // This clips both the visible rail and its collider; shared chunk edges keep continuous rails.
            const double TrimA = IsBoundary(A) ? 0 : 500;
            const double TrimB = IsBoundary(B) ? 0 : 500;
            const double RailA = FMath::Max(Length * I / Segments, TrimA);
            const double RailB = FMath::Min(Length * (I + 1) / Segments, Length - TrimB);
            if (RailB - RailA < 40) continue;
            const double RailLength = RailB - RailA;
            const FVector RailCentre = FMath::Lerp(A, B, (RailA + RailB) * .5 / Length);
            const FVector Side = Transform.GetRotation().GetRightVector() * (Width * .5 - 18);
            for (double Sign : {-1.0, 1.0})
            {
                const FVector RP = RailCentre + Side * Sign;
                R.Parts.Add({FName(R.Region == RegionKind::Garden ? TEXT("WoodRail") : TEXT("StoneRail")),
                            FTransform(Transform.GetRotation(), RP, FVector(RailLength / 400, 1, 1)), 1});
                // The dedicated rail box is independent of Nanite's render fallback.
                R.Colliders.Add({FTransform(Transform.GetRotation(), RP + Normal * 48),
                                  FVector(RailLength * .5, 12, 48), false});
            }
        }
    }
}

FVector Turn(const FVector& P, double Yaw)
{
    return FRotator(0, Yaw, 0).RotateVector(P);
}

void Building(ChunkRecipe& R, FVector P, double Width, double Depth, int32 Floors, double Yaw, uint64 Variant)
{
    const double H = Floors * 400.0;
    auto PartAt = [&](const TCHAR* Mesh, FVector Offset, FVector Scale, uint8 Detail = 1)
    {
        AddPart(R, Mesh, P + Turn(Offset, Yaw), Scale, Yaw, Detail);
    };
    auto Wall = [&](FVector Centre, FVector Extent)
    {
        PartAt(TEXT("Wall"), Centre, Extent * .02);
        R.Colliders.Add({FTransform(FRotator(0, Yaw, 0), P + Turn(Centre, Yaw)), Extent, false});
    };
    AddFloor(R, P, Width + 150, Depth + 150, Yaw);
    PartAt(TEXT("Island"), FVector(0, 0, -35), FVector((Width + 350) / 4000, (Depth + 350) / 4000, .65), 0);
    Wall(FVector(-Width * .5 + 12, 0, H * .5), FVector(12, Depth * .5, H * .5));
    Wall(FVector(Width * .5 - 12, 0, H * .5), FVector(12, Depth * .5, H * .5));
    Wall(FVector(0, Depth * .5 - 12, H * .5), FVector(Width * .5, 12, H * .5));
    const double PierWidth = FMath::Max(100.0, (Width - 250) * .5);
    for (double S : {-1.0, 1.0})
        Wall(FVector(S * (125 + PierWidth * .5), -Depth * .5 + 12, H * .5), FVector(PierWidth * .5, 12, H * .5));
    Wall(FVector(0, -Depth * .5 + 12, 270 + (H - 270) * .5), FVector(125, 12, (H - 270) * .5));
    PartAt(TEXT("DoorArch"), FVector(0, -Depth * .5 - 18, 0), FVector(1, 1, 1), 2);
    for (int32 Level = 0; Level < Floors; ++Level)
    {
        for (int32 I = 0; I < FMath::Max(2, int32(Width / 350)); ++I)
        {
            const double X = -Width * .5 + 180 + I * (Width - 360) / FMath::Max(1, int32(Width / 350) - 1);
            if (Level == 0 && FMath::Abs(X) < 180) continue;
            PartAt(TEXT("Window"), FVector(X, -Depth * .5 - 20, 80 + Level * 400), FVector(1, 1, 1), 2);
        }
        PartAt(TEXT("Cornice"), FVector(0, 0, (Level + 1) * 400 - 20),
               FVector((Width + 70) / 100, (Depth + 70) / 100, 1), 1);
        if (Level > 0 && ((Variant >> (Level + 3)) & 1))
            PartAt(*FString::Printf(TEXT("Balcony_%d"), int32((Variant + Level) % 4)),
                   FVector(0, -Depth * .5 - 65, Level * 400), FVector(FMath::Min(1.4, Width / 450), 1, 1), 2);
        for (int32 I = 0; I < 3; ++I)
            AddPart(R, TEXT("Window"), P + Turn(FVector(-Width*.3+I*Width*.3, Depth*.5+20, 80+Level*400), Yaw),
                    FVector(1,1,1), Yaw+180, 2);
        for (double Side : {-1.,1.})
            for (int32 I=0;I<3;++I)
                AddPart(R,TEXT("Window"),P+Turn(FVector(Side*(Width*.5+20),-Depth*.3+I*Depth*.3,80+Level*400),Yaw),
                        FVector(1,1,1),Yaw+Side*90,2);
    }
    for (double X : {-1.0, 1.0})
        for (double Y : {-1.0, 1.0})
            PartAt(TEXT("Column"), FVector(X * (Width * .5 - 25), Y * (Depth * .5 - 25), 0), FVector(.7, .7, H / 400), 1);
    PartAt((Variant % 3) == 0 ? TEXT("DomeRoof") : (Variant % 3) == 1 ? TEXT("GableRoof") : TEXT("FlatRoof"),
           FVector(0, 0, H), FVector((Width + 120) / 1000, (Depth + 120) / 1000, 1), 0);
    if (Variant % 5 == 0)
        PartAt(*FString::Printf(TEXT("Arcade_%d"), int32(Variant % 4)), FVector(0, Depth*.1, H+20), FVector(.75,.75,.75), 1);
    PartAt(TEXT("Bookshelf"), FVector(-Width * .5 + 120, Depth * .5 - 100, 0), FVector(1, 1, 1), 2);
    PartAt(TEXT("TableSet"), FVector(0, Depth * .22, 0), FVector(1, 1, 1), 2);
    for (double S : {-1.0, 1.0})
        PartAt(TEXT("Planter"), FVector(S * (Width * .5 - 120), -Depth * .5 - 150, 0), FVector(1, 1, 1), 2);
}

void Landmark(ChunkRecipe& R, int32 Kind, uint64 V)
{
    FVector P = R.Hub;
    switch (Kind)
    {
    case 0: // clock tower, offset keeps the centre of the square accessible
        P += FVector(650, 650, 0);
        AddPart(R, TEXT("ClockTower"), P, FVector(.52, .52, .32 + (V % 4) * .035), 0, 0);
        AddBox(R, P + FVector(0, 0, 760), FVector(190, 190, 380), 0, false);
        break;
    case 1:
        AddPart(R, V%2 ? *FString::Printf(TEXT("Arcade_%d"), int32(V%4)) : TEXT("LibraryPavilion"), P + FVector(0, 700, 0), FVector(1, 1, 1), 0, 0);
        for (int32 I = -2; I <= 2; ++I)
        {
            AddPart(R, TEXT("Bookshelf"), P + FVector(I * 280, 1250, 0), FVector(1, 1, 1), 0, 2);
            AddBox(R, P + FVector(I * 280, 1250, 220), FVector(120, 50, 110), 0, false);
        }
        break;
    case 2:
        AddPart(R, TEXT("Airship"), P + FVector(400, 1900, 950), FVector(1, 1, 1), 15, 0);
        AddPart(R, TEXT("MooringMast"), P + FVector(650, 1050, 0), FVector(1, 1, 1), 0, 1);
        break;
    case 3:
        AddPart(R, V%2 ? TEXT("Tree_Fan") : TEXT("Tree_Spiral"), P + FVector(700, 450, 0), FVector(1, 1, 1.1), V % 360, 0);
        AddPart(R, TEXT("CanopyLookout"), P + FVector(0, 1100, 750), FVector(1, 1, 1), 0, 1);
        AddFloor(R, P + FVector(0, 1100, 750), 1400, 1000, 0, TEXT("WoodDeck"));
        AddRoad(R, P + FVector(-1100, -850, 0), P + FVector(1000, -850, 300), 350);
        AddRoad(R, P + FVector(1000, -850, 300), P + FVector(1000, 600, 750), 350);
        AddRoad(R, P + FVector(1000, 600, 750), P + FVector(1000, 1100, 750), 350);
        // Reach the landing at its height before crossing the platform's edge.
        AddRoad(R, P + FVector(1000, 1100, 750), P + FVector(400, 1100, 750), 350);
        AddBox(R, P + FVector(700, 450, 5000), FVector(430, 430, 2500), 0, false);
        break;
    case 4:
        AddPart(R, V%2 ? TEXT("Tree_Willow") : TEXT("Tree_Spiral"), P + FVector(0, 1000, 0), FVector(1.35, 1.35, .8), V % 360, 0);
        AddPart(R, TEXT("TreeShrine"), P + FVector(0, 300, 0), FVector(1, 1, 1), 0, 1);
        AddBox(R, P + FVector(0, 1000, 4000), FVector(550, 550, 2000), 0, false);
        break;
    case 5:
        AddPart(R, TEXT("LuminousPond"), P + FVector(0, 450, 3), FVector(1, 1, 1), 0, 1);
        AddPart(R, V%2 ? TEXT("Tree_Willow") : TEXT("Tree_Blossom"), P + FVector(1600, 1700, 0), FVector(.8, .8, 1), V % 360, 0);
        break;
    case 6:
        AddPart(R, TEXT("CrystalSpire"), P + FVector(450, 650, 0), FVector(1.2, 1.2, 1.1 + (V % 5) * .1), V % 360, 0);
        AddBox(R, P + FVector(450, 650, 2800), FVector(400, 400, 1400), V % 360, false);
        break;
    case 7:
        AddPart(R, TEXT("CrystalCave"), P + FVector(0, 650, 0), FVector(1, 1, 1), 0, 0);
        // The cave has an open front and dedicated solid side walls.
        for (double S : {-1.0, 1.0})
            AddBox(R, P + FVector(S * 700, 800, 650), FVector(180, 700, 325), 0, false);
        break;
    case 8:
        for (int32 I = 0; I < 9; ++I)
        {
            double A = I * UE_DOUBLE_TWO_PI / 9;
            AddPart(R, TEXT("StandingStone"), P + FVector(FMath::Cos(A) * 1350, FMath::Sin(A) * 1350, 0),
                    FVector(1, 1, .8 + .1 * (I % 4)), I * 40, 1);
        }
        AddPart(R, TEXT("CrystalCluster"), P + FVector(650, 300, 0), FVector(1.5, 1.5, 1.5), 30, 1);
        break;
    default: break;
    }
    FVector Record = R.Hub + FVector(0, -1250, 0);
    AddPart(R, TEXT("Wayfinder"), Record + FVector(200, 0, 0), FVector(1, 1, 1), 0, 2);
    PlaceBookmark Place;
    Place.WorldCode = R.World.Code(); Place.Coord = R.Coord; Place.Kind = Kind;
    Place.Id = HashText(Place.WorldCode + TEXT("|place|") + R.Coord.Text()).Left(32);
    Place.Name = PlaceName(Kind);
    Place.LocalPosition = Record;
    R.Places.Add(MoveTemp(Place));
}

void Populate(ChunkRecipe& R)
{
    const auto& W = R.World;
    const int64 X = R.Coord.X, Y = R.Coord.Y;
    const uint64 V = W.Number("landmark", {X, Y});
    const int32 Kind = int32(R.Region) * 3 + int32(V % 3);
    if (R.Region==RegionKind::City)
    {
        const FVector P=R.Hub;
        AddFloor(R,P,440,440);
        if(Kind==0)
        {
            AddPart(R,TEXT("ClockTower"),P,FVector(.42,.42,.32),0,0);
            AddBox(R,P+FVector(0,0,760),FVector(160,160,380),0,false);
        }
        else if(Kind==1) AddPart(R,TEXT("Fountain_0"),P,FVector(.65),0,1);
        else
        {
            AddPart(R,TEXT("MooringMast"),P,FVector(.6),0,1);
            AddPart(R,TEXT("Airship"),P+FVector(0,0,1200),FVector(1),15,0);
        }
        const FVector Record=P+FVector(0,-1250,0);
        AddPart(R,TEXT("Wayfinder"),Record+FVector(200,0,0),FVector(1),0,2);
        PlaceBookmark Place;Place.WorldCode=W.Code();Place.Coord=R.Coord;Place.Kind=Kind;
        Place.Id=HashText(Place.WorldCode+TEXT("|place|")+R.Coord.Text()).Left(32);
        Place.Name=PlaceName(Kind);Place.LocalPosition=Record;R.Places.Add(Place);
        return;
    }
    if (R.Region==RegionKind::City && Kind!=0)
        AddFloor(R,R.Hub+FVector(0,650,0),2400,1800);
    Landmark(R, Kind, V);
    if (R.Region == RegionKind::City && Kind != 0)
        AddPart(R, *FString::Printf(TEXT("Fountain_%d"), int32(V%3)), R.Hub + FVector(-850,-100,0), FVector(1,1,1), 0, 1);
    TArray<TPair<FVector, double>> Occupied;
    Part Skyline;
    if (ScenicPart(W,R.Coord,Skyline))
        Occupied.Add({Skyline.Transform.GetTranslation(),2800});
    // Buildings and large plants are placed only after reserving all route footprints.
    for (int32 I = 0; I < 36 && Occupied.Num() < 7; ++I)
    {
        // In the city the adjoining multi-storey blocks define the streets.
        // Do not place detached cottages through their occupied facades.
        if (R.Region == RegionKind::City) break;
        uint64 A = W.Number("building-position", {X, Y, I});
        uint64 B = W.Number("building-form", {X, Y, I});
        FVector P(1000 + A % 10801, 1000 + (A >> 24) % 10801, R.Hub.Z);
        const double Width = 800 + B % 801, Depth = 800 + (B >> 16) % 601;
        // Nature meshes have wider footprints than the old cottage dimensions.
        // Keep crystal formations apart and the garden's arrival view open.
        const double Radius = R.Region==RegionKind::Crystal ? 2000. :
            R.Region==RegionKind::Garden ? 1400. : FMath::Sqrt(Width * Width + Depth * Depth) * .5 + 120;
        if (FVector::Dist2D(P, R.Hub) < 2700 + Radius || FVector::Dist2D(P, R.Hub) > 5400) continue;
        bool Clear = true;
        for (const auto& Path : R.Paths)
            if (DistanceToSegment2D(P, Path.A, Path.B) < Radius + Path.Width * .5 + 80) { Clear = false; break; }
        for (const auto& O : Occupied)
            if (FVector::Dist2D(P, O.Key) < Radius + O.Value + 120) Clear = false;
        if (!Clear) continue;
        Occupied.Add({P, Radius});
        const double Yaw = FMath::RoundToDouble(((R.Hub - P).Rotation().Yaw + 90) / 90) * 90;
        if (R.Region == RegionKind::City)
        {
            Building(R, P, Width, Depth, 1 + (B >> 30) % 4, Yaw, B);
            // A clear spur joins each entrance to the closest main route.
            FVector Door = P + Turn(FVector(0, -Depth * .5 + 30, 0), Yaw);
            FVector Best = R.Hub; double Distance = DBL_MAX;
            for (const auto& Path : R.Paths)
            {
                FVector Delta = Path.B - Path.A;
                double T = FMath::Clamp(FVector::DotProduct(Door - Path.A, Delta) / Delta.SizeSquared(), 0., 1.);
                FVector Q = Path.A + Delta * T;
                if ((Q - Door).SizeSquared() < Distance) { Distance = (Q - Door).SizeSquared(); Best = Q; }
            }
            AddRoad(R, Best, Door, 300, false);
        }
        else if (R.Region == RegionKind::Garden)
        {
            AddPart(R, *FString::Printf(TEXT("RootIsland_%d"), int32(B%4)), P - FVector(0, 0, 200), FVector(Width / 1700, Depth / 1700, 1), Yaw, 0);
            const TCHAR* Trees[] = {TEXT("GiantTree"), TEXT("Tree_Willow"), TEXT("Tree_Fan"), TEXT("Tree_Blossom"), TEXT("Tree_Spiral")};
            AddPart(R, Trees[(B>>8)%5], P,
                    FVector(.6 + (B % 4) * .2, .6 + (B % 4) * .2, .55 + ((B >> 10) % 6) * .13), Yaw, 0);
            AddPart(R, *FString::Printf(TEXT("ForestVerge_%d"), int32((B>>20)%3)),
                    P + FVector(400, 0, 0), FVector(.85, .85, .85), Yaw, 2);
        }
        else
        {
            AddPart(R, TEXT("Island"), P - FVector(0, 0, 150), FVector(Width / 1800, Depth / 1800, .7), Yaw, 0);
            const FString Formation = (B % 3) == 0 ? FString::Printf(TEXT("WeatheredRock_%d"), int32((B>>6)%5)) :
                FString::Printf(TEXT("CrystalGarden_%d"), int32((B>>6)%5));
            AddPart(R, *Formation, P, FVector(1.6+(B%4)*.2, 1.6+(B%4)*.2, 1.5+((B>>5)%4)*.4), Yaw, 0);
            AddPart(R, TEXT("CrystalShards"), P + FVector(600, -400, 0), FVector(2, 2, 2), Yaw, 2);
        }
    }
    // Small repeated pieces are batched into per-mesh instance components at runtime.
    const int32 MainPathCount = FMath::Min(R.Region==RegionKind::Garden?24:R.Region==RegionKind::City?8:10, R.Paths.Num());
    for (int32 I = 0; I < MainPathCount; ++I)
    {
        const PathSegment Path = R.Paths[I];
        FVector Mid = (Path.A + Path.B) * .5;
        FVector Side = FVector::CrossProduct((Path.B - Path.A).GetSafeNormal(), FVector::UpVector);
        const double Yaw = (Path.B - Path.A).Rotation().Yaw;
        uint64 Vary = W.Number("details", {X, Y, I});
        for (double S : {-1., 1.})
        {
            if (R.Region==RegionKind::Garden && ((Vary>>(S<0?4:12))%4)==0) continue;
            // A small supported terrace keeps furniture and plants out of the reserved route.
            const FVector Along = (Path.B - Path.A).GetSafeNormal2D();
            const double GardenOffset=double(int32((Vary>>20)%241)-120);
            const FVector Terrace = Mid + Side * (Path.Width * .5 + (R.Region==RegionKind::Garden?160:250)) * S +
                (R.Region==RegionKind::Garden?Along*GardenOffset:FVector::ZeroVector);
            bool ClearApproach=true;
            for (const auto& Approach : R.Paths)
                if ((FMath::Abs(Approach.A.Z-Terrace.Z)>10 || FMath::Abs(Approach.B.Z-Terrace.Z)>10) &&
                    DistanceToSegment2D(Terrace,Approach.A,Approach.B)<550+Approach.Width*.5+80)
                { ClearApproach=false; break; }
            if (!ClearApproach) continue;
            const TCHAR* Deck = R.Region == RegionKind::Garden ? TEXT("WoodDeck") : R.Region == RegionKind::Crystal ? TEXT("RockDeck") : TEXT("StoneDeck");
            if (R.Region!=RegionKind::Garden)
            {
                AddFloor(R, Terrace, 900, 600, Yaw, Deck);
                R.Parts.Last().Detail = 2;
            }
            FVector P = Terrace + Side * 80 * S;
            if (R.Region == RegionKind::City)
            {
                AddPart(R, TEXT("Lamp"), P, FVector::OneVector, Yaw, 2);
                AddPart(R, TEXT("Planter"), P + Along * 260, FVector(1, 1, 1), Yaw, 2);
                if ((Vary % 3) == 0) AddPart(R, TEXT("Bench"), P - Along * 180, FVector::OneVector, Yaw, 2);
            }
            else if (R.Region == RegionKind::Garden)
            {
                const double Size=.78+double((Vary>>28)%23)*.01;
                AddPart(R, *FString::Printf(TEXT("ForestVerge_%d"), int32((Vary>>8)%3)),
                        P,FVector(Size,Size,1),Vary%360,2);
                // A small contact surface stays inside the shelf's solid soil.
                AddBox(R,P,FVector(100,100,20),Yaw,true);
                if ((Vary>>36)%3==0)
                    AddPart(R,*FString::Printf(TEXT("Wildflower_%d"),int32(Vary%4)),
                            P+Along*90,FVector(.28,.28,.40),Vary%360,2);
            }
            else
            {
                AddPart(R, TEXT("CrystalShards"), P, FVector(1.3, 1.3, 1.3), Vary % 360, 2);
            }
        }
    }
}
}

FString HashText(const FString& Text)
{
    FTCHARToUTF8 Bytes(*Text);
    return LexToString(FBlake3::HashBuffer(Bytes.Get(), Bytes.Length())).ToLower();
}

int64 FloorDiv(int64 V, int64 D)
{
    check(D > 0);
    int64 Q = V / D;
    return Q - (V % D < 0 ? 1 : 0);
}

bool WorldDescriptor::Valid() const
{
    if (Seed.Len() != 32) return false;
    for (TCHAR C : Seed) if (!((C >= '0' && C <= '9') || (C >= 'a' && C <= 'f'))) return false;
    return true;
}

FString WorldDescriptor::Code() const { return TEXT("ew2-kit1-") + Seed; }
WorldDescriptor WorldDescriptor::NewWorld() { return {FGuid::NewGuid().ToString(EGuidFormats::Digits).ToLower()}; }
WorldDescriptor WorldDescriptor::ReferenceWorld() { return {TEXT("c286c8776ab548d9b0c1e4a9d2f18fce")}; }

bool WorldDescriptor::Parse(const FString& Input, WorldDescriptor& Out, FString& Error)
{
    FString Code = Input.TrimStartAndEnd();
    if (Code.Len() != 41 || !Code.StartsWith(TEXT("ew2-kit1-")))
    {
        Error = TEXT("この世界コードの形式・生成器・素材の版には対応していません。ew2-kit1 のコードを入力してください。");
        return false;
    }
    WorldDescriptor W{Code.Mid(9)};
    if (!W.Valid()) { Error = TEXT("世界番号は半角の小文字・数字32文字で指定してください。"); return false; }
    Out = MoveTemp(W); Error.Empty(); return true;
}

uint64 WorldDescriptor::Number(const ANSICHAR* Domain, std::initializer_list<int64> Values) const
{
    TArray<uint8, TInlineAllocator<192>> Bytes;
    auto Write = [&](uint64 Value) { for (int32 I = 0; I < 8; ++I) Bytes.Add(uint8(Value >> (I * 8))); };
    Write(GeneratorVersion); Write(CatalogueVersion);
    for (int32 I = 0; I < 32; I += 2)
    {
        auto N = [](TCHAR C) { return C <= '9' ? C - '0' : C - 'a' + 10; };
        Bytes.Add(uint8((N(Seed[I]) << 4) | N(Seed[I + 1])));
    }
    const int32 Len = FCStringAnsi::Strlen(Domain);
    Write(Len); Bytes.Append(reinterpret_cast<const uint8*>(Domain), Len); Write(Values.size());
    for (int64 V : Values) Write(uint64(V));
    const FBlake3Hash Hash = FBlake3::HashBuffer(Bytes.GetData(), Bytes.Num());
    uint64 Result = 0;
    for (int32 I = 0; I < 8; ++I) Result |= uint64(Hash.GetBytes()[I]) << (I * 8);
    return Result;
}

double HeightAt(const WorldDescriptor& W, int64 X, int64 Y)
{
    const int64 Span = ChunkSize * 8;
    const int64 CX = FloorDiv(X, Span), CY = FloorDiv(Y, Span);
    const int64 RX = X - CX * Span, RY = Y - CY * Span;
    const int64 H00 = W.Number("height", {CX, CY}) % 4801;
    const int64 H10 = W.Number("height", {CX + 1, CY}) % 4801;
    const int64 H01 = W.Number("height", {CX, CY + 1}) % 4801;
    const int64 H11 = W.Number("height", {CX + 1, CY + 1}) % 4801;
    return double(((H00 * (Span - RX) + H10 * RX) * (Span - RY) +
                   (H01 * (Span - RX) + H11 * RX) * RY) / (Span * Span));
}

RegionKind RegionAt(const WorldDescriptor& W, ChunkCoord C)
{
    // A deterministic arrival region puts all three landscapes within a short first walk.
    if (C.X >= -2 && C.X <= 2 && C.Y >= -2 && C.Y <= 2) return RegionKind::City;
    if (C.X >= 3 && C.X <= 8 && C.Y >= -2 && C.Y <= 2) return RegionKind::Garden;
    if (C.Y >= 3 && C.Y <= 8 && C.X >= -2 && C.X <= 2) return RegionKind::Crystal;
    const int64 BX = FloorDiv(C.X, 6), BY = FloorDiv(C.Y, 6);
    int64 Best = MAX_int64; ChunkCoord Site;
    for (int32 DY = -1; DY <= 1; ++DY)
        for (int32 DX = -1; DX <= 1; ++DX)
        {
            const int64 X = BX + DX, Y = BY + DY;
            const int64 PX = X * 6 + W.Number("region-x", {X, Y}) % 6;
            const int64 PY = Y * 6 + W.Number("region-y", {X, Y}) % 6;
            const int64 D = (C.X - PX) * (C.X - PX) + (C.Y - PY) * (C.Y - PY);
            if (D < Best || (D == Best && (X < Site.X || (X == Site.X && Y < Site.Y))))
            { Best = D; Site = {X, Y}; }
        }
    return RegionKind(W.Number("region-kind", {Site.X, Site.Y}) % 3);
}

FString RegionName(RegionKind R)
{
    static const TCHAR* Names[] = {TEXT("白い浮遊都市"), TEXT("巨大樹の庭園"), TEXT("結晶の荒野")};
    return Names[FMath::Clamp(int32(R), 0, 2)];
}

FString PlaceName(int32 Kind)
{
    if(Kind>=15 && Kind<=19)return EWOuterWater::Name(Kind);
    if(Kind>=9 && Kind<=14)return EWExplorationPlan::Name(Kind-9);
    static const TCHAR* Names[] = {TEXT("風時計の塔"), TEXT("回廊書庫"), TEXT("空の港"), TEXT("樹冠の展望台"),
        TEXT("古木の祠"), TEXT("光る池"), TEXT("巨大晶柱"), TEXT("晶洞"), TEXT("環状列石")};
    return Names[FMath::Clamp(Kind, 0, 8)];
}

FString PlaceDescription(int32 Kind)
{
    if(Kind>=15 && Kind<=19)return EWOuterWater::Description(Kind);
    if(Kind>=9 && Kind<=14)return EWExplorationPlan::Description(Kind-9);
    static const TCHAR* Text[] = {
        TEXT("雲を渡る風を数える時計。影が回廊を横切り、遠くの橋へ旅人を誘う。"),
        TEXT("空の航路と、まだ名のない土地の記録を収めた書庫。開いた窓から光が落ちる。"),
        TEXT("浮遊する島々を結ぶ小さな港。船の下には、どこまでも雲海が続いている。"),
        TEXT("葉の天蓋と同じ高さに伸びた見晴らしの場所。枝の向こうに次の道が見える。"),
        TEXT("太い根の内側に守られた祠。誰かが磨いた石段に、木漏れ日が揺れている。"),
        TEXT("古木の根元で静かに光をたたえる池。水辺の花が、道の縁を照らしている。"),
        TEXT("遠くからも目印になる結晶の柱。層になった色が空の光を映している。"),
        TEXT("岩に包まれた結晶の空洞。入口をくぐると、外の風景が額縁のように見える。"),
        TEXT("荒野に並ぶ石の輪。隙間の向こうに、別の島と細い回廊が重なって見える。")};
    return Text[FMath::Clamp(Kind, 0, 8)];
}

double DistanceToSegment2D(const FVector& P, const FVector& A, const FVector& B)
{
    const FVector2D D(B.X - A.X, B.Y - A.Y), V(P.X - A.X, P.Y - A.Y);
    const double T = D.SizeSquared() > 0 ? FMath::Clamp(FVector2D::DotProduct(V, D) / D.SizeSquared(), 0., 1.) : 0;
    return (V - D * T).Size();
}

ChunkRecipe GenerateChunk(const WorldDescriptor& W, ChunkCoord C)
{
    ChunkRecipe R; R.World = W; R.Coord = C;
    if (!W.Valid() || !C.Valid()) return R;
    R.Region = RegionAt(W, C);
    const uint64 H = W.Number("hub", {C.X, C.Y});
    R.Hub = FVector(6000 + H % 801, 6000 + (H >> 20) % 801, 0);
    if(R.Region==RegionKind::City) R.Hub=FVector(6400,6400,0);
    R.Hub.Z = HeightAt(W, C.X * ChunkSize + int64(R.Hub.X), C.Y * ChunkSize + int64(R.Hub.Y));
    if(R.Region==RegionKind::City)
    {
        const double Datum=FMath::FloorToDouble(HeightAt(W,0,0)/450.)*450.;
        R.Hub.Z=Datum+FMath::RoundToDouble((R.Hub.Z-Datum)/1800.)*1800.;
    }
    if (R.Region == RegionKind::Garden) R.Hub.Z += 600;
    for (int32 D = 0; D < 4; ++D)
    {
        int32 Axis = D < 2 ? 0 : 1;
        ChunkCoord B = C;
        if (D == 1) ++B.X;
        if (D == 3) ++B.Y;
        const double Offset = 4800 + W.Number("portal-offset", {Axis, B.X, B.Y}) % 3201;
        FVector P = Axis == 0 ? FVector(D == 0 ? 0 : ChunkSize, Offset, 0) :
                                     FVector(Offset, D == 2 ? 0 : ChunkSize, 0);
        P.Z = HeightAt(W, C.X * ChunkSize + int64(P.X), C.Y * ChunkSize + int64(P.Y));
        const double Width = 550 + W.Number("portal-width", {Axis, B.X, B.Y}) % 151;
        R.Portals.Add({Axis, B, P, Width});
    }
    if(EWOuterWater::Contains(W,C))
    {
        EWOuterWater::Build(R);
        EWSkyrailPlan::AddInfrastructure(R);
        FString Error;if(!R.Validate(Error)){R.bFallback=true;UE_LOG(LogTemp,Error,TEXT("CASCADE86_INVALID %s"),*Error);}
        return R;
    }
    const TCHAR* Floor = R.Region == RegionKind::Garden ? TEXT("GardenEarthDeck") :
        R.Region == RegionKind::Crystal ? TEXT("RockDeck") : TEXT("StoneDeck");
    if (R.Region==RegionKind::City) BuildCityGround(R);
    else
    {
        AddFloor(R, R.Hub, 4200, 4200, 0, Floor);
        const FString Base = R.Region == RegionKind::Garden ? FString::Printf(TEXT("RootIsland_%d"), int32(H%4)) : TEXT("Island");
        AddPart(R, *Base, R.Hub - FVector(0, 0, 35), FVector(1.17, 1.17, 1), H % 4 * 90, 0);
    TArray<FVector> Ring;
    int32 N = R.Region == RegionKind::Garden ? 24 : R.Region == RegionKind::Crystal ? 10 : 8;
    for (int32 I = 0; I < N; ++I)
    {
        const double A = I * UE_DOUBLE_TWO_PI / N;
        double Radius = R.Region == RegionKind::City ? 2800 :
            R.Region == RegionKind::Garden ? 2850 : 2400 + W.Number("ring-radius", {C.X, C.Y, I}) % 800;
        FVector P = R.Hub + FVector(FMath::Cos(A) * Radius, FMath::Sin(A) * Radius, 0);
        if (R.Region == RegionKind::City)
        {
            P.X = R.Hub.X + FMath::Clamp(FMath::Cos(A) * 4000, -2600., 2600.);
            P.Y = R.Hub.Y + FMath::Clamp(FMath::Sin(A) * 4000, -2600., 2600.);
        }
        Ring.Add(P);
    }
    for (int32 I = 0; I < N; ++I) AddRoad(R, Ring[I], Ring[(I + 1) % N], 550, false);
    for (const Portal& Port : R.Portals)
    {
        int32 Closest = 0; double Dist = DBL_MAX;
        for (int32 I = 0; I < N; ++I)
        {
            double V = FVector::DistSquared2D(Port.Position, Ring[I]);
            if (V < Dist) { Dist = V; Closest = I; }
        }
        FVector B = Ring[Closest];
        // The last segment at a shared edge is always perpendicular to that edge.
        FVector Inside = Port.Position;
        if (Port.Axis == 0) Inside.X += Port.Position.X < 1 ? 900 : -900;
        else Inside.Y += Port.Position.Y < 1 ? 900 : -900;
        // Finish climbing before the broad ring deck begins. Ending a ramp at
        // the deck centre creates a raised lip along the deck's outer edge.
        FVector Apron = B + (Inside-B).GetSafeNormal2D()*600;
        Apron.Z=B.Z;
        const double FirstRun=FVector::Dist2D(Port.Position,Inside);
        const double SecondRun=FVector::Dist2D(Inside,Apron);
        Inside.Z = FMath::Lerp(Port.Position.Z, B.Z, FirstRun/(FirstRun+SecondRun));
        AddRoad(R, Port.Position, Inside, Port.Width);
        AddRoad(R, Inside, Apron, Port.Width);
        AddRoad(R, Apron, B, Port.Width,false);
        FVector Entry = R.Hub + (B - R.Hub).GetSafeNormal2D() * 1650;
        Entry.Z = R.Hub.Z;
        AddRoad(R, B, Entry, 650, false);
    }
    }
    Populate(R);
    if (R.Region == RegionKind::City) { CityVolumeParts(W,C,R.Parts); BuildCityAccess(R); BuildWaterCity(R); }
    EWSkyTheatrePlan::AddInfrastructure(R);
    EWHotelPlan::AddInfrastructure(R);
    EWAeroYachtPlan::AddInfrastructure(R);
    EWSkyrailPlan::AddInfrastructure(R);
    EWExplorationPlan::Finish(R);
    EWPoolPlan::AddInfrastructure(R);
    EWCraft95::Apply(R);
    Part Scenery;
    if (ScenicPart(W, C, Scenery)) R.Parts.Add(Scenery);
    FString Error;
    if (!R.Validate(Error))
    {
        UE_LOG(LogTemp,Warning,TEXT("EW chunk %s fallback: %s (parts=%d colliders=%d interiors=%d)"),*C.Text(),*Error,R.Parts.Num(),R.Colliders.Num(),R.Interiors.Num());
        // A bounded deterministic fallback still honors the same four edge contracts.
        R.Parts.Reset(); R.Colliders.Reset(); R.Paths.Reset(); R.Places.Reset(); R.Lifts.Reset(); R.CityFloors.Reset(); R.Residences.Reset(); R.Interiors.Reset(); R.WaterPaths.Reset(); R.bWaterCity=false; R.bFallback = true;
        AddFloor(R, R.Hub, 4200, 4200, 0, Floor);
        AddPart(R, TEXT("Island"), R.Hub - FVector(0, 0, 35), FVector(1.17, 1.17, 1), 0, 0);
        for (const Portal& P : R.Portals) AddRoad(R, P.Position, R.Hub, P.Width);
        Landmark(R, int32(R.Region) * 3, H);
    }
    return R;
}

bool ScenicPart(const WorldDescriptor& W, ChunkCoord C, Part& Out)
{
    if (EWOuterWater::Contains(W,C) || RegionAt(W,C)==RegionKind::City) return false;
    const bool ArrivalLandmark = C==ChunkCoord{1,1} || C==ChunkCoord{2,0} || C==ChunkCoord{5,1} || C==ChunkCoord{1,5};
    if (!ArrivalLandmark && (C.X - FloorDiv(C.X, 4)*4 != 2 || C.Y - FloorDiv(C.Y, 4)*4 != 2)) return false;
    const auto V = W.Number("distant-scenery", {C.X,C.Y}); const auto Region = RegionAt(W,C);
    const double Z = HeightAt(W,C.X*ChunkSize+1000,C.Y*ChunkSize+1000);
    FString Name; FVector Scale; double Height = Z;
    if (Region == RegionKind::City)
    { Name = FString::Printf(TEXT("SkyWard_%d"), C==ChunkCoord{1,1} ? 3 : int32(V%4)); Scale=FVector(1.0); Height-=600; }
    else if (Region == RegionKind::Garden)
    {
        Name=FString::Printf(TEXT("CanopyWorld_%d"),int32(V%3)); Scale=FVector(1.0); Height-=600;
    }
    else
    { Name=FString::Printf(TEXT("CrystalWorld_%d"), int32(V%3)); Scale=FVector(1.0); Height+=400; }
    Out={FName(*Name), FTransform(FRotator(0,V%360,0),FVector(1000,1000,Height),Scale),0};
    return true;
}

void CityVolumeParts(const WorldDescriptor& W, ChunkCoord C, TArray<Part>& Out)
{
    if (!W.Valid() || !C.Valid() || RegionAt(W,C)!=RegionKind::City) return;
    // One shared storey datum makes every joining facade and street agree,
    // including across chunk borders. BuildCityAccess derives the public floors
    // and all collision from these same final transforms.
    const double Datum=FMath::FloorToDouble(HeightAt(W,0,0)/450.)*450.;
    auto Place=[&](const FString& Mesh,FVector P,double Yaw=0.,FVector Scale=FVector::OneVector)
    { Out.Add({FName(*Mesh),FTransform(FRotator(0,Yaw,0),P,Scale),0}); };
    auto ColumnCentre=[&](ChunkCoord Chunk,int32 X,int32 Y)
    {
        const auto V=W.Number("urban-column",{Chunk.X,Chunk.Y,X,Y});
        return FVector((Chunk.X-C.X)*ChunkSize+3200+X*6400+double(int32((V>>20)%201)-100),
                       (Chunk.Y-C.Y)*ChunkSize+3200+Y*6400+double(int32((V>>32)%201)-100),Datum);
    };
    for (int32 Y=0;Y<2;++Y) for (int32 X=0;X<2;++X)
    {
        const auto Form=W.Number("urban-column",{C.X,C.Y,X,Y});
        const int32 Low=-6-int32((Form>>8)%2), High=4+int32((Form>>12)%5);
        const FVector XY=ColumnCentre(C,X,Y);
        // Reserve enough clear width for both the lift shaft and central road,
        // including the widest facade and maximum positional variation.
        const double SizeX=.86+double(Form%6)*.025, SizeY=.92+double((Form>>5)%6)*.025;
        // The arrival has a recognisable library on one side and residences
        // opposite it. Other districts keep their seed-defined architectural mix.
        const int32 Profile=C==ChunkCoord{0,0} ? (X==1 && Y==1 ? 1 : X==0 && Y==1 ? 0 : X==1 ? 2 : 3) :
            int32((Form>>44)%4);
        for (int32 Level=Low;Level<=High;++Level)
        {
            const auto V=W.Number("urban-storeys",{C.X,C.Y,X,Y,Level});
            // Upper districts step back as a group rather than alternating
            // between narrow and wide, so the silhouette reads as a building.
            const double Setback=Level>=High-1 ? .12 : Level>=3 ? .06 : 0.;
            const bool Library=(Profile==1 && Level%3!=2) ||
                (Profile==2 && (Level+10)%4==0) || (Profile==3 && Level==-1);
            FString Mesh=Library ? FString::Printf(TEXT("UrbanLibrary_%d"),int32(V%3)) :
                FString::Printf(TEXT("UrbanBlock_%d"),int32(V%8));
            // Keep every seed, transform, public floor and bridge unchanged.
            // Only one closed block gains an interior, on the arrival public floor.
            if(IsRainWindowDistrict(W,C) && X==0 && Y==1)
            {
                const double Ground=Datum+FMath::RoundToDouble((HeightAt(W,6400,6400)-Datum)/1800.)*1800.;
                const double Relative=Ground-(Datum+Level*3600.);
                if(Relative>=-.01 && Relative<3600.-.01) Mesh=TEXT("UrbanBlock_RainWindow");
            }
            const double Yaw=double((Form>>16)%4)*90.;
            Place(Mesh,XY+FVector(0,0,Level*3600.),Yaw,FVector(SizeX-Setback,SizeY-Setback,1));
            if (Level>=-3 && Level<High && (Level+12+int32(Form%3))%3==0)
                Place(FString::Printf(TEXT("UrbanTerrace_%d"),int32(V%3)),
                    XY+FVector(0,0,Level*3600.),Yaw,FVector(SizeX-Setback,SizeY-Setback,1));
        }
        if (Profile==1 || Profile==2)
            Place(FString::Printf(TEXT("UrbanGarden_%d"),int32(Form%3)),XY+FVector(0,0,(High+1)*3600.),
                double((Form>>16)%4)*90.,FVector(SizeX-.12,SizeY-.12,1));
        else
            Place(FString::Printf(TEXT("UrbanCrown_%d"),int32(Form%4)),XY+FVector(0,0,(High+1)*3600.),
                double((Form>>16)%4)*90.,FVector(SizeX-.12,SizeY-.12,1));
    }
    struct FNode
    {
        bool Valid=false;uint64 Form=0;int32 Low=0,High=0,Profile=0;
        FVector Centre=FVector::ZeroVector;double Ground=0;
    };
    TMap<ChunkCoord,FNode> Nodes;
    auto Node=[&](ChunkCoord N)->FNode
    {
        if(const auto* Existing=Nodes.Find(N))return *Existing;
        const ChunkCoord Chunk{FloorDiv(N.X,2),FloorDiv(N.Y,2)};
        FNode F;
        if(Chunk.Valid() && RegionAt(W,Chunk)==RegionKind::City)
        {
            const int32 X=int32(N.X-Chunk.X*2),Y=int32(N.Y-Chunk.Y*2);
            F.Valid=true;F.Form=W.Number("urban-column",{Chunk.X,Chunk.Y,X,Y});
            F.Low=-6-int32((F.Form>>8)%2);F.High=4+int32((F.Form>>12)%5);
            F.Profile=Chunk==ChunkCoord{0,0}?(X==1 && Y==1?1:X==0 && Y==1?0:X==1?2:3):int32((F.Form>>44)%4);
            F.Centre=ColumnCentre(Chunk,X,Y);
            F.Ground=Datum+FMath::RoundToDouble((HeightAt(W,Chunk.X*ChunkSize+6400,Chunk.Y*ChunkSize+6400)-Datum)/1800.)*1800.;
        }
        Nodes.Add(N,F);return F;
    };
    auto Available=[&](ChunkCoord N,int32 Step)
    {
        const FNode F=Node(N);
        return F.Valid && Step>=F.Low*2 && Step<=(F.High+1)*2 && FMath::Abs(Datum+Step*1800.-F.Ground)>1.;
    };
    // Each gallery chooses one neighbouring tower. Pair directions and offsets
    // alternate by public floor, avoiding a repeated wall of covered bridges.
    // At a height boundary, an unpaired tower chooses an available neighbour.
    // Either endpoint may own that choice, so no reachable gallery is stranded.
    for(int32 Step=-14;Step<=18;++Step)
    {
        TMap<ChunkCoord,ChunkCoord> Choices;
        auto Choose=[&](ChunkCoord N)
        {
            if(const auto* Existing=Choices.Find(N))return *Existing;
            ChunkCoord Chosen=N;
            if(Available(N,Step))
            {
                const int32 Axis=int32(Step-FloorDiv(Step,2)*2);
                const int64 Phase=FloorDiv(Step,2);
                const int64 Along=(Axis==0?N.X:N.Y)+Phase;
                const int64 Direction=Along-FloorDiv(Along,2)*2==0?1:-1;
                const ChunkCoord Preferred=N+(Axis==0?ChunkCoord{Direction,0}:ChunkCoord{0,Direction});
                if(Available(Preferred,Step))Chosen=Preferred;
                else
                {
                    uint64 Best=MAX_uint64;
                    for(ChunkCoord Offset:{ChunkCoord{1,0},ChunkCoord{-1,0},ChunkCoord{0,1},ChunkCoord{0,-1}})
                    {
                        const ChunkCoord Other=N+Offset;if(!Available(Other,Step))continue;
                        const uint64 Score=W.Number("skywalk-fallback",{N.X,N.Y,Other.X,Other.Y,Step});
                        if(Score<Best){Best=Score;Chosen=Other;}
                    }
                }
            }
            Choices.Add(N,Chosen);return Chosen;
        };
    for (int32 Axis=0;Axis<2;++Axis) for (int32 Across=0;Across<2;++Across)
        for (int32 Edge=0;Edge<2;++Edge)
        {
            ChunkCoord Next=C;
            if (Edge==1) { if (Axis==0) ++Next.X; else ++Next.Y; }
            if (!Next.Valid() || RegionAt(W,Next)!=RegionKind::City) continue;
            const int32 AX=Axis==0 ? Edge : Across, AY=Axis==0 ? Across : Edge;
            const int32 BX=Axis==0 ? (Edge==0 ? 1 : 0) : Across, BY=Axis==0 ? Across : (Edge==0 ? 1 : 0);
            const ChunkCoord NA{C.X*2+AX,C.Y*2+AY},NB{Next.X*2+BX,Next.Y*2+BY};
            if(Choose(NA)!=NB && Choose(NB)!=NA)continue;
            const FNode FA=Node(NA),FB=Node(NB);
            const FVector Direction=FB.Centre-FA.Centre;
                const FVector Unit=Direction.GetSafeNormal2D();
                auto Endpoint=[&](const FNode& F,FVector D)
                {
                    const int32 Level=int32(FloorDiv(Step,2));
                    const double Setback=Level>=F.High-1?.12:Level>=3?.06:0.;
                    const double SX=.86+double(F.Form%6)*.025-Setback,SY=.92+double((F.Form>>5)%6)*.025-Setback;
                    const FVector Local=FRotator(0,double((F.Form>>16)%4)*-90.,0).RotateVector(D);
                    const double Reach=FMath::Min(2300.*SX/FMath::Max(.0001,FMath::Abs(Local.X)),2300.*SY/FMath::Max(.0001,FMath::Abs(Local.Y)));
                    FVector P=F.Centre+D*Reach;
                    P.Z=Datum+Step*1800.;
                    if(Step==(F.High+1)*2 && F.Profile!=1 && F.Profile!=2)P.Z+=45.;
                    return P;
                };
                const FVector Start=Endpoint(FA,Unit),End=Endpoint(FB,-Unit);
                const uint64 V=W.Number("skywalk-finish",{NA.X,NA.Y,NB.X,NB.Y,Step});
                const bool Covered=Step%4==2 && V%5==0;
                const FString Mesh=Covered?TEXT("UrbanPromenade_0"):FString::Printf(TEXT("UrbanBridge_%d"),int32(V%2)*2);
                const FQuat Q=FRotationMatrix::MakeFromXZ((End-Start).GetSafeNormal(),FVector::UpVector).ToQuat();
                Out.Add({FName(*Mesh),FTransform(Q,(Start+End)*.5,FVector((End-Start).Size()/6400.,.72,1)),0});
        }
    }
}

bool ChunkRecipe::Validate(FString& Error) const
{
    if (!World.Valid() || !Coord.Valid() || Portals.Num() != 4 || Paths.Num() < 4 ||
        Parts.Num() > 5000 || Colliders.Num() > 16000 || Places.Num() > 7 || Residences.Num()>1 || Interiors.Num()>600)
    { Error = TEXT("区画データの範囲が不正です"); return false; }
    for (const auto& P : Parts)
        if (P.Transform.ContainsNaN() || P.Transform.GetScale3D().GetMin() <= 0)
        { Error = TEXT("形状の座標が不正です"); return false; }
    for(const auto& Room:Interiors)
        if(Room.Kind<0 || Room.Kind>24 || Room.Frame.ContainsNaN() || Room.Frame.GetScale3D().GetMin()<=0)
        {Error=TEXT("室内の配置が不正です");return false;}
    for(const auto& R:Residences)
        if(R.Revision!=1 || R.Id!=TEXT("rain-window/0,0/nw") || R.Frame.ContainsNaN() ||
            R.Frame.GetScale3D().GetMin()<=0 || R.Entry.ContainsNaN() || R.Valve.ContainsNaN() ||
            R.Seat.ContainsNaN() || R.Stand.ContainsNaN() || R.ViewTarget.ContainsNaN() ||
            !FMath::IsFinite(R.ViewYaw) || !FMath::IsFinite(R.ViewPitch) ||
            R.OpeningSide<0 || R.OpeningSide>3 ||
            (FMath::Abs(R.FloorOffset)>.01 && FMath::Abs(R.FloorOffset-1800.)>.01))
        { Error=TEXT("窓辺の部屋の仕様が不正です");return false; }
    if(bCascadeCity && !EWOuterWater::Contains(World,Coord)){Error=TEXT("水都の範囲が不正です");return false;}
    if(bWaterCity && (!IsWaterCityDistrict(World,Coord) || !GetWaterCityPlan().bValid))
    {Error=TEXT("水都の配置定義が不正です");return false;}
    for(const auto& P:WaterPaths)
        if(P.A.ContainsNaN() || P.B.ContainsNaN() || FVector::Dist2D(P.A,P.B)<1 || P.Width<220 ||
            FMath::Abs(P.A.Z-P.B.Z)/FVector::Dist2D(P.A,P.B)>.2)
        {Error=TEXT("水辺の道の仕様が不正です");return false;}
    for (const auto& P : Paths)
    {
        const double Horizontal = FVector::Dist2D(P.A, P.B);
        if (Horizontal < 1 || FMath::Abs(P.A.Z - P.B.Z) / Horizontal > .35 || P.Width < 250)
        { Error = TEXT("道の幅・勾配が範囲外です"); return false; }
    }
    for (const auto& Port : Portals)
    {
        bool Joined = false;
        for (const auto& Path : Paths)
            if (Path.A.Equals(Port.Position, .01) || Path.B.Equals(Port.Position, .01)) Joined = true;
        if (!Joined) { Error = TEXT("境界の道が接続されていません"); return false; }
    }
    Error.Empty(); return true;
}

double ChunkRecipe::LowestWalkSurface() const
{
    double Lowest=Hub.Z;
    for(const auto& C:Colliders)if(C.bWalkSurface)
    {
        const FQuat Q=C.Transform.GetRotation();
        const double HalfZ=FMath::Abs(Q.GetAxisX().Z)*C.Extent.X+
            FMath::Abs(Q.GetAxisY().Z)*C.Extent.Y+FMath::Abs(Q.GetAxisZ().Z)*C.Extent.Z;
        Lowest=FMath::Min(Lowest,C.Transform.GetLocation().Z-HalfZ);
    }
    return Lowest;
}

FVector ChunkRecipe::SafePosition(const FVector& Preferred) const
{
    // Score in three dimensions so a saved upper/lower floor cannot silently
    // resolve to the first ground-level surface with the same XY.
    FVector Best=Hub+FVector(0,-1250,100);double Distance=DBL_MAX;
    auto Clear=[&](const FVector& Surface,int32 Support)
    {
        for(int32 I=0;I<Colliders.Num();++I)
        {
            if(I==Support)continue;
            const auto& C=Colliders[I];
            const FVector Q=C.Transform.InverseTransformPosition(Surface+FVector(0,0,91));
            if(FMath::Abs(Q.X)<C.Extent.X+38 && FMath::Abs(Q.Y)<C.Extent.Y+38 && FMath::Abs(Q.Z)<C.Extent.Z+87)
                return false;
        }
        return true;
    };
    for(int32 I=0;I<Colliders.Num();++I)
    {
        const auto& F=Colliders[I];if(!F.bWalkSurface)continue;
        const FVector N=F.Transform.GetRotation().GetUpVector();if(N.Z<.7)continue;
        const FVector Top=F.Transform.GetLocation()+N*F.Extent.Z;
        FVector Surface(Preferred.X,Preferred.Y,Top.Z-((Preferred.X-Top.X)*N.X+(Preferred.Y-Top.Y)*N.Y)/N.Z);
        FVector Q=F.Transform.InverseTransformPosition(Surface);
        // The projected edge candidate also offers recovery near an upper floor,
        // rather than sending an unsafe imported coordinate down to the plaza.
        Q.X=FMath::Clamp(Q.X,-F.Extent.X+65,F.Extent.X-65);
        Q.Y=FMath::Clamp(Q.Y,-F.Extent.Y+65,F.Extent.Y-65);Q.Z=F.Extent.Z;
        Surface=F.Transform.TransformPosition(Q);
        const FVector Candidate=Surface+FVector(0,0,100);
        const double D=FVector::DistSquared(Candidate,Preferred);
        if(D<Distance && Clear(Surface,I)){Distance=D;Best=Candidate;}
    }
    // A save made on a moving lift resolves to that shaft's closest served
    // landing; this remains safe even when the new car starts on another floor.
    return Best;
}

FString ChunkRecipe::Digest() const
{
    TArray<uint8> Bytes;
    AppendString(Bytes, World.Code()); Append64(Bytes, Coord.X); Append64(Bytes, Coord.Y);
    Append64(Bytes, uint64(Region)); AppendVector(Bytes, Hub); Append64(Bytes, bFallback);
    for (const auto& P : Portals)
    {
        Append64(Bytes, P.Axis); Append64(Bytes, P.Boundary.X); Append64(Bytes, P.Boundary.Y);
        AppendVector(Bytes, P.Position); Append64(Bytes, int64(P.Width));
    }
    for (const auto& P : Parts)
    {
        AppendString(Bytes, P.Mesh.ToString()); AppendVector(Bytes, P.Transform.GetTranslation());
        AppendVector(Bytes, P.Transform.GetScale3D()); AppendVector(Bytes, P.Transform.GetRotation().Euler());
        Append64(Bytes, P.Detail);
    }
    for (const auto& C : Colliders)
    {
        AppendVector(Bytes, C.Transform.GetTranslation()); AppendVector(Bytes, C.Transform.GetRotation().Euler());
        AppendVector(Bytes, C.Extent); Append64(Bytes, C.bWalkSurface);
    }
    for (const auto& P : Places) { AppendString(Bytes, P.Id); Append64(Bytes, P.Kind); AppendVector(Bytes, P.LocalPosition); }
    for(const auto& L:Lifts)
    {
        AppendString(Bytes,L.Id);AppendVector(Bytes,L.Cabin);AppendVector(Bytes,L.Outward);
        for(const auto& S:L.Stops){AppendVector(Bytes,S.Landing);Append64(Bytes,int64(S.Height*100));Append64(Bytes,S.Floor);AppendString(Bytes,S.Label);}
    }
    for(const auto& F:CityFloors){Append64(Bytes,F.Column);AppendVector(Bytes,F.Transform.GetLocation());Append64(Bytes,F.bRoof);}
    if(bWaterCity)
    {
        AppendString(Bytes,TEXT("water-city-v1"));
        for(const auto& P:WaterPaths){AppendVector(Bytes,P.A);AppendVector(Bytes,P.B);Append64(Bytes,int64(P.Width));}
    }

    // Empty residence sets append no bytes: unrelated old recipes retain their digest.
    for(const auto& R:Residences)
    {
        AppendString(Bytes,R.Id);Append64(Bytes,R.Revision);AppendString(Bytes,R.OriginalMesh.ToString());
        AppendVector(Bytes,R.Frame.GetLocation());AppendVector(Bytes,R.Frame.GetRotation().Euler());
        AppendVector(Bytes,R.Frame.GetScale3D());Append64(Bytes,R.OpeningSide);Append64(Bytes,int64(R.FloorOffset));
        AppendVector(Bytes,R.Entry);AppendVector(Bytes,R.Valve);AppendVector(Bytes,R.Seat);AppendVector(Bytes,R.Stand);
        Append64(Bytes,int64(FMath::RoundToDouble(R.ViewYaw*10000.)));
        Append64(Bytes,int64(FMath::RoundToDouble(R.ViewPitch*10000.)));AppendVector(Bytes,R.ViewTarget);
    }

    return LexToString(FBlake3::HashBuffer(Bytes.GetData(), Bytes.Num())).ToLower();
}

FString PlaceBookmark::Code() const
{
    const FString Body = FString::Printf(TEXT("ewp2|%s|%lld|%lld|%lld|%lld|%lld|%lld|%d"),
        *WorldCode, Coord.X, Coord.Y, int64(FMath::RoundToDouble(LocalPosition.X)),
        int64(FMath::RoundToDouble(LocalPosition.Y)), int64(FMath::RoundToDouble(LocalPosition.Z)),
        int64(FMath::RoundToDouble(FRotator::NormalizeAxis(Yaw) * 100)), Kind);
    return Body + TEXT("|") + HashText(Body).Left(12);
}

bool PlaceBookmark::Parse(const FString& Input, PlaceBookmark& Out, FString& Error)
{
    FString Code = Input.TrimStartAndEnd();
    if (Code.Len() > 256) { Error = TEXT("場所コードが長すぎます。"); return false; }
    TArray<FString> Fields; Code.ParseIntoArray(Fields, TEXT("|"), false);
    if (Fields.Num() != 10 || Fields[0] != TEXT("ewp2"))
    { Error = TEXT("場所コードの形式が違います。"); return false; }
    const FString Body = Code.LeftChop(Fields.Last().Len() + 1);
    if (Fields.Last() != HashText(Body).Left(12))
    { Error = TEXT("場所コードが途中で変わっています。もう一度コピーしてください。"); return false; }
    WorldDescriptor W;
    if (!WorldDescriptor::Parse(Fields[1], W, Error)) return false;
    int64 Values[7] = {};
    for (int32 I = 0; I < 7; ++I)
        if (!ParseInteger(Fields[I + 2], Values[I])) { Error = TEXT("場所の座標が不正です。"); return false; }
    PlaceBookmark B; B.WorldCode = W.Code(); B.Coord = {Values[0], Values[1]};
    if (!B.Coord.Valid() || Values[2] < 0 || Values[2] > ChunkSize || Values[3] < 0 || Values[3] > ChunkSize ||
        Values[4] < -100000 || Values[4] > 100000 || FMath::Abs(Values[5]) > 18000 || Values[6] < 0 || Values[6] > 19)
    { Error = TEXT("この場所の座標は対応範囲外です。"); return false; }
    B.LocalPosition = FVector(Values[2], Values[3], Values[4]); B.Yaw = Values[5] / 100.;
    B.Kind = int32(Values[6]); B.Name = PlaceName(B.Kind);
    B.Id = B.Kind>=17 ? TEXT("sky92/")+B.Coord.Text()+TEXT("/landmark") : B.Kind>=15 ? TEXT("cascade86/")+B.Coord.Text()+(B.Kind==16?TEXT("/belvedere"):TEXT("/sanctum")) : B.Kind>=9 ? FString::Printf(TEXT("exploration85/%d"),B.Kind-9) : HashText(B.WorldCode + TEXT("|place|") + B.Coord.Text()).Left(32);
    Out = MoveTemp(B); Error.Empty(); return true;
}
}
