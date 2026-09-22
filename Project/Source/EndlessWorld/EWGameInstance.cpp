#include "EWGameInstance.h"
#include "EWTerminal.h"
#include "EWMusic.h"
#include "EWMemoryAudit.h"
#include "EWSky92Audit.h"
#include "EWWaterView.h"
#include "EWPoolAudit.h"
#include "EWLightingAudit.h"
#include "EWCinematicCapture95.h"
#include "EWShutdownTrace.h"
#include "EWChunkManager.h"
#include "EWLift.h"
#include "EWSkyrail.h"
#include "EWAirship.h"
#include "EWAeroYacht.h"
#include "EWAeroYachtPlan.h"
#include "EWSkyTheatrePlan.h"
#include "EWSkyrailPlan.h"
#include "EWSkyrailAudit.h"
#include "EWUpperRailAudit.h"
#include "EWMediaScreen.h"
#include "EWCinemaPlan.h"
#include "EWDayCycle.h"
#include "EWCinemaAudit.h"
#include "EWCinemaSession.h"
#include "EWSocialSession.h"
#include "EWFishing.h"
#include "EWPhotoMode.h"
#include "EWConceptRuntime.h"
#include "EWCity82Audit.h"
#include "EWQualityAudit.h"
#include "EWHotelPlan.h"
#include "EWExplorationPlan.h"
#include "EWOuterWater.h"
#include "EWExploreAudit.h"
#include "EWCascadeAudit.h"
#include "EWAeroAudit.h"
#include "EWHotelAudit.h"
#include "EWHighlightAudit.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "EWMediaAudit.h"
#include "EWResidence.h"
#include "EWInteriors.h"
#include "EWNightLighting.h"
#include "EWInteriorAudit.h"
#include "EWCharacter.h"
#include "EWRuntimeAudit.h"
#include "EWTraversalAudit.h"
#include "EWFeatureAudit.h"
#include "EWLifeAudit.h"
#include "EWPresentationAudit.h"
#include "EWTemporalAudit.h"
#include "EWResidenceAudit.h"
#include "EWWaterCityRuntimeAudit.h"
#include "EWRRHDRRuntimeAudit.h"
#include "EWArtStudy.h"
#include "EWSaveStore.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/GameUserSettings.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformMemory.h"
#include "HAL/IConsoleManager.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "Scalability.h"
#include "Engine/PostProcessVolume.h"
#include "Materials/MaterialInterface.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EWGamepadModule.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericApplication.h"
#include "Widgets/SWindow.h"

namespace
{
FString MonitorTopology(const FDisplayMetrics& Metrics)
{
    TArray<FString> Entries;
    for (const auto& M : Metrics.MonitorInfo)
        Entries.Add(FString::Printf(TEXT("%s:%d:%d,%d,%d,%d"), *M.ID, M.bIsPrimary,
            M.DisplayRect.Left, M.DisplayRect.Top, M.DisplayRect.Right, M.DisplayRect.Bottom));
    Entries.Sort();
    return FString::Join(Entries, TEXT("|"));
}
}

void UEWGameInstance::Init()
{
    Super::Init();
    // Editor commandlets can instantiate the game instance while inspecting the map.
    // They own their explicit test stores and must never open the player's save.
    if (IsRunningCommandlet()) return;
    bScriptedWorldAudit = FParse::Param(FCommandLine::Get(),TEXT("EWCinematic95")) || FParse::Param(FCommandLine::Get(),TEXT("EWSky92Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWLighting91Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWPool90Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWMemory89Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWAero87Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWCascade86Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWExplore85Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWHighlight84Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWHotel83Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWQuality83Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWCity82Audit")) || FParse::Param(FCommandLine::Get(), TEXT("EWLifeAudit")) || FParse::Param(FCommandLine::Get(), TEXT("EWSkyrailAudit")) || FParse::Param(FCommandLine::Get(), TEXT("EWTraversalAudit")) ||
        FParse::Param(FCommandLine::Get(), TEXT("EWRuntimeAudit")) ||
        FParse::Param(FCommandLine::Get(), TEXT("EWArtStudy")) ||
        FParse::Param(FCommandLine::Get(), TEXT("EWGraphicsAudit")) ||
        FParse::Param(FCommandLine::Get(), TEXT("EWPresentationAudit")) ||
        FParse::Param(FCommandLine::Get(), TEXT("EWTemporalAudit")) ||
        FParse::Param(FCommandLine::Get(), TEXT("EWResidenceAudit")) || FParse::Param(FCommandLine::Get(),TEXT("EWInteriorAudit")) ||
        FParse::Param(FCommandLine::Get(), TEXT("EWWaterCityRuntimeAudit")) ||
        FParse::Param(FCommandLine::Get(), TEXT("EWRRHDRRuntimeAudit")) || (FParse::Param(FCommandLine::Get(),TEXT("EWMediaAudit")) || (FParse::Param(FCommandLine::Get(),TEXT("EWCinemaAudit")) || FParse::Param(FCommandLine::Get(),TEXT("EWOnlineAudit"))));
    FString Root = FPaths::Combine(FPlatformProcess::UserSettingsDir(), TEXT("EndlessWorld"));
    const bool ExplicitRoot=FParse::Value(FCommandLine::Get(), TEXT("EWDataDir="), Root);
    if ((FParse::Param(FCommandLine::Get(),TEXT("EWCinematic95")) || FParse::Param(FCommandLine::Get(),TEXT("EWSky92Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWLighting91Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWPool90Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWMemory89Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWAero87Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWCascade86Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWExplore85Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWHighlight84Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWHotel83Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWQuality83Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWCity82Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWLifeAudit")) || FParse::Param(FCommandLine::Get(),TEXT("EWSkyrailAudit")) || FParse::Param(FCommandLine::Get(),TEXT("EWRuntimeAudit")) || FParse::Param(FCommandLine::Get(),TEXT("EWArtStudy")) ||
        FParse::Param(FCommandLine::Get(),TEXT("EWTraversalAudit")) || FParse::Param(FCommandLine::Get(),TEXT("EWGamepadAudit")) ||
        FParse::Param(FCommandLine::Get(),TEXT("EWGraphicsAudit")) || FParse::Param(FCommandLine::Get(),TEXT("EWPresentationAudit")) ||
        FParse::Param(FCommandLine::Get(),TEXT("EWTemporalAudit")) ||
        FParse::Param(FCommandLine::Get(),TEXT("EWResidenceAudit")) || FParse::Param(FCommandLine::Get(),TEXT("EWInteriorAudit")) ||
        FParse::Param(FCommandLine::Get(),TEXT("EWWaterCityRuntimeAudit")) ||
        FParse::Param(FCommandLine::Get(),TEXT("EWRRHDRRuntimeAudit")) || (FParse::Param(FCommandLine::Get(),TEXT("EWMediaAudit")) || (FParse::Param(FCommandLine::Get(),TEXT("EWCinemaAudit")) || FParse::Param(FCommandLine::Get(),TEXT("EWOnlineAudit"))))) && !ExplicitRoot)
        Root=FPaths::ProjectSavedDir()/TEXT("Verification")/FGuid::NewGuid().ToString(EGuidFormats::Digits)/TEXT("Data");
    FParse::Value(FCommandLine::Get(),TEXT("EWInputEvidence="),InputEvidencePath);
    Saves = MakeUnique<FEWSaveStore>();
    if (!Saves->Open(Root)) Notify(Saves->Error(), 86400);
    else if (!Saves->Notice().IsEmpty()) Notify(Saves->Notice(), 30);
    GConfig->GetBool(TEXT("EndlessWorld"),TEXT("SoftStyle"),bSoftStyle,GGameUserSettingsIni);
    GConfig->GetFloat(TEXT("EndlessWorld.Input"), TEXT("PadLookSpeed"), PadLookSpeed, GGameUserSettingsIni);
    GConfig->GetBool(TEXT("EndlessWorld.Input"), TEXT("InvertPadY"), bInvertPadY, GGameUserSettingsIni);
    PadLookSpeed = FMath::Clamp(PadLookSpeed, 60.f, 180.f);
    if (GEngine && GEngine->GetGameUserSettings()) bHighQuality=GEngine->GetGameUserSettings()->GetOverallScalabilityLevel()>=3;
    TickerHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UEWGameInstance::Tick), 1);
    WorldTearDownHandle = FWorldDelegates::OnWorldBeginTearDown.AddUObject(this, &UEWGameInstance::BeforeWorldTearDown);
}

