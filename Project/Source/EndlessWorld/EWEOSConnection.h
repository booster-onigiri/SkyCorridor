#pragma once
#include "CoreMinimal.h"

struct FEWCityListing
{
    FString Id, Name, Language, Activity;
    int32 Members=0, Capacity=8;
};

// Owns the EOS platform, anonymous identity, lobby, P2P and voice lifetimes.
// Configuration secrets and opaque EOS handles never enter UI or evidence.
class FEWEOSConnection
{
public:
    FEWEOSConnection();
    ~FEWEOSConnection();
    bool Configure(const FString& Path,const FString& Cache);
    void Authenticate();
    void Host(const FString& Name,const FString& Language,const FString& Activity);
    void Find();
    void Join(const FString& Id);
    void Leave();
    void Tick();
    bool Send(const FString& Peer,const FString& Text,bool Reliable);
    void Kick(const FString& Peer);
    void Report(const FString& Peer,const FString& Reason);
    bool Configured() const;
    bool LoggedIn() const;
    bool Busy() const;
    bool InLobby() const;
    bool Hosting() const;
    FString Status() const;
    FString SelfId() const;
    FString HostId() const;
    FString LobbyId() const;
    const TSet<FString>& Members() const;
    const TArray<FEWCityListing>& Listings() const;
    void SetMicrophone(bool Sending);
    bool VoiceReady() const;
    FString VoiceStatus() const;
    void BlockVoice(const FString& Id,bool Blocked);
    TArray<TPair<FString,FString>> Microphones() const;
    void SelectMicrophone(const FString& Id);
    TFunction<void(const FString&,const FString&)> OnPacket;
    TFunction<void()> OnChanged;
    // This callback runs on EOS's audio thread. It must only enqueue bounded PCM.
    TFunction<void(const FString&,const TArray<int16>&,int32)> OnVoicePCM;
private:
    struct FImpl;
    TUniquePtr<FImpl> Impl;
};
