#include "EWArtStudy.h"
#include "EWGameInstance.h"
#include "EWChunkManager.h"
#include "EWWorld.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "DynamicRHI.h"
#include "UnrealClient.h"
#include "EngineUtils.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/VolumetricCloudComponent.h"

namespace
{
struct StudyView { const TCHAR* Name; EW::ChunkCoord Coord, Target; FVector Eye, Aim; };
const StudyView StudyViews[] = {
    {TEXT("city-eye"), {0,0}, {0,1}, FVector(6400,4600,170), FVector(6400,6000,5000)},
    {TEXT("city-up"), {0,0}, {0,0}, FVector(6400,4600,170), FVector(6400,10000,16000)},
    {TEXT("city-upper"), {0,0}, {0,1}, FVector(6400,6500,12500), FVector(6400,6000,9000)},
    {TEXT("city-below"), {0,0}, {0,0}, FVector(6200,6600,1800), FVector(5800,7400,-12500)},
    {TEXT("city-library"), {0,0}, {0,0}, FVector(6400,6500,700), FVector(9600,9600,2200)},
    {TEXT("city-terrace"), {0,0}, {0,0}, FVector(6100,7200,8100), FVector(9600,9600,8500)},
    {TEXT("garden"), {4,0}, {5,1}, FVector(4200,3300,800), FVector(1000,1000,6500)},
    {TEXT("garden-below"), {4,0}, {5,1}, FVector(11500,6500,4000), FVector(1000,1000,-4000)},
    {TEXT("crystal"), {0,4}, {1,5}, FVector(3400,3600,1600), FVector(1000,1000,6000)}
};
}

AEWArtStudy::AEWArtStudy() { PrimaryActorTick.bCanEverTick=true; }

void AEWArtStudy::BeginPlay()
{
    Super::BeginPlay(); Started=FPlatformTime::Seconds();
    FParse::Value(FCommandLine::Get(),TEXT("EWArtOutput="),OutputPrefix);
    if (OutputPrefix.IsEmpty()) OutputPrefix=FPaths::ProjectSavedDir()/TEXT("ArtStudy/")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(OutputPrefix),true);
    Camera=GetWorld()->SpawnActor<ACameraActor>(); Camera->GetCameraComponent()->SetFieldOfView(85);
    FString SelectedView;
    if (FParse::Value(FCommandLine::Get(),TEXT("EWArtView="),SelectedView))
    {
        for (int32 I=0; I<UE_ARRAY_COUNT(StudyViews); ++I)
            if (SelectedView==StudyViews[I].Name) SelectedViewIndex=I;
        if (SelectedViewIndex<0) { Finish(TEXT("Unknown art view name.")); return; }
    }
    // Optional lighting isolation is only active in this authoring study.
    const bool NoFog=FParse::Param(FCommandLine::Get(),TEXT("EWArtNoFog"));
    const bool NoClouds=FParse::Param(FCommandLine::Get(),TEXT("EWArtNoClouds"));
    for (TActorIterator<AActor> It(GetWorld()); It; ++It)
    {
        if (NoFog)
            if (auto* Fog=It->FindComponentByClass<UExponentialHeightFogComponent>())
            { Fog->SetFogDensity(0); Fog->SetSecondFogDensity(0); Fog->SetVolumetricFog(false); }
        if (NoClouds)
            if (auto* Cloud=It->FindComponentByClass<UVolumetricCloudComponent>()) Cloud->SetVisibility(false);
    }
    float Exposure=0;
    if (FParse::Value(FCommandLine::Get(),TEXT("EWArtExposure="),Exposure))
    {
        auto& Settings=Camera->GetCameraComponent()->PostProcessSettings;
        Settings.bOverride_AutoExposureMinBrightness=true;
        Settings.bOverride_AutoExposureMaxBrightness=true;
        Settings.AutoExposureMinBrightness=Exposure; Settings.AutoExposureMaxBrightness=Exposure;
        Camera->GetCameraComponent()->PostProcessBlendWeight=1;
    }
    if (auto* S=GEngine ? GEngine->GetGameUserSettings() : nullptr)
    {
        S->SetScreenResolution(FIntPoint(2560,1440)); S->SetFullscreenMode(EWindowMode::Windowed);
        S->SetOverallScalabilityLevel(3); S->SetDynamicResolutionEnabled(false);
        S->SetResolutionScaleValueEx(100); S->SetFrameRateLimit(60);
        S->ApplyResolutionSettings(false); S->ApplyNonResolutionSettings();
    }
    if (auto* P=IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"))) P->Set(100.f,ECVF_SetByConsole);
    if (auto* P=IConsoleManager::Get().FindConsoleVariable(TEXT("r.SecondaryScreenPercentage.GameViewport"))) P->Set(100.f,ECVF_SetByConsole);
}

