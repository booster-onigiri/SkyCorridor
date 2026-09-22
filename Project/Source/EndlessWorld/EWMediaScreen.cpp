#include "EWMediaScreen.h"
#include "EWMediaTabletView.h"
#include "EWTerminal.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SViewport.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Serialization/JsonSerializer.h"
#include "EWShutdownTrace.h"
#include "EWBrowserSurface.h"
#include "EWGameInstance.h"
#include "EWChunkManager.h"
#include "EWCharacter.h"
#include "EWClock.h"
#include "EWCinemaPlan.h"
#include "EWSkyTheatrePlan.h"
#include "EWDayCycle.h"
#include "EWCinemaSession.h"
#include "Components/PointLightComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/WidgetComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/StaticMesh.h"
#include "EWBrowserAudioWave.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundSubmix.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SWindow.h"
#include "Dom/JsonObject.h"
#include "Misc/Paths.h"
#include "RHI.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "AudioDevice.h"
#include "AudioThread.h"
#include "AudioMixerDevice.h"
#include "ISubmixBufferListener.h"
#include "Misc/ScopeLock.h"

class FEWMediaOutputProbe final:public ISubmixBufferListener
{
public:
    FCriticalSection Mutex;
    double Left=0,Right=0,LastLeft=0,LastRight=0;int64 Frames=0,Total=0;int32 Channels=0;
    void OnNewSubmixBuffer(const USoundSubmix*,float* Data,int32 Num,int32 Count,int32 Rate,double)override
    {
        FScopeLock Lock(&Mutex);Channels=Count;if(Count<2)return;
        for(int I=0;I<Num;I+=Count){Left+=Data[I]*Data[I];Right+=Data[I+1]*Data[I+1];++Frames;++Total;}
        if(Frames>=Rate){LastLeft=FMath::Sqrt(Left/Frames);LastRight=FMath::Sqrt(Right/Frames);Frames=0;Left=Right=0;}
    }
    TSharedRef<FJsonObject> Evidence()
    {FScopeLock Lock(&Mutex);auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("left_rms"),LastLeft);O->SetNumberField(TEXT("right_rms"),LastRight);O->SetNumberField(TEXT("channels"),Channels);O->SetNumberField(TEXT("frames"),double(Total));return O;}
};

