#include "EWGraphics.h"
#include "EWLocalization.h"
#if EW_WITH_NVIDIA
#include "DLSSLibrary.h"
#include "StreamlineLibraryDLSSG.h"
#include "StreamlineLibraryReflex.h"
#endif
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "UnrealClient.h"
#include "HDRHelper.h"
#include "RenderUtils.h"
#include "Widgets/SWindow.h"
#include "GenericPlatform/GenericWindow.h"
// These probes are optional local instrumentation, not part of stock plugins.
#if EW_WITH_NVIDIA && __has_include("EWRayReconstructionEvidence.h")
#define EW_WITH_RR_EVIDENCE 1
#include "EWRayReconstructionEvidence.h"
#else
#define EW_WITH_RR_EVIDENCE 0
#endif
#if EW_WITH_NVIDIA && PLATFORM_WINDOWS && __has_include("EWPresentationEvidence.h")
#define EW_WITH_PRESENTATION_EVIDENCE 1
#include "EWPresentationEvidence.h"
#else
#define EW_WITH_PRESENTATION_EVIDENCE 0
#endif

namespace
{
#if EW_WITH_NVIDIA
UDLSSMode SRMode(int32 Value)
{
    static const UDLSSMode Modes[] = {UDLSSMode::Off, UDLSSMode::DLAA, UDLSSMode::Quality,
        UDLSSMode::Balanced, UDLSSMode::Performance, UDLSSMode::UltraPerformance};
    return Modes[FMath::Clamp(Value, 0, 5)];
}
EStreamlineDLSSGMode FGMode(int32 Value)
{
    switch (Value)
    {
    case 2: return EStreamlineDLSSGMode::On2X;
    case 3: return EStreamlineDLSSGMode::On3X;
    case 4: return EStreamlineDLSSGMode::On4X;
    case 5: return EStreamlineDLSSGMode::On5X;
    case 6: return EStreamlineDLSSGMode::On6X;
    case 10: return EStreamlineDLSSGMode::OnDynamic;
    default: return EStreamlineDLSSGMode::Off;
    }
}
#endif
void SetCVar(const TCHAR* Name, float Value)
{
    if (auto* C = IConsoleManager::Get().FindConsoleVariable(Name)) C->Set(Value, ECVF_SetByConsole);
}
bool NvidiaCVarAvailable(const TCHAR* Name)
{
#if EW_WITH_NVIDIA
    return IConsoleManager::Get().FindConsoleVariable(Name) != nullptr;
#else
    return false;
#endif
}
#if EW_WITH_NVIDIA
int32 CVarInt(const TCHAR* Name)
{
    const auto* C = IConsoleManager::Get().FindConsoleVariable(Name);
    return C ? C->GetInt() : 0;
}
#endif
TSharedPtr<SWindow> GameWindow()
{
    return GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWindow() : nullptr;
}
bool CurrentDisplaySupportsHDR()
{
    const auto Window = GameWindow();
    if (!Window) return false;
    const FVector2D TopLeft = Window->GetPositionInScreen(), BottomRight = TopLeft + Window->GetSizeInScreen();
    FDisplayInformationArray Displays;
    RHIGetDisplaysInformation(Displays);
    double BestArea = 0;
    bool Supported = false;
    for (const auto& Display : Displays)
    {
        const auto& Rect = Display.DesktopCoordinates;
        const double W = FMath::Max(0., FMath::Min(BottomRight.X, double(Rect.Max.X)) - FMath::Max(TopLeft.X, double(Rect.Min.X)));
        const double H = FMath::Max(0., FMath::Min(BottomRight.Y, double(Rect.Max.Y)) - FMath::Max(TopLeft.Y, double(Rect.Min.Y)));
        if (W * H > BestArea) { BestArea = W * H; Supported = Display.bHDRSupported; }
    }
    return BestArea > 0 && Supported;
}
#if EW_WITH_PRESENTATION_EVIDENCE
FEWPresentationEvidence PresentationEvidence()
{
    const auto Window = GameWindow();
    return Window && Window->GetNativeWindow() ? EWGetPresentationEvidence(Window->GetNativeWindow()->GetOSWindowHandle()) : FEWPresentationEvidence();
}
#endif
#if EW_WITH_NVIDIA
FString SRReason(UDLSSSupport Support)
{
    switch (Support)
    {
    case UDLSSSupport::Supported: return EWL::Pick(TEXT("利用できます"), TEXT("Available"));
    case UDLSSSupport::NotSupportedIncompatibleHardware: return EWL::Pick(TEXT("GeForce RTXなどの対応GPUが必要です"), TEXT("Requires a supported GPU, such as GeForce RTX"));
    case UDLSSSupport::NotSupportedDriverOutOfDate: return EWL::Pick(TEXT("NVIDIAドライバーの更新が必要です"), TEXT("Update your NVIDIA driver"));
    case UDLSSSupport::NotSupportedOperatingSystemOutOfDate: return EWL::Pick(TEXT("Windowsの更新が必要です"), TEXT("Update Windows"));
    case UDLSSSupport::NotSupportedIncompatibleAPICaptureToolActive: return EWL::Pick(TEXT("描画キャプチャーツールとの併用に対応していません"), TEXT("Not compatible with an active graphics capture tool"));
    default: return EWL::Pick(TEXT("この環境では利用できません"), TEXT("Unavailable on this system"));
    }
}
#endif
}

