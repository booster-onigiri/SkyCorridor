#include "EWChunkManager.h"
#include "EWLocalization.h"
#include "EWCharacter.h"
#include "EWLift.h"
#include "EWResidence.h"
#include "EWWaterCity.h"
#include "EWOuterWater.h"
#include "EWGameInstance.h"
#include "EWAeroYacht.h"
#include "EWSkyrail.h"
#include "EWAeroYachtPlan.h"
#include "EWClock.h"
#include "Components/WidgetComponent.h"
#include "Async/Async.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

AEWChunkActor::AEWChunkActor()
{
    PrimaryActorTick.bCanEverTick = false;
    Root = CreateDefaultSubobject<USceneComponent>(TEXT("ChunkRoot"));
    SetRootComponent(Root);
}

void AEWChunkActor::BeginApply(TSharedPtr<EW::ChunkRecipe> InRecipe, uint8 InDetail)
{
    Recipe = MoveTemp(InRecipe); Detail = InDetail;
}

void AEWChunkActor::SetCollisionFocus(double WorldZ)
{
    const double Z=WorldZ-GetActorLocation().Z;
    if(FMath::Abs(CollisionFocus-Z)<600)return;
    CollisionFocus=Z;ColliderCursor=0;
    if(Detail==2)bComplete=false;
}
bool AEWChunkActor::NeedsApply() const
{
    return Recipe && (PartCursor<Recipe->Parts.Num() || (Detail==2 &&
        (ColliderCursor<Recipe->Colliders.Num() || LiftCursor<Recipe->Lifts.Num() || ResidenceCursor<Recipe->Residences.Num() ||
         ((Recipe->bWaterCity || Recipe->bCascadeCity) && !bWaterCityApplied))));
}