AEWMediaScreen::AEWMediaScreen()
{
    PrimaryActorTick.bCanEverTick=true;
    Root=CreateDefaultSubobject<USceneComponent>(TEXT("MonitorAnchor"));SetRootComponent(Root);
    Panel=CreateDefaultSubobject<UWidgetComponent>(TEXT("WorldScreen"));Panel->SetupAttachment(Root);
    Panel->SetWidgetSpace(EWidgetSpace::World);Panel->SetDrawSize(FVector2D(1280,720));
    Panel->SetPivot(FVector2D(.5,.5));Panel->SetBlendMode(EWidgetBlendMode::Opaque);
    Panel->SetTwoSided(false);Panel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    // This city uses daylight EV 13.3-15.2. Unit-intensity unlit UI is black
    // at that exposure; match the physical luminance scale of its lit windows.
    Panel->SetTintColorAndOpacity(FLinearColor(6000,6000,6000,1));
    Panel->SetRelativeScale3D(FVector(1,1600./1280.,1600./1280.));
    Panel->SetTickWhenOffscreen(true);Panel->SetRedrawTime(1.f/30.f);
    BackPanel=CreateDefaultSubobject<UWidgetComponent>(TEXT("WorldScreenBack"));BackPanel->SetupAttachment(Root);
    BackPanel->SetWidgetSpace(EWidgetSpace::World);BackPanel->SetDrawSize(FVector2D(1280,720));
    BackPanel->SetPivot(FVector2D(.5,.5));BackPanel->SetBlendMode(EWidgetBlendMode::Opaque);
    BackPanel->SetTwoSided(false);BackPanel->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    BackPanel->SetTintColorAndOpacity(FLinearColor(6000,6000,6000,1));
    BackPanel->SetRelativeScale3D(FVector(1,1600./1280.,1600./1280.));
    // A separately oriented face preserves readable text on the back. Its
    // pixels come from the same browser and it sits beyond the solid housing.
    BackPanel->SetRelativeLocation(FVector(-54,0,0));BackPanel->SetRelativeRotation(FRotator(0,180,0));
    BackPanel->SetTickWhenOffscreen(true);BackPanel->SetRedrawTime(1.f/30.f);
    Frame=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("MonitorFrame"));Frame->SetupAttachment(Root);
    Frame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TabletAnchor=CreateDefaultSubobject<USceneComponent>(TEXT("TabletAnchor"));TabletAnchor->SetupAttachment(Root);
    TabletAnchor->SetAbsolute(true,true,true);
    Tablet=CreateDefaultSubobject<UEWTerminalScreen>(TEXT("LightTablet"));Tablet->SetupAttachment(TabletAnchor);
    Tablet->SetWidgetSpace(EWidgetSpace::World);Tablet->SetDrawSize(FVector2D(1280,960));Tablet->SetPivot(FVector2D(.5,.5));
    Tablet->SetBlendMode(EWidgetBlendMode::Masked);Tablet->SetBackgroundColor(FLinearColor::Transparent);Tablet->SetTwoSided(false);
    Tablet->SetRelativeScale3D(FVector(1,92./1280.,92./1280.));Tablet->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Tablet->SetCollisionResponseToAllChannels(ECR_Ignore);Tablet->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    Tablet->SetRedrawTime(.1f);Tablet->SetTickWhenOffscreen(false);
    TabletFrame=CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TabletHousing"));TabletFrame->SetupAttachment(TabletAnchor);
    TabletFrame->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Audio=CreateDefaultSubobject<UAudioComponent>(TEXT("MonitorObjectAudio"));Audio->SetupAttachment(Root);
    Audio->bAutoActivate=false;Audio->bAllowSpatialization=true;Audio->bOverrideAttenuation=true;
    auto& A=Audio->AttenuationOverrides;A.bAttenuate=true;A.bSpatialize=true;
    A.AttenuationShape=EAttenuationShape::Sphere;A.AttenuationShapeExtents=FVector(1800,0,0);A.FalloffDistance=7200;
    A.DistanceAlgorithm=EAttenuationDistanceModel::Linear;
    A.bEnableOcclusion=true;A.OcclusionTraceChannel=ECC_Visibility;A.OcclusionLowPassFilterFrequency=1400;A.OcclusionVolumeAttenuation=.4f;
    A.OcclusionInterpolationTime=.15f;A.bAttenuateWithLPF=true;
    A.LPFRadiusMin=3000;A.LPFRadiusMax=9000;A.LPFFrequencyAtMin=20000;A.LPFFrequencyAtMax=4000;
    PrimaryActorTick.TickGroup=TG_PostUpdateWork;
    RightAudio=CreateDefaultSubobject<UAudioComponent>(TEXT("CinemaRightSpeaker"));RightAudio->SetupAttachment(Root);
    RightAudio->bAutoActivate=false;
    for(int32 I=0;I<9;++I)
    {
        auto* L=CreateDefaultSubobject<UPointLightComponent>(*FString::Printf(TEXT("CinemaLight_%d"),I));
        L->SetupAttachment(Root);L->SetMobility(EComponentMobility::Movable);L->SetVisibility(false);
        L->SetIntensityUnits(ELightUnits::Lumens);L->SetIntensity(24000);L->SetAttenuationRadius(1400);
        L->SetLightColor(FLinearColor(1,.59,.29));L->SetSourceRadius(35);L->SetCastShadows(true);
        L->SetVolumetricScatteringIntensity(0);CinemaLights.Add(L);
    }
}
void AEWMediaScreen::BeginPlay()
{
    Super::BeginPlay();Surface=MakeShared<FEWBrowserSurface>();
    if(GUsingNullRHI)return;
    TabletFrame->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/EndlessWorld/Kit/SM_Wall.SM_Wall")));
    TabletFrame->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndlessWorld/Materials/M_InteriorBrass.M_InteriorBrass")));
    // Thin metal edges float above the existing console; no new obstacle in the aisle.
    auto Edge=[&](FVector P,FVector Size){TabletFrame->AddInstance(FTransform(FQuat::Identity,P,Size/100.));};
    Edge(FVector(-1.2,0,35.4),FVector(2.4,88,1));Edge(FVector(-1.2,0,-35.4),FVector(2.4,88,1));
    Edge(FVector(-1.2,-46.9,0),FVector(2.4,1,64));Edge(FVector(-1.2,46.9,0),FVector(2.4,1,64));
    TabletView=SNew(SEWMediaTabletView).Owner(this);Tablet->SetSlateWidget(TabletView);
    if(IsTheatre())
    {
        Panel->SetRelativeScale3D(FVector(1,ScreenWidth()/1280.,ScreenWidth()/1280.));
        Panel->SetTintColorAndOpacity(FLinearColor(80,80,80,1));BackPanel->SetVisibility(false);
        Frame->SetVisibility(true);
        for(auto* Speaker:{Audio.Get(),RightAudio.Get()})
        {
            Speaker->bAllowSpatialization=true;Speaker->bOverrideAttenuation=true;
            auto& A=Speaker->AttenuationOverrides;A.bAttenuate=true;A.bSpatialize=true;
            A.AttenuationShape=EAttenuationShape::Sphere;A.AttenuationShapeExtents=FVector(1400,0,0);
            A.FalloffDistance=5000;A.DistanceAlgorithm=EAttenuationDistanceModel::Linear;
            A.bEnableOcclusion=false;A.bAttenuateWithLPF=false;
        }
        // Local +Y is screen-left when looking toward the panel's -X face.
        Audio->SetRelativeLocation(FVector(25,ScreenWidth()*.42,-ScreenWidth()*.20));RightAudio->SetRelativeLocation(FVector(25,-ScreenWidth()*.42,-ScreenWidth()*.20));
    }
    Frame->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/EndlessWorld/Kit/SM_Wall.SM_Wall")));
    const double W=ScreenWidth(),H=W*9./16.;
    for(double Y:{-W*.5-17,W*.5+17})Frame->AddInstance(FTransform(FRotator::ZeroRotator,FVector(-10,Y,0),FVector(.35,.25,(H+40)/100.)));
    for(double Z:{-H*.5-12,H*.5+12})Frame->AddInstance(FTransform(FRotator::ZeroRotator,FVector(-10,0,Z),FVector(.35,(W+60)/100.,.25)));
    Frame->AddInstance(FTransform(FRotator::ZeroRotator,FVector(-35,0,0),FVector(.35,(W+60)/100.,(H+40)/100.)));
    if(IsTheatre())Frame->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndlessWorld/Materials/M_InteriorBrass.M_InteriorBrass")));
    const FSlateFontInfo Font(FPaths::ProjectContentDir()/TEXT("Fonts/DroidSansFallback.ttf"),48);
    auto Display=[this,Font]()->TSharedRef<SWidget>{return SNew(SOverlay)
        +SOverlay::Slot()[SNew(SImage).Image(Surface->Brush())]
        +SOverlay::Slot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor(FLinearColor(.015,.045,.055,1)).HAlign(HAlign_Center).VAlign(VAlign_Center)
            .Visibility_Lambda([this]{return Surface.IsValid() && Surface->HasPage()?EVisibility::Collapsed:EVisibility::Visible;})
            [SNew(STextBlock).Text(FText::FromString(bSkyTheatre?TEXT("天空シアター\n\n入口の光る端末で上映を選ぶ"):bCinema?TEXT("水鏡の映写室\n\nロビーの光る端末で上映を選ぶ"):TEXT("空の回廊 シネマ\n\n広場の光る端末で YouTubeを探す")))
                .Justification(ETextJustify::Center).Font(Font).ColorAndOpacity(FLinearColor(.7,.9,.85,1))]];};
    Panel->SetSlateWidget(Display());if(!IsTheatre())BackPanel->SetSlateWidget(Display());
    Wave=NewObject<UEWBrowserAudioWave>(this);Wave->SetSampleRate(48000);Wave->NumChannels=1;
    Wave->Duration=INDEFINITELY_LOOPING_DURATION;Wave->bLooping=false;Wave->SoundGroup=SOUNDGROUP_Music;
    Submix=NewObject<USoundSubmix>(this);Wave->SoundSubmixObject=Submix;
    // Runtime submixes do not get the asset PostLoad registration path. This
    // must run for normal play, before diagnostics (which also register them).
    if(auto Device=GetWorld()->GetAudioDevice())Device->RegisterSoundSubmix(Submix,true);
    Wave->VirtualizationMode=EVirtualizationMode::PlayWhenSilent;Audio->SetSound(Wave);Audio->SetVolumeMultiplier(UserVolume);
    if(IsTheatre())
    {
        RightWave=NewObject<UEWBrowserAudioWave>(this);RightWave->SetSampleRate(48000);RightWave->NumChannels=1;
        RightWave->Duration=INDEFINITELY_LOOPING_DURATION;RightWave->bLooping=false;RightWave->SoundGroup=SOUNDGROUP_Music;
        RightWave->SoundSubmixObject=Submix;RightWave->VirtualizationMode=EVirtualizationMode::PlayWhenSilent;
        RightAudio->SetSound(RightWave);RightAudio->SetVolumeMultiplier(0);Audio->SetVolumeMultiplier(0);
    }
    FString EvidencePath;
    if(FParse::Value(FCommandLine::Get(),TEXT("EWInputEvidence="),EvidencePath))if(auto Device=GetWorld()->GetAudioDevice())
    {
        OutputProbe=MakeShared<FEWMediaOutputProbe,ESPMode::ThreadSafe>();MainProbe=MakeShared<FEWMediaOutputProbe,ESPMode::ThreadSafe>();
        Device->RegisterSubmixBufferListener(OutputProbe.ToSharedRef(),*Submix);
        Device->RegisterSubmixBufferListener(MainProbe.ToSharedRef(),Device->GetMainSubmixObject());
    }
    if(FParse::Param(FCommandLine::Get(),TEXT("EWSilentAudit")))if(auto Device=GetWorld()->GetAudioDevice())
        Device->SetSubmixOutputVolume(&Device->GetMainSubmixObject(),0.f);
}
bool AEWMediaScreen::Available() const{return bInDistrict;}
bool AEWMediaScreen::Nearby() const
{
    const auto* P=UGameplayStatics::GetPlayerPawn(this,0);
    if(!bInDistrict || !P || !TabletAnchor || FVector::Dist2D(P->GetActorLocation(),Station)>240 || FMath::Abs(P->GetActorLocation().Z-Station.Z)>125)return false;
    const auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0);if(!Camera)return false;
    const FVector Eye=bTabletOpen?P->GetActorLocation()+FVector(0,0,60):Camera->GetCameraLocation();
    if(FVector::DotProduct(Eye-TabletAnchor->GetComponentLocation(),TabletAnchor->GetForwardVector())<12)return false;
    FCollisionQueryParams Q;Q.AddIgnoredActor(this);Q.AddIgnoredActor(P);FHitResult Hit;
    return !GetWorld()->LineTraceSingleByChannel(Hit,Eye,TabletAnchor->GetComponentLocation(),ECC_Visibility,Q);
}
bool AEWMediaScreen::CanControlPlayback() const
{const auto* G=GetGameInstance<UEWGameInstance>();return !(bCinema && G && G->CinemaSession && G->CinemaSession->Guest());}
void AEWMediaScreen::OpenControls()
{
    auto* G=GetGameInstance<UEWGameInstance>();auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if(!G || !PC || !TabletView || bTabletOpen)return;
    if(!Nearby()){G->Notify(Title()+TEXT("の光る端末に近づいて操作してください。"));return;}
    if(G->ActiveMediaScreen && G->ActiveMediaScreen!=this)G->ActiveMediaScreen->CloseControls();
    PreviousView=PC->GetViewTarget();PreviousLook=PC->GetControlRotation();
    if(!TabletCamera){TabletCamera=GetWorld()->SpawnActor<ACameraActor>();TabletCamera->GetCameraComponent()->SetFieldOfView(66);}
    bTabletOpen=true;G->ActiveMediaScreen=this;UpdateTablet();
    Tablet->SetCollisionEnabled(ECollisionEnabled::QueryOnly);Tablet->SetRedrawTime(1.f/30.f);Tablet->BindToViewport();
    PC->SetViewTargetWithBlend(TabletCamera,.28f);G->SetMenu(EEWMenu::Monitor);
}
void AEWMediaScreen::CloseControls()
{
    if(!bTabletOpen)return;bTabletOpen=false;Tablet->SetCollisionEnabled(ECollisionEnabled::NoCollision);Tablet->SetRedrawTime(.1f);
    if(auto* PC=UGameplayStatics::GetPlayerController(this,0))
    {PC->SetViewTarget(PreviousView.IsValid()?PreviousView.Get():PC->GetPawn());PC->SetControlRotation(PreviousLook);}
    PreviousView.Reset();
}
TSharedPtr<SWidget> AEWMediaScreen::TabletFocusWidget() const{return TabletView;}
void AEWMediaScreen::FocusTablet(){if(TabletView)TabletView->FocusFirst();}
void AEWMediaScreen::UpdateTablet()
{
    const FRotator Face(28,bCinema?Auditorium.Rotator().Yaw-90:-90,0);
    TabletAnchor->SetWorldLocationAndRotation(Station+FVector(0,0,65),Face);
    if(bTabletOpen && TabletCamera)
    {
        const FVector Aim=TabletAnchor->GetComponentLocation();const FVector Eye=Aim+TabletAnchor->GetForwardVector()*142;
        TabletCamera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());
    }
    // Follow the actual camera exposure, including the lobby and its doorway.
    // A fixed auditorium luminance left the lobby tablet unreadably dark.
    const auto* G=GetGameInstance<UEWGameInstance>();const float EV=G && G->DayCycle?G->DayCycle->CameraExposureEV():12.6f;
    const float Luminance=FMath::Clamp(6800.f*FMath::Pow(2.f,EV-14.2f),8.f,6800.f);
    Tablet->SetTintColorAndOpacity(FLinearColor(Luminance,Luminance,Luminance,1));
}
void AEWMediaScreen::ControlPlayback(FName Action)
{
    if(!Surface || !bTabletOpen || !CanControlPlayback())return;
    if(Action==TEXT("play"))Surface->SetPlaybackPaused(!Surface->VideoPaused());
    else if(Action==TEXT("fullscreen"))Surface->SetVideoFullscreen(true);
    else if(Action==TEXT("page"))Surface->SetVideoFullscreen(false);
    else if(Action==TEXT("back"))Surface->Back();
    else if(Action==TEXT("sound")){if(UserVolume<.01f)SetVolume(.85f);Surface->EnableSound();}
    else if(Action==TEXT("stop"))Stop();
}
void AEWMediaScreen::ToggleLocalMute()
{if(UserVolume>.001f){LastAudibleVolume=UserVolume;SetVolume(0);}else SetVolume(LastAudibleVolume);}
void AEWMediaScreen::LookAtScreen()
{if(Surface && CanControlPlayback())Surface->SetVideoFullscreen(true);if(auto* G=GetGameInstance<UEWGameInstance>())G->SetMenu(EEWMenu::None);if(auto* PC=UGameplayStatics::GetPlayerController(this,0))PC->SetControlRotation((GetActorLocation()-PC->PlayerCameraManager->GetCameraLocation()).Rotation());}
void AEWMediaScreen::Search(const FString& Query)
{
    if(bCinema)if(const auto* G=GetGameInstance<UEWGameInstance>())if(G->CinemaSession && G->CinemaSession->Guest())return;
    if(Surface)Surface->Search(Query);
}
FString AEWMediaScreen::PreviewScreen(bool Film,int32 Seat)
{
    if(!GetWorld()->IsPlayInEditor())return TEXT("PIE required");
    if(Seat>=0)SitSeat(Seat);if(Film && Surface)Surface->TestSyncFilm();LookAtScreen();
    FString Text;FJsonSerializer::Serialize(Evidence(),TJsonWriterFactory<>::Create(&Text));return Text;
}
void AEWMediaScreen::Stop(){if(Surface)Surface->Stop();if(Audio)Audio->Stop();if(Wave)Wave->FlushSamples();if(RightAudio)RightAudio->Stop();if(RightWave)RightWave->FlushSamples();bSoundActive=false;}
void AEWMediaScreen::SetVolume(float V){UserVolume=FMath::Clamp(V,0.f,1.f);Audio->SetVolumeMultiplier(UserVolume*(IsTheatre()?RoomGain:1));RightAudio->SetVolumeMultiplier(UserVolume*RoomGain);}
void AEWMediaScreen::Tick(float Delta)
{
    Super::Tick(Delta);auto* G=GetGameInstance<UEWGameInstance>();auto* M=G?G->Manager.Get():nullptr;
    const bool WasHere=bInDistrict;bInDistrict=false;
    if(M && M->Descriptor().Seed==EW::WorldDescriptor::ReferenceWorld().Seed)
    {
        const auto R=M->RecipeAt({0,0});
        if(R && FMath::Abs(M->PlayerCoord().X)<=1 && FMath::Abs(M->PlayerCoord().Y)<=1)
        {
            if(bSkyTheatre)
            {
                FTransform Local;
                if(EWSkyTheatrePlan::Frame(*R,Local))
                {
                    bInDistrict=true;Auditorium=Local;Auditorium.SetLocation(M->ToRender({0,0},Local.GetLocation()));
                    SetActorLocationAndRotation(Auditorium.TransformPosition(EWSkyTheatrePlan::Screen()),EWSkyTheatrePlan::ScreenRotation());
                    // The panel floats beyond the roof. Keep the physical
                    // speakers beside the audience, within their sound zone.
                    Audio->SetWorldLocation(Auditorium.TransformPosition(FVector(-1850,850,220)));
                    RightAudio->SetWorldLocation(Auditorium.TransformPosition(FVector(1850,850,220)));
                    Station=Auditorium.TransformPosition(FVector(350,-2040,110));
                }
            }
            else if(bCinema)
            {
                for(const auto& Room:R->Interiors)if(Room.Kind==10)
                {
                    bInDistrict=true;Auditorium=Room.Frame;
                    Auditorium.SetLocation(M->ToRender({0,0},Room.Frame.GetLocation()));
                    SetActorLocationAndRotation(Auditorium.TransformPosition(EWCinemaPlan::Screen()),
                        (Auditorium.GetRotation()*FRotator(0,-90,0).Quaternion()).Rotator());
                    Station=Auditorium.TransformPosition(FVector(500,-1340,110));break;
                }
            }
            else
            {
                bInDistrict=true;SetActorLocationAndRotation(M->ToRender({0,0},EWClock::ScreenCentre(*R)),FRotator(0,-90,0));
                Station=M->ToRender({0,0},R->Hub+FVector(0,-1200,90));
            }
        }
    }
    SetActorHiddenInGame(!bInDistrict);
    UpdateTablet();
    if(bTabletOpen && (!Nearby() || M->IsTravelling() || G->Menu()!=EEWMenu::Monitor || G->ActiveMediaScreen!=this))
    {if(G->Menu()==EEWMenu::Monitor && G->ActiveMediaScreen==this)G->SetMenu(EEWMenu::None);else CloseControls();}
    if(WasHere && !bInDistrict)Stop();
    if(!Surface)return;Surface->Tick();
    bool Minimized=false;
    if(GEngine && GEngine->GameViewport)if(auto W=GEngine->GameViewport->GetWindow())Minimized=W->IsWindowMinimized();
    Surface->Pause(!bInDistrict || Minimized);
    TArray<int16> PCM;
    if(IsTheatre())Surface->DrainStereoAudio(PCM);else Surface->DrainAudio(PCM);
    const auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0);
    RoomGain=IsTheatre() && bInDistrict && Camera?(bSkyTheatre?EWSkyTheatrePlan::AudioGain(Auditorium.InverseTransformPosition(Camera->GetCameraLocation())):EWCinemaPlan::AudioGain(Auditorium.InverseTransformPosition(Camera->GetCameraLocation()))):0;
    const bool OtherCinema=!bCinema && G && G->CinemaScreen && G->CinemaScreen->ListenerInside();
    const bool Audible=Surface->HasAudio() && bInDistrict && !Minimized && !OtherCinema && (!IsTheatre() || RoomGain>0);
    // Hard gate every source outside the auditorium, before queueing PCM. The
    // vestibule, adjacent rooms and floors never receive an attenuated tail.
    const float Gain=Audible?UserVolume*(IsTheatre()?RoomGain:1):0;
    Audio->SetVolumeMultiplier(Gain);RightAudio->SetVolumeMultiplier(Gain);
    if(!Audible)
    {
        if(bSoundActive){Audio->Stop();RightAudio->Stop();bSoundActive=false;}
        if(Wave)Wave->FlushSamples();if(RightWave)RightWave->FlushSamples();
    }
    if(Wave && PCM.Num() && Audible)
    {
        if(Wave->PendingBytes()>19200 || (RightWave && RightWave->PendingBytes()>19200))
        {Wave->FlushSamples();if(RightWave)RightWave->FlushSamples();}
        if(IsTheatre())
        {
            TArray<int16> Left,Right;Left.SetNumUninitialized(PCM.Num()/2);Right.SetNumUninitialized(PCM.Num()/2);
            for(int32 I=0;I<Left.Num();++I){Left[I]=PCM[I*2];Right[I]=PCM[I*2+1];}
            Wave->PushSamples(reinterpret_cast<const uint8*>(Left.GetData()),Left.Num()*sizeof(int16));
            RightWave->PushSamples(reinterpret_cast<const uint8*>(Right.GetData()),Right.Num()*sizeof(int16));
        }
        else Wave->PushSamples(reinterpret_cast<const uint8*>(PCM.GetData()),PCM.Num()*sizeof(int16));
        if(!bSoundActive || !Audio->IsPlaying()){Audio->Play();if(IsTheatre())RightAudio->Play();bSoundActive=true;}
    }
    for(int32 I=0;I<CinemaLights.Num();++I)
    {
        auto* L=CinemaLights[I].Get();L->SetVisibility(bCinema && bInDistrict);
        if(!bCinema || !bInDistrict)continue;
        const FVector Pos=I<6?FVector(I%2?-1010:1010,-750+(I/2)*650,360):(I==8?FVector(1150,-1850,340):FVector(I==6?-600:300,-1400,310));
        L->SetWorldLocation(Auditorium.TransformPosition(Pos));
        L->SetIntensity(I<6?24000:(I==8?350000:150000)*(G && G->DayCycle?G->DayCycle->InteriorLightScale():1.f));
    }
    const float Luminance=bCinema?80.f:6000.f*(G && G->DayCycle?G->DayCycle->EmissionScale():1.f);
    Panel->SetTintColorAndOpacity(FLinearColor(Luminance,Luminance,Luminance,1));
    if(!bCinema)BackPanel->SetTintColorAndOpacity(FLinearColor(Luminance,Luminance,Luminance,1));

}
void AEWMediaScreen::EndPlay(const EEndPlayReason::Type Reason)
{
    EWShutdownTrace(bCinema?TEXT("Cinema.EndPlay.begin"):TEXT("Plaza.EndPlay.begin"));
    CloseControls();Tablet->SetSlateWidget(nullptr);TabletView.Reset();if(TabletCamera)TabletCamera->Destroy();
    Stop();
    if(auto Device=GetWorld()->GetAudioDevice())
    {
        if(OutputProbe)Device->UnregisterSubmixBufferListener(OutputProbe.ToSharedRef(),*Submix);
        if(MainProbe)Device->UnregisterSubmixBufferListener(MainProbe.ToSharedRef(),Device->GetMainSubmixObject());
        if(Submix)Device->UnregisterSoundSubmix(Submix,false);
    }
    if(Submix)
    {
        // UE 5.7 roots registered submixes but does not unroot them on unregister.
        // Finish audio-thread registration/removal before releasing this actor's
        // private submix, or PIE teardown tries to destroy a rooted world object.
        FAudioCommandFence Fence;Fence.BeginFence();Fence.Wait();
        EWShutdownTrace(TEXT("Media.AudioFence.end"));
        if(Submix->IsRooted())Submix->RemoveFromRoot();
    }
    OutputProbe.Reset();MainProbe.Reset();Panel->SetSlateWidget(nullptr);BackPanel->SetSlateWidget(nullptr);if(Surface)Surface->Shutdown();Surface.Reset();Super::EndPlay(Reason);
    EWShutdownTrace(TEXT("Media.EndPlay.end"));
}
TSharedRef<FJsonObject> AEWMediaScreen::Evidence() const
{
    auto O=Surface?Surface->Evidence():MakeShared<FJsonObject>();O->SetBoolField(TEXT("in_district"),bInDistrict);
    O->SetBoolField(TEXT("tablet_open"),bTabletOpen);O->SetBoolField(TEXT("tablet_nearby"),Nearby());O->SetBoolField(TEXT("tablet_can_control"),CanControlPlayback());
    O->SetBoolField(TEXT("tablet_world_widget"),Tablet && Tablet->GetWidgetSpace()==EWidgetSpace::World);
    O->SetBoolField(TEXT("tablet_render_target"),Tablet && Tablet->GetRenderTarget());O->SetBoolField(TEXT("tablet_hardware_input"),Tablet && Tablet->GetReceiveHardwareInput());
    O->SetBoolField(TEXT("tablet_hit_enabled"),Tablet && Tablet->GetCollisionEnabled()==ECollisionEnabled::QueryOnly);
    O->SetStringField(TEXT("tablet_position"),TabletAnchor->GetComponentLocation().ToString());O->SetStringField(TEXT("tablet_station"),Station.ToString());
    O->SetStringField(TEXT("tablet_query"),TabletView?TabletView->QueryText():FString());
    O->SetBoolField(TEXT("world_widget"),Panel && Panel->GetWidgetSpace()==EWidgetSpace::World);
    O->SetBoolField(TEXT("render_target"),Panel && Panel->GetRenderTarget());
    O->SetBoolField(TEXT("back_render_target"),BackPanel && BackPanel->GetRenderTarget());
    O->SetBoolField(TEXT("two_readable_faces"),Panel && BackPanel && FVector::DotProduct(Panel->GetForwardVector(),BackPanel->GetForwardVector())<-.999
        && BackPanel->GetRelativeLocation().X<-52.5 && BackPanel->GetDrawSize()==Panel->GetDrawSize());
    O->SetNumberField(TEXT("browser_instances"),Surface.IsValid()?1:0);
    O->SetNumberField(TEXT("audio_sources"),IsTheatre()?2:1);
    O->SetBoolField(TEXT("sky_theatre"),bSkyTheatre);O->SetNumberField(TEXT("screen_width_cm"),ScreenWidth());
    O->SetBoolField(TEXT("cinema"),bCinema);O->SetNumberField(TEXT("room_gain"),RoomGain);
    O->SetNumberField(TEXT("right_queued_audio_bytes"),RightWave?RightWave->PendingBytes():0);
    O->SetBoolField(TEXT("right_playing"),RightAudio && RightAudio->IsPlaying());
    O->SetNumberField(TEXT("left_gain"),Audio->VolumeMultiplier);O->SetNumberField(TEXT("right_gain"),RightAudio->VolumeMultiplier);
    O->SetStringField(TEXT("right_source_position"),RightAudio->GetComponentLocation().ToString());
    O->SetBoolField(TEXT("widget_tick"),Panel && Panel->IsComponentTickEnabled());
    O->SetStringField(TEXT("widget_material"),Panel && Panel->GetMaterial(0)?Panel->GetMaterial(0)->GetPathName():TEXT("missing"));
    if(auto* MI=Panel->GetMaterialInstance())
    {UTexture* T=nullptr;FLinearColor Tint;MI->GetTextureParameterValue(FMaterialParameterInfo(TEXT("SlateUI")),T);MI->GetVectorParameterValue(FMaterialParameterInfo(TEXT("TintColorAndOpacity")),Tint);O->SetStringField(TEXT("widget_bound_texture"),T?T->GetPathName():TEXT("missing"));O->SetStringField(TEXT("widget_tint"),Tint.ToString());}
    O->SetBoolField(TEXT("spatialized"),Audio && Audio->bAllowSpatialization && Audio->AttenuationOverrides.bSpatialize);
    O->SetBoolField(TEXT("occlusion"),Audio && Audio->AttenuationOverrides.bEnableOcclusion);
    O->SetBoolField(TEXT("audio_playing"),Audio && Audio->IsPlaying());O->SetNumberField(TEXT("volume"),UserVolume);
    O->SetNumberField(TEXT("queued_audio_bytes"),Wave?Wave->PendingBytes():0);
    if(OutputProbe)O->SetObjectField(TEXT("source_output"),OutputProbe->Evidence());
    if(MainProbe)O->SetObjectField(TEXT("main_output"),MainProbe->Evidence());
    if(auto Device=GetWorld()->GetAudioDevice())
    {
        const auto& Info=static_cast<Audio::FMixerDevice*>(Device.GetAudioDevice())->GetPlatformDeviceInfo();
        O->SetStringField(TEXT("output_device"),Info.Name);O->SetNumberField(TEXT("output_channels"),Info.NumChannels);
    }
    if(auto* PC=UGameplayStatics::GetPlayerController(this,0))if(PC->PlayerCameraManager)
    {
        const FVector Eye=PC->PlayerCameraManager->GetCameraLocation();
        O->SetStringField(TEXT("listener_camera"),Eye.ToString());O->SetStringField(TEXT("audio_position"),Audio->GetComponentLocation().ToString());
        FHitResult Hit;FCollisionQueryParams Query;Query.AddIgnoredActor(this);if(PC->GetPawn())Query.AddIgnoredActor(PC->GetPawn());
        const bool Blocked=GetWorld()->LineTraceSingleByChannel(Hit,Audio->GetComponentLocation(),Eye,ECC_Visibility,Query);
        O->SetBoolField(TEXT("listener_trace_blocked"),Blocked);
        O->SetStringField(TEXT("listener_trace_component"),Hit.GetComponent()?Hit.GetComponent()->GetPathName():TEXT(""));
        O->SetStringField(TEXT("listener_trace_hit"),Hit.ImpactPoint.ToString());
        O->SetNumberField(TEXT("distance_gain"),Audio->AttenuationOverrides.Evaluate(Audio->GetComponentTransform(),Eye));
    }
    O->SetStringField(TEXT("position"),GetActorLocation().ToString());return O;
}
void AEWMediaScreen::CaptureSurface(const FString& Path) const
{
    if(auto* RT=Panel->GetRenderTarget())if(auto* Resource=RT->GameThread_GetRenderTargetResource())
    {TArray<FColor> Pixels;if(Resource->ReadPixels(Pixels)){TArray<uint8> PNG;FImageUtils::CompressImageArray(RT->SizeX,RT->SizeY,Pixels,PNG);FFileHelper::SaveArrayToFile(PNG,*Path);}}
}
void AEWMediaScreen::CaptureBackSurface(const FString& Path) const
{
    if(auto* RT=BackPanel->GetRenderTarget())if(auto* Resource=RT->GameThread_GetRenderTargetResource())
    {TArray<FColor> Pixels;if(Resource->ReadPixels(Pixels)){TArray<uint8> PNG;FImageUtils::CompressImageArray(RT->SizeX,RT->SizeY,Pixels,PNG);FFileHelper::SaveArrayToFile(PNG,*Path);}}
}

