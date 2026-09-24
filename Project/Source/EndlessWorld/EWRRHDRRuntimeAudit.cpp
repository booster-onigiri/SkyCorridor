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
#include "GenericPlatform/GenericPlatformMisc.h"
#include "PixelFormat.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
bool Number(const TSharedRef<FJsonObject>& E,const TCHAR* Name,double& V)
{return E->TryGetNumberField(Name,V) && FMath::IsFinite(V);}
bool Equals(const TSharedRef<FJsonObject>& E,const TCHAR* Name,double Expected)
{double V=0;return Number(E,Name,V) && FMath::IsNearlyEqual(V,Expected,.001);}
bool Flag(const TSharedRef<FJsonObject>& E,const TCHAR* Name,bool Expected=true)
{bool V=false;return E->TryGetBoolField(Name,V) && V==Expected;}
bool TextIs(const TSharedRef<FJsonObject>& E,const TCHAR* Name,const TCHAR* Expected)
{FString V;return E->TryGetStringField(Name,V) && V==Expected;}
double PaperWhite(int32 Brightness,int32 Peak)
{return FMath::Min(203.*FMath::Clamp(Brightness,50,200)/100.,double(FMath::Clamp(Peak,400,2000)));}
const TCHAR* const PolicyCVars[]={TEXT("r.HDR.Aces.Version"),TEXT("r.HDR.Display.OverrideOSMaxLuminance"),
    TEXT("r.HDR.PaperWhite.Mode"),TEXT("r.HDR.PaperWhite"),TEXT("r.HDR.UI.Luminance.Mode"),
    TEXT("r.HDR.UI.Luminance"),TEXT("r.HDR.UI.Level"),TEXT("r.HDR.UI.CompositeMode")};
bool PQMetadata(const TSharedRef<FJsonObject>& E)
{return (Equals(E,TEXT("hdr_viewport_output_device"),int32(EDisplayOutputFormat::HDR_ACES_1000nit_ST2084)) ||
    Equals(E,TEXT("hdr_viewport_output_device"),int32(EDisplayOutputFormat::HDR_ACES_2000nit_ST2084))) &&
    Equals(E,TEXT("hdr_viewport_color_gamut"),int32(EDisplayColorGamut::Rec2020_D65));}