bool AEWChunkActor::ApplyUntil(double Deadline)
{
    if (!Recipe) return false;
    while (PartCursor < Recipe->Parts.Num() && FPlatformTime::Seconds() < Deadline)
    {
        const EW::Part& P = Recipe->Parts[PartCursor++];
        if (P.Detail > Detail) continue;
        if(P.Mesh==TEXT("ClockTower") && EWClock::IsCentralPlaza(*Recipe))continue;
        TObjectPtr<UInstancedStaticMeshComponent>* Existing = MeshGroups.Find(P.Mesh);
        UInstancedStaticMeshComponent* Group = Existing ? Existing->Get() : nullptr;
        if (!Group)
        {
            FString Path = TEXT("/Game/EndlessWorld/Kit/SM_") + P.Mesh.ToString() + TEXT(".SM_") + P.Mesh.ToString();
            UStaticMesh* Mesh = Cast<UStaticMesh>(FSoftObjectPath(Path).ResolveObject());
            if (!Mesh)
            {
                UE_LOG(LogTemp, Error, TEXT("EW_MISSING_ASSET %s"), *Path);
                bAssetFailed = true;
                if (auto* GI = GetGameInstance<UEWGameInstance>())
                    GI->Notify(EWL::Pick(TEXT("必要な素材を読み込めません。配布ファイルを展開し直してください。"), TEXT("Required assets could not be loaded. Extract the distribution files again.")), 86400);
                continue;
            }
            Group = NewObject<UInstancedStaticMeshComponent>(this);
            Group->SetupAttachment(Root); Group->SetMobility(EComponentMobility::Movable);
            Group->SetStaticMesh(Mesh); Group->SetCollisionEnabled(ECollisionEnabled::NoCollision);
            Group->SetCanEverAffectNavigation(false);
            const FString Name=P.Mesh.ToString();
            const bool WaterSurface=(Name.StartsWith(TEXT("WaterCity")) &&
                (Name.EndsWith(TEXT("Water")) || Name.EndsWith(TEXT("Fall")) || Name.EndsWith(TEXT("Foam")))) ||
                (Name.StartsWith(TEXT("Cascade86")) && (Name.Contains(TEXT("Water")) || Name.Contains(TEXT("Fall")) || Name.EndsWith(TEXT("Weir")) || Name.EndsWith(TEXT("Foam"))));
            Group->SetCastShadow(!WaterSurface && Name!=TEXT("Pool90Water") && Name!=TEXT("Pool90Glass"));
            Group->RegisterComponent();
            MeshGroups.Add(P.Mesh, Group);
        }
        Group->AddInstance(P.Transform, false); ++AppliedInstances;
        if(P.Mesh==TEXT("ClockTower"))EWClock::AddFaces(this,Root,P,ClockFaces);
    }
    if (PartCursor >= Recipe->Parts.Num() && Detail == 2)
    {
        while (ColliderCursor < Recipe->Colliders.Num() && FPlatformTime::Seconds() < Deadline)
        {
            const int32 Index=ColliderCursor++;
            const EW::Collider& C = Recipe->Colliders[Index];
            if(EWClock::IsCentralPlaza(*Recipe) && !C.bWalkSurface && C.Extent.Equals(FVector(160,160,380),.01)
                && C.Transform.GetLocation().Equals(Recipe->Hub+FVector(0,0,380),.01))continue;
            const FQuat Q=C.Transform.GetRotation();
            const double HalfZ=FMath::Abs(Q.GetAxisX().Z)*C.Extent.X+FMath::Abs(Q.GetAxisY().Z)*C.Extent.Y+FMath::Abs(Q.GetAxisZ().Z)*C.Extent.Z;
            if(FMath::Abs(C.Transform.GetLocation().Z-CollisionFocus)>HalfZ+4000 || ActiveBoxes.Contains(Index))continue;
            UBoxComponent* Box = NewObject<UBoxComponent>(this);
            Box->SetupAttachment(Root); Box->SetMobility(EComponentMobility::Movable);
            Box->SetBoxExtent(C.Extent, false); Box->SetRelativeTransform(C.Transform);
            Box->SetCollisionObjectType(ECC_WorldStatic);
            Box->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
            Box->SetCollisionResponseToAllChannels(ECR_Block);
            Box->SetGenerateOverlapEvents(false); Box->SetCanEverAffectNavigation(false);
            Box->SetHiddenInGame(true);
            Box->ComponentTags.Add(C.bWalkSurface ? TEXT("EWFloor") : TEXT("EWObstacle"));
            Box->ComponentTags.Add(FName(*FString::Printf(TEXT("EWCollider_%d"),Index)));
            Box->RegisterComponent(); ActiveBoxes.Add(Index,Box);
        }
        if(ColliderCursor>=Recipe->Colliders.Num())
        {
            // Register the new height band before retiring the old support.
            for(auto It=ActiveBoxes.CreateIterator();It;++It)
            {
                const auto* B=It.Value().Get();
                if(B && FMath::Abs(B->Bounds.Origin.Z-GetActorLocation().Z-CollisionFocus)>B->Bounds.BoxExtent.Z+5000)
                {It.Value()->DestroyComponent();It.RemoveCurrent();}
            }
            while(LiftCursor<Recipe->Lifts.Num() && FPlatformTime::Seconds()<Deadline)
            {
                FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                auto* L=GetWorld()->SpawnActor<AEWLift>(GetActorLocation(),FRotator::ZeroRotator,Params);
                L->AttachToActor(this,FAttachmentTransformRules::KeepWorldTransform);
                L->Initialize(Recipe->Lifts[LiftCursor++],CollisionFocus);Lifts.Add(L);
            }
            while(ResidenceCursor<Recipe->Residences.Num() && FPlatformTime::Seconds()<Deadline)
            {
                FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                auto* Home=GetWorld()->SpawnActor<AEWResidence>(GetActorLocation(),FRotator::ZeroRotator,Params);
                if(Home)
                {
                    Home->AttachToActor(this,FAttachmentTransformRules::KeepWorldTransform);
                    if(!Home->Initialize(Recipe->Residences[ResidenceCursor],Recipe->World.Code()))
                    {
                        bAssetFailed=true;
                        if(auto* GI=GetGameInstance<UEWGameInstance>())GI->Notify(EWL::Pick(TEXT("窓辺の素材を読み込めません。配布ファイルを確認してください。"), TEXT("Window assets could not be loaded. Check the distribution files.")),86400);
                    }
                    Residences.Add(Home);
                }
                else bAssetFailed=true;
                ++ResidenceCursor;
            }
            if((Recipe->bWaterCity || Recipe->bCascadeCity) && !bWaterCityApplied && FPlatformTime::Seconds()<Deadline)
            {
                bWaterCityApplied=true;
                FActorSpawnParameters Params;Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
                WaterCity=GetWorld()->SpawnActor<AEWWaterCity>(GetActorLocation(),FRotator::ZeroRotator,Params);
                if(WaterCity)
                {
                    WaterCity->AttachToActor(this,FAttachmentTransformRules::KeepWorldTransform);
                    if(!WaterCity->Initialize(Recipe->Coord))bAssetFailed=true;
                }
                else bAssetFailed=true;
                if(bAssetFailed && GetGameInstance<UEWGameInstance>())
                    GetGameInstance<UEWGameInstance>()->Notify(EWL::Pick(TEXT("水都の素材を読み込めません。配布ファイルを確認してください。"), TEXT("Water City assets could not be loaded. Check the distribution files.")),86400);
            }
        }
    }
    if(!NeedsApply())bComplete=true;

    return bComplete;
}

