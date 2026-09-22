#include "EWPublicMenuAudit.h"
#include "EWGameInstance.h"
#include "EWChunkManager.h"
#include "EWCharacter.h"
#include "EWInterface.h"
#include "EWTerminal.h"
#include "EWPhotoMode.h"
#include "EWSocialSession.h"
#include "EWCinemaSession.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Layout/Children.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "UnrealClient.h"

namespace
{
struct FPublicButton { FString Text; bool Enabled = false; };
FString WidgetText(const TSharedRef<SWidget>& Widget)
{
    if (Widget->GetTypeAsString() == TEXT("STextBlock"))
        return StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString();
    FString Result;
    if (FChildren* Children = Widget->GetChildren())
        for (int32 I = 0; I < Children->Num(); ++I)
        {
            const FString Part = WidgetText(Children->GetChildAt(I));
            if (!Part.IsEmpty()) { if (!Result.IsEmpty()) Result += TEXT(" "); Result += Part; }
        }
    return Result;
}
void CollectButtons(const TSharedRef<SWidget>& Widget, TArray<FPublicButton>& Out, bool ParentEnabled = true)
{
    const bool Enabled = ParentEnabled && Widget->IsEnabled();
    if (Widget->GetTypeAsString() == TEXT("SButton"))
        Out.Add({WidgetText(Widget), Enabled});
    if (FChildren* Children = Widget->GetChildren())
        for (int32 I = 0; I < Children->Num(); ++I)
            CollectButtons(Children->GetChildAt(I), Out, Enabled);
}
bool Matches(const TArray<FPublicButton>& Buttons, const TCHAR* Label, bool Enabled, bool DevelopmentBadge = false)
{
    int32 Found = 0;
    for (const FPublicButton& Button : Buttons)
        if (Button.Text.Contains(Label))
        {
            if (Button.Enabled != Enabled || (DevelopmentBadge && !Button.Text.Contains(TEXT("開発中")))) return false;
            ++Found;
        }
    return Found == 1;
}
void AddEvidence(const TArray<FPublicButton>& Found, const FString& Surface, TArray<TSharedPtr<FJsonValue>>& Out)
{
    for (const FPublicButton& Button : Found)
    {
        auto O = MakeShared<FJsonObject>();
        O->SetStringField(TEXT("surface"), Surface);
        O->SetStringField(TEXT("label"), Button.Text);
        O->SetBoolField(TEXT("enabled"), Button.Enabled);
        Out.Add(MakeShared<FJsonValueObject>(O));
    }
}
}

