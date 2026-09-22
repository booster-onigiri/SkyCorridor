#pragma once
#include "CoreMinimal.h"

namespace EW
{
constexpr int64 ChunkSize = 12800; // centimetres
constexpr int64 CoordLimit = (1LL << 40) - 1;
constexpr int32 GeneratorVersion = 2;
constexpr int32 CatalogueVersion = 1;
constexpr int32 MaxResidentChunks = 49;
constexpr int32 MaxQueue = 8;
constexpr int32 MaxJobs = 2;
constexpr int32 DiscoveryQuota = 10000;

struct ENDLESSWORLD_API ChunkCoord
{
    int64 X = 0, Y = 0;
    bool operator==(const ChunkCoord& B) const { return X == B.X && Y == B.Y; }
    bool operator!=(const ChunkCoord& B) const { return !(*this == B); }
    ChunkCoord operator+(const ChunkCoord& B) const { return {X + B.X, Y + B.Y}; }
    FString Text() const { return FString::Printf(TEXT("%lld,%lld"), X, Y); }
    bool Valid() const { return X >= -CoordLimit && X <= CoordLimit && Y >= -CoordLimit && Y <= CoordLimit; }
    friend uint32 GetTypeHash(const ChunkCoord& C) { return HashCombineFast(::GetTypeHash(C.X), ::GetTypeHash(C.Y)); }
};

enum class RegionKind : uint8 { City, Garden, Crystal };

struct ENDLESSWORLD_API WorldDescriptor
{
    FString Seed;
    FString Code() const;
    static bool Parse(const FString& Code, WorldDescriptor& Out, FString& Error);
    static WorldDescriptor NewWorld();
    static WorldDescriptor ReferenceWorld();
    bool Valid() const;
    uint64 Number(const ANSICHAR* Domain, std::initializer_list<int64> Values) const;
};

struct ENDLESSWORLD_API Portal
{
    int32 Axis = 0;
    ChunkCoord Boundary;
    FVector Position = FVector::ZeroVector;
    double Width = 600;
};

struct ENDLESSWORLD_API PathSegment
{
    FVector A, B;
    double Width = 600;
};

struct ENDLESSWORLD_API Part
{
    FName Mesh;
    FTransform Transform;
    uint8 Detail = 0; // 0 silhouette, 1 architecture, 2 close detail
};

struct ENDLESSWORLD_API Collider
{
    FTransform Transform;
    FVector Extent;
    bool bWalkSurface = false;
};

struct ENDLESSWORLD_API LiftStop
{
    double Height = 0;
    FVector Landing = FVector::ZeroVector; // on the fixed approach, outside the car
    FString Label;
    int32 Floor = 0; // 4.5m building storeys; public stops occur every four storeys
    FVector Entry = FVector::ZeroVector; // actual gallery edge, including setbacks
    FVector Threshold = FVector::ZeroVector;
    FRotator BoardingRotation = FRotator::ZeroRotator; // local +Y points towards this floor
};

struct ENDLESSWORLD_API LiftSpec
{
    FString Id;
    FVector Cabin = FVector::ZeroVector; // XY fixed; each stop supplies Z
    FVector Outward = FVector::ZeroVector;
    FRotator Rotation = FRotator::ZeroRotator; // car boards through local +Y
    TArray<LiftStop> Stops;
};

struct ENDLESSWORLD_API CityWalkFloor
{
    int32 Column = 0;
    FTransform Transform;
    bool bRoof = false;
};

// An additive, authored pocket in the existing ew2-kit1 reference city.
// This revision is deliberately not part of WorldDescriptor::Number().
struct ENDLESSWORLD_API ResidenceSpec
{
    FString Id = TEXT("rain-window/0,0/nw");
    int32 Revision = 1;
    FName OriginalMesh;
    FTransform BlockTransform;
    FTransform Frame; // room floor datum; outward is local -Y; centimetres
    int32 OpeningSide = 0;
    double FloorOffset = 0;
    FVector Entry = FVector::ZeroVector; // chunk-local capsule centres
    FVector Valve = FVector::ZeroVector;
    FVector Seat = FVector::ZeroVector;
    FVector Stand = FVector::ZeroVector;
    double ViewYaw = 0;
    double ViewPitch = 0;
    FVector ViewTarget = FVector::ZeroVector; // authored point on the real arrival clock hand
};

struct ENDLESSWORLD_API PlaceBookmark
{
    FString WorldCode;
    ChunkCoord Coord;
    FString Id;
    FString Name;
    int32 Kind = 0;
    FVector LocalPosition = FVector::ZeroVector;
    double Yaw = 0;
    bool bFavourite = false;
    FString Code() const;
    static bool Parse(const FString& Code, PlaceBookmark& Out, FString& Error);
};

struct ENDLESSWORLD_API InteriorRoom
{
    FTransform Frame;
    int32 Kind=0; // living, bedroom, study, dining, library, rain-window, cafe, botanical, atelier, music
    int32 Variant=0;
};

struct ENDLESSWORLD_API ChunkRecipe
{
    WorldDescriptor World;
    ChunkCoord Coord;
    RegionKind Region = RegionKind::City;
    FVector Hub = FVector::ZeroVector;
    TArray<Portal> Portals;
    TArray<PathSegment> Paths;
    TArray<Part> Parts;
    TArray<Collider> Colliders;
    TArray<PlaceBookmark> Places;
    TArray<LiftSpec> Lifts;
    TArray<CityWalkFloor> CityFloors;
    TArray<ResidenceSpec> Residences;
    TArray<InteriorRoom> Interiors;
    // Authored additions keep the original path graph and save codes intact.
    TArray<PathSegment> WaterPaths;
    bool bWaterCity = false;
    bool bCascadeCity = false;
    bool bFallback = false;
    FString Digest() const;
    bool Validate(FString& Error) const;
    FVector SafePosition(const FVector& Preferred) const;
    double LowestWalkSurface() const;
};

ENDLESSWORLD_API int64 FloorDiv(int64 Value, int64 Divisor);
ENDLESSWORLD_API double HeightAt(const WorldDescriptor& World, int64 X, int64 Y);
ENDLESSWORLD_API RegionKind RegionAt(const WorldDescriptor& World, ChunkCoord Coord);
ENDLESSWORLD_API FString RegionName(RegionKind Region);
ENDLESSWORLD_API FString PlaceName(int32 Kind);
ENDLESSWORLD_API FString PlaceDescription(int32 Kind);
ENDLESSWORLD_API ChunkRecipe GenerateChunk(const WorldDescriptor& World, ChunkCoord Coord);
ENDLESSWORLD_API bool ScenicPart(const WorldDescriptor& World, ChunkCoord Coord, Part& Out);
ENDLESSWORLD_API void CityVolumeParts(const WorldDescriptor& World, ChunkCoord Coord, TArray<Part>& Out);
ENDLESSWORLD_API double DistanceToSegment2D(const FVector& P, const FVector& A, const FVector& B);
ENDLESSWORLD_API FString HashText(const FString& Text);
}
