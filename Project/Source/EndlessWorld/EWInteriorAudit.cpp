#include "EWInteriorAudit.h"
#include "EWInteriors.h"
#include "EWClock.h"
#include "EWChunkManager.h"
#include "EWGameInstance.h"
#include "EWCharacter.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/PointLightComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/Parse.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

namespace
{
void Write(const FString& Path,const TSharedRef<FJsonObject>& O)
{
    FString S;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&S));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path),true);
    FFileHelper::SaveStringToFile(S,*Path,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
}
bool Clear(const EW::ChunkRecipe& R,FVector Feet,FString& Reason)
{
    if(Feet.ContainsNaN()){Reason=TEXT("non-finite route point");return false;}
    const FVector Centre=Feet+FVector(0,0,90);
    bool Supported=false;
    for(const auto& C:R.Colliders)
    {
        const auto Q=C.Transform.InverseTransformPosition(Centre);
        if(FMath::Abs(Q.X)<C.Extent.X+38-.1 && FMath::Abs(Q.Y)<C.Extent.Y+38-.1 && FMath::Abs(Q.Z)<C.Extent.Z+88-.1)
        {Reason=TEXT("blocked ")+Centre.ToString()+TEXT(" at ")+C.Transform.GetLocation().ToString()+TEXT(" extent ")+C.Extent.ToString();return false;}
        const auto F=C.Transform.InverseTransformPosition(Feet);
        if(C.bWalkSurface && FMath::Abs(F.X)<=C.Extent.X && FMath::Abs(F.Y)<=C.Extent.Y && FMath::Abs(F.Z-C.Extent.Z)<4)Supported=true;
    }
    if(!Supported)Reason=TEXT("unsupported ")+Feet.ToString();return Supported;
}
}
UEWInteriorAuditCommandlet::UEWInteriorAuditCommandlet()
{IsClient=false;IsServer=false;IsEditor=true;LogToConsole=true;ShowErrorCount=true;}
int32 UEWInteriorAuditCommandlet::Main(const FString& Params)
{
    FString Output;if(!FParse::Value(*Params,TEXT("EWReport="),Output) || IFileManager::Get().FileExists(*Output))return 2;
    auto Report=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Failures,Layouts;
    int32 Counts[19]={},Samples=0,MaxBoxes=0,Chunks=0,Storeys=0,Hints=0,CinemaRoutes=0;
    const auto World=EW::WorldDescriptor::ReferenceWorld();
    for(int32 Y=-2;Y<=2;++Y)for(int32 X=-2;X<=2;++X)
    {
        const auto R=EW::GenerateChunk(World,{X,Y});FString Error;
        if(!R.Validate(Error) || R.bFallback){Failures.Add(MakeShared<FJsonValueString>(R.Coord.Text()+TEXT(" fallback ")+Error));continue;}
        if(R.Region!=EW::RegionKind::City)continue;++Chunks;MaxBoxes=FMath::Max(MaxBoxes,R.Colliders.Num());
        TMap<FString,TSet<int32>> Facades;
        for(const auto& Room:R.Interiors)
        {
            ++Counts[Room.Kind];if(Room.Kind>=11)continue; // Hotel83 has its own swept runtime routes.
            const auto Points=EWInteriors::Route(Room);bool OK=true;
            // The cinema has real stair treads. Its swept CharacterMovement
            // route is checked by EWCinemaAudit, rather than this flat-floor sampler.
            if(Room.Kind==10){++CinemaRoutes;continue;}
            if(Room.Kind!=4 && Room.Kind!=5)
            {
                Facades.FindOrAdd(Room.Frame.GetLocation().ToString()).Add(FMath::RoundToInt(Room.Frame.Rotator().Yaw));
                ++Hints;
                if(!EWInteriors::NearbyText(R,Points[0]+FVector(0,0,90)).StartsWith(EWInteriors::Name(Room.Kind)))
                    Failures.Add(MakeShared<FJsonValueString>(R.Coord.Text()+TEXT(" entrance hint mismatch ")+EWInteriors::Name(Room.Kind)));
            }
            for(int32 I=1;I<Points.Num();++I)
            {
                const int32 N=FMath::Max(1,FMath::CeilToInt(FVector::Dist2D(Points[I-1],Points[I])/35.));
                for(int32 J=0;J<=N;++J)
                {
                    ++Samples;if(!Clear(R,FMath::Lerp(Points[I-1],Points[I],double(J)/N),Error))
                    {OK=false;break;}
                }
                if(!OK)break;
            }
            if(!OK)Failures.Add(MakeShared<FJsonValueString>(R.Coord.Text()+TEXT(" ")+EWInteriors::Name(Room.Kind)+TEXT(" ")+Room.Frame.GetLocation().ToString()+TEXT(" ")+Error));
            if(X==0 && Y==0)
            {
                auto L=MakeShared<FJsonObject>();L->SetNumberField(TEXT("kind"),Room.Kind);L->SetNumberField(TEXT("variant"),Room.Variant);
                L->SetStringField(TEXT("frame"),Room.Frame.ToString());L->SetBoolField(TEXT("route_clear"),OK);Layouts.Add(MakeShared<FJsonValueObject>(L));
                EW::PlaceBookmark Entry;Entry.WorldCode=World.Code();Entry.Coord=R.Coord;Entry.Name=EWInteriors::Name(Room.Kind);Entry.Id=TEXT("interior-entry");
                Entry.LocalPosition=Points[0]+FVector(0,0,90);Entry.Yaw=(Points[1]-Points[0]).Rotation().Yaw;
                L->SetStringField(TEXT("entry_code"),Entry.Code());L->SetNumberField(TEXT("height_cm"),Room.Frame.GetLocation().Z);
                L->SetNumberField(TEXT("arrival_distance_cm"),FVector::Dist2D(Entry.LocalPosition,R.Hub));
            }
        }
        for(const auto& Floor:Facades)
        {
            ++Storeys;
            if(Floor.Value.Num()!=4)Failures.Add(MakeShared<FJsonValueString>(R.Coord.Text()+TEXT(" public floor missing a facade entrance ")+Floor.Key));
        }
    }
    const auto Plaza=EW::GenerateChunk(World,{0,0});
    if(!FMath::IsNearlyEqual(EWClock::ScreenCentre(Plaza).Z,3019.,.01))Failures.Add(MakeShared<FJsonValueString>(TEXT("monitor height mismatch")));
    TArray<TSharedPtr<FJsonValue>> KindCounts;for(int32 N:Counts){KindCounts.Add(MakeShared<FJsonValueNumber>(N));if(N==0)Failures.Add(MakeShared<FJsonValueString>(TEXT("missing room kind")));}
    Report->SetBoolField(TEXT("success"),Failures.IsEmpty());Report->SetArrayField(TEXT("failures"),Failures);
    Report->SetArrayField(TEXT("kind_counts"),KindCounts);Report->SetArrayField(TEXT("central_rooms"),Layouts);
    Report->SetNumberField(TEXT("capsule_samples"),Samples);Report->SetNumberField(TEXT("city_chunks"),Chunks);Report->SetNumberField(TEXT("max_colliders"),MaxBoxes);
    Report->SetNumberField(TEXT("four_sided_apartment_storeys"),Storeys);Report->SetNumberField(TEXT("entrance_hints_checked"),Hints);
    Report->SetNumberField(TEXT("stair_routes_checked_by_cinema_runtime_audit"),CinemaRoutes);
    Report->SetStringField(TEXT("screen_centre"),EWClock::ScreenCentre(Plaza).ToString());Write(Output,Report);
    return Failures.IsEmpty()?0:1;
}