void AEWArtStudy::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    const double Now=FPlatformTime::Seconds();
    if (bFinished) return;
    if (Now-Started>600) { Finish(TEXT("The art study did not complete within ten minutes.")); return; }
    auto* GI=GetGameInstance<UEWGameInstance>(); auto* M=GI ? GI->Manager.Get() : nullptr;
    auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if (!M || !PC || M->IsTravelling()) return;
    const int32 ViewCount=SelectedViewIndex>=0 ? 1 : FParse::Param(FCommandLine::Get(),TEXT("EWCityStudy")) ? 6 : UE_ARRAY_COUNT(StudyViews);
    if (ViewIndex>=ViewCount) { Finish(); return; }
    const auto& V=StudyViews[SelectedViewIndex>=0 ? SelectedViewIndex : ViewIndex]; const auto W=EW::WorldDescriptor::ReferenceWorld();
    if (!bDestinationSet)
    {
        if (M->PlayerCoord()!=V.Coord)
        {
            EW::PlaceBookmark P; P.WorldCode=W.Code(); P.Coord=V.Coord;
            P.LocalPosition=FVector(6400,5150,EW::HeightAt(W,V.Coord.X*EW::ChunkSize+6400,V.Coord.Y*EW::ChunkSize+5150));
            P.Name=TEXT("景色の確認"); P.Id=EW::HashText(P.WorldCode+V.Coord.Text()).Left(32);
            P.Kind=int32(EW::RegionAt(W,V.Coord))*3;
            GI->Visit(P); bDestinationSet=true; return;
        }
        bDestinationSet=true;
    }
    if (!bCameraSet)
    {
        const double Ground=EW::HeightAt(W,V.Coord.X*EW::ChunkSize+int64(V.Eye.X),V.Coord.Y*EW::ChunkSize+int64(V.Eye.Y));
        const double TargetGround=EW::HeightAt(W,V.Target.X*EW::ChunkSize+int64(V.Aim.X),V.Target.Y*EW::ChunkSize+int64(V.Aim.Y));
        FVector Eye=M->ToRender(V.Coord,V.Eye+FVector(0,0,Ground));
        FVector Target=M->ToRender(V.Target,V.Aim+FVector(0,0,TargetGround));
        if (FCString::Strcmp(V.Name,TEXT("city-terrace"))==0)
        {
            // Inspect the actual roof garden, whose altitude follows its tower,
            // rather than aiming at an arbitrary floor of the facade.
            TArray<EW::Part> Parts; EW::CityVolumeParts(W,V.Coord,Parts);
            for (const auto& Part:Parts)
            {
                const FVector P=Part.Transform.GetTranslation();
                if (Part.Mesh.ToString().StartsWith(TEXT("UrbanGarden_")) && P.X>6400 && P.Y>6400)
                {
                    Target=M->ToRender(V.Coord,P+FVector(0,0,250));
                    Eye=M->ToRender(V.Coord,P+FVector(-2900,-3100,1700));
                    break;
                }
            }
        }
        Camera->SetActorLocationAndRotation(Eye,(Target-Eye).Rotation());
        PC->SetViewTarget(Camera); GI->SetMenu(EEWMenu::None);
        bCameraSet=true; ReadyAt=Now+18; return;
    }
    if (CapturedAt==0 && Now>=ReadyAt)
    {
        const FString Path=OutputPrefix+TEXT("-")+V.Name+TEXT(".png");
        if (IFileManager::Get().FileExists(*Path)) { Finish(TEXT("Refusing to overwrite an existing art image.")); return; }
        FScreenshotRequest::RequestScreenshot(Path,false,false);
        auto O=MakeShared<FJsonObject>(); O->SetStringField(TEXT("name"),V.Name); O->SetStringField(TEXT("file"),Path);
        O->SetStringField(TEXT("chunk"),V.Coord.Text()); O->SetStringField(TEXT("camera"),Camera->GetActorLocation().ToString());
        O->SetStringField(TEXT("rotation"),Camera->GetActorRotation().ToString()); O->SetNumberField(TEXT("elapsed_seconds"),Now-Started);
        Views.Add(MakeShared<FJsonValueObject>(O)); CapturedAt=Now; return;
    }
    if (CapturedAt>0 && Now-CapturedAt>3)
    {
        const FString Path=OutputPrefix+TEXT("-")+V.Name+TEXT(".png");
        if (!IFileManager::Get().FileExists(*Path)) { Finish(TEXT("The screenshot file was not written.")); return; }
        ++ViewIndex; CapturedAt=0; bDestinationSet=false; bCameraSet=false;
    }
}

void AEWArtStudy::Finish(const FString& Failure)
{
    if (bFinished) return; bFinished=true;
    auto O=MakeShared<FJsonObject>(); O->SetBoolField(TEXT("success"),Failure.IsEmpty());
    O->SetStringField(TEXT("scope"),TEXT("Scripted camera views of the real rendered 3D world; this is not a walking or OS input test."));
    O->SetStringField(TEXT("failure"),Failure); O->SetArrayField(TEXT("views"),Views);
    O->SetBoolField(TEXT("diagnostic_no_fog"),FParse::Param(FCommandLine::Get(),TEXT("EWArtNoFog")));
    O->SetBoolField(TEXT("diagnostic_no_clouds"),FParse::Param(FCommandLine::Get(),TEXT("EWArtNoClouds")));
    if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
    {
        const auto Size=GEngine->GameViewport->Viewport->GetSizeXY();
        O->SetNumberField(TEXT("render_width"),Size.X); O->SetNumberField(TEXT("render_height"),Size.Y);
    }
    if (auto* P=IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"))) O->SetNumberField(TEXT("screen_percentage"),P->GetFloat());
    if (auto* P=IConsoleManager::Get().FindConsoleVariable(TEXT("r.SecondaryScreenPercentage.GameViewport"))) O->SetNumberField(TEXT("secondary_screen_percentage"),P->GetFloat());
    if (auto* S=GEngine ? GEngine->GetGameUserSettings() : nullptr) O->SetBoolField(TEXT("dynamic_resolution"),S->IsDynamicResolutionEnabled());
    O->SetStringField(TEXT("rhi"),GDynamicRHI ? GDynamicRHI->GetName() : TEXT("none"));
    FString Json; FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Json));
    FFileHelper::SaveStringToFile(Json,*(OutputPrefix+TEXT(".json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    if (FParse::Param(FCommandLine::Get(),TEXT("EWExitAfterArt")))
        if (auto* PC=UGameplayStatics::GetPlayerController(this,0)) PC->ConsoleCommand(TEXT("quit"));
}
