#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "EWLocalizationAudit.generated.h"

// Isolated opt-in verification of localized production widgets and saved language.
UCLASS()
class ENDLESSWORLD_API AEWLocalizationAudit : public AActor
{
    GENERATED_BODY()
public:
    AEWLocalizationAudit();
    virtual void BeginPlay() override;
    virtual void Tick(float Delta) override;
private:
    int32 Phase=0;
    double Started=0,Stage=0;
    bool Finished=false,Failed=false,PersistOnly=false,RecordOpened=false,TitleCaptured=false;
    FString Report;
    TArray<TSharedPtr<class FJsonValue>> Checks,Surfaces;
    void Check(bool Pass,const FString& Name);
    FString Inspect(const TSharedRef<class SWidget>& Widget,const FString& Surface,bool English=true);
    void InspectTitle(const FString& Surface);
    void InspectMediaTablets(const FString& Surface,bool English);
    void InspectContent();
    FString ImagePath(const TCHAR* Name) const;
    void Finish(const FString& Error=FString());
};