void AEWChunkActor::Release()
{
    for(auto& Face:ClockFaces)if(Face){Face->SetSlateWidget(nullptr);Face->DestroyComponent();}ClockFaces.Empty();
    for (auto& Pair : MeshGroups)
        if (Pair.Value) { Pair.Value->ClearInstances(); Pair.Value->SetStaticMesh(nullptr); Pair.Value->DestroyComponent(); }
    for (auto& Pair : ActiveBoxes) if (Pair.Value) Pair.Value->DestroyComponent();
    for(const auto& L:Lifts)if(IsValid(L))L->Destroy();
    for(const auto& Home:Residences)if(IsValid(Home))Home->Destroy();
    if(IsValid(WaterCity))WaterCity->Destroy();WaterCity=nullptr;bWaterCityApplied=false;
    Residences.Empty();Lifts.Empty();MeshGroups.Empty();ActiveBoxes.Empty();Recipe.Reset();
}

AEWChunkManager::AEWChunkManager()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PrePhysics;
    if ((FParse::Param(FCommandLine::Get(),TEXT("EWRuntimeAudit")) || FParse::Param(FCommandLine::Get(),TEXT("EWAero87Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWSkyrailAudit"))) &&
        FParse::Param(FCommandLine::Get(),TEXT("EWRebaseEachChunk"))) RebaseDistance=1;
}

AEWCharacter* AEWChunkManager::Character() const
{
    return Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
}

void AEWChunkManager::Retire(FEWLoadedChunk& C)
{
    if (auto* A = C.Actor.Get()) { A->Release(); A->Destroy(); }
    C.Actor.Reset();
    if (C.LoadHandle) { C.LoadHandle->CancelHandle(); C.LoadHandle->ReleaseHandle(); C.LoadHandle.Reset(); }
    C.Recipe.Reset();
}

void AEWChunkManager::EndPlay(const EEndPlayReason::Type Reason)
{
    ++Epoch;
    for (auto& Pair : Loaded) Retire(Pair.Value);
    Retire(Horizon);
    Loaded.Empty(); Queue.Empty(); Desired.Empty();
    // Jobs own only value data. Their results are never applied after EndPlay.
    Jobs.Empty();
    Super::EndPlay(Reason);
}

void AEWChunkManager::StartWorld(const EW::WorldDescriptor& InWorld, const EW::PlaceBookmark& Destination)
{
    if (!InWorld.Valid() || !Destination.Coord.Valid()) return;
    ++Epoch;
    for (auto& Pair : Loaded) Retire(Pair.Value);
    Retire(Horizon);
    Loaded.Empty(); Queue.Empty(); Desired.Empty();
    World = InWorld; Origin = Destination.Coord; PendingDestination = Destination;
    PendingDestination.WorldCode = World.Code();
    bHasWorld = true; bTravelling = true; LastCentre = {MAX_int64, MAX_int64};
    if (auto* Player = Character())
    {
        Player->SetStreamingHold(true);
        Player->SetActorLocation(Destination.LocalPosition + FVector(0, 0, 120), false, nullptr, ETeleportType::TeleportPhysics);
        if (auto* PC = Cast<APlayerController>(Player->GetController())) PC->SetControlRotation(FRotator(-8, Destination.Yaw, 0));
    }
    RefreshDesired();
    UE_LOG(LogTemp, Display, TEXT("EW_WORLD_START code=%s chunk=%s epoch=%llu"), *World.Code(), *Destination.Coord.Text(), Epoch);
}

EW::ChunkCoord AEWChunkManager::PlayerCoord() const
{
    if (bTravelling) return PendingDestination.Coord;
    if (auto* Player = Character())
    {
        const FVector P = Player->GetActorLocation();
        return {Origin.X + EW::FloorDiv(int64(FMath::FloorToDouble(P.X)), EW::ChunkSize),
                Origin.Y + EW::FloorDiv(int64(FMath::FloorToDouble(P.Y)), EW::ChunkSize)};
    }
    return Origin;
}

FVector AEWChunkManager::ToRender(EW::ChunkCoord C, const FVector& Local) const
{
    return FVector(double(C.X - Origin.X) * EW::ChunkSize, double(C.Y - Origin.Y) * EW::ChunkSize, 0) + Local;
}

