#include "EWResidenceAudit.h"
#include "EWResidence.h"
#include "EWCharacter.h"
#include "EWChunkManager.h"
#include "EWGameInstance.h"
#include "EWLift.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Components/PointLightComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/StaticMesh.h"
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

AEWResidenceAudit::AEWResidenceAudit()
{ PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostPhysics; }
AEWCharacter* AEWResidenceAudit::Player() const
{ return Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0)); }
AEWChunkManager* AEWResidenceAudit::Manager() const
{ const auto* GI=GetGameInstance<UEWGameInstance>();return GI?GI->Manager.Get():nullptr; }
AEWResidence* AEWResidenceAudit::Residence() const
{
    for(TActorIterator<AEWResidence> It(GetWorld());It;++It)if(It->Spec.Id==TEXT("rain-window/0,0/nw"))return *It;
    return nullptr;
}
void AEWResidenceAudit::BeginPlay()
{
    Super::BeginPlay();Started=StageStart=FPlatformTime::Seconds();
    ReportPath=FPaths::ProjectSavedDir()/TEXT("Verification/residence-runtime-")+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT(".json");
    FParse::Value(FCommandLine::Get(),TEXT("EWReport="),ReportPath);StreamPath=FPaths::ChangeExtension(ReportPath,TEXT("jsonl"));
    if(IFileManager::Get().FileExists(*ReportPath) || IFileManager::Get().FileExists(*StreamPath))
    { bFinished=true;UE_LOG(LogTemp,Error,TEXT("EW_RESIDENCE_AUDIT refuses to overwrite evidence"));FPlatformMisc::RequestExit(false);return; }
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath),true);
    bResume=FParse::Param(FCommandLine::Get(),TEXT("EWResidenceResumeOnly"));
    bSkipPhotos=FParse::Param(FCommandLine::Get(),TEXT("EWNoResidenceScreenshots"));
    Event(TEXT("start"),TEXT("Actual CharacterMovement, existing Interact path, two water states, seat/stand, stream retirement and origin rebases. Explicit setup teleports are excluded from walked distance."));
}
void AEWResidenceAudit::Event(const FString& Type,const FString& Detail)
{
    auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("event"),Type);O->SetStringField(TEXT("detail"),Detail);
    O->SetNumberField(TEXT("elapsed"),FPlatformTime::Seconds()-Started);O->SetNumberField(TEXT("phase"),Phase);
    if(const auto* P=Player())
    { O->SetStringField(TEXT("position"),P->GetActorLocation().ToString());O->SetBoolField(TEXT("seated"),P->IsSeated());O->SetBoolField(TEXT("on_ground"),P->GetCharacterMovement()->IsMovingOnGround()); }
    Evidence.Add(MakeShared<FJsonValueObject>(O));FString JSON;
    FJsonSerializer::Serialize(O,TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&JSON));
    FFileHelper::SaveStringToFile(JSON+TEXT("\n"),*StreamPath,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);
    UE_LOG(LogTemp,Display,TEXT("EW_RESIDENCE_AUDIT_%s %s"),*Type.ToUpper(),*Detail);
}
void AEWResidenceAudit::Finish(const FString& Failure)
{
    if(bFinished)return;bFinished=true;if(auto* P=Player())P->SetTestMovement(FVector::ZeroVector,false);
    Event(Failure.IsEmpty()?TEXT("pass"):TEXT("failure"),Failure);
    auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("success"),Failure.IsEmpty());R->SetStringField(TEXT("failure"),Failure);
    R->SetArrayField(TEXT("evidence"),Evidence);R->SetArrayField(TEXT("screenshots"),Photos);R->SetBoolField(TEXT("resume_only"),bResume);
    R->SetNumberField(TEXT("walked_metres"),Walked/100.);R->SetNumberField(TEXT("seconds"),FPlatformTime::Seconds()-Started);
    if(Manager())
    {
        EW::PlaceBookmark Entry;Entry.WorldCode=Manager()->Descriptor().Code();Entry.Coord={0,0};Entry.Id=TEXT("rain-window-entry");
        Entry.Name=TEXT("雨継ぎの窓辺　入口");Entry.Kind=0;Entry.LocalPosition=Spec.Entry;
        Entry.Yaw=Spec.Frame.TransformVectorNoScale(FVector(0,1,0)).Rotation().Yaw;
        R->SetStringField(TEXT("entry_place_code"),Entry.Code());R->SetStringField(TEXT("standing_place_code"),StandingBookmark.Code());
        R->SetStringField(TEXT("entry_local_position"),Spec.Entry.ToString());
    }
    if(const auto* P=Player())
    { R->SetNumberField(TEXT("fall_recoveries"),P->FallRecoveries-InitialFalls);R->SetNumberField(TEXT("hardware_forward_events"),P->ForwardEvents);R->SetNumberField(TEXT("hardware_look_events"),P->LookEvents); }
    if(const auto* M=Manager())
    { R->SetNumberField(TEXT("rebase_count"),M->RebaseCount-InitialRebases);R->SetStringField(TEXT("final_chunk"),M->PlayerCoord().Text());R->SetStringField(TEXT("final_local_position"),M->CurrentPosition().LocalPosition.ToString()); }
    if(const auto* GI=GetGameInstance<UEWGameInstance>())if(GI->Store())R->SetStringField(TEXT("data_directory"),GI->Store()->Root());
    R->SetStringField(TEXT("not_covered"),TEXT("Screenshots require visual review. This does not certify attractive composition, human hearing, physical controls, every lift ride in both modes, or performance. Existing traversal regression covers lift movement separately."));
    FString JSON;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&JSON));
    FFileHelper::SaveStringToFile(JSON,*ReportPath,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);FPlatformMisc::RequestExit(false);
}
void AEWResidenceAudit::WaitingEvidence()
{
    const double Now=FPlatformTime::Seconds();if(Now-LastWaitingEvidence<10.)return;LastWaitingEvidence=Now;
    const auto* P=Player();const auto* M=Manager();const auto* GI=GetGameInstance<UEWGameInstance>();const auto* Home=Residence();
    Event(TEXT("waiting"),FString::Printf(TEXT("manager=%d player=%d store=%d session=%d travelling=%d preview_ready=%d ready_chunks=%d resident_chunks=%d colliders=%d travel_progress=%.3f held=%d movement_mode=%d residence=%d residence_state=%d paused=%d"),
        M!=nullptr,P!=nullptr,GI && GI->Store() && GI->Store()->IsOpen(),GI && GI->SessionStarted(),M && M->IsTravelling(),M && M->ReadyAt(M->PlayerCoord()),
        M?M->ReadyCount():-1,M?M->ResidentCount():-1,M?M->ColliderCount():-1,M?M->TravelProgress():0.f,
        P && P->IsStreamingHeld(),P?int32(P->GetCharacterMovement()->MovementMode):-1,Home!=nullptr,Home && Home->StateReady(),UGameplayStatics::IsGamePaused(this)));
}
bool AEWResidenceAudit::GroundReady(EW::ChunkCoord Coord) const
{
    const auto* M=Manager();const auto* P=Player();
    return M && P && !M->IsTravelling() && M->ReadyAt(Coord) && !P->IsStreamingHeld() && P->GetCharacterMovement()->IsMovingOnGround();
}
void AEWResidenceAudit::SetupAt(EW::ChunkCoord Coord,FVector Centre)
{
    auto* P=Player();if(P->IsSeated())P->LeaveSeat();P->SetTestMovement(FVector::ZeroVector,false);P->ClearHeldInput();P->SetBase(static_cast<UPrimitiveComponent*>(nullptr));
    P->SetActorLocation(Manager()->ToRender(Coord,Centre)+FVector(0,0,3),false,nullptr,ETeleportType::TeleportPhysics);
    P->ResetSafeLocation();P->SetStreamingHold(true);P->GetCharacterMovement()->MaxWalkSpeed=350;
    StageStart=FPlatformTime::Seconds();Event(TEXT("setup_teleport"),Coord.Text()+TEXT(" ")+Centre.ToString());
}
void AEWResidenceAudit::BeginWalk(const FString& Name,const TArray<FVector>& Points,int32 Next)
{
    RouteName=Name;Route=Points;Point=0;NextPhase=Next;Phase=100;BestDistance=DBL_MAX;
    ProgressAt=GetWorld()->GetTimeSeconds();StageStart=FPlatformTime::Seconds();LastPosition=Player()->GetActorLocation();Event(TEXT("walk_begin"),Name);
}
void AEWResidenceAudit::AimAt(FVector PointToView,int32 Next)
{
    auto* P=Player();P->SetTestMovement(FVector::ZeroVector,false);P->ClearHeldInput();
    if(P->Controller)P->Controller->SetControlRotation((Manager()->ToRender({0,0},PointToView)-P->Camera->GetComponentLocation()).Rotation());
    NextPhase=Next;WaitUntil=FPlatformTime::Seconds()+.4;Phase=101;
}
void AEWResidenceAudit::Photo(const FString& Name,FVector Target,int32 Next)
{
    if(bSkipPhotos){AimAt(Target,Next);return;}
    auto* P=Player();P->SetTestMovement(FVector::ZeroVector,false);P->ClearHeldInput();
    if(P->Controller)P->Controller->SetControlRotation((Manager()->ToRender({0,0},Target)-P->Camera->GetComponentLocation()).Rotation());
    PhotoPath=FPaths::GetPath(ReportPath)/(FPaths::GetBaseFilename(ReportPath)+TEXT("-")+Name+TEXT(".png"));
    if(IFileManager::Get().FileExists(*PhotoPath)){Finish(TEXT("screenshot already exists"));return;}
    bCaptured=false;WaitUntil=FPlatformTime::Seconds()+2.;NextPhase=Next;Phase=102;
}
bool AEWResidenceAudit::VerifyState(int32 Expected)
{
    auto* Home=Residence();auto* GI=GetGameInstance<UEWGameInstance>();int32 Stored=-1;
    if(!Home || !Home->StateReady() || Home->FlowMode()!=Expected || !GI->Store()->LoadResidenceState(Manager()->Descriptor().Code(),Spec.Id,Spec.Revision,Stored) || Stored!=Expected)
    { Finish(TEXT("runtime water choice differs from committed state"));return false; }
    bool Eaves=false,Window=false,Handle=false;int32 PlayingWater=0;
    TArray<UStaticMeshComponent*> Meshes;Home->GetComponents(Meshes);
    for(const auto* M:Meshes)if(M->GetStaticMesh())
    {
        const FName N=M->GetStaticMesh()->GetFName();
        if(N==TEXT("SM_RainWaterEaves"))Eaves=M->IsVisible()==(Expected==0);
        if(N==TEXT("SM_RainWaterWindow"))Window=M->IsVisible()==(Expected==1);
    }
    TArray<USceneComponent*> Components;Home->GetComponents(Components);
    for(const auto* C:Components)if(C->GetFName()==TEXT("ValvePivot"))Handle=FMath::Abs(C->GetRelativeRotation().Pitch-(Expected?42.:-42.))<.1;
    TArray<UAudioComponent*> Audio;Home->GetComponents(Audio);
    for(const auto* A:Audio)if(A->IsPlaying())++PlayingWater;
    TArray<UPointLightComponent*> Lights;Home->GetComponents(Lights);
    for(const auto* L:Lights)Event(TEXT("local_light"),FString::Printf(TEXT("position=%s intensity=%.1f units=%d radius=%.1f visible=%d registered=%d"),
        *L->GetComponentLocation().ToString(),L->Intensity,int32(L->IntensityUnits),L->AttenuationRadius,L->IsVisible(),L->IsRegistered()));
    if(!Eaves || !Window || !Handle){Finish(TEXT("handle or water mesh visibility does not match choice"));return false;}
    const auto R=Manager()->RecipeAt({0,0});int32 Lifts=0;bool LiftCalls=true;
    for(TActorIterator<AEWLift> It(GetWorld());It;++It)if(It->Spec.Id.StartsWith(TEXT("0,0/")))
    {
        ++Lifts;const auto* Recipe=R->Lifts.FindByPredicate([&](const EW::LiftSpec& L){return L.Id==It->Spec.Id;});
        LiftCalls&=Recipe && Recipe->Stops.Num()==It->Spec.Stops.Num() && It->Call(It->FloorIndex());
    }
    if(Lifts!=4 || !LiftCalls){Finish(TEXT("existing lift stops/calls changed with water mode"));return false;}
    Event(TEXT("state_verified"),FString::Printf(TEXT("mode=%d visible_routes=matched handle=matched active_audio_components=%d existing_lifts=%d calls=accepted"),Expected,PlayingWater,Lifts));return true;
}
bool AEWResidenceAudit::InteractWith(bool Seat)
{
    EEWResidenceAction Action=EEWResidenceAction::None;auto* H=Manager()->NearestResidence(Action);
    if(H!=Residence() || Action!=(Seat?EEWResidenceAction::Seat:EEWResidenceAction::Valve))
    { Finish(TEXT("nearby prompt and intended interaction target disagree"));return false; }
    GetGameInstance<UEWGameInstance>()->Interact();return true;
}
void AEWResidenceAudit::Tick(float Delta)
{
    Super::Tick(Delta);if(bFinished)return;auto* P=Player();auto* M=Manager();auto* GI=GetGameInstance<UEWGameInstance>();
    const double Now=FPlatformTime::Seconds();
    if(Now-StageStart>(Phase==0 || Phase==90?180.:600.) || Now-Started>1200){WaitingEvidence();Finish(TEXT("runtime residence stage timeout"));return;}
    if(!P || !M || !GI || !GI->Store()){WaitingEvidence();return;}
    if(Phase==0 || Phase==1 || Phase==12 || Phase==13 || Phase==14 || Phase==16 || Phase==90)WaitingEvidence();
    if(Phase>0 && (P->ForwardEvents || P->LookEvents)){Finish(TEXT("physical input contaminated scripted audit"));return;}
    if(Phase>0 && P->FallRecoveries!=InitialFalls){Finish(TEXT("unexpected fall recovery"));return;}
    auto Local=[&](double X,double Y,double Z=88.){return Spec.Frame.TransformPosition(FVector(X,Y,Z));};
    const FVector BenchAim=Local(650,-1710,115),SeatApproach=Local(675,-1600);
    if(Phase==100)
    {
        Walked+=FVector::Dist2D(P->GetActorLocation(),LastPosition);LastPosition=P->GetActorLocation();
        if(Point>=Route.Num()){P->SetTestMovement(FVector::ZeroVector,false);P->ClearHeldInput();Event(TEXT("walk_pass"),RouteName);Phase=NextPhase;return;}
        const FVector Target=M->ToRender({0,0},Route[Point]);const double D=FVector::Dist2D(Target,P->GetActorLocation());
        if(D<25)
        {
            if(FMath::Abs(P->GetActorLocation().Z-Target.Z)>12 || !P->GetCharacterMovement()->IsMovingOnGround())
            {Finish(TEXT("walk target reached on wrong floor or without support"));return;}
            ++Point;BestDistance=DBL_MAX;ProgressAt=GetWorld()->GetTimeSeconds();return;
        }
        if(D<BestDistance-5){BestDistance=D;ProgressAt=GetWorld()->GetTimeSeconds();}
        if(GetWorld()->GetTimeSeconds()-ProgressAt>8){Finish(TEXT("walk stalled: ")+RouteName+TEXT(" target=")+Target.ToString());return;}
        P->SetTestMovement(Target-P->GetActorLocation(),true);return;
    }
    if(Phase==101){if(Now>=WaitUntil)Phase=NextPhase;return;}
    if(Phase==102)
    {
        if(Now<WaitUntil)return;
        if(!bCaptured){FScreenshotRequest::RequestScreenshot(PhotoPath,false,false);bCaptured=true;WaitUntil=Now+2.;return;}
        if(!IFileManager::Get().FileExists(*PhotoPath)){Finish(TEXT("screenshot was not written"));return;}
        auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("file"),PhotoPath);O->SetStringField(TEXT("eye"),P->Camera->GetComponentLocation().ToString());
        O->SetStringField(TEXT("rotation"),P->GetControlRotation().ToString());Photos.Add(MakeShared<FJsonValueObject>(O));Event(TEXT("screenshot"),PhotoPath);Phase=NextPhase;return;
    }
    switch(Phase)
    {
    case 0:
        if(!GI->bScriptedWorldAudit){Finish(TEXT("scripted input isolation was not enabled"));return;}
        if(bResume)
        {
            // ContinueWorld only requires the preview's streaming transaction
            // to finish. Its residence actor and grounded pawn are not the
            // saved session we are here to test.
            if(M->IsTravelling() || !M->ReadyAt(M->PlayerCoord()))return;
            if(GI->SessionStarted()){Finish(TEXT("resume audit requires EWPlay and EWWorld to be absent"));return;}
            if(!GI->Store()->LoadCurrent(StandingBookmark)){Finish(TEXT("no saved runtime residence bookmark"));return;}
            if(StandingBookmark.Coord!=EW::ChunkCoord{0,0} || StandingBookmark.WorldCode!=EW::WorldDescriptor::ReferenceWorld().Code())
            {Finish(TEXT("saved bookmark is outside the reference residence district"));return;}
            InitialFalls=P->FallRecoveries;InitialRebases=M->RebaseCount;
            GI->ContinueWorld();
            if(!GI->SessionStarted() || !M->IsTravelling()){Finish(TEXT("ContinueWorld did not start the saved session"));return;}
            Event(TEXT("resume_requested"),StandingBookmark.Code());Phase=90;StageStart=Now;return;
        }
        if(!GroundReady({0,0}) || !Residence())return;
        Spec=Residence()->Spec;InitialFalls=P->FallRecoveries;InitialRebases=M->RebaseCount;
        if(!GI->SessionStarted()){GI->ReferenceWorld();return;}
        if(!VerifyState(0))return;
        SetupAt({0,0},Local(-1100,-2200));Phase=1;return;
    case 1:
        if(!GroundReady({0,0}))return;
        BeginWalk(TEXT("left_gallery_through_door_to_seat"),{Spec.Entry,Local(225,-1450),Local(450,-1450),Local(450,-1600),SeatApproach},2);return;
    case 2:AimAt(BenchAim,3);return;
    case 3:
        if(!InteractWith(true))return;
        if(!P->IsSeated() || FMath::Abs(P->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()-42)>1){Finish(TEXT("seat did not use its compact capsule"));return;}
        if(!GI->SaveNow() || !GI->Store()->LoadCurrent(StandingBookmark) || !StandingBookmark.LocalPosition.Equals(Spec.Stand,1.) || StandingBookmark.Coord!=EW::ChunkCoord{0,0})
        {Finish(TEXT("seated save did not record the safe standing location"));return;}
        Event(TEXT("seat_save_pass"),StandingBookmark.LocalPosition.ToString());Photo(TEXT("seat-eaves-clock"),Spec.ViewTarget,4);return;
    case 4:
        GI->SetMenu(EEWMenu::Settings);if(!P->IsSeated()){Finish(TEXT("menu unexpectedly discarded seat"));return;}GI->SetMenu(EEWMenu::None);GI->Interact();
        if(P->IsSeated() || FMath::Abs(P->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()-88)>1 || FVector::Dist(P->GetActorLocation(),M->ToRender({0,0},Spec.Stand))>15)
        {Finish(TEXT("leave seat did not restore full standing capsule at return point"));return;}
        BeginWalk(TEXT("room_to_diverter"),{Local(450,-1320),Local(225,-1450),Spec.Entry,Local(-225,-2200)},5);return;
    case 5:Photo(TEXT("diverter-eaves"),Spec.Valve,40);return;
    case 6:
        if(!InteractWith(false))return;
        Photo(TEXT("diverter-window"),Spec.Valve,7);return;
    case 7:
        if(!VerifyState(1))return;
        BeginWalk(TEXT("gallery_to_window_flow_view"),{Local(-1100,-2300)},44);return;
    case 8:AimAt(BenchAim,9);return;
    case 9:
        if(!InteractWith(true) || !P->IsSeated()){if(!bFinished)Finish(TEXT("second seat interaction failed"));return;}
        Photo(TEXT("seat-window-clock"),Spec.ViewTarget,10);return;
    case 10:Photo(TEXT("room-interior"),Local(425,-1220,195),11);return;
    case 11:
        GI->Interact();if(P->IsSeated()){Finish(TEXT("second leave-seat failed"));return;}
        BeginWalk(TEXT("room_to_interior_overview"),{Local(450,-1320)},20);return;
    case 20:Photo(TEXT("room-overview"),Local(650,-1710,100),21);return;
    case 21:
        if(!GI->SaveNow()){Finish(TEXT("standing save before stream retirement failed"));return;}
        RetiredResidence=Residence();FarCoord={M->RebaseThreshold()+2,0};
        { const auto R=EW::GenerateChunk(M->Descriptor(),FarCoord);SetupAt(FarCoord,R.SafePosition(R.Hub+FVector(0,-1250,88))); }
        Phase=12;return;
    case 12:
        if(!GroundReady(FarCoord))return;
        if(RetiredResidence.IsValid() || Residence() || M->RebaseCount<=InitialRebases || M->OriginCoord()==EW::ChunkCoord{0,0})
        {Finish(TEXT("old room did not retire after travelling beyond origin threshold"));return;}
        Event(TEXT("stream_retired_and_rebased"),M->OriginCoord().Text());SetupAt({0,0},Spec.Stand);Phase=13;return;
    case 13:
        if(!GroundReady({0,0}) || !Residence())return;
        if(M->RebaseCount<InitialRebases+2 || M->OriginCoord()!=EW::ChunkCoord{0,0}){Finish(TEXT("return origin shift was not observed"));return;}
        if(!VerifyState(1))return;
        Event(TEXT("stream_return_kept_choice"),Residence()->GetName());
        { const auto R=M->RecipeAt({0,0});SetupAt({0,0},FVector(6400,5150,R->Hub.Z+88)); }Phase=14;return;
    case 14:
        if(!GroundReady({0,0}))return;
        Photo(TEXT("arrival-looking-at-window"),Local(675,-1820,195),15);return;
    case 15:SetupAt({0,0},Spec.Stand);Phase=16;return;
    case 16:
        if(!GroundReady({0,0}))return;
        if(!GI->SaveNow() || !GI->Store()->LoadCurrent(StandingBookmark) || !StandingBookmark.LocalPosition.Equals(Spec.Stand,5.) || P->IsSeated())
        {Finish(TEXT("final bookmark is not safe standing room position"));return;}
        Event(TEXT("resume_fixture_ready"),StandingBookmark.Code());Finish();return;
    case 40:BeginWalk(TEXT("gallery_to_eaves_flow_view"),{Local(-1100,-2300)},41);return;
    case 41:Photo(TEXT("water-routes-eaves"),Local(300,-1940,260),42);return;
    case 42:BeginWalk(TEXT("flow_view_to_diverter"),{Local(-225,-2200)},43);return;
    case 43:AimAt(Spec.Valve,6);return;
    case 44:Photo(TEXT("water-routes-window"),Local(300,-1940,260),45);return;
    case 45:BeginWalk(TEXT("right_gallery_through_door_to_seat"),{Local(1250,-2200),Spec.Entry,Local(225,-1450),Local(450,-1450),Local(450,-1600),SeatApproach},8);return;
    case 90:
        if(!GroundReady({0,0}) || !Residence())return;
        Spec=Residence()->Spec;
        if(P->IsSeated() || !StandingBookmark.LocalPosition.Equals(Spec.Stand,5.) ||
           FVector::Dist2D(M->ToLocal({0,0},P->GetActorLocation()),Spec.Stand)>5 || FMath::Abs(P->GetActorLocation().Z-Spec.Stand.Z)>8)
        {Finish(TEXT("separate process did not resume standing at the room bookmark"));return;}
        if(!VerifyState(1))return;Event(TEXT("separate_process_resume_pass"),Spec.Stand.ToString());Finish();return;
    default:Finish(TEXT("unknown residence audit stage"));return;
    }
}
