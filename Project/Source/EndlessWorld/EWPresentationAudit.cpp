#include "EWPresentationAudit.h"
#include "EWGameInstance.h"
#include "EWCharacter.h"
#include "EWChunkManager.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "UnrealClient.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformProcess.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

AEWPresentationAudit::AEWPresentationAudit()
{ PrimaryActorTick.bCanEverTick = true; PrimaryActorTick.TickGroup = TG_PostPhysics; }
void AEWPresentationAudit::BeginPlay()
{
    Super::BeginPlay(); Started = FPlatformTime::Seconds();
    ReportPath = FPaths::ProjectSavedDir() / TEXT("Verification/presentation-") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".json");
    FParse::Value(FCommandLine::Get(), TEXT("EWReport="), ReportPath);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
}
bool AEWPresentationAudit::Check(bool Pass, const FString& Name)
{
    auto O = MakeShared<FJsonObject>(); O->SetStringField(TEXT("check"), Name); O->SetBoolField(TEXT("pass"), Pass);
    Checks.Add(MakeShared<FJsonValueObject>(O)); UE_LOG(LogTemp, Display, TEXT("EW_PRESENTATION_CHECK %s %d"), *Name, Pass);
    if (!Pass) Finish(Name); return Pass;
}
void AEWPresentationAudit::Finish(const FString& Failure)
{
    if (bFinished) return; bFinished = true;
    auto O = MakeShared<FJsonObject>(); O->SetBoolField(TEXT("success"), Failure.IsEmpty()); O->SetStringField(TEXT("failure"), Failure);
    O->SetStringField(TEXT("scope"), TEXT("Foreground GPU integration. Display rate counts Streamline-reported actual presentations per real frame over wall time. External presentation capture and temporal image quality are separate acceptance checks."));
    O->SetNumberField(TEXT("seconds"), FPlatformTime::Seconds() - Started); O->SetArrayField(TEXT("checks"), Checks); O->SetArrayField(TEXT("samples"), Samples);
    FString S; FJsonSerializer::Serialize(O, TJsonWriterFactory<>::Create(&S));
    FFileHelper::SaveStringToFile(S, *ReportPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp, Display, TEXT("EW_PRESENTATION_DONE %s"), Failure.IsEmpty() ? TEXT("PASS") : *Failure);
    FPlatformMisc::RequestExit(false);
}
void AEWPresentationAudit::Tick(float Delta)
{
    Super::Tick(Delta); if (bFinished) return;
    if (Phase == 2 && Cases.IsValidIndex(Step) && Cases[Step].WorkMs > 0)
        FPlatformProcess::SleepNoStats(Cases[Step].WorkMs / 1000.0f);
    const double Now = FPlatformTime::Seconds();
    if (Now - Started > 900) { Finish(TEXT("foreground or scene timeout")); return; }
    auto* G = GetGameInstance<UEWGameInstance>();
    auto* P = Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    if (!G || !G->Manager || !P || G->Manager->IsTravelling() || P->IsStreamingHeld()) return;
    const auto E = G->Graphics.Evidence();
    if (!E->GetBoolField(TEXT("foreground")))
    { NextAction = Now + 3; LastFrame = Now; SampleSeconds = 0; RenderCount = PresentedCount = GeneratedRenderCount = 0; return; }
    if (Phase == 0)
    {
        if (G->Manager->ReadyCount() < 49 || !P->GetCharacterMovement()->IsMovingOnGround()) return;
        G->SetMenu(EEWMenu::None); G->Graphics.SetVSync(false); G->Graphics.SetSuperResolution(2);
        Cases.Add({TEXT("fg_off_cap_60"), 0, 60, false});
        for (auto Option : G->Graphics.FrameGenerationOptions()) if (Option.Supported)
        {
            if (Option.Value == 2) Cases.Add({TEXT("fg_2_cap_120"), 2, 120, false});
            if (Option.Value == 3) { Cases.Add({TEXT("fg_3_cap_60"), 3, 60, false}); Cases.Add({TEXT("fg_3_cap_120"), 3, 120, false}); Cases.Add({TEXT("fg_3_cap_144"), 3, 144, false}); }
            if (Option.Value == 6) { Cases.Add({TEXT("fg_6_cap_240"), 6, 240, false}); Cases.Add({TEXT("fg_6_cap_120_cpu_load"), 6, 120, false, 12}); }
            if (Option.Value == 10) Cases.Add({TEXT("fg_dynamic_cap_120"), 10, 120, false});
        }
        Cases.Add({TEXT("fg_off_vsync_cap_120"), 0, 120, true});
        if (E->GetBoolField(TEXT("fg_vsync_supported")) && G->Graphics.CanFrameGenerate()) Cases.Add({TEXT("fg_3_vsync_cap_120"), 3, 120, true});
        Cases.Add({TEXT("fg_off_unlimited"), 0, 0, false});
        Phase = 1; NextAction = Now; return;
    }
    if (Phase == 1 && Now >= NextAction)
    {
        if (Step >= Cases.Num())
        {
            G->Graphics.SetVSync(false); G->Graphics.SetFrameGeneration(3); G->Graphics.SetMaxDisplayFPS(120);
            Phase = 3; NextAction = Now + 4; return;
        }
        const auto& C = Cases[Step];
        G->Graphics.SetVSync(false); G->Graphics.SetFrameGeneration(C.FG); G->Graphics.SetMaxDisplayFPS(C.Cap); G->Graphics.SetVSync(C.VSync);
        UE_LOG(LogTemp, Display, TEXT("EW_PRESENTATION_CASE %s"), *C.Name);
        Phase = 2; NextAction = Now + 5; LastFrame = Now; SampleSeconds = 0; RenderCount = PresentedCount = GeneratedRenderCount = 0; return;
    }
    if (Phase == 2)
    {
        const double Elapsed = Now - LastFrame; LastFrame = Now;
        if (Now < NextAction) return;
        const int32 Presented = FMath::Max(1, int32(E->GetNumberField(TEXT("frames_presented"))));
        SampleSeconds += Elapsed; ++RenderCount; PresentedCount += Presented; GeneratedRenderCount += Presented > 1 ? 1 : 0;
        if (SampleSeconds < 6) return;
        const auto& C = Cases[Step]; const double Rate = PresentedCount / SampleSeconds;
        E->SetStringField(TEXT("case"), C.Name); E->SetNumberField(TEXT("sample_seconds"), SampleSeconds);
        E->SetNumberField(TEXT("synthetic_game_thread_delay_ms"), C.WorkMs);
        E->SetNumberField(TEXT("render_fps_measured"), RenderCount / SampleSeconds); E->SetNumberField(TEXT("display_fps_measured"), Rate);
        E->SetNumberField(TEXT("generated_render_count"), GeneratedRenderCount); Samples.Add(MakeShared<FJsonValueObject>(E));
        if (!Check(E->GetNumberField(TEXT("effective_fg")) == C.FG && E->GetBoolField(TEXT("vsync_effective")) == C.VSync, C.Name + TEXT("_active"))) return;
        if (C.FG && !Check(GeneratedRenderCount > RenderCount / 2, C.Name + TEXT("_actually_generates"))) return;
        if (C.Cap && !Check(Rate <= C.Cap * 1.02 + 1, C.Name + TEXT("_includes_generated_frames_in_cap"))) return;
        if (C.Name == TEXT("fg_3_cap_60") && !Check(Rate >= 57 && RenderCount / SampleSeconds >= 19,
            TEXT("fixed_fg_limiter_does_not_divide_output_cap_twice"))) return;
        if (!Check(E->GetNumberField(TEXT("t.MaxFPS")) == C.Cap, C.Name + TEXT("_single_presentation_pacing_target"))) return;
        if (!Check(E->GetNumberField(TEXT("r.Streamline.DLSSG.UIRecomposition")) == 1 && E->GetNumberField(TEXT("r.Streamline.ClearSceneColorAlpha")) == 1, C.Name + TEXT("_ui_separated"))) return;
        ++Step; Phase = 1; NextAction = Now + .2; return;
    }
    const EEWMenu Menus[] = {EEWMenu::Main, EEWMenu::Settings, EEWMenu::Journal, EEWMenu::Lift, EEWMenu::Worlds, EEWMenu::Main};
    if (Phase == 3 && Now >= NextAction)
    {
        if (MenuIndex == UE_ARRAY_COUNT(Menus))
        {
            G->SetMenu(EEWMenu::Settings); G->Graphics.SetMaxDisplayFPS(144); G->SetQuality(false); G->SetQuality(true);
            if (!Check(G->Graphics.MaxDisplayFPS == 144, TEXT("quality_changes_preserve_display_limit"))) return;
            int32 SavedCap = -1; GConfig->GetInt(TEXT("EndlessWorld.Graphics"), TEXT("MaxDisplayFPS"), SavedCap, GGameUserSettingsIni);
            if (!Check(SavedCap == 144, TEXT("display_limit_preference_saved"))) return;
            G->Graphics.BeginDisplayChange(); G->SetMenu(EEWMenu::None); Phase = 5; NextAction = Now + .3; return;
        }
        G->SetMenu(Menus[MenuIndex]); Phase = 4; NextAction = Now + 1; return;
    }
    if (Phase == 4 && Now >= NextAction)
    {
        const auto V = G->Graphics.Evidence();
        if (!Check(V->GetNumberField(TEXT("effective_fg")) == 0 && G->Graphics.FrameGeneration == 3 && G->Graphics.MaxDisplayFPS == 120 &&
            V->GetNumberField(TEXT("render_fps_limit")) == 120, FString::Printf(TEXT("menu_%d_pauses_without_changing_preferences"), MenuIndex))) return;
        G->SetMenu(EEWMenu::None);
        const auto R = G->Graphics.Evidence();
        if (!Check(R->GetNumberField(TEXT("effective_fg")) == 3 && R->GetNumberField(TEXT("render_fps_limit")) == 40,
            FString::Printf(TEXT("menu_%d_restores_multiplier_and_display_cap"), MenuIndex))) return;
        ++MenuIndex; Phase = 3; NextAction = Now + 1; return;
    }
    if (Phase == 5 && Now >= NextAction)
    {
        if (!Check(E->GetNumberField(TEXT("effective_fg")) == 0 && E->GetStringField(TEXT("fg_suspend_reason")) == TEXT("display_change"), TEXT("display_change_pauses_fg"))) return;
        Phase = 6; NextAction = Now + 2; return;
    }
    if (Phase == 6 && Now >= NextAction)
    {
        if (!Check(E->GetNumberField(TEXT("effective_fg")) == 3 && G->Graphics.MaxDisplayFPS == 144, TEXT("display_change_restores_fg_and_cap"))) return;
        G->Graphics.SetVSync(true); const int32 Before = G->Graphics.FrameGeneration;
        G->Graphics.SetFrameGeneration(10);
        if (!Check(!G->Graphics.VSync || G->Graphics.FrameGeneration == Before, TEXT("dynamic_and_vsync_are_not_combined"))) return;
        Finish();
    }
}
