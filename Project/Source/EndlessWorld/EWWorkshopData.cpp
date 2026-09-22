#include "EWWorkshopData.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace EWWorkshop
{
namespace
{
bool Keys(const TSharedPtr<FJsonObject>& O,std::initializer_list<const TCHAR*> Expected)
{if(!O || O->Values.Num()!=int32(Expected.size()))return false;for(const auto* K:Expected)if(!O->HasField(K))return false;return true;}
bool String(const TSharedPtr<FJsonObject>& O,const TCHAR* K,FString& S,int32 Max=96)
{return O && O->TryGetStringField(K,S) && !S.IsEmpty() && S.Len()<=Max && !S.Contains(TEXT("\n")) && !S.Contains(TEXT("\r"));}
bool Equal(const TSharedPtr<FJsonObject>& O,const TCHAR* K,const TCHAR* Value)
{FString S;return String(O,K,S) && S==Value;}
bool Number(const TSharedPtr<FJsonObject>& O,const TCHAR* K,double& N)
{return O && O->TryGetNumberField(K,N) && FMath::IsFinite(N);}
TSharedPtr<FJsonObject> Object(const TSharedPtr<FJsonObject>& O,const TCHAR* K)
{const TSharedPtr<FJsonObject>* P=nullptr;return O && O->TryGetObjectField(K,P)?*P:nullptr;}
bool Parse(const FString& T,TSharedPtr<FJsonObject>& O)
{return T.Len()<=32768 && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(T),O) && O.IsValid();}
bool Colour(const TSharedPtr<FJsonObject>& O,const TCHAR* Key,FLinearColor& C)
{
    FString S;if(!String(O,Key,S,7) || S.Len()!=7 || S[0]!='#')return false;
    for(int I=1;I<7;++I)if(!FChar::IsHexDigit(S[I]))return false;
    C=FLinearColor(FColor::FromHex(S));return true;
}
bool Coordinate(const TSharedPtr<FJsonObject>& O,const TCHAR* K,int64& V)
{FString S;if(!String(O,K,S,15) || !LexTryParseString(V,*S) || LexToString(V)!=S)return false;return V>=-EW::CoordLimit && V<=EW::CoordLimit;}
}
bool SafeId(const FString& S)
{if(S.IsEmpty() || S.Len()>48)return false;for(TCHAR C:S)if(!((C>='a' && C<='z') || (C>='0' && C<='9') || C=='-'))return false;return true;}
bool SafeFilename(const FString& S,const FString& Suffix)
{return S.EndsWith(Suffix) && SafeId(S.LeftChop(Suffix.Len()));}
bool ReadBoundedFile(const FString& Path,FString& T,FString& Error)
{
    const int64 Size=IFileManager::Get().FileSize(*Path);
    if(Size<=0 || Size>32768 || !FFileHelper::LoadFileToString(T,*Path)){Error=TEXT("ファイルを読めません。UTF-8、32 KB 以下で保存してください。");return false;}return true;
}
bool ReadEgg(const FString& Text,Egg& Out,FString& Error)
{
    Error=TEXT("卵の形式が一致しません。見本と同じ項目・対応バージョンを使用してください。");
    TSharedPtr<FJsonObject> O;Egg E;double Version=0,Catalog=0;
    if(!Parse(Text,O) || !Keys(O,{TEXT("format"),TEXT("version"),TEXT("id"),TEXT("title"),TEXT("generator"),TEXT("arrival")}) ||
        !Equal(O,TEXT("format"),TEXT("ew.egg")) || !Number(O,TEXT("version"),Version) || Version!=1 ||
        !String(O,TEXT("id"),E.Id) || !SafeId(E.Id) || !String(O,TEXT("title"),E.Title,60))return false;
    const auto G=Object(O,TEXT("generator")),A=Object(O,TEXT("arrival"));
    if(!Keys(G,{TEXT("id"),TEXT("catalogue"),TEXT("content"),TEXT("seed")}) || !Equal(G,TEXT("id"),TEXT("endless.v2")) ||
        !Equal(G,TEXT("content"),TEXT("city82")) || !Number(G,TEXT("catalogue"),Catalog) || Catalog!=EW::CatalogueVersion ||
        !String(G,TEXT("seed"),E.World.Seed,32) || !E.World.Valid() ||
        !Keys(A,{TEXT("chunk_x"),TEXT("chunk_y"),TEXT("landmark")}) || !Equal(A,TEXT("landmark"),TEXT("hub")) ||
        !Coordinate(A,TEXT("chunk_x"),E.Arrival.X) || !Coordinate(A,TEXT("chunk_y"),E.Arrival.Y))return false;
    Out=MoveTemp(E);Error.Empty();return true;
}
bool ReadConcept(const FString& Text,Concept& Out,FString& Error)
{
    Error=TEXT("追加要素の形式が一致しません。対応するチェスの見本を確認してください。");
    TSharedPtr<FJsonObject> O;Concept C;double Version=0;
    if(!Parse(Text,O) || !Keys(O,{TEXT("format"),TEXT("version"),TEXT("id"),TEXT("title"),TEXT("runtime"),TEXT("rules"),TEXT("appearance")}) ||
        !Equal(O,TEXT("format"),TEXT("ew.concept")) || !Number(O,TEXT("version"),Version) || Version!=1 ||
        !Equal(O,TEXT("runtime"),TEXT("tabletop.v1")) || !String(O,TEXT("id"),C.Id) || !SafeId(C.Id) || !String(O,TEXT("title"),C.Title,60))return false;
    const auto R=Object(O,TEXT("rules")),A=Object(O,TEXT("appearance"));
    if(!Keys(R,{TEXT("id")}) || !Equal(R,TEXT("id"),TEXT("chess.standard.v1")) ||
        !Keys(A,{TEXT("size_cm"),TEXT("light"),TEXT("dark"),TEXT("accent"),TEXT("pieces")}) || !Equal(A,TEXT("pieces"),TEXT("classic.v1")) ||
        !Number(A,TEXT("size_cm"),C.Size) || C.Size<120 || C.Size>220 ||
        !Colour(A,TEXT("light"),C.Light) || !Colour(A,TEXT("dark"),C.Dark) || !Colour(A,TEXT("accent"),C.Accent))return false;
    if(FMath::Abs(C.Light.GetLuminance()-C.Dark.GetLuminance())<.12){Error=TEXT("盤面の明暗が近すぎます。見分けやすい2色を指定してください。");return false;}
    Out=MoveTemp(C);Error.Empty();return true;
}
bool AtomicWrite(const FString& Path,const FString& Text)
{
    IFileManager& Files=IFileManager::Get();Files.MakeDirectory(*FPaths::GetPath(Path),true);
    const FString Tmp=Path+TEXT(".tmp"),Backup=Path+TEXT(".backup");
    if(!FFileHelper::SaveStringToFile(Text,*Tmp,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))return false;
    if(Files.FileExists(*Path) && Files.Copy(*Backup,*Path,true,true)!=COPY_OK)return false;
    return Files.Move(*Path,*Tmp,true,true,false,true);
}
}
