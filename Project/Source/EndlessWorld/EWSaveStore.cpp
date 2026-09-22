#include "EWSaveStore.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
FString SQLQuote(FString S) { return TEXT("'") + S.Replace(TEXT("'"), TEXT("''")) + TEXT("'"); }
bool BindPlace(FSQLitePreparedStatement& S, const EW::PlaceBookmark& P)
{
    return S.SetBindingValueByName(TEXT(":world"), P.WorldCode) &&
        S.SetBindingValueByName(TEXT(":id"), P.Id) &&
        S.SetBindingValueByName(TEXT(":name"), P.Name) &&
        S.SetBindingValueByName(TEXT(":kind"), P.Kind) &&
        S.SetBindingValueByName(TEXT(":cx"), P.Coord.X) &&
        S.SetBindingValueByName(TEXT(":cy"), P.Coord.Y) &&
        S.SetBindingValueByName(TEXT(":x"), P.LocalPosition.X) &&
        S.SetBindingValueByName(TEXT(":y"), P.LocalPosition.Y) &&
        S.SetBindingValueByName(TEXT(":z"), P.LocalPosition.Z) &&
        S.SetBindingValueByName(TEXT(":yaw"), P.Yaw);
}
const TCHAR* Columns = TEXT("world,id,name,kind,cx,cy,x,y,z,yaw,favourite");
}

FEWSaveStore::~FEWSaveStore() { Close(); }
void FEWSaveStore::Close() { if (DB.IsValid()) DB.Close(); }
bool FEWSaveStore::Fail(const FString& Message) { LastError = Message; return false; }

bool FEWSaveStore::Open(const FString& InDirectory, bool ReadOnly)
{
    Close(); LastError.Empty(); RecoveryNotice.Empty(); bReadOnly = ReadOnly;
    Directory = FPaths::ConvertRelativePathToFull(InDirectory);
    DatabasePath = FPaths::Combine(Directory, TEXT("exploration.sqlite3"));
    if (!ReadOnly && !IFileManager::Get().MakeDirectory(*Directory, true))
        return Fail(TEXT("保存フォルダーを作成できません。保存場所の空き容量と権限を確認してください。"));
    const bool Existing = IFileManager::Get().FileExists(*DatabasePath);
    const auto Mode = ReadOnly ? ESQLiteDatabaseOpenMode::ReadOnly : ESQLiteDatabaseOpenMode::ReadWriteCreate;
    bool OK = DB.Open(*DatabasePath, Mode);
    if (OK && Existing) OK = DB.PerformQuickIntegrityCheck();
    if (!OK)
    {
        Close();
        const FString BackupPath = FPaths::Combine(Directory, TEXT("exploration.previous.sqlite3"));
        FSQLiteDatabase Previous;
        const bool Recoverable = !ReadOnly && Previous.Open(*BackupPath, ESQLiteDatabaseOpenMode::ReadOnly) &&
            Previous.PerformQuickIntegrityCheck();
        if (Previous.IsValid()) Previous.Close();
        if (!Recoverable) return Fail(TEXT("保存データを開けませんでした。元のファイルは保持しています。"));
        const FString Preserved = FPaths::Combine(Directory,
            TEXT("exploration.unreadable-") + FDateTime::UtcNow().ToString(TEXT("%Y%m%d-%H%M%S")) +
            TEXT("-") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8) + TEXT(".sqlite3"));
        if (Existing && IFileManager::Get().Copy(*Preserved, *DatabasePath, false, false) != COPY_OK)
            return Fail(TEXT("破損した保存データの退避に失敗しました。元のファイルを保持しています。"));
        const FString RestorePath = FPaths::Combine(Directory, TEXT("exploration.restore-pending.sqlite3"));
        if (IFileManager::Get().Copy(*RestorePath, *BackupPath, true, false) != COPY_OK ||
            !IFileManager::Get().Move(*DatabasePath, *RestorePath, true, false, false, true) ||
            !DB.Open(*DatabasePath, Mode))
            return Fail(TEXT("バックアップの復旧に失敗しました。退避した保存データは保持しています。"));
        RecoveryNotice = TEXT("保存データを前回のバックアップから復旧しました。読み取れなかったファイルも保存フォルダーに保持しています。");
    }
    int32 Version = 0;
    if (!DB.GetUserVersion(Version) || Version > 1)
    { Close(); return Fail(TEXT("新しい版の保存データです。対応するゲームで開いてください。")); }
    if (ReadOnly) return true;
    // UE's wrapper executes one prepared statement, not an SQL script.
    const TCHAR* Schema[] = {
        TEXT("PRAGMA journal_mode=DELETE;"), TEXT("PRAGMA synchronous=FULL;"), TEXT("PRAGMA busy_timeout=250;"),
        TEXT("BEGIN IMMEDIATE;"),
        TEXT("CREATE TABLE IF NOT EXISTS discoveries(world TEXT NOT NULL,id TEXT NOT NULL,name TEXT NOT NULL,kind INTEGER NOT NULL,"
            "cx INTEGER NOT NULL,cy INTEGER NOT NULL,x REAL NOT NULL,y REAL NOT NULL,z REAL NOT NULL,yaw REAL NOT NULL,"
            "favourite INTEGER NOT NULL DEFAULT 0,created INTEGER NOT NULL,PRIMARY KEY(world,id));"),
        TEXT("CREATE INDEX IF NOT EXISTS discovery_order ON discoveries(world,favourite DESC,created DESC,id);"),
        TEXT("CREATE TABLE IF NOT EXISTS positions(world TEXT PRIMARY KEY,id TEXT NOT NULL,name TEXT NOT NULL,kind INTEGER NOT NULL,"
            "cx INTEGER NOT NULL,cy INTEGER NOT NULL,x REAL NOT NULL,y REAL NOT NULL,z REAL NOT NULL,yaw REAL NOT NULL,"
            "favourite INTEGER NOT NULL DEFAULT 0);"),
        TEXT("CREATE TABLE IF NOT EXISTS metadata(name TEXT PRIMARY KEY,value TEXT NOT NULL);"),
        TEXT("PRAGMA user_version=1;"), TEXT("COMMIT;")
    };
    for (const TCHAR* Statement : Schema)
        if (!DB.Execute(Statement))
        { DB.Execute(TEXT("ROLLBACK;")); Close(); return Fail(TEXT("保存データを準備できません。空き容量と書き込み権限を確認してください。")); }
    return true;
}

