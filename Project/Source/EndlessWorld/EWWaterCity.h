#pragma once
#include "CoreMinimal.h"
#include "EWWorld.h"
#include "GameFramework/Actor.h"
#include "EWWaterCity.generated.h"

class UInstancedStaticMeshComponent;
class UAudioComponent;
class USoundWaveProcedural;

namespace EW
{
struct WaterCityBody
{
    FString Id,Group;
    TArray<FVector2D> Polygon;
    FVector2D Flow;
    double WaterZ=0,Depth=0;
};
struct WaterCityBank
{
    FVector A,B;
    double Width=0,Top=0,Bottom=0;
};
struct WaterCityFall
{
    FString Id,Group;
    double X0=0,X1=0,StartY=0,EndY=0,Top=0,Bottom=0;
};
struct WaterCityWalk
{
    FString Id;
    FVector A,B;
    double Width=0,Thickness=0,EndPad=0;
    bool bAccess=false;
};
struct WaterCityRail
{
    FVector A,B;
    double Height=0,Width=0;
};
struct WaterCitySupport
{
    double X=0,Y=0,Bottom=0,Top=0,HalfWidth=0,HalfDepth=0;
};
struct WaterCityView
{
    FString Id;
    FVector Eye,Target;
};
struct ENDLESSWORLD_API WaterCityPlan
{
    bool bValid=false;
    int32 Revision=0;
    FString Error,BaselineSHA;
    TMap<FString,FString> BaselineDigests;
    TArray<WaterCityBody> Bodies;
    TArray<WaterCityBank> Banks;
    TArray<WaterCityFall> Falls;
    TArray<WaterCityWalk> Walks;
    TArray<WaterCityRail> Rails;
    TArray<WaterCitySupport> Supports;
    TArray<WaterCityView> Views;
};
ENDLESSWORLD_API const WaterCityPlan& GetWaterCityPlan();
ENDLESSWORLD_API bool IsWaterCityDistrict(const WorldDescriptor& World,ChunkCoord Coord);
ENDLESSWORLD_API TArray<PathSegment> WaterCityAccessPaths(const WorldDescriptor& World,ChunkCoord Coord,bool bEndPads=false);
ENDLESSWORLD_API void BuildWaterCity(ChunkRecipe& Recipe);
}

// Geometry and waves stream with the ordinary chunk parts. This actor owns
// only bounded nearby spray and local sound; it never reads or writes saves.
UCLASS()
class ENDLESSWORLD_API AEWWaterCity : public AActor
{
    GENERATED_BODY()
public:
    AEWWaterCity();
    bool Initialize(EW::ChunkCoord Coord);
    static void AppendAssetPaths(TArray<FSoftObjectPath>& Paths);
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    int32 SprayInstanceCount() const;
    int32 ActiveSoundCount() const;
private:
    UPROPERTY() TObjectPtr<USceneComponent> Root;
    UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Spray;
    UPROPERTY() TArray<TObjectPtr<UAudioComponent>> Sounds;
    UPROPERTY() TArray<TObjectPtr<USoundWaveProcedural>> Waves;
    TArray<EW::WaterCityFall> LocalFalls;
};
