#include "EWLocalizationAudit.h"
#include "EWLocalization.h"
#include "EWGameInstance.h"
#include "EWChunkManager.h"
#include "EWCharacter.h"
#include "EWInterface.h"
#include "EWTerminal.h"
#include "EWMediaScreen.h"
#include "EWPhotoMode.h"
#include "EWFishing.h"
#include "EWSkyrailPlan.h"
#include "EWHotelPlan.h"
#include "EWExplorationPlan.h"
#include "EWLift.h"
#include "EWSocialSession.h"
#include "EWCinemaSession.h"
#include "Camera/CameraComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Layout/Children.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
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
struct FLocalizedButton { FString Text;bool Enabled;TSharedPtr<SButton> Widget; };
FString WidgetText(const TSharedRef<SWidget>& Widget,bool SkipLanguage=false)
{
    if(!Widget->GetVisibility().IsVisible())return {};
    if(Widget->GetTypeAsString()==TEXT("STextBlock"))
    {
        const FString Text=StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString();
        return SkipLanguage && Text.StartsWith(TEXT("Language /"))?FString():Text;
    }
    FString Result;
    if(auto* Children=Widget->GetChildren())for(int32 I=0;I<Children->Num();++I)
    {
        const FString Part=WidgetText(Children->GetChildAt(I),SkipLanguage);
        if(!Part.IsEmpty()){if(!Result.IsEmpty())Result+=TEXT("\n");Result+=Part;}
    }
    return Result;
}
void CollectButtons(const TSharedRef<SWidget>& Widget,TArray<FLocalizedButton>& Out,bool ParentEnabled=true)
{
    if(!Widget->GetVisibility().IsVisible())return;
    const bool Enabled=ParentEnabled && Widget->IsEnabled();
    if(Widget->GetTypeAsString()==TEXT("SButton"))Out.Add({WidgetText(Widget),Enabled,StaticCastSharedRef<SButton>(Widget)});
    if(auto* Children=Widget->GetChildren())for(int32 I=0;I<Children->Num();++I)CollectButtons(Children->GetChildAt(I),Out,Enabled);
}
bool Matches(const TSharedRef<SWidget>& View,const TCHAR* Label,bool Enabled,bool Badge=false)
{
    TArray<FLocalizedButton> Buttons;CollectButtons(View,Buttons);int32 Found=0;
    for(const auto& B:Buttons)if(B.Text.Contains(Label))
    {
        if(B.Enabled!=Enabled || (Badge && !B.Text.Contains(TEXT("In development"))))return false;
        ++Found;
    }
    return Found==1;
}
bool Activate(const TSharedRef<SWidget>& View,const FString& Label)
{
    TArray<FLocalizedButton> Buttons;CollectButtons(View,Buttons);
    for(const auto& B:Buttons)if(B.Enabled && B.Text.Contains(Label))
    {
        // Deliver Slate's real Accept key handlers directly. No desktop input,
        // cursor movement, private widget state or test-only UI action is used.
        const FKeyEvent Key(EKeys::Enter,FModifierKeysState(),0,false,0,0);
        const auto Down=B.Widget->OnKeyDown(B.Widget->GetCachedGeometry(),Key);
        const auto Up=B.Widget->OnKeyUp(B.Widget->GetCachedGeometry(),Key);
        return Down.IsEventHandled() && Up.IsEventHandled();
    }
    return false;
}
TSharedRef<SEWOverlay> Overlay(UEWGameInstance* G){return SNew(SEWOverlay).Owner(G);}
void UpdateTabletAttributes(const TSharedRef<SWidget>& Widget)
{
    // SEditableText's hint is a cached Slate attribute. Its collapsed controls
    // have not received a render prepass yet when the language button changes.
    // Evaluate the real bound attributes throughout the existing widget tree,
    // including those children, exactly as their visible prepass would do.
    Widget->UpdateAllAttributes();
    if(auto* Children=Widget->GetChildren())for(int32 I=0;I<Children->Num();++I)
        UpdateTabletAttributes(Children->GetChildAt(I));
}
FString MediaText(const TSharedRef<SWidget>& Widget)
{
    // Include the closed tablet's controls as well as the visible idle screen.
    // These are the actual persistent widgets, never a reconstructed test copy.
    if(Widget->GetTypeAsString()==TEXT("STextBlock"))
        return StaticCastSharedRef<STextBlock>(Widget)->GetText().ToString();
    if(Widget->GetTypeAsString()==TEXT("SEditableTextBox"))
        return StaticCastSharedRef<SEditableTextBox>(Widget)->GetHintText().ToString();
    FString Text;
    if(auto* Children=Widget->GetChildren())for(int32 I=0;I<Children->Num();++I)
    {const FString Part=MediaText(Children->GetChildAt(I));if(!Part.IsEmpty())Text+=Part+TEXT("\n");}
    return Text;
}
}

