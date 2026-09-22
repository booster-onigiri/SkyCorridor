#include "EWWaterCityRuntimeAudit.h"
#include "EWWaterCity.h"
#include "EWCharacter.h"
#include "EWChunkManager.h"
#include "EWGameInstance.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/AudioComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

AEWWaterCityRuntimeAudit::AEWWaterCityRuntimeAudit()
{PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostPhysics;}
AEWCharacter* AEWWaterCityRuntimeAudit::Player() const
{return Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));}
AEWChunkManager* AEWWaterCityRuntimeAudit::Manager() const
{const auto* GI=GetGameInstance<UEWGameInstance>();return GI?GI->Manager.Get():nullptr;}
bool AEWWaterCityRuntimeAudit::Ready() const
{
    const auto* P=Player();const auto* M=Manager();
    return P && M && !M->IsTravelling() && !P->IsStreamingHeld() && M->ReadyAt(M->PlayerCoord()) && P->GetCharacterMovement()->IsMovingOnGround();
}
void AEWWaterCityRuntimeAudit::BeginPlay()
{
    Super::BeginPlay();Started=StageStart=FPlatformTime::Seconds();
    ReportPath=FPaths::ProjectSavedDir()/TEXT("Verification/water-city-runtime-")+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT(".json");
    FParse::Value(FCommandLine::Get(),TEXT("EWReport="),ReportPath);StreamPath=FPaths::ChangeExtension(ReportPath,TEXT("jsonl"));
    if(IFileManager::Get().FileExists(*ReportPath) || IFileManager::Get().FileExists(*StreamPath))
    {bFinished=true;UE_LOG(LogTemp,Error,TEXT("EW_WATER_CITY_RUNTIME refuses to overwrite evidence"));FPlatformMisc::RequestExit(false);return;}
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath),true);
    bSkipPhotos=FParse::Param(FCommandLine::Get(),TEXT("EWNoWaterCityScreenshots"));
    bResumeOnly=FParse::Param(FCommandLine::Get(),TEXT("EWWaterCityResumeOnly"));
    bVisualOnly=FParse::Param(FCommandLine::Get(),TEXT("EWWaterCityVisualOnly"));
    for(const auto& W:EW::GetWaterCityPlan().Walks)Routes.Add({W.Id,{W.A,W.B}});
    // These connected journeys exercise corner joins as well as the sixteen
    // individual floor segments. Every setup teleport is explicit and excluded.
    Routes.Add({TEXT("clock-quay-connected-circuit"),{
        FVector(6400,5700,2250),FVector(6400,5200,2250),FVector(7250,5200,2250),FVector(7600,5550,2250),
        FVector(7600,7300,2250),FVector(7250,7650,2250),FVector(6400,7650,2250),FVector(6400,7000,2250),
        FVector(6400,7650,2250),FVector(5500,7650,2250),FVector(5150,7300,2250),FVector(5150,5550,2250),
        FVector(5500,5200,2250),FVector(6400,5200,2250)}});
    for(const double Z:{4050.,7650.})
    {
        const double Y=Z<5000?12100:12850;
        Routes.Add({Z<5000?TEXT("middle-gallery-to-water-front-return"):TEXT("upper-gallery-to-water-front-return"),{
            FVector(5300,9571,Z),FVector(6400,9571,Z),FVector(6400,Y,Z),FVector(5170,Y,Z),
            FVector(7630,Y,Z),FVector(6400,Y,Z),FVector(6400,9571,Z),FVector(5300,9571,Z)}});
    }
    Views={
        {TEXT("wide-east-waterfall"),FVector(7100,5200,2412),FVector(7300,12100,3000)},
        {TEXT("clock-water-a"),FVector(5700,5800,2412),FVector(4850,6400,1950)},
        {TEXT("clock-water-b"),FVector(5700,5800,2412),FVector(4850,6400,1950)},
        {TEXT("middle-water-garden"),FVector(6400,12200,4212),FVector(6780,12900,3850)},
        {TEXT("upper-water-garden"),FVector(6200,12780,7812),FVector(6700,14200,7450)},
        {TEXT("south-cascade-a"),FVector(7100,-1850,2412),FVector(6950,-1240,1350)},
        {TEXT("south-cascade-b"),FVector(7100,-1850,2412),FVector(6950,-1240,1350)},
        {TEXT("window-and-water-square"),FVector(3805,7857.1,2427),FVector(5400,5800,1950)}};
    if(FParse::Param(FCommandLine::Get(),TEXT("EWWaterGardenViews")))
    {
        Views.Add({TEXT("garden-promenade"),FVector(5550,12850,7812),FVector(6450,14700,7610)});
        Views.Add({TEXT("cascade-architecture"),FVector(6400,10900,4212),FVector(6920,13000,5700)});
        Views.Add({TEXT("planted-water-square"),FVector(5150,7100,2412),FVector(4770,7000,2050)});
    }
    Event(TEXT("start"),TEXT("Sixteen new walk segments and three connected journeys, staged real-world cameras, live bounded spray, streamed retirement/recreation and saved upper quay. Human controls, audio quality and visual acceptance are separate."));
}
void AEWWaterCityRuntimeAudit::Event(const FString& Kind,const FString& Detail)
{
    auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("event"),Kind);O->SetStringField(TEXT("detail"),Detail);
    O->SetNumberField(TEXT("phase"),Phase);O->SetNumberField(TEXT("elapsed"),FPlatformTime::Seconds()-Started);
    if(const auto* P=Player()){O->SetStringField(TEXT("position"),P->GetActorLocation().ToString());O->SetBoolField(TEXT("grounded"),P->GetCharacterMovement()->IsMovingOnGround());}
    Evidence.Add(MakeShared<FJsonValueObject>(O));FString JSON;
    FJsonSerializer::Serialize(O,TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&JSON));
    FFileHelper::SaveStringToFile(JSON+TEXT("\n"),*StreamPath,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);
    UE_LOG(LogTemp,Display,TEXT("EW_WATER_CITY_RUNTIME_%s %s"),*Kind.ToUpper(),*Detail);
}
void AEWWaterCityRuntimeAudit::RestorePlayerCamera()
{
    if(auto* PC=UGameplayStatics::GetPlayerController(this,0))if(Player())PC->SetViewTarget(Player());
}
void AEWWaterCityRuntimeAudit::Finish(const FString& Failure)
{
    if(bFinished)return;bFinished=true;RestorePlayerCamera();
    if(auto* P=Player()){P->SetTestMovement(FVector::ZeroVector,false);P->ClearHeldInput();}
    Event(Failure.IsEmpty()?TEXT("pass"):TEXT("failure"),Failure);
    auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("format"),TEXT("water-city-runtime-v1"));
    O->SetBoolField(TEXT("success"),Failure.IsEmpty());O->SetStringField(TEXT("failure"),Failure);
    O->SetArrayField(TEXT("evidence"),Evidence);O->SetArrayField(TEXT("routes"),RouteResults);O->SetArrayField(TEXT("screenshots"),Photos);
    O->SetBoolField(TEXT("resume_only"),bResumeOnly);O->SetBoolField(TEXT("spray_transform_changed"),bMotionSeen);
    O->SetBoolField(TEXT("visual_only"),bVisualOnly);
    O->SetNumberField(TEXT("walked_metres"),Walked/100.);O->SetNumberField(TEXT("seconds"),FPlatformTime::Seconds()-Started);
    O->SetNumberField(TEXT("completed_routes"),RouteResults.Num());O->SetStringField(TEXT("saved_upper_place_code"),SavedUpper.Code());
    if(const auto* P=Player())
    {O->SetNumberField(TEXT("fall_recoveries"),P->FallRecoveries-InitialFalls);O->SetNumberField(TEXT("hardware_forward_events"),P->ForwardEvents);O->SetNumberField(TEXT("hardware_look_events"),P->LookEvents);}
    if(const auto* M=Manager())
    {O->SetNumberField(TEXT("rebase_count"),M->RebaseCount-InitialRebases);O->SetStringField(TEXT("final_chunk"),M->PlayerCoord().Text());O->SetStringField(TEXT("final_local_position"),M->CurrentPosition().LocalPosition.ToString());}
    if(const auto* GI=GetGameInstance<UEWGameInstance>())if(GI->Store())O->SetStringField(TEXT("data_directory"),GI->Store()->Root());
    O->SetStringField(TEXT("not_covered"),TEXT("Screenshots use staged CameraActors and need visual review; their eyes are not all standing locations. GPU water motion is checked from paired images, not CPU transforms. Audio registration is not human hearing. Existing lift traversal and display/performance regressions run separately."));
    FString JSON;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&JSON));
    FFileHelper::SaveStringToFile(JSON,*ReportPath,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);FPlatformMisc::RequestExit(false);
}
bool AEWWaterCityRuntimeAudit::VerifyComponents(const FString& Label)
{
    int32 CityActors=0,DropInstances=0,AudioCount=0,AudioRegistered=0,AudioPlaying=0;TSet<FName> Names;
    for(TActorIterator<AEWWaterCity> It(GetWorld());It;++It)
    {
        ++CityActors;TArray<UInstancedStaticMeshComponent*> Meshes;It->GetComponents(Meshes);
        for(auto* C:Meshes)if(C->GetStaticMesh() && C->GetStaticMesh()->GetFName()==TEXT("SM_WaterCityDrop"))DropInstances+=C->GetInstanceCount();
        TArray<UAudioComponent*> Audio;It->GetComponents(Audio);
        for(const auto* A:Audio){++AudioCount;AudioRegistered+=A->IsRegistered()?1:0;AudioPlaying+=A->IsPlaying()?1:0;}
    }
    for(TActorIterator<AActor> It(GetWorld());It;++It)
    {
        TArray<UStaticMeshComponent*> Meshes;It->GetComponents(Meshes);
        for(const auto* C:Meshes)if(C->GetStaticMesh() && C->GetStaticMesh()->GetName().StartsWith(TEXT("SM_WaterCity")))Names.Add(C->GetStaticMesh()->GetFName());
    }
    Event(TEXT("components"),FString::Printf(TEXT("%s city_actors=%d unique_meshes=%d drop_instances=%d audio=%d registered=%d playing=%d"),
        *Label,CityActors,Names.Num(),DropInstances,AudioCount,AudioRegistered,AudioPlaying));
    if(CityActors!=3 || Names.Num()!=13 || DropInstances!=72 || AudioCount!=6 || AudioRegistered!=6)
    {Finish(TEXT("water-city components or bounded counts do not match the imported plan"));return false;}
    return true;
}
void AEWWaterCityRuntimeAudit::Setup(EW::ChunkCoord Coord,FVector Feet)
{
    auto* P=Player();RestorePlayerCamera();if(P->IsSeated())P->LeaveSeat();
    P->SetTestMovement(FVector::ZeroVector,false);P->ClearHeldInput();P->SetBase(static_cast<UPrimitiveComponent*>(nullptr));
    P->SetActorLocation(Manager()->ToRender(Coord,Feet)+FVector(0,0,91),false,nullptr,ETeleportType::TeleportPhysics);
    P->ResetSafeLocation();P->SetStreamingHold(true);P->GetCharacterMovement()->MaxWalkSpeed=550;
    StageStart=FPlatformTime::Seconds();Event(TEXT("setup_teleport"),Coord.Text()+TEXT(" feet=")+Feet.ToString());
}
void AEWWaterCityRuntimeAudit::BeginRoute()
{
    if(RouteIndex>=Routes.Num()){Setup({0,0},FVector(6400,5150,2250));Phase=10;return;}
    Setup({0,0},Routes[RouteIndex].Feet[0]);Phase=1;PointIndex=1;RouteWalked=0;BestDistance=DBL_MAX;
}
void AEWWaterCityRuntimeAudit::BeginPhoto()
{
    if(PhotoIndex>=Views.Num() || bSkipPhotos)
    {
        RestorePlayerCamera();RetiredCities.Empty();
        for(TActorIterator<AEWWaterCity> It(GetWorld());It;++It)RetiredCities.Add(*It);
        FarCoord={Manager()->RebaseThreshold()+2,0};const auto R=EW::GenerateChunk(Manager()->Descriptor(),FarCoord);
        Setup(FarCoord,R.SafePosition(R.Hub+FVector(0,-1250,88))-FVector(0,0,88));Phase=20;return;
    }
    if(!PhotoCamera)PhotoCamera=GetWorld()->SpawnActor<ACameraActor>();
    if(!PhotoCamera){Finish(TEXT("could not create the staged review camera"));return;}
    const auto& V=Views[PhotoIndex];const FVector Eye=Manager()->ToRender({0,0},V.Eye),Target=Manager()->ToRender({0,0},V.Target);
    PhotoCamera->SetActorLocationAndRotation(Eye,(Target-Eye).Rotation());PhotoCamera->GetCameraComponent()->SetFieldOfView(90);
    UGameplayStatics::GetPlayerController(this,0)->SetViewTarget(PhotoCamera);
    PhotoPath=FPaths::GetPath(ReportPath)/(FPaths::GetBaseFilename(ReportPath)+TEXT("-")+V.Name+TEXT(".png"));
    if(IFileManager::Get().FileExists(*PhotoPath)){Finish(TEXT("screenshot already exists"));return;}
    bCaptured=false;WaitUntil=FPlatformTime::Seconds()+2.5;StageStart=FPlatformTime::Seconds();Phase=11;
}
void AEWWaterCityRuntimeAudit::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);if(bFinished)return;auto* P=Player();auto* M=Manager();auto* GI=GetGameInstance<UEWGameInstance>();
    const double Now=FPlatformTime::Seconds();
    if(Now-StageStart>240 || Now-Started>1500){Finish(TEXT("water-city runtime stage timeout"));return;}
    if(!P || !M || !GI || !GI->Store())return;
    if(Phase>0 && (P->ForwardEvents || P->StrafeEvents || P->LookEvents || P->JumpEvents))
    {Finish(TEXT("live input contaminated the scripted water-city audit"));return;}
    if(Phase>0 && P->FallRecoveries!=InitialFalls){Finish(TEXT("unexpected fall recovery"));return;}
    if(Phase==0)
    {
        if(!GI->bScriptedWorldAudit){Finish(TEXT("scripted input isolation was not enabled"));return;}
        if(M->IsTravelling() || !M->ReadyAt(M->PlayerCoord()))return;
        InitialFalls=P->FallRecoveries;InitialRebases=M->RebaseCount;
        if(bResumeOnly)
        {
            if(GI->SessionStarted() || !GI->Store()->LoadCurrent(SavedUpper))
            {Finish(TEXT("resume requires a saved upper quay and no EWPlay/EWWorld startup flag"));return;}
            GI->ContinueWorld();Phase=90;StageStart=Now;Event(TEXT("resume_requested"),SavedUpper.Code());return;
        }
        if(!GI->SessionStarted()){GI->ReferenceWorld();return;}
        if(!Ready() || !M->ReadyAt({0,-1}) || !M->ReadyAt({0,1}))return;
        if(Routes.Num()!=19 || !VerifyComponents(TEXT("initial")))return;
        if(bVisualOnly)
        {
            Event(TEXT("walking_not_exercised"),TEXT("Material review only; use a separate complete walking run for route acceptance."));
            Setup({0,0},FVector(6400,5150,2250));Phase=10;return;
        }
        BeginRoute();return;
    }
    if(Phase==1)
    {
        if(Now-StageStart<.35 || !Ready())return;
        const FVector Start=M->ToRender({0,0},Routes[RouteIndex].Feet[0])+FVector(0,0,88);
        if(FVector::Dist2D(P->GetActorLocation(),Start)>6 || FMath::Abs(P->GetActorLocation().Z-Start.Z)>10)
        {Finish(TEXT("route setup did not settle on its specified new deck"));return;}
        LastPosition=P->GetActorLocation();RouteStarted=Now;ProgressAt=Now;Phase=2;Event(TEXT("walk_begin"),Routes[RouteIndex].Name);return;
    }
    if(Phase==2)
    {
        if(P->IsStreamingHeld()){LastPosition=P->GetActorLocation();ProgressAt=Now;return;}
        const double Moved=FVector::Dist2D(P->GetActorLocation(),LastPosition);Walked+=Moved;RouteWalked+=Moved;LastPosition=P->GetActorLocation();
        const FVector Target=M->ToRender({0,0},Routes[RouteIndex].Feet[PointIndex])+FVector(0,0,88);
        const double Distance=FVector::Dist2D(Target,P->GetActorLocation());
        if(Distance<23)
        {
            if(!P->GetCharacterMovement()->IsMovingOnGround() || FMath::Abs(P->GetActorLocation().Z-Target.Z)>10)
            {Finish(TEXT("walk target reached without support at the required height"));return;}
            if(++PointIndex>=Routes[RouteIndex].Feet.Num())
            {
                P->SetTestMovement(FVector::ZeroVector,false);P->ClearHeldInput();
                auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("name"),Routes[RouteIndex].Name);
                O->SetBoolField(TEXT("pass"),true);O->SetNumberField(TEXT("walked_metres"),RouteWalked/100.);O->SetNumberField(TEXT("seconds"),Now-RouteStarted);
                RouteResults.Add(MakeShared<FJsonValueObject>(O));Event(TEXT("walk_pass"),Routes[RouteIndex].Name);++RouteIndex;BeginRoute();return;
            }
            BestDistance=DBL_MAX;ProgressAt=Now;return;
        }
        if(Distance<BestDistance-5){BestDistance=Distance;ProgressAt=Now;}
        if(Now-ProgressAt>8){Finish(TEXT("walking stalled: ")+Routes[RouteIndex].Name+TEXT(" target=")+Target.ToString());return;}
        P->SetTestMovement(Target-P->GetActorLocation(),true);return;
    }
    if(Phase==10)
    {
        if(!Ready() || !M->ReadyAt({0,-1}) || !M->ReadyAt({0,1}))return;
        if(!VerifyComponents(bVisualOnly?TEXT("before_photos"):TEXT("after_walking")))return;BeginPhoto();return;
    }
    if(Phase==11)
    {
        if(Now<WaitUntil)return;
        if(!bCaptured)
        {
            FScreenshotRequest::RequestScreenshot(PhotoPath,false,false);bCaptured=true;WaitUntil=Now+1.;
            if(Views[PhotoIndex].Name==TEXT("south-cascade-a"))
                for(TActorIterator<AEWWaterCity> It(GetWorld());It;++It)
                {
                    TArray<UInstancedStaticMeshComponent*> Sprays;It->GetComponents(Sprays);
                    for(auto* C:Sprays)if(C->IsVisible() && C->GetInstanceCount()>0)
                    {MotionSpray=C;C->GetInstanceTransform(0,FirstSprayTransform,false);break;}
                    if(MotionSpray.IsValid())break;
                }
            return;
        }
        if(!IFileManager::Get().FileExists(*PhotoPath)){Finish(TEXT("review screenshot was not written"));return;}
        if(Views[PhotoIndex].Name==TEXT("south-cascade-b") && MotionSpray.IsValid())
        {FTransform Later;MotionSpray->GetInstanceTransform(0,Later,false);bMotionSeen=!Later.Equals(FirstSprayTransform,.05);}
        auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("name"),Views[PhotoIndex].Name);O->SetStringField(TEXT("file"),PhotoPath);
        O->SetStringField(TEXT("staged_eye_global_cm"),Views[PhotoIndex].Eye.ToString());O->SetStringField(TEXT("target_global_cm"),Views[PhotoIndex].Target.ToString());
        O->SetNumberField(TEXT("world_time"),GetWorld()->GetTimeSeconds());Photos.Add(MakeShared<FJsonValueObject>(O));Event(TEXT("screenshot"),PhotoPath);
        ++PhotoIndex;BeginPhoto();return;
    }
    if(Phase==20)
    {
        if(!Ready() || M->PlayerCoord()!=FarCoord)return;
        bool Retired=true;for(const auto& Old:RetiredCities)Retired&=!Old.IsValid();
        if(!Retired || TActorIterator<AEWWaterCity>(GetWorld()) || M->RebaseCount<=InitialRebases || M->OriginCoord()==EW::ChunkCoord{0,0})
        {Finish(TEXT("old water-city actors did not retire beyond the origin threshold"));return;}
        Event(TEXT("retired_and_rebased"),M->OriginCoord().Text());Setup({0,0},FVector(6400,5150,2250));Phase=21;return;
    }
    if(Phase==21)
    {
        if(!Ready() || !M->ReadyAt({0,-1}) || !M->ReadyAt({0,1}))return;
        if(M->RebaseCount<InitialRebases+2 || M->OriginCoord()!=EW::ChunkCoord{0,0})
        {Finish(TEXT("return origin rebase was not observed"));return;}
        if(!VerifyComponents(TEXT("recreated_after_rebase")))return;
        if(!bSkipPhotos && !bMotionSeen){Finish(TEXT("nearby spray did not show a changing instance transform"));return;}
        Setup({0,0},FVector(6400,12850,7650));Phase=22;return;
    }
    if(Phase==22)
    {
        if(!Ready())return;
        const FVector Local=M->ToLocal({0,0},P->GetActorLocation());
        if(FVector::Dist2D(Local,FVector(6400,12850,7738))>6 || FMath::Abs(Local.Z-7738)>10 ||
           !GI->SaveNow() || !GI->Store()->LoadCurrent(SavedUpper))
        {Finish(TEXT("upper new quay did not produce a supported saved bookmark"));return;}
        Event(TEXT("upper_quay_resume_fixture_saved"),SavedUpper.Code());Finish();return;
    }
    if(Phase==90)
    {
        if(!Ready())return;
        const FVector Local=M->ToLocal({0,0},P->GetActorLocation());
        if(M->Descriptor().Code()!=EW::WorldDescriptor::ReferenceWorld().Code() ||
           FVector::Dist2D(Local,FVector(6400,12850,7738))>6 || FMath::Abs(Local.Z-7738)>10)
        {Finish(TEXT("separate process resumed at the wrong floor instead of the new upper quay"));return;}
        Event(TEXT("separate_process_upper_quay_resume_pass"),Local.ToString());Finish();return;
    }
    Finish(TEXT("unknown water-city runtime audit phase"));
}
