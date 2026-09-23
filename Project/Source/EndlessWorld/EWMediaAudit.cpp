#include "EWMediaAudit.h"
#include "EWMediaPolicy.h"
#include "EWTerminal.h"
#include "EWMediaScreen.h"
#include "EWBrowserSurface.h"
#include "EWGameInstance.h"
#include "EWChunkManager.h"
#include "EWCharacter.h"
#include "EWClock.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"
#include "Rendering/NaniteResources.h"
#include "Components/WidgetComponent.h"
#include "EWLift.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "AudioDevice.h"
#include "ISubmixBufferListener.h"
#include "Sound/SoundSubmix.h"
#include "Components/AudioComponent.h"
#include "Misc/ScopeLock.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/App.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

// Taps only the monitor's UE submix, after spatialization. This is not an OS
// loopback recording, and never includes another application or microphone.
class FEWMonitorMeter:public ISubmixBufferListener
{
public:
    FCriticalSection Mutex;
    double L=0,R=0;int64 Frames=0;int32 Channels=0;
    void OnNewSubmixBuffer(const USoundSubmix*,float* Data,int32 N,int32 C,int32,double) override
    {FScopeLock Lock(&Mutex);Channels=C;if(C<2)return;for(int32 I=0;I<N;I+=C){L+=Data[I]*Data[I];R+=Data[I+1]*Data[I+1];++Frames;}}
    void Reset(){FScopeLock Lock(&Mutex);L=R=0;Frames=0;}
    TSharedRef<FJsonObject> Snapshot()
    {FScopeLock Lock(&Mutex);auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("left_rms"),Frames?FMath::Sqrt(L/Frames):0);O->SetNumberField(TEXT("right_rms"),Frames?FMath::Sqrt(R/Frames):0);O->SetNumberField(TEXT("frames"),double(Frames));O->SetNumberField(TEXT("channels"),Channels);return O;}
};
AEWMediaAudit::AEWMediaAudit(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostPhysics;}
void AEWMediaAudit::BeginPlay()
{Super::BeginPlay();FApp::SetUnfocusedVolumeMultiplier(1);FApp::SetVolumeMultiplier(1);Started=Stage=FPlatformTime::Seconds();Report=FPaths::ProjectSavedDir()/TEXT("Verification/media-audit.json");FParse::Value(FCommandLine::Get(),TEXT("EWReport="),Report);}
bool AEWMediaAudit::Check(bool Value,const FString& Name)
{auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("check"),Name);O->SetBoolField(TEXT("pass"),Value);Checks.Add(MakeShared<FJsonValueObject>(O));if(!Value)Finish(Name);return Value;}
void AEWMediaAudit::Finish(const FString& Error)
{
    if(Finished)return;Finished=true;auto G=GetGameInstance<UEWGameInstance>();auto O=MakeShared<FJsonObject>();
    O->SetBoolField(TEXT("success"),Error.IsEmpty());O->SetStringField(TEXT("failure"),Error);O->SetNumberField(TEXT("seconds"),FPlatformTime::Seconds()-Started);
    O->SetStringField(TEXT("physical_output_check"),!EWMediaPolicy::PlaybackEnabled?TEXT("NOT_APPLICABLE: browser playback removed; no output requested"):FParse::Param(FCommandLine::Get(),TEXT("EWSilentAudit"))?TEXT("NOT_MEASURED: final main submix muted for silent testing; source spatialization measured"):TEXT("main submix measured; human listening not measured"));
    O->SetStringField(TEXT("scope"),!EWMediaPolicy::PlaybackEnabled?TEXT("Public-build browser removal: all three screens and handheld reject UI, direct search, resume, synchronization, and diagnostic playback. Does not measure jump height or physical audio output."):TEXT("CharacterMovement jumps, central screen placement, live clock rendering, and CEF tone through positional and main UE output at the actual plaza listener. YouTube playback and physical key timing are separate UI checks."));
    O->SetArrayField(TEXT("checks"),Checks);O->SetArrayField(TEXT("audio_cases"),Samples);O->SetNumberField(TEXT("single_jump_cm"),SinglePeak-GroundZ);O->SetNumberField(TEXT("double_jump_cm"),PeakZ-GroundZ);
    O->SetArrayField(TEXT("clock_frames"),ClockSamples);
    if(BaselineOutput)O->SetObjectField(TEXT("main_before_tone"),BaselineOutput);
    if(UnprobedOutput)O->SetObjectField(TEXT("main_without_source_probe"),UnprobedOutput);
    if(auto* Walk=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/EndlessWorld/Kit/SM_WaterCityWalks.SM_WaterCityWalks")))
        if(auto* Render=Walk->GetRenderData())O->SetNumberField(TEXT("walk_nanite_input_triangles"),Render->NaniteResourcesPtr->NumInputTriangles);
    if(G && G->MediaScreen)O->SetObjectField(TEXT("monitor"),G->MediaScreen->Evidence());
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Report),true);FString S;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&S));FFileHelper::SaveStringToFile(S,*Report,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    FPlatformMisc::RequestExit(false);
}
void AEWMediaAudit::EndPlay(const EEndPlayReason::Type Reason)
{
    if(MainMeter)if(auto D=GetWorld()->GetAudioDevice())D->UnregisterSubmixBufferListener(MainMeter.ToSharedRef(),D->GetMainSubmixObject());
    if(auto* G=GetGameInstance<UEWGameInstance>())if(G->MediaScreen && G->MediaScreen->OutputSubmix() && Meter)
        if(auto D=GetWorld()->GetAudioDevice())D->UnregisterSubmixBufferListener(Meter.ToSharedRef(),*G->MediaScreen->OutputSubmix());
    if(auto* PC=UGameplayStatics::GetPlayerController(this,0))PC->ClearAudioListenerOverride();
    Meter.Reset();MainMeter.Reset();Super::EndPlay(Reason);
}
void AEWMediaAudit::Tick(float Delta)
{
    Super::Tick(Delta);if(Finished)return;auto* G=GetGameInstance<UEWGameInstance>();auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
    if(!G || !G->Manager || !G->MediaScreen || !P)return;
    const double Now=FPlatformTime::Seconds(),Age=Now-Stage;
    if(Now-Started>180){Finish(TEXT("audit timeout"));return;}
    if(G->Manager->IsTravelling() || P->IsStreamingHeld())return;
    if(!EWMediaPolicy::PlaybackEnabled)
    {
        if(!G->Terminal || !G->CinemaScreen || !G->SkyTheatre || G->Manager->ReadyCount()<49)return;
        const EEWMenu Before=G->Menu();const int32 PhonePage=G->Terminal->PageIndex();
        G->SetMenu(EEWMenu::Monitor);G->Terminal->ShowPage(3);
        G->Terminal->SearchVideo(TEXT("https://www.youtube.com/watch?v=EWTEST00001"));
        if(!Check(G->Menu()==Before && G->Terminal->PageIndex()==PhonePage,TEXT("direct monitor and phone video requests preserve existing UI")))return;
        TArray<TSharedPtr<FEWBrowserSurface>> Browsers;
        Browsers.Add(G->Terminal->Browser());
        for(auto* Screen:{G->MediaScreen.Get(),G->CinemaScreen.Get(),G->SkyTheatre.Get()})
        {
            Screen->OpenControls();Screen->Search(TEXT("https://www.youtube.com/watch?v=EWTEST00001"));Screen->ControlPlayback(TEXT("play"));
            if(!Check(!Screen->Available() && !Screen->CanControlPlayback() && !Screen->TabletOpen() && G->Menu()==Before,Screen->GetName()+TEXT(": public controls cannot open or play")))return;
            Browsers.Add(Screen->Browser());
        }
        int32 Index=0;
        for(const auto& Browser:Browsers)
        {
            if(!Check(Browser.IsValid(),TEXT("inert media surface exists")))return;
            if(!Check(!Browser->Start(),TEXT("browser initialization rejected")))return;
            Browser->Search(TEXT("public removal audit"));Browser->ApplyPlayback(TEXT("EWTEST00001"),7,false);
            Browser->Pause(false);Browser->SetPlaybackPaused(false);Browser->SetVideoFullscreen(true);Browser->EnableSound();
            Browser->TestTone();Browser->TestStereoTone(0);Browser->TestSyncFilm();Browser->SeekForAudit(12,false);
            Browser->Tick();TArray<int16> PCM;PCM.Add(123);Browser->DrainStereoAudio(PCM);
            const auto State=Browser->Evidence();
            if(!Check(!Browser->HasPage() && !Browser->HasAudio() && Browser->VideoPaused() && PCM.IsEmpty() &&
                !State->GetBoolField(TEXT("initialized")) && State->GetNumberField(TEXT("runtime_browser_owners"))==0 &&
                State->GetNumberField(TEXT("audio_samples"))==0 && State->GetStringField(TEXT("video_id")).IsEmpty(),
                FString::Printf(TEXT("surface %d remains inert after direct/resume/sync/audit requests"),Index++)))return;
        }
        Finish();return;
    }
    auto* Move=P->GetCharacterMovement();auto* S=G->MediaScreen.Get();auto B=S->Browser();
    if(Phase==0)
    {
        if(G->Manager->ReadyCount()<49 || !Move->IsMovingOnGround())return;
        G->SetMenu(EEWMenu::None);GroundZ=P->GetActorLocation().Z;PeakZ=GroundZ;
        int32 ExpectedWalkTriangles=0;
        if(FParse::Value(FCommandLine::Get(),TEXT("EWExpectedWalkTriangles="),ExpectedWalkTriangles))
        {
            auto* Walk=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/EndlessWorld/Kit/SM_WaterCityWalks.SM_WaterCityWalks"));
            const auto* Render=Walk?Walk->GetRenderData():nullptr;
            if(!Check(Render && Render->NaniteResourcesPtr->NumInputTriangles==ExpectedWalkTriangles,TEXT("packaged walkway mesh matches rebuilt geometry")))return;
        }
        if(!Check(P->JumpMaxCount==2,TEXT("two jumps configured")))return;
        if(!Check(EWClock::AnglesAt(FDateTime(2026,9,12,3,15,30)).Equals(FVector(97.75,93,180),.00001),TEXT("clock hour minute and second angles")))return;
        if(!Check(EWClock::AnglesAt(FDateTime(2026,9,12,23,59,59)).Equals(FVector(359.9916666667,359.9,354),.00001)
            && EWClock::AnglesAt(FDateTime(2026,9,13,0,0,0)).IsNearlyZero(),TEXT("clock midnight rollover")))return;
        const auto Plaza=G->Manager->RecipeAt({0,0});
        if(!Check(Plaza && S->GetActorLocation().Equals(G->Manager->ToRender({0,0},EWClock::ScreenCentre(*Plaza)),.01)
            && FMath::IsNearlyEqual(EWClock::ScreenCentre(*Plaza).Z-Plaza->Hub.Z,769.,.01),TEXT("screen lowered 225 cm from its previous centre")))return;
        if(!Check(S->Evidence()->GetBoolField(TEXT("two_readable_faces")) && S->Evidence()->GetBoolField(TEXT("back_render_target")),TEXT("opposing readable monitor faces clear the housing")))return;
        if(!Check(S->Evidence()->GetNumberField(TEXT("browser_instances"))==1 && S->Evidence()->GetNumberField(TEXT("audio_sources"))==1,TEXT("both faces share one browser and one positional audio source")))return;
        int ExpectedFaces=0,ActualFaces=0;bool CentralClear=true;
        for(TActorIterator<AEWChunkActor> It(GetWorld());It;++It)if(It->Recipe)
        {
            const bool Central=EWClock::IsCentralPlaza(*It->Recipe);
            for(const auto& Part:It->Recipe->Parts)if(Part.Mesh==TEXT("ClockTower") && Part.Detail<=It->GetDetail() && !Central)ExpectedFaces+=4;
            ActualFaces+=It->ClockFaces.Num();if(Central && It->ClockFaces.Num())CentralClear=false;
        }
        if(!Check(ExpectedFaces>0 && ActualFaces==ExpectedFaces && CentralClear,TEXT("all remaining clock towers have four live dials")))return;
        P->Jump();Phase=1;Stage=Now;return;
    }
    if(Phase<10)PeakZ=FMath::Max(PeakZ,P->GetActorLocation().Z);
    if(Phase==1)
    {
        if(Age>.10)P->StopJumping();
        if(Age>1 && Move->IsMovingOnGround())
        {SinglePeak=PeakZ;PeakZ=GroundZ;if(!Check(SinglePeak-GroundZ>140,TEXT("first jump leaves floor")))return;P->Jump();Phase=2;Stage=Now;}return;
    }
    if(Phase==2)
    {
        if(Age>.10)P->StopJumping();
        if(Age>.35){if(!Check(Move->IsFalling(),TEXT("second jump starts in air")))return;P->Jump();Phase=3;Stage=Now;}return;
    }
    if(Phase==3)
    {
        if(Age>.08){if(!Check(P->JumpCurrentCount==2 && Move->Velocity.Z>400,TEXT("second jump renews upward velocity")))return;P->StopJumping();Phase=4;Stage=Now;}return;
    }
    if(Phase==4)
    {
        if(Age>.25){SecondVelocity=Move->Velocity.Z;P->Jump();Phase=5;Stage=Now;}return;
    }
    if(Phase==5)
    {
        if(Age>.08){if(!Check(P->JumpCurrentCount==2 && Move->Velocity.Z<SecondVelocity,TEXT("third airborne jump rejected")))return;P->StopJumping();Phase=6;Stage=Now;}return;
    }
    if(Phase==6)
    {
        if(Move->IsMovingOnGround())
        {
            if(!Check(PeakZ>SinglePeak+90 && P->JumpCurrentCount==0,TEXT("extra height and landing reset")))return;
            P->Jump();Phase=7;Stage=Now;
        }return;
    }
    if(Phase==7)
    {
        if(Age>.10){if(!Check(P->JumpCurrentCount==1 && Move->IsFalling(),TEXT("jump available again after landing")))return;P->StopJumping();Phase=10;Stage=Now;}return;
    }
    if(Phase==10)
    {
        if(!Move->IsMovingOnGround())return;
        if(!Check(S->OutputSubmix()!=nullptr,TEXT("monitor owns an audio submix")))return;
        if(auto* PC=UGameplayStatics::GetPlayerController(this,0))PC->SetControlRotation((S->GetActorLocation()-(P->GetActorLocation()+FVector(0,0,74))).Rotation());
        FScreenshotRequest::RequestScreenshot(FPaths::ChangeExtension(Report,TEXT("-screen.png")),false,false);
        // Never register a monitor-submix probe before the ordinary output
        // check: UE implicitly initializes unregistered submixes for probes.
        FString InputEvidence;
        if(!Check(!FParse::Value(FCommandLine::Get(),TEXT("EWInputEvidence="),InputEvidence),TEXT("normal audio route tested without source instrumentation")))return;
        MainMeter=MakeShared<FEWMonitorMeter,ESPMode::ThreadSafe>();
        if(auto Device=GetWorld()->GetAudioDevice())Device->RegisterSubmixBufferListener(MainMeter.ToSharedRef(),Device->GetMainSubmixObject());else{Finish(TEXT("no audio device"));return;}
        B->TestTone();Phase=11;Stage=Now;return;
    }
    if(Phase==11)
    {if(Age>3 && B->Evidence()->GetNumberField(TEXT("paint_frames"))>0){BaselineOutput=MainMeter->Snapshot();MainMeter->Reset();B->Mouse(200,230,1);B->Mouse(200,230,2);Phase=12;Stage=Now;}return;}
    if(Phase==12)
    {
        if(Age>4 && B->Evidence()->GetNumberField(TEXT("audio_packets"))>10)
        {
            if(!Check(B->Evidence()->GetNumberField(TEXT("audio_peak"))>.1,TEXT("CEF browser delivers nonzero PCM")))return;
            UnprobedOutput=MainMeter->Snapshot();
            const double Before=BaselineOutput->GetNumberField(TEXT("left_rms"))+BaselineOutput->GetNumberField(TEXT("right_rms"));
            const double After=UnprobedOutput->GetNumberField(TEXT("left_rms"))+UnprobedOutput->GetNumberField(TEXT("right_rms"));
            if(FParse::Param(FCommandLine::Get(),TEXT("EWSilentAudit")))
            {if(!Check(After<.0000001,TEXT("final speaker output remains muted during silent audit")))return;}
            else if(!Check(After>.04 && After>Before*4+.01,TEXT("ordinary output audible before any monitor submix probe")))return;
            Meter=MakeShared<FEWMonitorMeter,ESPMode::ThreadSafe>();
            if(auto Device=GetWorld()->GetAudioDevice())Device->RegisterSubmixBufferListener(Meter.ToSharedRef(),*S->OutputSubmix());
            S->CaptureSurface(FPaths::ChangeExtension(Report,TEXT("-texture.png")));FScreenshotRequest::RequestScreenshot(FPaths::ChangeExtension(Report,TEXT("-tone.png")),false,false);
            S->CaptureBackSurface(FPaths::ChangeExtension(Report,TEXT("-texture-back.png")));
            Phase=20;Stage=Now;Measuring=false;
        }
        else if(Age>20)Finish(TEXT("browser test tone did not produce audio"));return;
    }
    if(Phase==20)
    {
        auto* PC=UGameplayStatics::GetPlayerController(this,0);
        const FVector Origin=S->GetActorLocation();
        const FVector Listener=Origin+(AudioCase<2?FVector(1500,-1500,0):AudioCase==2?FVector(0,-18000,0):FVector(0,-1500,0));
        if(AudioCase<4)PC->SetAudioListenerOverride(nullptr,Listener,FRotator(0,AudioCase==1?-90:90,0));
        else PC->ClearAudioListenerOverride();
        if(!Measuring && Age>1.5){Meter->Reset();MainMeter->Reset();Measuring=true;Stage=Now;return;}
        if(Measuring && Age>1.5)
        {
            auto O=Meter->Snapshot();O->SetNumberField(TEXT("case"),AudioCase);O->SetStringField(TEXT("listener"),(AudioCase<4?Listener:P->GetActorLocation()+FVector(0,0,74)).ToString());Samples.Add(MakeShared<FJsonValueObject>(O));
            O->SetBoolField(TEXT("actual_player_listener"),AudioCase>=4);O->SetStringField(TEXT("player"),P->GetActorLocation().ToString());
            O->SetObjectField(TEXT("main_output"),MainMeter->Snapshot());O->SetNumberField(TEXT("app_volume"),FApp::GetVolumeMultiplier());
            O->SetBoolField(TEXT("has_focus"),FApp::HasFocus());
            if(!Check(O->GetNumberField(TEXT("frames"))>10000,TEXT("post-spatialization samples ")+FString::FromInt(AudioCase)))return;
            ++AudioCase;Stage=Now;Measuring=false;
            if(AudioCase==4)G->SetMenu(EEWMenu::Monitor);
            if(AudioCase==5)G->SetMenu(EEWMenu::None);
            if(AudioCase==6)
            {
                const auto A=Samples[0]->AsObject(),C=Samples[1]->AsObject(),Far=Samples[2]->AsObject(),Near=Samples[3]->AsObject();
                const double AL=A->GetNumberField(TEXT("left_rms")),AR=A->GetNumberField(TEXT("right_rms")),CL=C->GetNumberField(TEXT("left_rms")),CR=C->GetNumberField(TEXT("right_rms"));
                if(!Check(FMath::Max(AL,AR)>.001 && (AL-AR)*(CL-CR)<0 && FMath::Max(AL,AR)>FMath::Min(AL,AR)*1.5,TEXT("turning reverses audible left and right")))return;
                if(!Check(Far->GetNumberField(TEXT("left_rms"))+Far->GetNumberField(TEXT("right_rms"))<(Near->GetNumberField(TEXT("left_rms"))+Near->GetNumberField(TEXT("right_rms")))*.02,TEXT("distant monitor becomes inaudible")))return;
                for(int I=4;I<6;++I)
                {
                    const auto ListenerOutput=Samples[I]->AsObject()->GetObjectField(TEXT("main_output"));
                    const bool Silent=FParse::Param(FCommandLine::Get(),TEXT("EWSilentAudit"));
                    if(!Check(Samples[I]->AsObject()->GetNumberField(TEXT("left_rms"))>.02
                        && (Silent?ListenerOutput->GetNumberField(TEXT("left_rms"))<.0000001:ListenerOutput->GetNumberField(TEXT("left_rms"))>.02),
                        I==4?TEXT("spatial source at real plaza position with controls open"):TEXT("spatial source at real plaza position while exploring")))return;
                }
                S->Stop();Phase=21;Stage=Now;
            }
        }return;
    }
    if(Phase==21 && Age>1)
    {
        if(!Check(!S->Sound()->IsPlaying() && !B->HasPage(),TEXT("stop clears picture and positional audio")))return;
        for(TActorIterator<AEWLift> It(GetWorld());It;++It)if(It->Spec.Id==TEXT("0,0/0")){GateLift=*It;break;}
        if(!GateLift.IsValid()){Finish(TEXT("missing gate capture lift"));return;}
        auto* L=GateLift.Get();GateStop=L->Spec.Stops.Num()-5;const auto& Stop=L->Spec.Stops[GateStop];
        const FVector Position=L->GetActorTransform().TransformPosition(Stop.Entry+(Stop.Entry-Stop.Threshold).GetSafeNormal2D()*170.)+FVector(0,0,91);
        P->ClearHeldInput();P->SetBase(static_cast<UPrimitiveComponent*>(nullptr));P->SetActorLocation(Position,false,nullptr,ETeleportType::TeleportPhysics);P->ResetSafeLocation();
        if(auto* PC=UGameplayStatics::GetPlayerController(this,0))PC->SetControlRotation((L->GetActorTransform().TransformPosition(Stop.Threshold+FVector(0,0,82))-(Position+FVector(0,0,74))).Rotation());
        Phase=30;Stage=Now;
    }
    if(Phase==30 && Age>2)
    {FScreenshotRequest::RequestScreenshot(FPaths::ChangeExtension(Report,TEXT("-gate-closed.png")),false,false);Phase=31;Stage=Now;}
    if(Phase==31 && Age>1)
    {if(!GateLift->Call(GateStop)){Finish(TEXT("gate call failed"));return;}Phase=32;Stage=Now;}
    if(Phase==32 && !GateLift->IsMoving() && Age>2)
    {FScreenshotRequest::RequestScreenshot(FPaths::ChangeExtension(Report,TEXT("-gate-open.png")),false,false);Phase=33;Stage=Now;return;}
    if(Phase==33 && Age>1)
    {
        for(TActorIterator<AEWChunkActor> It(GetWorld());It;++It)
            if(It->ClockFaces.Num() && It->Recipe && FMath::Abs(It->Recipe->Coord.X)<=1 && FMath::Abs(It->Recipe->Coord.Y)<=1){ClockCapture=It->ClockFaces[0];break;}
        if(!ClockCapture.IsValid()){Finish(TEXT("no nearby clock to render"));return;}
        auto* Face=ClockCapture.Get();const FVector Normal=Face->GetForwardVector();
        const FVector Eye=Face->GetComponentLocation()+Normal*450;
        // A camera-only inspection keeps streaming/floor recovery from moving
        // the character down while the dial's rendering is being measured.
        auto* Camera=GetWorld()->SpawnActor<ACameraActor>(Eye,(-Normal).Rotation());
        Camera->GetCameraComponent()->FieldOfView=85;
        if(auto* PC=UGameplayStatics::GetPlayerController(this,0))PC->SetViewTarget(Camera);
        Phase=34;Stage=Now;return;
    }
    if(Phase==34 && Age>2)
    {ClockSamples.Add(MakeShared<FJsonValueObject>(EWClock::FaceEvidence(ClockCapture.Get())));FScreenshotRequest::RequestScreenshot(FPaths::ChangeExtension(Report,TEXT("-clock-a.png")),false,false);Phase=35;Stage=Now;return;}
    if(Phase==35 && Age>3)
    {
        ClockSamples.Add(MakeShared<FJsonValueObject>(EWClock::FaceEvidence(ClockCapture.Get())));
        if(!Check(ClockSamples[1]->AsObject()->GetNumberField(TEXT("paint_count"))>ClockSamples[0]->AsObject()->GetNumberField(TEXT("paint_count"))
            && ClockSamples[0]->AsObject()->GetNumberField(TEXT("second_degrees"))!=ClockSamples[1]->AsObject()->GetNumberField(TEXT("second_degrees")),TEXT("visible clock repaints and advances with system time")))return;
        FScreenshotRequest::RequestScreenshot(FPaths::ChangeExtension(Report,TEXT("-clock-b.png")),false,false);Phase=36;Stage=Now;return;
    }
    if(Phase==36 && Age>1)
    {
        const auto Plaza=G->Manager->RecipeAt({0,0});
        const FVector Eye=G->Manager->ToRender({0,0},Plaza->Hub+FVector(0,-1400,164));
        FloorCamera=GetWorld()->SpawnActor<ACameraActor>(Eye,FRotator(-18,90,0));
        FloorCamera->GetCameraComponent()->FieldOfView=85;
        if(auto* PC=UGameplayStatics::GetPlayerController(this,0))PC->SetViewTarget(FloorCamera);
        Phase=37;Stage=Now;return;
    }
    if(Phase==37 && Age>2)
    {FScreenshotRequest::RequestScreenshot(FPaths::ChangeExtension(Report,TEXT("-floor-a.png")),false,false);Phase=38;Stage=Now;return;}
    if(Phase==38 && Age>1)
    {FloorCamera->AddActorWorldOffset(FVector(70,100,0));FloorCamera->SetActorRotation(FRotator(-24,90,0));Phase=39;Stage=Now;return;}
    if(Phase==39 && Age>2)
    {FScreenshotRequest::RequestScreenshot(FPaths::ChangeExtension(Report,TEXT("-floor-b.png")),false,false);Phase=40;Stage=Now;return;}
    if(Phase==40 && Age>1)Finish();
}
