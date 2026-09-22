#include "EWSocialStore.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

namespace
{
FString Quote(const FString& S){return TEXT("'")+S.Replace(TEXT("'"),TEXT("''"))+TEXT("'");}
bool CheckDatabase(const FString& Path)
{
    FSQLiteDatabase Check;int32 Version=0;
    const bool OK=Check.Open(*Path,ESQLiteDatabaseOpenMode::ReadOnly) && Check.PerformQuickIntegrityCheck() &&
        Check.GetUserVersion(Version) && Version==1;
    if(Check.IsValid())Check.Close();return OK;
}
}
FEWSocialStore::~FEWSocialStore(){Close();}
void FEWSocialStore::Close(){if(DB.IsValid())DB.Close();}
bool FEWSocialStore::Open(const FString& Directory)
{
    Close();LastError.Reset();Recovery.Reset();Root=FPaths::ConvertRelativePathToFull(Directory);Path=Root/TEXT("social.sqlite3");
    auto& Files=IFileManager::Get();if(!Files.MakeDirectory(*Root,true))return Fail(TEXT("生活データの保存先を作成できません。"));
    const bool Exists=Files.FileExists(*Path);
    bool OK=DB.Open(*Path,ESQLiteDatabaseOpenMode::ReadWriteCreate);
    if(OK && Exists)OK=DB.PerformQuickIntegrityCheck();
    if(!OK)
    {
        Close();const FString Previous=Root/TEXT("social.previous.sqlite3");
        if(!CheckDatabase(Previous))return Fail(TEXT("生活データを開けません。元のファイルを保持しています。"));
        const FString Id=FGuid::NewGuid().ToString(EGuidFormats::Digits);
        const FString Damaged=Root/(TEXT("social.unreadable-")+Id+TEXT(".sqlite3"));
        const FString Pending=Root/(TEXT("social.restore-")+Id+TEXT(".sqlite3"));
        if((Exists && Files.Copy(*Damaged,*Path,false,false)!=COPY_OK) || Files.Copy(*Pending,*Previous,false,false)!=COPY_OK ||
            !CheckDatabase(Pending) || !Files.Move(*Path,*Pending,true,false,false,true) || !DB.Open(*Path,ESQLiteDatabaseOpenMode::ReadWrite))
            return Fail(TEXT("生活データを復旧できません。元データとバックアップを保持しています。"));
        Recovery=TEXT("生活データを前回のバックアップから復旧しました。読み取れなかったファイルも保存しています。");
    }
    int32 Version=0;
    if(!DB.GetUserVersion(Version) || Version>1){Close();return Fail(TEXT("新しい版の生活データです。対応するゲームで開いてください。"));}
    const TCHAR* Statements[]={TEXT("PRAGMA journal_mode=DELETE;"),TEXT("PRAGMA synchronous=FULL;"),TEXT("PRAGMA busy_timeout=250;"),
        TEXT("BEGIN IMMEDIATE;"),TEXT("CREATE TABLE IF NOT EXISTS preferences(key TEXT PRIMARY KEY,value TEXT NOT NULL);"),
        TEXT("CREATE TABLE IF NOT EXISTS fish_catches(id TEXT PRIMARY KEY,species TEXT NOT NULL,site INTEGER NOT NULL,length REAL NOT NULL CHECK(length>0),hour REAL NOT NULL CHECK(hour>=0 AND hour<24),utc TEXT NOT NULL);"),
        TEXT("CREATE INDEX IF NOT EXISTS fish_by_species ON fish_catches(species,length);"),
        TEXT("PRAGMA user_version=1;"),TEXT("COMMIT;")};
    for(const auto* Statement:Statements)if(!DB.Execute(Statement))
    {DB.Execute(TEXT("ROLLBACK;"));Close();return Fail(TEXT("生活データを準備できません。"));}
    return true;
}
FString FEWSocialStore::Get(const FString& Key,const FString& Default)
{
    if(!DB.IsValid())return Default;auto S=DB.PrepareStatement(TEXT("SELECT value FROM preferences WHERE key=?1;"));FString Value;
    return S.IsValid() && S.SetBindingValueByIndex(1,Key) && S.Step()==ESQLitePreparedStatementStepResult::Row && S.GetColumnValueByIndex(0,Value)?Value:Default;
}
bool FEWSocialStore::Backup()
{
    if(!DB.IsValid() || !DB.PerformQuickIntegrityCheck())return Fail(TEXT("生活データを検証できないため保存を止めました。"));
    const FString Temp=Root/(TEXT("social.backup-")+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT(".sqlite3"));
    if(!DB.Execute(*(TEXT("VACUUM INTO ")+Quote(Temp)+TEXT(";"))) || !CheckDatabase(Temp) ||
        !IFileManager::Get().Move(*(Root/TEXT("social.previous.sqlite3")),*Temp,true,false,false,true))
        return Fail(TEXT("バックアップを作成できないため保存を止めました。空き容量を確認してください。"));
    return true;
}
bool FEWSocialStore::Put(const FString& Key,const FString& Value)
{
    if(Key.IsEmpty() || Key.Len()>128 || Value.Len()>4096)return Fail(TEXT("保存する設定が不正です。"));
    if(!Backup() || !DB.Execute(TEXT("BEGIN IMMEDIATE;")))return false;
    bool OK=false;
    {
        auto S=DB.PrepareStatement(TEXT("INSERT INTO preferences(key,value) VALUES(?1,?2) ON CONFLICT(key) DO UPDATE SET value=excluded.value;"));
        OK=S.IsValid() && S.SetBindingValueByIndex(1,Key) && S.SetBindingValueByIndex(2,Value) && S.Execute();
    }
    if(OK && DB.Execute(TEXT("COMMIT;"))){LastError.Reset();return true;}
    DB.Execute(TEXT("ROLLBACK;"));return Fail(TEXT("生活データを保存できませんでした。前回の記録を保持しています。"));
}
TSet<FString> FEWSocialStore::Flags(const FString& Prefix)
{
    TSet<FString> Out;if(!DB.IsValid())return Out;
    auto S=DB.PrepareStatement(TEXT("SELECT key FROM preferences WHERE value='1' AND substr(key,1,?1)=?2 LIMIT 512;"));
    if(!S.IsValid() || !S.SetBindingValueByIndex(1,Prefix.Len()) || !S.SetBindingValueByIndex(2,Prefix))return Out;
    while(S.Step()==ESQLitePreparedStatementStepResult::Row){FString Key;if(S.GetColumnValueByIndex(0,Key))Out.Add(Key.Mid(Prefix.Len()));}
    return Out;
}