AEWPublicMenuAudit::AEWPublicMenuAudit()
{
    PrimaryActorTick.bCanEverTick = true;
    PrimaryActorTick.TickGroup = TG_PostUpdateWork;
}
void AEWPublicMenuAudit::BeginPlay()
{
    Super::BeginPlay(); Started = Stage = FPlatformTime::Seconds();
    Report = FPaths::ProjectSavedDir() / TEXT("Verification/public-menu-audit.json");
    FParse::Value(FCommandLine::Get(), TEXT("EWReport="), Report);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Report), true);
    if (FParse::Param(FCommandLine::Get(), TEXT("EWEnableExperimentalOnline")) ||
        FParse::Param(FCommandLine::Get(), TEXT("EWEnableExperimentalWorkshop")))
        Finish(TEXT("Run this public-default audit without experimental opt-in flags."));
}
void AEWPublicMenuAudit::Check(bool Pass, const FString& Name)
{
    auto O = MakeShared<FJsonObject>(); O->SetStringField(TEXT("check"), Name); O->SetBoolField(TEXT("pass"), Pass);
    Checks.Add(MakeShared<FJsonValueObject>(O)); Failed |= !Pass;
}
FString AEWPublicMenuAudit::ImagePath(const TCHAR* Name) const
{
    return FPaths::GetPath(Report) / (FPaths::GetBaseFilename(Report) + TEXT("-") + Name + TEXT(".png"));
}
void AEWPublicMenuAudit::InspectMain(bool Paused)
{
    auto* G = GetGameInstance<UEWGameInstance>();
    // Construct the production widget, then inspect real enabled attributes and
    // rendered text descendants instead of repeating its availability predicate.
    const TSharedRef<SEWOverlay> View = SNew(SEWOverlay).Owner(G);
    TArray<FPublicButton> Found; CollectButtons(View, Found);
    const FString Surface = Paused ? TEXT("pause") : TEXT("title");
    AddEvidence(Found, Surface, Buttons);
    for (const TCHAR* Label : {TEXT("街を探す・街を開く"), TEXT("世界の卵・追加要素"), TEXT("みんなで映画を見る")})
        Check(Matches(Found, Label, false, true), Surface + TEXT(": disabled with development badge: ") + Label);
    for (const TCHAR* Label : {TEXT("画質と操作"), TEXT("水辺の魚図鑑・釣りに行く"), TEXT("映画館へ行く"), TEXT("上層の客室を訪ねる")})
        Check(Matches(Found, Label, true), Surface + TEXT(": working destination remains enabled: ") + Label);
    Check(Matches(Found, G->CanContinue() ? TEXT("続きから") : TEXT("散策を始める"), true), Surface + TEXT(": solo start remains enabled"));
    if (Paused)
    {
        Check(Matches(Found, TEXT("探索に戻る"), true), TEXT("pause: return to exploration remains enabled"));
        Check(Matches(Found, TEXT("写真を撮る"), true), TEXT("pause: photo mode remains enabled"));
    }
}
void AEWPublicMenuAudit::InspectPhone()
{
    auto* T = GetGameInstance<UEWGameInstance>()->Terminal.Get();
    const TSharedPtr<SWidget> View = T->FocusWidget();
    Check(View.IsValid(), TEXT("phone has its production Slate widget"));
    if (!View) return;
    TArray<FPublicButton> Found; CollectButtons(View.ToSharedRef(), Found);
    AddEvidence(Found, TEXT("phone home"), Buttons);
    Check(Matches(Found, TEXT("友人"), false, true), TEXT("phone: Friends disabled with development badge"));
    for (const TCHAR* Label : {TEXT("観測"), TEXT("地図"), TEXT("記録"), TEXT("YouTube"), TEXT("カメラ")})
        Check(Matches(Found, Label, true), FString(TEXT("phone: local app remains enabled: ")) + Label);
}
void AEWPublicMenuAudit::Finish(const FString& Error)
{
    if (Finished) return; Finished = true;
    auto O = MakeShared<FJsonObject>();
    O->SetBoolField(TEXT("success"), !Failed && Error.IsEmpty()); O->SetStringField(TEXT("failure"), Error);
    O->SetNumberField(TEXT("seconds"), FPlatformTime::Seconds() - Started);
    O->SetStringField(TEXT("scope"), TEXT("Actual production Slate button trees, public-default menu and phone guards, solo UI preservation. No online service, microphone, workshop generation, or personal save is exercised."));
    O->SetArrayField(TEXT("checks"), Checks); O->SetArrayField(TEXT("buttons"), Buttons);
    if (auto* G = GetGameInstance<UEWGameInstance>())
    {
        if (G->Store()) O->SetStringField(TEXT("isolated_save_root"), G->Store()->Root());
        if (G->SocialSession) O->SetObjectField(TEXT("social"), G->SocialSession->Evidence());
        if (G->CinemaSession) O->SetObjectField(TEXT("cinema"), G->CinemaSession->Evidence());
    }
    FString Json; FJsonSerializer::Serialize(O, TJsonWriterFactory<>::Create(&Json));
    FFileHelper::SaveStringToFile(Json, *Report, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    FPlatformMisc::RequestExit(false);
}
void AEWPublicMenuAudit::Tick(float Delta)
{
    Super::Tick(Delta); if (Finished) return;
    const double Now = FPlatformTime::Seconds(), Age = Now - Stage;
    if (Now - Started > 180) { Finish(TEXT("public menu audit timed out")); return; }
    auto* G = GetGameInstance<UEWGameInstance>();
    auto* P = Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
    if (!G || !G->Manager || !G->Terminal || !G->SocialSession || !G->CinemaSession || !G->PhotoMode || !P) return;
    if (G->Manager->IsTravelling() || P->IsStreamingHeld() || G->Manager->ReadyCount() < 49) return;
    const EEWMenu Blocked[] = {EEWMenu::City, EEWMenu::Online, EEWMenu::Workshop, EEWMenu::Chess};
    if (Phase == 0)
    {
        if (G->SessionStarted() || G->Menu() != EEWMenu::Main) { Finish(TEXT("audit must begin on normal title menu")); return; }
        Check(G->CanPlay(), TEXT("initial title permits solo play")); InspectMain(false);
        for (EEWMenu Menu : Blocked)
        {
            G->SetMenu(Menu);
            Check(!G->IsMenuAvailable(Menu) && G->Menu() == EEWMenu::Main,
                FString::Printf(TEXT("title: direct blocked menu %d preserves Main"), int32(Menu)));
        }
        G->SetMenu(EEWMenu::Settings); Check(G->Menu() == EEWMenu::Settings, TEXT("settings opens normally"));
        G->SetMenu(EEWMenu::City); Check(G->Menu() == EEWMenu::Settings, TEXT("blocked city preserves active settings"));
        G->SetMenu(EEWMenu::Main);
        FScreenshotRequest::RequestScreenshot(ImagePath(TEXT("title")), true, false);
        Phase = 1; Stage = Now; return;
    }
    if (Phase == 1 && Age > 1)
    {
        G->ContinueWorld(); Check(G->SessionStarted(), TEXT("normal solo start starts a session"));
        Phase = 2; Stage = Now; return;
    }
    if (Phase == 2 && Age > 1)
    {
        G->SetMenu(EEWMenu::Main); InspectMain(true);
        for (EEWMenu Menu : Blocked)
        {
            G->SetMenu(Menu);
            Check(G->Menu() == EEWMenu::Main, FString::Printf(TEXT("pause: direct blocked menu %d preserves Main"), int32(Menu)));
        }
        FScreenshotRequest::RequestScreenshot(ImagePath(TEXT("pause")), true, false);
        Phase = 3; Stage = Now; return;
    }
    if (Phase == 3 && Age > 1)
    {
        G->PhotoMode->Open(); Check(G->Menu() == EEWMenu::Photo && G->PhotoMode->Active(), TEXT("photo mode opens normally"));
        for (EEWMenu Menu : Blocked)
        {
            G->SetMenu(Menu);
            Check(G->Menu() == EEWMenu::Photo && G->PhotoMode->Active(),
                FString::Printf(TEXT("blocked menu %d does not close active photo mode"), int32(Menu)));
        }
        G->SetMenu(EEWMenu::None); G->Terminal->Open(); G->Terminal->ShowPage(0);
        Check(G->Menu() == EEWMenu::Terminal, TEXT("handheld terminal opens normally")); InspectPhone();
        G->Terminal->ShowPage(4);
        Check(G->Terminal->PageIndex() == 0 && G->Menu() == EEWMenu::Terminal, TEXT("direct Friends page request preserves phone home"));
        G->Terminal->InviteFriends();
        Check(G->Menu() == EEWMenu::Terminal, TEXT("direct phone invite request preserves terminal"));
        for (int32 Page : {1, 2, 3, 5})
        {
            G->Terminal->ShowPage(Page);
            Check(G->Terminal->PageIndex() == Page, FString::Printf(TEXT("local phone page %d still opens"), Page));
        }
        G->Terminal->ShowPage(0); Phase = 4; Stage = Now; return;
    }
    if (Phase == 4 && Age > 2)
    {
        G->Terminal->CaptureScreen(ImagePath(TEXT("phone-screen")));
        FScreenshotRequest::RequestScreenshot(ImagePath(TEXT("phone")), true, false);
        Phase = 5; Stage = Now; return;
    }
    if (Phase == 5 && Age > 2)
    {
        auto& Net = G->SocialSession->Network();
        Check(!Net.Configured() && !Net.LoggedIn() && !Net.Busy() && !Net.VoiceReady() && !G->SocialSession->Active() && !G->SocialSession->Speaking(),
            TEXT("EOS authentication, voice, and public city remain inactive"));
        Check(!G->CinemaSession->Connected() && !G->CinemaSession->Hosting() && G->CinemaSession->Invite().IsEmpty(),
            TEXT("cinema sharing remains inactive without a host or invite"));
        for (const TCHAR* Name : {TEXT("title"), TEXT("pause"), TEXT("phone-screen"), TEXT("phone")})
            Check(IFileManager::Get().FileSize(*ImagePath(Name)) > 1000, FString(TEXT("rendered screenshot exists: ")) + Name);
        Finish();
    }
}
