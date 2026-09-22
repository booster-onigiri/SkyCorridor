#include "EWCinemaSession.h"
#include "Framework/Application/SlateApplication.h"
#include "EWGameInstance.h"
#include "EWCharacter.h"
#include "EWChunkManager.h"
#include "EWMediaScreen.h"
#include "EWBrowserSurface.h"
#include "EWDayCycle.h"
#include "EWWorldClock.h"
#include "EWSaveStore.h"
#include "WebSocketsModule.h"
#include "IWebSocket.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Components/StaticMeshComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/StaticMesh.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/CoreStyle.h"
#include "Dom/JsonObject.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"
#include "HAL/FileManager.h"
#include "Async/Async.h"
#include "Misc/App.h"
#include "UnrealClient.h"
#include "GameFramework/CharacterMovementComponent.h"

AEWCinemaPeer::AEWCinemaPeer()
{
    PrimaryActorTick.bCanEverTick=false;
    Body=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GuestBody"));SetRootComponent(Body);
    Body->SetMobility(EComponentMobility::Movable);Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Nameplate=CreateDefaultSubobject<UWidgetComponent>(TEXT("GuestName"));Nameplate->SetupAttachment(Body);
    Nameplate->SetWidgetSpace(EWidgetSpace::World);Nameplate->SetDrawSize(FVector2D(512,80));Nameplate->SetPivot(FVector2D(.5,.5));
    Nameplate->SetBlendMode(EWidgetBlendMode::Transparent);Nameplate->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Nameplate->SetRelativeScale3D(FVector(.16,.16,.16));Nameplate->SetTickWhenOffscreen(false);Nameplate->SetRedrawTime(.1f);
    Nameplate->SetTintColorAndOpacity(FLinearColor(65,65,65,1));
}
void AEWCinemaPeer::BeginPlay()
{
    Super::BeginPlay();StandingMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/EndlessWorld/Kit/SM_CinemaGuestStanding.SM_CinemaGuestStanding"));
    SeatedMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/EndlessWorld/Kit/SM_CinemaGuestSeated.SM_CinemaGuestSeated"));
    Body->SetStaticMesh(StandingMesh);
    Nameplate->SetSlateWidget(SNew(STextBlock).Font(FSlateFontInfo(FPaths::ProjectContentDir()/TEXT("Fonts/DroidSansFallback.ttf"),28))
        .Justification(ETextJustify::Center).ColorAndOpacity(FLinearColor(.7,.9,.84,1))
        .Text_Lambda([this]{return FText::FromString(DisplayName);}));
}
void AEWCinemaPeer::UpdatePose(const FTransform& Frame,FVector Position,float Yaw,bool Seated,bool Visible,const FString& Name,float Delta)
{
    DisplayName=Name;SetActorHiddenInGame(!Visible);if(!Visible){bPlaced=false;return;}
    Position.Z-=Seated?95:88;const FVector Target=Frame.TransformPosition(Position);
    const FRotator Rotation(0,Frame.Rotator().Yaw+Yaw-90,0);
    SetActorLocationAndRotation(bPlaced?FMath::VInterpTo(GetActorLocation(),Target,Delta,14):Target,
        bPlaced?FMath::RInterpTo(GetActorRotation(),Rotation,Delta,14):Rotation);
    SetActorScale3D(Frame.GetScale3D());Body->SetStaticMesh(Seated?SeatedMesh:StandingMesh);bPlaced=true;
    const FVector Head=Frame.TransformPosition(Position+FVector(0,0,Seated?162:205));Nameplate->SetWorldLocation(Head);
    if(auto* C=UGameplayStatics::GetPlayerCameraManager(this,0))Nameplate->SetWorldRotation((C->GetCameraLocation()-Head).Rotation());
}
AEWCinemaSession::AEWCinemaSession(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;}
void AEWCinemaSession::BeginPlay()
{
    Super::BeginPlay();FString NameArg,JoinArg;
    if(FParse::Param(FCommandLine::Get(),TEXT("EWOnlineAudit")) && FParse::Value(FCommandLine::Get(),TEXT("EWOnlineControl="),AuditControl))
    {AuditStarted=FPlatformTime::Seconds();FApp::SetUnfocusedVolumeMultiplier(1);FApp::SetVolumeMultiplier(1);}
    if(FParse::Value(FCommandLine::Get(),TEXT("EWOnlineName="),NameArg))DisplayName=NameArg.Left(20);
    if(FParse::Param(FCommandLine::Get(),TEXT("EWOnlineHost")))Host(DisplayName);
    else if(FParse::Value(FCommandLine::Get(),TEXT("EWOnlineJoin="),JoinArg))Join(JoinArg,DisplayName);
}
void AEWCinemaSession::Host(const FString& Name)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("EWEnableExperimentalOnline")))
    {Message=TEXT("この公開版は一人用です。オンライン機能は開発者向けの実験機能です。");return;}
    Leave();DisplayName=Name.TrimStartAndEnd().Left(20);if(DisplayName.IsEmpty())DisplayName=TEXT("旅人");
    auto* G=GetGameInstance<UEWGameInstance>();if(!G || !G->Store())return;
    const FString Runtime=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("CinemaOnline"));
    const FString Node=Runtime/TEXT("node.exe"),Server=Runtime/TEXT("server.mjs");
    if(!FPaths::FileExists(Node) || !FPaths::FileExists(Server)){Message=TEXT("オンライン用ファイルがありません。最新の探索用実行版を使ってください。");return;}
    InfoPath=G->Store()->Root()/TEXT("Online")/FGuid::NewGuid().ToString(EGuidFormats::Digits)/TEXT("session.json");
    const double Hour=G->DayCycle?G->DayCycle->Evidence()->GetNumberField(TEXT("game_hour")):10;
    FString Args=FString::Printf(TEXT("\"%s\" --info=\"%s\" --parent=%u --hour=%.6f --day-seconds=%.0f"),*Server,*InfoPath,FPlatformProcess::GetCurrentProcessId(),Hour,EWWorldClock::DaySeconds);
    if(FParse::Param(FCommandLine::Get(),TEXT("EWOnlineLocalOnly")))Args+=TEXT(" --local-only");
    ServerProcess=FPlatformProcess::CreateProc(*Node,*Args,false,true,true,nullptr,0,*Runtime,nullptr,nullptr);
    if(!ServerProcess.IsValid()){Message=TEXT("部屋を作れませんでした。もう一度お試しください。");return;}
    bHosting=true;Started=FPlatformTime::Seconds();Message=TEXT("部屋を開いています… 招待コードを準備中です。");G->VisitCinema();
}
void AEWCinemaSession::Join(const FString& Code,const FString& Name)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("EWEnableExperimentalOnline")))
    {Message=TEXT("この公開版は一人用です。オンライン機能は開発者向けの実験機能です。");return;}
    TArray<FString> Parts;Code.TrimStartAndEnd().ParseIntoArray(Parts,TEXT("|"),false);
    if(Parts.Num()!=3 || Parts[0]!=TEXT("ewcinema1") || Parts[2].Len()!=48)
    {Message=TEXT("招待コード全体を貼り付けてください。");return;}
    for(const TCHAR C:Parts[2])if(!FChar::IsHexDigit(C)){Message=TEXT("招待コードが正しくありません。");return;}
    FString Endpoint=Parts[1];
    if(Endpoint.StartsWith(TEXT("https://")))Endpoint=TEXT("wss://")+Endpoint.Mid(8);
    else if(Endpoint.StartsWith(TEXT("http://127.0.0.1:")))Endpoint=TEXT("ws://")+Endpoint.Mid(7);
    else{Message=TEXT("暗号化された招待用の接続先を使ってください。");return;}
    if(Endpoint.Contains(TEXT("?")) || Endpoint.Contains(TEXT("#")) || Endpoint.Contains(TEXT("@")) || Endpoint.Len()>240)
    {Message=TEXT("招待用の接続先が正しくありません。");return;}
    Leave();DisplayName=Name.TrimStartAndEnd().Left(20);if(DisplayName.IsEmpty())DisplayName=TEXT("旅人");InviteCode=Code.TrimStartAndEnd();
    Connect(Endpoint,Parts[2],false);if(auto* G=GetGameInstance<UEWGameInstance>())G->VisitCinema();
}
void AEWCinemaSession::Connect(const FString& Endpoint,const FString& Key,bool HostRole)
{
    bHosting=HostRole;bConnecting=true;Started=FPlatformTime::Seconds();Message=TEXT("上映室に接続しています…");
    const FString URL=Endpoint+(HostRole?TEXT("/host?key="):TEXT("/join?key="))+Key+TEXT("&name=")+FGenericPlatformHttp::UrlEncode(DisplayName);
    Socket=FWebSocketsModule::Get().CreateWebSocket(URL,TEXT(""));const TWeakObjectPtr<AEWCinemaSession> Weak(this);const int32 Generation=++ConnectionGeneration;
    Socket->OnMessage().AddLambda([Weak,Generation](const FString& Text){if(Text.Len()>64000)return;AsyncTask(ENamedThreads::GameThread,[Weak,Generation,Text]{if(Weak.IsValid() && !Weak->bEnding && Weak->ConnectionGeneration==Generation)Weak->Receive(Text);});});
    Socket->OnConnectionError().AddLambda([Weak,Generation](const FString&){AsyncTask(ENamedThreads::GameThread,[Weak,Generation]{if(Weak.IsValid() && !Weak->bEnding && Weak->ConnectionGeneration==Generation){Weak->bConnected=false;Weak->bConnecting=false;Weak->Message=TEXT("接続できませんでした。ホストが部屋を開いているか、招待コードを確認してください。");}});});
    Socket->OnClosed().AddLambda([Weak,Generation](int32,const FString&,bool){AsyncTask(ENamedThreads::GameThread,[Weak,Generation]{if(Weak.IsValid() && !Weak->bEnding && Weak->ConnectionGeneration==Generation){Weak->bConnected=false;Weak->bConnecting=false;Weak->Message=TEXT("上映室との接続が終了しました。招待コードで入り直せます。");}});});
    Socket->Connect();
}
void AEWCinemaSession::Send(const TSharedRef<FJsonObject>& Json)
{if(!Socket || !Socket->IsConnected())return;FString Text;FJsonSerializer::Serialize(Json,TJsonWriterFactory<>::Create(&Text));Socket->Send(Text);}
void AEWCinemaSession::ReserveSeat(int32 Index)
{if(!bConnected || Index<0 || Index>=32)return;auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("type"),TEXT("seat"));O->SetNumberField(TEXT("index"),Index);Send(O);}
void AEWCinemaSession::ReleaseSeat()
{if(HeldSeat<0)return;auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("type"),TEXT("release"));O->SetNumberField(TEXT("index"),HeldSeat);Send(O);HeldSeat=-1;}
void AEWCinemaSession::Receive(const FString& Text)
{
    TSharedPtr<FJsonObject> O;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),O) || !O)return;
    FString Type;if(!O->TryGetStringField(TEXT("type"),Type))return;
    auto* G=GetGameInstance<UEWGameInstance>();
    if(Type==TEXT("welcome"))
    {
        O->TryGetStringField(TEXT("id"),SelfId);O->TryGetBoolField(TEXT("host"),bHosting);
        bConnected=!SelfId.IsEmpty();bConnecting=false;LastWelcome=FPlatformTime::Seconds();
        Message=bHosting?TEXT("ホストとして参加中。招待コードを共有できます。"):TEXT("参加しました。上映はホストに同期します。");return;
    }
    if(Type==TEXT("pong"))
    {double Echo=0;if(O->TryGetNumberField(TEXT("echo"),Echo))RTT=FMath::Clamp(FPlatformTime::Seconds()-Echo,0.,5.);return;}
    if(Type==TEXT("seat"))
    {
        bool Ok=false;int32 Index=-1;O->TryGetBoolField(TEXT("ok"),Ok);O->TryGetNumberField(TEXT("index"),Index);
        if(Ok && Index>=0 && Index<32){HeldSeat=Index;if(G && G->CinemaScreen && !G->CinemaScreen->SitSeat(Index))ReleaseSeat();}
        else if(G)G->Notify(TEXT("その席には別の人が座っています。空いている席を選んでください。"));return;
    }
    if(Type==TEXT("ended")){Leave();Message=TEXT("ホストが上映室を閉じました。");return;}
    if(Type!=TEXT("state") || !bConnected)return;
    const TArray<TSharedPtr<FJsonValue>>* Peers=nullptr;double Hour=0;
    if(!O->TryGetArrayField(TEXT("peers"),Peers) || Peers->Num()>32 || !O->TryGetNumberField(TEXT("dayHour"),Hour) || !FMath::IsFinite(Hour))return;
    double Period=EWWorldClock::LegacyDaySeconds;
    if(O->HasField(TEXT("daySeconds")) && (!O->TryGetNumberField(TEXT("daySeconds"),Period) || !FMath::IsFinite(Period) || Period<60 || Period>86400))return;
    State=O;PeerCount=Peers->Num();LastState=FPlatformTime::Seconds();ServerHour=Hour;ServerDaySeconds=Period;
    if(!bHosting && G && G->CinemaScreen)
    {
        const TSharedPtr<FJsonObject>* Media=nullptr;double Time=0,Updated=0,ServerTime=0;bool Paused=true;FString Id;
        if(O->TryGetObjectField(TEXT("media"),Media) && (*Media)->TryGetStringField(TEXT("id"),Id)
            && (*Media)->TryGetNumberField(TEXT("time"),Time) && (*Media)->TryGetNumberField(TEXT("updated"),Updated)
            && (*Media)->TryGetBoolField(TEXT("paused"),Paused) && O->TryGetNumberField(TEXT("serverTime"),ServerTime))
        {
            const double Target=Time+(Paused?0:FMath::Clamp((ServerTime-Updated)/1000.+RTT*.5,0.,10.));
            auto Browser=G->CinemaScreen->Browser();if(Browser)
            {
                const auto Actual=Browser->Evidence();const double ActualTime=Actual->GetNumberField(TEXT("video_time"))+(Actual->GetBoolField(TEXT("video_paused"))?0:Actual->GetNumberField(TEXT("video_state_age")));
                SyncError=FMath::Abs(ActualTime-Target);
                Browser->ApplyPlayback(Id,Target,Paused);
            }
        }
    }
}
double AEWCinemaSession::SharedHour() const
{return EWWorldClock::Advance(ServerHour,FPlatformTime::Seconds()-LastState+RTT*.5,ServerDaySeconds);}
void AEWCinemaSession::Tick(float Delta)
{
    Super::Tick(Delta);TickAudit();const double Now=FPlatformTime::Seconds();auto* G=GetGameInstance<UEWGameInstance>();
    if(bHosting && ServerProcess.IsValid())
    {
        if(!FPlatformProcess::IsProcRunning(ServerProcess)){Leave();Message=TEXT("上映室を閉じました。もう一度部屋を作れます。");return;}
        FString Text;TSharedPtr<FJsonObject> Info;
        if(Now-LastInfoRead>=.5 && FFileHelper::LoadFileToString(Text,*InfoPath) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Info) && Info)
        {
            LastInfoRead=Now;
            Info->TryGetStringField(TEXT("invite"),InviteCode);
            if(!Socket && !bConnecting)
            {FString Endpoint,Key;if(Info->TryGetStringField(TEXT("endpoint"),Endpoint) && !Endpoint.IsEmpty() && Info->TryGetStringField(TEXT("hostKey"),Key))Connect(Endpoint,Key,true);}
            FString Error;if(Info->TryGetStringField(TEXT("error"),Error) && !Error.IsEmpty())Message=Error;
        }
        if(InviteCode.IsEmpty() && Now-Started>70 && Message.Contains(TEXT("準備中")))Message=TEXT("招待用の接続先の準備に時間がかかっています…");
    }
    if(bConnecting && Now-Started>25){Leave();Message=TEXT("接続がタイムアウトしました。招待コードを確認してください。");return;}
    if(!bConnected)
    {
        for(auto& Pair:Avatars)if(IsValid(Pair.Value))Pair.Value->SetActorHiddenInGame(true);
        if(!bHosting && State.IsValid() && G && G->CinemaScreen){G->CinemaScreen->Stop();State.Reset();}return;
    }
    if(LastState>0 && Now-LastState>12){Leave();Message=TEXT("通信が途切れたため上映を停止しました。入り直してください。");return;}
    if(!G || !G->CinemaScreen || !G->Manager)return;
    auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));if(!P)return;
    const FTransform Frame=G->CinemaScreen->RoomFrame();const FVector Local=Frame.InverseTransformPosition(P->GetActorLocation());
    if(Now-LastPose>=.1)
    {
        LastPose=Now;if(!P->IsSeated())ReleaseSeat();
        auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("type"),TEXT("pose"));
        O->SetArrayField(TEXT("position"),{MakeShared<FJsonValueNumber>(Local.X),MakeShared<FJsonValueNumber>(Local.Y),MakeShared<FJsonValueNumber>(Local.Z)});
        O->SetNumberField(TEXT("yaw"),P->GetActorRotation().Yaw-Frame.Rotator().Yaw);O->SetNumberField(TEXT("seat"),HeldSeat);
        O->SetBoolField(TEXT("visible"),G->CinemaScreen->Available() && FMath::Abs(Local.X)<1800 && Local.Y>-2300 && Local.Y<1800 && Local.Z>-50 && Local.Z<1600);Send(O);
    }
    if(Now-LastPing>2){LastPing=Now;auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("type"),TEXT("ping"));O->SetNumberField(TEXT("echo"),Now);Send(O);}
    if(bHosting && Now-LastMedia>=.4)
    {
        LastMedia=Now;auto B=G->CinemaScreen->Browser();if(B)
        {
            auto Info=B->Evidence();if(Info->GetBoolField(TEXT("video_ad")))
            {
                // Freeze the film while the host sees an advertisement. Each
                // viewer still sees their own provider-required advertisements.
                auto O=MakeShared<FJsonObject>();const FString Id=Info->GetStringField(TEXT("video_id"));double FilmTime=0;
                const TSharedPtr<FJsonObject>* Media=nullptr;
                if(State && State->TryGetObjectField(TEXT("media"),Media) && (*Media)->GetStringField(TEXT("id"))==Id)FilmTime=(*Media)->GetNumberField(TEXT("time"));
                O->SetStringField(TEXT("type"),TEXT("media"));O->SetStringField(TEXT("id"),Id);O->SetNumberField(TEXT("time"),FilmTime);O->SetBoolField(TEXT("paused"),true);Send(O);
            }
            else
            {
                auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("type"),TEXT("media"));
                O->SetStringField(TEXT("id"),Info->GetBoolField(TEXT("video"))?Info->GetStringField(TEXT("video_id")):TEXT(""));
                O->SetNumberField(TEXT("time"),FMath::Min(172800.,Info->GetNumberField(TEXT("video_time"))+(Info->GetBoolField(TEXT("video_paused"))?0:Info->GetNumberField(TEXT("video_state_age")))));
                O->SetBoolField(TEXT("paused"),Info->GetBoolField(TEXT("video_paused")));Send(O);
            }
        }
    }
    if(State)
    {
        const auto& Peers=State->GetArrayField(TEXT("peers"));TSet<FString> Present;
        for(const auto& Value:Peers)
        {
            const auto Peer=Value->AsObject();if(!Peer)continue;FString Id,Name;double Yaw=0;int32 SeatIndex=-1;bool Visible=false;
            const TArray<TSharedPtr<FJsonValue>>* Position=nullptr;
            if(!Peer->TryGetStringField(TEXT("id"),Id) || Id==SelfId || Id.Len()>32 || !Peer->TryGetArrayField(TEXT("position"),Position) || Position->Num()!=3)continue;
            Peer->TryGetStringField(TEXT("name"),Name);Peer->TryGetNumberField(TEXT("yaw"),Yaw);Peer->TryGetNumberField(TEXT("seat"),SeatIndex);Peer->TryGetBoolField(TEXT("visible"),Visible);
            const FVector Pos((*Position)[0]->AsNumber(),(*Position)[1]->AsNumber(),(*Position)[2]->AsNumber());if(Pos.ContainsNaN() || !FMath::IsFinite(Yaw))continue;
            Present.Add(Id);auto& Avatar=Avatars.FindOrAdd(Id);if(!IsValid(Avatar))Avatar=GetWorld()->SpawnActor<AEWCinemaPeer>();
            Avatar->UpdatePose(Frame,Pos,float(Yaw),SeatIndex>=0,Visible && G->CinemaScreen->Available(),Name.Left(20),Delta);
        }
        for(auto It=Avatars.CreateIterator();It;++It)if(!Present.Contains(It.Key())){if(IsValid(It.Value()))It.Value()->Destroy();It.RemoveCurrent();}
    }
}
void AEWCinemaSession::Leave()
{
    ++ConnectionGeneration;
    if(Socket)
    {
        if(bHosting){auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("type"),TEXT("end"));Send(O);}
        Socket->OnMessage().Clear();Socket->OnClosed().Clear();Socket->OnConnectionError().Clear();Socket->Close();Socket.Reset();
    }
    if(ServerProcess.IsValid()){FPlatformProcess::TerminateProc(ServerProcess,true);FPlatformProcess::CloseProc(ServerProcess);ServerProcess.Reset();}
    for(auto& Pair:Avatars)if(IsValid(Pair.Value))Pair.Value->Destroy();Avatars.Empty();
    if(bConnected)if(auto* G=GetGameInstance<UEWGameInstance>())if(IsValid(G->CinemaScreen))G->CinemaScreen->Stop();
    bConnected=bHosting=bConnecting=false;PeerCount=0;HeldSeat=-1;SelfId.Empty();InfoPath.Empty();InviteCode.Empty();State.Reset();LastState=0;
    Message=TEXT("一人で鑑賞中");
}
void AEWCinemaSession::TickAudit()
{
    if(AuditControl.IsEmpty())return;
    auto* G=GetGameInstance<UEWGameInstance>();if(!G)return;
    if(FPlatformTime::Seconds()-AuditStarted>(FParse::Param(FCommandLine::Get(),TEXT("EWTablet94Audit"))?1800:600)){G->QuitWithoutSaving();return;}
    FString Text;TSharedPtr<FJsonObject> O;int32 Sequence=0;FString Action;
    if(!FFileHelper::LoadFileToString(Text,*AuditControl) || Text.Len()>16000 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),O) || !O
        || !O->TryGetNumberField(TEXT("sequence"),Sequence) || Sequence<=AuditSequence || !O->TryGetStringField(TEXT("action"),Action))return;
    auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));AEWMediaScreen* S=G->CinemaScreen;
    if(!P || !S || !S->Available() || G->Manager->IsTravelling())return;
    AuditSequence=Sequence;bool Ok=true;
    TSharedPtr<FJsonObject> TabletResult;
    if(Action==TEXT("tablet94") && FParse::Param(FCommandLine::Get(),TEXT("EWTablet94Audit")))
    {
        int32 Index=0;FString Op;O->TryGetNumberField(TEXT("screen"),Index);O->TryGetStringField(TEXT("op"),Op);
        AEWMediaScreen* Target=Index==0?G->MediaScreen.Get():Index==1?G->CinemaScreen.Get():Index==2?G->SkyTheatre.Get():nullptr;
        if(!Target)Ok=false;
        else
        {
            if(Op==TEXT("visit")){if(Index==0)G->ReturnToPlaza();else if(Index==1)G->VisitCinema();else G->VisitSkyTheatre();}
            else if(Op==TEXT("tap")){double X=0,Y=0;O->TryGetNumberField(TEXT("x"),X);O->TryGetNumberField(TEXT("y"),Y);Ok=Target->TabletTap(X,Y);}
            else if(Op==TEXT("capture")){FString Path;O->TryGetStringField(TEXT("path"),Path);Target->CaptureTablet(Path);}
            else if(Op==TEXT("enter"))
            {FKeyEvent Key(EKeys::Enter,FModifierKeysState(),0,false,0,13);FSlateApplication::Get().ProcessKeyDownEvent(Key);FSlateApplication::Get().ProcessKeyUpEvent(Key);}
            const FString TabletState=Target->TabletPreview(Op);
            FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TabletState),TabletResult);
        }
    }
    else if(Action==TEXT("seat")){int32 Index=-1;O->TryGetNumberField(TEXT("index"),Index);Ok=S->SitSeat(Index);}
    else if(Action==TEXT("stand"))P->LeaveSeat();
    else if(Action==TEXT("film")){if(Hosting())S->Browser()->TestSyncFilm();else Ok=false;}
    else if(Action==TEXT("seek")){double Seconds=0;bool Paused=true;O->TryGetNumberField(TEXT("seconds"),Seconds);O->TryGetBoolField(TEXT("paused"),Paused);if(Hosting() && FMath::IsFinite(Seconds) && Seconds>=0 && Seconds<172800)S->Browser()->SeekForAudit(Seconds,Paused);else Ok=false;}
    else if(Action==TEXT("search")){FString Query;O->TryGetStringField(TEXT("query"),Query);if(Hosting())S->Search(Query);else Ok=false;}
    else if(Action==TEXT("play")){if(Hosting()){S->Browser()->EnableSound();S->Browser()->SetVideoFullscreen(true);}else Ok=false;}
    else if(Action==TEXT("capture")){FString Path;O->TryGetStringField(TEXT("path"),Path);FScreenshotRequest::RequestScreenshot(Path,true,false);}
    else if(Action==TEXT("surface")){FString Path;O->TryGetStringField(TEXT("path"),Path);S->CaptureSurface(Path);}
    else if(Action==TEXT("move"))
    {
        double X=0,Y=0,Z=0,Yaw=90;O->TryGetNumberField(TEXT("x"),X);O->TryGetNumberField(TEXT("y"),Y);O->TryGetNumberField(TEXT("z"),Z);O->TryGetNumberField(TEXT("yaw"),Yaw);
        P->LeaveSeat();P->ClearHeldInput();P->GetCharacterMovement()->DisableMovement();const FTransform Frame=S->RoomFrame();
        P->SetActorLocationAndRotation(Frame.TransformPosition(FVector(X,Y,Z)),FRotator(0,Frame.Rotator().Yaw+Yaw,0),false,nullptr,ETeleportType::TeleportPhysics);
        if(auto* PC=UGameplayStatics::GetPlayerController(this,0))PC->SetControlRotation(FRotator(0,Frame.Rotator().Yaw+Yaw,0));
    }
    else if(Action==TEXT("menu")){FString Menu;O->TryGetStringField(TEXT("menu"),Menu);if(Menu==TEXT("monitor"))S->OpenControls();else G->SetMenu(Menu==TEXT("online")?EEWMenu::Online:EEWMenu::None);}
    else if(Action==TEXT("leave"))Leave();
    else if(Action==TEXT("join")){FString Code;O->TryGetStringField(TEXT("invite"),Code);Join(Code,DisplayName);}
    else if(Action!=TEXT("snapshot") && Action!=TEXT("quit"))Ok=false;
    auto Reply=MakeShared<FJsonObject>();Reply->SetNumberField(TEXT("sequence"),Sequence);Reply->SetBoolField(TEXT("success"),Ok);
    if(TabletResult)Reply->SetObjectField(TEXT("tablet"),TabletResult);
    Reply->SetObjectField(TEXT("online"),Evidence());Reply->SetObjectField(TEXT("cinema"),S->Evidence());Reply->SetBoolField(TEXT("seated"),P->IsSeated());
    FJsonSerializer::Serialize(Reply,TJsonWriterFactory<>::Create(&Text));FFileHelper::SaveStringToFile(Text,*(AuditControl+TEXT(".reply.json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    if(Action==TEXT("quit")){G->QuitWithoutSaving();FPlatformMisc::RequestExit(false);}
}
void AEWCinemaSession::EndPlay(const EEndPlayReason::Type Reason)
{bEnding=true;Leave();Super::EndPlay(Reason);}
TSharedRef<FJsonObject> AEWCinemaSession::Evidence() const
{
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("connected"),bConnected);O->SetBoolField(TEXT("hosting"),bHosting);
    O->SetNumberField(TEXT("members"),PeerCount);O->SetNumberField(TEXT("remote_avatars"),Avatars.Num());O->SetNumberField(TEXT("seat"),HeldSeat);
    O->SetNumberField(TEXT("round_trip_ms"),RTT*1000);O->SetNumberField(TEXT("sync_error_seconds"),SyncError);O->SetNumberField(TEXT("shared_hour"),HasSharedHour()?SharedHour():-1);
    O->SetBoolField(TEXT("invite_ready"),!InviteCode.IsEmpty());O->SetStringField(TEXT("status"),Message);return O;
}