void UEWGameInstance::Shutdown()
{
    EWShutdownTrace(TEXT("GameInstance.Shutdown.begin"));
    NativeHUD.Shutdown();
    Graphics.Shutdown();
    if (TickerHandle.IsValid()) FTSTicker::GetCoreTicker().RemoveTicker(TickerHandle);
    if (WorldTearDownHandle.IsValid()) FWorldDelegates::OnWorldBeginTearDown.Remove(WorldTearDownHandle);
    if (DisplayMetricsHandle.IsValid() && FSlateApplication::IsInitialized())
        if (auto App = FSlateApplication::Get().GetPlatformApplication()) App->OnDisplayMetricsChanged().Remove(DisplayMetricsHandle);
    CacheCurrentPosition(); bWorldEnding = true;
    if (!bDiscardExit) SaveNow();
    Saves.Reset(); Super::Shutdown();
    EWShutdownTrace(TEXT("GameInstance.Shutdown.end"));
}

EW::PlaceBookmark UEWGameInstance::StartPlace(const EW::WorldDescriptor& W) const
{
    EW::PlaceBookmark P; P.WorldCode = W.Code(); P.Coord = {0, 0};
    P.LocalPosition = FVector(6400, 5150, EW::HeightAt(W, 6400, 5150));
    P.Yaw = 90; P.Kind = 0; P.Name = TEXT("旅のはじまり");
    P.Id = EW::HashText(W.Code() + TEXT("|arrival")).Left(32);
    return P;
}

void UEWGameInstance::AttachWorld()
{
    if (Manager || !GetWorld()) return;
    bWorldEnding = false;
    Manager = GetWorld()->SpawnActor<AEWChunkManager>();
    const auto W = EW::WorldDescriptor::ReferenceWorld();
    Manager->StartWorld(W, StartPlace(W));
    MediaScreen=GetWorld()->SpawnActor<AEWMediaScreen>();
    ActiveMediaScreen=MediaScreen;
    CinemaScreen=GetWorld()->SpawnActorDeferred<AEWMediaScreen>(AEWMediaScreen::StaticClass(),FTransform::Identity);
    CinemaScreen->bCinema=true;CinemaScreen->FinishSpawning(FTransform::Identity);
    SkyTheatre=GetWorld()->SpawnActorDeferred<AEWMediaScreen>(AEWMediaScreen::StaticClass(),FTransform::Identity);
    SkyTheatre->bSkyTheatre=true;SkyTheatre->FinishSpawning(FTransform::Identity);
    Airships.Reset();Yachts.Reset();
    for(int I=0;I<3;++I)
    {
        auto* A=GetWorld()->SpawnActorDeferred<AEWAirship>(AEWAirship::StaticClass(),FTransform::Identity);
        A->FleetIndex=I;A->FinishSpawning(FTransform::Identity);Airships.Add(A);
    }
    Airship=Airships[0];
    for(int I=0;I<EWAeroYachtPlan::Ships;++I)
    {
        auto* A=GetWorld()->SpawnActorDeferred<AEWAeroYacht>(AEWAeroYacht::StaticClass(),FTransform::Identity);
        A->Index=I;A->FinishSpawning(FTransform::Identity);Yachts.Add(A);
    }
    Skyport=GetWorld()->SpawnActor<AEWAeroPort>();
    DayCycle=GetWorld()->SpawnActor<AEWDayCycle>();
    Skyrail=GetWorld()->SpawnActor<AEWSkyrail>();
    SkyrailUpper=GetWorld()->SpawnActorDeferred<AEWSkyrail>(AEWSkyrail::StaticClass(),FTransform::Identity);
    SkyrailUpper->Line=1;SkyrailUpper->FinishSpawning(FTransform::Identity);
    CinemaSession=GetWorld()->SpawnActor<AEWCinemaSession>();
    SocialSession=GetWorld()->SpawnActor<AEWSocialSession>();
    Fishing=GetWorld()->SpawnActor<AEWFishing>();
    PhotoMode=GetWorld()->SpawnActor<AEWPhotoMode>();
    Concepts=GetWorld()->SpawnActor<AEWConceptRuntime>();
    Terminal=GetWorld()->SpawnActor<AEWTerminal>();
    Music=GetWorld()->SpawnActor<AEWMusic>();
    WaterView=GetWorld()->SpawnActor<AEWWaterView>();
    // A resume audit must preserve the existing current place while the
    // preview world loads. It starts the saved session after loading finishes.
    if (!FParse::Param(FCommandLine::Get(), TEXT("EWTraversalResumeOnly")) &&
        !FParse::Param(FCommandLine::Get(), TEXT("EWWaterCityResumeOnly")) &&
        (FParse::Param(FCommandLine::Get(), TEXT("EWPlay")) || FParse::Param(FCommandLine::Get(), TEXT("EWRuntimeAudit")) || FParse::Param(FCommandLine::Get(),TEXT("EWArtStudy")) || FParse::Param(FCommandLine::Get(),TEXT("EWTraversalAudit"))))
    { bSessionStarted = true; CurrentMenu = EEWMenu::None; }
    FString Code;
    if (FParse::Value(FCommandLine::Get(), TEXT("EWWorld="), Code))
    {
        EW::WorldDescriptor Given; FString Error;
        if (EW::WorldDescriptor::Parse(Code, Given, Error))
        { bSessionStarted = true; CurrentMenu = EEWMenu::None; Manager->StartWorld(Given, StartPlace(Given)); }
        else Notify(Error, 30);
    }
    RefreshUI();
    SetSoftStyle(bSoftStyle);
    int32 NativeResolutionVersion=0;
    GConfig->GetInt(TEXT("EndlessWorld"),TEXT("NativeResolutionVersion"),NativeResolutionVersion,GGameUserSettingsIni);
    if (!FParse::Param(FCommandLine::Get(),TEXT("ForceRes")) &&
        !FParse::Param(FCommandLine::Get(),TEXT("EWRuntimeAudit")) &&
        !FParse::Param(FCommandLine::Get(),TEXT("EWTraversalAudit")) &&
        (NativeResolutionVersion<1 || FParse::Param(FCommandLine::Get(),TEXT("EWNativeResolution"))))
        SetNativeResolution();
    Graphics.Initialize();
    if(!GUsingNullRHI)
    {
        GetWorld()->SpawnActor<AEWInteriorLighting>();
        GetWorld()->SpawnActor<AEWNightLighting>();
    }
    UpdatePresentationState();
    if (!bScriptedWorldAudit && !FParse::Param(FCommandLine::Get(), TEXT("ForceRes")) && FSlateApplication::IsInitialized())
    {
        FDisplayMetrics Metrics; FDisplayMetrics::RebuildDisplayMetrics(Metrics); DisplayTopology = MonitorTopology(Metrics);
        if (auto App = FSlateApplication::Get().GetPlatformApplication())
            DisplayMetricsHandle = App->OnDisplayMetricsChanged().AddUObject(this, &UEWGameInstance::DisplayMetricsChanged);
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("EWCinematic95")))GetWorld()->SpawnActor<AEWCinematicCapture95>();
    if(FParse::Param(FCommandLine::Get(),TEXT("EWLighting91Audit")))GetWorld()->SpawnActor<AEWLightingAudit>();
    if(FParse::Param(FCommandLine::Get(),TEXT("EWPool90Audit")))GetWorld()->SpawnActor<AEWPoolAudit>();
    if(FParse::Param(FCommandLine::Get(),TEXT("EWMemory89Audit")))GetWorld()->SpawnActor<AEWMemoryAudit>();
    if(FParse::Param(FCommandLine::Get(),TEXT("EWSky92Audit")))GetWorld()->SpawnActor<AEWSky92Audit>();
    if (FParse::Param(FCommandLine::Get(), TEXT("EWRuntimeAudit"))) GetWorld()->SpawnActor<AEWRuntimeAudit>();
    if(FParse::Param(FCommandLine::Get(),TEXT("EWHotel83Audit")))GetWorld()->SpawnActor<AEWHotelAudit>();
    if(FParse::Param(FCommandLine::Get(),TEXT("EWHighlight84Audit")))GetWorld()->SpawnActor<AEWHighlightAudit>();
    if(FParse::Param(FCommandLine::Get(),TEXT("EWExplore85Audit")))GetWorld()->SpawnActor<AEWExploreAudit>();
    if(FParse::Param(FCommandLine::Get(),TEXT("EWCascade86Audit")))GetWorld()->SpawnActor<AEWCascadeAudit>();
    if(FParse::Param(FCommandLine::Get(),TEXT("EWAero87Audit")))GetWorld()->SpawnActor<AEWAeroAudit>();
    if(FParse::Param(FCommandLine::Get(),TEXT("EWQuality83Audit")))GetWorld()->SpawnActor<AEWQualityAudit>();
    if(FParse::Param(FCommandLine::Get(),TEXT("EWCity82Audit")))GetWorld()->SpawnActor<AEWCity82Audit>();
    if (FParse::Param(FCommandLine::Get(), TEXT("EWLifeAudit"))) GetWorld()->SpawnActor<AEWLifeAudit>();
    if (FParse::Param(FCommandLine::Get(), TEXT("EWArtStudy"))) GetWorld()->SpawnActor<AEWArtStudy>();
    if (FParse::Param(FCommandLine::Get(), TEXT("EWTraversalAudit"))) GetWorld()->SpawnActor<AEWTraversalAudit>();
    if(FParse::Param(FCommandLine::Get(),TEXT("EWGamepadAudit")) || FParse::Param(FCommandLine::Get(),TEXT("EWGraphicsAudit")))GetWorld()->SpawnActor<AEWFeatureAudit>();
    if(FParse::Param(FCommandLine::Get(),TEXT("EWPresentationAudit")))GetWorld()->SpawnActor<AEWPresentationAudit>();
    if(FParse::Param(FCommandLine::Get(),TEXT("EWTemporalAudit")))GetWorld()->SpawnActor<AEWTemporalAudit>();
    if(FParse::Param(FCommandLine::Get(),TEXT("EWResidenceAudit")))GetWorld()->SpawnActor<AEWResidenceAudit>();
    if(FParse::Param(FCommandLine::Get(),TEXT("EWInteriorAudit")))GetWorld()->SpawnActor<AEWInteriorAudit>();
    if(FParse::Param(FCommandLine::Get(),TEXT("EWWaterCityRuntimeAudit")))GetWorld()->SpawnActor<AEWWaterCityRuntimeAudit>();
    if(FParse::Param(FCommandLine::Get(),TEXT("EWRRHDRRuntimeAudit")))GetWorld()->SpawnActor<AEWRRHDRRuntimeAudit>();
    if(FParse::Param(FCommandLine::Get(),TEXT("EWMediaAudit")))GetWorld()->SpawnActor<AEWMediaAudit>();
    if(FParse::Param(FCommandLine::Get(),TEXT("EWCinemaAudit")))GetWorld()->SpawnActor<AEWCinemaAudit>();
    if(FParse::Param(FCommandLine::Get(),TEXT("EWSkyrailAudit")))
    {if(FParse::Param(FCommandLine::Get(),TEXT("EWUpper88")))GetWorld()->SpawnActor<AEWUpperRailAudit>();else GetWorld()->SpawnActor<AEWSkyrailAudit>();}
}

