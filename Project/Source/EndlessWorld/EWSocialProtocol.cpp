#include "EWSocialProtocol.h"
#include "EWWorldClock.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

namespace EWSocial
{
bool ReadCoord(const FString& Text,int64& Value)
{
    if(Text.IsEmpty() || Text.Len()>14)return false;
    const bool Negative=Text[0]==TEXT('-');int32 I=Negative?1:0;
    if(I==Text.Len())return false;
    int64 Result=0;
    for(;I<Text.Len();++I)
    {
        if(Text[I]<TEXT('0') || Text[I]>TEXT('9'))return false;
        const int32 Digit=Text[I]-TEXT('0');
        if(Result>(EW::CoordLimit-Digit)/10)return false;
        Result=Result*10+Digit;
    }
    Value=Negative?-Result:Result;return true;
}
bool FPose::Valid() const
{
    return Chunk.Valid() && !Local.ContainsNaN() && Local.X>=0 && Local.X<EW::ChunkSize &&
        Local.Y>=0 && Local.Y<EW::ChunkSize && FMath::IsFinite(Local.Z) && FMath::Abs(Local.Z)<=1.e8 &&
        FMath::IsFinite(Yaw) && FMath::Abs(Yaw)<=360 && FMath::IsFinite(Pitch) && FMath::Abs(Pitch)<=90 &&
        (Motion==TEXT("idle") || Motion==TEXT("walk") || Motion==TEXT("jump") || Motion==TEXT("sit"));
}
FPose FPose::FromRender(EW::ChunkCoord Origin,const FVector& Position,const FRotator& Rotation)
{
    FPose P;
    const int64 DX=FMath::FloorToInt64(Position.X/double(EW::ChunkSize));
    const int64 DY=FMath::FloorToInt64(Position.Y/double(EW::ChunkSize));
    P.Chunk={Origin.X+DX,Origin.Y+DY};
    P.Local={Position.X-double(DX)*EW::ChunkSize,Position.Y-double(DY)*EW::ChunkSize,Position.Z};
    P.Yaw=FRotator::NormalizeAxis(Rotation.Yaw);P.Pitch=FMath::Clamp(FRotator::NormalizeAxis(Rotation.Pitch),-90.,90.);
    return P;
}
FVector FPose::RelativeTo(EW::ChunkCoord Origin) const
{ return {double(Chunk.X-Origin.X)*EW::ChunkSize+Local.X,double(Chunk.Y-Origin.Y)*EW::ChunkSize+Local.Y,Local.Z}; }
TSharedRef<FJsonObject> FPose::Json() const
{
    auto O=MakeShared<FJsonObject>();
    O->SetStringField(TEXT("cx"),LexToString(Chunk.X));O->SetStringField(TEXT("cy"),LexToString(Chunk.Y));
    O->SetNumberField(TEXT("x"),Local.X);O->SetNumberField(TEXT("y"),Local.Y);O->SetNumberField(TEXT("z"),Local.Z);
    O->SetNumberField(TEXT("yaw"),Yaw);O->SetNumberField(TEXT("pitch"),Pitch);O->SetNumberField(TEXT("sequence"),Sequence);
    O->SetStringField(TEXT("motion"),Motion);return O;
}
bool FPose::Read(const TSharedPtr<FJsonObject>& O,FPose& Out)
{
    if(!O)return false;FPose P;FString X,Y;double Yaw,Pitch,Seq;
    if(!O->HasTypedField<EJson::String>(TEXT("cx")) || !O->HasTypedField<EJson::String>(TEXT("cy")) || !O->HasTypedField<EJson::String>(TEXT("motion")))return false;
    for(const TCHAR* Key:{TEXT("x"),TEXT("y"),TEXT("z"),TEXT("yaw"),TEXT("pitch"),TEXT("sequence")})if(!O->HasTypedField<EJson::Number>(Key))return false;
    if(!O->TryGetStringField(TEXT("cx"),X) || !O->TryGetStringField(TEXT("cy"),Y) || !ReadCoord(X,P.Chunk.X) || !ReadCoord(Y,P.Chunk.Y) ||
        !O->TryGetNumberField(TEXT("x"),P.Local.X) || !O->TryGetNumberField(TEXT("y"),P.Local.Y) || !O->TryGetNumberField(TEXT("z"),P.Local.Z) ||
        !O->TryGetNumberField(TEXT("yaw"),Yaw) || !O->TryGetNumberField(TEXT("pitch"),Pitch) || !O->TryGetNumberField(TEXT("sequence"),Seq) ||
        !FMath::IsFinite(Seq) || Seq<1 || Seq>MAX_uint32 || FMath::FloorToDouble(Seq)!=Seq || !O->TryGetStringField(TEXT("motion"),P.Motion))return false;
    P.Yaw=float(Yaw);P.Pitch=float(Pitch);P.Sequence=uint32(Seq);if(!P.Valid())return false;Out=P;return true;
}
double Distance(const FPose& A,const FPose& B)
{ return (A.RelativeTo(B.Chunk)-B.Local).Size(); }
double DayHour(double Hour,double Elapsed)
{ return EWWorldClock::Advance(Hour,Elapsed); }
FString CleanText(const FString& Text,int32 Limit)
{
    FString Result;for(const TCHAR C:Text)
    {
        if(Result.Len()>=Limit)break;
        if(C>=32 && C!=127 && !(C>=0x202a && C<=0x202e) && !(C>=0x2066 && C<=0x2069))Result.AppendChar(C);
    }
    return Result.TrimStartAndEnd();
}
TSharedRef<FJsonObject> Message(const FString& Type)
{ auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("type"),Type);return O; }
FString Encode(const TSharedRef<FJsonObject>& O)
{ FString Text;FJsonSerializer::Serialize(O,TJsonWriterFactory<TCHAR,TCondensedJsonPrintPolicy<TCHAR>>::Create(&Text));return Text; }
bool Decode(const FString& Text,TSharedPtr<FJsonObject>& O)
{
    O.Reset();if(Text.IsEmpty() || Text.Len()>MaxMessageBytes)return false;
    int32 Depth=0;bool String=false,Escape=false;
    for(const TCHAR C:Text)
    {
        if(C==0)return false;
        if(String){if(Escape)Escape=false;else if(C==TEXT('\\'))Escape=true;else if(C==TEXT('"'))String=false;continue;}
        if(C==TEXT('"'))String=true;
        else if(C==TEXT('{') || C==TEXT('[')){if(++Depth>32)return false;}
        else if(C==TEXT('}') || C==TEXT(']')){if(--Depth<0)return false;}
    }
    return Depth==0 && !String && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),O) && O.IsValid();
}

