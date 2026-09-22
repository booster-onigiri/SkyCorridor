#include "EWQualityAudit.h"
#include "EWGameInstance.h"
#include "EWChunkManager.h"
#include "EWMediaScreen.h"
#include "EWBrowserSurface.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "UnrealClient.h"

AEWQualityAudit::AEWQualityAudit(){PrimaryActorTick.bCanEverTick=true;}
void AEWQualityAudit::BeginPlay()
{
    Super::BeginPlay();Started=Stage=FPlatformTime::Seconds();
    FParse::Value(FCommandLine::Get(),TEXT("EWReport="),Report);
    URL=TEXT("https://www.youtube.com/watch?v=aqz-KE-bpKQ");FParse::Value(FCommandLine::Get(),TEXT("EWQualityURL="),URL);
    if(Report.IsEmpty() || IFileManager::Get().FileExists(*Report)){Report.Empty();Finish(TEXT("fresh explicit report required"));}
}
void AEWQualityAudit::Finish(const FString& Error)
{
    if(Finished)return;Finished=true;
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),Error.IsEmpty());O->SetStringField(TEXT("failure"),Error);
    O->SetStringField(TEXT("url"),URL);O->SetNumberField(TEXT("seconds"),FPlatformTime::Seconds()-Started);
    O->SetStringField(TEXT("scope"),TEXT("real YouTube settings selection and decoded source resolution, silent; world texture remains 1280x720"));
    if(auto* G=GetGameInstance<UEWGameInstance>())if(G->CinemaScreen)O->SetObjectField(TEXT("browser"),G->CinemaScreen->Evidence());
    FString Text;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Text));
    if(!Report.IsEmpty()){IFileManager::Get().MakeDirectory(*FPaths::GetPath(Report),true);FFileHelper::SaveStringToFile(Text,*Report);}
    FPlatformMisc::RequestExit(false);
}
void AEWQualityAudit::Tick(float Delta)
{
    Super::Tick(Delta);if(Finished)return;const double Now=FPlatformTime::Seconds();
    if(Now-Started>180){Finish(TEXT("YouTube did not reach selected maximum resolution within timeout"));return;}
    auto* G=GetGameInstance<UEWGameInstance>();if(!G || !G->Manager || !G->CinemaScreen || G->Manager->IsTravelling() || G->Manager->ReadyCount()<49)return;
    if(Phase==0){G->VisitCinema();Phase=1;Stage=Now;return;}
    auto* S=G->CinemaScreen.Get();auto B=S->Browser();
    if(Phase==1 && Now-Stage>4 && S->Available())
    {S->SitSeat(22);S->LookAtScreen();B->Search(URL);Phase=2;Stage=Now;return;}
    if(Phase==2)
    {
        if(Now-LastResume>8){LastResume=Now;B->EnableSound();}
        const auto E=B->Evidence();const double Wanted=E->GetNumberField(TEXT("quality_height"));
        if(E->GetStringField(TEXT("quality_status"))==TEXT("selected") && !E->GetBoolField(TEXT("video_ad")) && Wanted>=720 && E->GetNumberField(TEXT("video_height"))>=Wanted && E->GetNumberField(TEXT("video_time"))>2)
        {FScreenshotRequest::RequestScreenshot(FPaths::GetPath(Report)/TEXT("youtube-highest.png"),false,false);Phase=3;Stage=Now;}
    }
    if(Phase==3 && Now-Stage>2)Finish();
}
