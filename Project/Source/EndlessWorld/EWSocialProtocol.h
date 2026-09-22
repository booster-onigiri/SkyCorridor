#pragma once
#include "CoreMinimal.h"
#include "EWWorld.h"
#include "Dom/JsonObject.h"

namespace EWSocial
{
constexpr int32 Protocol = 1;
constexpr int32 Capacity = 8;
constexpr int32 MaxMessageBytes = 65536;
inline constexpr const TCHAR* Build = TEXT("city92-skylandmarks");
struct FPose
{
    EW::ChunkCoord Chunk;
    FVector Local = FVector(6400,5150,2338);
    float Yaw = 0, Pitch = 0;
    FString Motion = TEXT("idle");
    uint32 Sequence = 0;
    bool Valid() const;
    FVector RelativeTo(EW::ChunkCoord Origin) const;
    static FPose FromRender(EW::ChunkCoord Origin, const FVector& Position, const FRotator& Rotation);
    TSharedRef<FJsonObject> Json() const;
    static bool Read(const TSharedPtr<FJsonObject>& Json, FPose& Out);
};
struct FPeer
{
    FString Id, Name;
    FPose Pose;
    double LastSeen = 0, LastPose = 0, LastChat = -100;
    bool bHasPose = false;
};
FString CleanText(const FString& Text, int32 Limit);
FString Encode(const TSharedRef<FJsonObject>& Value);
bool Decode(const FString& Text, TSharedPtr<FJsonObject>& Value);
bool ReadCoord(const FString& Text, int64& Value);
double Distance(const FPose& A, const FPose& B);
double DayHour(double InitialHour, double ElapsedSeconds);
TSharedRef<FJsonObject> Message(const FString& Type);

constexpr int32 FragmentBytes=1080,HeaderBytes=16;
TArray<TArray<uint8>> Frame(const FString& Text,uint32 Id,bool Reliable);
class FWireInbox
{
public:
    bool Accept(const FString& Sender,const uint8* Bytes,int32 Size,uint8 Channel,double Now,FString& Text);
    void Prune(double Now);
    void Reset(){Pending.Reset();Limits.Reset();}
    int32 PendingCount() const{return Pending.Num();}
private:
    struct FAssembly {FString Sender;int32 Total=0,Count=0,Received=0;double Started=0;TArray<TArray<uint8>> Parts;};
    struct FLimit {double Last=0,Tokens=196608,Packets=256;TArray<uint32> Complete;};
    TMap<FString,FAssembly> Pending;
    TMap<FString,FLimit> Limits;
};

// The transport supplies authenticated EOS product IDs. A payload never selects its sender.
class FHost
{
public:
    void Reset(const FString& Id, const FString& Name, double Now);
    bool Admit(const FString& Id, const FString& Name, int32 Version, const FString& Content, double Now);
    bool Pose(const FString& Id, const FPose& Value, double Now);
    bool Chat(const FString& Id, const FString& Text, double Now, FString& Clean);
    void Remove(const FString& Id);
    void Ban(const FString& Id);
    bool Reserve(const FString& Id, const FString& Object);
    void Release(const FString& Id);
    const TMap<FString,FPeer>& Players() const { return Peers; }
    const TSet<FString>& Banned() const { return Bans; }
    FString Owner() const { return HostId; }
private:
    FString HostId;
    TMap<FString,FPeer> Peers;
    TMap<FString,FString> Claims;
    TSet<FString> Bans;
};
}