bool FEWSaveStore::Backup()
{
    if (!DB.IsValid() || bReadOnly) return Fail(TEXT("この保存場所には書き込めません。以前の記録は保持しています。"));
    const FString Temporary = FPaths::Combine(Directory, TEXT("exploration.backup-pending.sqlite3"));
    const FString Previous = FPaths::Combine(Directory, TEXT("exploration.previous.sqlite3"));
    // Only this fixed, task-owned temporary file can be removed. Never delete a saved backup first.
    if (IFileManager::Get().FileExists(*Temporary) && !IFileManager::Get().Delete(*Temporary, false, false, true))
        return Fail(TEXT("バックアップ用の一時ファイルを更新できません。"));
    if (!DB.Execute(*(TEXT("VACUUM INTO ") + SQLQuote(Temporary) + TEXT(";"))))
        return Fail(TEXT("バックアップを作成できないため保存を見送りました。空き容量と権限を確認してください。"));
    FSQLiteDatabase Verify;
    bool OK = Verify.Open(*Temporary, ESQLiteDatabaseOpenMode::ReadOnly) && Verify.PerformQuickIntegrityCheck();
    if (Verify.IsValid()) Verify.Close();
    if (!OK || !IFileManager::Get().Move(*Previous, *Temporary, true, false, false, true))
        return Fail(TEXT("バックアップを検証・更新できませんでした。以前の記録は保持しています。"));
    return true;
}

bool FEWSaveStore::BeginWrite()
{
    LastError.Empty();
    return Backup() && (DB.Execute(TEXT("BEGIN IMMEDIATE;")) || Fail(TEXT("保存データが使用中です。もう一度お試しください。")));
}

bool FEWSaveStore::FinishWrite(bool Success)
{
    if (Success && DB.Execute(TEXT("COMMIT;"))) { LastError.Empty(); return true; }
    DB.Execute(TEXT("ROLLBACK;"));
    return Fail(TEXT("保存できませんでした。以前の記録は保持しています。空き容量と権限を確認してください。"));
}

bool FEWSaveStore::CheckPlace(const EW::PlaceBookmark& P)
{
    EW::WorldDescriptor W; FString Error;
    if (!EW::WorldDescriptor::Parse(P.WorldCode, W, Error) || !P.Coord.Valid() || P.Id.Len() > 64 || P.Id.IsEmpty() ||
        P.Name.Len() > 100 || P.Kind < 0 || P.Kind > 16 || P.LocalPosition.ContainsNaN() || !FMath::IsFinite(P.Yaw) ||
        P.LocalPosition.X < 0 || P.LocalPosition.X > EW::ChunkSize || P.LocalPosition.Y < 0 || P.LocalPosition.Y > EW::ChunkSize ||
        P.LocalPosition.Z < -100000 || P.LocalPosition.Z > 100000)
        return Fail(TEXT("保存する場所の情報が不正です。以前の記録は保持しています。"));
    return true;
}