FVector AEWChunkManager::ToLocal(EW::ChunkCoord C, const FVector& Render) const
{
    return Render - FVector(double(C.X - Origin.X) * EW::ChunkSize, double(C.Y - Origin.Y) * EW::ChunkSize, 0);
}

void AEWChunkManager::RefreshDesired()
{
    const auto Centre = PlayerCoord();
    if (Centre == LastCentre) return;
    LastCentre = Centre; Desired.Empty(); Queue.Empty();
    for (int32 Y = -3; Y <= 3; ++Y)
        for (int32 X = -3; X <= 3; ++X)
        {
            EW::ChunkCoord C{Centre.X + X, Centre.Y + Y};
            if (C.Valid()) Desired.Add(C, FMath::Max(FMath::Abs(X), FMath::Abs(Y)) <= 1 ? 2 :
                FMath::Max(FMath::Abs(X), FMath::Abs(Y)) <= 2 ? 1 : 0);
        }
    for (auto It = Loaded.CreateIterator(); It; ++It)
    {
        const uint8* Detail = Desired.Find(It.Key());
        if (!Detail) { Retire(It.Value()); It.RemoveCurrent(); }
        else if (*Detail != It.Value().Detail)
        {
            // Keep the deterministic recipe but release geometry and asset references for the old detail level.
            auto Recipe = It.Value().Recipe;
            Retire(It.Value());
            It.Value().Recipe = MoveTemp(Recipe); It.Value().Detail = *Detail;
        }
    }
    PeakResident = FMath::Max(PeakResident, Loaded.Num());
    RefreshHorizon();
}

void AEWChunkManager::RefreshHorizon()
{
    // Distant scenery has no simulation or collision. The identical part appears in
    // its real chunk as the player approaches; there is no painted world boundary.
    Retire(Horizon);
    auto Recipe=MakeShared<EW::ChunkRecipe>(); Recipe->World=World; Recipe->Coord=Origin;
    const auto Centre=PlayerCoord();
    // The city continues as real adjoining blocks past the detailed walking
    // radius. The same deterministic geometry is used when a chunk moves near.
    for (int32 Y=-6;Y<=6;++Y) for (int32 X=-6;X<=6;++X)
    {
        if (FMath::Abs(X)<=3 && FMath::Abs(Y)<=3) continue;
        const EW::ChunkCoord C{Centre.X+X,Centre.Y+Y};
        if (!C.Valid()) continue;
        const bool Cascade=EWOuterWater::Contains(World,C);
        if(!Cascade && EW::RegionAt(World,C)!=EW::RegionKind::City)continue;
        const int32 First=Recipe->Parts.Num();
        if(Cascade)EWOuterWater::Skyline(World,C,Recipe->Parts);else EW::CityVolumeParts(World,C,Recipe->Parts);
        for (int32 I=First;I<Recipe->Parts.Num();++I)
            Recipe->Parts[I].Transform.AddToTranslation(ToRender(C,FVector::ZeroVector));
    }
    const int64 MX=EW::FloorDiv(Centre.X,4), MY=EW::FloorDiv(Centre.Y,4);
    TArray<EW::ChunkCoord> Candidates;
    for (int32 Y=-5;Y<=5;++Y) for (int32 X=-5;X<=5;++X)
    {
        EW::ChunkCoord C{(MX+X)*4+2,(MY+Y)*4+2};
        if (!C.Valid() || (FMath::Abs(C.X-Centre.X)<=3 && FMath::Abs(C.Y-Centre.Y)<=3)) continue;
        Candidates.Add(C);
    }
    for (const EW::ChunkCoord C : {EW::ChunkCoord{1,1},EW::ChunkCoord{2,0},EW::ChunkCoord{5,1},EW::ChunkCoord{1,5}})
        if (FMath::Abs(C.X-Centre.X)<=24 && FMath::Abs(C.Y-Centre.Y)<=24 &&
            !(FMath::Abs(C.X-Centre.X)<=3 && FMath::Abs(C.Y-Centre.Y)<=3)) Candidates.AddUnique(C);
    Candidates.Sort([Centre](const EW::ChunkCoord& A,const EW::ChunkCoord& B)
    {
        const int64 DA=(A.X-Centre.X)*(A.X-Centre.X)+(A.Y-Centre.Y)*(A.Y-Centre.Y);
        const int64 DB=(B.X-Centre.X)*(B.X-Centre.X)+(B.Y-Centre.Y)*(B.Y-Centre.Y);
        return DA!=DB ? DA<DB : A.X!=B.X ? A.X<B.X : A.Y<B.Y;
    });
    for (int32 I=0;I<FMath::Min(121,Candidates.Num());++I)
    {
        const EW::ChunkCoord C=Candidates[I];
        EW::Part P;
        if (EW::ScenicPart(World,C,P))
        { P.Transform.AddToTranslation(ToRender(C,FVector::ZeroVector)); Recipe->Parts.Add(P); }
    }
    Horizon.Recipe=MoveTemp(Recipe); Horizon.Detail=0;
}

