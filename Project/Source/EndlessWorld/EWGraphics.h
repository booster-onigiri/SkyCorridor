#pragma once
#include "CoreMinimal.h"

struct FEWGraphicsOption
{
    int32 Value;
    FString Label;
    bool Supported;
};

// Optional NVIDIA selections require both the build profile and live device
// support. Saved preferences survive the baseline build or a different GPU.
class FEWGraphics
{
public:
    void Initialize();
    void Apply(bool SavePreference = false);
    void SetSuperResolution(int32 Value);
    void SetFrameGeneration(int32 Value);
    void SetRayReconstruction(bool Enabled);
    void SetHDR(bool Enabled);
    void SetHDRPeakNits(int32 Value);
    void SetHDRBrightness(int32 Value);
    void SetReflex(bool Enabled);
    void SetVSync(bool Enabled);
    void SetMaxDisplayFPS(int32 Value);
    void StepMaxDisplayFPS(int32 Direction);
    void UpdateRuntime(bool MenuOpen, bool Loading, bool Foreground, bool HUDReady);
    void BeginDisplayChange();
    void Shutdown();
    void SetNative();
    TArray<FEWGraphicsOption> SuperResolutionOptions() const;
    TArray<FEWGraphicsOption> FrameGenerationOptions() const;
    FString Status() const;
    TSharedRef<class FJsonObject> Evidence() const;
    int32 SuperResolution = 0; // 0 native TSR, 1 DLAA, 2 quality, 3 balanced, 4 performance, 5 ultra performance
    int32 FrameGeneration = 0; // 0 off, 2..6 multiplier, 10 dynamic
    bool RayReconstruction = false, Reflex = true;
    bool HDR = false;
    int32 HDRPeakNits = 1000, HDRBrightness = 100;
    bool VSync = false;
    int32 MaxDisplayFPS = 60; // UE cap, or presentation target with its optional extension; zero is unlimited.
    bool CanUseVSync() const;
    FString PresentationStatus() const;
    bool CanSuperResolve() const;
    bool CanFrameGenerate() const;
    bool CanReconstructRays() const;
    bool IsRayReconstructionActive() const;
    FString RayReconstructionStatus() const;
    bool CanHDR() const;
    // Engine/window state; Evidence() identifies whether DXGI was also measured.
    bool IsHDRActive() const;
    FString HDRStatus() const;
    bool ConsumeOutputStatusChange();
    bool CanReflex() const;
private:
    bool bInitialized = false;
    bool bMenuOpen = true, bLoading = true, bForeground = false;
    bool bHUDReady = false;
    bool bEffectiveVSync = false;
    int32 EffectiveFG = 0;
    float RenderFPSLimit = 0; // Requested UE limit, or derived budget with the presentation-cap extension.
    double DisplayChangeUntil = 0;
    FIntPoint LastViewportSize = FIntPoint::ZeroValue;
    FString SuspendReason;
    bool bRRRuntimeFailure = false, bRRApplied = false;
    uint64 RRStartEvaluations = 0, RRStartFailures = 0, RRStartFallbacks = 0;
    bool bHDRScreenSupported = false, bHDRApplied = false, bHDRInitialized = false;
    bool bOutputStatusChanged = false;
    int32 AppliedHDRPeakNits = 0, AppliedHDRBrightness = 0;
    void UpdateHDROutput();
    void ApplyRayReconstruction(bool Enable);
    void ApplyPresentation();
};
