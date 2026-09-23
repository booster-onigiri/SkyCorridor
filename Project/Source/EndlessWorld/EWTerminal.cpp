#include "EWTerminal.h"
#include "EWLocalization.h"
#include "EWMediaPolicy.h"
#include "EWTerminalView.h"
#include "EWSky92Plan.h"
#include "Camera/CameraActor.h"
#include "EWGameInstance.h"
#include "EWCharacter.h"
#include "EWChunkManager.h"
#include "EWDayCycle.h"
#include "EWBrowserSurface.h"
#include "EWBrowserAudioWave.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/AudioComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "ImageUtils.h"
#include "RHI.h"
#include "StaticMeshResources.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SWindow.h"
#include "Widgets/SViewport.h"
#include "Framework/Application/SlateApplication.h"
#include "Components/LocalFogVolumeComponent.h"
#include "EngineUtils.h"
#include "SceneInterface.h"

UEWTerminalScreen::UEWTerminalScreen(){bReceiveHardwareInput=true;}
void UEWTerminalScreen::BindToViewport()
{
    if(auto* G=GetWorld()->GetGameInstance())if(auto* V=G->GetGameViewportClient())
    {
        RegisterHitTesterWithViewport(V->GetGameViewportWidget());
        if(auto Window=GetSlateWindow())Window->AssignParentWidget(V->GetGameViewportWidget());
    }
}
AEWTerminal::AEWTerminal()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;
    Root=CreateDefaultSubobject<USceneComponent>(TEXT("HeldMemoryDevice"));SetRootComponent(Root);
    Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CeramicTerminal"));Body->SetupAttachment(Root);
    Body->SetRelativeRotation(FRotator(0,90,0));
    Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);Body->SetCastShadow(false);
    Screen=CreateDefaultSubobject<UEWTerminalScreen>(TEXT("TouchScreen"));Screen->SetupAttachment(Root);
    Screen->SetWidgetSpace(EWidgetSpace::World);Screen->SetDrawSize(FVector2D(720,1440));Screen->SetPivot(FVector2D(.5,.5));
    Screen->SetBlendMode(EWidgetBlendMode::Masked);Screen->SetBackgroundColor(FLinearColor::Transparent);Screen->SetTwoSided(false);
    Screen->SetRelativeLocation(FVector(-1.01,0,0));Screen->SetRelativeRotation(FRotator(0,180,0));
    Screen->SetRelativeScale3D(FVector(1,.012,.012));Screen->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Screen->SetCollisionResponseToAllChannels(ECR_Ignore);Screen->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
    Screen->SetTickWhenOffscreen(true);Screen->SetRedrawTime(1.f/30.f);
    Observation=CreateDefaultSubobject<UPostProcessComponent>(TEXT("FadedObservation"));Observation->SetupAttachment(Root);
    Observation->bUnbound=true;Observation->Priority=500;Observation->BlendWeight=0;
    auto& S=Observation->Settings;S.bOverride_ColorSaturation=true;S.ColorSaturation=FVector4(.67,.70,.73,1);
    S.bOverride_VignetteIntensity=true;S.VignetteIntensity=.30;
    Audio=CreateDefaultSubobject<UAudioComponent>(TEXT("PrivateTerminalSound"));Audio->SetupAttachment(Root);
    Audio->bAutoActivate=false;Audio->bAllowSpatialization=false;
}
void AEWTerminal::BeginPlay()
{
    Super::BeginPlay();bSilent=FParse::Param(FCommandLine::Get(),TEXT("EWSilentAudit")) || FParse::Param(FCommandLine::Get(),TEXT("nosound"));
    Body->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/EndlessWorld/Kit/SM_Sky92Phone.SM_Sky92Phone")));
    Surface=MakeShared<FEWBrowserSurface>();
    Wave=NewObject<UEWBrowserAudioWave>(this);Wave->SetSampleRate(48000);Wave->NumChannels=1;Wave->Duration=INDEFINITELY_LOOPING_DURATION;
    Wave->bLooping=false;Audio->SetSound(Wave);Audio->SetVolumeMultiplier(.65f);
    Journal=MakeUnique<FEWSaveStore>(32);
    if(auto* G=GetGameInstance<UEWGameInstance>())if(G->Store())
        if(!Journal->Open(G->Store()->Root()/TEXT("Remembrance")))G->Notify(Journal->Error(),30);
    View=SNew(SEWTerminalView).Owner(this);Screen->SetSlateWidget(View);
}
void AEWTerminal::BuildPlaces()
{
    Memories=EWMemoryPlan::Build();Found.Reset();
    for(int32 I=0;I<Memories.Num();++I)
    {
        const auto& Place=Memories[I].Place;
        if(Journal && Journal->HasRecord(Place.WorldCode,Place.Id))Found.Add(I);
        auto* Shadow=NewObject<UStaticMeshComponent>(this);Shadow->SetMobility(EComponentMobility::Movable);
        Shadow->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/EndlessWorld/Memory89/SM_Memory89Trace.SM_Memory89Trace")));
        Shadow->SetCollisionEnabled(ECollisionEnabled::NoCollision);Shadow->SetCastShadow(false);Shadow->SetVisibility(false);
        Shadow->RegisterComponent();Shadows.Add(Shadow);
        auto* Material=UMaterialInstanceDynamic::Create(LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/EndlessWorld/Memory89/M_MemoryTrace.M_MemoryTrace")),this);
        Shadow->SetMaterial(0,Material);ShadowMaterials.Add(Material);
    }
    Target=0;while(Found.Contains(Target) && Target<Memories.Num()-1)++Target;
    Refresh();
}
void AEWTerminal::Tick(float Delta)
{
    Super::Tick(Delta);auto* G=GetGameInstance<UEWGameInstance>();auto* M=G?G->Manager.Get():nullptr;
    auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));if(!G || !M || !P)return;
    if(Root->GetAttachParent()!=P->Camera)Root->AttachToComponent(P->Camera,FAttachmentTransformRules::KeepRelativeTransform);
    if(Memories.IsEmpty() && M->ReadyCount()>16)BuildPlaces();
    const bool Opened=G->Menu()==EEWMenu::Terminal && G->SessionStarted() && !M->IsTravelling();
    Raised=FMath::FInterpTo(Raised,Opened?1.f:0.f,Delta,9);
    Root->SetRelativeLocation(FMath::Lerp(FVector(28,5,-35),FVector(26,2,-2.5),Raised));
    Root->SetRelativeRotation(FRotator(1,0,-1));
    Body->SetVisibility(Raised>.015);Screen->SetVisibility(Raised>.015);
    Screen->SetCollisionEnabled(Opened?ECollisionEnabled::QueryOnly:ECollisionEnabled::NoCollision);
    const float L=6000.f*(G->DayCycle?G->DayCycle->InteriorLightScale():1.f);
    Screen->SetTintColorAndOpacity(FLinearColor(L,L,L,1));
    if(Opened && !bHadOpen){Screen->BindToViewport();FocusFirst();}
    if(bHadOpen && !Opened)PauseVideo();
    bHadOpen=Opened;
    const bool Ready=G->SessionStarted() && !M->IsTravelling() && M->Descriptor().Seed==EW::WorldDescriptor::ReferenceWorld().Seed;
    if(Ready && !bIntroShown && !Memories.IsEmpty())
    {bIntroShown=true;G->Notify(EWL::Pick(TEXT("水は、まだ流れている。　Q / 右スティック押込み：記録端末を持ち上げる"), TEXT("The water still flows.  Q / Press right stick: raise your memory device")),18);}
    Scan=FMath::FInterpTo(Scan,Ready && bObserving && !Opened?1.f:0.f,Delta,2.5f);Observation->BlendWeight=Scan;
    Near=INDEX_NONE;double Best=600;
    const FVector Eye=P->Camera->GetComponentLocation();
    for(int32 I=0;I<Memories.Num();++I)
    {
        const auto& Place=Memories[I].Place;
        const FVector At=M->ToRender(Place.Coord,Place.LocalPosition);
        const double Distance=FVector::Dist(P->GetActorLocation(),At+FVector(0,0,88));
        const bool Loaded=Ready && M->ReadyAt(Place.Coord) && Distance<2600;
        bool Seen=false;
        if(Loaded && Scan>.01)
        {
            FCollisionQueryParams Query;Query.AddIgnoredActor(this);Query.AddIgnoredActor(P);FHitResult Hit;
            Seen=!GetWorld()->LineTraceSingleByChannel(Hit,Eye,At+FVector(0,0,100),ECC_Visibility,Query);
            Shadows[I]->SetWorldLocationAndRotation(At,FRotator(0,(Eye-At).Rotation().Yaw-90,0));
            const float Alpha=Scan*FMath::Clamp((2600-Distance)/1400.,0.,1.)*TraceOpacity;
            ShadowMaterials[I]->SetScalarParameterValue(TEXT("Visibility"),Seen?Alpha:0);
            ShadowMaterials[I]->SetVectorParameterValue(TEXT("Glow"),FLinearColor(L*TraceStrength,L*TraceStrength*1.08,L*TraceStrength*.96,1));
            if(Seen && Distance<Best && FVector::DotProduct(P->Camera->GetForwardVector(),(At+FVector(0,0,100)-Eye).GetSafeNormal())>.72)
            {Best=Distance;Near=I;}
        }
        Shadows[I]->SetVisibility(Loaded && Seen && Scan>.01);
    }
    if(Near!=LastObserved){ObservedSeconds=0;LastObserved=Near;}
    if(Near!=INDEX_NONE && Best<270 && Scan>.8)ObservedSeconds+=Delta;
    else ObservedSeconds=0;
    if(Surface)
    {
        Surface->Tick();TArray<int16> Samples;Surface->DrainAudio(Samples);
        if(Opened && Page==3 && !bSilent && Surface->HasAudio())
        {
            if(Wave->PendingBytes()>19200)Wave->FlushSamples();
            if(Samples.Num())Wave->PushSamples(reinterpret_cast<const uint8*>(Samples.GetData()),Samples.Num()*sizeof(int16));
            if(!Audio->IsPlaying())Audio->Play();bSoundPlaying=true;
        }
        else{Audio->Stop();Wave->FlushSamples();bSoundPlaying=false;}
    }
}
void AEWTerminal::Refresh(){if(View)View->Rebuild();}
void AEWTerminal::Open()
{
    if(auto* G=GetGameInstance<UEWGameInstance>())if(G->SessionStarted() && G->Manager && !G->Manager->IsTravelling())
    {G->SetMenu(G->Menu()==EEWMenu::Terminal?EEWMenu::None:EEWMenu::Terminal);Refresh();FocusFirst();}
}
void AEWTerminal::Close(){if(auto* G=GetGameInstance<UEWGameInstance>())G->SetMenu(EEWMenu::None);}
void AEWTerminal::ShowPage(int32 Value)
{
    if(Value<0 || Value>5)return;
    if(Value==3 && !EWMediaPolicy::PlaybackEnabled)
    {if(auto* G=GetGameInstance<UEWGameInstance>())G->Notify(EWMediaPolicy::Unavailable());return;}
    if(Value==4)
    {
        auto* G=GetGameInstance<UEWGameInstance>();
        if(!G || !G->IsMenuAvailable(EEWMenu::City))
        {if(G)G->Notify(EWL::Pick(TEXT("友人機能は開発中です。この公開版では利用できません。"), TEXT("Friends is in development and unavailable in this public build.")));return;}
    }
    if(Page==3 && Value!=3)PauseVideo();Page=Value;Refresh();FocusFirst();
}
void AEWTerminal::ToggleObservation()
{
    bObserving=!bObserving;Close();
    if(auto* G=GetGameInstance<UEWGameInstance>())G->Notify(bObserving?EWL::Pick(TEXT("観測中　近づいて静かに眺め、E で記録する。Q で端末。"), TEXT("Observing  Approach and watch quietly. E: record. Q: device.")) : EWL::Pick(TEXT("観測を終えました。"), TEXT("Observation ended.")),7);
}
bool AEWTerminal::RecordNearby()
{
    if(!bObserving || Near==INDEX_NONE || !Memories.IsValidIndex(Near))return false;
    auto* G=GetGameInstance<UEWGameInstance>();if(!G)return false;
    if(ObservedSeconds<2){G->Notify(EWL::Pick(TEXT("もう少しそばで、静かに眺めてみる。"), TEXT("Stay nearby and watch quietly for a moment longer.")),3);return true;}
    if(Found.Contains(Near)){G->Notify(EWL::Translate(Memories[Near].Moment));return true;}
    if(!Journal || !Journal->Record(Memories[Near].Place)){G->Notify(Journal?Journal->Error():EWL::Pick(TEXT("記録を保存できません。"), TEXT("Unable to save this memory.")),20);return true;}
    Found.Add(Near);G->Notify(EWL::Pick(TEXT("端末に残しました　「"), TEXT("Saved to your device: “"))+EWL::Translate(Memories[Near].Place.Name)+EWL::Pick(TEXT("」　Q で読む"), TEXT("”  Q: read")),9);
    if(Target==Near)for(int32 I=0;I<Memories.Num();++I)if(!Found.Contains(I)){Target=I;break;}
    Refresh();return true;
}
void AEWTerminal::SelectTarget(int32 Index){if(Memories.IsValidIndex(Index)){Target=Index;Refresh();}}
FString AEWTerminal::Hint() const
{
    if(!bObserving || Near==INDEX_NONE || !Memories.IsValidIndex(Near))return {};
    const auto& P=Memories[Near];
    return EWL::Translate(P.Moment)+(ObservedSeconds>=2?(Found.Contains(Near)?EWL::Pick(TEXT("　Q 記録を読む"), TEXT("  Q: read memory")):EWL::Pick(TEXT("　E 端末に記録する"), TEXT("  E: record on device"))):EWL::Pick(TEXT("　そばで、少し眺めてみる"), TEXT("  Stay nearby and watch for a moment")));
}
FString AEWTerminal::RouteText(int32 Index) const
{
    if(!Memories.IsValidIndex(Index))return {};
    const auto& P=Memories[Index];auto* G=GetGameInstance<UEWGameInstance>();auto* M=G?G->Manager.Get():nullptr;
    const auto* Player=UGameplayStatics::GetPlayerPawn(this,0);if(!M || !Player)return EWL::Translate(P.Direction);
    const FVector D=M->ToRender(P.Place.Coord,P.Place.LocalPosition)-Player->GetActorLocation();
    const double Angle=FMath::FindDeltaAngleDegrees(Player->GetActorRotation().Yaw,D.Rotation().Yaw);
    const TCHAR* Bearing=FMath::Abs(Angle)<40?EWL::Pick(TEXT("正面"), TEXT("Ahead")):FMath::Abs(Angle)>140?EWL::Pick(TEXT("後方"), TEXT("Behind you")):Angle>0?EWL::Pick(TEXT("右手"), TEXT("To your right")):EWL::Pick(TEXT("左手"), TEXT("To your left"));
    return EWL::Format(TEXT("%s　直線で約 %.0f m\n%s\n\n%s"), TEXT("%s  About %.0f m in a straight line\n%s\n\n%s"),Bearing,D.Size2D()/100.,D.Z>400?EWL::Pick(TEXT("今いる場所より上の階です。昇降機を利用してください。"), TEXT("On a higher floor. Use an elevator.")):D.Z<-400?EWL::Pick(TEXT("今いる場所より下の階です。"), TEXT("On a lower floor.")):EWL::Pick(TEXT("ほぼ同じ高さにあります。"), TEXT("At roughly the same height.")),*EWL::Translate(P.Direction));
}
void AEWTerminal::SearchVideo(const FString& Query)
{if(EWMediaPolicy::PlaybackEnabled && Surface && !Query.TrimStartAndEnd().IsEmpty()){Surface->Search(Query);Surface->EnableSound();}}
void AEWTerminal::StopVideo(){if(Surface)Surface->Stop();Audio->Stop();Wave->FlushSamples();bSoundPlaying=false;}
void AEWTerminal::PauseVideo(){if(Surface)Surface->Pause(true);Audio->Stop();Wave->FlushSamples();bSoundPlaying=false;}
bool AEWTerminal::MediaAudible() const{return bSoundPlaying && Audio && Audio->IsPlaying();}
void AEWTerminal::InviteFriends()
{
    auto* G=GetGameInstance<UEWGameInstance>();if(!G)return;
    if(!G->IsMenuAvailable(EEWMenu::City))
    {G->Notify(EWL::Pick(TEXT("友人機能は開発中です。この公開版では利用できません。"), TEXT("Friends is in development and unavailable in this public build.")));return;}
    StopVideo();G->SetMenu(EEWMenu::City);
}
TSharedPtr<SWidget> AEWTerminal::FocusWidget() const{return View;}
void AEWTerminal::FocusFirst(){if(View)View->FocusFirst();}
TSharedRef<FJsonObject> AEWTerminal::Evidence() const
{
    auto O=MakeShared<FJsonObject>();O->SetNumberField(TEXT("places"),Memories.Num());O->SetNumberField(TEXT("records"),Found.Num());
    O->SetBoolField(TEXT("observing"),bObserving);O->SetNumberField(TEXT("scan_blend"),Scan);O->SetNumberField(TEXT("raised"),Raised);
    O->SetNumberField(TEXT("near"),Near);O->SetNumberField(TEXT("observed_seconds"),ObservedSeconds);O->SetNumberField(TEXT("page"),Page);
    O->SetBoolField(TEXT("body_mesh"),Body && Body->GetStaticMesh());O->SetBoolField(TEXT("screen_render_target"),Screen && Screen->GetRenderTarget());
    O->SetBoolField(TEXT("hardware_input"),Screen->GetReceiveHardwareInput());O->SetBoolField(TEXT("journal_integrity"),Journal && Journal->Integrity());
    O->SetBoolField(TEXT("media_audible"),MediaAudible());O->SetNumberField(TEXT("audio_pending"),Wave?Wave->PendingBytes():0);
    if(const auto* G=GetGameInstance<UEWGameInstance>())if(G->Manager)O->SetBoolField(TEXT("travelling"),G->Manager->IsTravelling());
    if(auto* P=UGameplayStatics::GetPlayerPawn(this,0))O->SetStringField(TEXT("player"),P->GetActorLocation().ToString());
    if(FSlateApplication::IsInitialized())if(auto Focus=FSlateApplication::Get().GetKeyboardFocusedWidget())O->SetStringField(TEXT("focus_type"),Focus->GetTypeAsString());
    TArray<TSharedPtr<FJsonValue>> Values;
    for(int32 I=0;I<Memories.Num();++I)
    {
        auto V=MakeShared<FJsonObject>();const auto& P=Memories[I];V->SetStringField(TEXT("id"),P.Place.Id);V->SetStringField(TEXT("area"),P.Area);
        V->SetStringField(TEXT("chunk"),P.Place.Coord.Text());V->SetStringField(TEXT("local"),P.Place.LocalPosition.ToString());V->SetBoolField(TEXT("recorded"),Found.Contains(I));
        if(Shadows.IsValidIndex(I))
        {
            const auto* Shadow=Shadows[I].Get();V->SetBoolField(TEXT("visible"),Shadow->IsVisible());
            V->SetStringField(TEXT("transform"),Shadow->GetComponentTransform().ToString());
            V->SetNumberField(TEXT("opacity"),ShadowMaterials[I]->K2_GetScalarParameterValue(TEXT("Visibility")));
            if(const UStaticMesh* Mesh=Shadow->GetStaticMesh())if(const auto* Data=Mesh->GetRenderData())if(Data->LODResources.Num())
            {
                const auto& LOD=Data->LODResources[0];V->SetNumberField(TEXT("triangles"),LOD.GetNumTriangles());
                V->SetNumberField(TEXT("sections"),LOD.Sections.Num());
                V->SetBoolField(TEXT("registered"),Shadow->IsRegistered());
                V->SetNumberField(TEXT("last_render_time"),Shadow->GetLastRenderTimeOnScreen());
            }
        }
        Values.Add(MakeShared<FJsonValueObject>(V));
    }
    O->SetArrayField(TEXT("memories"),Values);return O;
}
FString AEWTerminal::TerminalEvidence() const{FString Text;FJsonSerializer::Serialize(Evidence(),TJsonWriterFactory<>::Create(&Text));return Text;}
void AEWTerminal::CaptureScreen(const FString& Path) const
{
    if(auto* RT=Screen->GetRenderTarget())if(auto* Resource=RT->GameThread_GetRenderTargetResource())
    {TArray<FColor> Pixels;if(Resource->ReadPixels(Pixels)){TArray<uint8> PNG;FImageUtils::CompressImageArray(RT->SizeX,RT->SizeY,Pixels,PNG);FFileHelper::SaveArrayToFile(PNG,*Path);}}
}
bool AEWTerminal::PreviewAt(int32 Index)
{
    if(!GetWorld()->IsPlayInEditor() && !FParse::Param(FCommandLine::Get(),TEXT("EWMemory89Audit")) && !FParse::Param(FCommandLine::Get(),TEXT("EWSky92Audit")))return false;
    if(!Memories.IsValidIndex(Index))return false;
    auto* G=GetGameInstance<UEWGameInstance>();if(!G)return false;
    auto P=Memories[Index].Place;P.LocalPosition=Memories[Index].Approach+FVector(0,0,91);
    P.Yaw=(Memories[Index].Place.LocalPosition-Memories[Index].Approach).Rotation().Yaw;
    G->Visit(P);Target=Index;bObserving=true;return true;
}
bool AEWTerminal::PreviewTap(float X,float Y)
{
    if(!GetWorld()->IsPlayInEditor() && !FParse::Param(FCommandLine::Get(),TEXT("EWMemory89Audit")) && !FParse::Param(FCommandLine::Get(),TEXT("EWSky92Audit")))return false;
    if(Raised<.99 || X<0 || X>720 || Y<0 || Y>1440)return false;
    auto* G=GetGameInstance<UEWGameInstance>();auto* PC=UGameplayStatics::GetPlayerController(this,0);
    auto* V=G?G->GetGameViewportClient():nullptr;if(!PC || !V || !V->GetGameViewportWidget())return false;
    const FVector WorldPoint=Screen->GetComponentTransform().TransformPosition(FVector(0,360-X,720-Y));
    FVector2D Pixel;if(!PC->ProjectWorldLocationToScreen(WorldPoint,Pixel))return false;
    const FGeometry& Geometry=V->GetGameViewportWidget()->GetCachedGeometry();
    const FVector2D Desktop=Geometry.LocalToAbsolute(Pixel/Geometry.Scale);
    auto& App=FSlateApplication::Get();const FVector2D Old=App.GetCursorPos();
    App.SetCursorPos(Desktop);
    FPointerEvent Move(0,Desktop,Old,TSet<FKey>(),EKeys::Invalid,0,FModifierKeysState());
    App.ProcessMouseMoveEvent(Move,false);
    FPointerEvent Down(0,Desktop,Desktop,TSet<FKey>{EKeys::LeftMouseButton},EKeys::LeftMouseButton,0,FModifierKeysState());
    FPointerEvent Up(0,Desktop,Desktop,TSet<FKey>(),EKeys::LeftMouseButton,0,FModifierKeysState());
    App.ProcessMouseButtonDownEvent(nullptr,Down);App.ProcessMouseButtonUpEvent(Up);
    return true;
}
bool AEWTerminal::PreviewTrace(float Strength,float Opacity,float Hour)
{
    if(!GetWorld()->IsPlayInEditor())return false;
    TraceStrength=FMath::Clamp(Strength,0.f,20.f);TraceOpacity=FMath::Clamp(Opacity,.02f,.8f);
    if(auto* G=GetGameInstance<UEWGameInstance>())if(G->DayCycle)G->DayCycle->SetAuditHour(Hour);
    return true;
}
bool AEWTerminal::PreviewLandmark(int32 Index,int32 PreviewView,float Hour)
{
    if(!GetWorld()->IsPlayInEditor() || Index<0 || Index>2)return false;
    auto* G=GetGameInstance<UEWGameInstance>();auto* PC=UGameplayStatics::GetPlayerController(this,0);
    if(!G || !G->Manager || !PC)return false;
    if(PreviewView<0){PC->SetViewTarget(UGameplayStatics::GetPlayerPawn(this,0));return true;}
    const auto C=EWSky92Plan::Coord(Index);if(!G->Manager->ReadyAt(C))return false;
    const auto F=EWSky92Plan::Frame(G->Manager->Descriptor(),Index);FVector Eye,Aim;
    if(Index==0){Eye=PreviewView==0?FVector(0,-7800,4300):FVector(700,-1450,165);Aim=PreviewView==0?FVector(0,0,4300):FVector(-800,-1120,190);}
    else if(Index==1){Eye=PreviewView==0?FVector(5200,-7800,3100):FVector(100,-1200,165);Aim=PreviewView==0?FVector(0,0,1000):FVector(0,1000,550);}
    else{Eye=PreviewView==0?FVector(10500,-15000,6400):FVector(50,-2850,165);Aim=PreviewView==0?FVector(0,0,1500):FVector(0,1300,1150);}
    if(PreviewView==2){Eye=FVector(0,550,165);Aim=FVector(0,1600,650);}
    Eye=G->Manager->ToRender(C,F.TransformPosition(Eye));Aim=G->Manager->ToRender(C,F.TransformPosition(Aim));
    if(!PreviewCamera)PreviewCamera=GetWorld()->SpawnActor<ACameraActor>();
    PreviewCamera->SetActorLocationAndRotation(Eye,(Aim-Eye).Rotation());PreviewCamera->GetCameraComponent()->SetFieldOfView(67);
    PC->SetViewTarget(PreviewCamera);if(G->DayCycle)G->DayCycle->SetAuditHour(Hour);Close();return true;
}
FString AEWTerminal::Sky92FogEvidence() const
{
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("scene_has_volumes"),GetWorld()->Scene && GetWorld()->Scene->HasAnyLocalFogVolume());
    if(auto* V=GetWorld()->GetGameViewport())O->SetBoolField(TEXT("viewport_fog_flag"),V->EngineShowFlags.Fog);
    TArray<TSharedPtr<FJsonValue>> Rows;
    for(TActorIterator<AActor> It(GetWorld());It;++It)
    {
        TArray<ULocalFogVolumeComponent*> Components;It->GetComponents(Components);
        for(const auto* C:Components)
        {
            auto A=MakeShared<FJsonObject>();A->SetStringField(TEXT("path"),C->GetPathName());
            A->SetBoolField(TEXT("registered"),C->IsRegistered());A->SetBoolField(TEXT("render_state"),C->IsRenderStateCreated());
            A->SetBoolField(TEXT("should_render"),C->ShouldRender());A->SetBoolField(TEXT("owner_hidden"),It->IsHidden());
            A->SetBoolField(TEXT("add_to_scene"),C->ShouldComponentAddToScene());Rows.Add(MakeShared<FJsonValueObject>(A));
        }
    }
    O->SetArrayField(TEXT("components"),Rows);FString Text;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Text));return Text;
}
void AEWTerminal::EndPlay(const EEndPlayReason::Type Reason)
{if(Surface)Surface->Shutdown();Audio->Stop();Journal.Reset();View.Reset();Super::EndPlay(Reason);}