void AEWChunkManager::CollectJobs()
{
    for (int32 I = Jobs.Num() - 1; I >= 0; --I)
    {
        if (!Jobs[I].Future.IsReady()) continue;
        EW::ChunkRecipe Recipe = Jobs[I].Future.Get();
        const EW::ChunkCoord Coord = Jobs[I].Coord;
        if (Jobs[I].Epoch == Epoch && Desired.Contains(Coord) && !Loaded.Contains(Coord))
        {
            FString Error;
            if (Recipe.Validate(Error))
            {
                FEWLoadedChunk Chunk; Chunk.Detail = Desired.FindChecked(Coord);
                Chunk.Recipe = MakeShared<EW::ChunkRecipe>(MoveTemp(Recipe));
                Chunk.LowestWalkZ=Chunk.Recipe->LowestWalkSurface();
                Loaded.Add(Coord, MoveTemp(Chunk)); ++Generated;
            }
            else UE_LOG(LogTemp, Error, TEXT("EW_INVALID_RECIPE coord=%s reason=%s"), *Coord.Text(), *Error);
        }
        else ++Discarded;
        Jobs.RemoveAtSwap(I);
    }
    PeakResident = FMath::Max(PeakResident, Loaded.Num());
}

void AEWChunkManager::FillQueue()
{
    TArray<EW::ChunkCoord> Candidates;
    for (const auto& Pair : Desired)
    {
        if (Loaded.Contains(Pair.Key)) continue;
        bool Running = false;
        for (const auto& Job : Jobs) if (Job.Epoch == Epoch && Job.Coord == Pair.Key) Running = true;
        if (!Running) Candidates.Add(Pair.Key);
    }
    const auto Centre = PlayerCoord();
    Candidates.Sort([Centre](const EW::ChunkCoord& A, const EW::ChunkCoord& B)
    {
        const int64 AD = (A.X - Centre.X) * (A.X - Centre.X) + (A.Y - Centre.Y) * (A.Y - Centre.Y);
        const int64 BD = (B.X - Centre.X) * (B.X - Centre.X) + (B.Y - Centre.Y) * (B.Y - Centre.Y);
        return AD != BD ? AD < BD : A.Y != B.Y ? A.Y < B.Y : A.X < B.X;
    });
    Queue.Reset();
    for (int32 I = 0; I < FMath::Min(EW::MaxQueue, Candidates.Num()); ++I) Queue.Add(Candidates[I]);
    PeakQueue = FMath::Max(PeakQueue, Queue.Num());
}

void AEWChunkManager::LaunchJobs()
{
    while (Jobs.Num() < EW::MaxJobs && !Queue.IsEmpty())
    {
        const auto C = Queue[0]; Queue.RemoveAt(0);
        const EW::WorldDescriptor W = World;
        FEWGenerationJob Job; Job.Coord = C; Job.Epoch = Epoch;
        Job.Future = Async(EAsyncExecution::ThreadPool, [W, C]() { return EW::GenerateChunk(W, C); });
        Jobs.Add(MoveTemp(Job));
    }
    PeakJobs = FMath::Max(PeakJobs, Jobs.Num());
}

void AEWChunkManager::ApplyChunks()
{
    TArray<EW::ChunkCoord> Order;
    Loaded.GetKeys(Order); const auto Centre = PlayerCoord();
    Order.Sort([Centre](const EW::ChunkCoord& A, const EW::ChunkCoord& B)
    {
        int64 AD = (A.X - Centre.X) * (A.X - Centre.X) + (A.Y - Centre.Y) * (A.Y - Centre.Y);
        int64 BD = (B.X - Centre.X) * (B.X - Centre.X) + (B.Y - Centre.Y) * (B.Y - Centre.Y);
        return AD != BD ? AD < BD : A.X != B.X ? A.X < B.X : A.Y < B.Y;
    });
    const double Started = FPlatformTime::Seconds(), Deadline = Started + .002;
    for (const auto C : Order)
    {
        FEWLoadedChunk& Chunk = Loaded.FindChecked(C);
        ApplyOne(Chunk,ToRender(C,FVector::ZeroVector),Deadline);
        if (FPlatformTime::Seconds() >= Deadline) break;
    }
    if (FPlatformTime::Seconds()<Deadline) ApplyOne(Horizon,FVector::ZeroVector,Deadline);
    LastApplyMilliseconds = (FPlatformTime::Seconds() - Started) * 1000;
    MaxApplyMilliseconds = FMath::Max(MaxApplyMilliseconds, LastApplyMilliseconds);
}