bool FEWSaveStore::LoadResidenceState(const FString& World,const FString& Id,int32 Revision,int32& FlowMode)
{
    FlowMode=0;LastError.Empty();
    EW::WorldDescriptor W;FString ParseError;
    if(!DB.IsValid() || !EW::WorldDescriptor::Parse(World,W,ParseError) || Id.IsEmpty() || Id.Len()>64 || Revision!=1)
        return Fail(TEXT("窓辺の記録を読み取る条件が不正です。以前の記録は保持しています。"));
    {
        auto Table=DB.PrepareStatement(TEXT("SELECT 1 FROM sqlite_master WHERE type='table' AND name='residence_state';"));
        if(!Table.IsValid())return Fail(TEXT("窓辺の保存表を確認できませんでした。"));
        const auto Result=Table.Step();
        if(Result==ESQLitePreparedStatementStepResult::Done)return true; // untouched v1 save
        if(Result!=ESQLitePreparedStatementStepResult::Row)return Fail(TEXT("窓辺の保存表を確認できませんでした。"));
    }
    auto S=DB.PrepareStatement(TEXT("SELECT revision,flow_mode FROM residence_state WHERE world=?1 AND id=?2;"));
    if(!S.IsValid() || !S.SetBindingValueByIndex(1,World) || !S.SetBindingValueByIndex(2,Id))
        return Fail(TEXT("窓辺の記録を読み取れませんでした。"));
    const auto Result=S.Step();
    if(Result==ESQLitePreparedStatementStepResult::Done)return true;
    int32 StoredRevision=0,StoredMode=0;
    if(Result!=ESQLitePreparedStatementStepResult::Row || !S.GetColumnValueByIndex(0,StoredRevision) ||
        !S.GetColumnValueByIndex(1,StoredMode))return Fail(TEXT("窓辺の記録を読み取れませんでした。"));
    if(StoredRevision!=Revision || StoredMode<0 || StoredMode>1)
        return Fail(TEXT("この窓辺の記録は対応する版で開いてください。保存済みの選択は変更していません。"));
    FlowMode=StoredMode;return true;
}

bool FEWSaveStore::SaveResidenceState(const FString& World,const FString& Id,int32 Revision,int32 FlowMode)
{
    if(FlowMode<0 || FlowMode>1)return Fail(TEXT("水の行き先が不正です。以前の選択は保持しています。"));
    int32 Previous=0;
    // Also rejects a future per-feature revision instead of overwriting it.
    if(!LoadResidenceState(World,Id,Revision,Previous) || !BeginWrite())return false;
    bool OK=DB.Execute(TEXT("CREATE TABLE IF NOT EXISTS residence_state("
        "world TEXT NOT NULL,id TEXT NOT NULL,revision INTEGER NOT NULL,"
        "flow_mode INTEGER NOT NULL CHECK(flow_mode IN (0,1)),PRIMARY KEY(world,id));"));
    if(OK)
    {
        auto S=DB.PrepareStatement(TEXT("INSERT INTO residence_state(world,id,revision,flow_mode) VALUES(?1,?2,?3,?4) "
            "ON CONFLICT(world,id) DO UPDATE SET revision=excluded.revision,flow_mode=excluded.flow_mode;"));
        OK=S.IsValid() && S.SetBindingValueByIndex(1,World) && S.SetBindingValueByIndex(2,Id) &&
            S.SetBindingValueByIndex(3,Revision) && S.SetBindingValueByIndex(4,FlowMode) && S.Execute();
    }
    return FinishWrite(OK);
}

bool FEWSaveStore::HasRecord(const FString& World, const FString& Id)
{
    if (!DB.IsValid()) return false;
    auto S = DB.PrepareStatement(TEXT("SELECT 1 FROM discoveries WHERE world=?1 AND id=?2;"));
    if (!S.IsValid()) return false;
    S.SetBindingValueByIndex(1, World); S.SetBindingValueByIndex(2, Id);
    return S.Step() == ESQLitePreparedStatementStepResult::Row;
}

int32 FEWSaveStore::Count(const FString& World)
{
    if (!DB.IsValid()) return -1;
    auto S = DB.PrepareStatement(TEXT("SELECT COUNT(*) FROM discoveries WHERE world=?1;"));
    if (!S.IsValid() || !S.SetBindingValueByIndex(1, World) || S.Step() != ESQLitePreparedStatementStepResult::Row) return -1;
    int32 Number = 0; S.GetColumnValueByIndex(0, Number); return Number;
}

