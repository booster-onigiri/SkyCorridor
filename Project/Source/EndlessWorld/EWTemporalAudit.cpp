#include "EWTemporalAudit.h"
#include "EWGameInstance.h"
#include "EWChunkManager.h"
#include "EWCharacter.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"
#include "Engine/StaticMesh.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

namespace
{
struct FTemporalMode { const TCHAR* Name; int32 SR, FG; };
const FTemporalMode TemporalModes[] = {{TEXT("A"),3,0},{TEXT("B"),3,3},{TEXT("C"),2,0},{TEXT("D"),2,3},
    {TEXT("E"),2,2},{TEXT("F"),1,0},{TEXT("G"),0,0}};
void TemporalCVar(const TCHAR* Name, float Value)
{ if (auto* C = IConsoleManager::Get().FindConsoleVariable(Name)) C->Set(Value, ECVF_SetByConsole); }
}
AEWTemporalAudit::AEWTemporalAudit() { PrimaryActorTick.bCanEverTick = true; }
void AEWTemporalAudit::BeginPlay()
{
    Super::BeginPlay(); Started = PhaseAt = FPlatformTime::Seconds();
    Output = FPaths::ProjectSavedDir() / TEXT("Verification/temporal-") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    FParse::Value(FCommandLine::Get(), TEXT("EWTemporalOutput="), Output);
    FParse::Value(FCommandLine::Get(), TEXT("EWTemporalScene="), Scene);
    FParse::Value(FCommandLine::Get(), TEXT("EWTemporalMode="), OnlyMode);
    bGeneratedOnly = FParse::Param(FCommandLine::Get(), TEXT("EWTemporalGeneratedOnly"));
    FParse::Value(FCommandLine::Get(), TEXT("EWTemporalFastPanSeconds="), FastPanSeconds);
    FastPanSeconds = FMath::Clamp(FastPanSeconds, 10.f, 60.f);
    if (Scene != TEXT("city") && Scene != TEXT("nature")) { Finish(TEXT("unknown scene")); return; }
    if (!OnlyMode.IsEmpty())
    {
        bool Found = false;
        for (int32 I = 0; I < UE_ARRAY_COUNT(TemporalModes); ++I) if (OnlyMode == TemporalModes[I].Name) { Mode = I; Found = true; }
        if (!Found) { Finish(TEXT("unknown comparison mode")); return; }
    }
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Output), true);
    Camera = GetWorld()->SpawnActor<ACameraActor>(); Camera->GetCameraComponent()->SetFieldOfView(85);
    // Match the ordinary scene exposure; prevent auto-exposure changes between
    // comparison conditions while retaining the normal artistic post process.
    auto& Post = Camera->GetCameraComponent()->PostProcessSettings;
    Post.bOverride_AutoExposureMinBrightness = true; Post.bOverride_AutoExposureMaxBrightness = true;
    Post.AutoExposureMinBrightness = Post.AutoExposureMaxBrightness = 15;
    Camera->GetCameraComponent()->PostProcessBlendWeight = 1;
}
void AEWTemporalAudit::Event(const FString& Name)
{
    auto* G = GetGameInstance<UEWGameInstance>(); auto O = G ? G->Graphics.Evidence() : MakeShared<FJsonObject>();
    O->SetStringField(TEXT("event"), Name); O->SetStringField(TEXT("scene"), Scene);
    O->SetStringField(TEXT("condition"), TemporalModes[FMath::Clamp(Mode,0,6)].Name);
    O->SetNumberField(TEXT("elapsed_seconds"), FPlatformTime::Seconds()-Started);
    O->SetStringField(TEXT("utc"), FDateTime::UtcNow().ToIso8601());
    O->SetBoolField(TEXT("generated_only_diagnostic"), bGeneratedOnly);
    O->SetBoolField(TEXT("desktop_hud"), G && G->IsDesktopHUDActive());
    if (G) O->SetStringField(TEXT("desktop_hud_evidence"), G->DesktopHUDEvidence());
    if (Camera) { O->SetStringField(TEXT("eye"), Camera->GetActorLocation().ToString()); O->SetStringField(TEXT("view"), Camera->GetActorRotation().ToString()); }
    FString S; FJsonSerializer::Serialize(O, TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&S));
    FFileHelper::SaveStringToFile(S+TEXT("\n"), *(Output+TEXT(".jsonl")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM, &IFileManager::Get(), FILEWRITE_Append);
    FFileHelper::SaveStringToFile(S, *(Output+TEXT("-current.json")), FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    if (Name != TEXT("sample")) UE_LOG(LogTemp, Display, TEXT("EW_TEMPORAL %s %s %s"), *Scene, TemporalModes[Mode].Name, *Name);
}
void AEWTemporalAudit::Finish(const FString& Failure)
{
    if (bFinished) return; bFinished = true;
    Event(Failure.IsEmpty() ? TEXT("completed") : TEXT("failed: ")+Failure);
    TemporalCVar(TEXT("r.Streamline.DLSSG.GeneratedOnlyAudit"), 0);
    UE_LOG(LogTemp, Display, TEXT("EW_TEMPORAL_DONE %s"), Failure.IsEmpty() ? TEXT("CAPTURE_SEQUENCE_COMPLETED") : *Failure);
    FPlatformMisc::RequestExit(false);
}
void AEWTemporalAudit::Tick(float Delta)
{
    Super::Tick(Delta); if (bFinished) return;
    auto* G = GetGameInstance<UEWGameInstance>(); auto* M = G ? G->Manager.Get() : nullptr;
    auto* PC = UGameplayStatics::GetPlayerController(this,0);
    const double Now = FPlatformTime::Seconds();
    if (Now - Started > 900) { Finish(TEXT("load or foreground timeout")); return; }
    if (!M || !PC || M->IsTravelling()) return;
    if (!G->Graphics.Evidence()->GetBoolField(TEXT("foreground"))) { PhaseAt += Delta; return; }
    const EW::ChunkCoord Coord = Scene == TEXT("city") ? EW::ChunkCoord{0,0} : EW::ChunkCoord{4,0};
    if (Phase == 0)
    {
        if (M->PlayerCoord() != Coord)
        {
            auto P = M->CurrentPosition(); P.Coord = Coord;
            P.LocalPosition = FVector(6400,5150,EW::HeightAt(M->Descriptor(), Coord.X*EW::ChunkSize+6400,Coord.Y*EW::ChunkSize+5150));
            G->Visit(P); return;
        }
        if (M->ReadyCount() < 49) return;
        TArray<EW::Part> Parts;
        if (Scene == TEXT("city"))
        {
            EW::CityVolumeParts(M->Descriptor(),Coord,Parts);
            const auto* Plant = Parts.FindByPredicate([](const EW::Part& Part) { return Part.Mesh.ToString().StartsWith(TEXT("UrbanGarden_")); });
            if (!Plant) { Finish(TEXT("city rooftop garden not found")); return; }
            // Blender's (-12,-10)m tree imports with Y reflected. Aim at the
            // actual tree instead of the pavilion on the opposite side.
            Eye = M->ToRender(Coord, Plant->Transform.TransformPosition(FVector(-1200,1850,270)));
            Aim = M->ToRender(Coord, Plant->Transform.TransformPosition(FVector(-1200,1000,470)));
        }
        else
        {
            const auto Recipe = EW::GenerateChunk(M->Descriptor(),Coord);
            const auto* Plant = Recipe.Parts.FindByPredicate([](const EW::Part& Part) { return Part.Mesh == TEXT("Tree_Willow"); });
            if (!Plant) Plant = Recipe.Parts.FindByPredicate([](const EW::Part& Part) { return Part.Mesh.ToString().StartsWith(TEXT("Tree_")); });
            if (!Plant) { Finish(TEXT("forest tree not found")); return; }
            const FString Path = TEXT("/Game/EndlessWorld/Kit/SM_")+Plant->Mesh.ToString()+TEXT(".SM_")+Plant->Mesh.ToString();
            const auto* Mesh = LoadObject<UStaticMesh>(nullptr,*Path);
            if (!Mesh) { Finish(TEXT("forest tree bounds unavailable")); return; }
            const FBox Bounds = Mesh->GetBoundingBox();
            const FVector Crown(Bounds.GetCenter().X,Bounds.GetCenter().Y,Bounds.Min.Z+Bounds.GetSize().Z*.72);
            const double Radius = FMath::Max(Bounds.GetExtent().X,Bounds.GetExtent().Y);
            Aim = M->ToRender(Coord,Plant->Transform.TransformPosition(Crown));
            Eye = M->ToRender(Coord,Plant->Transform.TransformPosition(Crown+FVector(-Radius*1.8,-Radius*1.8,-Bounds.GetSize().Z*.10)));
        }
        View = (Aim-Eye).Rotation(); PC->SetViewTarget(Camera); G->SetMenu(EEWMenu::None);
        G->Graphics.SetVSync(false); G->Graphics.SetRayReconstruction(false); Phase = 1;
    }
    if (Phase == 1)
    {
        if (Mode >= UE_ARRAY_COUNT(TemporalModes)) { Mode = 6; Finish(); return; }
        const auto& Case = TemporalModes[Mode];
        G->Graphics.SetFrameGeneration(0); G->Graphics.SetSuperResolution(Case.SR); G->Graphics.SetFrameGeneration(Case.FG);
        G->Graphics.SetMaxDisplayFPS(Case.FG ? Case.FG*60 : 60);
        TemporalCVar(TEXT("r.Streamline.DLSSG.GeneratedOnlyAudit"), bGeneratedOnly && Case.FG ? 1 : 0);
        Camera->SetActorLocationAndRotation(Eye,View); Phase = 2; PhaseAt = Now; Event(TEXT("warmup")); return;
    }
    const double T = Now - PhaseAt;
    if (Now - LastSample > .25) { LastSample = Now; Event(TEXT("sample")); }
    if (Phase == 2 && T >= 8)
    {
        FScreenshotRequest::RequestScreenshot(Output+TEXT("-")+TemporalModes[Mode].Name+TEXT("-render.png"),false,false);
        TemporalCVar(TEXT("r.Streamline.UIAlphaCaptureIndex"), Mode*10);
        Phase = 3; PhaseAt = Now; Event(TEXT("static")); return;
    }
    if (Phase == 3 && T >= 3) { Phase = 4; PhaseAt = Now; Event(TEXT("slow_pan")); return; }
    if (Phase == 4)
    {
        Camera->SetActorRotation(View+FRotator(0,FMath::Sin(T*1.2)*12,0));
        if (T >= 5) { Phase = 5; PhaseAt = Now; TemporalCVar(TEXT("r.Streamline.UIAlphaCaptureIndex"), Mode*10+1); Event(TEXT("fast_pan")); } return;
    }
    if (Phase == 5)
    {
        Camera->SetActorRotation(View+FRotator(FMath::Sin(T*2)*5,FMath::Sin(T*4)*35,0));
        if (T >= FastPanSeconds) { Phase = 6; PhaseAt = Now; Event(TEXT("lateral_motion")); } return;
    }
    if (Phase == 6)
    {
        Camera->SetActorRotation(View); Camera->SetActorLocation(Eye+FRotationMatrix(View).GetUnitAxis(EAxis::Y)*FMath::Sin(T*1.7)*200);
        if (T >= 5) { G->SetMenu(EEWMenu::Settings); Phase = 7; PhaseAt = Now; Event(TEXT("menu")); } return;
    }
    if (Phase == 7 && T >= 2)
    {
        G->SetMenu(EEWMenu::None); Event(TEXT("restored"));
        if (!OnlyMode.IsEmpty()) { Finish(); return; }
        ++Mode; Phase = 1;
    }
}