void AEWChunkManager::ApplyOne(FEWLoadedChunk& Chunk,const FVector& Position,double Deadline)
{
    if (!Chunk.Recipe) return;
    if (!Chunk.LoadHandle)
    {
        TArray<FSoftObjectPath> Paths;
        for (const auto& Part:Chunk.Recipe->Parts) if (Part.Detail<=Chunk.Detail)
            Paths.AddUnique(FSoftObjectPath(TEXT("/Game/EndlessWorld/Kit/SM_")+Part.Mesh.ToString()+TEXT(".SM_")+Part.Mesh.ToString()));
        if(Chunk.Detail==2 && !Chunk.Recipe->Lifts.IsEmpty())
            Paths.AddUnique(FSoftObjectPath(TEXT("/Game/EndlessWorld/Kit/SM_UrbanLiftCabin.SM_UrbanLiftCabin")));
        if(Chunk.Detail==2 && !Chunk.Recipe->Residences.IsEmpty())AEWResidence::AppendAssetPaths(Paths);
        if(Chunk.Detail==2 && (Chunk.Recipe->bWaterCity || Chunk.Recipe->bCascadeCity))AEWWaterCity::AppendAssetPaths(Paths);
        Chunk.LoadHandle=Loader.RequestAsyncLoad(Paths,FStreamableDelegate());
    }
    if (!Chunk.LoadHandle || !Chunk.LoadHandle->HasLoadCompleted()) return;
    auto* Actor=Chunk.Actor.Get();
    if (!Actor)
    {
        FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        Actor=GetWorld()->SpawnActor<AEWChunkActor>(Position,FRotator::ZeroRotator,Params);
        Actor->BeginApply(Chunk.Recipe,Chunk.Detail); Chunk.Actor=Actor;
    }
    if(auto* P=Character())Actor->SetCollisionFocus(P->GetActorLocation().Z);
    if (Actor->NeedsApply()) Actor->ApplyUntil(Deadline);
}

bool AEWChunkManager::ReadyAt(EW::ChunkCoord C) const
{
    const auto* L = Loaded.Find(C);
    return L && L->Detail == 2 && L->Actor.IsValid() && L->Actor->IsComplete();
}

TSharedPtr<const EW::ChunkRecipe> AEWChunkManager::RecipeAt(EW::ChunkCoord C) const
{
    const auto* L = Loaded.Find(C); return L ? L->Recipe : nullptr;
}

void AEWChunkManager::FinishTravelIfReady()
{
    if (!bTravelling) return;
    const auto C = PendingDestination.Coord;
    for (int32 Y = -1; Y <= 1; ++Y)
        for (int32 X = -1; X <= 1; ++X)
        {
            EW::ChunkCoord N{C.X + X, C.Y + Y};
            if (N.Valid() && !ReadyAt(N)) return;
        }
    auto* Player = Character(); const auto Recipe = RecipeAt(C);
    if (!Player || !Recipe) return;
    FVector Safe = Recipe->SafePosition(PendingDestination.LocalPosition);
    FVector Render = ToRender(C, Safe);
    FHitResult Hit;
    FCollisionQueryParams Query(SCENE_QUERY_STAT(EWArrivalFloor), false, Player);
    if (GetWorld()->LineTraceSingleByChannel(Hit, Render, Render - FVector(0, 0, 1000), ECC_Visibility, Query)
        && Hit.Component.IsValid() && Hit.Component->ComponentHasTag(TEXT("EWFloor")))
        Render.Z = Hit.ImpactPoint.Z + Player->GetCapsuleComponent()->GetScaledCapsuleHalfHeight() + 3;
    else return;
    Player->SetActorLocation(Render, false, nullptr, ETeleportType::TeleportPhysics);
    Player->ResetSafeLocation();
    Player->GetCharacterMovement()->StopMovementImmediately();
    bTravelling = false; Player->SetStreamingHold(false);
    if (auto* GI = GetGameInstance<UEWGameInstance>()) GI->TravelFinished();
    UE_LOG(LogTemp, Display, TEXT("EW_WORLD_READY chunk=%s ready=%d"), *C.Text(), ReadyCount());
}