bool FEWSaveStore::Record(const EW::PlaceBookmark& P)
{
    if (!CheckPlace(P)) return false;
    if (HasRecord(P.WorldCode, P.Id)) { LastError.Empty(); return true; }
    const int32 N = Count(P.WorldCode);
    if (N < 0) return Fail(TEXT("発見記録を読み取れませんでした。"));
    if (N >= Quota) return Fail(TEXT("この世界の発見記録が上限に達しました。図鑑から記録を書き出せます。以前の記録は保持します。"));
    if (!BeginWrite()) return false;
    bool OK = false;
    {
        auto S = DB.PrepareStatement(TEXT("INSERT INTO discoveries(world,id,name,kind,cx,cy,x,y,z,yaw,favourite,created) "
            "VALUES(:world,:id,:name,:kind,:cx,:cy,:x,:y,:z,:yaw,0,:created);"));
        OK = S.IsValid() && BindPlace(S, P) &&
            S.SetBindingValueByName(TEXT(":created"), FDateTime::UtcNow().ToUnixTimestamp()) && S.Execute();
    }
    return FinishWrite(OK);
}

bool FEWSaveStore::SetFavourite(const FString& World, const FString& Id, bool Value)
{
    if (!HasRecord(World, Id)) return Fail(TEXT("この発見記録が見つかりません。"));
    if (!BeginWrite()) return false;
    bool OK;
    {
        auto S = DB.PrepareStatement(TEXT("UPDATE discoveries SET favourite=?1 WHERE world=?2 AND id=?3;"));
        OK = S.IsValid() && S.SetBindingValueByIndex(1, int32(Value)) &&
            S.SetBindingValueByIndex(2, World) && S.SetBindingValueByIndex(3, Id) && S.Execute();
    }
    return FinishWrite(OK);
}

bool FEWSaveStore::SaveCurrent(const EW::PlaceBookmark& P)
{
    if (!CheckPlace(P) || !BeginWrite()) return false;
    bool OK;
    {
        auto S = DB.PrepareStatement(TEXT("INSERT INTO positions(world,id,name,kind,cx,cy,x,y,z,yaw,favourite) "
            "VALUES(:world,:id,:name,:kind,:cx,:cy,:x,:y,:z,:yaw,0) ON CONFLICT(world) DO UPDATE SET "
            "id=excluded.id,name=excluded.name,kind=excluded.kind,cx=excluded.cx,cy=excluded.cy,"
            "x=excluded.x,y=excluded.y,z=excluded.z,yaw=excluded.yaw;"));
        OK = S.IsValid() && BindPlace(S, P) && S.Execute();
    }
    if (OK)
    {
        auto M = DB.PrepareStatement(TEXT("INSERT INTO metadata(name,value) VALUES('last_world',?1) "
            "ON CONFLICT(name) DO UPDATE SET value=excluded.value;"));
        OK = M.IsValid() && M.SetBindingValueByIndex(1, P.WorldCode) && M.Execute();
    }
    return FinishWrite(OK);
}

bool FEWSaveStore::ReadPlace(const FSQLitePreparedStatement& S, EW::PlaceBookmark& P)
{
    int32 Favourite = 0;
    bool OK = S.GetColumnValueByIndex(0, P.WorldCode) && S.GetColumnValueByIndex(1, P.Id) &&
        S.GetColumnValueByIndex(2, P.Name) && S.GetColumnValueByIndex(3, P.Kind) &&
        S.GetColumnValueByIndex(4, P.Coord.X) && S.GetColumnValueByIndex(5, P.Coord.Y) &&
        S.GetColumnValueByIndex(6, P.LocalPosition.X) && S.GetColumnValueByIndex(7, P.LocalPosition.Y) &&
        S.GetColumnValueByIndex(8, P.LocalPosition.Z) && S.GetColumnValueByIndex(9, P.Yaw) &&
        S.GetColumnValueByIndex(10, Favourite);
    P.bFavourite = Favourite != 0;
    return OK && CheckPlace(P);
}

bool FEWSaveStore::LoadCurrent(EW::PlaceBookmark& P)
{
    if (!DB.IsValid()) return false;
    FString World;
    {
        auto S = DB.PrepareStatement(TEXT("SELECT value FROM metadata WHERE name='last_world';"));
        if (!S.IsValid() || S.Step() != ESQLitePreparedStatementStepResult::Row || !S.GetColumnValueByIndex(0, World)) return false;
    }
    return LoadWorldPosition(World, P);
}