AEWInteriorAudit::AEWInteriorAudit()
{PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostPhysics;}
void AEWInteriorAudit::BeginPlay()
{
    Super::BeginPlay();Started=Stage=FPlatformTime::Seconds();
    if(!FParse::Value(FCommandLine::Get(),TEXT("EWReport="),Report) || IFileManager::Get().FileExists(*Report))
    {Done=true;FPlatformMisc::RequestExit(false);return;}
    SkipPhotos=FParse::Param(FCommandLine::Get(),TEXT("EWNoInteriorPhotos"));VisualOnly=FParse::Param(FCommandLine::Get(),TEXT("EWInteriorVisualOnly"));
}
void AEWInteriorAudit::Finish(const FString& Failure)
{
    if(Done)return;Done=true;auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
    if(P){P->SetTestMovement(FVector::ZeroVector,false);P->ClearHeldInput();}
    if(auto* PC=UGameplayStatics::GetPlayerController(this,0))if(P)PC->SetViewTarget(P);
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),Failure.IsEmpty());O->SetStringField(TEXT("failure"),Failure);
    O->SetBoolField(TEXT("visual_only"),VisualOnly);O->SetArrayField(TEXT("rooms"),Results);O->SetArrayField(TEXT("screenshots"),Photos);
    O->SetNumberField(TEXT("walked_metres"),Walked/100.);O->SetNumberField(TEXT("seconds"),FPlatformTime::Seconds()-Started);
    O->SetNumberField(TEXT("fall_recoveries"),P?P->FallRecoveries-InitialFalls:0);Write(Report,O);FPlatformMisc::RequestExit(false);
}
void AEWInteriorAudit::Tick(float Delta)
{
    Super::Tick(Delta);if(Done)return;
    const double Now=FPlatformTime::Seconds();auto* G=GetGameInstance<UEWGameInstance>();auto* M=G?G->Manager.Get():nullptr;
    auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if(Now-Stage>180 || Now-Started>900){Finish(TEXT("interior audit timeout"));return;}
    if(!P || !M || !PC)return;
    auto Ready=[&]{return !M->IsTravelling() && M->ReadyAt({0,0}) && !P->IsStreamingHeld() && P->GetCharacterMovement()->IsMovingOnGround();};
    if(Phase==0)
    {
        if(!Ready())return;const auto R=M->RecipeAt({0,0});if(!R)return;
        if(!G->bScriptedWorldAudit || !G->SessionStarted()){Finish(TEXT("audit input isolation/session missing"));return;}
        for(int32 Kind=0;Kind<10;++Kind)
        {
            const EW::InteriorRoom* BestRoom=nullptr;double Score=DBL_MAX;
            for(const auto& Room:R->Interiors)if(Room.Kind==Kind)
            {
                const double D=FMath::Abs(Room.Frame.GetLocation().Z-R->Hub.Z)*10+FVector::Dist2D(Room.Frame.GetLocation(),R->Hub);
                if(D<Score){Score=D;BestRoom=&Room;}
            }
            if(!BestRoom){Finish(TEXT("missing runtime room kind"));return;}Rooms.Add(*BestRoom);
        }
        if(SkipPhotos)
        {
            // Walk all four doors at one ordinary gallery datum as well as
            // the ten room types. This catches side transforms/collision fills.
            for(const auto& Room:R->Interiors)
                if(Room.Frame.GetLocation().Equals(Rooms[0].Frame.GetLocation(),.01) && Room.Kind!=4 && Room.Kind!=5)
                    Rooms.Add(Room);
        }
        G->SetMenu(EEWMenu::None);InitialFalls=P->FallRecoveries;Phase=1;Stage=Now;return;
    }
    if(P->FallRecoveries!=InitialFalls){Finish(TEXT("fall recovery inside an interior"));return;}
    if(Phase==1)
    {
        if(RoomIndex>=Rooms.Num()){if(SkipPhotos)Finish();else Phase=6;Stage=Now;return;}
        Route=EWInteriors::Route(Rooms[RoomIndex]);P->SetTestMovement(FVector::ZeroVector,false);P->ClearHeldInput();P->SetBase(static_cast<UPrimitiveComponent*>(nullptr));
        P->SetActorLocation(M->ToRender({0,0},Route[0])+FVector(0,0,93),false,nullptr,ETeleportType::TeleportPhysics);
        P->ResetSafeLocation();P->SetStreamingHold(true);P->GetCharacterMovement()->MaxWalkSpeed=350;Point=1;Phase=2;Stage=Now;return;
    }
    if(Phase==2)
    {
        if(!Ready())return;LastPosition=P->GetActorLocation();Best=DBL_MAX;ProgressAt=GetWorld()->GetTimeSeconds();Phase=VisualOnly?4:3;Stage=Now;return;
    }
    if(Phase==3)
    {
        Walked+=FVector::Dist2D(P->GetActorLocation(),LastPosition);LastPosition=P->GetActorLocation();
        if(Point>=Route.Num())
        {
            P->SetTestMovement(FVector::ZeroVector,false);P->ClearHeldInput();auto Row=MakeShared<FJsonObject>();
            Row->SetStringField(TEXT("kind"),EWInteriors::Name(Rooms[RoomIndex].Kind));Row->SetStringField(TEXT("frame"),Rooms[RoomIndex].Frame.ToString());
            Row->SetBoolField(TEXT("walk_pass"),true);Results.Add(MakeShared<FJsonValueObject>(Row));Phase=4;return;
        }
        const auto Target=M->ToRender({0,0},Route[Point])+FVector(0,0,90);const double D=FVector::Dist2D(Target,P->GetActorLocation());
        if(D<20)
        {
            if(!Ready() || FMath::Abs(P->GetActorLocation().Z-Target.Z)>12){Finish(TEXT("wrong interior floor"));return;}
            ++Point;Best=DBL_MAX;ProgressAt=GetWorld()->GetTimeSeconds();return;
        }
        if(D<Best-4){Best=D;ProgressAt=GetWorld()->GetTimeSeconds();}
        if(GetWorld()->GetTimeSeconds()-ProgressAt>8){Finish(TEXT("blocked interior route: ")+EWInteriors::Name(Rooms[RoomIndex].Kind)+TEXT(" ")+Target.ToString());return;}
        P->SetTestMovement(Target-P->GetActorLocation(),true);return;
    }
    if(Phase==4)
    {
        if(SkipPhotos){++RoomIndex;Phase=1;Stage=Now;return;}
        if(!Camera)Camera=GetWorld()->SpawnActor<ACameraActor>();
        const auto& Room=Rooms[RoomIndex];const auto& T=Room.Frame;
        const FVector Eye=T.TransformPosition(Room.Kind==4?FVector(150,620,170):FVector(310,-1630,162));
        const FVector Aim=T.TransformPosition(Room.Kind==4?FVector(1050,900,220):FVector(590,-1280,145));
        Camera->SetActorLocationAndRotation(M->ToRender({0,0},Eye),(Aim-Eye).Rotation());Camera->GetCameraComponent()->SetFieldOfView(83);PC->SetViewTarget(Camera);
        PhotoPath=FPaths::GetPath(Report)/(FPaths::GetBaseFilename(Report)+FString::Printf(TEXT("-room-%d.png"),Room.Kind));
        if(IFileManager::Get().FileExists(*PhotoPath)){Finish(TEXT("photo already exists"));return;}
        WaitUntil=Now+4.;Captured=false;Phase=5;Stage=Now;return;
    }
    if(Phase==5)
    {
        if(Now<WaitUntil)return;
        if(!Captured){FScreenshotRequest::RequestScreenshot(PhotoPath,false,false);Captured=true;WaitUntil=Now+2;return;}
        if(!IFileManager::Get().FileExists(*PhotoPath)){Finish(TEXT("photo missing"));return;}
        auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("file"),PhotoPath);O->SetStringField(TEXT("kind"),EWInteriors::Name(Rooms[RoomIndex].Kind));
        TArray<TSharedPtr<FJsonValue>> ActiveLights;
        for(TActorIterator<AEWInteriorLighting> It(GetWorld());It;++It)
        {
            TArray<UPointLightComponent*> Components;It->GetComponents(Components);
            for(const auto* L:Components)if(L->IsVisible())
            {
                auto Light=MakeShared<FJsonObject>();Light->SetStringField(TEXT("position"),L->GetComponentLocation().ToString());
                Light->SetNumberField(TEXT("lumens"),L->Intensity);Light->SetBoolField(TEXT("registered"),L->IsRegistered());
                ActiveLights.Add(MakeShared<FJsonValueObject>(Light));
            }
        }
        O->SetArrayField(TEXT("room_lights"),ActiveLights);
        O->SetStringField(TEXT("eye"),Camera->GetActorLocation().ToString());Photos.Add(MakeShared<FJsonValueObject>(O));
        PC->SetViewTarget(P);++RoomIndex;Phase=1;Stage=Now;return;
    }
    if(Phase==6)
    {
        if(MonitorSide>=3){Finish();return;}
        if(!Camera)Camera=GetWorld()->SpawnActor<ACameraActor>();
        const auto R=M->RecipeAt({0,0});if(!R)return;
        const auto Aim=M->ToRender({0,0},EWClock::ScreenCentre(*R));
        FVector Eye=Aim+FVector(0,MonitorSide?2400:-2400,-500),Target=Aim;
        if(MonitorSide==2)
        {
            const auto& T=Rooms[0].Frame;
            Eye=M->ToRender({0,0},T.TransformPosition(FVector(-50,-2320,175)));
            Target=M->ToRender({0,0},T.TransformPosition(FVector(240,-1620,170)));
        }
        Camera->SetActorLocationAndRotation(Eye,(Target-Eye).Rotation());Camera->GetCameraComponent()->SetFieldOfView(73);PC->SetViewTarget(Camera);
        PhotoPath=FPaths::GetPath(Report)/(FPaths::GetBaseFilename(Report)+(MonitorSide==2?TEXT("-open-entrance.png"):MonitorSide?TEXT("-monitor-back.png"):TEXT("-monitor-front.png")));
        if(IFileManager::Get().FileExists(*PhotoPath)){Finish(TEXT("monitor photo already exists"));return;}
        WaitUntil=Now+4;Captured=false;Phase=7;return;
    }
    if(Phase==7)
    {
        if(Now<WaitUntil)return;
        if(!Captured){FScreenshotRequest::RequestScreenshot(PhotoPath,false,false);Captured=true;WaitUntil=Now+2;return;}
        if(!IFileManager::Get().FileExists(*PhotoPath)){Finish(TEXT("monitor photo missing"));return;}
        auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("file"),PhotoPath);O->SetStringField(TEXT("kind"),MonitorSide==2?TEXT("回廊の開いた入口"):MonitorSide?TEXT("モニター裏面"):TEXT("モニター表面"));
        Photos.Add(MakeShared<FJsonValueObject>(O));++MonitorSide;Phase=6;Stage=Now;return;
    }
}