AEWLocalizationAudit::AEWLocalizationAudit()
{
    PrimaryActorTick.bCanEverTick=true;PrimaryActorTick.TickGroup=TG_PostUpdateWork;
}
void AEWLocalizationAudit::BeginPlay()
{
    Super::BeginPlay();Started=Stage=FPlatformTime::Seconds();
    PersistOnly=FParse::Param(FCommandLine::Get(),TEXT("EWLanguagePersistAudit"));
    Report=FPaths::ProjectSavedDir()/TEXT("Verification/localization-audit.json");
    FParse::Value(FCommandLine::Get(),TEXT("EWReport="),Report);
    IFileManager::Get().MakeDirectory(*FPaths::GetPath(Report),true);
    if(FParse::Param(FCommandLine::Get(),TEXT("EWEnableExperimentalOnline")) || FParse::Param(FCommandLine::Get(),TEXT("EWEnableExperimentalWorkshop")))
        Finish(TEXT("Run the localization audit with public-default feature gates."));
}
void AEWLocalizationAudit::Check(bool Pass,const FString& Name)
{
    auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("check"),Name);O->SetBoolField(TEXT("pass"),Pass);
    Checks.Add(MakeShared<FJsonValueObject>(O));Failed|=!Pass;
}
FString AEWLocalizationAudit::ImagePath(const TCHAR* Name) const
{return FPaths::GetPath(Report)/(FPaths::GetBaseFilename(Report)+TEXT("-")+Name+TEXT(".png"));}
FString AEWLocalizationAudit::Inspect(const TSharedRef<SWidget>& View,const FString& Surface,bool English)
{
    const FString Text=WidgetText(View),Filtered=WidgetText(View,true);
    auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("surface"),Surface);O->SetStringField(TEXT("text"),Text);
    Surfaces.Add(MakeShared<FJsonValueObject>(O));
    Check(!Filtered.IsEmpty(),Surface+TEXT(": production widget has text"));
    Check(English?!EWL::HasJapanese(Filtered):EWL::HasJapanese(Filtered),Surface+TEXT(": expected display language"));
    return Text;
}
void AEWLocalizationAudit::InspectTitle(const FString& Surface)
{
    auto* G=GetGameInstance<UEWGameInstance>();const auto View=Overlay(G);Inspect(View,Surface);
    for(const TCHAR* Label:{TEXT("Find / Host a City"),TEXT("World Eggs & Add-ons"),TEXT("Watch Movies Together")})
        Check(Matches(View,Label,false,true),Surface+TEXT(": disabled and marked in development: ")+Label);
    for(const TCHAR* Label:{TEXT("Graphics & Controls"),TEXT("Fish Journal / Go Fishing"),TEXT("Visit the Cinema"),TEXT("Visit the Upper-floor Suites")})
        Check(Matches(View,Label,true),Surface+TEXT(": enabled local action: ")+Label);
    Check(Matches(View,G->CanContinue()?TEXT("Continue"):TEXT("Begin exploring"),true),Surface+TEXT(": solo start enabled"));
    for(EEWMenu Menu:{EEWMenu::City,EEWMenu::Online,EEWMenu::Workshop,EEWMenu::Chess})
    {
        G->SetMenu(Menu);
        Check(G->Menu()==EEWMenu::Main && !G->IsMenuAvailable(Menu),FString::Printf(TEXT("English direct menu %d remains blocked"),int32(Menu)));
    }
}
void AEWLocalizationAudit::InspectMediaTablets(const FString& Surface,bool English)
{
    auto* G=GetGameInstance<UEWGameInstance>();
    for(AEWMediaScreen* Screen:{G->MediaScreen.Get(),G->CinemaScreen.Get(),G->SkyTheatre.Get()})
    {
        const FString Name=Surface+TEXT(": ")+(Screen?Screen->GetName():TEXT("missing screen"));
        const TSharedPtr<SWidget> View=Screen?Screen->TabletFocusWidget():nullptr;
        Check(View.IsValid(),Name+TEXT(" persistent tablet exists"));if(!View)continue;
        UpdateTabletAttributes(View.ToSharedRef());
        const FString Text=MediaText(View.ToSharedRef());
        auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("surface"),Name);O->SetStringField(TEXT("text"),Text);
        Surfaces.Add(MakeShared<FJsonValueObject>(O));
        Check(English?!EWL::HasJapanese(Text):EWL::HasJapanese(Text),Name+TEXT(" idle and controls follow language switch"));
        Check(Text.Contains(Screen->Title()) && Text.Contains(English?TEXT("Choose a video to play"):TEXT("上映する動画を選ぶ")) &&
            Text.Contains(English?TEXT("Search YouTube / Paste video URL"):TEXT("YouTubeで検索 / 動画URLを貼り付け")),Name+TEXT(" title, idle label and search hint update"));
    }
}
void AEWLocalizationAudit::InspectContent()
{
    auto* G=GetGameInstance<UEWGameInstance>();auto* T=G->Terminal.Get();
    Check(T->Places().Num()==15,TEXT("all 15 memory places loaded"));
    for(int32 I=0;I<T->Places().Num();++I)
    {
        const auto& P=T->Places()[I];
        const FString Values[]={P.Area,P.Place.Name,P.Moment,P.Record,P.Direction};
        int32 K=0;for(const FString& Source:Values)
        {
            const FString Translated=EWL::Translate(Source);
            Check(!Translated.IsEmpty() && !EWL::HasJapanese(Translated),FString::Printf(TEXT("memory %d field %d has complete English text"),I,K++));
        }
        Check(!EWL::HasJapanese(T->RouteText(I)),FString::Printf(TEXT("memory %d route is English"),I));
    }
    for(int32 I=0;I<8;++I)
    {
        Check(!EWL::HasJapanese(EWL::Translate(EWHotelPlan::Name(I))),FString::Printf(TEXT("hotel %d name is English"),I));
        Check(!EWL::HasJapanese(EWL::Translate(EWHotelPlan::Description(I))),FString::Printf(TEXT("hotel %d description is English"),I));
    }
    for(int32 Line=0;Line<2;++Line)
    {
        Check(!EWL::HasJapanese(EWL::Translate(EWSkyrailPlan::LineName(Line))),FString::Printf(TEXT("rail line %d English"),Line));
        for(int32 I=0;I<EWSkyrailPlan::Count(Line);++I)
            Check(!EWL::HasJapanese(EWL::Translate(EWSkyrailPlan::Name(I,Line))),FString::Printf(TEXT("rail line %d station %d English"),Line,I));
    }
    int32 LiftCount=0;bool LiftsEnglish=true;FString FailedLifts;
    for(TActorIterator<AEWLift> It(GetWorld());It;++It)for(const auto& Stop:It->Spec.Stops)
    {
        ++LiftCount;const FString Translated=EWL::Translate(Stop.Label);
        if(EWL::HasJapanese(Translated)){LiftsEnglish=false;if(!FailedLifts.Contains(Stop.Label))FailedLifts+=TEXT(" [")+Stop.Label+TEXT(" => ")+Translated+TEXT("]");}
    }
    Check(LiftCount>0 && LiftsEnglish,TEXT("all loaded elevator destination labels translate")+FailedLifts);
    for(const auto& Fish:EWFishing::Species())
    {
        Check(!EWL::HasJapanese(EWL::Translate(Fish.Name)) && !EWL::HasJapanese(EWL::Translate(Fish.Description)),TEXT("fish name and description: ")+Fish.Id);
        Check(!EWL::HasJapanese(EWL::Translate(EWFishing::SiteDescription(Fish.Sites))) && !EWL::HasJapanese(EWL::Translate(EWFishing::TimeDescription(Fish.Hours))),TEXT("fish habitat and hours: ")+Fish.Id);
    }
    Check(EWL::Translate(TEXT("player-input-0123"))==TEXT("player-input-0123"),TEXT("unknown text is preserved"));
    Check(EWL::Translate(T->Places()[0].Place.Id)==T->Places()[0].Place.Id,TEXT("stable memory IDs are preserved"));
}
void AEWLocalizationAudit::Finish(const FString& Error)
{
    if(Finished)return;Finished=true;
    auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),!Failed && Error.IsEmpty());O->SetStringField(TEXT("failure"),Error);
    O->SetBoolField(TEXT("persistence_only"),PersistOnly);O->SetStringField(TEXT("language"),EWL::Language());
    O->SetNumberField(TEXT("seconds"),FPlatformTime::Seconds()-Started);
    O->SetStringField(TEXT("scope"),TEXT("Production Slate text and disabled states, real Slate button Accept dispatch, language preference reload, map and all memory narratives, hotel/transit/fish labels. First-memory recording uses an isolated position/camera fixture; not a physical input or full gameplay test. No online or microphone is started."));
    O->SetArrayField(TEXT("checks"),Checks);O->SetArrayField(TEXT("surfaces"),Surfaces);
    FString Json;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&Json));
    FFileHelper::SaveStringToFile(Json,*Report,FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
    FPlatformMisc::RequestExit(false);
}
void AEWLocalizationAudit::Tick(float Delta)
{
    Super::Tick(Delta);if(Finished)return;
    const double Now=FPlatformTime::Seconds(),Age=Now-Stage;
    if(Now-Started>240){Finish(TEXT("localization audit timed out"));return;}
    auto* G=GetGameInstance<UEWGameInstance>();auto* P=Cast<AEWCharacter>(UGameplayStatics::GetPlayerCharacter(this,0));
    if(!G || !P || !G->Manager || !G->Terminal || !G->Fishing || !G->PhotoMode || !G->SocialSession || !G->CinemaSession)return;
    if(G->Manager->IsTravelling() || P->IsStreamingHeld() || G->Manager->ReadyCount()<49 || G->Terminal->Places().Num()<15)return;
    auto* T=G->Terminal.Get();
    if(PersistOnly)
    {
        if(Phase==0)
        {
            Check(EWL::Language()==TEXT("en"),TEXT("English preference restored on fresh process"));
            Check(!G->SessionStarted() && G->Menu()==EEWMenu::Main,TEXT("persistence test starts on normal title"));
            InspectTitle(TEXT("persisted English title"));FScreenshotRequest::RequestScreenshot(ImagePath(TEXT("title")),true,false);
            Phase=1;Stage=Now;return;
        }
        if(Age>1){Check(IFileManager::Get().FileSize(*ImagePath(TEXT("title")))>1000,TEXT("persisted title screenshot exists"));Finish();}
        return;
    }
    if(Phase==0)
    {
        Check(!G->SessionStarted() && G->Menu()==EEWMenu::Main,TEXT("audit starts on normal public title"));
        G->SetLanguage(TEXT("ja"));Inspect(Overlay(G),TEXT("Japanese title"),false);InspectMediaTablets(TEXT("Japanese before title switch"),false);
        Check(Activate(Overlay(G),TEXT("Language /")) && EWL::IsEnglish(),TEXT("title language button switches Japanese to English"));
        InspectTitle(TEXT("English title"));InspectMediaTablets(TEXT("English after title switch"),true);
        Phase=1;Stage=Now;return;
    }
    if(Phase==1 && !TitleCaptured && Age>.5)
    {
        // The world-space tablet draws at 10 Hz. Capture after its render target
        // has had time to redraw, rather than in the same frame as the switch.
        FScreenshotRequest::RequestScreenshot(ImagePath(TEXT("title")),true,false);
        if(G->MediaScreen)G->MediaScreen->CaptureTablet(ImagePath(TEXT("plaza-tablet")));
        TitleCaptured=true;Stage=Now;return;
    }
    if(Phase==1 && TitleCaptured && Age>1)
    {
        G->SetMenu(EEWMenu::Settings);Inspect(Overlay(G),TEXT("English settings"));
        Check(Activate(Overlay(G),TEXT("Language /")) && !EWL::IsEnglish(),TEXT("settings language button switches English to Japanese"));
        InspectMediaTablets(TEXT("Japanese after settings switch"),false);
        Inspect(Overlay(G),TEXT("Japanese settings"),false);
        Check(Activate(Overlay(G),TEXT("Language /")) && EWL::IsEnglish(),TEXT("settings language button switches Japanese to English"));
        InspectMediaTablets(TEXT("English after settings switch"),true);
        Check(!G->SetLanguage(TEXT("invalid")) && EWL::IsEnglish(),TEXT("invalid language leaves selection unchanged"));
        FScreenshotRequest::RequestScreenshot(ImagePath(TEXT("settings")),true,false);
        Phase=2;Stage=Now;return;
    }
    if(Phase==2 && Age>1)
    {
        InspectContent();
        for(EEWMenu Menu:{EEWMenu::Explore,EEWMenu::SkyResidences,EEWMenu::Journal,EEWMenu::Worlds,EEWMenu::SaveFailure,EEWMenu::FishJournal,EEWMenu::Photo})
        {
            G->SetMenu(Menu);Inspect(Overlay(G),FString::Printf(TEXT("English menu %d"),int32(Menu)));
        }
        G->SetMenu(EEWMenu::Main);G->ContinueWorld();Check(G->SessionStarted(),TEXT("solo play starts in English"));
        Phase=3;Stage=Now;return;
    }
    if(Phase==3 && Age>1)
    {
        G->SetMenu(EEWMenu::None);T->Open();T->ShowPage(0);const auto View=T->FocusWidget();
        Check(View.IsValid(),TEXT("phone production widget available"));if(!View){Finish(TEXT("phone widget missing"));return;}
        Inspect(View.ToSharedRef(),TEXT("English phone home"));Check(Matches(View.ToSharedRef(),TEXT("Friends"),false,true),TEXT("English Friends icon remains disabled with badge"));
        for(const TCHAR* Label:{TEXT("Observe"),TEXT("Map"),TEXT("Records"),TEXT("YouTube"),TEXT("Camera")})
            Check(Matches(View.ToSharedRef(),Label,true),FString(TEXT("English local phone app enabled: "))+Label);
        T->ShowPage(4);Check(T->PageIndex()==0,TEXT("English Friends direct page is blocked"));
        Phase=4;Stage=Now;return;
    }
    if(Phase==4 && Age>2)
    {
        T->CaptureScreen(ImagePath(TEXT("phone-home")));FScreenshotRequest::RequestScreenshot(ImagePath(TEXT("phone-world")),true,false);
        T->ShowPage(1);Inspect(T->FocusWidget().ToSharedRef(),TEXT("English phone map"));Phase=5;Stage=Now;return;
    }
    if(Phase==5 && Age>2)
    {
        T->CaptureScreen(ImagePath(TEXT("phone-map")));T->ShowPage(2);Inspect(T->FocusWidget().ToSharedRef(),TEXT("English phone record list"));Phase=6;Stage=Now;return;
    }
    if(Phase==6 && Age>2)
    {
        T->CaptureScreen(ImagePath(TEXT("phone-records")));T->ShowPage(5);Inspect(T->FocusWidget().ToSharedRef(),TEXT("English phone observe"));Phase=7;Stage=Now;return;
    }
    if(Phase==7 && Age>2)
    {
        T->CaptureScreen(ImagePath(TEXT("phone-observe")));T->Close();
        auto Place=T->Places()[0].Place;Place.LocalPosition+=FVector(-210,0,88);Place.Yaw=0;G->Visit(Place);
        if(!T->Observing())T->ToggleObservation();Phase=8;Stage=Now;return;
    }
    if(Phase==8)
    {
        // Place the isolated player by the real first memory, holding a stable
        // view while normal observation, line-of-sight, and save logic run.
        const auto& Place=T->Places()[0].Place;const FVector At=G->Manager->ToRender(Place.Coord,Place.LocalPosition);
        P->SetActorLocation(At+FVector(-210,0,88),false);P->GetCharacterMovement()->DisableMovement();
        if(auto* PC=P->GetController())PC->SetControlRotation((At+FVector(0,0,100)-P->Camera->GetComponentLocation()).Rotation());
        if(T->ObservedSecondsForAudit()<2.2 && Age<20)return;
        Check(T->ObservedSecondsForAudit()>=2,TEXT("first memory observed through normal proximity and visibility logic"));
        Check(T->RecordNearby() && T->Recorded(0),TEXT("first memory recorded and saved in isolated data"));
        P->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
        T->Open();T->ShowPage(2);
        RecordOpened=T->Recorded(0) && Activate(T->FocusWidget().ToSharedRef(),EWL::Translate(Place.Name));
        Check(RecordOpened,TEXT("real record card opens through Slate Accept handlers"));
        const FString Text=Inspect(T->FocusWidget().ToSharedRef(),TEXT("English memory detail"));
        Check(Text.Contains(EWL::Translate(T->Places()[0].Record)),TEXT("memory detail contains the complete English narrative"));
        Phase=9;Stage=Now;return;
    }
    if(Phase==9 && Age>2)
    {
        T->CaptureScreen(ImagePath(TEXT("phone-memory-detail")));
        Check(G->SetLanguage(TEXT("ja")) && !EWL::IsEnglish(),TEXT("switch back to Japanese during a session"));
        T->ShowPage(2);Inspect(T->FocusWidget().ToSharedRef(),TEXT("Japanese phone record detail"),false);
        Check(T->Recorded(0),TEXT("language changes retain collected memory"));
        Check(G->SetLanguage(TEXT("en")) && EWL::IsEnglish(),TEXT("return to English during a session"));
        FString SavedLanguage;Check(G->Store() && FFileHelper::LoadFileToString(SavedLanguage,*(G->Store()->Root()/TEXT("language.txt"))) && SavedLanguage.TrimStartAndEnd()==TEXT("en"),TEXT("English language preference saved to isolated data"));
        auto& Net=G->SocialSession->Network();Check(!Net.LoggedIn() && !Net.Busy() && !Net.VoiceReady() && !G->SocialSession->Active() && !G->SocialSession->Speaking(),TEXT("online city and voice remain inactive"));
        Check(!G->CinemaSession->Connected() && !G->CinemaSession->Hosting(),TEXT("shared cinema remains inactive"));
        for(const TCHAR* Name:{TEXT("title"),TEXT("settings"),TEXT("plaza-tablet"),TEXT("phone-home"),TEXT("phone-world"),TEXT("phone-map"),TEXT("phone-records"),TEXT("phone-observe"),TEXT("phone-memory-detail")})
            Check(IFileManager::Get().FileSize(*ImagePath(Name))>1000,FString(TEXT("rendered image exists: "))+Name);
        Finish();
    }
}