void UEWGameInstance::NoteInput(bool Gamepad, bool Sony)
{
    bUsingGamepad = Gamepad;
    if (Gamepad) { bSonyGamepad = Sony; ++GamepadEvents; }
    if (auto* PC = UGameplayStatics::GetPlayerController(this, 0)) PC->bShowMouseCursor = CurrentMenu != EEWMenu::None && !Gamepad;
}
FString UEWGameInstance::InputHint(const FString& Keyboard, const FString& Xbox, const FString& Sony) const
{ return bUsingGamepad ? (bSonyGamepad ? Sony : Xbox) : Keyboard; }
void UEWGameInstance::SetPadLookSpeed(float Value)
{
    PadLookSpeed = FMath::Clamp(Value, 60.f, 180.f);
    GConfig->SetFloat(TEXT("EndlessWorld.Input"), TEXT("PadLookSpeed"), PadLookSpeed, GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni); RefreshUI();
}
void UEWGameInstance::TogglePadInvertY()
{
    bInvertPadY = !bInvertPadY;
    GConfig->SetBool(TEXT("EndlessWorld.Input"), TEXT("InvertPadY"), bInvertPadY, GGameUserSettingsIni);
    GConfig->Flush(false, GGameUserSettingsIni); RefreshUI();
}
void UEWGameInstance::BackFromMenu()
{
    if (CurrentMenu == EEWMenu::Main)
    { if (bSessionStarted) SetMenu(EEWMenu::None); }
    else if ((CurrentMenu == EEWMenu::Journal || CurrentMenu == EEWMenu::Lift || CurrentMenu == EEWMenu::Monitor || CurrentMenu == EEWMenu::FishJournal || CurrentMenu == EEWMenu::Catch || CurrentMenu == EEWMenu::Photo) && bSessionStarted) SetMenu(EEWMenu::None);
    else SetMenu(EEWMenu::Main);
}
void UEWGameInstance::Interact()
{
    for(auto* Screen:{MediaScreen.Get(),CinemaScreen.Get(),SkyTheatre.Get()})
        if(Screen && Screen->Nearby()){Screen->OpenControls();return;}
    if(Terminal && Terminal->RecordNearby())return;
    if(Concepts && Concepts->Interact())return;
    if(Fishing && Fishing->Interact())return;
    auto* Player = Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    if(SkyrailUpper && SkyrailUpper->Interact(Player))return;
    if(Skyrail && Skyrail->Interact(Player))return;
    if (Player && Player->IsSeated()) { Player->LeaveSeat(); return; }
    if(EWExplorationPlan::Sit(this))return;
    if(SkyTheatre && SkyTheatre->SeatNearby()>=0){SkyTheatre->SitNearest();return;}
    if(CinemaScreen && CinemaScreen->SeatNearby()>=0){CinemaScreen->SitNearest();return;}
    if(CinemaScreen && CinemaScreen->Nearby()){CinemaScreen->OpenControls();return;}
    if(Manager)if(auto* L=Manager->NearestLift())
    {
        auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));int32 Stop;bool OnCar;
        if(L->Nearby(P,Stop,OnCar))
        {
            if(L->IsMoving())Notify(TEXT("展望昇降機が移動しています。"));
            else if(OnCar){ActiveLift=L;SetMenu(EEWMenu::Lift);}
            else if(L->FloorIndex()==Stop)Notify(TEXT("昇降機のかごの中央へ進み、行き先を選んでください。"));
            else if(L->Call(Stop))Notify(TEXT("この階へ昇降機を呼びました。"));
            return;
        }
    }
    EEWResidenceAction Action = EEWResidenceAction::None;
    if (Manager) if (auto* Residence = Manager->NearestResidence(Action))
    {
        if (Action == EEWResidenceAction::Valve)
        { FString Result; Residence->ToggleWater(Result); Notify(Result); }
        else if (Action == EEWResidenceAction::Seat && Player)
        { if (Player->SitAt(Residence)) Notify(TEXT("雨継ぎの窓辺　水音を聞きながら、出発した広場を眺められます。")); }
        return;
    }
    if(SkyTheatre && SkyTheatre->Nearby()){SkyTheatre->OpenControls();return;}
    if(MediaScreen && MediaScreen->Nearby()){MediaScreen->OpenControls();return;}
    RecordNearest();
}
void UEWGameInstance::SelectLiftFloor(int32 Stop)
{
    auto* L=ActiveLift.Get();auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
    if(L && L->Ride(P,Stop)){SetMenu(EEWMenu::None);Notify(TEXT("展望昇降機：")+L->Spec.Stops[Stop].Label+TEXT("へ移動します。"));}
    else {SetMenu(EEWMenu::None);Notify(TEXT("かごの中央に立ってから、もう一度行き先を選んでください。"));}
}

