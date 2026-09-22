#pragma once
#include "CoreMinimal.h"
#include "EWWorld.h"
#include "SQLiteDatabase.h"

class ENDLESSWORLD_API FEWSaveStore
{
public:
    explicit FEWSaveStore(int32 InQuota = EW::DiscoveryQuota) : Quota(InQuota) {}
    ~FEWSaveStore();
    bool Open(const FString& Directory, bool ReadOnly = false);
    void Close();
    bool Record(const EW::PlaceBookmark& Place);
    bool SetFavourite(const FString& World, const FString& Id, bool Value);
    bool SaveCurrent(const EW::PlaceBookmark& Place);
    bool LoadCurrent(EW::PlaceBookmark& Place);
    bool LoadWorldPosition(const FString& World, EW::PlaceBookmark& Place);
    bool HasRecord(const FString& World, const FString& Id);
    int32 Count(const FString& World);
    TArray<EW::PlaceBookmark> Records(const FString& World, int32 Offset = 0, int32 Limit = 50);
    TArray<EW::PlaceBookmark> Worlds(int32 Offset = 0, int32 Limit = 12);
    int32 WorldCount();
    bool Export(const FString& World, FString& OutputPath);
    // Optional additive table; the old positions/discoveries schema and
    // SQLite user_version=1 remain readable by BuildTraversal28.
    bool LoadResidenceState(const FString& World,const FString& Id,int32 Revision,int32& FlowMode);
    bool SaveResidenceState(const FString& World,const FString& Id,int32 Revision,int32 FlowMode);
    bool IsOpen() const { return DB.IsValid(); }
    FString Error() const { return LastError; }
    FString Notice() const { return RecoveryNotice; }
    FString Root() const { return Directory; }
    FString Filename() const { return DatabasePath; }
    bool Integrity() const { return DB.IsValid() && DB.PerformQuickIntegrityCheck(); }
private:
    FSQLiteDatabase DB;
    FString Directory, DatabasePath, LastError, RecoveryNotice;
    int32 Quota;
    bool bReadOnly = false;
    bool Backup();
    bool BeginWrite();
    bool FinishWrite(bool Success);
    bool Fail(const FString& Message);
    bool CheckPlace(const EW::PlaceBookmark& Place);
    bool ReadPlace(const FSQLitePreparedStatement& Statement, EW::PlaceBookmark& Out);
};