namespace
{
void Put32(uint8* P,uint32 V){for(int I=0;I<4;++I)P[I]=uint8(V>>(I*8));}
uint32 Get32(const uint8* P){uint32 V=0;for(int I=0;I<4;++I)V|=uint32(P[I])<<(I*8);return V;}
}
TArray<TArray<uint8>> Frame(const FString& Text,uint32 Id,bool Reliable)
{
    TArray<TArray<uint8>> Out;FTCHARToUTF8 Bytes(*Text);const int32 Size=Bytes.Length();
    if(Size<=0 || Size>MaxMessageBytes)return Out;for(const TCHAR C:Text)if(C==0)return Out;
    const int32 Count=(Size+FragmentBytes-1)/FragmentBytes;
    for(int32 Part=0;Part<Count;++Part)
    {
        const int32 Length=FMath::Min(FragmentBytes,Size-Part*FragmentBytes);TArray<uint8> Data;Data.SetNumUninitialized(HeaderBytes+Length);
        Put32(Data.GetData(),0x31435745);Put32(Data.GetData()+4,Id);Put32(Data.GetData()+8,uint32(Size));
        Data[12]=uint8(Part);Data[13]=uint8(Count);Data[14]=1;Data[15]=Reliable?1:0;
        FMemory::Memcpy(Data.GetData()+HeaderBytes,Bytes.Get()+Part*FragmentBytes,Length);Out.Add(MoveTemp(Data));
    }
    return Out;
}
void FWireInbox::Prune(double Now)
{
    for(auto It=Pending.CreateIterator();It;++It)if(Now-It.Value().Started>3)It.RemoveCurrent();
    for(auto It=Limits.CreateIterator();It;++It)if(Now-It.Value().Last>60)It.RemoveCurrent();
}
bool FWireInbox::Accept(const FString& Sender,const uint8* Data,int32 Size,uint8 Channel,double Now,FString& Text)
{
    Text.Reset();if(!Data || Sender.IsEmpty() || Sender.Len()>64 || Size<=HeaderBytes || Size>HeaderBytes+FragmentBytes || Channel>1 ||
        Get32(Data)!=0x31435745 || Data[14]!=1 || Data[15]!=Channel || !FMath::IsFinite(Now))return false;
    auto* L=Limits.Find(Sender);if(!L){if(Limits.Num()>=Capacity)return false;L=&Limits.Add(Sender);L->Last=Now;}
    const double Elapsed=FMath::Clamp(Now-L->Last,0.,2.);L->Last=Now;
    L->Tokens=FMath::Min(196608.,L->Tokens+Elapsed*131072);L->Packets=FMath::Min(256.,L->Packets+Elapsed*256);
    if(L->Tokens<Size || L->Packets<1)return false;L->Tokens-=Size;L->Packets-=1;
    const uint32 Id=Get32(Data+4),Total=Get32(Data+8);const uint8 Part=Data[12],Count=Data[13];
    if(L->Complete.Contains(Id) || Total==0 || Total>MaxMessageBytes || Count!=(Total+FragmentBytes-1)/FragmentBytes || Part>=Count ||
        Size-HeaderBytes!=FMath::Min(uint32(FragmentBytes),Total-uint32(Part)*FragmentBytes))return false;
    Prune(Now);const FString Key=Sender+TEXT("/")+LexToString(Id);auto* A=Pending.Find(Key);
    if(!A)
    {
        int32 PerPeer=0;for(const auto& Pair:Pending)if(Pair.Value.Sender==Sender)++PerPeer;
        if(PerPeer>=4 || Pending.Num()>=32)return false;FAssembly New;New.Sender=Sender;New.Started=Now;New.Total=Total;New.Count=Count;New.Parts.SetNum(Count);
        A=&Pending.Add(Key,MoveTemp(New));
    }
    if(A->Total!=int32(Total) || A->Count!=Count || !A->Parts[Part].IsEmpty())return false;
    A->Parts[Part].Append(Data+HeaderBytes,Size-HeaderBytes);++A->Received;if(A->Received!=Count)return false;
    TArray<uint8> Joined;Joined.Reserve(Total);for(auto& Chunk:A->Parts)Joined.Append(Chunk);Pending.Remove(Key);
    if(Joined.Contains(0))return false;
    const FUTF8ToTCHAR Decoded(reinterpret_cast<const char*>(Joined.GetData()),Joined.Num());const FString Candidate(Decoded.Length(),Decoded.Get());
    const FTCHARToUTF8 Encoded(*Candidate);
    if(Encoded.Length()!=Joined.Num() || FMemory::Memcmp(Encoded.Get(),Joined.GetData(),Joined.Num())!=0)return false;
    L=Limits.Find(Sender);L->Complete.Add(Id);if(L->Complete.Num()>256)L->Complete.RemoveAt(0,L->Complete.Num()-256);
    Text=Candidate;return true;
}
void FHost::Reset(const FString& Id,const FString& Name,double Now)
{ Peers.Reset();Claims.Reset();Bans.Reset();HostId=Id;Admit(Id,Name,Protocol,Build,Now); }
bool FHost::Admit(const FString& Id,const FString& Name,int32 Version,const FString& Content,double Now)
{
    if(Id.IsEmpty() || Id.Len()>64 || Version!=Protocol || Content!=Build || Bans.Contains(Id))return false;
    if(Peers.Contains(Id))return true;
    if(Peers.Num()>=Capacity)return false;
    FPeer P;P.Id=Id;P.Name=CleanText(Name,20);if(P.Name.IsEmpty())P.Name=TEXT("旅人");P.LastSeen=Now;Peers.Add(Id,P);return true;
}
bool FHost::Pose(const FString& Id,const FPose& Value,double Now)
{
    auto* P=Peers.Find(Id);if(!P || !Value.Valid() || Value.Sequence==0)return false;
    if(P->bHasPose)
    {
        if(int32(Value.Sequence-P->Pose.Sequence)<=0 || Now-P->LastPose<.025)return false;
        // Small correction tolerance plus generous falling/transit speed; travel uses an explicit message.
        if(Distance(Value,P->Pose)>300+FMath::Clamp(Now-P->LastPose,0.,2.)*6000)return false;
    }
    P->Pose=Value;P->LastSeen=P->LastPose=Now;P->bHasPose=true;return true;
}
bool FHost::Chat(const FString& Id,const FString& Text,double Now,FString& Clean)
{
    auto* P=Peers.Find(Id);if(!P || Now-P->LastChat<1.)return false;
    Clean=CleanText(Text,240);if(Clean.IsEmpty())return false;P->LastChat=Now;P->LastSeen=Now;return true;
}
void FHost::Release(const FString& Id)
{ for(auto It=Claims.CreateIterator();It;++It)if(It.Value()==Id)It.RemoveCurrent(); }
void FHost::Remove(const FString& Id){Release(Id);Peers.Remove(Id);}
void FHost::Ban(const FString& Id){if(Id!=HostId){Bans.Add(Id);Remove(Id);}}
bool FHost::Reserve(const FString& Id,const FString& Object)
{
    if(!Peers.Contains(Id) || Object.IsEmpty() || Object.Len()>120)return false;
    if(const auto* Holder=Claims.Find(Object))return *Holder==Id;
    Release(Id);Claims.Add(Object,Id);return true;
}
}