void AEWChunkManager::RebaseIfNeeded()
{
    if (bTravelling || (Character() && Character()->IsLiftRiding())) return;
    const auto C = PlayerCoord();
    if (FMath::Abs(C.X - Origin.X) < RebaseDistance && FMath::Abs(C.Y - Origin.Y) < RebaseDistance) return;
    const FVector Shift(double(C.X - Origin.X) * EW::ChunkSize, double(C.Y - Origin.Y) * EW::ChunkSize, 0);
    if (auto* Player = Character())
    {
        Player->ShiftLocalOrigin(-Shift);
    }
    for (auto& Pair : Loaded)
        if (auto* A = Pair.Value.Actor.Get()) A->AddActorWorldOffset(-Shift, false, nullptr, ETeleportType::TeleportPhysics);
    if (auto* A=Horizon.Actor.Get()) A->AddActorWorldOffset(-Shift,false,nullptr,ETeleportType::TeleportPhysics);
    // Shift moving bases before CharacterMovement records their new origin.
    // Otherwise a yacht carries its passenger a second time on the next tick.
    if(auto* G=GetGameInstance<UEWGameInstance>())
    {
        for(const auto A:G->Yachts)if(A)A->ApplyWorldOffset(-Shift,false);
        if(G->Skyport)G->Skyport->ApplyWorldOffset(-Shift,false);
        if(G->Skyrail)G->Skyrail->ApplyWorldOffset(-Shift,false);
        if(G->SkyrailUpper)G->SkyrailUpper->ApplyWorldOffset(-Shift,false);
    }
    if (auto* Player=Character()) Player->GetCharacterMovement()->SaveBaseLocation();
    Origin = C; ++RebaseCount;
    UE_LOG(LogTemp, Display, TEXT("EW_REBASE origin=%s count=%d"), *Origin.Text(), RebaseCount);
}

bool AEWChunkManager::TryCurrentPosition(EW::PlaceBookmark& Out) const
{
    const auto* Player = Character();
    if (!bHasWorld || bTravelling || !IsValid(Player) || Player->IsActorBeingDestroyed()) return false;
    const auto P = CurrentPosition();
    if (!P.Coord.Valid() || P.LocalPosition.ContainsNaN() || !FMath::IsFinite(P.Yaw)) return false;
    Out = P; return true;
}

EW::PlaceBookmark AEWChunkManager::CurrentPosition() const
{
    EW::PlaceBookmark P; P.WorldCode = World.Code(); P.Coord = PlayerCoord();
    P.Id = EW::HashText(P.WorldCode + TEXT("|position|") + P.Coord.Text()).Left(32);
    P.Name = TEXT("前回の場所"); P.Kind = int32(EW::RegionAt(World, P.Coord)) * 3;
    if (auto* Player = Character())
    {
        if(const auto* G=GetGameInstance<UEWGameInstance>())if(G->YachtFor(Player))
        {
            // A moving ship is not a persistent world floor. Resume at its
            // reachable port; never load a save into empty sky after it leaves.
            return EWAeroYachtPlan::Arrival();
        }
        const FTransform Bookmark=Player->BookmarkTransform();
        const FVector B=Bookmark.GetLocation();
        P.Coord=Origin+EW::ChunkCoord{int64(FMath::FloorToDouble(B.X/EW::ChunkSize)),int64(FMath::FloorToDouble(B.Y/EW::ChunkSize))};
        P.Id=EW::HashText(P.WorldCode+TEXT("|position|")+P.Coord.Text()).Left(32);
        P.Kind=int32(EW::RegionAt(World,P.Coord))*3;
        P.LocalPosition = ToLocal(P.Coord, B);
        if(Player->IsSeated())P.Yaw=Bookmark.Rotator().Yaw;
        else if (auto* PC = Cast<APlayerController>(Player->GetController())) P.Yaw = PC->GetControlRotation().Yaw;
    }
    return P;
}

TOptional<EW::PlaceBookmark> AEWChunkManager::NearestPlace(double MaxDistance) const
{
    TOptional<EW::PlaceBookmark> Result;
    const auto* Player = Character(); if (!Player || bTravelling) return Result;
    double Best = MaxDistance * MaxDistance;
    for (const auto& Pair : Loaded)
        if (Pair.Value.Recipe && ReadyAt(Pair.Key))
            for (const auto& Place : Pair.Value.Recipe->Places)
            {
                const double D = FVector::DistSquared(ToRender(Place.Coord, Place.LocalPosition), Player->GetActorLocation());
                if (D <= Best && (Place.Kind<9 || D<=650.*650.)) { Best = D; Result = Place; }
            }
    return Result;
}

