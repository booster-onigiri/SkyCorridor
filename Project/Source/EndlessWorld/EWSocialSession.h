#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWEOSConnection.h"
#include "EWSocialProtocol.h"
#include "EWSocialStore.h"
#include "EWSocialSession.generated.h"

class AEWCinemaPeer;
class UAudioComponent;
class UEWBrowserAudioWave;
struct FEWSocialAudioInbox;

UCLASS()
class ENDLESSWORLD_API AEWSocialSession:public AActor
{
    GENERATED_BODY()
public:
    AEWSocialSession();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
    virtual void EndPlay(const EEndPlayReason::Type Reason) override;
    void Authenticate();
    void FindCities();
    void HostCity(const FString& Name,const FString& Language,const FString& Activity,const FString& Nickname);
    void JoinCity(const FString& Code,const FString& Nickname);
    void LeaveCity();
    void SendChat(const FString& Text);
    void PushToTalk(bool Pressed);
    void ToggleMute(const FString& Id);
    void ToggleBlock(const FString& Id);
    void SetPlayerVolume(const FString& Id,float Value);
    float PlayerVolume(const FString& Id) const;
    void ToggleQuiet();
    void KickPlayer(const FString& Id);
    void ReportPlayer(const FString& Id,const FString& Reason);
    bool IsMuted(const FString& Id) const {return Muted.Contains(Id);}
    bool IsBlocked(const FString& Id) const {return Blocked.Contains(Id);}
    bool IsQuiet() const {return bQuiet;}
    bool Speaking() const {return bSending;}
    bool HearingVoice() const;
    bool Active() const {return bActive;}
    bool HasSharedHour() const {return bActive && (Connection.Hosting() || LastClock>0);}
    double SharedHour() const;
    FString Nickname() const {return LocalName;}
    FString Status() const;
    FString Invite() const;
    FString ChatText() const;
    const TMap<FString,EWSocial::FPeer>& Players() const {return Peers;}
    FEWEOSConnection& Network(){return Connection;}
    // Local UI/gameplay only. One SQLite handle is shared because Unreal's file
    // adapter opens writable databases exclusively. Receive() never writes it.
    FEWSocialStore& LocalSaveStore(){return Store;}
    TSharedRef<FJsonObject> Evidence() const;
    UFUNCTION(BlueprintCallable,Category="Social verification") FString SocialEvidence() const;
    UFUNCTION(BlueprintCallable,Category="Social verification") void OpenCityMenu();
private:
    FEWEOSConnection Connection;
    FEWSocialStore Store;
    EWSocial::FHost Authority;
    TMap<FString,EWSocial::FPeer> Peers;
    TSet<FString> Muted,Blocked,Banned;
    TMap<FString,float> Volumes;
    TMap<FString,double> SpeechUntil,HelloTimes;
    TSharedPtr<FEWSocialAudioInbox,ESPMode::ThreadSafe> Inbox;
    UPROPERTY() TMap<FString,TObjectPtr<AEWCinemaPeer>> Avatars;
    UPROPERTY() TMap<FString,TObjectPtr<UAudioComponent>> Speakers;
    UPROPERTY() TMap<FString,TObjectPtr<UEWBrowserAudioWave>> Waves;
    TSet<FString> Audible;
    TArray<FString> ChatLines;
    FString LocalName=TEXT("旅人"),SeenLobby,LocalStatus,AcousticRegion;
    EW::ChunkCoord LastOrigin;
    uint32 Sequence=0;
    bool bActive=false,bEnding=false,bQuiet=false,bPTT=false,bSending=false,bSilent=false;
    double LastPose=0,LastState=0,LastPing=0,LastHello=0,LastServer=0,EnteredAt=0,LastClock=0;
    double StartHour=10,ClockHour=10,ClockStarted=0,RTT=0,PendingPing=0;
    uint64 PosesReceived=0,PacketsRejected=0,VoiceFrames=0,VoiceFramesDropped=0;
    void RefreshMenu();
    void Entered();
    void ClearSession(bool ReturnHome);
    void SetName(const FString& Value);
    void Receive(const FString& Sender,const FString& Text);
    void Broadcast(const TSharedRef<FJsonObject>& Message,bool Reliable);
    void Snapshot(const FString& Recipient={});
    void SendPose(double Now);
    void UpdatePeople(float Delta,double Now);
    void UpdateVoice(double Now);
    void Silence(const FString& Id);
    void AddChat(const FString& Id,const FString& Name,const FString& Text);
    void Remember(const FString& Key,const FString& Value);
    FString Zone(const EWSocial::FPose& Pose) const;
};