bool FEWSaveStore::LoadWorldPosition(const FString& World, EW::PlaceBookmark& P)
{
    if (!DB.IsValid()) return false;
    auto S = DB.PrepareStatement(*(FString(TEXT("SELECT ")) + Columns + TEXT(" FROM positions WHERE world=?1;")));
    return S.IsValid() && S.SetBindingValueByIndex(1, World) && S.Step() == ESQLitePreparedStatementStepResult::Row && ReadPlace(S, P);
}

TArray<EW::PlaceBookmark> FEWSaveStore::Records(const FString& World, int32 Offset, int32 Limit)
{
    TArray<EW::PlaceBookmark> Result;
    if (!DB.IsValid()) return Result;
    auto S = DB.PrepareStatement(*(FString(TEXT("SELECT ")) + Columns +
        TEXT(" FROM discoveries WHERE world=?1 ORDER BY favourite DESC,created DESC,id LIMIT ?2 OFFSET ?3;")));
    if (!S.IsValid()) return Result;
    S.SetBindingValueByIndex(1, World); S.SetBindingValueByIndex(2, FMath::Clamp(Limit, 1, EW::DiscoveryQuota));
    S.SetBindingValueByIndex(3, FMath::Max(0, Offset));
    while (S.Step() == ESQLitePreparedStatementStepResult::Row)
    {
        EW::PlaceBookmark P; if (ReadPlace(S, P)) Result.Add(MoveTemp(P));
    }
    return Result;
}

int32 FEWSaveStore::WorldCount()
{
    if (!DB.IsValid()) return 0;
    auto S = DB.PrepareStatement(TEXT("SELECT COUNT(*) FROM positions;"));
    int32 N = 0;
    if (S.IsValid() && S.Step() == ESQLitePreparedStatementStepResult::Row) S.GetColumnValueByIndex(0, N);
    return N;
}

TArray<EW::PlaceBookmark> FEWSaveStore::Worlds(int32 Offset, int32 Limit)
{
    TArray<EW::PlaceBookmark> Result;
    if (!DB.IsValid()) return Result;
    auto S = DB.PrepareStatement(*(FString(TEXT("SELECT ")) + Columns + TEXT(" FROM positions ORDER BY rowid DESC LIMIT ?1 OFFSET ?2;")));
    if (!S.IsValid()) return Result;
    S.SetBindingValueByIndex(1, FMath::Clamp(Limit, 1, 30)); S.SetBindingValueByIndex(2, FMath::Max(0, Offset));
    while (S.Step() == ESQLitePreparedStatementStepResult::Row)
    { EW::PlaceBookmark P; if (ReadPlace(S, P)) Result.Add(MoveTemp(P)); }
    return Result;
}

bool FEWSaveStore::Export(const FString& World, FString& OutputPath)
{
    EW::WorldDescriptor Descriptor; FString Error;
    if (!EW::WorldDescriptor::Parse(World, Descriptor, Error)) return Fail(Error);
    auto Object = MakeShared<FJsonObject>();
    Object->SetStringField(TEXT("format"), TEXT("endless-world-discoveries-v1"));
    Object->SetStringField(TEXT("world"), World);
    TArray<TSharedPtr<FJsonValue>> List;
    for (const auto& P : Records(World, 0, EW::DiscoveryQuota))
    {
        auto Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("name"), P.Name); Item->SetStringField(TEXT("id"), P.Id);
        Item->SetStringField(TEXT("place_code"), P.Code()); Item->SetBoolField(TEXT("favourite"), P.bFavourite);
        List.Add(MakeShared<FJsonValueObject>(Item));
    }
    Object->SetArrayField(TEXT("discoveries"), List);
    FString JSON; auto Writer = TJsonWriterFactory<>::Create(&JSON);
    if (!FJsonSerializer::Serialize(Object, Writer)) return Fail(TEXT("記録をまとめられませんでした。"));
    const FString ExportDir = FPaths::Combine(Directory, TEXT("Exports"));
    if (!IFileManager::Get().MakeDirectory(*ExportDir, true)) return Fail(TEXT("書き出し先を作成できません。"));
    OutputPath = FPaths::Combine(ExportDir, TEXT("発見記録-") + FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S")) +
        TEXT("-") + FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(6) + TEXT(".json"));
    if (!FFileHelper::SaveStringToFile(JSON, *OutputPath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
        return Fail(TEXT("記録を書き出せませんでした。空き容量と権限を確認してください。"));
    LastError.Empty(); return true;
}