bool AEWMediaScreen::AudibleAtListener() const
{
    if(!bSoundActive || !Audio || Audio->VolumeMultiplier<=.001f)return false;
    if(IsTheatre())return RoomGain>0;
    const auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0);
    return Camera && Audio->AttenuationOverrides.Evaluate(Audio->GetComponentTransform(),Camera->GetCameraLocation())>.01f;
}
bool AEWMediaScreen::ListenerInside() const
{
    const auto* Camera=UGameplayStatics::GetPlayerCameraManager(this,0);
    return IsTheatre() && bInDistrict && Camera && (bSkyTheatre?EWSkyTheatrePlan::AudioGain(Auditorium.InverseTransformPosition(Camera->GetCameraLocation())):EWCinemaPlan::AudioGain(Auditorium.InverseTransformPosition(Camera->GetCameraLocation())))>0;
}
int32 AEWMediaScreen::SeatNearby() const
{
    const auto* P=UGameplayStatics::GetPlayerPawn(this,0);if(!IsTheatre() || !bInDistrict || !P)return INDEX_NONE;
    const auto Local=Auditorium.InverseTransformPosition(P->GetActorLocation());const auto Seats=bSkyTheatre?EWSkyTheatrePlan::Seats():EWCinemaPlan::Seats();
    double Best=180;int32 Found=INDEX_NONE;
    for(int32 I=0;I<Seats.Num();++I)
    {
        const double D=FVector::Dist2D(Local,Seats[I].Position);
        if(D<Best && FMath::Abs(Local.Z-Seats[I].Position.Z)<140){Best=D;Found=I;}
    }
    return Found;
}
bool AEWMediaScreen::SitNearest(){return SitSeat(SeatNearby());}
bool AEWMediaScreen::SitSeat(int32 Index)
{
    auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));const auto Seats=bSkyTheatre?EWSkyTheatrePlan::Seats():EWCinemaPlan::Seats();
    if(!IsTheatre() || !bInDistrict || !P || !Seats.IsValidIndex(Index))return false;
    if(auto* G=GetGameInstance<UEWGameInstance>())if(bCinema && G->CinemaSession && G->CinemaSession->Connected() && G->CinemaSession->Seat()!=Index)
    {G->CinemaSession->ReserveSeat(Index);return true;}
    const auto& S=Seats[Index];const FVector Position=Auditorium.TransformPosition(S.Position);
    const FRotator Look=(Auditorium.TransformPosition(bSkyTheatre?EWSkyTheatrePlan::Screen():EWCinemaPlan::Screen())-(Position+FVector(0,0,47))).Rotation();
    return P->SitAtTransform(this,FTransform(Look,Position),FTransform(FRotator(0,Look.Yaw,0),Auditorium.TransformPosition(S.Stand)));
}
