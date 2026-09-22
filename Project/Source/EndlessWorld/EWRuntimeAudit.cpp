#include "EWRuntimeAudit.h"
#include "EWCharacter.h"
#include "EWChunkManager.h"
#include "EWGameInstance.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformMemory.h"
#include "HAL/FileManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "UnrealClient.h"
#include "TimerManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameUserSettings.h"
#include "HAL/IConsoleManager.h"
#include "DynamicRHI.h"

namespace
{
FString JsonString(const TSharedRef<FJsonObject>& O)
{
    FString S; FJsonSerializer::Serialize(O, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&S)); return S;
}
EW::ChunkCoord Neighbor(EW::ChunkCoord C, int32 Exit)
{
    if (Exit == 0) --C.X; else if (Exit == 1) ++C.X; else if (Exit == 2) --C.Y; else ++C.Y;
    return C;
}
}

AEWRuntimeAudit::AEWRuntimeAudit()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostPhysics;
}
AEWCharacter* AEWRuntimeAudit::Player() const { return Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)); }
AEWChunkManager* AEWRuntimeAudit::Manager() const
{
    auto* GI = GetGameInstance<UEWGameInstance>(); return GI ? GI->Manager.Get() : nullptr;
}
void AEWRuntimeAudit::BeginPlay()
{
    Super::BeginPlay();
    Started = LastTick = LastSample = StageStarted = ProgressTime = FPlatformTime::Seconds();
    FParse::Value(FCommandLine::Get(), TEXT("EWSoakSeconds="), SoakSeconds);
    SoakSeconds = FMath::Clamp(SoakSeconds, 0., 14400.);
    bSoakOnly = FParse::Param(FCommandLine::Get(), TEXT("EWSoakOnly"));
    ReportPath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Verification/runtime-")) + FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")) + TEXT(".json");
    FParse::Value(FCommandLine::Get(), TEXT("EWReport="), ReportPath);
    StreamPath = FPaths::ChangeExtension(ReportPath, TEXT("jsonl"));
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(ReportPath), true);
    Event(TEXT("start"), TEXT("Actual CharacterMovement; keyboard and mouse are a separate acceptance test."));
    const auto W = EW::WorldDescriptor::ReferenceWorld();
    bool Found[2][3][3] = {};
    for (int32 Y = -24; Y <= 24; ++Y)
        for (int32 X = -24; X <= 24; ++X)
            for (int32 Axis = 0; Axis < 2; ++Axis)
            {
                EW::ChunkCoord C{X, Y}, N = Neighbor(C, Axis == 0 ? 1 : 3);
                int32 A = int32(EW::RegionAt(W, C)), B = int32(EW::RegionAt(W, N));
                if (!Found[Axis][A][B]) { Cases.Add({C, Axis, A, B}); Found[Axis][A][B] = true; }
            }
    for (int64 S : {-1LL, 1LL})
    {
        EW::ChunkCoord C{S * (1LL << 38), S * (1LL << 37)};
        Cases.Add({C, 0, int32(EW::RegionAt(W, C)), int32(EW::RegionAt(W, Neighbor(C, 1)))});
    }
    if (Cases.Num() != 20) Fail(TEXT("Did not find all 18 axis/region combinations plus 2 distant cases."));
    if (auto* P = Player()) { InitialRecoveries = P->FallRecoveries; P->GetCharacterMovement()->MaxWalkSpeed = 650; }
    if (GEngine && GEngine->GetGameUserSettings())
    {
        auto* Settings=GEngine->GetGameUserSettings();
        Settings->SetScreenResolution(FIntPoint(1920,1080)); Settings->SetFullscreenMode(EWindowMode::Windowed);
        Settings->SetOverallScalabilityLevel(3); Settings->SetResolutionScaleValueEx(100);
        Settings->SetFrameRateLimit(0); Settings->SetVSyncEnabled(false);
        Settings->ApplyResolutionSettings(false); Settings->ApplyNonResolutionSettings();
    }
    if (auto* Percentage=IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"))) Percentage->Set(100.f,ECVF_SetByConsole);
    if (auto* PC = UGameplayStatics::GetPlayerController(this, 0))
    { PC->ConsoleCommand(TEXT("t.MaxFPS 0")); PC->ConsoleCommand(TEXT("r.VSync 0")); }
}
void AEWRuntimeAudit::Event(const FString& Type, const FString& Detail)
{
    auto O = MakeShared<FJsonObject>(); O->SetStringField(TEXT("event"), Type);
    O->SetStringField(TEXT("utc"),FDateTime::UtcNow().ToIso8601());
    O->SetNumberField(TEXT("elapsed"), FPlatformTime::Seconds() - Started); O->SetStringField(TEXT("detail"), Detail);
    FFileHelper::SaveStringToFile(JsonString(O) + TEXT("\n"), *StreamPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
    UE_LOG(LogTemp, Display, TEXT("EW_AUDIT_%s %s"), *Type.ToUpper(), *Detail);
}
void AEWRuntimeAudit::Fail(const FString& Detail)
{
    Failures.Add(Detail); Event(TEXT("failure"), Detail); Finish();
}
void AEWRuntimeAudit::BeginCase()
{
    auto* M = Manager(); if (!M) return;
    if (CaseIndex >= Cases.Num()) { BeginSoak(); return; }
    const auto& C = Cases[CaseIndex]; const auto W = EW::WorldDescriptor::ReferenceWorld();
    auto R = EW::GenerateChunk(W, C.C); const auto Port = R.Portals[C.Axis == 0 ? 1 : 3];
    FVector Along = C.Axis == 0 ? FVector(1, 0, 0) : FVector(0, 1, 0);
    EW::PlaceBookmark P; P.WorldCode = W.Code(); P.Coord = C.C; P.LocalPosition = Port.Position - Along * 550;
    P.Kind = C.From * 3; P.Yaw = C.Axis == 0 ? 0 : 90;
    P.Id = EW::HashText(P.WorldCode + C.C.Text()).Left(32); P.Name = TEXT("境界検証");
    Route = {{C.C, Port.Position + Along * 550}, {C.C, Port.Position - Along * 550}};
    PointIndex = 0; BestDistance = DBL_MAX; StageStarted = ProgressTime = FPlatformTime::Seconds();
    Phase = 1; M->StartWorld(W, P);
    Event(TEXT("case_travel"), FString::Printf(TEXT("%d coord=%s axis=%d region=%d->%d"), CaseIndex, *C.C.Text(), C.Axis, C.From, C.To));
}
void AEWRuntimeAudit::BeginSoak()
{
    if (SoakSeconds <= 0) { Finish(); return; }
    auto* M = Manager(); if (!M) return;
    SoakCoord = {0, 0}; const auto W = EW::WorldDescriptor::ReferenceWorld(); const auto R = EW::GenerateChunk(W, SoakCoord);
    EW::PlaceBookmark P; P.WorldCode = W.Code(); P.Coord = SoakCoord; P.LocalPosition = R.Portals[0].Position + FVector(350, 0, 0);
    P.Name = TEXT("連続探索検証"); P.Id = EW::HashText(TEXT("soak-start")).Left(32);
    M->StartWorld(W, P); Phase = 3; StageStarted = ProgressTime = FPlatformTime::Seconds();
    Event(TEXT("soak_travel"), TEXT("Start of continuous rectangle route: 10 east, 4 north, 10 west, 4 south; no further teleports."));
}
bool AEWRuntimeAudit::MakeRoute(EW::ChunkCoord C, int32 Entry, int32 Exit)
{
    const auto* M = Manager(); if (!M) return false;
    const auto R = EW::GenerateChunk(M->Descriptor(), C);
    const FString Hash = R.Digest();
    if (const auto* Previous = RevisitDigests.Find(C))
    { if (*Previous != Hash) { Fail(TEXT("Regenerated layout differs on revisit: ") + C.Text()); return false; } }
    else RevisitDigests.Add(C, Hash);
    TArray<FVector> Vertices;
    TArray<TPair<int32, int32>> Edges;
    auto Index = [&Vertices](const FVector& P)
    {
        for (int32 I = 0; I < Vertices.Num(); ++I) if (Vertices[I].Equals(P, .01)) return I;
        return Vertices.Add(P);
    };
    for (const auto& P : R.Paths) { const int32 A = Index(P.A), B = Index(P.B); Edges.Add({A, B}); }
    const int32 Start = Index(R.Portals[Entry].Position), End = Index(R.Portals[Exit].Position);
    TArray<int32> Previous; Previous.Init(-1, Vertices.Num()); Previous[Start] = Start;
    TArray<int32> Todo{Start};
    for (int32 Cursor = 0; Cursor < Todo.Num() && Previous[End] == -1; ++Cursor)
        for (const auto& Edge : Edges)
        {
            const int32 Next = Edge.Key == Todo[Cursor] ? Edge.Value : Edge.Value == Todo[Cursor] ? Edge.Key : -1;
            if (Next >= 0 && Previous[Next] == -1) { Previous[Next] = Todo[Cursor]; Todo.Add(Next); }
        }
    if (Previous[End] == -1) { Fail(TEXT("No connected path between portals: ") + C.Text()); return false; }
    TArray<int32> Reverse;
    for (int32 At = End; At != Start; At = Previous[At]) Reverse.Add(At);
    Route.Empty();
    for (int32 I = Reverse.Num() - 1; I >= 0; --I) Route.Add({C, Vertices[Reverse[I]]});
    // Go beyond the edge to prove the neighboring floor is actually active.
    const FVector Direction = Exit == 0 ? FVector(-1, 0, 0) : Exit == 1 ? FVector(1, 0, 0) : Exit == 2 ? FVector(0, -1, 0) : FVector(0, 1, 0);
    Route.Add({C, R.Portals[Exit].Position + Direction * 150});
    PointIndex = 0; BestDistance = DBL_MAX; ProgressTime = FPlatformTime::Seconds();
    Event(TEXT("route"), FString::Printf(TEXT("chunk=%s entry=%d exit=%d digest=%s"), *C.Text(), Entry, Exit, *Hash));
    return true;
}
void AEWRuntimeAudit::Tick(float Delta)
{
    Super::Tick(Delta); if (bFinished) return;
    const double Now = FPlatformTime::Seconds(), WallFrame = Now - LastTick; LastTick = Now;
    auto* M = Manager(); auto* P = Player(); if (!M || !P) return;
    if ((Phase==2 || Phase==4) && !M->IsTravelling())
    {
        const auto C=M->PlayerCoord();
        const FVector Local=M->ToLocal(C,P->GetActorLocation());
        if (bHasPreviousPosition)
        {
            const FVector Travel=FVector(double(C.X-PreviousCoord.X)*EW::ChunkSize,double(C.Y-PreviousCoord.Y)*EW::ChunkSize,0)+Local-PreviousLocal;
            if (Travel.Size2D()>FMath::Max(150.,900.*WallFrame+80.))
            {
                Fail(FString::Printf(TEXT("Logical position discontinuity: from=%s local=%s to=%s local=%s delta=%.1f frame=%.4f rebase=%d"),
                    *PreviousCoord.Text(),*PreviousLocal.ToString(),*C.Text(),*Local.ToString(),Travel.Size2D(),WallFrame,M->RebaseCount)); return;
            }
        }
        PreviousCoord=C; PreviousLocal=Local; bHasPreviousPosition=true;
    }
    else bHasPreviousPosition=false;
    if (P->FallRecoveries != InitialRecoveries) { Fail(TEXT("Character fell and invoked recovery.")); return; }
    if (M->ResidentCount() > 49 || M->JobCount() > 2 || M->QueueCount() > 8) { Fail(TEXT("Runtime residency/job bound exceeded.")); return; }
    PeakResident = FMath::Max(PeakResident, M->ResidentCount()); PeakJobs = FMath::Max(PeakJobs, M->JobCount()); PeakQueue = FMath::Max(PeakQueue, M->QueueCount());
    if (Phase == 4 && Now - SoakStarted > 20) { Frames.Add(float(WallFrame * 1000)); FrameSeconds += WallFrame; }
    if (Phase == 4 && Now - SoakStarted > 25)
    {
        const int32 Region=int32(EW::RegionAt(M->Descriptor(),M->PlayerCoord()));
        if (!(CapturedRegions & (1<<Region)))
        {
            if (CaptureAfter == 0) CaptureAfter=Now+4;
            if (Now>=CaptureAfter)
            {
                FScreenshotRequest::RequestScreenshot(FPaths::GetPath(ReportPath)/FPaths::GetBaseFilename(ReportPath)+FString::Printf(TEXT("-region-%d.png"),Region),true,false);
                CapturedRegions|=1<<Region; CaptureAfter=0;
            }
        }
    }
    if (Now - LastSample >= 1)
    {
        LastSample = Now; const auto Mem = FPlatformMemory::GetStats();
        PeakWorkingSet = FMath::Max(PeakWorkingSet, Mem.UsedPhysical); PeakVirtual = FMath::Max(PeakVirtual, Mem.UsedVirtual);
        auto O = MakeShared<FJsonObject>(); O->SetStringField(TEXT("event"), TEXT("sample"));
        O->SetNumberField(TEXT("elapsed"), Now - Started); O->SetNumberField(TEXT("phase"), Phase);
        O->SetStringField(TEXT("coord"), M->PlayerCoord().Text()); O->SetNumberField(TEXT("resident"), M->ResidentCount());
        O->SetNumberField(TEXT("jobs"), M->JobCount()); O->SetNumberField(TEXT("queue"), M->QueueCount());
        O->SetNumberField(TEXT("instances"), M->InstanceCount()); O->SetNumberField(TEXT("colliders"), M->ColliderCount());
        O->SetNumberField(TEXT("working_set"), double(Mem.UsedPhysical)); O->SetNumberField(TEXT("platform_virtual"), double(Mem.UsedVirtual));
        O->SetNumberField(TEXT("apply_ms"), M->LastApplyMilliseconds); O->SetNumberField(TEXT("rebase"), M->RebaseCount);
        O->SetNumberField(TEXT("loops"), Loops); O->SetNumberField(TEXT("chunks_walked"), WalkedChunks);
        O->SetNumberField(TEXT("measured_frames"),Frames.Num());
        O->SetNumberField(TEXT("mean_fps"),FrameSeconds>0?Frames.Num()/FrameSeconds:0);
        O->SetNumberField(TEXT("position_x"), P->GetActorLocation().X); O->SetNumberField(TEXT("position_y"), P->GetActorLocation().Y); O->SetNumberField(TEXT("position_z"), P->GetActorLocation().Z);
        O->SetBoolField(TEXT("on_ground"), P->GetCharacterMovement()->IsMovingOnGround());
        FFileHelper::SaveStringToFile(JsonString(O) + TEXT("\n"), *StreamPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
    }
    if (Phase == 0)
    {
        if (Now-Started>180) { Fail(TEXT("Initial world preparation exceeded 180 seconds.")); return; }
        if (M->IsTravelling() || Now - Started < 15) return;
        const FIntPoint Size=GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport ?
            GEngine->GameViewport->Viewport->GetSizeXY() : FIntPoint::ZeroValue;
        if (Size!=FIntPoint(1920,1080)) { Fail(FString::Printf(TEXT("Expected 1920x1080, got %dx%d; use -ForceRes for offscreen runs."),Size.X,Size.Y)); return; }
        if (bSoakOnly) BeginSoak(); else BeginCase(); return;
    }
    if (Phase == 1 || Phase == 3)
    {
        if (Now - StageStarted > 180) { Fail(TEXT("World preparation exceeded 180 seconds.")); return; }
        if (M->IsTravelling()) return;
        if (Phase == 1) Phase = 2;
        else { Phase = 4; SoakStarted = Now; if (!MakeRoute(SoakCoord, 0, 1)) return; }
        ProgressTime = StageStarted = Now; BestDistance = DBL_MAX;
    }
    if (Phase != 2 && Phase != 4) return;
    if (PointIndex < Route.Num())
    {
        const FVector Target = M->ToRender(Route[PointIndex].C, Route[PointIndex].Local);
        const FVector Direction = Target - P->GetActorLocation(); const double Distance = Direction.Size2D();
        if (Distance < 55)
        {
            ++PointIndex; BestDistance = DBL_MAX; ProgressTime = Now;
            P->SetTestMovement(FVector::ZeroVector, false);
        }
        else
        {
            if (Distance < BestDistance - 10) { BestDistance = Distance; ProgressTime = Now; }
            if (Now - ProgressTime > 12)
            {
                Fail(FString::Printf(TEXT("Movement stalled: phase=%d case=%d chunk=%s point=%d distance=%.1f pos=%s target=%s hold=%d"),
                    Phase, CaseIndex, *M->PlayerCoord().Text(), PointIndex, Distance, *P->GetActorLocation().ToString(), *Target.ToString(), P->IsStreamingHeld())); return;
            }
            P->SetTestMovement(Direction, true);
            if (auto* PC = Cast<APlayerController>(P->GetController())) PC->SetControlRotation(FRotator(-6, Direction.Rotation().Yaw, 0));
        }
        return;
    }
    P->SetTestMovement(FVector::ZeroVector, false);
    if (Phase == 2)
    {
        auto O = MakeShared<FJsonObject>(); const auto& C = Cases[CaseIndex];
        O->SetStringField(TEXT("coord"), C.C.Text()); O->SetNumberField(TEXT("axis"), C.Axis);
        O->SetNumberField(TEXT("from_region"), C.From); O->SetNumberField(TEXT("to_region"), C.To);
        O->SetBoolField(TEXT("round_trip"), true); O->SetNumberField(TEXT("walking_seconds"), Now - StageStarted);
        O->SetStringField(TEXT("method"), TEXT("CharacterMovement + collision + AddMovementInput"));
        CaseResults.Add(MakeShared<FJsonValueObject>(O)); Event(TEXT("case_pass"), FString::FromInt(CaseIndex));
        ++CaseIndex; BeginCase(); return;
    }
    const int32 Exits[] = {1, 3, 0, 2}, Lengths[] = {10, 4, 10, 4};
    const int32 PreviousExit = Exits[Segment]; SoakCoord = Neighbor(SoakCoord, PreviousExit); ++Step; ++WalkedChunks;
    LastRegions |= 1 << int32(EW::RegionAt(M->Descriptor(), SoakCoord));
    if (Step >= Lengths[Segment])
    {
        Step = 0; Segment = (Segment + 1) % 4;
        if (Segment == 0)
        {
            ++Loops; const auto Mem = FPlatformMemory::GetStats(); auto O = MakeShared<FJsonObject>();
            O->SetNumberField(TEXT("loop"), Loops); O->SetNumberField(TEXT("seconds"), Now - SoakStarted);
            O->SetNumberField(TEXT("working_set"), double(Mem.UsedPhysical)); O->SetNumberField(TEXT("platform_virtual"), double(Mem.UsedVirtual));
            O->SetNumberField(TEXT("resident"), M->ResidentCount()); LoopResults.Add(MakeShared<FJsonValueObject>(O));
            Event(TEXT("loop_complete"), JsonString(O));
        }
    }
    if (Now - SoakStarted >= SoakSeconds) { Finish(); return; }
    if (!MakeRoute(SoakCoord, PreviousExit ^ 1, Exits[Segment])) return;
}
void AEWRuntimeAudit::Finish()
{
    if (bFinished) return; bFinished = true;
    if (auto* P = Player()) { P->SetTestMovement(FVector::ZeroVector, false); P->GetCharacterMovement()->StopMovementImmediately(); }
    auto* M = Manager(); const double Now = FPlatformTime::Seconds();
    Frames.Sort(); auto O = MakeShared<FJsonObject>();
    bool SavedCurrentMatches = false;
    if (auto* GI = GetGameInstance<UEWGameInstance>(); GI && M)
    {
        EW::PlaceBookmark Expected, Saved;
        if (M->TryCurrentPosition(Expected) && GI->SaveNow() && GI->Store() && GI->Store()->LoadCurrent(Saved))
        {
            SavedCurrentMatches = Saved.WorldCode == Expected.WorldCode && Saved.Coord == Expected.Coord &&
                Saved.LocalPosition.Equals(Expected.LocalPosition,.01) && FMath::Abs(Saved.Yaw-Expected.Yaw)<.01;
            auto Position=MakeShared<FJsonObject>(); Position->SetStringField(TEXT("world"),Saved.WorldCode);
            Position->SetNumberField(TEXT("cx"),double(Saved.Coord.X)); Position->SetNumberField(TEXT("cy"),double(Saved.Coord.Y));
            Position->SetNumberField(TEXT("x"),Saved.LocalPosition.X); Position->SetNumberField(TEXT("y"),Saved.LocalPosition.Y);
            Position->SetNumberField(TEXT("z"),Saved.LocalPosition.Z); Position->SetNumberField(TEXT("yaw"),Saved.Yaw);
            Position->SetStringField(TEXT("place_code"),Saved.Code()); O->SetObjectField(TEXT("saved_current"),Position);
        }
        if (!SavedCurrentMatches && GI->Store()) O->SetStringField(TEXT("save_error"),GI->Store()->Error());
    }
    O->SetBoolField(TEXT("current_save_matches_player"),SavedCurrentMatches);
    if (!SavedCurrentMatches) Failures.Add(TEXT("Current position did not round-trip through the game's save path."));
    O->SetBoolField(TEXT("success"), Failures.IsEmpty()); O->SetStringField(TEXT("method"), TEXT("Packaged/runtime real RHI and CharacterMovement; OS keyboard/mouse not covered"));
    O->SetBoolField(TEXT("cooked_build"), FPlatformProperties::RequiresCookedData());
    const FIntPoint Size=GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport ?
        GEngine->GameViewport->Viewport->GetSizeXY() : FIntPoint::ZeroValue;
    O->SetNumberField(TEXT("render_width"),Size.X); O->SetNumberField(TEXT("render_height"),Size.Y);
    if (auto* Percentage=IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"))) O->SetNumberField(TEXT("screen_percentage"),Percentage->GetFloat());
    O->SetStringField(TEXT("rhi"),GDynamicRHI ? FString(GDynamicRHI->GetName()) : TEXT("none"));
    O->SetBoolField(TEXT("offscreen"),FParse::Param(FCommandLine::Get(),TEXT("RenderOffscreen")));
    O->SetStringField(TEXT("world"), EW::WorldDescriptor::ReferenceWorld().Code());
    O->SetNumberField(TEXT("elapsed_seconds"), Now - Started); O->SetNumberField(TEXT("requested_soak_seconds"), SoakSeconds);
    O->SetNumberField(TEXT("actual_soak_seconds"), SoakStarted > 0 ? Now - SoakStarted : 0);
    O->SetNumberField(TEXT("walked_chunks"), WalkedChunks); O->SetNumberField(TEXT("completed_loops"), Loops);
    O->SetNumberField(TEXT("regions_mask"), LastRegions); O->SetNumberField(TEXT("peak_resident"), PeakResident);
    O->SetNumberField(TEXT("peak_jobs"), PeakJobs); O->SetNumberField(TEXT("peak_queue"), PeakQueue);
    O->SetNumberField(TEXT("rebase_count"), M ? M->RebaseCount : 0); O->SetNumberField(TEXT("generated"), M ? M->Generated : 0);
    O->SetNumberField(TEXT("rebase_threshold_chunks"),M ? M->RebaseThreshold() : 0);
    O->SetNumberField(TEXT("measured_frames"), Frames.Num()); O->SetNumberField(TEXT("mean_fps"), FrameSeconds > 0 ? Frames.Num() / FrameSeconds : 0);
    O->SetNumberField(TEXT("p95_frame_ms"), Frames.IsEmpty() ? 0 : Frames[FMath::Min(Frames.Num()-1, FMath::FloorToInt(Frames.Num() * .95))]);
    O->SetNumberField(TEXT("peak_working_set"), double(PeakWorkingSet)); O->SetNumberField(TEXT("peak_platform_virtual"), double(PeakVirtual));
    O->SetStringField(TEXT("dedicated_vram"), TEXT("Measured separately with Windows GPU Process Memory counter for this PID"));
    O->SetNumberField(TEXT("fall_recoveries"), Player() ? Player()->FallRecoveries - InitialRecoveries : -1);
    O->SetArrayField(TEXT("boundary_cases"), CaseResults); O->SetArrayField(TEXT("loops"), LoopResults);
    TArray<TSharedPtr<FJsonValue>> Errors; for (const auto& E : Failures) Errors.Add(MakeShared<FJsonValueString>(E)); O->SetArrayField(TEXT("failures"), Errors);
    FString Output; FJsonSerializer::Serialize(O, TJsonWriterFactory<>::Create(&Output));
    FFileHelper::SaveStringToFile(Output, *ReportPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    Event(TEXT("complete"), Failures.IsEmpty() ? TEXT("PASS") : TEXT("FAIL"));
    FScreenshotRequest::RequestScreenshot(FPaths::ChangeExtension(ReportPath, TEXT("png")), true, false);
    if (FParse::Param(FCommandLine::Get(), TEXT("EWExitAfterAudit")))
    {
        FTimerHandle ExitTimer;
        GetWorld()->GetTimerManager().SetTimer(ExitTimer, FTimerDelegate::CreateWeakLambda(this, [this]()
        { if (auto* GI = GetGameInstance<UEWGameInstance>()) GI->Quit(); }), 2, false);
    }
}
