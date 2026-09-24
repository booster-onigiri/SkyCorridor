#include "EWGraphics.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "HAL/IConsoleManager.h"
#include "Misc/ScopeExit.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FEWHDRCalibrationTest, "EndlessWorld.Graphics.HDRCalibrationPolicy",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FEWHDRCalibrationTest::RunTest(const FString&)
{
    // No viewport, device, settings file or shader rendering: exercise the real
    // reversible CVar policy independently of physical HDR availability.
    const TCHAR* Names[] = {TEXT("r.HDR.Aces.Version"), TEXT("r.HDR.Display.OverrideOSMaxLuminance"),
        TEXT("r.HDR.PaperWhite.Mode"), TEXT("r.HDR.PaperWhite"), TEXT("r.HDR.UI.Luminance.Mode"),
        TEXT("r.HDR.UI.Luminance"), TEXT("r.HDR.UI.Level"), TEXT("r.HDR.UI.CompositeMode")};
    const float Before[] = {2, 0, 0, 177, 0, 119, .6f, 0};
    const FName TestTag(TEXT("EndlessWorldHDRCalibrationTestBaseline"));
    auto& Console = IConsoleManager::Get();
    TArray<IConsoleVariable*> Variables;
    for (const TCHAR* Name : Names)
    {
        auto* C = Console.FindConsoleVariable(Name);
        if (!TestNotNull(Name, C)) return false;
        Variables.Add(C);
    }
    FEWGraphics G;
    ON_SCOPE_EXIT
    {
        G.ApplyHDRCalibration(false);
        for (auto* C : Variables) C->Unset(ECVF_SetByTemp, TestTag);
    };
    for (int32 I = 0; I < Variables.Num(); ++I) Variables[I]->Set(Before[I], ECVF_SetByTemp, TestTag);
    G.HDR = true; G.HDRPeakNits = 600; G.HDRBrightness = 75;
    G.ApplyHDRCalibration(true);
    const float Expected[] = {2, 1, 1, 152.25f, 1, 80, 1, 1};
    for (int32 I = 0; I < Variables.Num(); ++I)
        TestTrue(FString::Printf(TEXT("HDR policy applies %s"), Names[I]), FMath::IsNearlyEqual(Variables[I]->GetFloat(), Expected[I]));
    G.HDRBrightness = 200; G.HDRPeakNits = 400;
    G.ApplyHDRCalibration(true);
    TestEqual(TEXT("Paper white does not exceed the selected peak"), Variables[3]->GetFloat(), 400.f);
    G.HDRBrightness = 50; G.HDRPeakNits = 2000;
    G.ApplyHDRCalibration(true);
    TestEqual(TEXT("Brightness changes the ACES2 input"), Variables[3]->GetFloat(), 101.5f);
    G.ApplyHDRCalibration(false);
    G.ApplyHDRCalibration(false);
    for (int32 I = 0; I < Variables.Num(); ++I)
        TestTrue(FString::Printf(TEXT("SDR restores preexisting %s"), Names[I]), FMath::IsNearlyEqual(Variables[I]->GetFloat(), Before[I]));
    TestTrue(TEXT("SDR fallback retains requested HDR"), G.HDR);
    TestEqual(TEXT("SDR fallback retains brightness"), G.HDRBrightness, 50);
    TestEqual(TEXT("SDR fallback retains peak"), G.HDRPeakNits, 2000);
    G.ApplyHDRCalibration(true);
    TestEqual(TEXT("HDR resumes with retained brightness"), Variables[3]->GetFloat(), 101.5f);
    return true;
}
#endif