void UEWGameInstance::TravelFinished()
{
    LastSaveTime = FPlatformTime::Seconds();
    if (bSessionStarted) SaveNow();
    RefreshUI();
}

void UEWGameInstance::Start(const EW::WorldDescriptor& W, const EW::PlaceBookmark& P)
{
    if (!Manager || Manager->IsTravelling()) return;
    if(ActiveMediaScreen)ActiveMediaScreen->CloseControls();
    if(SocialSession && SocialSession->Active())
    {Notify(TEXT("公開の街では徒歩で移動できます。場所コードでの移動は街を退出してから利用してください。"));return;}
    if(Skyrail)Skyrail->ReleaseRider();
    if(SkyrailUpper)SkyrailUpper->ReleaseRider();
    if (bSessionStarted && !SaveNow()) return;
    if (auto* Player = Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
    {
        if (Player->IsSeated() && !Player->LeaveSeat()) return;
        if(auto* PC=UGameplayStatics::GetPlayerController(this,0))PC->SetViewTarget(Player);
    }
    bSessionStarted = true; CurrentMenu = EEWMenu::None; JournalPageIndex = 0;
    LastKnownPosition.Reset();
    Manager->StartWorld(W, P); RefreshUI();
}

void UEWGameInstance::NewWorld()
{
    auto W = EW::WorldDescriptor::NewWorld(); Start(W, StartPlace(W));
}
void UEWGameInstance::ReferenceWorld()
{
    auto W = EW::WorldDescriptor::ReferenceWorld(); EW::PlaceBookmark P;
    if (!Saves || !Saves->LoadWorldPosition(W.Code(), P)) P = StartPlace(W);
    Start(W, P);
}
void UEWGameInstance::ContinueWorld()
{
    ReferenceWorld();
}
void UEWGameInstance::ReturnToPlaza()
{const auto W=EW::WorldDescriptor::ReferenceWorld();Start(W,StartPlace(W));}
void UEWGameInstance::VisitSkyrail()
{
    const auto W=EW::WorldDescriptor::ReferenceWorld();
    EW::PlaceBookmark P;P.WorldCode=W.Code();P.Coord={0,0};P.Id=TEXT("skyrail78/station0");
    P.Name=TEXT("時計広場駅");P.LocalPosition=EWSkyrailPlan::Boarding(0);P.Yaw=270;Start(W,P);
}
void UEWGameInstance::VisitSkyTheatre()
{
    const auto W=EW::WorldDescriptor::ReferenceWorld();const auto R=EW::GenerateChunk(W,{0,0});FTransform T;
    if(!EWSkyTheatrePlan::Frame(R,T))return;
    EW::PlaceBookmark P;P.WorldCode=W.Code();P.Coord={0,0};P.Id=TEXT("sky-theatre82");P.Name=TEXT("天空シアター");
    P.LocalPosition=T.TransformPosition(EWSkyTheatrePlan::Entry());P.Yaw=90;Start(W,P);
}
void UEWGameInstance::VisitSkyport(){Start(EW::WorldDescriptor::ReferenceWorld(),EWAeroYachtPlan::Arrival());}
AEWAeroYacht* UEWGameInstance::YachtFor(const AEWCharacter* Player) const
{for(const auto A:Yachts)if(A && A->Contains(Player))return A;return nullptr;}
void UEWGameInstance::VisitOuterWater()
{const auto W=EW::WorldDescriptor::ReferenceWorld();Start(W,EWOuterWater::Entrance(W));}
bool UEWGameInstance::PreviewOuterWater(int32 View,double Hour)
{
    if(!GIsEditor || !Manager || !DayCycle || !FMath::IsFinite(Hour) || Hour<0 || Hour>=24 || View<0 || View>5)return false;
    const EW::ChunkCoord C{3,0};const auto R=Manager->RecipeAt(C);auto* PC=UGameplayStatics::GetPlayerController(this,0);if(!R || !PC)return false;
    const double H=R->Hub.Z;const FVector Eyes[]={FVector(1000,6100,H+170),FVector(6400,10800,H+1065),FVector(6400,7000,H+165),FVector(1050,4400,H+2200),FVector(-1000,-2000,H+11500),FVector(500,4000,H+3500)};
    const FVector Aims[]={FVector(11600,8000,H+3100),FVector(15500,3000,H+3600),FVector(2300,2300,H+3600),FVector(2300,3480,H+2300),FVector(7800,8900,H+6400),FVector(2300,2300,H+3700)};
    if(!HotelPreviewCamera)HotelPreviewCamera=GetWorld()->SpawnActor<ACameraActor>();
    const auto Eye=Manager->ToRender(C,Eyes[View]);HotelPreviewCamera->SetActorLocationAndRotation(Eye,(Manager->ToRender(C,Aims[View])-Eye).Rotation());HotelPreviewCamera->GetCameraComponent()->SetFieldOfView(85);PC->SetViewTarget(HotelPreviewCamera);DayCycle->SetAuditHour(Hour);SetMenu(EEWMenu::None);return true;
}
void UEWGameInstance::VisitPublicPlace(int32 Index)
{
    const auto W=EW::WorldDescriptor::ReferenceWorld();const auto R=EW::GenerateChunk(W,{0,0});
    if(const auto* Room=EWExplorationPlan::Find(R,Index))Start(W,EWExplorationPlan::Bookmark(R,*Room,true));
}
bool UEWGameInstance::PreviewPublicPlace(int32 Index,double Hour,int32 View)
{
    if(!GIsEditor || !Manager || !DayCycle || !FMath::IsFinite(Hour) || Hour<0 || Hour>=24 || View<0 || View>2)return false;
    const auto R=Manager->RecipeAt({0,0});const auto* Room=R?EWExplorationPlan::Find(*R,Index):nullptr;
    auto* PC=UGameplayStatics::GetPlayerController(this,0);if(!Room || !PC)return false;
    if(!HotelPreviewCamera)HotelPreviewCamera=GetWorld()->SpawnActor<ACameraActor>();
    const FVector Eyes[]={FVector(-120,-1420,165),FVector(0,-2150,170),FVector(500,1500,585)};
    const FVector Aims[]={FVector(0,550,Index==2?430:270),FVector(0,550,650),FVector(0,-800,150)};
    const auto Eye=Manager->ToRender({0,0},Room->Frame.TransformPosition(Eyes[View]));
    const auto Aim=Manager->ToRender({0,0},Room->Frame.TransformPosition(Aims[View]));
    HotelPreviewCamera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());HotelPreviewCamera->GetCameraComponent()->SetFieldOfView(85);
    PC->SetViewTarget(HotelPreviewCamera);DayCycle->SetAuditHour(Hour);SetMenu(EEWMenu::None);return true;
}
void UEWGameInstance::VisitSkyResidence(int32 Index)
{
    if(Index<0 || Index>=8)return;
    const auto W=EW::WorldDescriptor::ReferenceWorld();const auto R=EW::GenerateChunk(W,{0,0});
    const auto* Room=EWHotelPlan::Find(R,Index);if(!Room)return;
    EW::PlaceBookmark P;P.WorldCode=W.Code();P.Coord={0,0};P.Id=FString::Printf(TEXT("hotel83/%d"),Index);
    P.Name=EWHotelPlan::Name(Index);P.LocalPosition=Room->Frame.TransformPosition(FVector(0,-740,110));
    P.Yaw=Room->Frame.Rotator().Yaw+90;Start(W,P);
}
bool UEWGameInstance::PreviewSkyResidence(int32 Index,double Hour,int32 View)
{
    if(!GIsEditor || !Manager || !DayCycle || !FMath::IsFinite(Hour) || Hour<0 || Hour>=24 || View<0 || View>3)return false;
    const auto R=Manager->RecipeAt({0,0});const auto* Room=R?EWHotelPlan::Find(*R,Index):nullptr;
    auto* PC=UGameplayStatics::GetPlayerController(this,0);if(!Room || !PC)return false;
    if(!HotelPreviewCamera)HotelPreviewCamera=GetWorld()->SpawnActor<ACameraActor>();
    const FVector Eyes[]={FVector(-110,-425,165),FVector(110,-240,165),FVector(-450,750,168),FVector(0,-740,168)};
    const FVector Aims[]={FVector(-350,180,145),FVector(460,235,138),FVector(-430,1550,190),FVector(0,100,160)};
    const auto T=Room->Frame;const auto Eye=Manager->ToRender({0,0},T.TransformPosition(Eyes[View]));
    HotelPreviewCamera->SetActorLocationAndRotation(Eye,(Manager->ToRender({0,0},T.TransformPosition(Aims[View]))-Eye).Rotation());
    HotelPreviewCamera->GetCameraComponent()->SetFieldOfView(80);PC->SetViewTarget(HotelPreviewCamera);DayCycle->SetAuditHour(Hour);SetMenu(EEWMenu::None);return true;
}
void UEWGameInstance::VisitCinema()
{
    const auto W=EW::WorldDescriptor::ReferenceWorld();const auto R=EW::GenerateChunk(W,{0,0});
    for(const auto& Room:R.Interiors)if(Room.Kind==10)
    {
        EW::PlaceBookmark P;P.WorldCode=W.Code();P.Coord={0,0};P.Id=TEXT("cinema/waterlight");
        P.Name=TEXT("水鏡の映写室");P.LocalPosition=Room.Frame.TransformPosition(EWCinemaPlan::Entry());
        P.Yaw=Room.Frame.Rotator().Yaw+90;Start(W,P);return;
    }
}
void UEWGameInstance::StartFromCode(const FString& Input)
{
    const FString Code = Input.TrimStartAndEnd(); FString Error;
    if (Code.StartsWith(TEXT("ewp")))
    {
        EW::PlaceBookmark P;
        if (EW::PlaceBookmark::Parse(Code, P, Error)) Visit(P); else Notify(Error);
    }
    else
    {
        EW::WorldDescriptor W;
        if (!EW::WorldDescriptor::Parse(Code, W, Error)) { Notify(Error); return; }
        EW::PlaceBookmark P;
        if (!Saves || !Saves->LoadWorldPosition(W.Code(), P)) P = StartPlace(W);
        Start(W, P);
    }
}
void UEWGameInstance::Visit(const EW::PlaceBookmark& P)
{
    EW::WorldDescriptor W; FString Error;
    if (EW::WorldDescriptor::Parse(P.WorldCode, W, Error)) Start(W, P); else Notify(Error);
}

