#pragma once
#include "CoreMinimal.h"
#include "SQLiteDatabase.h"
#include "EWFishingModel.h"

// Only local UI actions write this store. Network snapshots have no persistence API.
class FEWSocialStore
{
public:
    ~FEWSocialStore();
    bool Open(const FString& Directory);
    void Close();
    FString Get(const FString& Key,const FString& Default={});
    bool Put(const FString& Key,const FString& Value);
    TSet<FString> Flags(const FString& Prefix);
    bool Backup();
    bool AddCatch(const EWFishing::FCatch& Catch,bool& Added);
    TArray<EWFishing::FRecord> FishRecords();
    bool SetDisplayFish(const FString& Species);
    bool Integrity() const { return DB.IsValid() && DB.PerformQuickIntegrityCheck(); }
    FString Error() const { return LastError; }
    FString Notice() const { return Recovery; }
private:
    FSQLiteDatabase DB;
    FString Root,Path,LastError,Recovery;
    bool Fail(const FString& Text) { LastError=Text;return false; }
};
