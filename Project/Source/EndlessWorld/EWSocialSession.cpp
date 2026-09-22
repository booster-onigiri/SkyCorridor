#include "EWSocialSession.h"
#include "EWGameInstance.h"
#include "EWCharacter.h"
#include "EWChunkManager.h"
#include "EWCinemaSession.h"
#include "EWDayCycle.h"
#include "EWMediaScreen.h"
#include "EWCinemaPlan.h"
#include "EWBrowserAudioWave.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundAttenuation.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "HAL/PlatformMisc.h"

struct FEWSocialAudioInbox
{
    struct FFrame{FString Id;TArray<int16> Samples;int32 Rate;};
    FCriticalSection Lock;TArray<FFrame> Frames;bool Active=true;
    void Push(const FString& Id,const TArray<int16>& Samples,int32 Rate)
    {
        FScopeLock Guard(&Lock);if(!Active || Frames.Num()>=64 || Samples.Num()>9600 || Id.Len()>64)return;
        Frames.Add({Id,Samples,Rate});
    }
    TArray<FFrame> Drain(){FScopeLock Guard(&Lock);TArray<FFrame> Out=MoveTemp(Frames);Frames.Reset();return Out;}
    void Clear(bool Enabled){FScopeLock Guard(&Lock);Active=Enabled;Frames.Reset();}
};
namespace
{
bool Number(const TSharedPtr<FJsonObject>& O,const TCHAR* Key,double& N,double Min,double Max)
{return O && O->HasTypedField<EJson::Number>(Key) && O->TryGetNumberField(Key,N) && FMath::IsFinite(N) && N>=Min && N<=Max;}
TSharedPtr<FJsonObject> Object(const TSharedPtr<FJsonObject>& O,const TCHAR* Key)
{const TSharedPtr<FJsonObject>* V=nullptr;return O && O->TryGetObjectField(Key,V)?*V:nullptr;}
}
AEWSocialSession::AEWSocialSession(){PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;}
void AEWSocialSession::BeginPlay()
{
    Super::BeginPlay();bSilent=FParse::Param(FCommandLine::Get(),TEXT("EWSilentAudit"));
    auto* G=GetGameInstance<UEWGameInstance>();if(!G || !G->Store())return;
    if(!Store.Open(G->Store()->Root()))G->Notify(Store.Error(),30);
    else if(!Store.Notice().IsEmpty())G->Notify(Store.Notice(),30);
    LocalName=EWSocial::CleanText(Store.Get(TEXT("nickname"),TEXT("旅人")),20);
    Muted=Store.Flags(TEXT("mute:"));Blocked=Store.Flags(TEXT("block:"));Banned=Store.Flags(TEXT("ban:"));
    bQuiet=Store.Get(TEXT("quiet"))==TEXT("1");
    FString Config=FPlatformMisc::GetEnvironmentVariable(TEXT("EW_EOS_CONFIG"));
    if(Config.IsEmpty())Config=FPaths::ProjectDir()/TEXT("Config/EOS.json");
    FParse::Value(FCommandLine::Get(),TEXT("EWEOSConfig="),Config);
    // Public builds stay offline unless a developer explicitly enables experiments.
    if(FParse::Param(FCommandLine::Get(),TEXT("EWEnableExperimentalOnline")))
        Connection.Configure(Config,G->Store()->Root()/TEXT("EOSCache"));
    Connection.OnPacket=[this](const FString& Peer,const FString& Text){Receive(Peer,Text);};
    Connection.OnChanged=[this]{RefreshMenu();};
    Inbox=MakeShared<FEWSocialAudioInbox,ESPMode::ThreadSafe>();const auto Queue=Inbox;
    Connection.OnVoicePCM=[Queue](const FString& Peer,const TArray<int16>& PCM,int32 Rate){Queue->Push(Peer,PCM,Rate);};
}
void AEWSocialSession::EndPlay(const EEndPlayReason::Type Reason)
{
    bEnding=true;Connection.OnChanged=nullptr;Connection.OnPacket=nullptr;
    if(Inbox)Inbox->Clear(false);Connection.Leave();Connection.OnVoicePCM=nullptr;
    ClearSession(false);Store.Close();Super::EndPlay(Reason);
}
void AEWSocialSession::RefreshMenu(){if(!bEnding)if(auto* G=GetGameInstance<UEWGameInstance>())if(G->Menu()==EEWMenu::City)G->RefreshUI();}
void AEWSocialSession::OpenCityMenu(){if(auto* G=GetGameInstance<UEWGameInstance>())G->SetMenu(EEWMenu::City);}
void AEWSocialSession::Remember(const FString& Key,const FString& Value)
{if(!Store.Put(Key,Value))if(auto* G=GetGameInstance<UEWGameInstance>())G->Notify(Store.Error(),20);}
void AEWSocialSession::SetName(const FString& Value)
{LocalName=EWSocial::CleanText(Value,20);if(LocalName.IsEmpty())LocalName=TEXT("旅人");Remember(TEXT("nickname"),LocalName);}
void AEWSocialSession::Authenticate(){LocalStatus.Reset();Connection.Authenticate();}
void AEWSocialSession::FindCities(){LocalStatus.Reset();Connection.Find();}
void AEWSocialSession::HostCity(const FString& Name,const FString& Language,const FString& Activity,const FString& Nick)
{if(Connection.Busy() || Connection.InLobby())return;SetName(Nick);LocalStatus.Reset();Connection.Host(Name,Language,Activity);}
void AEWSocialSession::JoinCity(const FString& Code,const FString& Nick)
{
    if(Connection.Busy() || Connection.InLobby())return;SetName(Nick);LocalStatus.Reset();
    FString Id=Code.TrimStartAndEnd();Id.RemoveFromStart(TEXT("ewcity1|"));Connection.Join(Id);
}
FString AEWSocialSession::Invite() const{return Connection.InLobby()?TEXT("ewcity1|")+Connection.LobbyId():FString();}
FString AEWSocialSession::Status() const{return LocalStatus.IsEmpty()?Connection.Status():LocalStatus;}
void AEWSocialSession::LeaveCity(){Connection.Leave();ClearSession(true);LocalStatus.Reset();RefreshMenu();}
void AEWSocialSession::ClearSession(bool ReturnHome)
{
    const bool WasIn=!SeenLobby.IsEmpty();bActive=false;bPTT=bSending=false;Connection.SetMicrophone(false);SeenLobby.Reset();
    if(Inbox)Inbox->Clear(false);
    for(auto& Pair:Speakers)if(Pair.Value)Pair.Value->DestroyComponent();Speakers.Reset();
    for(auto& Pair:Waves)if(Pair.Value)Pair.Value->FlushSamples();Waves.Reset();
    for(auto& Pair:Avatars)if(Pair.Value)Pair.Value->Destroy();Avatars.Reset();
    Peers.Reset();Audible.Reset();SpeechUntil.Reset();HelloTimes.Reset();LastClock=0;AcousticRegion.Reset();
    if(ReturnHome && WasIn && !bEnding)if(auto* G=GetGameInstance<UEWGameInstance>())
    {G->ReturnToPlaza();G->Notify(TEXT("街から退出し、一人用の時計広場へ戻りました。"));}
}
void AEWSocialSession::Entered()
{
    auto* G=GetGameInstance<UEWGameInstance>();if(!G)return;
    if(G->CinemaSession)G->CinemaSession->Leave();
    if(G->Manager && G->Manager->IsTravelling())return;
    G->ReturnToPlaza();
    if(!G->SessionStarted() || !G->Manager || !G->Manager->IsTravelling())
    {Connection.Leave();LocalStatus=TEXT("時計広場へ移動できませんでした。保存状態を確認して、もう一度参加してください。");return;}
    SeenLobby=Connection.LobbyId();EnteredAt=LastServer=FPlatformTime::Seconds();Sequence=0;
    LastHello=LastPose=LastPing=LastState=0;ClockStarted=EnteredAt;ChatLines.Reset();
    if(Inbox)Inbox->Clear(true);
    StartHour=G->DayCycle?G->DayCycle->Evidence()->GetNumberField(TEXT("game_hour")):10;
    if(Connection.Hosting())
    {
        Authority.Reset(Connection.SelfId(),LocalName,EnteredAt);for(const auto& Id:Banned)Authority.Ban(Id);
        Peers=Authority.Players();bActive=true;LocalStatus=TEXT("街を開きました。招待コードを友達に共有できます。");
    }
    else LocalStatus=TEXT("時計広場と住人を読み込んでいます…");
    RefreshMenu();
}
void AEWSocialSession::Broadcast(const TSharedRef<FJsonObject>& O,bool Reliable)
{const FString Text=EWSocial::Encode(O);for(const auto& Pair:Authority.Players())if(Pair.Key!=Connection.SelfId())Connection.Send(Pair.Key,Text,Reliable);}
void AEWSocialSession::Snapshot(const FString& Recipient)
{
    auto O=EWSocial::Message(TEXT("state"));O->SetNumberField(TEXT("protocol"),EWSocial::Protocol);O->SetStringField(TEXT("build"),EWSocial::Build);
    O->SetNumberField(TEXT("hour"),SharedHour());TArray<TSharedPtr<FJsonValue>> List;
    for(const auto& Pair:Authority.Players())
    {
        const auto& P=Pair.Value;auto V=MakeShared<FJsonObject>();V->SetStringField(TEXT("id"),P.Id);V->SetStringField(TEXT("name"),P.Name);
        if(P.bHasPose)V->SetObjectField(TEXT("pose"),P.Pose.Json());List.Add(MakeShared<FJsonValueObject>(V));
    }
    O->SetArrayField(TEXT("peers"),List);if(Recipient.IsEmpty())Broadcast(O,true);else Connection.Send(Recipient,EWSocial::Encode(O),true);
}
double AEWSocialSession::SharedHour() const
{return Connection.Hosting()?EWSocial::DayHour(StartHour,FPlatformTime::Seconds()-ClockStarted):EWSocial::DayHour(ClockHour,FPlatformTime::Seconds()-LastClock);}
void AEWSocialSession::Receive(const FString& Sender,const FString& Text)
{
    const double Now=FPlatformTime::Seconds();TSharedPtr<FJsonObject> O;FString Type;
    if(!EWSocial::Decode(Text,O) || !O->TryGetStringField(TEXT("type"),Type)){++PacketsRejected;return;}
    if(Connection.Hosting())
    {
        if(Type==TEXT("hello"))
        {
            if(Now-HelloTimes.FindRef(Sender)<.8)return;HelloTimes.Add(Sender,Now);
            double Version;FString Build,Name;
            if(!Number(O,TEXT("protocol"),Version,1,10000) || FMath::FloorToDouble(Version)!=Version ||
                !O->TryGetStringField(TEXT("build"),Build) || !O->TryGetStringField(TEXT("name"),Name) ||
                !Authority.Admit(Sender,Name,int32(Version),Build,Now))
            {++PacketsRejected;Connection.Kick(Sender);return;}
            Snapshot(Sender);Peers=Authority.Players();RefreshMenu();return;
        }
        if(!Authority.Players().Contains(Sender)){++PacketsRejected;return;}
        if(Type==TEXT("pose"))
        {
            EWSocial::FPose Pose;if(!EWSocial::FPose::Read(Object(O,TEXT("pose")),Pose) || !Authority.Pose(Sender,Pose,Now)){++PacketsRejected;return;}
            auto State=EWSocial::Message(TEXT("pose"));State->SetStringField(TEXT("id"),Sender);State->SetObjectField(TEXT("pose"),Pose.Json());Broadcast(State,false);
            Peers=Authority.Players();++PosesReceived;
        }
        else if(Type==TEXT("ping"))
        {
            double Stamp;if(!Number(O,TEXT("stamp"),Stamp,0,1.e12))return;
            auto Pong=EWSocial::Message(TEXT("pong"));Pong->SetNumberField(TEXT("stamp"),Stamp);Pong->SetNumberField(TEXT("hour"),SharedHour());
            Connection.Send(Sender,EWSocial::Encode(Pong),true);
        }
        else if(Type==TEXT("chat"))
        {
            FString Value,Clean;if(!O->TryGetStringField(TEXT("text"),Value) || !Authority.Chat(Sender,Value,Now,Clean)){++PacketsRejected;return;}
            const FString Name=Authority.Players().FindChecked(Sender).Name;auto C=EWSocial::Message(TEXT("chat"));
            C->SetStringField(TEXT("id"),Sender);C->SetStringField(TEXT("name"),Name);C->SetStringField(TEXT("text"),Clean);Broadcast(C,true);AddChat(Sender,Name,Clean);
        }
        else ++PacketsRejected;
        return;
    }
    if(Sender!=Connection.HostId()){++PacketsRejected;return;}
    if(Type==TEXT("state"))
    {
        double Version,Hour;FString Build;const TArray<TSharedPtr<FJsonValue>>* List=nullptr;
        if(!Number(O,TEXT("protocol"),Version,1,10000) || Version!=EWSocial::Protocol || !O->TryGetStringField(TEXT("build"),Build) || Build!=EWSocial::Build ||
            !Number(O,TEXT("hour"),Hour,0,24) || !O->TryGetArrayField(TEXT("peers"),List) || List->Num()>EWSocial::Capacity){++PacketsRejected;return;}
        TMap<FString,EWSocial::FPeer> Updated;
        for(const auto& Value:*List)
        {
            const TSharedPtr<FJsonObject>* V=nullptr;FString Id,Name;
            if(!Value->TryGetObject(V) || !(*V)->TryGetStringField(TEXT("id"),Id) || !Connection.Members().Contains(Id) || Updated.Contains(Id) ||
                !(*V)->TryGetStringField(TEXT("name"),Name)){++PacketsRejected;return;}
            EWSocial::FPeer Peer;if(const auto* Previous=Peers.Find(Id))Peer=*Previous;Peer.Id=Id;Peer.Name=EWSocial::CleanText(Name,20);Peer.LastSeen=Now;
            EWSocial::FPose Pose;if(auto J=Object(*V,TEXT("pose")))
            {
                if(!EWSocial::FPose::Read(J,Pose)){++PacketsRejected;return;}
                if(!Peer.bHasPose || int32(Pose.Sequence-Peer.Pose.Sequence)>0){Peer.Pose=Pose;Peer.LastPose=Now;Peer.bHasPose=true;}
            }
            Updated.Add(Id,Peer);
        }
        if(!Updated.Contains(Connection.SelfId()) || !Updated.Contains(Sender)){++PacketsRejected;return;}
        const bool New=!bActive;const int32 PreviousCount=Peers.Num();Peers=MoveTemp(Updated);bActive=true;LastServer=Now;
        if(LastClock<=0){ClockHour=Hour;LastClock=Now;}
        LocalStatus=TEXT("街に入りました。Vで話す、Tで文字会話と住人一覧を開きます。");
        if(New || PreviousCount!=Peers.Num())RefreshMenu();
    }
    else if(Type==TEXT("pose") && bActive)
    {
        FString Id;EWSocial::FPose Pose;
        if(!O->TryGetStringField(TEXT("id"),Id) || !EWSocial::FPose::Read(Object(O,TEXT("pose")),Pose)){++PacketsRejected;return;}
        auto* P=Peers.Find(Id);if(P && (!P->bHasPose || int32(Pose.Sequence-P->Pose.Sequence)>0))
        {P->Pose=Pose;P->bHasPose=true;P->LastPose=P->LastSeen=Now;++PosesReceived;LastServer=Now;}
    }
    else if(Type==TEXT("pong"))
    {
        double Stamp,Hour;if(!Number(O,TEXT("stamp"),Stamp,0,1.e12) || Stamp!=PendingPing || !Number(O,TEXT("hour"),Hour,0,24) || Now-Stamp>5)return;
        RTT=FMath::Max(0.,Now-Stamp);ClockHour=EWSocial::DayHour(Hour,RTT*.5);LastClock=LastServer=Now;
    }
    else if(Type==TEXT("chat") && bActive)
    {
        FString Id,TextValue;if(O->TryGetStringField(TEXT("id"),Id) && O->TryGetStringField(TEXT("text"),TextValue))
            if(const auto* P=Peers.Find(Id))AddChat(Id,P->Name,EWSocial::CleanText(TextValue,240));
    }
    else ++PacketsRejected;
}
void AEWSocialSession::SendPose(double Now)
{
    auto* G=GetGameInstance<UEWGameInstance>();auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if(!bActive || !G || !G->Manager || !P || P->IsStreamingHeld())return;
    auto Pose=EWSocial::FPose::FromRender(G->Manager->OriginCoord(),P->GetActorLocation(),P->GetControlRotation());
    Pose.Sequence=++Sequence;if(Sequence==0)Pose.Sequence=++Sequence;
    Pose.Motion=P->IsSeated()?TEXT("sit"):P->GetCharacterMovement()->IsFalling()?TEXT("jump"):P->GetVelocity().SizeSquared2D()>16?TEXT("walk"):TEXT("idle");
    if(!Pose.Valid())return;
    auto O=EWSocial::Message(TEXT("pose"));O->SetObjectField(TEXT("pose"),Pose.Json());
    if(Connection.Hosting())
    {
        if(!Authority.Pose(Connection.SelfId(),Pose,Now))return;Peers=Authority.Players();O->SetStringField(TEXT("id"),Connection.SelfId());Broadcast(O,false);
    }
    else Connection.Send(Connection.HostId(),EWSocial::Encode(O),false);
}
void AEWSocialSession::AddChat(const FString& Id,const FString& Name,const FString& Text)
{
    if(Blocked.Contains(Id) || Muted.Contains(Id))return;
    ChatLines.Add(Name+TEXT("：")+Text);if(ChatLines.Num()>60)ChatLines.RemoveAt(0,ChatLines.Num()-60);
    if(auto* G=GetGameInstance<UEWGameInstance>())if(G->Menu()!=EEWMenu::City)G->Notify(Name+TEXT("：")+Text,7);
}
FString AEWSocialSession::ChatText() const{return FString::Join(ChatLines,TEXT("\n"));}
void AEWSocialSession::SendChat(const FString& Text)
{
    if(!bActive)return;auto O=EWSocial::Message(TEXT("chat"));O->SetStringField(TEXT("text"),EWSocial::CleanText(Text,240));
    if(Connection.Hosting())Receive(Connection.SelfId(),EWSocial::Encode(O));else Connection.Send(Connection.HostId(),EWSocial::Encode(O),true);
}
void AEWSocialSession::PushToTalk(bool Pressed){bPTT=Pressed && bActive && !bQuiet && !bSilent;if(!bPTT){bSending=false;Connection.SetMicrophone(false);}}
float AEWSocialSession::PlayerVolume(const FString& Id) const{const auto* V=Volumes.Find(Id);return V?*V:1.f;}
void AEWSocialSession::SetPlayerVolume(const FString& Id,float Value){if(!FMath::IsFinite(Value))return;Volumes.Add(Id,FMath::Clamp(Value,0.f,2.f));Remember(TEXT("volume:")+Id,LexToString(FMath::Clamp(Value,0.f,2.f)));}
void AEWSocialSession::ToggleMute(const FString& Id)
{if(Muted.Contains(Id))Muted.Remove(Id);else Muted.Add(Id);Remember(TEXT("mute:")+Id,Muted.Contains(Id)?TEXT("1"):TEXT("0"));Silence(Id);RefreshMenu();}
void AEWSocialSession::ToggleBlock(const FString& Id)
{if(Blocked.Contains(Id))Blocked.Remove(Id);else Blocked.Add(Id);Remember(TEXT("block:")+Id,Blocked.Contains(Id)?TEXT("1"):TEXT("0"));Silence(Id);RefreshMenu();}
void AEWSocialSession::ToggleQuiet(){bQuiet=!bQuiet;PushToTalk(false);Remember(TEXT("quiet"),bQuiet?TEXT("1"):TEXT("0"));RefreshMenu();}
void AEWSocialSession::KickPlayer(const FString& Id)
{if(!Connection.Hosting() || Id==Connection.SelfId())return;Banned.Add(Id);Remember(TEXT("ban:")+Id,TEXT("1"));Authority.Ban(Id);Connection.Kick(Id);Peers=Authority.Players();Snapshot();RefreshMenu();}
void AEWSocialSession::ReportPlayer(const FString& Id,const FString& Reason)
{
    if(EWSocial::CleanText(Reason,240).IsEmpty()){if(auto* G=GetGameInstance<UEWGameInstance>())G->Notify(TEXT("通報理由を入力してから送信してください。"));return;}
    Connection.Report(Id,Reason);
}
FString AEWSocialSession::Zone(const EWSocial::FPose& Pose) const
{
    const auto* G=GetGameInstance<UEWGameInstance>();if(!G || !G->Manager)return TEXT("unloaded");
    // The recipe frame is global-chunk based, so acoustic membership also survives render rebasing.
    const auto R=G->Manager->RecipeAt({0,0});
    if(R)for(const auto& Room:R->Interiors)if(Room.Kind==10)
        if(EWCinemaPlan::AudioGain(Room.Frame.InverseTransformPosition(Pose.RelativeTo({0,0})+FVector(0,0,74)))>0)return TEXT("cinema");
    return TEXT("outside");
}
void AEWSocialSession::Silence(const FString& Id)
{if(auto* A=Speakers.Find(Id))if(*A){(*A)->SetVolumeMultiplier(0);(*A)->Stop();}if(auto* W=Waves.Find(Id))if(*W)(*W)->FlushSamples();Audible.Remove(Id);SpeechUntil.Remove(Id);}
void AEWSocialSession::UpdatePeople(float Delta,double Now)
{
    auto* G=GetGameInstance<UEWGameInstance>();if(!G || !G->Manager)return;const auto Origin=G->Manager->OriginCoord();
    for(auto It=Avatars.CreateIterator();It;++It)if(!Peers.Contains(It.Key()))
    {Silence(It.Key());if(auto* A=Speakers.Find(It.Key()))if(*A)(*A)->DestroyComponent();Speakers.Remove(It.Key());Waves.Remove(It.Key());It.Value()->Destroy();It.RemoveCurrent();}
    for(const auto& Pair:Peers)
    {
        if(Pair.Key==Connection.SelfId())continue;const auto& P=Pair.Value;
        auto* Avatar=Avatars.FindRef(Pair.Key).Get();if(!Avatar){Avatar=GetWorld()->SpawnActor<AEWCinemaPeer>();Avatars.Add(Pair.Key,Avatar);}
        const bool Visible=P.bHasPose && !Blocked.Contains(Pair.Key) && Now-P.LastPose<5 &&
            FMath::Abs(P.Pose.Chunk.X-Origin.X)<=4 && FMath::Abs(P.Pose.Chunk.Y-Origin.Y)<=4 && G->Manager->ReadyAt(P.Pose.Chunk);
        const FVector Position=P.Pose.RelativeTo(Origin);const bool Sit=P.Pose.Motion==TEXT("sit");
        if(!(LastOrigin==Origin))Avatar->UpdatePose(FTransform::Identity,Position,P.Pose.Yaw,Sit,false,P.Name,Delta);
        Avatar->UpdatePose(FTransform::Identity,Position,P.Pose.Yaw,Sit,Visible,P.Name+(SpeechUntil.FindRef(Pair.Key)>Now?TEXT("  発話中"):TEXT("")),Delta);
    }
    LastOrigin=Origin;
}
void AEWSocialSession::UpdateVoice(double Now)
{
    auto* G=GetGameInstance<UEWGameInstance>();auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerPawn(this,0));
    if(!G || !G->Manager || !P || !Inbox)return;
    const auto Here=EWSocial::FPose::FromRender(G->Manager->OriginCoord(),P->GetActorLocation(),P->GetControlRotation());
    const FString Region=Zone(Here);const bool ActiveWindow=FSlateApplication::IsInitialized() && FSlateApplication::Get().IsActive();
    if(Region!=AcousticRegion || !ActiveWindow || P->IsStreamingHeld())
    {for(const auto& Pair:Waves)Silence(Pair.Key);Inbox->Drain();AcousticRegion=Region;PushToTalk(false);}
    bSending=bActive && bPTT && !bQuiet && !bSilent && ActiveWindow && G->Menu()==EEWMenu::None && Connection.VoiceReady();
    if(!bSending)bPTT=false;Connection.SetMicrophone(bSending);
    TSet<FString> Allowed;
    for(const auto& Pair:Peers)
    {
        if(Pair.Key==Connection.SelfId())continue;const auto& Remote=Pair.Value;
        if(!Volumes.Contains(Pair.Key))
        {float Value=1;LexTryParseString(Value,*Store.Get(TEXT("volume:")+Pair.Key,TEXT("1")));Volumes.Add(Pair.Key,FMath::IsFinite(Value)?FMath::Clamp(Value,0.f,2.f):1.f);}
        const bool OK=bActive && ActiveWindow && !P->IsStreamingHeld() && !bQuiet && !Muted.Contains(Pair.Key) && !Blocked.Contains(Pair.Key) && Remote.bHasPose &&
            Now-Remote.LastPose<2 && EWSocial::Distance(Here,Remote.Pose)<2500 && Region==Zone(Remote.Pose);
        Connection.BlockVoice(Pair.Key,!OK);if(OK)Allowed.Add(Pair.Key);else Silence(Pair.Key);
        if(auto* Speaker=Speakers.Find(Pair.Key))if(*Speaker)
            (*Speaker)->SetWorldLocation(Remote.Pose.RelativeTo(G->Manager->OriginCoord())+FVector(0,0,Remote.Pose.Motion==TEXT("sit")?58:68));
    }
    for(auto& Frame:Inbox->Drain())
    {
        if(!Allowed.Contains(Frame.Id) || bSilent){++VoiceFramesDropped;continue;}
        const auto* Peer=Peers.Find(Frame.Id);if(!Peer)continue;
        auto* A=Speakers.FindRef(Frame.Id).Get();auto* W=Waves.FindRef(Frame.Id).Get();
        if(!A)
        {
            A=NewObject<UAudioComponent>(this);A->bAutoActivate=false;A->bAutoDestroy=false;A->bAllowSpatialization=true;A->bOverrideAttenuation=true;
            auto& T=A->AttenuationOverrides;T.bAttenuate=true;T.bSpatialize=true;T.SpatializationAlgorithm=ESoundSpatializationAlgorithm::SPATIALIZATION_HRTF;
            T.AttenuationShape=EAttenuationShape::Sphere;T.AttenuationShapeExtents=FVector(150,0,0);T.FalloffDistance=2350;T.DistanceAlgorithm=EAttenuationDistanceModel::Linear;
            T.bEnableOcclusion=true;T.OcclusionTraceChannel=ECC_Visibility;T.OcclusionLowPassFilterFrequency=1200;T.OcclusionVolumeAttenuation=.25f;
            T.bEnableReverbSend=false;A->bReverb=false;A->RegisterComponent();Speakers.Add(Frame.Id,A);
            W=NewObject<UEWBrowserAudioWave>(this);W->NumChannels=1;W->SetSampleRate(Frame.Rate);W->Duration=INDEFINITELY_LOOPING_DURATION;
            W->SoundGroup=SOUNDGROUP_Voice;W->bLooping=false;A->SetSound(W);Waves.Add(Frame.Id,W);
        }
        if(W->GetSampleRateForCurrentPlatform()!=Frame.Rate){A->Stop();W->FlushSamples();W->SetSampleRate(Frame.Rate);}
        A->SetWorldLocation(Peer->Pose.RelativeTo(G->Manager->OriginCoord())+FVector(0,0,Peer->Pose.Motion==TEXT("sit")?58:68));
        A->SetVolumeMultiplier(PlayerVolume(Frame.Id));W->PushSamples(reinterpret_cast<const uint8*>(Frame.Samples.GetData()),Frame.Samples.Num()*sizeof(int16));
        if(!A->IsPlaying())A->Play();Audible.Add(Frame.Id);++VoiceFrames;
        int32 Peak=0;for(const int16 Sample:Frame.Samples)Peak=FMath::Max(Peak,FMath::Abs(int32(Sample)));if(Peak>300)SpeechUntil.Add(Frame.Id,Now+.25);
    }
}
void AEWSocialSession::Tick(float Delta)
{
    Super::Tick(Delta);Connection.Tick();const double Now=FPlatformTime::Seconds();
    if(Connection.InLobby() && SeenLobby.IsEmpty())Entered();
    if(!SeenLobby.IsEmpty() && !Connection.InLobby()){ClearSession(true);LocalStatus.Reset();RefreshMenu();return;}
    if(SeenLobby.IsEmpty())return;
    if(!bActive && Now-LastHello>1)
    {LastHello=Now;auto O=EWSocial::Message(TEXT("hello"));O->SetNumberField(TEXT("protocol"),EWSocial::Protocol);O->SetStringField(TEXT("build"),EWSocial::Build);O->SetStringField(TEXT("name"),LocalName);Connection.Send(Connection.HostId(),EWSocial::Encode(O),true);}
    if(!Connection.Hosting() && Now-LastServer>20){Connection.Leave();ClearSession(true);LocalStatus=TEXT("ホストとの通信が途切れたため、一人用の時計広場に戻りました。");return;}
    if(Connection.Hosting() && Now-LastState>1)
    {
        LastState=Now;TArray<FString> Removed;for(const auto& Pair:Authority.Players())if(!Connection.Members().Contains(Pair.Key))Removed.Add(Pair.Key);
        for(const auto& Id:Removed)Authority.Remove(Id);Peers=Authority.Players();Snapshot();if(!Removed.IsEmpty())RefreshMenu();
        for(const auto& Id:Connection.Members())if(Banned.Contains(Id))Connection.Kick(Id);
    }
    if(bActive && Now-LastPose>=.05){LastPose=Now;SendPose(Now);}
    if(bActive && !Connection.Hosting() && Now-LastPing>=2)
    {LastPing=PendingPing=Now;auto O=EWSocial::Message(TEXT("ping"));O->SetNumberField(TEXT("stamp"),Now);Connection.Send(Connection.HostId(),EWSocial::Encode(O),true);}
    UpdatePeople(Delta,Now);UpdateVoice(Now);
}
TSharedRef<FJsonObject> AEWSocialSession::Evidence() const
{
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("configured"),Connection.Configured());O->SetBoolField(TEXT("anonymous_login"),Connection.LoggedIn());
    O->SetBoolField(TEXT("active"),bActive);O->SetBoolField(TEXT("host"),Connection.Hosting());O->SetNumberField(TEXT("players"),Peers.Num());
    O->SetNumberField(TEXT("capacity"),EWSocial::Capacity);O->SetBoolField(TEXT("voice_channel_ready"),Connection.VoiceReady());
    O->SetBoolField(TEXT("microphone_sending"),bSending);O->SetNumberField(TEXT("voice_frames_rendered"),VoiceFrames);O->SetNumberField(TEXT("voice_frames_discarded"),VoiceFramesDropped);
    O->SetNumberField(TEXT("poses_received"),PosesReceived);O->SetNumberField(TEXT("rejected_packets"),PacketsRejected);O->SetNumberField(TEXT("rtt_ms"),RTT*1000);
    if(HasSharedHour())O->SetNumberField(TEXT("shared_hour"),SharedHour());else O->SetField(TEXT("shared_hour"),MakeShared<FJsonValueNull>());
    O->SetNumberField(TEXT("avatars"),Avatars.Num());O->SetBoolField(TEXT("local_store_integrity"),Store.Integrity());
    O->SetStringField(TEXT("real_service_verification"),TEXT("requires distinct devices and real EOS configuration"));return O;
}
FString AEWSocialSession::SocialEvidence() const{return EWSocial::Encode(Evidence());}

bool AEWSocialSession::HearingVoice() const
{
    if(bSending)return true;const double Now=FPlatformTime::Seconds();
    for(const auto& Id:Audible)if(SpeechUntil.FindRef(Id)>Now)return true;
    return false;
}