void UEWGameInstance::RecordNearest()
{
    if (!Manager || !bSessionStarted || Manager->IsTravelling() || !Saves) return;
    const auto Place = Manager->NearestPlace(2200);
    if (!Place) { Notify(TEXT("発見地点の案内標へ近づくと、旅の図鑑へ記録できます。")); return; }
    if (Saves->HasRecord(Place->WorldCode, Place->Id)) { Notify(TEXT("この場所は図鑑に記録されています。")); return; }
    if (Saves->Record(*Place)) Notify(TEXT("発見を記録しました：") + Place->Name);
    else Notify(Saves->Error(), 15);
}
void UEWGameInstance::Favourite(const EW::PlaceBookmark& P)
{
    if (!Saves) return;
    if (!Saves->SetFavourite(P.WorldCode, P.Id, !P.bFavourite)) Notify(Saves->Error(), 15);
    RefreshUI();
}
void UEWGameInstance::CopyWorld()
{
    if (Manager) { FPlatformApplicationMisc::ClipboardCopy(*Manager->Descriptor().Code()); Notify(TEXT("世界コードをコピーしました。")); }
}
void UEWGameInstance::CopyPlace(const EW::PlaceBookmark& P)
{
    FPlatformApplicationMisc::ClipboardCopy(*P.Code()); Notify(TEXT("場所コードをコピーしました。同じ版のゲームでこの場所を訪れられます。"));
}
void UEWGameInstance::ExportRecords()
{
    FString Path;
    if (Saves && Manager && Saves->Export(Manager->Descriptor().Code(), Path))
        Notify(TEXT("発見記録を書き出しました。保存フォルダーの Exports にあります。"), 15);
    else if (Saves) Notify(Saves->Error(), 15);
}
void UEWGameInstance::OpenSaveDirectory() { if (Saves) FPlatformProcess::ExploreFolder(*Saves->Root()); }

void UEWGameInstance::SetMenu(EEWMenu Menu)
{
    if(CurrentMenu==EEWMenu::Monitor && Menu!=EEWMenu::Monitor && ActiveMediaScreen)ActiveMediaScreen->CloseControls();
    if(PhotoMode && PhotoMode->Active() && Menu!=EEWMenu::Photo)PhotoMode->Close();
    if(CurrentMenu==EEWMenu::Terminal && Menu!=EEWMenu::Terminal && Terminal)Terminal->PauseVideo();
    if (Menu == EEWMenu::None && !bSessionStarted) Menu = EEWMenu::Main;
    if(Menu!=EEWMenu::None && SocialSession)SocialSession->PushToTalk(false);
    CurrentMenu = Menu; RefreshUI();
}
void UEWGameInstance::ToggleMenu()
{
    SetMenu(CurrentMenu == EEWMenu::None ? EEWMenu::Main : bSessionStarted ? EEWMenu::None : EEWMenu::Main);
}
void UEWGameInstance::ToggleJournal()
{
    if (bSessionStarted) SetMenu(CurrentMenu == EEWMenu::Journal ? EEWMenu::None : EEWMenu::Journal);
}
void UEWGameInstance::RefreshUI()
{
    UpdatePresentationState();
    if (auto* PC = Cast<AEWPlayerController>(UGameplayStatics::GetPlayerController(this, 0))) PC->RefreshInterface();
}
void UEWGameInstance::UpdatePresentationState()
{
    const bool Foreground = GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport &&
        GEngine->GameViewport->Viewport->IsForegroundWindow();
    if (bWorldEnding || FParse::Param(FCommandLine::Get(),TEXT("EWCinematic95"))) NativeHUD.Hide(); else NativeHUD.Update(*this);
    Graphics.UpdateRuntime(CurrentMenu != EEWMenu::None, !bSessionStarted || !Manager || Manager->IsTravelling() || bWorldEnding,
        Foreground, NativeHUD.IsVisible());
    if (Graphics.ConsumeOutputStatusChange() && CurrentMenu == EEWMenu::Settings)
        if (auto* PC = Cast<AEWPlayerController>(UGameplayStatics::GetPlayerController(this, 0))) PC->RefreshInterface();
}
void UEWGameInstance::Notify(const FString& Text, double Seconds)
{
    Message = Text; MessageUntil = FPlatformTime::Seconds() + Seconds;
    UE_LOG(LogTemp, Display, TEXT("EW_NOTICE %s"), *Text);
}