bool FEWGraphics::CanSuperResolve() const
{
#if EW_WITH_NVIDIA
    return bInitialized && UDLSSLibrary::IsDLSSSupported();
#else
    return false;
#endif
}
bool FEWGraphics::CanFrameGenerate() const
{
#if EW_WITH_NVIDIA
    return bInitialized && UStreamlineLibraryDLSSG::IsDLSSGSupported();
#else
    return false;
#endif
}
bool FEWGraphics::CanReconstructRays() const
{
#if EW_WITH_NVIDIA
    return CanSuperResolve() && UDLSSLibrary::IsDLSSRRSupported() && UDLSSLibrary::IsRayTracingAvailable() &&
        IsRayTracingEnabled() && CVarInt(TEXT("r.Lumen.HardwareRayTracing")) != 0;
#else
    return false;
#endif
}
bool FEWGraphics::IsRayReconstructionActive() const
{
#if EW_WITH_RR_EVIDENCE
    if (!bInitialized || !bRRApplied || bRRRuntimeFailure || !UDLSSLibrary::IsDLSSRREnabled()) return false;
    const auto E = EWGetRayReconstructionEvidence();
    return E.Evaluations > RRStartEvaluations && FPlatformTime::Seconds() - E.LastEvaluationSeconds < 2.;
#else
    // A requested SDK setting alone does not prove a native evaluation.
    return false;
#endif
}
bool FEWGraphics::CanHDR() const { return bInitialized && IsHDRAllowed() && GRHISupportsHDROutput && bHDRScreenSupported; }
bool FEWGraphics::IsHDRActive() const
{
    const auto Window = GameWindow();
    if (!CanHDR() || !IsHDREnabled() || !Window || !Window->GetIsHDR()) return false;
#if EW_WITH_PRESENTATION_EVIDENCE
    const auto E = PresentationEvidence();
    // DXGI HDR10/PQ with R10G10B10A2, or linear scRGB with RGBA16F.
    if (E.Available) return (E.ColorSpace == 12 && E.Format == 24) || (E.ColorSpace == 1 && E.Format == 10);
#endif
    // UE engine/window state only; Evidence() separately reports DXGI coverage.
    return true;
}
bool FEWGraphics::ConsumeOutputStatusChange()
{
    const bool Changed = bOutputStatusChanged; bOutputStatusChanged = false; return Changed;
}
bool FEWGraphics::CanReflex() const
{
#if EW_WITH_NVIDIA
    return bInitialized && UStreamlineLibraryReflex::IsReflexSupported();
#else
    return false;
#endif
}
bool FEWGraphics::CanUseVSync() const
{
#if EW_WITH_NVIDIA
    if (!FrameGeneration || !CanFrameGenerate()) return true;
    if (!UStreamlineLibraryDLSSG::IsDLSSGModeSupported(FGMode(FrameGeneration)) ||
        (FrameGeneration == 10 && !NvidiaCVarAvailable(TEXT("r.Streamline.DLSSG.DynamicTargetFrameRate")))) return true;
    return FrameGeneration != 10 && UStreamlineLibraryDLSSG::GetDLSSGIsVsyncSupportAvailable();
#else
    return true;
#endif
}