bool FEWSocialStore::AddCatch(const EWFishing::FCatch& Catch,bool& Added)
{
    Added=false;if(!DB.IsValid() || !Catch.Valid())return Fail(TEXT("釣果の内容を確認できませんでした。"));
    // A retry after a full disk / UI refresh must never grant the same fish twice.
    {
        auto S=DB.PrepareStatement(TEXT("SELECT species,site,length,hour,utc FROM fish_catches WHERE id=?1;"));
        if(!S.IsValid() || !S.SetBindingValueByIndex(1,Catch.Id))return Fail(TEXT("釣果を確認できません。"));
        const auto Step=S.Step();if(Step==ESQLitePreparedStatementStepResult::Row)
        {
            FString Species,UTC;int32 Site=-1;double Length=0,Hour=0;
            const bool Same=S.GetColumnValueByIndex(0,Species) && S.GetColumnValueByIndex(1,Site) && S.GetColumnValueByIndex(2,Length) &&
                S.GetColumnValueByIndex(3,Hour) && S.GetColumnValueByIndex(4,UTC) && Species==Catch.Species && Site==Catch.Site &&
                Length==Catch.Length && Hour==Catch.Hour && UTC==Catch.UTC;
            return Same?true:Fail(TEXT("同じ釣果番号に異なる記録があるため、保存を止めました。"));
        }
        if(Step!=ESQLitePreparedStatementStepResult::Done)return Fail(TEXT("釣果を確認できません。"));
    }
    if(!Backup() || !DB.Execute(TEXT("BEGIN IMMEDIATE;")))return false;
    bool OK=false;
    {
        auto S=DB.PrepareStatement(TEXT("INSERT INTO fish_catches(id,species,site,length,hour,utc) VALUES(?1,?2,?3,?4,?5,?6);"));
        OK=S.IsValid() && S.SetBindingValueByIndex(1,Catch.Id) && S.SetBindingValueByIndex(2,Catch.Species) &&
            S.SetBindingValueByIndex(3,Catch.Site) && S.SetBindingValueByIndex(4,Catch.Length) && S.SetBindingValueByIndex(5,Catch.Hour) &&
            S.SetBindingValueByIndex(6,Catch.UTC) && S.Execute();
    }
    if(OK && DB.Execute(TEXT("COMMIT;"))){LastError.Reset();Added=true;return true;}
    DB.Execute(TEXT("ROLLBACK;"));return Fail(TEXT("釣果を保存できません。空き容量を確認して「保存を再試行」を選んでください。"));
}
TArray<EWFishing::FRecord> FEWSocialStore::FishRecords()
{
    TArray<EWFishing::FRecord> Out;if(!DB.IsValid())return Out;
    auto S=DB.PrepareStatement(TEXT("SELECT species,COUNT(*),MAX(length) FROM fish_catches GROUP BY species ORDER BY species LIMIT 32;"));
    if(!S.IsValid())return Out;
    while(S.Step()==ESQLitePreparedStatementStepResult::Row)
    {
        EWFishing::FRecord R;
        if(S.GetColumnValueByIndex(0,R.Species) && S.GetColumnValueByIndex(1,R.Count) && S.GetColumnValueByIndex(2,R.Largest) && EWFishing::Find(R.Species))Out.Add(R);
    }
    return Out;
}
bool FEWSocialStore::SetDisplayFish(const FString& Species)
{
    if(!Species.IsEmpty())
    {
        const auto Records=FishRecords();
        if(!Records.ContainsByPredicate([&](const EWFishing::FRecord& R){return R.Species==Species && R.Count>0;}))
            return Fail(TEXT("釣った魚から展示する魚を選んでください。"));
    }
    return Put(TEXT("fish:display"),Species);
}