void UEWGameInstance::CacheCurrentPosition()
{
    if (!bSessionStarted || bWorldEnding || !IsValid(Manager) || Manager->IsTravelling()) return;
    EW::PlaceBookmark P;
    if (Manager->TryCurrentPosition(P)) LastKnownPosition = P;
}

void UEWGameInstance::BeforeWorldTearDown(UWorld* World)
{
    // UE also tears down a temporary startup world before AttachWorld. That
    // event must not mark the later playable world as already ending.
    if (!bSessionStarted || !IsValid(Manager) || World != Manager->GetWorld()) return;
    // Snapshot while the pawn and its controller still exist. Shutdown runs
    // after actor teardown and must never synthesize a zero-vector position.
    CacheCurrentPosition(); bWorldEnding = true;
    if (!bDiscardExit) SaveNow();
}

bool UEWGameInstance::SaveNow()
{
    if (!bSessionStarted) return true;
    if(Fishing && !Fishing->FlushPending()){Notify(Fishing->SaveError(),15);return false;}
    if (!bWorldEnding && (!IsValid(Manager) || Manager->IsTravelling())) return true;
    CacheCurrentPosition();
    if (!LastKnownPosition.IsSet()) { Notify(TEXT("現在地を取得できないため、保存を見送りました。"),15); return false; }
    if (!Saves || !Saves->IsOpen()) { Notify(TEXT("保存場所を開けないため、現在地を保存できません。"), 15); return false; }
    const EW::PlaceBookmark& P = LastKnownPosition.GetValue();
    if (!Saves->SaveCurrent(P)) { Notify(Saves->Error(), 15); return false; }
    LastSaved = P; LastSaveTime = FPlatformTime::Seconds(); return true;
}
bool UEWGameInstance::Tick(float Seconds)
{
    if (PendingDisplayRestore > 0 && FPlatformTime::Seconds() >= PendingDisplayRestore)
    {
        PendingDisplayRestore = -1;
        MoveToPrimaryDisplay();
    }
    CacheCurrentPosition();
    // Explicit opt-in evidence for keyboard/mouse QA; absent in normal play.
    if (!InputEvidencePath.IsEmpty() && Manager)
    {
        auto O=MakeShared<FJsonObject>(); O->SetNumberField(TEXT("time"),FPlatformTime::Seconds());
        O->SetNumberField(TEXT("menu"),int32(CurrentMenu)); O->SetBoolField(TEXT("travelling"),Manager->IsTravelling());
        O->SetStringField(TEXT("world"),Manager->Descriptor().Code());
        O->SetBoolField(TEXT("soft_style"),bSoftStyle);
        O->SetObjectField(TEXT("graphics"), Graphics.Evidence());
        if(MediaScreen)O->SetObjectField(TEXT("monitor"),MediaScreen->Evidence());
        if(CinemaScreen)O->SetObjectField(TEXT("cinema"),CinemaScreen->Evidence());
        if(SkyTheatre)O->SetObjectField(TEXT("sky_theatre"),SkyTheatre->Evidence());
        if(Airship)O->SetObjectField(TEXT("airship"),Airship->Evidence());
        TArray<TSharedPtr<FJsonValue>> Ships,YachtStates;
        for(const auto A:Airships)if(A)Ships.Add(MakeShared<FJsonValueObject>(A->Evidence()));
        for(const auto A:Yachts)if(A)YachtStates.Add(MakeShared<FJsonValueObject>(A->Evidence()));
        O->SetArrayField(TEXT("airships"),Ships);O->SetArrayField(TEXT("yachts"),YachtStates);
        if(Skyport)O->SetObjectField(TEXT("skyport"),Skyport->Evidence());
        if(DayCycle)O->SetObjectField(TEXT("day_cycle"),DayCycle->Evidence());
        if(Skyrail)O->SetObjectField(TEXT("skyrail"),Skyrail->Evidence());
        if(SkyrailUpper)O->SetObjectField(TEXT("skyrail_upper"),SkyrailUpper->Evidence());
        if(CinemaSession)O->SetObjectField(TEXT("online_cinema"),CinemaSession->Evidence());
        O->SetBoolField(TEXT("desktop_hud"), NativeHUD.IsVisible());
        O->SetStringField(TEXT("desktop_hud_evidence"), NativeHUD.Evidence());
        O->SetNumberField(TEXT("gamepad_events"), double(GamepadEvents));
        O->SetBoolField(TEXT("gamepad_ui"), bUsingGamepad);
        O->SetBoolField(TEXT("sony_ui"), bSonyGamepad);
        O->SetBoolField(TEXT("invert_pad_y"), bInvertPadY);
        O->SetBoolField(TEXT("scripted_world_audit"), bScriptedWorldAudit);
        O->SetNumberField(TEXT("display_restores"), DisplayRestoreCount);
        if (GEngine && GEngine->GameViewport)
            if (auto Window = GEngine->GameViewport->GetWindow())
            {
                O->SetStringField(TEXT("window_position"), Window->GetPositionInScreen().ToString());
                O->SetNumberField(TEXT("window_mode"), int32(Window->GetWindowMode()));
                O->SetBoolField(TEXT("window_visible"), Window->IsVisible());
                O->SetBoolField(TEXT("window_minimized"), Window->IsWindowMinimized());
            }
        if (GEngine && GEngine->GetGameUserSettings())
            O->SetNumberField(TEXT("settings_window_mode"), int32(GEngine->GetGameUserSettings()->GetFullscreenMode()));
        if (auto* PC = Cast<AEWPlayerController>(UGameplayStatics::GetPlayerController(this, 0)))
        {
            O->SetStringField(TEXT("focused_control"), PC->FocusedControlLabel());
            O->SetNumberField(TEXT("right_y_after_viewport"), PC->GetInputAnalogKeyState(EKeys::Gamepad_RightY));
        }
        if (const auto* Pad = FEWGamepadModule::GetIfLoaded())
        { O->SetBoolField(TEXT("sony_connected"), Pad->SonyConnected()); O->SetNumberField(TEXT("sony_native_events"), double(Pad->SonyEventCount())); }
        if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
        {
            const auto Size=GEngine->GameViewport->Viewport->GetSizeXY();
            O->SetNumberField(TEXT("viewport_width"),Size.X); O->SetNumberField(TEXT("viewport_height"),Size.Y);
        }
        if (auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0)))
        {
            auto Loc=Manager->CurrentPosition(); O->SetStringField(TEXT("coord"),Loc.Coord.Text());
            O->SetNumberField(TEXT("x"),Loc.LocalPosition.X); O->SetNumberField(TEXT("y"),Loc.LocalPosition.Y); O->SetNumberField(TEXT("z"),Loc.LocalPosition.Z);
            O->SetNumberField(TEXT("yaw"),P->GetControlRotation().Yaw); O->SetNumberField(TEXT("pitch"),P->GetControlRotation().Pitch);
            O->SetBoolField(TEXT("grounded"),P->GetCharacterMovement()->IsMovingOnGround()); O->SetNumberField(TEXT("fall_recoveries"),P->FallRecoveries);
            O->SetNumberField(TEXT("forward_events"),P->ForwardEvents); O->SetNumberField(TEXT("strafe_events"),P->StrafeEvents);
            O->SetNumberField(TEXT("look_events"),P->LookEvents); O->SetNumberField(TEXT("jump_events"),P->JumpEvents);
            O->SetNumberField(TEXT("jump_count"),P->JumpCurrentCount);
            O->SetBoolField(TEXT("seated"),P->IsSeated());
        }
        O->SetNumberField(TEXT("discoveries"),RecordCount());
        FString Text; FJsonSerializer::Serialize(O,TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(InputEvidencePath),true);
        FFileHelper::SaveStringToFile(Text+TEXT("\n"),*InputEvidencePath,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,&IFileManager::Get(),FILEWRITE_Append);
    }
    if (bSessionStarted && Manager && !Manager->IsTravelling() && FPlatformTime::Seconds() - LastSaveTime >= 30)
    {
        const auto P = Manager->CurrentPosition();
        if (P.WorldCode != LastSaved.WorldCode || P.Coord != LastSaved.Coord ||
            !P.LocalPosition.Equals(LastSaved.LocalPosition, 20) || FMath::Abs(P.Yaw - LastSaved.Yaw) > 1) SaveNow();
        LastSaveTime = FPlatformTime::Seconds();
    }
    return true;
}
void UEWGameInstance::Quit()
{
    if (!SaveNow()) { SetMenu(EEWMenu::SaveFailure); return; }
    if (auto* PC = UGameplayStatics::GetPlayerController(this, 0)) PC->ConsoleCommand(TEXT("quit"));
}
void UEWGameInstance::QuitWithoutSaving()
{
    bDiscardExit = true;
    if (auto* PC = UGameplayStatics::GetPlayerController(this, 0)) PC->ConsoleCommand(TEXT("quit"));
}
void UEWGameInstance::Capture()
{
    if(PhotoMode && PhotoMode->Active()){PhotoMode->Shoot();return;}
    if (!Manager || Manager->IsTravelling()) return;
    const FString Dir = Saves ? FPaths::Combine(Saves->Root(), TEXT("Screenshots")) : FPaths::ScreenShotDir();
    IFileManager::Get().MakeDirectory(*Dir, true);
    const FString Path = FPaths::Combine(Dir, TEXT("空の回廊-") + FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")) + TEXT(".png"));
    FScreenshotRequest::RequestScreenshot(Path, false, true);
    Notify(TEXT("撮影しました。保存フォルダーの Screenshots から開けます。"));
}
void UEWGameInstance::SetQuality(bool High)
{
    bHighQuality = High;
    auto* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
    if (Settings)
    {
        Settings->SetOverallScalabilityLevel(High ? 3 : 2);
        Settings->SetResolutionScaleValueEx(High ? 100 : 75);
        Settings->ApplyNonResolutionSettings(); Settings->SaveSettings();
    }
    if (auto* PC = UGameplayStatics::GetPlayerController(this, 0))
    {
        PC->ConsoleCommand(High ? TEXT("r.ScreenPercentage 100") : TEXT("r.ScreenPercentage 75"));
        PC->ConsoleCommand(High ? TEXT("r.Shadow.Virtual.MaxPhysicalPages 4096") : TEXT("r.Shadow.Virtual.MaxPhysicalPages 2048"));
    }
    Graphics.Apply();
    RefreshUI();
}
void UEWGameInstance::SetNativeResolution()
{
    Graphics.SetNative();
    MoveToPrimaryDisplay();
}
void UEWGameInstance::MoveToPrimaryDisplay()
{
    auto* Settings=GEngine ? GEngine->GetGameUserSettings() : nullptr;
    if (!Settings) return;
    FDisplayMetrics Metrics; FDisplayMetrics::RebuildDisplayMetrics(Metrics);
    const auto* Primary = Metrics.MonitorInfo.FindByPredicate([](const FMonitorInfo& M) { return M.bIsPrimary; });
    const FIntPoint Native = Primary ? FIntPoint(Primary->DisplayRect.Right - Primary->DisplayRect.Left,
        Primary->DisplayRect.Bottom - Primary->DisplayRect.Top) : Settings->GetDesktopResolution();
    if (Native.X<640 || Native.Y<480) return;
    Graphics.BeginDisplayChange();
    const FVector2D Position = Primary ? FVector2D(Primary->DisplayRect.Left, Primary->DisplayRect.Top) : FVector2D::ZeroVector;
    auto Window = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWindow() : TSharedPtr<SWindow>();
    // A borderless window stays on its current display when only resolution is
    // changed. Move the owned window first, then restore borderless rendering.
    if (Window)
    {
        Window->SetWindowMode(EWindowMode::Windowed);
        Window->MoveWindowTo(Position);
    }
    Settings->SetWindowPosition(FMath::RoundToInt(Position.X), FMath::RoundToInt(Position.Y));
    Settings->SetFullscreenMode(EWindowMode::WindowedFullscreen);
    Settings->SetScreenResolution(Native);
    Settings->SetDynamicResolutionEnabled(false);
    Settings->ApplySettings(false);
    if (Window)
    {
        Window->SetWindowMode(EWindowMode::WindowedFullscreen);
        Window->ReshapeWindow(Position, FVector2D(Native));
    }
    Graphics.Apply();
    Settings->ConfirmVideoMode(); Settings->SaveSettings();
    GConfig->SetInt(TEXT("EndlessWorld"),TEXT("NativeResolutionVersion"),1,GGameUserSettingsIni);
    GConfig->Flush(false,GGameUserSettingsIni);
    if (auto* P=IConsoleManager::Get().FindConsoleVariable(TEXT("r.SecondaryScreenPercentage.GameViewport"))) P->Set(100.f,ECVF_SetByConsole);
    DisplayTopology = MonitorTopology(Metrics); PendingDisplayRestore = -1; ++DisplayRestoreCount;
    Notify(FString::Printf(TEXT("メインディスプレイ %d × %d に表示しました。"), Native.X, Native.Y));
    RefreshUI();
}
void UEWGameInstance::DisplayMetricsChanged(const FDisplayMetrics& Metrics)
{
    const FString Updated = MonitorTopology(Metrics);
    if (Updated == DisplayTopology) return;
    DisplayTopology = Updated;
    const auto* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;
    if (Settings && Settings->GetFullscreenMode() == EWindowMode::WindowedFullscreen)
        PendingDisplayRestore = FPlatformTime::Seconds() + 1.5;
}
FString UEWGameInstance::ResolutionText() const
{
    if (!GEngine || !GEngine->GameViewport || !GEngine->GameViewport->Viewport) return {};
    const FIntPoint Size=GEngine->GameViewport->Viewport->GetSizeXY();
    const auto* Percentage=IConsoleManager::Get().FindConsoleVariable(TEXT("r.ScreenPercentage"));
    const double Scale=Percentage ? Percentage->GetFloat() : 100;
    return FString::Printf(TEXT("現在の表示　%d × %d ／ 描画 %.0f%%"),Size.X,Size.Y,Scale);
}
bool UEWGameInstance::CanContinue() const { EW::PlaceBookmark P; return Saves && Saves->LoadWorldPosition(EW::WorldDescriptor::ReferenceWorld().Code(),P); }
void UEWGameInstance::SetSoftStyle(bool Value)
{
    bSoftStyle=Value;
    auto* Material=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndlessWorld/Materials/M_IllustratedLight.M_IllustratedLight"));
    if (Material && GetWorld())
    {
        TArray<AActor*> Volumes; UGameplayStatics::GetAllActorsOfClass(this,APostProcessVolume::StaticClass(),Volumes);
        for (auto* Actor:Volumes) if (auto* Volume=Cast<APostProcessVolume>(Actor))
            Volume->AddOrUpdateBlendable(Material,Value?1.f:0.f);
    }
    GConfig->SetBool(TEXT("EndlessWorld"),TEXT("SoftStyle"),Value,GGameUserSettingsIni);
    GConfig->Flush(false,GGameUserSettingsIni); RefreshUI();
}
bool UEWGameInstance::CanPlay() const { return Manager && !Manager->IsTravelling(); }
TArray<EW::PlaceBookmark> UEWGameInstance::JournalPage(int32 Page) const
{
    return Saves && Manager ? Saves->Records(Manager->Descriptor().Code(), FMath::Max(0, Page) * 12, 12) : TArray<EW::PlaceBookmark>();
}
int32 UEWGameInstance::RecordCount() const { return Saves && Manager ? FMath::Max(0, Saves->Count(Manager->Descriptor().Code())) : 0; }
bool UEWGameInstance::IsRecorded(const EW::PlaceBookmark& P) const { return Saves && Saves->HasRecord(P.WorldCode, P.Id); }

FString UEWGameInstance::RegionText() const
{
    if (!Manager || !Manager->bHasWorld) return TEXT("空の回廊");
    if(EWOuterWater::Contains(Manager->Descriptor(),Manager->PlayerCoord()))return TEXT("白塔の水都");
    return EW::RegionName(EW::RegionAt(Manager->Descriptor(), Manager->PlayerCoord()));
}
FString UEWGameInstance::NearbyText() const
{
    if (!Manager || CurrentMenu != EEWMenu::None) return {};
    for(const auto* Screen:{MediaScreen.Get(),CinemaScreen.Get(),SkyTheatre.Get()})
        if(Screen && Screen->Nearby())return Screen->Title()+TEXT("の光る端末　")+InputHint(TEXT("E 操作"),TEXT("X 操作"),TEXT("□ 操作"));
    if(Terminal){const FString H=Terminal->Hint();if(!H.IsEmpty())return H;}
    const auto PublicHint=EWExplorationPlan::Hint(this);if(!PublicHint.IsEmpty())return PublicHint;
    if(Concepts && Concepts->Nearby())return TEXT("チェスの卓　遊ぶ　E");
    if(Fishing){const auto Hint=Fishing->Hint();if(!Hint.IsEmpty())return Hint;}
    if (auto* Player = Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
    {
        if(const auto* Ship=YachtFor(Player))return Ship->Hint(Player);
        if(Skyport && FVector::DistSquared(Player->GetActorLocation(),Skyport->GetActorLocation())<FMath::Square(1900.))return TEXT("アウレリア空中港　正面の発着案内へ / 寄港中は橋を歩いて乗船");
        if(SkyrailUpper){const FString H=SkyrailUpper->Hint(Player);if(!H.IsEmpty())return H;}
        if(Skyrail){const FString H=Skyrail->Hint(Player);if(!H.IsEmpty())return H;}
        if (Player->IsSeated()) return (SkyTheatre && SkyTheatre->ListenerInside()?TEXT("天空シアター　"):CinemaScreen && CinemaScreen->ListenerInside()?TEXT("水鏡の映写室　"):TEXT("雨継ぎの窓辺　")) + InputHint(TEXT("E / WASD 立ち上がる　マウス 見回す"),
            TEXT("X / 左スティック 立ち上がる　右スティック 見回す"), TEXT("□ / 左スティック 立ち上がる　右スティック 見回す"));
    }
    if(auto* L=Manager->NearestLift())
        return L->Hint(Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0)))+TEXT("　")+InputHint(TEXT("E"),TEXT("X"),TEXT("□"));
    EEWResidenceAction Action = EEWResidenceAction::None;
    if (auto* Residence = Manager->NearestResidence(Action))
        return Residence->Hint(Action) + TEXT("　") + InputHint(TEXT("E"),TEXT("X"),TEXT("□"));
    if(SkyTheatre && SkyTheatre->SeatNearby()>=0)return TEXT("天空シアターの客席　座る　")+InputHint(TEXT("E"),TEXT("X"),TEXT("□"));
    if(SkyTheatre && SkyTheatre->Nearby())return TEXT("天空シアター　検索・再生　")+InputHint(TEXT("E"),TEXT("X"),TEXT("□"));
    if(CinemaScreen && CinemaScreen->SeatNearby()>=0)return TEXT("映画館の客席　座る　")+InputHint(TEXT("E"),TEXT("X"),TEXT("□"));
    if(CinemaScreen && CinemaScreen->Nearby())return TEXT("水鏡の映写室　検索・再生　")+InputHint(TEXT("E"),TEXT("X"),TEXT("□"));
    if(MediaScreen && MediaScreen->Nearby())return TEXT("広場のモニター　検索・再生　")+InputHint(TEXT("E"),TEXT("X"),TEXT("□"));
    if(const auto* Player=UGameplayStatics::GetPlayerCharacter(this,0))
        if(const auto Recipe=Manager->RecipeAt(Manager->PlayerCoord()))
        {
            const auto RoomText=EWInteriors::NearbyText(*Recipe,Manager->ToLocal(Recipe->Coord,Player->GetActorLocation()));
            if(!RoomText.IsEmpty())return RoomText;
        }
    const auto Place = Manager->NearestPlace(2200);
    if (!Place) return InputHint(TEXT("WASD 歩く　Shift 走る　Space 2段ジャンプ　Tab 図鑑　Esc メニュー"),
        TEXT("左スティック 移動　LB 走る　A 2段ジャンプ　Y 図鑑　Menu メニュー"),
        TEXT("左スティック 移動　L1 走る　× 2段ジャンプ　△ 図鑑　Options メニュー"));
    return Place->Name + (IsRecorded(*Place) ? TEXT("　記録済み ／ ") + InputHint(TEXT("Tab"), TEXT("Y"), TEXT("△")) + TEXT(" 図鑑") :
        TEXT("　") + InputHint(TEXT("E"), TEXT("X"), TEXT("□")) + TEXT(" 発見を記録する"));
}
FString UEWGameInstance::StatusText() const
{
    if (FPlatformTime::Seconds() <= MessageUntil) return Message;
    if (Manager && Manager->IsTravelling()) return TEXT("周囲の景色と道を準備しています…");
    return {};
}
FString UEWGameInstance::DiagnosticsText() const
{
    if (!bDiagnostics || !Manager) return {};
    const auto M = FPlatformMemory::GetStats();
    return FString::Printf(TEXT("区画 %d/49　生成 %d/2　待機 %d/8　原点移動 %d\n描画個数 %d　当たり判定 %d　反映 %.2f ms　RAM %.2f GB\n%s"),
        Manager->ResidentCount(), Manager->JobCount(), Manager->QueueCount(), Manager->RebaseCount,
        Manager->InstanceCount(), Manager->ColliderCount(), Manager->LastApplyMilliseconds, M.UsedPhysical / 1073741824.,
        *Manager->PlayerCoord().Text());
}
