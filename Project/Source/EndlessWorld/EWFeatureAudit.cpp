#include "EWFeatureAudit.h"
#include "EWGameInstance.h"
#include "EWCharacter.h"
#include "EWChunkManager.h"
#include "EWGamepadModule.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#if EW_WITH_NVIDIA
#include "StreamlineLibraryDLSSG.h"
#endif

AEWFeatureAudit::AEWFeatureAudit(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostPhysics;}
void AEWFeatureAudit::BeginPlay()
{
    Super::BeginPlay();Started=PhaseStart=FPlatformTime::Seconds();
    bPad=FParse::Param(FCommandLine::Get(),TEXT("EWGamepadAudit"));
    ReportPath=FPaths::ProjectSavedDir()/TEXT("Verification/features-")+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT(".json");
    FParse::Value(FCommandLine::Get(),TEXT("EWReport="),ReportPath);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath),true);
    UE_LOG(LogTemp,Display,TEXT("EW_FEATURE_AUDIT waiting for loaded world and foreground viewport"));
}
void AEWFeatureAudit::Check(bool Pass,const FString& Name)
{
    auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("check"),Name);O->SetBoolField(TEXT("pass"),Pass);Checks.Add(MakeShared<FJsonValueObject>(O));
    UE_LOG(LogTemp,Display,TEXT("EW_FEATURE_CHECK %s %d"),*Name,Pass);if(!Pass)Finish(Name);
}
void AEWFeatureAudit::Finish(const FString& Failure)
{
    if(bFinished)return;bFinished=true;
    if(auto* Pad=FEWGamepadModule::GetIfLoaded())if(bPad)Pad->AuditVirtualConnect(false);
    auto* G=GetGameInstance<UEWGameInstance>();
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),bPad && Failure.IsEmpty());O->SetStringField(TEXT("failure"),Failure);
    O->SetBoolField(TEXT("audit_completed"),Failure.IsEmpty());
    O->SetBoolField(TEXT("observed_checks_passed"),Failure.IsEmpty());
    O->SetStringField(TEXT("status"),!Failure.IsEmpty()?TEXT("FAIL"):bPad?TEXT("PASS"):TEXT("PARTIAL_NOT_MEASURED"));
    if(!bPad)
    {
        O->SetStringField(TEXT("native_sr_evaluation_status"),TEXT("NOT_MEASURED"));
        O->SetStringField(TEXT("native_rr_evaluation_status"),TEXT("NOT_MEASURED"));
        O->SetStringField(TEXT("independent_presented_fps_status"),TEXT("NOT_MEASURED"));
        O->SetStringField(TEXT("image_quality_status"),TEXT("NOT_MEASURED"));
        O->SetNumberField(TEXT("settle_seconds_per_mode"),8);
        O->SetNumberField(TEXT("sampling_seconds_per_mode"),5);
    }
    O->SetStringField(TEXT("scope"),bPad?TEXT("SDL virtual joystick through the production Sony input device, Slate, and CharacterMovement. Physical DualSense USB/Bluetooth and Xbox XInput hardware are not represented."):
        TEXT("Supported DLSS/Streamline modes in a rendered world: configured settings and stock SDK frame-generation state. SDK timing is an estimate, not independent display FPS. Native SR/RR evaluation and visual quality are not measured."));
    O->SetNumberField(TEXT("seconds"),FPlatformTime::Seconds()-Started);O->SetArrayField(TEXT("checks"),Checks);O->SetArrayField(TEXT("samples"),Samples);
    if(G)O->SetObjectField(TEXT("final_graphics"),G->Graphics.Evidence());
    FString S;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&S));FFileHelper::SaveStringToFile(S,*ReportPath,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp,Display,TEXT("EW_FEATURE_DONE %s"),Failure.IsEmpty()?(bPad?TEXT("PASS"):TEXT("PARTIAL_NOT_MEASURED")):*Failure);FPlatformMisc::RequestExit(false);
}
void AEWFeatureAudit::Tick(float Delta)
{
    Super::Tick(Delta);if(bFinished)return;
    auto* G=GetGameInstance<UEWGameInstance>();auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
    auto* PC=Cast<AEWPlayerController>(UGameplayStatics::GetPlayerController(this,0));auto* Pad=FEWGamepadModule::GetIfLoaded();
    if(!G || !G->Manager || !P || !PC)return;
    const double Now=FPlatformTime::Seconds();
    if(Now-PhaseStart>600){Finish(TEXT("world or foreground input timeout"));return;}
    if(G->Manager->IsTravelling() || P->IsStreamingHeld())return;
    if(!FSlateApplication::IsInitialized() || !FSlateApplication::Get().IsActive() ||
        (!bPad && (!GEngine || !GEngine->GameViewport || !GEngine->GameViewport->Viewport || !GEngine->GameViewport->Viewport->IsForegroundWindow())))
    {NextAction=Now+.5;return;}
    if(Phase==0)
    {
        if(G->Manager->ReadyCount()<49 || !P->GetCharacterMovement()->IsMovingOnGround())return;
        PhaseStart=Now;NextAction=Now+2;
        if(bPad){if(!Pad || !Pad->AuditVirtualConnect(true)){Finish(TEXT("SDL virtual device attach failed"));return;}Phase=1;}
        else
        {
            auto Skip=[this](const FString& Name)
            {auto C=MakeShared<FJsonObject>();C->SetStringField(TEXT("check"),Name);C->SetStringField(TEXT("status"),TEXT("SKIPPED_UNSUPPORTED"));Checks.Add(MakeShared<FJsonValueObject>(C));};
            bool Quality=false;
            for(auto M:G->Graphics.SuperResolutionOptions())
            {if(M.Supported){Modes.Add({M.Value,0,false});Quality|=M.Value==2;}else Skip(FString::Printf(TEXT("sr_%d"),M.Value));}
            for(auto M:G->Graphics.FrameGenerationOptions())if(M.Value)
            {if(Quality && M.Supported)Modes.Add({2,M.Value,false});else Skip(FString::Printf(TEXT("fg_%d"),M.Value));}
            if(Quality && G->Graphics.CanReconstructRays())
            {
                Modes.Add({2,0,true});
                for(auto M:G->Graphics.FrameGenerationOptions())if(M.Supported && (M.Value==2 || M.Value==10))Modes.Add({2,M.Value,true});
            }
            else Skip(TEXT("ray_reconstruction"));
            Modes.Add({0,0,false});Phase=100;
        }
        return;
    }
    if(bPad)
    {
        if(Phase==5)MaxJumpZ=FMath::Max(MaxJumpZ,P->GetActorLocation().Z);
        if(Now<NextAction)return;
        switch(Phase)
        {
        case 1:
            Check(Pad->SonyConnected(),TEXT("virtual_device_detected_by_production_adapter"));if(bFinished)return;
            Before=P->GetActorLocation();Pad->AuditVirtualAxis(1,-.8f);NextAction=Now+1.1;Phase=2;break;
        case 2:
            Pad->AuditVirtualAxis(1,0);Check(FVector::Dist2D(Before,P->GetActorLocation())>180,TEXT("left_stick_moves_character"));if(bFinished)return;
            LookBefore=PC->GetControlRotation();Pad->AuditVirtualAxis(2,.8f);Pad->AuditVirtualAxis(3,-.5f);NextAction=Now+.65;Phase=3;break;
        case 3:
            Pad->AuditVirtualAxis(2,0);Pad->AuditVirtualAxis(3,0);
            Check(FMath::Abs(FRotator::NormalizeAxis(PC->GetControlRotation().Yaw-LookBefore.Yaw))>20 &&
                FRotator::NormalizeAxis(PC->GetControlRotation().Pitch-LookBefore.Pitch)>5,TEXT("right_stick_looks_right_and_up"));if(bFinished)return;
            Before=P->GetActorLocation();JumpBefore=P->JumpEvents;MaxJumpZ=Before.Z;Pad->AuditVirtualButton(0,true);NextAction=Now+.12;Phase=4;break;
        case 4:Pad->AuditVirtualButton(0,false);NextAction=Now+1.4;Phase=5;break;
        case 5:
            Check(P->JumpEvents>JumpBefore && MaxJumpZ>Before.Z+40 && P->GetCharacterMovement()->IsMovingOnGround(),TEXT("cross_button_jumps_and_lands"));if(bFinished)return;
            RecordsBefore=G->RecordCount();Pad->AuditVirtualButton(2,true);NextAction=Now+.14;Phase=6;break;
        case 6:Pad->AuditVirtualButton(2,false);NextAction=Now+.4;Phase=7;break;
        case 7:
            Check(G->RecordCount()==RecordsBefore+1,TEXT("square_button_records_nearby_place"));if(bFinished)return;
            Pad->AuditVirtualButton(6,true);NextAction=Now+.12;Phase=8;break;
        case 8:Pad->AuditVirtualButton(6,false);NextAction=Now+.3;Phase=9;break;
        case 9:
            Check(G->Menu()==EEWMenu::Main && G->bSonyGamepad,TEXT("options_opens_menu_with_sony_labels"));if(bFinished)return;
            FocusBefore=PC->FocusedControlLabel();Pad->AuditVirtualButton(12,true);NextAction=Now+.12;Phase=10;break;
        case 10:Pad->AuditVirtualButton(12,false);NextAction=Now+.2;Phase=11;break;
        case 11:
            Check(PC->FocusedControlLabel()!=FocusBefore,TEXT("dpad_moves_actual_slate_focus"));if(bFinished)return;
            Phase=12;NavCount=0;NextAction=Now+.1;break;
        case 12:
            if(PC->FocusedControlLabel()==TEXT("画質と操作"))
            {Pad->AuditVirtualButton(0,true);NextAction=Now+.13;Phase=14;break;}
            if(++NavCount>18){Finish(TEXT("could not navigate to settings with dpad"));return;}
            Pad->AuditVirtualButton(12,true);NextAction=Now+.12;Phase=13;break;
        case 13:Pad->AuditVirtualButton(12,false);NextAction=Now+.18;Phase=12;break;
        case 14:Pad->AuditVirtualButton(0,false);NextAction=Now+.4;Phase=15;break;
        case 15:
            Check(G->Menu()==EEWMenu::Settings,TEXT("cross_activates_focused_settings_button"));if(bFinished)return;
            FocusBefore=PC->FocusedControlLabel();Pad->AuditVirtualAxis(1,.9f);NextAction=Now+1.2;Phase=16;break;
        case 16:
            Pad->AuditVirtualAxis(1,0);Check(PC->FocusedControlLabel()!=FocusBefore,TEXT("held_left_stick_scrolls_settings_focus"));if(bFinished)return;
            Pad->AuditVirtualButton(1,true);NextAction=Now+.12;Phase=17;break;
        case 17:Pad->AuditVirtualButton(1,false);NextAction=Now+.3;Phase=18;break;
        case 18:
            Check(G->Menu()==EEWMenu::Main,TEXT("circle_returns_to_main_menu"));if(bFinished)return;
            Pad->AuditVirtualButton(1,true);NextAction=Now+.12;Phase=19;break;
        case 19:Pad->AuditVirtualButton(1,false);NextAction=Now+.3;Phase=20;break;
        case 20:
            Check(G->Menu()==EEWMenu::None,TEXT("circle_returns_to_exploration"));if(bFinished)return;
            Pad->AuditVirtualButton(3,true);NextAction=Now+.12;Phase=21;break;
        case 21:Pad->AuditVirtualButton(3,false);NextAction=Now+.3;Phase=22;break;
        case 22:
            Check(G->Menu()==EEWMenu::Journal,TEXT("triangle_opens_journal"));if(bFinished)return;
            Pad->AuditVirtualButton(1,true);NextAction=Now+.12;Phase=23;break;
        case 23:Pad->AuditVirtualButton(1,false);NextAction=Now+.3;Phase=24;break;
        case 24:
            Pad->AuditVirtualAxis(0,.9f);Pad->AuditVirtualButton(9,true);NextAction=Now+.3;Phase=25;break;
        case 25:Pad->AuditVirtualConnect(false);NextAction=Now+.6;Phase=26;break;
        case 26:
            Check(!Pad->SonyConnected() && P->GetVelocity().Size2D()<1 && P->GetCharacterMovement()->MaxWalkSpeed==350,
                TEXT("unplug_releases_movement_and_sprint"));if(bFinished)return;
            Check(Pad->SonyEventCount()>20 && G->GamepadEvents>20,TEXT("events_reach_player_and_slate"));if(!bFinished)Finish();break;
        }
        return;
    }
    if(Phase==100 && Now>=NextAction)
    {
        if(Step>=Modes.Num()){Finish();return;}
        const auto Mode=Modes[Step];G->Graphics.SetFrameGeneration(0);G->Graphics.SetRayReconstruction(false);
        G->Graphics.SetSuperResolution(Mode.SR);G->Graphics.SetFrameGeneration(Mode.FG);G->Graphics.SetRayReconstruction(Mode.RR);
        PhaseStart=Now;NextAction=Now+8;Phase=101;
        UE_LOG(LogTemp,Display,TEXT("EW_FEATURE_GRAPHICS_MODE sr=%d fg=%d rr=%d"),Mode.SR,Mode.FG,Mode.RR);return;
    }
    if(Phase==101 && Now>=NextAction)
    {
        auto E=G->Graphics.Evidence();E->SetNumberField(TEXT("game_delta_ms_instantaneous"),Delta*1000);
        E->SetNumberField(TEXT("mode_index"),Step);E->SetNumberField(TEXT("mode_elapsed_seconds"),Now-PhaseStart);
        E->SetNumberField(TEXT("requested_sr"),Modes[Step].SR);E->SetNumberField(TEXT("requested_fg"),Modes[Step].FG);E->SetBoolField(TEXT("requested_rr"),Modes[Step].RR);
        // Preserve the real foreground/HUD/loading evidence; never substitute it.
        E->SetStringField(TEXT("sdk_timing_scope"),TEXT("stock SDK state: estimated FPS = engine average FPS * last SDK frames-presented count; not independent display timing"));
        if(GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
            E->SetStringField(TEXT("viewport"),GEngine->GameViewport->Viewport->GetSizeXY().ToString());
        Samples.Add(MakeShared<FJsonValueObject>(E));
        const auto Mode=Modes[Step];
        auto Number=[](const TSharedPtr<FJsonObject>& O,const TCHAR* Name,double& Value)
        {return O.IsValid() && O->HasTypedField<EJson::Number>(Name) && O->TryGetNumberField(Name,Value) && FMath::IsFinite(Value);};
        double ActualSR=-1,ActualFG=-1;
        Check(Number(E,TEXT("sr_preference"),ActualSR) && ActualSR==Mode.SR && E->GetBoolField(TEXT("sr_enabled"))==(Mode.SR!=0),FString::Printf(TEXT("sr_mode_%d_configured"),Mode.SR));if(bFinished)return;
        Check(Number(E,TEXT("effective_fg"),ActualFG) && ActualFG==Mode.FG,TEXT("requested_fg_effective"));if(bFinished)return;
#if EW_WITH_NVIDIA
        EStreamlineDLSSGMode Expected=EStreamlineDLSSGMode::Off;
        switch(Mode.FG){case 2:Expected=EStreamlineDLSSGMode::On2X;break;case 3:Expected=EStreamlineDLSSGMode::On3X;break;
            case 4:Expected=EStreamlineDLSSGMode::On4X;break;case 5:Expected=EStreamlineDLSSGMode::On5X;break;
            case 6:Expected=EStreamlineDLSSGMode::On6X;break;case 10:Expected=EStreamlineDLSSGMode::OnDynamic;break;}
        Check(UStreamlineLibraryDLSSG::GetDLSSGMode()==Expected,TEXT("stock_fg_mode_matches_request"));if(bFinished)return;
#endif
        Check(E->GetBoolField(TEXT("rr_configured"))==Mode.RR,TEXT("rr_configuration_matches_request"));if(bFinished)return;
        if(Now-PhaseStart<13){NextAction=Now+1;return;}
        // Check a window of SDK samples instead of treating nullable data as 0.
        if(Mode.FG)
        {
            double MaxFrames=0;int32 AvailableSamples=0;
            for(const auto& Sample:Samples)
            {
                auto O=Sample->AsObject();double Index=-1,Frames=0,Rate=0;
                if(Number(O,TEXT("mode_index"),Index) && Index==Step &&
                    Number(O,TEXT("frames_presented"),Frames) && Number(O,TEXT("presented_fps"),Rate) && Frames>0 && Rate>0)
                {++AvailableSamples;MaxFrames=FMath::Max(MaxFrames,Frames);}
            }
            E->SetNumberField(TEXT("window_sdk_samples"),AvailableSamples);E->SetNumberField(TEXT("window_max_sdk_frames_presented"),MaxFrames);
            if(!AvailableSamples || (Mode.FG==10 && MaxFrames<2))
            {
                auto C=MakeShared<FJsonObject>();C->SetStringField(TEXT("check"),TEXT("sdk_generated_frames_observed"));
                C->SetStringField(TEXT("status"),TEXT("NOT_MEASURED"));C->SetNumberField(TEXT("mode_index"),Step);
                C->SetStringField(TEXT("reason"),!AvailableSamples?TEXT("Stock SDK timing was unavailable"):
                    TEXT("Dynamic mode can present one frame when rendering already meets its display target; generation was not observed in this window"));
                Checks.Add(MakeShared<FJsonValueObject>(C));
            }
            else {Check(MaxFrames>=2,TEXT("sdk_generated_frames_observed"));if(bFinished)return;}
        }
        ++Step;Phase=100;NextAction=Now+.2;
    }
}