bool ScRGBMetadata(const TSharedRef<FJsonObject>& E)
{return (Equals(E,TEXT("hdr_viewport_output_device"),int32(EDisplayOutputFormat::HDR_ACES_1000nit_ScRGB)) ||
    Equals(E,TEXT("hdr_viewport_output_device"),int32(EDisplayOutputFormat::HDR_ACES_2000nit_ScRGB))) &&
    Equals(E,TEXT("hdr_viewport_color_gamut"),int32(EDisplayColorGamut::sRGB_D65));}
}

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
    bHDROnly = FParse::Param(FCommandLine::Get(), TEXT("EWHDRReadbackOnly"));
}
bool AEWRRHDRRuntimeAudit::Check(bool Passed, const FString& Name)
{
    auto C = MakeShared<FJsonObject>(); C->SetStringField(TEXT("name"), Name); C->SetBoolField(TEXT("pass"), Passed);
    C->SetStringField(TEXT("status"),Passed?TEXT("PASS"):TEXT("FAIL"));C->SetNumberField(TEXT("case"),Index);
    Checks.Add(MakeShared<FJsonValueObject>(C));
    if (!Passed) Finish(Name);
    return Passed;
}
void AEWRRHDRRuntimeAudit::Missing(const FString& Name,const FString& Reason)
{
    if(Unmeasured.Contains(Name))return;Unmeasured.Add(Name);
    auto C=MakeShared<FJsonObject>();C->SetStringField(TEXT("name"),Name);
    C->SetStringField(TEXT("status"),TEXT("NOT_MEASURED"));C->SetStringField(TEXT("reason"),Reason);
    C->SetField(TEXT("pass"),MakeShared<FJsonValueNull>());C->SetNumberField(TEXT("first_case"),Index);
    Checks.Add(MakeShared<FJsonValueObject>(C));
}
bool AEWRRHDRRuntimeAudit::CheckHDRReadbacks(const FMode& M,const TSharedRef<FJsonObject>& E,bool TextureSample)
{
    if(!Check(Flag(E,TEXT("hdr_preference"),M.HDR) && Equals(E,TEXT("hdr_peak_nits"),M.Peak) &&
        Equals(E,TEXT("hdr_brightness"),M.Brightness),TEXT("hdr_requested_controls_match_case")))return false;
    if(!Check(Flag(E,TEXT("hdr_engine_enabled"),M.HDR) && Flag(E,TEXT("hdr_enabled"),M.HDR) &&
        Flag(E,TEXT("hdr_viewport_metadata_available")) && Flag(E,TEXT("hdr_viewport_hdr"),M.HDR),TEXT("hdr_engine_and_viewport_state_match_case")))return false;
    if(!Check(Equals(E,TEXT("viewport_width"),2560) && Equals(E,TEXT("viewport_height"),1440),TEXT("wqhd_unreal_viewport")))return false;
    if(M.HDR)
    {
        // RenderCore.cpp explicitly selects PaperWhite; PostProcessTonemap.cpp
        // selects effective peak through OverrideOSMaxLuminance. The legacy
        // MidLuminance curve is not evidence that ACES2 calibration was applied.
        const double Expected[]={2,1,1,PaperWhite(M.Brightness,M.Peak),1,80,1,1};
        static_assert(UE_ARRAY_COUNT(Expected)==UE_ARRAY_COUNT(PolicyCVars));
        for(int32 I=0;I<UE_ARRAY_COUNT(PolicyCVars);++I)
            if(!Check(Equals(E,PolicyCVars[I],Expected[I]),TEXT("hdr_effective_policy_")+FString(PolicyCVars[I])))return false;
        if(!Check(Equals(E,TEXT("r.HDR.Display.MaxLuminance"),M.Peak) && Equals(E,TEXT("hdr_effective_peak_nits"),M.Peak),TEXT("hdr_effective_peak_matches_user_control")))return false;
        if(!Check(Equals(E,TEXT("hdr_viewport_paper_white_nits"),Expected[3]) && Equals(E,TEXT("hdr_effective_ui_luminance_nits"),80),TEXT("hdr_viewport_paper_white_and_ui_readback")))return false;
        if(!Check(TextIs(E,TEXT("hdr_calibration_status"),TEXT("applied")),TEXT("hdr_calibration_readback_applied")))return false;
        if(!Check(PQMetadata(E) || ScRGBMetadata(E),TEXT("hdr_viewport_has_hdr_transfer_and_gamut")))return false;
        double Peak=0,Multiplier=0;
        if(!Check(Number(E,TEXT("hdr_viewport_maximum_luminance_nits"),Peak) && Peak>0 &&
            Number(E,TEXT("hdr_effective_scene_multiplier"),Multiplier) && Multiplier>0,TEXT("hdr_metadata_readbacks_finite")))return false;
        // Raw viewport peak may come from OS metadata/fallback. Do not confuse
        // it with the overridden effective peak or a monitor measurement.
    }
    else
    {
        if(!Check(TextIs(E,TEXT("hdr_calibration_status"),TEXT("inactive")),TEXT("hdr_calibration_inactive_in_sdr")))return false;
        double Device=0;
        if(!Check(Number(E,TEXT("hdr_viewport_output_device"),Device) && Device>=int32(EDisplayOutputFormat::SDR_sRGB) &&
            Device<=int32(EDisplayOutputFormat::SDR_ExplicitGammaMapping),TEXT("sdr_viewport_output_restored")))return false;
        for(const auto& C:OriginalHDRPolicy)
            if(!Check(Equals(E,*C.Key,C.Value),TEXT("sdr_restores_owned_hdr_policy_")+C.Key))return false;
    }
    if(TextureSample)
    {
        if(!Flag(E,TEXT("hdr_viewport_output_texture_available")))
            Missing(TEXT("unreal_output_texture"),TEXT("Render-thread target unavailable; metadata alone is not a texture-format observation."));
        else
        {
            double Format=0;FString Name;
            const bool Known=Number(E,TEXT("hdr_viewport_output_texture_format"),Format) &&
                E->TryGetStringField(TEXT("hdr_viewport_output_texture_format_name"),Name) && !Name.IsEmpty();
            // The scene viewport may use a float intermediate before PQ output.
            // Its target format is deliberately not labelled as a DXGI format.
            const bool Precision=M.HDR?(Format==PF_A2B10G10R10 || Format==PF_FloatRGBA):
                (Format==PF_B8G8R8A8 || Format==PF_R8G8B8A8 || Format==PF_FloatRGBA);
            if(!Check(Known && Precision,TEXT("unreal_output_texture_format_observed")))return false;
            bTextureObserved=true;
        }
    }
    return true;
}
void AEWRRHDRRuntimeAudit::Finish(const FString& Failure)
{
    if (bFinished) return; bFinished = true;
    auto O = MakeShared<FJsonObject>();
    const bool Completed=Failure.IsEmpty(),CompleteEvidence=Completed && Unmeasured.IsEmpty();
    O->SetStringField(TEXT("format"), TEXT("rr-hdr-runtime-v2"));
    O->SetBoolField(TEXT("success"),CompleteEvidence);O->SetBoolField(TEXT("audit_completed"),Completed);
    O->SetBoolField(TEXT("observed_checks_passed"),Completed);O->SetBoolField(TEXT("settings_and_viewport_metadata_passed"),Completed);
    O->SetStringField(TEXT("status"),!Completed?TEXT("FAIL"):CompleteEvidence?TEXT("OBSERVED_CHECKS_PASS"):TEXT("PARTIAL_NOT_MEASURED"));
    O->SetStringField(TEXT("failure"),Failure);O->SetBoolField(TEXT("hdr_readback_only"),bHDROnly);
    O->SetStringField(TEXT("hdr_output_scope"),bDXGIObserved?TEXT("UNREAL_VIEWPORT_AND_OBSERVED_DXGI_RECORDS"):TEXT("UNREAL_VIEWPORT_ONLY"));
    O->SetStringField(TEXT("rr_native_status"),bHDROnly?TEXT("NOT_REQUESTED"):Unmeasured.Contains(TEXT("native_rr"))?TEXT("NOT_MEASURED"):bRRNativeObserved?TEXT("OBSERVED"):TEXT("NOT_MEASURED"));
    O->SetStringField(TEXT("dxgi_status"),Unmeasured.Contains(TEXT("native_dxgi"))?TEXT("NOT_MEASURED"):bDXGIObserved?TEXT("OBSERVED"):TEXT("NOT_MEASURED"));
    O->SetStringField(TEXT("unreal_output_texture_status"),Unmeasured.Contains(TEXT("unreal_output_texture"))?TEXT("NOT_MEASURED"):bTextureObserved?TEXT("OBSERVED"):TEXT("NOT_MEASURED"));
    O->SetStringField(TEXT("display_photometry_status"),TEXT("NOT_MEASURED"));O->SetStringField(TEXT("hdr_image_quality_status"),TEXT("NOT_MEASURED"));
    TArray<TSharedPtr<FJsonValue>> MissingNames;for(const auto& Name:Unmeasured)MissingNames.Add(MakeShared<FJsonValueString>(Name));
    O->SetArrayField(TEXT("unmeasured_checks"),MissingNames);
    O->SetBoolField(TEXT("resume_only"), bResumeOnly); O->SetBoolField(TEXT("hdr_supported"), bHDRSupported);
    O->SetBoolField(TEXT("rr_supported"), bRRSupported); O->SetNumberField(TEXT("seconds"), FPlatformTime::Seconds() - Started);
    O->SetArrayField(TEXT("checks"), Checks); O->SetArrayField(TEXT("samples"), Samples);
    O->SetStringField(TEXT("limits"),TEXT("Settings, Unreal metadata/target format and optional native probes are separate observations. Missing stock-plugin RR/DXGI probes never pass as zero counters. A viewport target may be an intermediate, not a DXGI measurement. No result measures physical luminance or establishes normal HDR image quality. Render-thread target snapshots are excluded from FPS assertions."));
    FString JSON; FJsonSerializer::Serialize(O, TJsonWriterFactory<>::Create(&JSON));
    FFileHelper::SaveStringToFile(JSON, *ReportPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    UE_LOG(LogTemp, Display, TEXT("EW_RR_HDR_COMPLETE success=%d completed=%d unmeasured=%d failure=%s"),CompleteEvidence,Completed,Unmeasured.Num(),*Failure);
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
        if (!Check(bHDRSupported, TEXT("active_display_supports_hdr")) || (!bHDROnly && !Check(bRRSupported, TEXT("gpu_supports_ray_reconstruction")))) return;
        if (bResumeOnly)
        {
            const int32 SR=bHDROnly?0:2,FG=bHDROnly?0:3;
            if (!Check(G.SuperResolution == SR && G.FrameGeneration == FG && G.RayReconstruction==!bHDROnly && G.HDR &&
                G.HDRPeakNits == 1100 && G.HDRBrightness == 100 && G.MaxDisplayFPS == 144 && G.VSync,
                TEXT("preferences_loaded_by_second_process"))) return;
            Modes.Add({SR,FG,!bHDROnly,true,1100,100});
            bBaselineRRAvailable=Number(G.Evidence(),TEXT("rr_native_evaluations"),BaselineEvaluations);
            NextAction = Now + 8; Phase = 2; StageStarted = Now; return;
        }
        G.SetRayReconstruction(false); G.SetHDR(false); G.SetFrameGeneration(0);
        for (const TCHAR* Name : { TEXT("r.Shadow.Denoiser"), TEXT("r.Lumen.Reflections.ScreenSpaceReconstruction"),
            TEXT("r.Lumen.Reflections.Temporal"), TEXT("r.Lumen.Reflections.BilateralFilter"), TEXT("r.Lumen.Reflections.ExportHitT") })
            if (auto* C = IConsoleManager::Get().FindConsoleVariable(Name)) OriginalDenoisers.Add(Name, C->GetInt());
        G.SetMaxDisplayFPS(144); G.SetVSync(true);
        for(const TCHAR* Name:PolicyCVars)
            if(auto* C=IConsoleManager::Get().FindConsoleVariable(Name))OriginalHDRPolicy.Add(Name,C->GetFloat());
        if(bHDROnly)Modes={{0,0,false,false},{0,0,false,true},{0,0,false,true,600,75},
            {0,0,false,true,1100,100},{0,0,false,false,1100,100},{0,0,false,true,1100,100}};
        else Modes = { {2,0,false,false}, {2,0,true,false}, {1,3,true,false},
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
        bBaselineRRAvailable=Number(G.Evidence(),TEXT("rr_native_evaluations"),BaselineEvaluations);
        Collected = 0; NextAction = Now + 8; StageStarted = Now; Phase = 2; return;
    }
    if (Phase == 2)
    {
        const auto& M = Modes[Index];const bool TextureSample=Collected==0;
        auto E = G.Evidence(TextureSample);
        E->SetNumberField(TEXT("case"), Index); E->SetNumberField(TEXT("elapsed"), Now - Started);
        E->SetBoolField(TEXT("desktop_hud"), GI->IsDesktopHUDActive());
        E->SetNumberField(TEXT("expected_paper_white_nits"),PaperWhite(M.Brightness,M.Peak));
        E->SetBoolField(TEXT("fps_assertion_excluded_for_render_thread_snapshot"),TextureSample);
        E->SetStringField(TEXT("display_photometry_status"),TEXT("NOT_MEASURED"));
        E->SetStringField(TEXT("hdr_image_quality_status"),TEXT("NOT_MEASURED"));
        Samples.Add(MakeShared<FJsonValueObject>(E));
        FString JSON; FJsonSerializer::Serialize(E, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&JSON));
        FFileHelper::SaveStringToFile(JSON + TEXT("\n"), *StreamPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
        if (!Check(Flag(E,TEXT("sr_enabled"),M.SR!=0), TEXT("sr_matches_case")) ||
            !Check(Flag(E,TEXT("rr_preference"),M.RR) && Flag(E,TEXT("rr_configured"),M.RR), TEXT("rr_requested_and_sdk_setting_match_case"))) return;
        // Stock SDK configuration is observable; native RR activity is not.
        // IsRayReconstructionActive intentionally stays false without its probe.
        if(Flag(E,TEXT("rr_native_evidence_available")) &&
            !Check(Flag(E,TEXT("rr_enabled"),M.RR),TEXT("rr_native_activity_matches_case")))return;
        if(!bHDROnly)
        {
            if(!Flag(E,TEXT("rr_native_evidence_available")))
                Missing(TEXT("native_rr"),TEXT("Official unmodified plugins lack optional private NGX evaluation counters. Enabled state is not native evaluation proof."));
            else
            {
                double Count=0;
                if(M.RR && !Check(bBaselineRRAvailable && Number(E,TEXT("rr_native_evaluations"),Count) && Count>BaselineEvaluations+20,TEXT("native_rr_evaluations_advance")))return;
                if(!Check(Equals(E,TEXT("rr_native_failures"),0) && Equals(E,TEXT("rr_creation_fallbacks"),0),TEXT("no_native_rr_failure_or_fallback")))return;
                bRRNativeObserved=true;
            }
        }
        if (!M.RR)
            for (const auto& C : OriginalDenoisers)
                if (!Check(Equals(E,*C.Key,C.Value), TEXT("denoiser_restored_") + C.Key)) return;
        if(!CheckHDRReadbacks(M,E,TextureSample))return;
        if(!Flag(E,TEXT("dxgi_evidence_available")))
            Missing(TEXT("native_dxgi"),TEXT("Private DXGI probe absent/unavailable. Unreal viewport metadata and target format do not measure native swapchain format, color space or presents."));
        else
        {
            double Presents=0;
            if(!Check(Number(E,TEXT("dxgi_presents"),Presents) && Presents>10,TEXT("real_swapchain_is_presenting")))return;
            if(!Check(Equals(E,TEXT("dxgi_width"),2560) && Equals(E,TEXT("dxgi_height"),1440),TEXT("wqhd_swapchain")))return;
            if(M.HDR)
            {
                const bool HDR10=PQMetadata(E) && Equals(E,TEXT("dxgi_format"),24) && Equals(E,TEXT("dxgi_color_space"),12);
                const bool ScRGB=ScRGBMetadata(E) && Equals(E,TEXT("dxgi_format"),10) && Equals(E,TEXT("dxgi_color_space"),1);
                if(!Check((HDR10 || ScRGB) && Flag(E,TEXT("dxgi_color_space_explicit")),TEXT("native_hdr_swapchain_matches_viewport_metadata")))return;
            }
            else if(!Check(Equals(E,TEXT("dxgi_format"),87) && Equals(E,TEXT("dxgi_color_space"),0),TEXT("sdr_swapchain_restored")))return;
            bDXGIObserved=true;
        }
        if(!Check(Equals(E,TEXT("effective_fg"),M.FG),TEXT("frame_generation_setting_matches_case")))return;
        if(!Check(Flag(E,TEXT("vsync_preference")) && Flag(E,TEXT("vsync_effective")) &&
            Equals(E,TEXT("max_display_fps"),144) && Equals(E,TEXT("unreal_frame_limit"),144),TEXT("vsync_and_unreal_frame_limit_retained")))return;
        const bool PresentationCap=Flag(E,TEXT("presentation_cap_extension_available"));
        if(PresentationCap)
        {
            if(!Check(Equals(E,TEXT("pacing_target_fps"),144),TEXT("presentation_cap_target_retained")))return;
        }
        else Missing(TEXT("generated_inclusive_presentation_cap"),TEXT("Stock plugin lacks explicit presentation-cap extension. UE frame limit 144 is a render setting, not proof of a generated-inclusive display cap."));
        if(!bHDROnly && !TextureSample)
        {
            if(!Flag(E,TEXT("presentation_timing_available")))
                Missing(TEXT("presentation_timing"),TEXT("Streamline SDK timing unavailable; no generated frame-count or frame-rate observation."));
            else
            {
                double Rate=0;
                if(!Check(M.FG==0 || Equals(E,TEXT("frames_presented"),M.FG),TEXT("sdk_generated_frame_count_matches_case")))return;
                if(!Check(Number(E,TEXT("presented_fps"),Rate) && Rate>0,TEXT("sdk_presented_frame_rate_observed")))return;
                // Without the optional cap, generated presentations may exceed
                // the requested UE render-frame limit; that is not an audit failure.
                if((PresentationCap || M.FG==0) && !Check(Rate<=148,TEXT("observed_frame_rate_below_applicable_cap_tolerance")))return;
            }
        }
        if(!Check(Flag(E,TEXT("desktop_hud")),TEXT("desktop_hud_visible_with_output_mode")))return;
        if (++Collected < 3) { NextAction = FPlatformTime::Seconds() + 1; return; }
        if (bResumeOnly) { Finish(); return; }
        ++Index; Phase = 1; NextAction = FPlatformTime::Seconds() + .25; StageStarted = Now;
    }
}

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEWHDRReadbackFixtureTest,
    "EndlessWorld.Graphics.HDRAuditReadbackFixtures",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::CommandletContext | EAutomationTestFlags::EngineFilter)
