#include "EWRRHDRRuntimeAudit.h"
#include "EWGameInstance.h"
#include "EWChunkManager.h"
#include "EWCharacter.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

AEWRRHDRRuntimeAudit::AEWRRHDRRuntimeAudit()
{ PrimaryActorTick.bCanEverTick = true; }
void AEWRRHDRRuntimeAudit::BeginPlay()
{
    Super::BeginPlay(); Started = StageStarted = FPlatformTime::Seconds();
    ReportPath = FPaths::ProjectSavedDir() / TEXT("Verification/rr-hdr-") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".json");
    FParse::Value(FCommandLine::Get(), TEXT("EWReport="), ReportPath);
    StreamPath = FPaths::ChangeExtension(ReportPath, TEXT("jsonl"));
    if (IFileManager::Get().FileExists(*ReportPath) || IFileManager::Get().FileExists(*StreamPath))
    { bFinished = true; FPlatformMisc::RequestExit(false); return; }
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
    bResumeOnly = FParse::Param(FCommandLine::Get(), TEXT("EWRRHDRResumeOnly"));
}
bool AEWRRHDRRuntimeAudit::Check(bool Passed, const FString& Name)
{
    auto C = MakeShared<FJsonObject>(); C->SetStringField(TEXT("name"), Name); C->SetBoolField(TEXT("pass"), Passed);
    Checks.Add(MakeShared<FJsonValueObject>(C));
    if (!Passed) Finish(Name);
    return Passed;
}
void AEWRRHDRRuntimeAudit::Finish(const FString& Failure)
{
    if (bFinished) return; bFinished = true;
    auto O = MakeShared<FJsonObject>();
    O->SetStringField(TEXT("format"), TEXT("rr-hdr-runtime-v1"));
    O->SetBoolField(TEXT("success"), Failure.IsEmpty()); O->SetStringField(TEXT("failure"), Failure);
    O->SetBoolField(TEXT("resume_only"), bResumeOnly); O->SetBoolField(TEXT("hdr_supported"), bHDRSupported);
    O->SetBoolField(TEXT("rr_supported"), bRRSupported); O->SetNumberField(TEXT("seconds"), FPlatformTime::Seconds() - Started);
    O->SetArrayField(TEXT("checks"), Checks); O->SetArrayField(TEXT("samples"), Samples);
    O->SetStringField(TEXT("limits"), TEXT("Native NGX successes and DXGI format/color-space/presentation records verify the real output path. Human visual brightness, monitor luminance measurement, physical controls and other GPUs are separate acceptance."));
    FString JSON; FJsonSerializer::Serialize(O, TJsonWriterFactory<>::Create(&JSON));
    FFileHelper::SaveStringToFile(JSON, *ReportPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp, Display, TEXT("EW_RR_HDR_COMPLETE success=%d failure=%s"), Failure.IsEmpty(), *Failure);
    FPlatformMisc::RequestExit(false);
}
void AEWRRHDRRuntimeAudit::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds); if (bFinished) return;
    const double Now = FPlatformTime::Seconds();
    if (Now - Started > 360 || Now - StageStarted > 60) { Finish(TEXT("stage timeout")); return; }
    auto* GI = GetGameInstance<UEWGameInstance>();
    auto* P = Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    if (!GI || !GI->Manager || !P || GI->Manager->IsTravelling() || P->IsStreamingHeld()) return;
    if (!GI->bScriptedWorldAudit) { Finish(TEXT("input isolation missing")); return; }
    if (P->ForwardEvents || P->StrafeEvents || P->LookEvents || P->JumpEvents) { Finish(TEXT("physical input contaminated audit")); return; }
    if (Now < NextAction) return;
    auto& G = GI->Graphics;
    if (Phase == 0)
    {
        if (Now - Started < 8 || !GI->IsDesktopHUDActive()) return;
        bHDRSupported = G.CanHDR(); bRRSupported = G.CanReconstructRays();
        if (!Check(bHDRSupported, TEXT("active_display_supports_hdr")) || !Check(bRRSupported, TEXT("gpu_supports_ray_reconstruction"))) return;
        if (bResumeOnly)
        {
            if (!Check(G.SuperResolution == 2 && G.FrameGeneration == 3 && G.RayReconstruction && G.HDR &&
                G.HDRPeakNits == 1100 && G.HDRBrightness == 100 && G.MaxDisplayFPS == 144 && G.VSync,
                TEXT("preferences_loaded_by_second_process"))) return;
            Modes.Add({2, 3, true, true, 1100, 100});
            BaselineEvaluations = G.Evidence()->GetNumberField(TEXT("rr_native_evaluations"));
            BaselinePresents = G.Evidence()->GetNumberField(TEXT("dxgi_presents"));
            NextAction = Now + 8; Phase = 2; StageStarted = Now; return;
        }
        G.SetRayReconstruction(false); G.SetHDR(false); G.SetFrameGeneration(0);
        for (const TCHAR* Name : { TEXT("r.Shadow.Denoiser"), TEXT("r.Lumen.Reflections.ScreenSpaceReconstruction"),
            TEXT("r.Lumen.Reflections.Temporal"), TEXT("r.Lumen.Reflections.BilateralFilter"), TEXT("r.Lumen.Reflections.ExportHitT") })
            if (auto* C = IConsoleManager::Get().FindConsoleVariable(Name)) OriginalDenoisers.Add(Name, C->GetInt());
        G.SetMaxDisplayFPS(144); G.SetVSync(true);
        Modes = { {2,0,false,false}, {2,0,true,false}, {1,3,true,false},
            {2,0,true,true}, {2,3,true,true}, {2,3,true,true,600,75},
            {2,3,false,true}, {0,0,false,true}, {2,3,true,false}, {2,3,true,true,1100,100} };
        Phase = 1; StageStarted = Now;
    }
    if (Phase == 1)
    {
        if (Index >= Modes.Num()) { Finish(); return; }
        const auto& M = Modes[Index];
        G.SetFrameGeneration(0); G.SetRayReconstruction(false); G.SetSuperResolution(M.SR);
        G.SetHDRPeakNits(M.Peak); G.SetHDRBrightness(M.Brightness); G.SetHDR(M.HDR);
        G.SetRayReconstruction(M.RR); G.SetFrameGeneration(M.FG);
        BaselineEvaluations = G.Evidence()->GetNumberField(TEXT("rr_native_evaluations"));
        BaselinePresents = G.Evidence()->GetNumberField(TEXT("dxgi_presents"));
        Collected = 0; NextAction = Now + 8; StageStarted = Now; Phase = 2; return;
    }
    if (Phase == 2)
    {
        const auto& M = Modes[Index]; auto E = G.Evidence();
        E->SetNumberField(TEXT("case"), Index); E->SetNumberField(TEXT("elapsed"), Now - Started);
        E->SetBoolField(TEXT("desktop_hud"), GI->IsDesktopHUDActive());
        Samples.Add(MakeShared<FJsonValueObject>(E));
        FString JSON; FJsonSerializer::Serialize(E, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JSON));
        FFileHelper::SaveStringToFile(JSON + TEXT("\n"), *StreamPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
        if (!Check(E->GetBoolField(TEXT("sr_enabled")) == (M.SR != 0), TEXT("sr_matches_case"))) return;
        if (!Check(E->GetBoolField(TEXT("rr_enabled")) == M.RR, TEXT("rr_actual_state_matches_case"))) return;
        if (M.RR && !Check(E->GetNumberField(TEXT("rr_native_evaluations")) > BaselineEvaluations + 20, TEXT("native_rr_evaluations_advance"))) return;
        if (!Check(E->GetNumberField(TEXT("rr_native_failures")) == 0 && E->GetNumberField(TEXT("rr_creation_fallbacks")) == 0, TEXT("no_native_rr_failure_or_fallback"))) return;
        if (!M.RR)
            for (const auto& C : OriginalDenoisers)
                if (!Check(E->GetNumberField(C.Key) == C.Value, TEXT("denoiser_restored_") + C.Key)) return;
        if (!Check(E->GetBoolField(TEXT("hdr_enabled")) == M.HDR, TEXT("actual_hdr_matches_case"))) return;
        if (!Check(E->GetBoolField(TEXT("dxgi_evidence_available")) && E->GetNumberField(TEXT("dxgi_presents")) > 10, TEXT("real_swapchain_is_presenting"))) return;
        if (!Check(E->GetNumberField(TEXT("dxgi_width")) == 2560 && E->GetNumberField(TEXT("dxgi_height")) == 1440, TEXT("wqhd_swapchain"))) return;
        if (M.HDR)
        {
            if (!Check(E->GetNumberField(TEXT("dxgi_format")) == 24 && E->GetNumberField(TEXT("dxgi_color_space")) == 12 &&
                E->GetBoolField(TEXT("dxgi_color_space_explicit")), TEXT("hdr10_swapchain_pq_bt2020_10bit"))) return;
            if (!Check(E->GetNumberField(TEXT("r.HDR.Display.MaxLuminance")) == M.Peak &&
                FMath::IsNearlyEqual(E->GetNumberField(TEXT("r.HDR.Display.MidLuminance")), 15. * M.Brightness / 100., .001), TEXT("hdr_calibration_applied"))) return;
        }
        else if (!Check(E->GetNumberField(TEXT("dxgi_format")) == 87 && E->GetNumberField(TEXT("dxgi_color_space")) == 0,
            TEXT("sdr_swapchain_restored"))) return;
        if (!Check(E->GetNumberField(TEXT("effective_fg")) == M.FG && (M.FG == 0 || E->GetNumberField(TEXT("frames_presented")) == M.FG), TEXT("frame_generation_with_rr_and_hdr"))) return;
        if (!Check(E->GetBoolField(TEXT("vsync_effective")) && E->GetNumberField(TEXT("pacing_target_fps")) == 144, TEXT("vsync_and_generated_inclusive_cap_retained"))) return;
        if (!Check(E->GetNumberField(TEXT("presented_fps")) <= 148, TEXT("generated_frame_rate_stays_below_cap_tolerance"))) return;
        if (!Check(E->GetBoolField(TEXT("desktop_hud")), TEXT("desktop_hud_visible_with_output_mode"))) return;
        if (++Collected < 3) { NextAction = Now + 1; return; }
        if (bResumeOnly) { Finish(); return; }
        ++Index; Phase = 1; NextAction = Now + .25; StageStarted = Now;
    }
}
