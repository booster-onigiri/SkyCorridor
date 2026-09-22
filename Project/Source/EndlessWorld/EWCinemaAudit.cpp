#include "EWCinemaAudit.h"
#include "EWCinemaPlan.h"
#include "EWMediaScreen.h"
#include "EWBrowserSurface.h"
#include "EWGameInstance.h"
#include "EWChunkManager.h"
#include "EWCharacter.h"
#include "EWDayCycle.h"
#include "EWLightingViews.h"
#include "EWClock.h"
#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/AudioComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "AudioDevice.h"
#include "ISubmixBufferListener.h"
#include "Sound/SoundSubmix.h"
#include "Misc/ScopeLock.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"
#include "RHI.h"
#include "Misc/App.h"

class FEWCinemaMeter:public ISubmixBufferListener
{
public:
    FCriticalSection Mutex;double L=0,R=0;int64 Frames=0;
    void OnNewSubmixBuffer(const USoundSubmix*,float* Data,int32 N,int32 C,int32,double) override
    {FScopeLock Lock(&Mutex);if(C<2)return;for(int32 I=0;I<N;I+=C){L+=Data[I]*Data[I];R+=Data[I+1]*Data[I+1];++Frames;}}
    void Reset(){FScopeLock Lock(&Mutex);L=R=0;Frames=0;}
    TSharedRef<FJsonObject> Snapshot()
    {FScopeLock Lock(&Mutex);auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("left_rms"),Frames?FMath::Sqrt(L/Frames):0);O->SetNumberField(TEXT("right_rms"),Frames?FMath::Sqrt(R/Frames):0);O->SetNumberField(TEXT("frames"),double(Frames));return O;}
};
AEWCinemaAudit::AEWCinemaAudit(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;}
void AEWCinemaAudit::BeginPlay()
{Super::BeginPlay();FApp::SetUnfocusedVolumeMultiplier(1);FApp::SetVolumeMultiplier(1);Started=Stage=FPlatformTime::Seconds();Report=FPaths::ProjectSavedDir()/TEXT("Verification/cinema-audit.json");FParse::Value(FCommandLine::Get(),TEXT("EWReport="),Report);}
bool AEWCinemaAudit::Check(bool Value,const FString& Name)
{auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("check"),Name);O->SetBoolField(TEXT("pass"),Value);Checks.Add(MakeShared<FJsonValueObject>(O));if(!Value)Finish(Name);return Value;}
void AEWCinemaAudit::Capture(const FString& Name)
{if(!GUsingNullRHI)FScreenshotRequest::RequestScreenshot(FPaths::GetPath(Report)/Name+TEXT(".png"),false,false);}
void AEWCinemaAudit::PlaceListener(FVector Local,float Yaw)
{
    auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if(!P || !PC)return;P->LeaveSeat();P->ClearHeldInput();P->GetCharacterMovement()->DisableMovement();
    P->SetActorLocationAndRotation(Auditorium.TransformPosition(Local),FRotator(0,Auditorium.Rotator().Yaw+Yaw,0),false,nullptr,ETeleportType::TeleportPhysics);
    PC->SetControlRotation(FRotator(0,Auditorium.Rotator().Yaw+Yaw,0));
}
void AEWCinemaAudit::Finish(const FString& Error)
{
    if(Finished)return;Finished=true;auto* G=GetGameInstance<UEWGameInstance>();auto O=MakeShared<FJsonObject>();
    if(auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0)))P->SetTestMovement(FVector::ZeroVector,false);
    O->SetBoolField(TEXT("success"),Error.IsEmpty());O->SetStringField(TEXT("failure"),Error);
    O->SetBoolField(TEXT("rendered_audio_run"),!GUsingNullRHI);O->SetNumberField(TEXT("seconds"),FPlatformTime::Seconds()-Started);
    O->SetArrayField(TEXT("checks"),Checks);O->SetArrayField(TEXT("audio_cases"),AudioCases);O->SetArrayField(TEXT("day_cases"),DayCases);
    if(G && G->CinemaScreen)O->SetObjectField(TEXT("cinema"),G->CinemaScreen->Evidence());
    if(G && G->DayCycle)O->SetObjectField(TEXT("day_cycle"),G->DayCycle->Evidence());
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Report),true);FString Text;
    FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*Report,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp,Display,TEXT("EW_CINEMA_AUDIT %s"),*Error);FPlatformMisc::RequestExit(false);
}
void AEWCinemaAudit::EndPlay(const EEndPlayReason::Type Reason)
{
    if(auto* G=GetGameInstance<UEWGameInstance>())if(G->CinemaScreen && G->CinemaScreen->OutputSubmix() && Meter)
        if(auto D=GetWorld()->GetAudioDevice())D->UnregisterSubmixBufferListener(Meter.ToSharedRef(),*G->CinemaScreen->OutputSubmix());
    if(auto* PC=UGameplayStatics::GetPlayerController(this,0))PC->ClearAudioListenerOverride();
    Meter.Reset();Super::EndPlay(Reason);
}
void AEWCinemaAudit::Tick(float Delta)
{
    Super::Tick(Delta);if(Finished)return;
    auto* G=GetGameInstance<UEWGameInstance>();auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
    auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if(!G || !G->Manager || !G->CinemaScreen || !G->DayCycle || !P || !PC)return;
    const double Now=FPlatformTime::Seconds(),Age=Now-Stage;
    if(Now-Started>600){Finish(TEXT("audit timeout"));return;}
    if(G->Manager->IsTravelling() || P->IsStreamingHeld() || G->Manager->ReadyCount()<49)return;
    auto* S=G->CinemaScreen.Get();auto* Move=P->GetCharacterMovement();auto B=S->Browser();
    if(Phase==0)
    {
        if(FParse::Param(FCommandLine::Get(),TEXT("EWDayVisualOnly")))
        {FParse::Value(FCommandLine::Get(),TEXT("EWDayFirst="),DayCase);DayCase=FMath::Clamp(DayCase,0,FParse::Param(FCommandLine::Get(),TEXT("EWLightingVisualAudit"))?5:3);Phase=7;Stage=Now;return;}
        G->SetMenu(EEWMenu::None);G->DayCycle->SetAuditHour(12);
        const auto R=G->Manager->RecipeAt({0,0});int32 Count=0;FString Error;
        if(R)for(const auto& Room:R->Interiors)Count+=Room.Kind==10;
        if(!Check(R && R->Validate(Error) && Count==1,TEXT("one cinema in reference plaza, valid streamed recipe")))return;
        if(!Check(EWCinemaPlan::Seats().Num()==32,TEXT("32 authored seats")))return;
        if(!Check(FMath::IsNearlyEqual(AEWDayCycle::HourAt(1800),AEWDayCycle::HourAt(0),.000001)
            && FMath::IsNearlyEqual(AEWDayCycle::HourAt(450),16.,.000001)
            && FMath::IsNearlyEqual(AEWDayCycle::HourAt(75),11.,.000001),TEXT("30 real minutes per day, independent of frame rate")))return;
        if(!Check(EWClock::AnglesAt(FDateTime(2026,9,12,3,15,30)).Equals(FVector(97.75,93,180),.00001),TEXT("wall clocks retain real hour minute second time")))return;
        for(const FVector Outside:{FVector(0,-1101,200),FVector(0,-1400,160),FVector(1201,0,200),FVector(-1201,0,200),FVector(0,1401,200),FVector(0,0,1131),FVector(0,0,-1)})
            if(!Check(EWCinemaPlan::AudioGain(Outside)==0,TEXT("auditorium bounds exclude ")+Outside.ToString()))return;
        G->VisitCinema();Phase=1;Stage=Now;return;
    }
    if(Phase==1)
    {
        if(Age<2 || !S->Available() || !Move->IsMovingOnGround())return;
        Auditorium=S->RoomFrame();Capture(TEXT("cinema-entrance"));Stage=SegmentStarted=Now;Phase=2;return;
    }
    if(Phase==2)
    {
        const auto Route=EWCinemaPlan::Route();const FVector Goal=Auditorium.TransformPosition(Route[Waypoint]);
        FVector Direction=Goal-P->GetActorLocation();Direction.Z=0;
        if(Direction.Size2D()<40)
        {
            P->SetTestMovement(FVector::ZeroVector,false);++Waypoint;SegmentStarted=Now;
            if(Waypoint>=Route.Num())
            {
                if(!Check(S->ListenerInside() && P->FallRecoveries==0,TEXT("walk from public gallery through vestibule and all six stairs")))return;
                Stage=Now;Phase=3;
            }
            return;
        }
        if(Now-SegmentStarted>20){Finish(FString::Printf(TEXT("route blocked at %d : %s"),Waypoint,*Auditorium.InverseTransformPosition(P->GetActorLocation()).ToString()));return;}
        PC->SetControlRotation(Direction.Rotation());P->SetTestMovement(Direction.GetSafeNormal(),true);return;
    }
    if(Phase==3)
    {
        if(Age<.12)return;Stage=Now;const auto Seats=EWCinemaPlan::Seats();
        if(SeatIndex<Seats.Num())
        {
            if(!Check(S->SitSeat(SeatIndex),TEXT("sit ")+FString(Seats[SeatIndex].Name)))return;
            const FVector Eye=Auditorium.TransformPosition(Seats[SeatIndex].Position)+FVector(0,0,47);
            for(FVector Corner:{FVector(-660,1300,250),FVector(660,1300,250),FVector(-660,1300,990),FVector(660,1300,990)})
            {
                FHitResult Hit;FCollisionQueryParams Q;Q.AddIgnoredActor(P);
                if(!Check(!GetWorld()->LineTraceSingleByChannel(Hit,Eye,Auditorium.TransformPosition(Corner),ECC_Visibility,Q),TEXT("screen sightline ")+FString(Seats[SeatIndex].Name)+Corner.ToString()))return;
            }
            if(!Check(P->LeaveSeat(),TEXT("stand clear of furniture ")+FString(Seats[SeatIndex].Name)))return;
            ++SeatIndex;return;
        }
        S->SitSeat(27);Phase=4;Stage=Now;return;
    }
    if(Phase==4)
    {
        if(Age<4)return;Capture(TEXT("cinema-auditorium"));
        if(GUsingNullRHI){Phase=7;Stage=Now;return;}
        if(!Check(G->DayCycle->Evidence()->GetBoolField(TEXT("parameter_collection")),TEXT("cooked emissive day-cycle collection loaded")))return;
        G->MediaScreen->Browser()->TestTone();
        Meter=MakeShared<FEWCinemaMeter,ESPMode::ThreadSafe>();
        if(auto D=GetWorld()->GetAudioDevice())D->RegisterSubmixBufferListener(Meter.ToSharedRef(),*S->OutputSubmix());
        Phase=5;Stage=Now;return;
    }
    if(Phase==5)
    {
        static const FVector Positions[]={FVector(0,-750,204),FVector(0,-750,204),FVector(0,-750,204),
            FVector(0,-1400,89),FVector(1350,0,89),FVector(0,0,1230),FVector(0,0,-200),FVector(0,1540,89),FVector(0,-750,204)};
        PlaceListener(Positions[AudioCase],AudioCase==1?-90:90);
        if(AudioCase==0 || AudioCase==2)B->TestStereoTone(AudioCase==0?0:1);
        Clicked=AudioCase!=0 && AudioCase!=2;Measuring=false;Stage=Now;Phase=6;return;
    }
    if(Phase==6)
    {
        const auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0);
        PC->SetAudioListenerOverride(nullptr,Camera->GetCameraLocation(),Camera->GetCameraRotation());
        if(!Clicked && Age>2){B->Mouse(220,235,1);B->Mouse(220,235,2);if(AudioCase==0){G->MediaScreen->Browser()->Mouse(220,235,1);G->MediaScreen->Browser()->Mouse(220,235,2);}Clicked=true;}
        if(!Measuring && Age>4){Meter->Reset();Measuring=true;}
        if(Age<6)return;
        auto O=Meter->Snapshot();const double L=O->GetNumberField(TEXT("left_rms")),R=O->GetNumberField(TEXT("right_rms"));
        O->SetNumberField(TEXT("case"),AudioCase);O->SetNumberField(TEXT("app_volume"),FApp::GetVolumeMultiplier());O->SetObjectField(TEXT("plaza"),G->MediaScreen->Evidence());O->SetObjectField(TEXT("screen"),S->Evidence());AudioCases.Add(MakeShared<FJsonValueObject>(O));
        if(!Check(O->GetNumberField(TEXT("frames"))>24000,TEXT("audio device samples case ")+FString::FromInt(AudioCase)))return;
        if(AudioCase==0){if(!Check(G->MediaScreen->Browser()->HasAudio() && B->Evidence()->GetNumberField(TEXT("runtime_browser_owners"))==2
            && !G->MediaScreen->Sound()->IsPlaying(),TEXT("two independent browsers, plaza sound excluded from cinema")))return;
            if(!Check(L>.0001 && L>R*1.05,TEXT("left channel originates at screen left")))return;}
        else if(AudioCase==1){if(!Check(R>.0001 && R>L*1.05,TEXT("turning around reverses perceived speaker direction")))return;}
        else if(AudioCase==2 || AudioCase==8)
        {if(!Check(R>.0001 && R>L*1.05,TEXT("right channel and resumed auditorium playback ")+FString::FromInt(AudioCase)))return;}
        else if(!Check(L<.0000001 && R<.0000001 && S->Evidence()->GetNumberField(TEXT("queued_audio_bytes"))==0
            && S->Evidence()->GetNumberField(TEXT("right_queued_audio_bytes"))==0 && !S->Sound()->IsPlaying(),
            TEXT("no emitted or queued audio outside auditorium case ")+FString::FromInt(AudioCase)))return;
        ++AudioCase;Stage=Now;Phase=AudioCase<9?5:7;return;
    }
    if(Phase==7)
    {
        S->Stop();G->MediaScreen->Stop();PC->ClearAudioListenerOverride();P->LeaveSeat();Move->SetMovementMode(MOVE_Walking);
        G->ReturnToPlaza();Phase=8;Stage=Now;return;
    }
    if(Phase==8)
    {
        if(Age<2)return;
        const auto R=G->Manager->RecipeAt({0,0});if(!R)return;
        int32 View=0;FParse::Value(FCommandLine::Get(),TEXT("EWDayView="),View);View=FMath::Clamp(View,0,4);
        const auto CameraPose=EWLighting::PreviewView(*R,View);
        auto* Camera=GetWorld()->SpawnActor<ACameraActor>(G->Manager->ToRender({0,0},CameraPose.GetLocation()),CameraPose.Rotator());
        Camera->GetCameraComponent()->FieldOfView=85;PC->SetViewTarget(Camera);
        const double Hours[]={6.6,12,17.4,23},ExtendedHours[]={5.5,6.6,12,17.4,18.5,23};
        G->DayCycle->SetAuditHour(FParse::Param(FCommandLine::Get(),TEXT("EWLightingVisualAudit"))?ExtendedHours[DayCase]:Hours[DayCase]);Stage=Now;Phase=9;return;
    }
    if(Phase==9)
    {
        double Settle=12;FParse::Value(FCommandLine::Get(),TEXT("EWDaySettle="),Settle);
        if(Age<Settle)return;const bool Extended=FParse::Param(FCommandLine::Get(),TEXT("EWLightingVisualAudit"));
        const FString Names[]={TEXT("morning"),TEXT("noon"),TEXT("sunset"),TEXT("night")},ExtendedNames[]={TEXT("dawn"),TEXT("morning"),TEXT("noon"),TEXT("sunset"),TEXT("dusk"),TEXT("night")};
        const FString Name=Extended?ExtendedNames[DayCase]:Names[DayCase];
        Capture(TEXT("city-")+Name);auto O=G->DayCycle->Evidence();O->SetStringField(TEXT("phase"),Name);
        DayCases.Add(MakeShared<FJsonValueObject>(O));
        const auto Night=O->GetObjectField(TEXT("night_fixtures"));
        if(!Check(Night->GetBoolField(TEXT("night_glow_material")) && Night->GetBoolField(TEXT("all_cast_shadows")) &&
            Night->GetIntegerField(TEXT("active_lights"))<=Night->GetIntegerField(TEXT("shadow_light_budget")),TEXT("bounded shadowed night fixtures ")+Name))return;
        if(!Check(Name==TEXT("noon")?Night->GetIntegerField(TEXT("active_lights"))==0:
            (Name!=TEXT("night") || Night->GetIntegerField(TEXT("active_arch_spots"))>0),TEXT("night fixtures switch with the sun ")+Name))return;
        if(!Check(O->GetBoolField(TEXT("sun_bound")) && O->GetBoolField(TEXT("sky_bound")) && O->GetBoolField(TEXT("exposure_bound")),TEXT("day-cycle lighting actors updated ")+Name))return;
        ++DayCase;Stage=Now;Phase=DayCase<(Extended?6:4)?8:10;return;
    }
    if(Phase==10 && Age>2){G->DayCycle->SetAuditHour(-1);Finish();}
}