bool FEWHDRReadbackFixtureTest::RunTest(const FString& Parameters)
{
    const auto E=MakeShared<FJsonObject>();double V=123;
    E->SetField(TEXT("rr_native_failures"),MakeShared<FJsonValueNull>());
    E->SetField(TEXT("dxgi_presents"),MakeShared<FJsonValueNull>());
    TestFalse(TEXT("Absent native failures never count as observed zero"),Equals(E,TEXT("rr_native_failures"),0));
    TestFalse(TEXT("Null presents are not a measurement"),Number(E,TEXT("dxgi_presents"),V));
    TestFalse(TEXT("Missing counters are not a measurement"),Number(E,TEXT("rr_native_evaluations"),V));
    TestFalse(TEXT("Missing availability is not true"),Flag(E,TEXT("dxgi_evidence_available")));
    E->SetNumberField(TEXT("rr_native_failures"),0);
    TestTrue(TEXT("Observed zero remains valid"),Equals(E,TEXT("rr_native_failures"),0));
    E->SetNumberField(TEXT("hdr_viewport_output_device"),int32(EDisplayOutputFormat::HDR_ACES_1000nit_ST2084));
    E->SetNumberField(TEXT("hdr_viewport_color_gamut"),int32(EDisplayColorGamut::Rec2020_D65));
    TestTrue(TEXT("PQ Rec2020 metadata accepted"),PQMetadata(E));
    E->SetNumberField(TEXT("hdr_viewport_color_gamut"),int32(EDisplayColorGamut::sRGB_D65));
    TestFalse(TEXT("PQ with wrong gamut rejected"),PQMetadata(E));
    E->SetNumberField(TEXT("hdr_viewport_output_device"),int32(EDisplayOutputFormat::HDR_ACES_2000nit_ScRGB));
    TestTrue(TEXT("ScRGB sRGB metadata accepted"),ScRGBMetadata(E));
    TestFalse(TEXT("ScRGB is not PQ"),PQMetadata(E));
    TestEqual(TEXT("Reference paper white"),PaperWhite(100,1100),203.);
    TestEqual(TEXT("Brightness scales paper white"),PaperWhite(75,600),152.25);
    TestEqual(TEXT("Paper white is capped by peak"),PaperWhite(200,400),400.);
    return true;
}
#endif