AEWLift* AEWChunkManager::NearestLift() const
{
    const auto* P=Character();if(!P || bTravelling)return nullptr;
    AEWLift* Best=nullptr;double Distance=DBL_MAX;
    for(const auto& Pair:Loaded)if(auto* A=Pair.Value.Actor.Get())for(const auto& L:A->Lifts)
    {
        int32 Stop;bool OnCar;
        if(L && L->Nearby(P,Stop,OnCar))
        {
            const double D=OnCar?0:FVector::DistSquared(L->LandingPosition(Stop),P->GetActorLocation());
            if(D<Distance){Best=L;Distance=D;}
        }
    }
    return Best;
}

AEWResidence* AEWChunkManager::NearestResidence(EEWResidenceAction& Action) const
{
    Action=EEWResidenceAction::None;
    const auto* P=Character();if(!P || bTravelling)return nullptr;
    AEWResidence* Best=nullptr;double Distance=DBL_MAX;
    for(const auto& Pair:Loaded)if(ReadyAt(Pair.Key))if(auto* A=Pair.Value.Actor.Get())
        for(const auto& Home:A->Residences)if(IsValid(Home))
        {
            double D;const auto Candidate=Home->Nearby(P,D);
            if(Candidate!=EEWResidenceAction::None && D<Distance){Best=Home;Action=Candidate;Distance=D;}
        }
    return Best;
}

float AEWChunkManager::TravelProgress() const
{
    if (!bTravelling) return 1;
    int32 Ready = 0, Total = 0;
    for (int32 Y = -1; Y <= 1; ++Y)
        for (int32 X = -1; X <= 1; ++X)
        {
            EW::ChunkCoord C{PendingDestination.Coord.X + X, PendingDestination.Coord.Y + Y};
            if (C.Valid()) { ++Total; if (ReadyAt(C)) ++Ready; }
        }
    return Total ? float(Ready) / Total : 0;
}

int32 AEWChunkManager::ReadyCount() const
{
    int32 N = 0;
    for (const auto& P : Loaded) if (P.Value.Actor.IsValid() && P.Value.Actor->IsComplete()) ++N;
    return N;
}
double AEWChunkManager::FallRecoveryHeight() const
{
    // Floors below the active collision band still count: crossing an upper
    // gallery edge must not recover the player before lower floors stream in.
    const auto Centre=PlayerCoord();
    if(EWOuterWater::Contains(World,Centre))return EWOuterWater::WaterHeight(World,Centre)-150;
    double Lowest=DBL_MAX;
    for(const auto& Pair:Loaded)
        if(Pair.Value.Recipe && FMath::Abs(Pair.Key.X-Centre.X)<=1 && FMath::Abs(Pair.Key.Y-Centre.Y)<=1)
            Lowest=FMath::Min(Lowest,Pair.Value.LowestWalkZ);
    return Lowest==DBL_MAX ? -DBL_MAX : Lowest-1200.;
}
int32 AEWChunkManager::InstanceCount() const
{
    int32 N = 0; for (const auto& P : Loaded) if (P.Value.Actor.IsValid()) N += P.Value.Actor->InstanceCount(); return N;
}
int32 AEWChunkManager::ColliderCount() const
{
    int32 N = 0; for (const auto& P : Loaded) if (P.Value.Actor.IsValid()) N += P.Value.Actor->ColliderCount(); return N;
}

void AEWChunkManager::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (!bHasWorld) return;
    RebaseIfNeeded(); RefreshDesired(); CollectJobs(); FillQueue(); LaunchJobs(); ApplyChunks(); FinishTravelIfReady();
    if (!bTravelling)
        if (auto* Player = Character())
        {
            const FVector Predicted = Player->GetActorLocation() + Player->GetVelocity() * .35;
            EW::ChunkCoord Next{Origin.X + EW::FloorDiv(int64(FMath::FloorToDouble(Predicted.X)), EW::ChunkSize),
                                Origin.Y + EW::FloorDiv(int64(FMath::FloorToDouble(Predicted.Y)), EW::ChunkSize)};
            const auto* G=GetGameInstance<UEWGameInstance>();
            const bool OnTransit=G && (G->YachtFor(Player) || (G->Skyrail && G->Skyrail->IsRider(Player)) || (G->SkyrailUpper && G->SkyrailUpper->IsRider(Player)));
            Player->SetStreamingHold(!OnTransit && (!ReadyAt(PlayerCoord()) || !Next.Valid() || !ReadyAt(Next)));
        }
    ensureMsgf(Loaded.Num() <= EW::MaxResidentChunks && Jobs.Num() <= EW::MaxJobs && Queue.Num() <= EW::MaxQueue,
               TEXT("Endless World residency budget exceeded"));
}
