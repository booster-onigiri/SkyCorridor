#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/StreamableManager.h"
#include "Async/Future.h"
#include "EWWorld.h"
#include "EWChunkManager.generated.h"

class UInstancedStaticMeshComponent;
class UBoxComponent;
class UWidgetComponent;
class AEWCharacter;
class AEWLift;
class AEWResidence;
class AEWWaterCity;
enum class EEWResidenceAction : uint8;

UCLASS()
class ENDLESSWORLD_API AEWChunkActor : public AActor
{
    GENERATED_BODY()
public:
    AEWChunkActor();
    void BeginApply(TSharedPtr<EW::ChunkRecipe> InRecipe, uint8 InDetail);
    bool ApplyUntil(double Deadline);
    void SetCollisionFocus(double WorldZ);
    bool NeedsApply() const;
    void Release();
    bool IsComplete() const { return bComplete && !bAssetFailed; }
    int32 InstanceCount() const { return AppliedInstances; }
    int32 ColliderCount() const { return ActiveBoxes.Num(); }
    uint8 GetDetail() const { return Detail; }
    TSharedPtr<EW::ChunkRecipe> Recipe;
    UPROPERTY() TArray<TObjectPtr<AEWLift>> Lifts;
    UPROPERTY() TArray<TObjectPtr<AEWResidence>> Residences;
    UPROPERTY() TObjectPtr<AEWWaterCity> WaterCity;
    UPROPERTY() TArray<TObjectPtr<UWidgetComponent>> ClockFaces;
private:
    UPROPERTY() TObjectPtr<USceneComponent> Root;
    UPROPERTY() TMap<FName, TObjectPtr<UInstancedStaticMeshComponent>> MeshGroups;
    UPROPERTY() TMap<int32,TObjectPtr<UBoxComponent>> ActiveBoxes;
    int32 PartCursor = 0, ColliderCursor = 0, AppliedInstances = 0;
    int32 LiftCursor=0;
    int32 ResidenceCursor=0;
    bool bWaterCityApplied=false;
    double CollisionFocus=DBL_MAX;
    uint8 Detail = 0;
    bool bComplete = false, bAssetFailed = false;
};

struct FEWGenerationJob
{
    EW::ChunkCoord Coord;
    uint64 Epoch = 0;
    TFuture<EW::ChunkRecipe> Future;
};

struct FEWLoadedChunk
{
    TSharedPtr<EW::ChunkRecipe> Recipe;
    TSharedPtr<FStreamableHandle> LoadHandle;
    TWeakObjectPtr<AEWChunkActor> Actor;
    uint8 Detail = 0;
    double LowestWalkZ=DBL_MAX;
};

UCLASS()
class ENDLESSWORLD_API AEWChunkManager : public AActor
{
    GENERATED_BODY()
public:
    AEWChunkManager();
    virtual void Tick(float DeltaSeconds) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void StartWorld(const EW::WorldDescriptor& World, const EW::PlaceBookmark& Destination);
    EW::ChunkCoord PlayerCoord() const;
    EW::PlaceBookmark CurrentPosition() const;
    bool TryCurrentPosition(EW::PlaceBookmark& Out) const;
    TOptional<EW::PlaceBookmark> NearestPlace(double MaxDistance = 2500) const;
    AEWLift* NearestLift() const;
    AEWResidence* NearestResidence(EEWResidenceAction& Action) const;
    TSharedPtr<const EW::ChunkRecipe> RecipeAt(EW::ChunkCoord Coord) const;
    bool ReadyAt(EW::ChunkCoord Coord) const;
    double FallRecoveryHeight() const;
    const EW::WorldDescriptor& Descriptor() const { return World; }
    EW::ChunkCoord OriginCoord() const { return Origin; }
    FVector ToRender(EW::ChunkCoord Coord, const FVector& Local) const;
    FVector ToLocal(EW::ChunkCoord Coord, const FVector& Render) const;
    bool IsTravelling() const { return bTravelling; }
    float TravelProgress() const;
    int32 ResidentCount() const { return Loaded.Num(); }
    int32 JobCount() const { return Jobs.Num(); }
    int32 QueueCount() const { return Queue.Num(); }
    int32 ReadyCount() const;
    int32 InstanceCount() const;
    int32 ColliderCount() const;
    int32 RebaseThreshold() const { return RebaseDistance; }
    int32 RebaseCount = 0, PeakResident = 0, PeakJobs = 0, PeakQueue = 0, Generated = 0, Discarded = 0;
    double LastApplyMilliseconds = 0, MaxApplyMilliseconds = 0;
    bool bHasWorld = false;
private:
    EW::WorldDescriptor World;
    EW::ChunkCoord Origin, LastCentre{MAX_int64, MAX_int64};
    EW::PlaceBookmark PendingDestination;
    TMap<EW::ChunkCoord, uint8> Desired;
    TMap<EW::ChunkCoord, FEWLoadedChunk> Loaded;
    TArray<EW::ChunkCoord> Queue;
    TArray<FEWGenerationJob> Jobs;
    FEWLoadedChunk Horizon;
    FStreamableManager Loader;
    uint64 Epoch = 0;
    bool bTravelling = false;
    int32 RebaseDistance = 8;
    void RefreshDesired();
    void CollectJobs();
    void FillQueue();
    void LaunchJobs();
    void ApplyChunks();
    void RefreshHorizon();
    void ApplyOne(FEWLoadedChunk& Chunk, const FVector& Position, double Deadline);
    void RebaseIfNeeded();
    void FinishTravelIfReady();
    void Retire(FEWLoadedChunk& Chunk);
    AEWCharacter* Character() const;
};