void FEWGraphics::Initialize()
{
    if (IsRunningCommandlet() || bInitialized) return;
    bInitialized = true;
    GConfig->GetInt(TEXT("EndlessWorld.Graphics"), TEXT("SuperResolution"), SuperResolution, GGameUserSettingsIni);
    GConfig->GetInt(TEXT("EndlessWorld.Graphics"), TEXT("FrameGeneration"), FrameGeneration, GGameUserSettingsIni);
    GConfig->GetBool(TEXT("EndlessWorld.Graphics"), TEXT("RayReconstruction"), RayReconstruction, GGameUserSettingsIni);
    GConfig->GetBool(TEXT("EndlessWorld.Graphics"), TEXT("HDR"), HDR, GGameUserSettingsIni);
    GConfig->GetInt(TEXT("EndlessWorld.Graphics"), TEXT("HDRPeakNits"), HDRPeakNits, GGameUserSettingsIni);
    GConfig->GetInt(TEXT("EndlessWorld.Graphics"), TEXT("HDRBrightness"), HDRBrightness, GGameUserSettingsIni);
    GConfig->GetBool(TEXT("EndlessWorld.Graphics"), TEXT("Reflex"), Reflex, GGameUserSettingsIni);
    auto* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
    VSync = Settings && Settings->IsVSyncEnabled();
    GConfig->GetBool(TEXT("EndlessWorld.Graphics"), TEXT("VSync"), VSync, GGameUserSettingsIni);
    if (!GConfig->GetInt(TEXT("EndlessWorld.Graphics"), TEXT("MaxDisplayFPS"), MaxDisplayFPS, GGameUserSettingsIni))
    {
        // Keep the previous limiter setting on migration. Streamline Reflex
        // paces generated presentations too; multiplying this value changes
        // the user's previous cap.
        const float LegacyRate = Settings ? Settings->GetFrameRateLimit() : 60.f;
        MaxDisplayFPS = FMath::RoundToInt(LegacyRate);
        GConfig->SetInt(TEXT("EndlessWorld.Graphics"), TEXT("MaxDisplayFPS"), MaxDisplayFPS, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
    }
    FParse::Value(FCommandLine::Get(), TEXT("EWDLSS="), SuperResolution);
    FParse::Value(FCommandLine::Get(), TEXT("EWFrameGeneration="), FrameGeneration);
    FParse::Value(FCommandLine::Get(), TEXT("EWMaxDisplayFPS="), MaxDisplayFPS);
    int32 VSyncOverride = VSync ? 1 : 0;
    if (FParse::Value(FCommandLine::Get(), TEXT("EWVSync="), VSyncOverride)) VSync = VSyncOverride != 0;
    if (FParse::Param(FCommandLine::Get(), TEXT("EWRayReconstruction"))) RayReconstruction = true;
    int32 RROverride = RayReconstruction ? 1 : 0, HDROverride = HDR ? 1 : 0;
    if (FParse::Value(FCommandLine::Get(), TEXT("EWRayReconstruction="), RROverride)) RayReconstruction = RROverride != 0;
    if (FParse::Value(FCommandLine::Get(), TEXT("EWHDR="), HDROverride)) HDR = HDROverride != 0;
    FParse::Value(FCommandLine::Get(), TEXT("EWHDRPeakNits="), HDRPeakNits);
    FParse::Value(FCommandLine::Get(), TEXT("EWHDRBrightness="), HDRBrightness);
    HDRPeakNits = FMath::Clamp(HDRPeakNits, 400, 2000);
    HDRBrightness = FMath::Clamp(HDRBrightness, 50, 200);
    // Art comparisons keep the original native render contract. A dedicated
    // graphics audit enables the requested modes after its own warm-up.
    if (FParse::Param(FCommandLine::Get(), TEXT("EWArtStudy")))
    { SuperResolution = 0; FrameGeneration = 0; RayReconstruction = false; HDR = false; }
    SuperResolution = FMath::Clamp(SuperResolution, 0, 5);
    if (!(FrameGeneration == 0 || (FrameGeneration >= 2 && FrameGeneration <= 6) || FrameGeneration == 10)) FrameGeneration = 0;
    MaxDisplayFPS = MaxDisplayFPS <= 0 ? 0 : FMath::Clamp(MaxDisplayFPS, 30, 360);
    Apply();
}

TArray<FEWGraphicsOption> FEWGraphics::SuperResolutionOptions() const
{
    const TCHAR* Names[] = {EWL::Pick(TEXT("ネイティブ / TSR"), TEXT("Native / TSR")), EWL::Pick(TEXT("DLAA / 描画100%"), TEXT("DLAA / 100% rendering")), EWL::Pick(TEXT("DLSS クオリティ"), TEXT("DLSS Quality")),
        EWL::Pick(TEXT("DLSS バランス"), TEXT("DLSS Balanced")), EWL::Pick(TEXT("DLSS パフォーマンス"), TEXT("DLSS Performance")), EWL::Pick(TEXT("DLSS ウルトラパフォーマンス"), TEXT("DLSS Ultra Performance"))};
    TArray<FEWGraphicsOption> Result;
    for (int32 I = 0; I < 6; ++I)
    {
        bool Supported = I == 0;
#if EW_WITH_NVIDIA
        Supported |= CanSuperResolve() && UDLSSLibrary::IsDLSSModeSupported(SRMode(I));
#endif
        Result.Add({I, Names[I], Supported});
    }
    return Result;
}
TArray<FEWGraphicsOption> FEWGraphics::FrameGenerationOptions() const
{
    TArray<FEWGraphicsOption> Result;
    for (int32 I : {0, 2, 3, 4, 5, 6, 10})
    {
        bool Supported = I == 0;
#if EW_WITH_NVIDIA
        Supported |= CanFrameGenerate() && UStreamlineLibraryDLSSG::IsDLSSGModeSupported(FGMode(I)) &&
            (!VSync || (I != 10 && UStreamlineLibraryDLSSG::GetDLSSGIsVsyncSupportAvailable())) &&
            (I != 10 || NvidiaCVarAvailable(TEXT("r.Streamline.DLSSG.DynamicTargetFrameRate")));
#endif
        Result.Add({I, I == 0 ? EWL::Pick(TEXT("フレーム生成なし"), TEXT("Frame generation off")) : I == 10 ? EWL::Pick(TEXT("動的マルチフレーム生成"), TEXT("Dynamic multi-frame generation")) :
            EWL::Format(TEXT("フレーム生成 %d倍"), TEXT("Frame generation %dx"), I), Supported});
    }
    return Result;
}
void FEWGraphics::SetSuperResolution(int32 Value)
{
    for (const auto& Option : SuperResolutionOptions()) if (Option.Value == Value && Option.Supported)
    { SuperResolution = Value; bRRRuntimeFailure = false; bRRApplied = false; Apply(true); return; }
}
void FEWGraphics::SetFrameGeneration(int32 Value)
{
    for (const auto& Option : FrameGenerationOptions()) if (Option.Value == Value && Option.Supported)
    { FrameGeneration = Value; if (Value) Reflex = true; Apply(true); return; }
}
void FEWGraphics::SetRayReconstruction(bool Enabled)
{
    if (!Enabled || CanReconstructRays())
    {
        RayReconstruction = Enabled; bRRRuntimeFailure = false; bRRApplied = false;
        if (Enabled && SuperResolution == 0) SuperResolution = 2;
        Apply(true);
    }
}
void FEWGraphics::SetHDR(bool Enabled)
{
    if (!Enabled || CanHDR()) { HDR = Enabled; Apply(true); }
}
void FEWGraphics::SetHDRPeakNits(int32 Value)
{ HDRPeakNits = FMath::Clamp(Value, 400, 2000); Apply(true); }
void FEWGraphics::SetHDRBrightness(int32 Value)
{ HDRBrightness = FMath::Clamp(Value, 50, 200); Apply(true); }
void FEWGraphics::SetReflex(bool Enabled)
{ if (!Enabled || CanReflex()) { Reflex = Enabled; if (!Enabled) FrameGeneration = 0; Apply(true); } }
void FEWGraphics::SetVSync(bool Enabled)
{ if (!Enabled || CanUseVSync()) { VSync = Enabled; Apply(true); } }
void FEWGraphics::SetMaxDisplayFPS(int32 Value)
{ MaxDisplayFPS = Value <= 0 ? 0 : FMath::Clamp(Value, 30, 360); Apply(true); }
void FEWGraphics::StepMaxDisplayFPS(int32 Direction)
{
    const int32 Rates[] = {30, 40, 45, 60, 75, 90, 100, 120, 144, 165, 180, 200, 240, 300, 360};
    if (Direction > 0)
    { for (int32 Rate : Rates) if (Rate > MaxDisplayFPS) { SetMaxDisplayFPS(Rate); return; } }
    else
    { for (int32 I = UE_ARRAY_COUNT(Rates) - 1; I >= 0; --I) if (!MaxDisplayFPS || Rates[I] < MaxDisplayFPS) { SetMaxDisplayFPS(Rates[I]); return; } }
}
void FEWGraphics::UpdateRuntime(bool MenuOpen, bool Loading, bool Foreground, bool HUDReady)
{
    bMenuOpen = MenuOpen; bLoading = Loading; bForeground = Foreground; bHUDReady = HUDReady;
    if (!bInitialized) return;
    UpdateHDROutput();
#if EW_WITH_RR_EVIDENCE
    if (bRRApplied)
    {
        const auto E = EWGetRayReconstructionEvidence();
        if (E.EvaluationFailures > RRStartFailures || E.CreationFallbacks > RRStartFallbacks)
        {
            bRRRuntimeFailure = true; bRRApplied = false;
            UDLSSLibrary::EnableDLSSRR(false);
            bOutputStatusChanged = true;
            UE_LOG(LogTemp, Warning, TEXT("EW_RR native evaluation failed; restored regular denoisers and retained user preference"));
        }
    }
#endif
    if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
    {
        const FIntPoint Size = GEngine->GameViewport->Viewport->GetSizeXY();
        if (Size.X > 0 && Size.Y > 0 && Size != LastViewportSize)
        {
            if (LastViewportSize.X > 0) DisplayChangeUntil = FPlatformTime::Seconds() + 1.25;
            LastViewportSize = Size;
        }
    }
    const FString Reason = bLoading ? TEXT("loading") : bMenuOpen ? TEXT("menu") : !bForeground ? TEXT("background") :
        FPlatformTime::Seconds() < DisplayChangeUntil ? TEXT("display_change") : !bHUDReady ? TEXT("hud_unavailable") : FString();
    if (Reason != SuspendReason) { SuspendReason = Reason; ApplyPresentation(); }
}
void FEWGraphics::BeginDisplayChange()
{
    DisplayChangeUntil = FPlatformTime::Seconds() + 1.25;
    UpdateRuntime(bMenuOpen, bLoading, bForeground, bHUDReady);
}
void FEWGraphics::Shutdown()
{
    if (!bInitialized) return;
#if EW_WITH_NVIDIA
    SetCVar(TEXT("r.Streamline.DLSSG.RetainResourcesWhenOff"), 0);
    UStreamlineLibraryDLSSG::SetDLSSGMode(EStreamlineDLSSGMode::Off);
    UDLSSLibrary::EnableDLSSRR(false);
#endif
}
void FEWGraphics::SetNative()
{ SuperResolution = 0; FrameGeneration = 0; RayReconstruction = false; if (bInitialized) Apply(true); }

void FEWGraphics::Apply(bool SavePreference)
{
    if (!bInitialized) return;
    float Percentage = 100.f;
#if EW_WITH_NVIDIA
    const bool SR = SuperResolution != 0 && CanSuperResolve() && UDLSSLibrary::IsDLSSModeSupported(SRMode(SuperResolution));
    if (SR)
    {
        bool Supported = false, Fixed = false; float Min = 0, Max = 0, Sharpness = 0;
        FIntPoint Size(2560, 1440);
        if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport) Size = GEngine->GameViewport->Viewport->GetSizeXY();
        UDLSSLibrary::GetDLSSModeInformation(SRMode(SuperResolution), FVector2D(Size), Supported, Percentage, Fixed, Min, Max, Sharpness);
        if (!Supported || !FMath::IsFinite(Percentage)) Percentage = 100.f;
    }
    // DLSS 4.5 second-generation transformer: M for regular modes, L for Ultra
    // Performance. Keep the bundled model stable; no runtime model download.
    SetCVar(TEXT("r.NGX.DLSS.Preset"), SuperResolution == 5 ? 12 : 13);
    UDLSSLibrary::EnableDLSS(SR);
    // The RR model is selected independently from the SR-only M/L presets.
    SetCVar(TEXT("r.NGX.DLSSRR.Preset"), 0);
    ApplyRayReconstruction(SR && RayReconstruction && CanReconstructRays() && !bRRRuntimeFailure);
#else
    // Retain saved NVIDIA preferences for a later compatible build. The
    // effective public baseline always renders with Unreal's native TSR.
    SetCVar(TEXT("r.AntiAliasingMethod"), 4);
    ApplyRayReconstruction(false);
#endif
    if (GEngine && GEngine->GetGameUserSettings())
    {
        auto* Settings = GEngine->GetGameUserSettings();
        Settings->SetDynamicResolutionEnabled(false);
        Settings->SetResolutionScaleValueEx(Percentage);
    }
    SetCVar(TEXT("r.ScreenPercentage"), Percentage);
    SetCVar(TEXT("r.SecondaryScreenPercentage.GameViewport"), 100.f);
#if EW_WITH_NVIDIA
    // Standard SDK HUD tags are separate from the optional local recomposition
    // extension. Missing extensions must not imply the private build's behavior.
    SetCVar(TEXT("r.Streamline.ClearSceneColorAlpha"), 1);
    SetCVar(TEXT("r.Streamline.TagSceneColorWithoutHUD"), 1);
    SetCVar(TEXT("r.Streamline.TagUIColorAlpha"), 1);
    if (NvidiaCVarAvailable(TEXT("r.Streamline.DLSSG.UIRecomposition")))
        SetCVar(TEXT("r.Streamline.DLSSG.UIRecomposition"), 1);
#endif
    UpdateRuntime(bMenuOpen, bLoading, bForeground, bHUDReady);
    ApplyPresentation();
    if (SavePreference)
    {
        if (GEngine && GEngine->GetGameUserSettings()) GEngine->GetGameUserSettings()->SaveSettings();
        GConfig->SetInt(TEXT("EndlessWorld.Graphics"), TEXT("SuperResolution"), SuperResolution, GGameUserSettingsIni);
        GConfig->SetInt(TEXT("EndlessWorld.Graphics"), TEXT("FrameGeneration"), FrameGeneration, GGameUserSettingsIni);
        GConfig->SetBool(TEXT("EndlessWorld.Graphics"), TEXT("RayReconstruction"), RayReconstruction, GGameUserSettingsIni);
        GConfig->SetBool(TEXT("EndlessWorld.Graphics"), TEXT("HDR"), HDR, GGameUserSettingsIni);
        GConfig->SetInt(TEXT("EndlessWorld.Graphics"), TEXT("HDRPeakNits"), HDRPeakNits, GGameUserSettingsIni);
        GConfig->SetInt(TEXT("EndlessWorld.Graphics"), TEXT("HDRBrightness"), HDRBrightness, GGameUserSettingsIni);
        GConfig->SetBool(TEXT("EndlessWorld.Graphics"), TEXT("Reflex"), Reflex, GGameUserSettingsIni);
        GConfig->SetBool(TEXT("EndlessWorld.Graphics"), TEXT("VSync"), VSync, GGameUserSettingsIni);
        GConfig->SetInt(TEXT("EndlessWorld.Graphics"), TEXT("MaxDisplayFPS"), MaxDisplayFPS, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
    }
#if EW_WITH_NVIDIA
    UE_LOG(LogTemp, Display, TEXT("EW_GRAPHICS profile=nvidia sr_preference=%d sr_enabled=%d percentage=%.3f fg_preference=%d fg_mode=%d rr_enabled=%d reflex=%d"),
        SuperResolution, UDLSSLibrary::IsDLSSEnabled(), Percentage, FrameGeneration,
        int32(UStreamlineLibraryDLSSG::GetDLSSGMode()), UDLSSLibrary::IsDLSSRREnabled(), int32(UStreamlineLibraryReflex::GetReflexMode()));
#else
    UE_LOG(LogTemp, Display, TEXT("EW_GRAPHICS profile=baseline renderer=TSR sr_preference=%d fg_preference=%d rr_preference=%d percentage=%.3f"),
        SuperResolution, FrameGeneration, RayReconstruction, Percentage);
#endif
}

void FEWGraphics::ApplyRayReconstruction(bool Enable)
{
#if EW_WITH_NVIDIA
#if EW_WITH_RR_EVIDENCE
    if (Enable && !bRRApplied)
    {
        const auto E = EWGetRayReconstructionEvidence();
        RRStartEvaluations = E.Evaluations; RRStartFailures = E.EvaluationFailures; RRStartFallbacks = E.CreationFallbacks;
    }
#endif
    bRRApplied = Enable;
    UDLSSLibrary::EnableDLSSRR(Enable);
#else
    bRRApplied = false;
#endif
}

void FEWGraphics::UpdateHDROutput()
{
    const bool Supported = CurrentDisplaySupportsHDR();
    if (Supported != bHDRScreenSupported) { bHDRScreenSupported = Supported; bOutputStatusChanged = true; }
    auto* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
    if (!Settings) return;
    const bool Wanted = HDR && CanHDR();
    if (!bHDRInitialized || Wanted != bHDRApplied || Settings->IsHDREnabled() != Wanted ||
        (Wanted && (AppliedHDRPeakNits != HDRPeakNits || AppliedHDRBrightness != HDRBrightness)))
    {
        // Pause frame generation before the HDR swapchain is recreated.
        DisplayChangeUntil = FPlatformTime::Seconds() + 1.25;
        if (EffectiveFG) { SuspendReason = TEXT("display_change"); ApplyPresentation(); }
        SetCVar(TEXT("r.HDR.UI.CompositeMode"), 1);
        // Keep paper menus near the 80-nit SDR reference white. A brighter
        // UI clips pale button fills and helper text in SDR screen captures.
        SetCVar(TEXT("r.HDR.UI.Luminance"), 80);
        SetCVar(TEXT("r.HDR.UI.Level"), 1);
        SetCVar(TEXT("r.HDR.Display.MidLuminance"), Wanted ? 15.f * HDRBrightness / 100.f : 15.f);
        Settings->EnableHDRDisplayOutput(Wanted, HDRPeakNits);
        HDRConfigureCVars(Wanted, HDRPeakNits, false);
        bHDRApplied = Wanted; bHDRInitialized = true;
        AppliedHDRPeakNits = HDRPeakNits; AppliedHDRBrightness = HDRBrightness;
        bOutputStatusChanged = true;
        UE_LOG(LogTemp, Display, TEXT("EW_HDR preference=%d screen_supported=%d applied=%d peak=%d brightness=%d"),
            HDR, bHDRScreenSupported, Wanted, HDRPeakNits, HDRBrightness);
    }
}

FString FEWGraphics::RayReconstructionStatus() const
{
    if (!bInitialized) return EWL::Pick(TEXT("対応状況を確認しています…"), TEXT("Checking support…"));
#if EW_WITH_NVIDIA
    if (!UDLSSLibrary::IsDLSSRRSupported()) return EWL::Pick(TEXT("この環境ではレイ再構成を利用できません。"), TEXT("Ray reconstruction is unavailable on this system. ")) + SRReason(UDLSSLibrary::QueryDLSSRRSupport());
    if (!CanReconstructRays()) return EWL::Pick(TEXT("レイ再構成にはハードウェアレイトレーシングが必要です。"), TEXT("Ray reconstruction requires hardware ray tracing."));
    if (!RayReconstruction) return EWL::Pick(TEXT("反射や間接光のノイズをAIで再構成します。オンにするとDLSSまたはDLAAと併用します。"), TEXT("Uses AI to reconstruct reflections and indirect lighting. Requires DLSS or DLAA."));
    if (bRRRuntimeFailure) return EWL::Pick(TEXT("レイ再構成を開始できなかったため、通常の描画へ戻しました。設定を入れ直すと再試行します。"), TEXT("Ray reconstruction could not start. Standard rendering restored. Toggle the setting to retry."));
    if (SuperResolution == 0) return EWL::Pick(TEXT("設定はオンです。DLSSまたはDLAAを選ぶと再開します。"), TEXT("Enabled in settings. Select DLSS or DLAA to resume."));
#if !EW_WITH_RR_EVIDENCE
    return bRRApplied ? EWL::Pick(TEXT("レイ再構成を設定しました。この構成では内部処理の計測情報を取得できません。"), TEXT("Ray reconstruction configured. Internal processing measurements are unavailable in this build.")) :
        EWL::Pick(TEXT("レイ再構成を開始していません。"), TEXT("Ray reconstruction has not started."));
#else
    return IsRayReconstructionActive() ? EWL::Pick(TEXT("レイ再構成：動作中"), TEXT("Ray reconstruction: active")) : EWL::Pick(TEXT("レイ再構成の動作を確認しています…"), TEXT("Checking ray reconstruction…"));
#endif
#else
    return EWL::Pick(TEXT("この配布版にはレイ再構成が含まれていません。保存済みの設定は保持されます。"), TEXT("Ray reconstruction is not included in this build. Your saved preference is retained."));
#endif
}

FString FEWGraphics::HDRStatus() const
{
    if (!bInitialized) return EWL::Pick(TEXT("画面の対応状況を確認しています…"), TEXT("Checking display support…"));
    if (!CanHDR()) return HDR ? EWL::Pick(TEXT("設定はオンです。現在の画面ではSDRで表示しています。HDR対応画面へ戻すと再開します。"), TEXT("Enabled in settings. Using SDR on this display. HDR resumes on a supported display.")) :
        EWL::Pick(TEXT("HDR対応画面で、Windowsの「HDRを使用する」をオンにすると利用できます。"), TEXT("On an HDR display, enable Use HDR in Windows settings."));
    if (!HDR) return EWL::Pick(TEXT("HDRを利用できます。明るい水面や空の光を、より広い明るさで表示します。"), TEXT("HDR is available. Water and sky highlights can use a wider brightness range."));
#if EW_WITH_PRESENTATION_EVIDENCE
    if (PresentationEvidence().Available)
        return IsHDRActive() ? EWL::Pick(TEXT("HDR：動作中"), TEXT("HDR: active")) : EWL::Pick(TEXT("HDR表示へ切り替えています…"), TEXT("Switching to HDR…"));
#endif
    return IsHDRActive() ? EWL::Pick(TEXT("HDR：Unreal Engineで有効（画面出力の直接計測は未対応）"), TEXT("HDR: enabled in Unreal Engine (display output not directly measured)")) : EWL::Pick(TEXT("HDR表示へ切り替えています…"), TEXT("Switching to HDR…"));
}

void FEWGraphics::ApplyPresentation()
{
    if (!bInitialized) return;
#if EW_WITH_NVIDIA
    const bool FGAvailable = FrameGeneration && CanFrameGenerate() && UStreamlineLibraryDLSSG::IsDLSSGModeSupported(FGMode(FrameGeneration)) &&
        (FrameGeneration != 10 || NvidiaCVarAvailable(TEXT("r.Streamline.DLSSG.DynamicTargetFrameRate")));
#else
    const bool FGAvailable = false;
#endif
    const bool FG = FGAvailable && SuspendReason.IsEmpty();
    EffectiveFG = FG ? FrameGeneration : 0;
    bEffectiveVSync = VSync && (!FG || CanUseVSync());
    // Only the optional local extension accepts an explicit presentation cap.
    // Stock SDKs receive the UE frame limit; do not claim a measured display cap.
    const bool HasPresentationCap = NvidiaCVarAvailable(TEXT("t.Streamline.Reflex.PresentationMaxFPS"));
    const int32 Multiplier = EffectiveFG >= 2 && EffectiveFG <= 6 ? EffectiveFG : 1;
    RenderFPSLimit = MaxDisplayFPS ? float(MaxDisplayFPS) / (HasPresentationCap ? Multiplier : 1) : 0;
#if EW_WITH_NVIDIA
    SetCVar(TEXT("t.Streamline.Reflex.HandleMaxTickRate"), 1);
    SetCVar(TEXT("t.Streamline.Reflex.PresentationMaxFPS"), MaxDisplayFPS);
    SetCVar(TEXT("r.Streamline.DLSSG.DynamicTargetFrameRate"), EffectiveFG == 10 ? MaxDisplayFPS : 0);
    SetCVar(TEXT("r.Streamline.DLSSG.RetainResourcesWhenOff"), FGAvailable ? 1 : 0);
    if (CanReflex()) UStreamlineLibraryReflex::SetReflexMode(Reflex || FG ? EStreamlineReflexMode::Enabled : EStreamlineReflexMode::Off);
    UStreamlineLibraryDLSSG::SetDLSSGMode(FG ? FGMode(FrameGeneration) : EStreamlineDLSSGMode::Off);
#endif
    if (auto* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
    {
        Settings->SetVSyncEnabled(bEffectiveVSync);
        Settings->SetFrameRateLimit(MaxDisplayFPS);
    }
    SetCVar(TEXT("r.VSync"), bEffectiveVSync ? 1 : 0);
    SetCVar(TEXT("t.MaxFPS"), MaxDisplayFPS);
    UE_LOG(LogTemp, Display, TEXT("EW_PRESENTATION fg_preference=%d effective_fg=%d suspend=%s display_cap=%d render_cap=%.3f vsync_preference=%d vsync_effective=%d"),
        FrameGeneration, EffectiveFG, *SuspendReason, MaxDisplayFPS, RenderFPSLimit, VSync, bEffectiveVSync);
}

FString FEWGraphics::PresentationStatus() const
{
    if (!CanFrameGenerate()) return EWL::Pick(TEXT("ゲーム描画のFPS上限を設定します。垂直同期と併用できます。"), TEXT("Set the game rendering FPS limit. Can be used with VSync."));
    if (!NvidiaCVarAvailable(TEXT("t.Streamline.Reflex.PresentationMaxFPS")))
        return EWL::Pick(TEXT("ゲーム描画のFPS上限を設定します。フレーム生成を使う場合、生成フレームを含む表示FPSの上限は保証されません。"), TEXT("Set the game rendering FPS limit. With frame generation, the total displayed FPS limit is not guaranteed."));
    FString Result = EWL::Pick(TEXT("上限には生成したフレームも含みます。"), TEXT("This limit includes generated frames."));
    if (FrameGeneration >= 2 && FrameGeneration <= 6 && MaxDisplayFPS)
        Result += EWL::Format(TEXT(" %d倍ではゲーム描画の上限は %.1f FPS です。"), TEXT(" At %dx, the game rendering limit is %.1f FPS."), FrameGeneration, float(MaxDisplayFPS) / FrameGeneration);
    if (FrameGeneration == 10) Result += EWL::Pick(TEXT("\n動的フレーム生成と垂直同期は併用できません。"), TEXT("\nDynamic frame generation cannot be combined with VSync."));
    else if (FrameGeneration && !CanUseVSync()) Result += EWL::Pick(TEXT("\nこの環境はフレーム生成と垂直同期の併用に対応していません。"), TEXT("\nThis system does not support frame generation with VSync."));
    return Result;
}

FString FEWGraphics::Status() const
{
    if (!bInitialized) return EWL::Pick(TEXT("描画機能を確認しています…"), TEXT("Checking graphics features…"));
#if EW_WITH_NVIDIA
    FString Result = TEXT("DLSS　") + SRReason(UDLSSLibrary::QueryDLSSSupport());
    if (!CanFrameGenerate())
    {
        switch(UStreamlineLibraryDLSSG::QueryDLSSGSupport())
        {
        case EStreamlineFeatureSupport::NotSupportedHardewareSchedulingDisabled:
            Result+=EWL::Pick(TEXT("\nフレーム生成には Windows のハードウェアアクセラレータによるGPUスケジューリングが必要です。"), TEXT("\nFrame generation requires Windows hardware-accelerated GPU scheduling."));break;
        case EStreamlineFeatureSupport::NotSupportedDriverOutOfDate:
            Result+=EWL::Pick(TEXT("\nフレーム生成には NVIDIA ドライバーの更新が必要です。"), TEXT("\nFrame generation requires an NVIDIA driver update."));break;
        case EStreamlineFeatureSupport::NotSupportedByRHI:
            Result+=EWL::Pick(TEXT("\nフレーム生成は DirectX 12 で起動すると利用できます。"), TEXT("\nLaunch with DirectX 12 to use frame generation."));break;
        default:Result+=EWL::Pick(TEXT("\nフレーム生成はこの環境で利用できません。"), TEXT("\nFrame generation is unavailable on this system."));break;
        }
    }
    if (SuperResolution && !UDLSSLibrary::IsDLSSEnabled()) Result += EWL::Pick(TEXT("\n現在はネイティブ描画を使用しています。"), TEXT("\nCurrently using native rendering."));
    if (FrameGeneration == 10 && !NvidiaCVarAvailable(TEXT("r.Streamline.DLSSG.DynamicTargetFrameRate")))
        Result += EWL::Pick(TEXT("\nこの構成は動的フレーム生成の目標FPS設定に対応していません。保存設定を保持し、生成を停止しています。"), TEXT("\nThis build does not support a target FPS for dynamic frame generation. Generation is paused; your setting is retained."));
    else if (FrameGeneration && SuspendReason == TEXT("hud_unavailable"))
        Result += EWL::Pick(TEXT("\n案内表示の準備ができないため、フレーム生成を一時停止しています。"), TEXT("\nFrame generation is paused while the HUD is unavailable."));
    else if (FrameGeneration && CanFrameGenerate() && !SuspendReason.IsEmpty())
        Result += bMenuOpen ? EWL::Pick(TEXT("\nメニュー中はフレーム生成を一時停止します。探索へ戻ると再開します。"), TEXT("\nFrame generation pauses in menus and resumes during exploration.")) : EWL::Pick(TEXT("\nフレーム生成を一時停止しています。"), TEXT("\nFrame generation is paused."));
    else if (FrameGeneration && UStreamlineLibraryDLSSG::GetDLSSGMode() == EStreamlineDLSSGMode::Off)
        Result += EWL::Pick(TEXT("\n保存されたフレーム生成設定はこの環境で利用できません。"), TEXT("\nThe saved frame generation setting is unavailable on this system."));
    return Result;
#else
    return EWL::Pick(TEXT("ネイティブ / TSR 描画を使用しています。\nこの配布版にはDLSS・フレーム生成・Reflexが含まれていません。保存済みの設定は保持されます。"), TEXT("Using Native / TSR rendering.\nThis build does not include DLSS, frame generation or Reflex. Saved preferences are retained."));
#endif
}
TSharedRef<FJsonObject> FEWGraphics::Evidence() const
{
    auto O = MakeShared<FJsonObject>();
    O->SetBoolField(TEXT("nvidia_compiled"), EW_WITH_NVIDIA != 0);
    const bool HasPresentationCap = NvidiaCVarAvailable(TEXT("t.Streamline.Reflex.PresentationMaxFPS"));
    O->SetBoolField(TEXT("presentation_cap_extension_available"), HasPresentationCap);
    O->SetBoolField(TEXT("dynamic_fg_target_available"), NvidiaCVarAvailable(TEXT("r.Streamline.DLSSG.DynamicTargetFrameRate")));
    O->SetBoolField(TEXT("ui_recomposition_extension_available"), NvidiaCVarAvailable(TEXT("r.Streamline.DLSSG.UIRecomposition")));
    O->SetStringField(TEXT("fps_limit_scope"), HasPresentationCap ? TEXT("presentation_target_requested") : TEXT("unreal_frame_limit_requested"));
    O->SetBoolField(TEXT("presentation_cap_externally_verified"), false);
    O->SetBoolField(TEXT("sr_supported"), CanSuperResolve());
    O->SetNumberField(TEXT("sr_preference"), SuperResolution);
    O->SetBoolField(TEXT("fg_supported"), CanFrameGenerate()); O->SetNumberField(TEXT("fg_preference"), FrameGeneration);
#if EW_WITH_NVIDIA
    O->SetStringField(TEXT("graphics_profile"), TEXT("nvidia"));
    O->SetStringField(TEXT("plugin"), TEXT("Locally installed DLSS / Streamline; versions not measured"));
    O->SetBoolField(TEXT("sr_enabled"), UDLSSLibrary::IsDLSSEnabled());
    O->SetNumberField(TEXT("sr_support_code"), int32(UDLSSLibrary::QueryDLSSSupport()));
    O->SetNumberField(TEXT("fg_support_code"), int32(UStreamlineLibraryDLSSG::QueryDLSSGSupport()));
    O->SetNumberField(TEXT("fg_mode"), int32(UStreamlineLibraryDLSSG::GetDLSSGMode()));
    O->SetStringField(TEXT("sr_support_status"), CanSuperResolve() ? TEXT("supported") : TEXT("unavailable"));
    O->SetStringField(TEXT("fg_support_status"), CanFrameGenerate() ? TEXT("supported") : TEXT("unavailable"));
    O->SetBoolField(TEXT("fg_vsync_supported"), UStreamlineLibraryDLSSG::GetDLSSGIsVsyncSupportAvailable());
    O->SetNumberField(TEXT("rr_support_code"), int32(UDLSSLibrary::QueryDLSSRRSupport()));
    O->SetBoolField(TEXT("rr_configured"), UDLSSLibrary::IsDLSSRREnabled());
    O->SetStringField(TEXT("rr_support_status"), CanReconstructRays() ? TEXT("supported") : TEXT("unavailable"));
    O->SetNumberField(TEXT("reflex_mode"), int32(UStreamlineLibraryReflex::GetReflexMode()));
#else
    O->SetStringField(TEXT("graphics_profile"), TEXT("baseline"));
    O->SetStringField(TEXT("plugin"), TEXT("none; Unreal TSR"));
    O->SetBoolField(TEXT("sr_enabled"), false);
    O->SetNumberField(TEXT("fg_mode"), 0);
    O->SetBoolField(TEXT("fg_vsync_supported"), false);
    O->SetBoolField(TEXT("rr_configured"), false);
    O->SetNumberField(TEXT("reflex_mode"), 0);
    for (const TCHAR* Name : {TEXT("sr_support_code"), TEXT("fg_support_code"), TEXT("rr_support_code")})
        O->SetField(Name, MakeShared<FJsonValueNull>());
    for (const TCHAR* Name : {TEXT("sr_support_status"), TEXT("fg_support_status"), TEXT("rr_support_status")})
        O->SetStringField(Name, TEXT("not_compiled"));
#endif
    O->SetNumberField(TEXT("effective_fg"), EffectiveFG); O->SetStringField(TEXT("fg_suspend_reason"), SuspendReason);
    O->SetNumberField(TEXT("max_display_fps"), MaxDisplayFPS); O->SetNumberField(TEXT("render_fps_limit"), RenderFPSLimit);
    if (HasPresentationCap) O->SetNumberField(TEXT("pacing_target_fps"), MaxDisplayFPS);
    else O->SetField(TEXT("pacing_target_fps"), MakeShared<FJsonValueNull>());
    O->SetNumberField(TEXT("unreal_frame_limit"), MaxDisplayFPS);
    if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
    {
        const auto Size = GEngine->GameViewport->Viewport->GetSizeXY();
        O->SetNumberField(TEXT("viewport_width"), Size.X); O->SetNumberField(TEXT("viewport_height"), Size.Y);
    }
    O->SetBoolField(TEXT("vsync_preference"), VSync); O->SetBoolField(TEXT("vsync_effective"), bEffectiveVSync);
    O->SetBoolField(TEXT("foreground"), bForeground); O->SetBoolField(TEXT("menu_open"), bMenuOpen); O->SetBoolField(TEXT("loading"), bLoading);
    O->SetBoolField(TEXT("hud_ready"), bHUDReady);
    O->SetBoolField(TEXT("rr_preference"), RayReconstruction);
    O->SetBoolField(TEXT("rr_supported"), CanReconstructRays());
    O->SetBoolField(TEXT("rr_enabled"), IsRayReconstructionActive());
    O->SetBoolField(TEXT("rr_runtime_failure"), bRRRuntimeFailure);
    O->SetBoolField(TEXT("rr_native_evidence_available"), EW_WITH_RR_EVIDENCE != 0);
#if EW_WITH_RR_EVIDENCE
    const auto RR = EWGetRayReconstructionEvidence();
    O->SetNumberField(TEXT("rr_native_evaluations"), double(RR.Evaluations));
    O->SetNumberField(TEXT("rr_native_failures"), double(RR.EvaluationFailures));
    O->SetNumberField(TEXT("rr_creation_fallbacks"), double(RR.CreationFallbacks));
    O->SetNumberField(TEXT("rr_last_evaluation_age"), RR.LastEvaluationSeconds > 0 ? FPlatformTime::Seconds() - RR.LastEvaluationSeconds : -1);
    O->SetStringField(TEXT("rr_activity_status"), IsRayReconstructionActive() ? TEXT("observed") : TEXT("not_observed"));
#else
    for (const TCHAR* Name : {TEXT("rr_native_evaluations"), TEXT("rr_native_failures"), TEXT("rr_creation_fallbacks"), TEXT("rr_last_evaluation_age")})
        O->SetField(Name, MakeShared<FJsonValueNull>());
    O->SetStringField(TEXT("rr_activity_status"), TEXT("not_measured"));
#endif
    O->SetBoolField(TEXT("ray_tracing_enabled"), bInitialized && IsRayTracingEnabled());
    O->SetBoolField(TEXT("hdr_preference"), HDR);
    O->SetBoolField(TEXT("hdr_supported"), CanHDR());
    O->SetBoolField(TEXT("hdr_screen_supported"), bHDRScreenSupported);
    O->SetBoolField(TEXT("hdr_engine_enabled"), bInitialized && IsHDREnabled());
    O->SetBoolField(TEXT("hdr_enabled"), IsHDRActive());
    O->SetNumberField(TEXT("hdr_peak_nits"), HDRPeakNits);
    O->SetNumberField(TEXT("hdr_brightness"), HDRBrightness);
    // Missing probes are missing measurements, never successful DXGI checks.
    O->SetBoolField(TEXT("dxgi_evidence_available"), false);
    O->SetStringField(TEXT("dxgi_evidence_status"), TEXT("probe_not_compiled"));
    O->SetStringField(TEXT("hdr_validation_scope"), TEXT("unreal_engine_and_window_state_only"));
    O->SetBoolField(TEXT("hdr_output_verified"), false);
    for (const TCHAR* Name : {TEXT("dxgi_width"), TEXT("dxgi_height"), TEXT("dxgi_format"), TEXT("dxgi_color_space"),
        TEXT("dxgi_color_space_explicit"), TEXT("dxgi_color_space_changes"), TEXT("dxgi_presents"), TEXT("dxgi_generation")})
        O->SetField(Name, MakeShared<FJsonValueNull>());
#if EW_WITH_PRESENTATION_EVIDENCE
    const auto Output = PresentationEvidence();
    O->SetBoolField(TEXT("dxgi_evidence_available"), Output.Available);
    O->SetStringField(TEXT("dxgi_evidence_status"), Output.Available ? TEXT("available") : TEXT("unavailable"));
    if (Output.Available)
    {
        O->SetStringField(TEXT("hdr_validation_scope"), TEXT("unreal_state_and_dxgi_swapchain"));
        O->SetBoolField(TEXT("hdr_output_verified"), IsHDRActive());
        O->SetNumberField(TEXT("dxgi_width"), Output.Width); O->SetNumberField(TEXT("dxgi_height"), Output.Height);
        O->SetNumberField(TEXT("dxgi_format"), Output.Format); O->SetNumberField(TEXT("dxgi_color_space"), Output.ColorSpace);
        O->SetBoolField(TEXT("dxgi_color_space_explicit"), Output.ColorSpaceExplicit);
        O->SetNumberField(TEXT("dxgi_color_space_changes"), double(Output.ColorSpaceChanges));
        O->SetNumberField(TEXT("dxgi_presents"), double(Output.Presents));
        O->SetNumberField(TEXT("dxgi_generation"), double(Output.Generation));
    }
#endif
    O->SetBoolField(TEXT("presentation_timing_available"), false);
    O->SetStringField(TEXT("presentation_timing_source"), TEXT("not_measured"));
    O->SetField(TEXT("presented_fps"), MakeShared<FJsonValueNull>());
    O->SetField(TEXT("frames_presented"), MakeShared<FJsonValueNull>());
#if EW_WITH_NVIDIA
    if (CanFrameGenerate())
    {
        float Rate = 0; int32 Presented = 0; UStreamlineLibraryDLSSG::GetDLSSGFrameTiming(Rate, Presented);
        const bool Available = FMath::IsFinite(Rate) && Rate > 0 && Presented > 0;
        O->SetBoolField(TEXT("presentation_timing_available"), Available);
        if (Available)
        {
            O->SetStringField(TEXT("presentation_timing_source"), TEXT("streamline_sdk"));
            O->SetNumberField(TEXT("presented_fps"), Rate); O->SetNumberField(TEXT("frames_presented"), Presented);
        }
    }
#endif
    for (const TCHAR* Name : {TEXT("r.ScreenPercentage"), TEXT("r.NGX.DLSS.Preset"), TEXT("r.VSync"), TEXT("t.MaxFPS"),
        TEXT("r.Streamline.DLSSG.DynamicTargetFrameRate"), TEXT("r.Streamline.DLSSG.UIRecomposition"),
        TEXT("r.Streamline.TagUIColorAlpha"), TEXT("r.Streamline.ClearSceneColorAlpha"),
        TEXT("t.Streamline.Reflex.PresentationMaxFPS"), TEXT("r.DefaultBackBufferPixelFormat"), TEXT("EnableHighDPIAwareness"),
        TEXT("r.NGX.DLSSRR.Preset"), TEXT("r.NGX.DLSS.DenoiserMode"), TEXT("r.Lumen.HardwareRayTracing"),
        TEXT("r.Lumen.Reflections.ScreenSpaceReconstruction"), TEXT("r.Lumen.Reflections.Temporal"),
        TEXT("r.Lumen.Reflections.BilateralFilter"), TEXT("r.Lumen.Reflections.ExportHitT"), TEXT("r.Shadow.Denoiser"),
        TEXT("r.AllowHDR"), TEXT("r.HDR.EnableHDROutput"), TEXT("r.HDR.Display.OutputDevice"), TEXT("r.HDR.Display.ColorGamut"),
        TEXT("r.HDR.Display.MaxLuminance"), TEXT("r.HDR.Display.MidLuminance"), TEXT("r.HDR.UI.Luminance"), TEXT("r.HDR.UI.CompositeMode")})
        if (auto* C = IConsoleManager::Get().FindConsoleVariable(Name)) O->SetNumberField(Name, C->GetFloat());
    return O;
}
