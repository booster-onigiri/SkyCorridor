#include "EWInterface.h"
#include "EWTerminal.h"
#include "EWMusic.h"
#include "EWWaterView.h"
#include "EWGameInstance.h"
#include "EWHotelPlan.h"
#include "EWExplorationPlan.h"
#include "EWLift.h"
#include "EWMediaScreen.h"
#include "EWCinemaSession.h"
#include "EWSocialSession.h"
#include "EWFishing.h"
#include "EWPhotoMode.h"
#include "EWConceptRuntime.h"
#include "EWBrowserSurface.h"
#include "EWChunkManager.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"
#include "Styling/CoreStyle.h"
#include "Misc/Paths.h"
#include "InputCoreTypes.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Framework/Application/SlateApplication.h"
#include "EWGamepadModule.h"
#include "HAL/PlatformApplicationMisc.h"
#include "HAL/PlatformProcess.h"

namespace
{
const FLinearColor Ink(.065, .1, .115, 1), Muted(.28, .36, .36, 1), Paper(.94, .955, .92, .97), Accent(.10, .30, .29, 1);
const FButtonStyle& PaperButtonStyle()
{
    static const FButtonStyle Style = FButtonStyle()
        .SetNormal(FSlateRoundedBoxBrush(FLinearColor(.78,.86,.81,1),6.f))
        .SetHovered(FSlateRoundedBoxBrush(FLinearColor(.88,.94,.87,1),6.f))
        .SetPressed(FSlateRoundedBoxBrush(FLinearColor(.59,.74,.66,1),6.f))
        .SetDisabled(FSlateRoundedBoxBrush(FLinearColor(.78,.81,.77,1),6.f));
    return Style;
}
const FEditableTextBoxStyle& PaperInputStyle()
{
    static const FEditableTextBoxStyle Style = FEditableTextBoxStyle()
        .SetBackgroundImageNormal(FSlateRoundedBoxBrush(FLinearColor(.98,.99,.96,1),6.f))
        .SetBackgroundImageHovered(FSlateRoundedBoxBrush(FLinearColor(1,1,.98,1),6.f))
        .SetBackgroundImageFocused(FSlateRoundedBoxBrush(FLinearColor(.94,1,.96,1),6.f))
        .SetBackgroundImageReadOnly(FSlateRoundedBoxBrush(FLinearColor(.84,.87,.83,1),6.f))
        .SetForegroundColor(Ink).SetBackgroundColor(FLinearColor::White);
    return Style;
}
}

void SEWOverlay::Construct(const FArguments& Args) { Owner = Args._Owner; Rebuild(); }
FSlateFontInfo SEWOverlay::Font(int32 Size) const
{
    return FSlateFontInfo(FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Fonts/DroidSansFallback.ttf")), Size);
}
TSharedRef<SWidget> SEWOverlay::Label(const FString& Text, int32 Size, FLinearColor Colour)
{
    return SNew(STextBlock).Text(FText::FromString(Text)).Font(Font(Size)).ColorAndOpacity(Colour).AutoWrapText(true);
}
TSharedRef<SWidget> SEWOverlay::Button(const FString& Text, TFunction<void()> Action, bool Enabled)
{
    const int32 Index = FocusControls.Num();
    auto Widget = SNew(SButton).ButtonStyle(&PaperButtonStyle()).ContentPadding(FMargin(14, 10))
        .ButtonColorAndOpacity_Lambda([this, Index] { return Index == CurrentFocus() ? FLinearColor(.61f, .94f, .84f, 1) : FLinearColor::White; })
        .IsEnabled(Enabled).OnClicked_Lambda([this, Index, Action]()
        {
            FocusIndex = Index;
            if (FocusControls.IsValidIndex(Index))
                if (auto Control = FocusControls[Index].Pin())
                    FSlateApplication::Get().SetKeyboardFocus(Control, EFocusCause::Mouse);
            Action(); return FReply::Handled();
        })
        [ SNew(STextBlock).Text(FText::FromString(Text)).Font(Font(16)).ColorAndOpacity(Ink) ];
    FocusControls.Add(Widget); FocusActions.Add(MoveTemp(Action));FocusLabels.Add(Text);
    return Widget;
}

void SEWOverlay::Rebuild()
{
    UEWGameInstance* GI = Owner.Get(); if (!GI) return;
    SetVisibility((GI->Menu()==EEWMenu::Terminal || GI->Menu()==EEWMenu::Monitor)?EVisibility::HitTestInvisible:EVisibility::SelfHitTestInvisible);
    const int32 OldFocus = CurrentFocus();
    bRestoreSettingsScroll = PreviousMenu == int32(GI->Menu()) &&
        GI->Menu() == EEWMenu::Settings && SettingsScroll.IsValid();
    SettingsScrollOffset = bRestoreSettingsScroll ? SettingsScroll->GetScrollOffset() : 0.f;
    SettingsScroll.Reset();
    FocusIndex = PreviousMenu == int32(GI->Menu()) ? FMath::Max(OldFocus, 0) : 0;
    PreviousMenu = int32(GI->Menu());
    FocusControls.Reset(); FocusActions.Reset();FocusLabels.Reset();CodeInput.Reset();
    const bool MenuOpen = GI->Menu() != EEWMenu::None;
    TSharedRef<SOverlay> Layers = SNew(SOverlay);
    if (!MenuOpen)
    {
        Layers->AddSlot().HAlign(HAlign_Left).VAlign(VAlign_Top).Padding(32)
        [ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
          .BorderBackgroundColor(FLinearColor(.025, .05, .065, .60)).Padding(FMargin(20, 12))
          .Visibility_Lambda([this] { return Owner.IsValid() && Owner->IsDesktopHUDActive() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
          [ SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()[SNew(STextBlock).Font(Font(22)).ColorAndOpacity(FLinearColor(.97, .96, .89, 1))
              .Text_Lambda([this] { return FText::FromString(Owner.IsValid() ? Owner->RegionText() : FString()); })]
            + SVerticalBox::Slot().AutoHeight().Padding(0, 6, 0, 0)
              [Label(TEXT("空の回廊  /  ENDLESS WORLD"), 11, FLinearColor(.75, .82, .81, 1))] ] ];
        Layers->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(16, 16, 16, 30)
        [ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
          .BorderBackgroundColor(FLinearColor(.025, .05, .065, .72)).Padding(FMargin(20, 12))
          .Visibility_Lambda([this] { return Owner.IsValid() && Owner->IsDesktopHUDActive() ? EVisibility::Collapsed : EVisibility::HitTestInvisible; })
          [ SNew(STextBlock).Font(Font(15)).ColorAndOpacity(FLinearColor(.97, .96, .9, 1))
            .Text_Lambda([this] { return FText::FromString(Owner.IsValid() ? Owner->NearbyText() : FString()); }) ] ];
    }
    else if(GI->Menu()!=EEWMenu::Terminal && GI->Menu()!=EEWMenu::Monitor)
    {
        TSharedRef<SWidget> Panel = GI->Menu() == EEWMenu::Journal ? JournalPanel() :
            GI->Menu() == EEWMenu::Settings ? SettingsPanel() : GI->Menu() == EEWMenu::Worlds ? WorldsPanel() :
            GI->Menu() == EEWMenu::SaveFailure ? SaveFailurePanel() : GI->Menu() == EEWMenu::Lift ? LiftPanel() :
            GI->Menu() == EEWMenu::Online ? OnlinePanel() : GI->Menu() == EEWMenu::City ? CityPanel() :
            GI->Menu() == EEWMenu::FishJournal ? FishJournalPanel() : GI->Menu() == EEWMenu::Catch ? CatchPanel() : GI->Menu()==EEWMenu::Photo ? PhotoPanel() : GI->Menu()==EEWMenu::Workshop ? WorkshopPanel() : GI->Menu()==EEWMenu::Chess ? ChessPanel() : GI->Menu()==EEWMenu::SkyResidences ? SkyResidencesPanel() : GI->Menu()==EEWMenu::Explore ? ExplorePanel() : MainPanel();
        const bool Wide = GI->Menu()==EEWMenu::Explore || GI->Menu()==EEWMenu::SkyResidences || GI->Menu() == EEWMenu::Journal || GI->Menu() == EEWMenu::Worlds || GI->Menu() == EEWMenu::City || GI->Menu()==EEWMenu::FishJournal || GI->Menu()==EEWMenu::Workshop || GI->Menu()==EEWMenu::Chess;
        const bool Photo=GI->Menu()==EEWMenu::Photo;
        Layers->AddSlot()
        [ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
          .BorderBackgroundColor(FLinearColor(.015, .035, .045, Photo?0:.25))
          .HAlign(Photo?HAlign_Right:HAlign_Left).VAlign(VAlign_Fill).Padding(FMargin(42, 32))
          [ SNew(SBox).WidthOverride(Photo ? 390 : Wide ? 870 : 570)
            [ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
              .BorderBackgroundColor(Paper).Padding(FMargin(30, 26))[Panel] ] ] ];
    }
    Layers->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Top).Padding(FMargin(30, 25))
    [ SNew(SBox).MaxDesiredWidth(800)
      [ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(FLinearColor(.025, .05, .065, .9)).Padding(FMargin(20, 12))
        .Visibility_Lambda([this] { return Owner.IsValid() && !Owner->IsDesktopHUDActive() && !Owner->StatusText().IsEmpty() ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
        [ SNew(STextBlock).Font(Font(16)).AutoWrapText(true).ColorAndOpacity(FLinearColor(.98, .97, .90, 1))
          .Text_Lambda([this] { return FText::FromString(Owner.IsValid() ? Owner->StatusText() : FString()); }) ] ] ];
    Layers->AddSlot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(24)
    [ SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
      .BorderBackgroundColor(FLinearColor(0, .015, .025, .8)).Padding(12)
      .Visibility_Lambda([this] { return Owner.IsValid() && !Owner->IsDesktopHUDActive() && Owner->bDiagnostics ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
      [ SNew(STextBlock).Font(Font(12)).ColorAndOpacity(FLinearColor(.85, 1, .88, 1))
        .Text_Lambda([this] { return FText::FromString(Owner.IsValid() ? Owner->DiagnosticsText() : FString()); }) ] ];
    ChildSlot[Layers];
}

TSharedRef<SWidget> SEWOverlay::WorkshopPanel()
{
    auto* G=Owner.Get();auto* C=G?G->Concepts.Get():nullptr;if(!C)return Label(TEXT("世界の読込み中です。"));
    TSharedRef<SVerticalBox> Box=SNew(SVerticalBox);
    Box->AddSlot().AutoHeight().Padding(0,0,0,12)[Label(TEXT("世界の卵・追加要素"),28)];
    Box->AddSlot().AutoHeight().Padding(0,0,0,12)[Label(TEXT("卵から世界を開き、今いる世界へ遊びを追加します。\n実証版：追加できる卓は1台。一人用の世界で利用できます。"),15,Muted)];
    TSharedRef<SVerticalBox> List=SNew(SVerticalBox);
    List->AddSlot().AutoHeight().Padding(0,12,0,8)[Label(TEXT("世界の卵"),22)];
    for(const FString File:C->Files(true))List->AddSlot().AutoHeight().Padding(0,0,0,5)
        [Button(TEXT("開く　")+File,[this,File]{if(Owner.IsValid() && Owner->Concepts)Owner->Concepts->LoadEgg(File);})];
    List->AddSlot().AutoHeight().Padding(0,20,0,8)[Label(TEXT("追加できる遊び"),22)];
    List->AddSlot().AutoHeight().Padding(0,0,0,10)[Label(TEXT("広い床の方を向いて「追加」。前方に卓が現れます。\n卓に近づいて E で遊び、撤去しても対局は残ります。"),15,Muted)];
    for(const FString File:C->Files(false))List->AddSlot().AutoHeight().Padding(0,0,0,5)
        [Button(TEXT("追加　")+File,[this,File]{if(Owner.IsValid() && Owner->Concepts){if(Owner->Concepts->Add(File))Owner->SetMenu(EEWMenu::None);else Owner->RefreshUI();}},!C->Active())];
    if(C->Active())
    {
        List->AddSlot().AutoHeight().Padding(0,10,0,5)[Label(TEXT("設置中：")+C->Package().Title,18)];
        List->AddSlot().AutoHeight().Padding(0,0,0,5)[Button(TEXT("卓で遊ぶ"),[this]{if(Owner.IsValid())Owner->SetMenu(EEWMenu::Chess);},C->Nearby())];
        List->AddSlot().AutoHeight().Padding(0,0,0,5)[Button(TEXT("卓を撤去する（対局は保存）"),[this]{if(Owner.IsValid() && Owner->Concepts){Owner->Concepts->Remove();Owner->RefreshUI();}})];
    }
    List->AddSlot().AutoHeight().Padding(0,18,0,5)[Button(TEXT("設計ファイルのフォルダーを開く"),[this]{if(Owner.IsValid() && Owner->Concepts)FPlatformProcess::ExploreFolder(*Owner->Concepts->Directory());})];
    List->AddSlot().AutoHeight().Padding(0,0,0,5)[Button(TEXT("ファイル一覧を更新"),[this]{if(Owner.IsValid())Owner->RefreshUI();})];
    Box->AddSlot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[List]];
    Box->AddSlot().AutoHeight().Padding(0,12,0,0)[Button(TEXT("戻る"),[this]{if(Owner.IsValid())Owner->SetMenu(EEWMenu::Main);})];
    return Box;
}

TSharedRef<SWidget> SEWOverlay::ChessPanel()
{
    auto* G=Owner.Get();auto* C=G?G->Concepts.Get():nullptr;if(!C || !C->Active())return WorkshopPanel();
    const auto& At=C->Match().At;const auto Legal=At.Legal();TSet<int32> Destinations;
    for(const auto& M:Legal)if(M.From==ChessFrom)Destinations.Add(M.To);
    const TCHAR* Names[]={TEXT(""),TEXT("P"),TEXT("N"),TEXT("B"),TEXT("R"),TEXT("Q"),TEXT("K")};
    TSharedRef<SVerticalBox> Box=SNew(SVerticalBox);
    Box->AddSlot().AutoHeight()[Label(C->Package().Title,27)];
    Box->AddSlot().AutoHeight().Padding(0,6,0,8)[Label(C->MatchStatus(),21,Accent)];
    Box->AddSlot().AutoHeight().Padding(0,0,0,8)[Label(TEXT("白・黒を交互に操作します。駒 → 移動先の順に選択。\nP ポーン / N ナイト / B ビショップ / R ルーク / Q クイーン / K キング"),13,Muted)];
    TSharedRef<SVerticalBox> Content=SNew(SVerticalBox);
    for(int Y=7;Y>=0;--Y)
    {
        TSharedRef<SHorizontalBox> Row=SNew(SHorizontalBox);
        Row->AddSlot().AutoWidth().VAlign(VAlign_Center)[SNew(SBox).WidthOverride(20)[Label(FString::FromInt(Y+1),14)]];
        for(int X=0;X<8;++X)
        {
            const int I=Y*8+X,P=At.Board[I];const FString Text=(P>0?FString(TEXT("白")):P<0?FString(TEXT("黒")):FString(TEXT("　")))+Names[FMath::Abs(P)];
            const bool Selected=ChessFrom==I,Destination=Destinations.Contains(I);
            auto Cell=SNew(SButton).ButtonStyle(&PaperButtonStyle()).ContentPadding(FMargin(2,7))
                .ButtonColorAndOpacity(Selected?FLinearColor(.85,.7,.2):Destination?FLinearColor(.2,.75,.48):(X+Y)%2?FLinearColor(.94,.92,.8):FLinearColor(.50,.64,.58))
                .ToolTipText(FText::FromString(FString::Printf(TEXT("%c%d"),TCHAR('a'+X),Y+1)))
                .OnClicked_Lambda([this,I]
                {
                    if(!Owner.IsValid() || !Owner->Concepts)return FReply::Handled();auto* Runtime=Owner->Concepts.Get();
                    for(const auto& M:Runtime->Match().At.Legal())if(M.From==ChessFrom && M.To==I && (!M.Promotion || M.Promotion==ChessPromotion))
                    {Runtime->Play(UTF8_TO_TCHAR(M.Uci().c_str()));ChessFrom=-1;Owner->RefreshUI();return FReply::Handled();}
                    const int Piece=Runtime->Match().At.Board[I];ChessFrom=Piece*Runtime->Match().At.Side>0 && ChessFrom!=I?I:-1;Owner->RefreshUI();return FReply::Handled();
                })[SNew(STextBlock).Text(FText::FromString(Text)).Font(Font(17)).ColorAndOpacity(Ink).Justification(ETextJustify::Center)];
            Row->AddSlot().FillWidth(1).Padding(1)[Cell];
        }
        Content->AddSlot().AutoHeight()[Row];
    }
    TSharedRef<SHorizontalBox> Coordinates=SNew(SHorizontalBox);
    Coordinates->AddSlot().AutoWidth()[SNew(SBox).WidthOverride(20)];
    for(int X=0;X<8;++X)Coordinates->AddSlot().FillWidth(1)[SNew(STextBlock).Text(FText::FromString(FString::Chr(TCHAR('a'+X)))).Font(Font(13)).ColorAndOpacity(Muted).Justification(ETextJustify::Center)];
    Content->AddSlot().AutoHeight().Padding(0,3,0,10)[Coordinates];
    TSharedRef<SHorizontalBox> Promotion=SNew(SHorizontalBox);
    for(int Kind:{5,4,3,2})Promotion->AddSlot().FillWidth(1).Padding(2)[Button((ChessPromotion==Kind?FString(TEXT("昇格先 ● ")):FString(TEXT("昇格先 ")))+Names[Kind],[this,Kind]{ChessPromotion=Kind;if(Owner.IsValid())Owner->RefreshUI();})];
    Content->AddSlot().AutoHeight()[Promotion];
    Content->AddSlot().AutoHeight().Padding(0,8,0,0)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,5,0)[Button(TEXT("一手戻す"),[this]{if(Owner.IsValid() && Owner->Concepts){Owner->Concepts->Undo();ChessFrom=-1;Owner->RefreshUI();}},!C->Match().Moves.empty())]
        +SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("引き分けを申請"),[this]{if(Owner.IsValid() && Owner->Concepts){Owner->Concepts->ClaimDraw();Owner->RefreshUI();}},C->Match().CanClaimDraw())]];
    Content->AddSlot().AutoHeight().Padding(0,5,0,0)[Button(TEXT("新しい対局"),[this]{if(Owner.IsValid() && Owner->Concepts){Owner->Concepts->NewMatch();ChessFrom=-1;Owner->RefreshUI();}})];
    Box->AddSlot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[Content]];
    Box->AddSlot().AutoHeight().Padding(0,12,0,0)[Button(TEXT("対局を保存して街へ戻る"),[this]{if(Owner.IsValid())Owner->SetMenu(EEWMenu::None);})];
    return Box;
}

TSharedRef<SWidget> SEWOverlay::LiftPanel()
{
    auto* GI=Owner.Get();TSharedRef<SVerticalBox> Box=SNew(SVerticalBox);
    Box->AddSlot().AutoHeight().Padding(0,0,0,12)[Label(TEXT("展望昇降機"),30,Ink)];
    Box->AddSlot().AutoHeight().Padding(0,0,0,12)[Label(TEXT("行き先の階を選んでください。"),16)];
    TSharedRef<SVerticalBox> Stops=SNew(SVerticalBox);
    if(auto* L=GI->ActiveLift.Get())
        for(int32 I=L->Spec.Stops.Num()-1;I>=0;--I)
            Stops->AddSlot().AutoHeight().Padding(0,0,0,5)[Button((I==L->FloorIndex()?TEXT("現在地　"):TEXT(""))+L->Spec.Stops[I].Label,
                [this,I]{if(Owner.IsValid())Owner->SelectLiftFloor(I);},I!=L->FloorIndex())];
    Box->AddSlot().FillHeight(1)[SNew(SScrollBox).ScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll)
        + SScrollBox::Slot()[Stops]];
    Box->AddSlot().AutoHeight().Padding(0,10,0,0)[Button(TEXT("戻る"),[this]{if(Owner.IsValid())Owner->SetMenu(EEWMenu::None);})];
    return Box;
}

TSharedRef<SWidget> SEWOverlay::MainPanel()
{
    auto* GI = Owner.Get();
    TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
    Box->AddSlot().AutoHeight()[Label(TEXT("空の回廊"), 36, Ink)];
    Box->AddSlot().AutoHeight().Padding(0, 6, 0, 18)[Label(TEXT("ENDLESS WORLD"), 14, Accent)];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 22)
        [Label(TEXT("誰もいなくなった街で、水だけが流れ続ける。\n手元の端末に、かつての暮らしを残す旅。"), 17, Muted)];
    if(GI->Terminal && GI->SessionStarted())Box->AddSlot().AutoHeight().Padding(0,0,0,10)[Button(TEXT("Q　記録端末を持ち上げる"),[this]{if(Owner.IsValid() && Owner->Terminal)Owner->Terminal->Open();})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,10)[Button(TEXT("街を探す・街を開く"),[this]{if(Owner.IsValid())Owner->SetMenu(EEWMenu::City);})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,8)[Button(TEXT("世界の卵・追加要素"),[this]{if(Owner.IsValid())Owner->SetMenu(EEWMenu::Workshop);})];
    if (GI->SessionStarted())
        Box->AddSlot().AutoHeight().Padding(0, 0, 0, 8)[Button(TEXT("探索に戻る"), [this] { if (Owner.IsValid()) Owner->SetMenu(EEWMenu::None); })];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
        [Button(GI->CanContinue()?TEXT("続きから"):TEXT("散策を始める"), [this] { if (Owner.IsValid()) Owner->ContinueWorld(); }, GI->CanPlay())];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 18)
        [Button(TEXT("中央広場へ戻る"), [this] { if (Owner.IsValid()) Owner->ReturnToPlaza(); }, GI->CanPlay())];
    Box->AddSlot().AutoHeight().Padding(0,0,0,8)[Button(bShowLegacyWorlds?TEXT("過去の記録・場所コードを閉じる"):TEXT("過去の記録・場所コード"),[this]{bShowLegacyWorlds=!bShowLegacyWorlds;if(Owner.IsValid())Owner->RefreshUI();})];
    if(bShowLegacyWorlds)
    {
    Box->AddSlot().AutoHeight().Padding(0,0,0,8)[Label(TEXT("以前の別世界の保存データも残っています。"),13,Muted)];
    Box->AddSlot().AutoHeight().Padding(0,0,0,8)[Button(TEXT("保存した世界の記録"),[this]{if(Owner.IsValid())Owner->SetMenu(EEWMenu::Worlds);},GI->Store() && GI->Store()->WorldCount()>0)];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 7)[Label(TEXT("場所コードから出発"), 14, Muted)];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
    [ SAssignNew(CodeInput, SEditableTextBox).Style(&PaperInputStyle()).Font(Font(14)).MinDesiredWidth(420).Padding(FMargin(10))
      .HintText(FText::FromString(TEXT("ここにコードを貼り付け")))
      .OnTextCommitted_Lambda([this](const FText& Text, ETextCommit::Type Type)
        { if (Type == ETextCommit::OnEnter && Owner.IsValid()) Owner->StartFromCode(Text.ToString()); }) ];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 14)
        [Button(TEXT("このコードで出発"), [this] { if (Owner.IsValid() && CodeInput) Owner->StartFromCode(CodeInput->GetText().ToString()); }, GI->CanPlay())];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 14)
        [Button(TEXT("コピーしたコードで出発"), [this] { if (Owner.IsValid()) { FString Code; FPlatformApplicationMisc::ClipboardPaste(Code); Owner->StartFromCode(Code); } }, GI->CanPlay())];
    }
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 14)[SNew(SSeparator)];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
        [Button(TEXT("旅の図鑑"), [this] { if (Owner.IsValid()) Owner->SetMenu(EEWMenu::Journal); }, GI->SessionStarted())];
    Box->AddSlot().AutoHeight().Padding(0,0,0,8)
        [Button(TEXT("水辺の魚図鑑・釣りに行く"),[this]{if(Owner.IsValid() && Owner->Fishing)Owner->Fishing->OpenJournal();})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,8)
        [Button(TEXT("写真を撮る　F9"),[this]{if(Owner.IsValid() && Owner->PhotoMode)Owner->PhotoMode->Open();},GI->SessionStarted())];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
        [Button(TEXT("画質と操作"), [this] { if (Owner.IsValid()) Owner->SetMenu(EEWMenu::Settings); })];
    Box->AddSlot().AutoHeight().Padding(0,0,0,8)
        [Button(TEXT("映画館へ行く"),[this]{if(Owner.IsValid())Owner->VisitCinema();})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,8)
        [Button(TEXT("天空シアターへ行く"),[this]{if(Owner.IsValid())Owner->VisitSkyTheatre();})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,8)
        [Button(TEXT("豪華飛行船に乗る　アウレリア空中港"),[this]{if(Owner.IsValid())Owner->VisitSkyport();})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,8)
        [Button(TEXT("上層の客室を訪ねる"),[this]{if(Owner.IsValid())Owner->SetMenu(EEWMenu::SkyResidences);})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,8)
        [Button(TEXT("街の寄り道案内　屋上プール・図書館・美術館"),[this]{if(Owner.IsValid())Owner->SetMenu(EEWMenu::Explore);})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,8)[Button(TEXT("東の外縁へ　白塔の水都"),[this]{if(Owner.IsValid())Owner->VisitOuterWater();})];
    if(!GI->SocialSession || !GI->SocialSession->Active())Box->AddSlot().AutoHeight().Padding(0,0,0,8)
        [Button(TEXT("みんなで映画を見る"),[this]{if(Owner.IsValid())Owner->SetMenu(EEWMenu::Online);})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,8)
        [Button(TEXT("時計広場駅へ　水都・ホテル・シアター・空港"),[this]{if(Owner.IsValid())Owner->VisitSkyrail();})];
    if(GI->SessionStarted() && GI->CinemaScreen && GI->CinemaScreen->ListenerInside())
        Box->AddSlot().AutoHeight().Padding(0,0,0,8)
            [Button(TEXT("映画館の上映を選ぶ"),[this]{if(Owner.IsValid() && Owner->CinemaScreen)Owner->CinemaScreen->OpenControls();})];
    if(GI->SessionStarted() && GI->MediaScreen && GI->MediaScreen->Available())
        Box->AddSlot().AutoHeight().Padding(0,0,0,8)
            [Button(TEXT("広場のモニター"),[this]{if(Owner.IsValid() && Owner->MediaScreen)Owner->MediaScreen->OpenControls();})];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
        [Button(TEXT("終了"), [this] { if (Owner.IsValid()) Owner->Quit(); })];
    Box->AddSlot().FillHeight(1);
    Box->AddSlot().AutoHeight().Padding(0, 10, 0, 0)
        [Label(TEXT("オフラインで生成・保存します。\n世界コードを共有すると、同じ世界を探索できます。"), 12, Muted)];
    Box->AddSlot().AutoHeight().Padding(0, 12, 0, 0)
    [SNew(SProgressBar).FillColorAndOpacity(Accent).Percent_Lambda([this]() -> TOptional<float>
        { return Owner.IsValid() && Owner->Manager ? Owner->Manager->TravelProgress() : 0.f; })
        .Visibility_Lambda([this] { return Owner.IsValid() && Owner->Manager && Owner->Manager->IsTravelling() ?
            EVisibility::Visible : EVisibility::Collapsed; })];
    return SNew(SScrollBox).ScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll) + SScrollBox::Slot()[Box];
}

TSharedRef<SWidget> SEWOverlay::ExplorePanel()
{
    auto Box=SNew(SVerticalBox);
    Box->AddSlot().AutoHeight()[Label(TEXT("街の寄り道案内"),30)];
    Box->AddSlot().AutoHeight().Padding(0,8,0,18)[Label(TEXT("静かな屋上の水辺と、6つの寄り道。\n昇降機の行き先にも施設名を表示しています。\n館内の案内台で E を押すと、旅の図鑑に残せます。"),16)];
    Box->AddSlot().AutoHeight().Padding(0,12,0,8)[Label(TEXT("屋上の水辺"),26)];
    Box->AddSlot().AutoHeight().Padding(0,0,0,10)[Label(TEXT("北・北東・西の屋上庭園に、3つのプール。昇降機でも上がれます。\n石段で水中へ降り、同じ階段から屋上へ戻れます。"),16)];
    if(Owner->WaterView)for(const auto& P:Owner->WaterView->Pools())
        Box->AddSlot().AutoHeight().Padding(0,0,0,10)[Button(P.Name+TEXT("へ"),[this,I=P.Index]{if(Owner.IsValid() && Owner->WaterView)Owner->WaterView->VisitPool(I);},!Owner->SocialSession || !Owner->SocialSession->Active())];
    const auto R=EW::GenerateChunk(EW::WorldDescriptor::ReferenceWorld(),{0,0});
    for(int32 I=0;I<6;++I)
    {
        const auto* Room=EWExplorationPlan::Find(R,I);const bool Seen=Room && Owner->IsRecorded(EWExplorationPlan::Bookmark(R,*Room));
        Box->AddSlot().AutoHeight()[Label((Seen?TEXT("発見済み　"):TEXT("未発見　"))+EWExplorationPlan::Name(I),22,Accent)];
        Box->AddSlot().AutoHeight().Padding(0,5,0,5)[Label(EWExplorationPlan::Description(I),15)];
        Box->AddSlot().AutoHeight().Padding(0,0,0,6)[Label(EWExplorationPlan::Directions(I),14,Muted)];
        Box->AddSlot().AutoHeight().Padding(0,0,0,18)[Button(TEXT("入口から散策する"),[this,I]{if(Owner.IsValid())Owner->VisitPublicPlace(I);},!Owner->SocialSession || !Owner->SocialSession->Active())];
    }
    Box->AddSlot().AutoHeight()[Button(TEXT("街へ戻る"),[this]{if(Owner.IsValid())Owner->SetMenu(EEWMenu::None);})];
    return SNew(SScrollBox).ScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll)+SScrollBox::Slot()[Box];
}
TSharedRef<SWidget> SEWOverlay::SkyResidencesPanel()
{
    auto Box=SNew(SVerticalBox);
    Box->AddSlot().AutoHeight().Padding(0,0,0,12)[Label(TEXT("雲上ホテル　8つの客室"),28)];
    Box->AddSlot().AutoHeight().Padding(0,0,0,14)[Label(TEXT("時計広場駅の昇降機で『上層線』へ → 雲上ホテル駅で降車。\nホーム右の昇降機で客室階へ。空中回廊の北館に101–104号室、南館に105–108号室があります。"),16)];
    Box->AddSlot().AutoHeight().Padding(0,0,0,18)[Button(TEXT("電車で向かう　時計広場駅へ"),[this]{if(Owner.IsValid())Owner->VisitSkyrail();})];
    for(int32 I=0;I<8;++I)
    {
        Box->AddSlot().AutoHeight().Padding(0,0,0,5)[Button(FString::Printf(TEXT("%d　%s　入口へ"),101+I,*EWHotelPlan::Name(I)),[this,I]{if(Owner.IsValid())Owner->VisitSkyResidence(I);})];
        Box->AddSlot().AutoHeight().Padding(4,0,0,18)[Label(EWHotelPlan::Description(I),15)];
    }
    Box->AddSlot().AutoHeight()[Button(TEXT("戻る"),[this]{if(Owner.IsValid())Owner->SetMenu(EEWMenu::Main);})];
    return SNew(SScrollBox).ScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll)+SScrollBox::Slot()[Box];
}
TSharedRef<SWidget> SEWOverlay::DiscoveryRow(const EW::PlaceBookmark& P)
{
    return SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
        .BorderBackgroundColor(FLinearColor(.84, .9, .85, .75)).Padding(FMargin(16, 12))
        [SNew(SVerticalBox)
         + SVerticalBox::Slot().AutoHeight()
           [Label((P.bFavourite ? TEXT("★ ") : TEXT("")) + P.Name, 20, Ink)]
         + SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 6)[Label(EW::PlaceDescription(P.Kind), 13, Muted)]
         + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 8)[Label(TEXT("場所 ") + P.Coord.Text(), 11, Muted)]
         + SVerticalBox::Slot().AutoHeight()
           [SNew(SHorizontalBox)
            + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
              [Button(P.bFavourite ? TEXT("お気に入り解除") : TEXT("お気に入り"), [this, P] { if (Owner.IsValid()) Owner->Favourite(P); })]
            + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
              [Button(TEXT("再訪する"), [this, P] { if (Owner.IsValid()) Owner->Visit(P); }, Owner.IsValid() && Owner->CanPlay())]
            + SHorizontalBox::Slot().AutoWidth()
              [Button(TEXT("場所コードをコピー"), [this, P] { if (Owner.IsValid()) Owner->CopyPlace(P); })] ] ];
}

TSharedRef<SWidget> SEWOverlay::JournalPanel()
{
    auto* GI = Owner.Get();
    TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
    const int32 Count = GI->RecordCount(), Page = GI->JournalPageIndex;
    Box->AddSlot().AutoHeight()[Label(TEXT("旅の図鑑"), 30, Ink)];
    Box->AddSlot().AutoHeight().Padding(0, 8, 0, 18)
        [Label(FString::Printf(TEXT("この世界で見つけた場所  %d / 10,000"), Count), 14, Muted)];
    TSharedRef<SScrollBox> Scroll = SNew(SScrollBox).ScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll);
    const auto Records = GI->JournalPage(Page);
    if (Records.IsEmpty()) Scroll->AddSlot()[Label(TEXT("まだ記録がありません。\n道の先にある案内標へ近づき、") + GI->InputHint(TEXT("E"), TEXT("X"), TEXT("□")) + TEXT("で発見を記録してください。"), 17, Muted)];
    for (const auto& P : Records) Scroll->AddSlot().Padding(0, 0, 0, 12)[DiscoveryRow(P)];
    Box->AddSlot().FillHeight(1)[Scroll];
    Box->AddSlot().AutoHeight().Padding(0, 16, 0, 12)
    [SNew(SHorizontalBox)
     + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
       [Button(TEXT("前のページ"), [this] { if (Owner.IsValid()) { --Owner->JournalPageIndex; Owner->RefreshUI(); } }, Page > 0)]
     + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
       [Button(TEXT("次のページ"), [this] { if (Owner.IsValid()) { ++Owner->JournalPageIndex; Owner->RefreshUI(); } }, (Page + 1) * 12 < Count)]
     + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
       [Label(FString::Printf(TEXT("%d / %d"), Page + 1, FMath::Max(1, (Count + 11) / 12)), 14, Muted)]];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
    [SNew(SHorizontalBox)
     + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
       [Button(TEXT("世界コードをコピー"), [this] { if (Owner.IsValid()) Owner->CopyWorld(); })]
     + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 8, 0)
       [Button(TEXT("記録を書き出す"), [this] { if (Owner.IsValid()) Owner->ExportRecords(); })]
     + SHorizontalBox::Slot().AutoWidth()
       [Button(TEXT("保存フォルダー"), [this] { if (Owner.IsValid()) Owner->OpenSaveDirectory(); })]];
    Box->AddSlot().AutoHeight()[Button(TEXT("探索へ戻る"), [this] { if (Owner.IsValid()) Owner->SetMenu(EEWMenu::None); })];
    return Box;
}

TSharedRef<SWidget> SEWOverlay::SettingsPanel()
{
    auto* GI = Owner.Get();
    TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
    Box->AddSlot().AutoHeight()[Label(TEXT("画質と操作"), 30, Ink)];
    Box->AddSlot().AutoHeight().Padding(0, 12, 0, 8)
        [SNew(STextBlock).Font(Font(14)).ColorAndOpacity(Muted)
          .Text_Lambda([this] { return FText::FromString(Owner.IsValid() ? Owner->ResolutionText() : FString()); })];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
        [Button(TEXT("モニターの解像度に合わせる ／ 100%"), [this] { if (Owner.IsValid()) Owner->SetNativeResolution(); })];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
        [Button(TEXT("メインディスプレイへ戻す"), [this] { if (Owner.IsValid()) Owner->MoveToPrimaryDisplay(); })];
    if(GI->Music)
    {
        Box->AddSlot().AutoHeight().Padding(0,16,0,8)[Label(FString::Printf(TEXT("音楽：%d %%"),FMath::RoundToInt(GI->Music->Volume()*100)),18,Accent)];
        Box->AddSlot().AutoHeight().Padding(0,0,0,8)[SNew(SHorizontalBox)
            + SHorizontalBox::Slot().FillWidth(1).Padding(0,0,5,0)[Button(TEXT("小さく"),[this]{if(Owner.IsValid() && Owner->Music){Owner->Music->SetVolume(Owner->Music->Volume()-.1f);Owner->RefreshUI();}})]
            + SHorizontalBox::Slot().FillWidth(1).Padding(0,0,5,0)[Button(TEXT("大きく"),[this]{if(Owner.IsValid() && Owner->Music){Owner->Music->SetVolume(Owner->Music->Volume()+.1f);Owner->RefreshUI();}})]
            + SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("切"),[this]{if(Owner.IsValid() && Owner->Music){Owner->Music->SetVolume(0);Owner->RefreshUI();}})]];
        Box->AddSlot().AutoHeight().Padding(0,0,0,8)[Label(TEXT("曲の間に静けさを残し、会話・上映中は音楽が遠のきます。"),13,Muted)];
    }
    Box->AddSlot().AutoHeight().Padding(0, 16, 0, 8)[Label(TEXT("表示のなめらかさ"), 18, Accent)];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
        [Button(GI->Graphics.VSync ? TEXT("● 垂直同期：入") : TEXT("垂直同期：切"),
            [this] { if (Owner.IsValid()) { Owner->Graphics.SetVSync(!Owner->Graphics.VSync); Owner->RefreshUI(); } },
            GI->Graphics.VSync || GI->Graphics.CanUseVSync())];
    Box->AddSlot().AutoHeight().Padding(0, 4, 0, 6)
        [Label(GI->Graphics.MaxDisplayFPS ? FString::Printf(TEXT("最大フレームレート：%d FPS（生成込み）"), GI->Graphics.MaxDisplayFPS) :
            TEXT("最大フレームレート：上限なし"), 16, Ink)];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 8)[SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 6, 0)
            [Button(TEXT("下げる"), [this] { if (Owner.IsValid()) { Owner->Graphics.StepMaxDisplayFPS(-1); Owner->RefreshUI(); } }, GI->Graphics.MaxDisplayFPS != 30)]
        + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 6, 0)
            [Button(TEXT("上げる"), [this] { if (Owner.IsValid()) { Owner->Graphics.StepMaxDisplayFPS(1); Owner->RefreshUI(); } }, GI->Graphics.MaxDisplayFPS < 360)]
        + SHorizontalBox::Slot().FillWidth(1)
            [Button(TEXT("上限なし"), [this] { if (Owner.IsValid()) { Owner->Graphics.SetMaxDisplayFPS(0); Owner->RefreshUI(); } }, GI->Graphics.MaxDisplayFPS != 0)]];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 10)[Label(GI->Graphics.PresentationStatus(), 13, Muted)];
    Box->AddSlot().AutoHeight().Padding(0, 16, 0, 12)[Label(TEXT("画質"), 18, Accent)];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
        [Button(GI->IsHighQuality() ? TEXT("● 品質優先") : TEXT("品質優先"),
            [this] { if (Owner.IsValid()) Owner->SetQuality(true); })];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 18)
        [Button(GI->IsHighQuality() ? TEXT("軽量設定") : TEXT("● 軽量設定"),
            [this] { if (Owner.IsValid()) Owner->SetQuality(false); })];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 18)
        [Button(GI->IsSoftStyle() ? TEXT("● 色合いをやわらかく") : TEXT("色合いをやわらかく"),
            [this] { if (Owner.IsValid()) Owner->SetSoftStyle(!Owner->IsSoftStyle()); })];
    Box->AddSlot().AutoHeight().Padding(0, 4, 0, 8)[Label(TEXT("DLSS 4.5"), 20, Accent)];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 12)[Label(GI->Graphics.Status(), 13, Muted)];
    for (const auto& Option : GI->Graphics.SuperResolutionOptions())
        Box->AddSlot().AutoHeight().Padding(0, 0, 0, 6)
            [Button((Option.Value == GI->Graphics.SuperResolution ? TEXT("● ") : TEXT("")) + Option.Label,
                [this, Value = Option.Value] { if (Owner.IsValid()) { Owner->Graphics.SetSuperResolution(Value); Owner->RefreshUI(); } }, Option.Supported)];
    Box->AddSlot().AutoHeight().Padding(0, 12, 0, 8)[Label(TEXT("フレーム生成"), 18, Accent)];
    for (const auto& Option : GI->Graphics.FrameGenerationOptions())
        Box->AddSlot().AutoHeight().Padding(0, 0, 0, 6)
            [Button((Option.Value == GI->Graphics.FrameGeneration ? TEXT("● ") : TEXT("")) + Option.Label,
                [this, Value = Option.Value] { if (Owner.IsValid()) { Owner->Graphics.SetFrameGeneration(Value); Owner->RefreshUI(); } }, Option.Supported)];
    Box->AddSlot().AutoHeight().Padding(0, 8, 0, 6)
        [Button(GI->Graphics.RayReconstruction ? TEXT("● レイ再構成：入") : TEXT("レイ再構成：切"),
            [this] { if (Owner.IsValid()) { Owner->Graphics.SetRayReconstruction(!Owner->Graphics.RayReconstruction); Owner->RefreshUI(); } },
            GI->Graphics.RayReconstruction || GI->Graphics.CanReconstructRays())];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 12)
        [SNew(STextBlock).Font(Font(13)).ColorAndOpacity(Muted).AutoWrapText(true)
            .Text_Lambda([this] { return FText::FromString(Owner.IsValid() ? Owner->Graphics.RayReconstructionStatus() : FString()); })];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 18)
        [Button(GI->Graphics.Reflex ? TEXT("● NVIDIA Reflex 低遅延") : TEXT("NVIDIA Reflex 低遅延"),
            [this] { if (Owner.IsValid()) { Owner->Graphics.SetReflex(!Owner->Graphics.Reflex); Owner->RefreshUI(); } }, GI->Graphics.CanReflex())];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 12)
        [Label(TEXT("フレーム生成はReflexと併用します。対応していない機能は選択できません。"), 13, Muted)];
    Box->AddSlot().AutoHeight().Padding(0, 12, 0, 8)[Label(TEXT("HDRの表示"), 20, Accent)];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
        [Button(GI->Graphics.HDR ? TEXT("● HDR：入") : TEXT("HDR：切"),
            [this] { if (Owner.IsValid()) { Owner->Graphics.SetHDR(!Owner->Graphics.HDR); Owner->RefreshUI(); } },
            GI->Graphics.HDR || GI->Graphics.CanHDR())];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 12)
        [SNew(STextBlock).Font(Font(13)).ColorAndOpacity(Muted).AutoWrapText(true)
            .Text_Lambda([this] { return FText::FromString(Owner.IsValid() ? Owner->Graphics.HDRStatus() : FString()); })];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 6)
        [Label(FString::Printf(TEXT("画面の最大輝度：%d nit"), GI->Graphics.HDRPeakNits), 16, Ink)];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 10)[SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 6, 0)
            [Button(TEXT("100 nit 下げる"), [this] { if (Owner.IsValid()) { Owner->Graphics.SetHDRPeakNits(Owner->Graphics.HDRPeakNits - 100); Owner->RefreshUI(); } }, GI->Graphics.HDR && GI->Graphics.HDRPeakNits > 400)]
        + SHorizontalBox::Slot().FillWidth(1)
            [Button(TEXT("100 nit 上げる"), [this] { if (Owner.IsValid()) { Owner->Graphics.SetHDRPeakNits(Owner->Graphics.HDRPeakNits + 100); Owner->RefreshUI(); } }, GI->Graphics.HDR && GI->Graphics.HDRPeakNits < 2000)]];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 6)
        [Label(FString::Printf(TEXT("HDRの明るさ：%d%%"), GI->Graphics.HDRBrightness), 16, Ink)];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 10)[SNew(SHorizontalBox)
        + SHorizontalBox::Slot().FillWidth(1).Padding(0, 0, 6, 0)
            [Button(TEXT("暗くする"), [this] { if (Owner.IsValid()) { Owner->Graphics.SetHDRBrightness(Owner->Graphics.HDRBrightness - 10); Owner->RefreshUI(); } }, GI->Graphics.HDR && GI->Graphics.HDRBrightness > 50)]
        + SHorizontalBox::Slot().FillWidth(1)
            [Button(TEXT("明るくする"), [this] { if (Owner.IsValid()) { Owner->Graphics.SetHDRBrightness(Owner->Graphics.HDRBrightness + 10); Owner->RefreshUI(); } }, GI->Graphics.HDR && GI->Graphics.HDRBrightness < 200)]];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 14)
        [Label(TEXT("最大輝度はお使いの画面に合わせて調整してください。明るさは100%が標準です。"), 13, Muted)];
    Box->AddSlot().AutoHeight().Padding(0, 12, 0, 8)[Label(TEXT("コントローラー"), 20, Accent)];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 12)
        [Label(TEXT("左スティック：移動　右スティック：見回す\nA / ×：ジャンプ　X / □：調べる・昇降機\nY / △・タッチパッド：図鑑\nLB / L1・左スティック押込：走る\nMenu / Options：メニュー\nView / Create：写真を保存\nメニュー内：十字キー・左スティックで選択\nA / ×：決定　B / ○：戻る"), 15, Ink)];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 6)
        [Button(FString::Printf(TEXT("視点の速さ　%.0f°/秒"), GI->PadLookSpeed),
            [this] { if (Owner.IsValid()) Owner->SetPadLookSpeed(Owner->PadLookSpeed >= 180 ? 60 : Owner->PadLookSpeed + 30); })];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 18)
        [Button(GI->bInvertPadY ? TEXT("● 右スティックの上下を反転") : TEXT("右スティックの上下を反転"),
            [this] { if (Owner.IsValid()) Owner->TogglePadInvertY(); })];
    Box->AddSlot().AutoHeight()[Label(TEXT("基本操作"), 18, Accent)];
    Box->AddSlot().AutoHeight().Padding(0, 12, 0, 20)
        [Label(TEXT("WASD     歩く\nマウス    見回す\nShift      走る\nSpace    ジャンプ\nE            調べる・座る・発見を記録する\nTab        図鑑を開く・閉じる\nEsc        メニュー\nF8          写真を保存する\nF11        全画面を切り替える\nF2          動作情報を表示する"), 17, Ink)];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 18)
        [Label(TEXT("現在地は定期的に保存されます。\n終了するときはメニューの「終了」を使ってください。"), 14, Muted)];
    Box->AddSlot().AutoHeight().Padding(0, 0, 0, 8)
        [Button(TEXT("保存フォルダーを開く"), [this] { if (Owner.IsValid()) Owner->OpenSaveDirectory(); })];
    Box->AddSlot().FillHeight(1);
    Box->AddSlot().AutoHeight()[Button(TEXT("戻る"), [this] { if (Owner.IsValid()) Owner->SetMenu(EEWMenu::Main); })];
    return SAssignNew(SettingsScroll, SScrollBox)
        .ScrollWhenFocusChanges(bRestoreSettingsScroll ? EScrollWhenFocusChanges::NoScroll : EScrollWhenFocusChanges::AnimatedScroll)
        + SScrollBox::Slot()[Box];
}

TSharedRef<SWidget> SEWOverlay::WorldsPanel()
{
    auto* GI = Owner.Get(); auto* Store = GI->Store();
    TSharedRef<SVerticalBox> Box = SNew(SVerticalBox);
    Box->AddSlot().AutoHeight()[Label(TEXT("これまでの世界"), 30, Ink)];
    Box->AddSlot().AutoHeight().Padding(0, 10, 0, 18)[Label(TEXT("世界ごとに、現在地と旅の図鑑を保存しています。"), 15, Muted)];
    TSharedRef<SScrollBox> Scroll = SNew(SScrollBox).ScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll);
    const int32 Page = GI->WorldPageIndex, Count = Store ? Store->WorldCount() : 0;
    if (Store) for (const auto& P : Store->Worlds(Page * 12, 12))
    {
        Scroll->AddSlot().Padding(0, 0, 0, 14)
        [SNew(SVerticalBox)
         + SVerticalBox::Slot().AutoHeight()[Label(P.WorldCode, 14, Ink)]
         + SVerticalBox::Slot().AutoHeight().Padding(0, 4, 0, 6)
           [Label(FString::Printf(TEXT("%s　場所 %s　発見 %d 件"), *EW::RegionName(EW::RegionKind(P.Kind / 3)), *P.Coord.Text(), Store->Count(P.WorldCode)), 13, Muted)]
         + SVerticalBox::Slot().AutoHeight()[Button(TEXT("この世界の続きから"), [this, P] { if (Owner.IsValid()) Owner->Visit(P); }, GI->CanPlay())]];
    }
    Box->AddSlot().FillHeight(1)[Scroll];
    Box->AddSlot().AutoHeight().Padding(0, 12, 0, 12)
    [SNew(SHorizontalBox)
     + SHorizontalBox::Slot().AutoWidth().Padding(0, 0, 10, 0)
       [Button(TEXT("前へ"), [this] { if (Owner.IsValid()) { --Owner->WorldPageIndex; Owner->RefreshUI(); } }, Page > 0)]
     + SHorizontalBox::Slot().AutoWidth()
       [Button(TEXT("次へ"), [this] { if (Owner.IsValid()) { ++Owner->WorldPageIndex; Owner->RefreshUI(); } }, (Page+1)*12 < Count)]];
    Box->AddSlot().AutoHeight()[Button(TEXT("戻る"), [this] { if (Owner.IsValid()) Owner->SetMenu(EEWMenu::Main); })];
    return Box;
}

TSharedRef<SWidget> SEWOverlay::SaveFailurePanel()
{
    return SNew(SVerticalBox)
     + SVerticalBox::Slot().AutoHeight()[Label(TEXT("現在地を保存できません"), 26, Ink)]
     + SVerticalBox::Slot().AutoHeight().Padding(0, 18, 0, 24)
       [Label(TEXT("以前に保存した記録は残っています。\n保存せず終了すると、最後の保存以降の移動は残りません。\n空き容量や保存フォルダーの権限を確認して再試行できます。"), 17, Muted)]
     + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
       [Button(TEXT("保存して終了を再試行"), [this] { if (Owner.IsValid()) Owner->Quit(); })]
     + SVerticalBox::Slot().AutoHeight().Padding(0, 0, 0, 12)
       [Button(TEXT("保存せず終了する"), [this] { if (Owner.IsValid()) Owner->QuitWithoutSaving(); })]
     + SVerticalBox::Slot().AutoHeight()
       [Button(TEXT("戻る"), [this] { if (Owner.IsValid()) Owner->SetMenu(EEWMenu::Main); })];
}

FReply SEWOverlay::OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (Owner.IsValid())
    {
        if (Event.GetKey() == EKeys::Escape) { Owner->ToggleMenu(); return FReply::Handled(); }
        if (Event.GetKey() == EKeys::Tab && Owner->Menu() == EEWMenu::Journal)
        { Owner->ToggleJournal(); return FReply::Handled(); }
    }
    return SCompoundWidget::OnKeyDown(Geometry, Event);
}

int32 SEWOverlay::CurrentFocus() const
{
    for (int32 I = 0; I < FocusControls.Num(); ++I)
        if (auto Control = FocusControls[I].Pin())
            if (Control->HasKeyboardFocus() || Control->HasFocusedDescendants()) return I;
    return FocusIndex;
}
FString SEWOverlay::FocusedLabel() const
{
    const int32 I=CurrentFocus();return FocusLabels.IsValidIndex(I)?FocusLabels[I]:FString();
}
void SEWOverlay::FocusInitialControl()
{
    if (!Owner.IsValid() || Owner->Menu() == EEWMenu::None || FocusControls.IsEmpty()) return;
    FocusIndex = FMath::Clamp(FocusIndex, 0, FocusControls.Num() - 1);
    for (int32 Offset = 0; Offset < FocusControls.Num(); ++Offset)
    {
        const int32 I = (FocusIndex + Offset) % FocusControls.Num();
        if (auto Control = FocusControls[I].Pin()) if (Control->IsEnabled())
        {
            FocusIndex = I;
            FSlateApplication::Get().SetKeyboardFocus(Control, EFocusCause::Navigation);
            if (bRestoreSettingsScroll && SettingsScroll.IsValid())
            {
                // Recreating the settings panel must not scroll a clicked control
                // using the new widget's not-yet-arranged geometry.
                SettingsScroll->SetScrollOffset(SettingsScrollOffset);
                SettingsScroll->SetScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll);
                bRestoreSettingsScroll = false;
            }
            return;
        }
    }
}
void SEWOverlay::MoveFocus(int32 Step)
{
    if (FocusControls.IsEmpty()) return;
    const int32 Current = CurrentFocus();
    for (int32 Offset = 1; Offset <= FocusControls.Num(); ++Offset)
    {
        const int32 I = (Current + Step * Offset + FocusControls.Num() * 2) % FocusControls.Num();
        if (auto Control = FocusControls[I].Pin()) if (Control->IsEnabled())
        { FocusIndex = I; FSlateApplication::Get().SetKeyboardFocus(Control, EFocusCause::Navigation); return; }
    }
}
FReply SEWOverlay::OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
    if (!Owner.IsValid() || Owner->Menu() == EEWMenu::None) return FReply::Unhandled();
    if (Owner->bScriptedWorldAudit) return FReply::Handled();
    const FKey Key = Event.GetKey();
    if (Key == EKeys::Escape)
    {
        Owner->NoteInput(false);
        if (!Event.IsRepeat()) Owner->BackFromMenu();
        return FReply::Handled();
    }
    if (Key.IsGamepadKey())
    {
        const auto* Pad = FEWGamepadModule::GetIfLoaded(); Owner->NoteInput(true, Pad && Pad->RecentSonyInput());
        if (Key == EKeys::Gamepad_DPad_Down || Key == EKeys::Gamepad_DPad_Right) { MoveFocus(1); return FReply::Handled(); }
        if (Key == EKeys::Gamepad_DPad_Up || Key == EKeys::Gamepad_DPad_Left) { MoveFocus(-1); return FReply::Handled(); }
        if (Key == EKeys::Gamepad_FaceButton_Right || Key == EKeys::Gamepad_Special_Right)
        { if (!Event.IsRepeat()) Owner->BackFromMenu(); return FReply::Handled(); }
        if (Key == EKeys::Gamepad_FaceButton_Bottom)
        {
            const int32 I = CurrentFocus();
            if (!Event.IsRepeat() && FocusActions.IsValidIndex(I))
                if (auto Control = FocusControls[I].Pin()) if (Control->IsEnabled())
                { auto Action = FocusActions[I]; Action(); }
            return FReply::Handled();
        }
    }
    return FReply::Unhandled();
}
FReply SEWOverlay::OnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event)
{
    if (!Owner.IsValid() || Owner->Menu() == EEWMenu::None || Event.GetKey() != EKeys::Gamepad_LeftY) return FReply::Unhandled();
    if (Owner->bScriptedWorldAudit) return FReply::Handled();
    const float Value = Event.GetAnalogValue();
    if (FMath::Abs(Value) < .35f) { NextStickNavigation = 0; return FReply::Handled(); }
    const double Now = FPlatformTime::Seconds();
    if (FMath::Abs(Value) > .55f && Now >= NextStickNavigation)
    {
        const auto* Pad = FEWGamepadModule::GetIfLoaded(); Owner->NoteInput(true, Pad && Pad->RecentSonyInput());
        MoveFocus(Value > 0 ? -1 : 1); NextStickNavigation = Now + .22;
    }
    return FReply::Handled();
}

TSharedRef<SWidget> SEWOverlay::OnlinePanel()
{
    auto* G=Owner.Get();if(!G || !G->CinemaSession)return Label(TEXT("映画館を準備しています。"));
    const TWeakObjectPtr<AEWCinemaSession> Session=G->CinemaSession;
    auto Box=SNew(SVerticalBox);
    Box->AddSlot().AutoHeight().Padding(0,0,0,18)[Label(TEXT("みんなで映画を見る"),26)];
    Box->AddSlot().AutoHeight().Padding(0,0,0,12)[Label(TEXT("最大32人。ホストのPCで部屋を開き、招待コードで集まれます。ホストが終了すると部屋も閉じます。"),16)];
    Box->AddSlot().AutoHeight().Padding(0,0,0,8)[Label(TEXT("表示名"),14)];
    Box->AddSlot().AutoHeight().Padding(0,0,0,14)
        [SAssignNew(OnlineNameInput,SEditableTextBox).Style(&PaperInputStyle()).Font(Font(16)).Text(FText::FromString(OnlineName))
            .OnTextChanged_Lambda([this](const FText& V){OnlineName=V.ToString().Left(20);})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,12)[Button(TEXT("部屋を作る"),[this,Session]{if(Session.IsValid())Session->Host(OnlineName);})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,8)[Label(TEXT("招待コードで参加"),14)];
    Box->AddSlot().AutoHeight().Padding(0,0,0,10)
        [SAssignNew(OnlineCodeInput,SEditableTextBox).Style(&PaperInputStyle()).Font(Font(14)).Text(FText::FromString(OnlineCode))
            .HintText(FText::FromString(TEXT("ewcinema1|… で始まる招待コードを貼り付け")))
            .OnTextChanged_Lambda([this](const FText& V){OnlineCode=V.ToString();})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,18)[Button(TEXT("参加する"),[this,Session]{if(Session.IsValid())Session->Join(OnlineCode,OnlineName);})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,12)
        [SNew(STextBlock).Font(Font(16)).ColorAndOpacity(Accent).AutoWrapText(true).Text_Lambda([Session]{return FText::FromString(Session.IsValid()?Session->Status():FString());})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,12)
        [SNew(STextBlock).Font(Font(14)).ColorAndOpacity(Muted).Text_Lambda([Session]{return FText::FromString(Session.IsValid() && Session->Connected()?FString::Printf(TEXT("参加者 %d / 32人"),Session->Members()):TEXT(""));})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,10)[Button(TEXT("招待コードをコピー"),[this,Session]{if(Session.IsValid() && !Session->Invite().IsEmpty()){FPlatformApplicationMisc::ClipboardCopy(*Session->Invite());if(Owner.IsValid())Owner->Notify(TEXT("招待コードをコピーしました。友達に共有してください。"));}})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,10)[Button(TEXT("映画館へ戻る"),[this]{if(Owner.IsValid()){Owner->VisitCinema();}})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,16)[Button(TEXT("部屋から退出"),[Session]{if(Session.IsValid())Session->Leave();})];
    Box->AddSlot().AutoHeight()[Label(TEXT("招待コードは一緒に見る人に共有してください。動画は各自の回線で再生し、ホストに合わせて同期します。広告や視聴制限のある動画では同期が遅れる場合があります。"),12,Muted)];
    return SNew(SScrollBox)+SScrollBox::Slot()[Box];
}

TSharedRef<SWidget> SEWOverlay::CityPanel()
{
    auto* G=Owner.Get();if(!G || !G->SocialSession)return Label(TEXT("街を準備しています。"));
    const TWeakObjectPtr<AEWSocialSession> Session=G->SocialSession;auto& Net=Session->Network();
    if(CityNickname.IsEmpty())CityNickname=Session->Nickname();
    auto Box=SNew(SVerticalBox);
    Box->AddSlot().AutoHeight().Padding(0,0,0,6)[Label(TEXT("空の回廊で、また会おう"),28)];
    Box->AddSlot().AutoHeight().Padding(0,0,0,14)[Label(TEXT("登録不要・最大8人。街はホストのPCが開いている間だけ存在します。"),14,Muted)];
    Box->AddSlot().AutoHeight().Padding(0,0,0,14)[SNew(STextBlock).Font(Font(16)).ColorAndOpacity(Accent).AutoWrapText(true)
        .Text_Lambda([Session]{return FText::FromString(Session.IsValid()?Session->Status():FString());})];
    auto Input=[this](FString& Target,int32 Limit,const FString& Hint)
    {
        return SNew(SEditableTextBox).Style(&PaperInputStyle()).Font(Font(15)).Padding(9).Text(FText::FromString(Target))
            .HintText(FText::FromString(Hint)).OnTextChanged_Lambda([&Target,Limit](const FText& T){Target=T.ToString().Left(Limit);});
    };
    if(!Net.InLobby())
    {
        Box->AddSlot().AutoHeight().Padding(0,0,0,6)[Label(TEXT("表示名"),14,Muted)];
        Box->AddSlot().AutoHeight().Padding(0,0,0,10)[Input(CityNickname,20,TEXT("街で呼ばれたい名前"))];
        if(!Net.LoggedIn())
            Box->AddSlot().AutoHeight().Padding(0,0,0,14)[Button(TEXT("登録せずに接続する"),[Session]{if(Session.IsValid())Session->Authenticate();},Net.Configured() && !Net.Busy())];
        else
        {
            Box->AddSlot().AutoHeight().Padding(0,0,0,12)[Button(TEXT("街を探す"),[Session]{if(Session.IsValid())Session->FindCities();},!Net.Busy())];
            for(const auto& City:Net.Listings())
            {
                const FString Text=FString::Printf(TEXT("%s　%d / %d人　%s・%s"),*City.Name,City.Members,City.Capacity,*City.Language,*City.Activity);
                Box->AddSlot().AutoHeight().Padding(0,0,0,6)[Button(Text,[this,Session,Id=City.Id]{if(Session.IsValid())Session->JoinCity(Id,CityNickname);},!Net.Busy())];
            }
            Box->AddSlot().AutoHeight().Padding(0,14,0,6)[Label(TEXT("招待コードで参加"),18)];
            Box->AddSlot().AutoHeight().Padding(0,0,0,8)[Input(CityCode,80,TEXT("ewcity1|…"))];
            Box->AddSlot().AutoHeight().Padding(0,0,0,18)[Button(TEXT("この街に参加"),[this,Session]{if(Session.IsValid())Session->JoinCity(CityCode,CityNickname);},!Net.Busy())];
            Box->AddSlot().AutoHeight().Padding(0,0,0,6)[Label(TEXT("街を開く"),18)];
            Box->AddSlot().AutoHeight().Padding(0,0,0,6)[Input(CityName,30,TEXT("街の名前"))];
            auto Row=SNew(SHorizontalBox);
            Row->AddSlot().FillWidth(1).Padding(0,0,5,0)[Input(CityLanguage,8,TEXT("会話する言語"))];
            Row->AddSlot().FillWidth(1)[Input(CityActivity,12,TEXT("散策・会話など"))];
            Box->AddSlot().AutoHeight().Padding(0,0,0,8)[Row];
            Box->AddSlot().AutoHeight().Padding(0,0,0,12)[Button(TEXT("時計広場で街を開く"),[this,Session]{if(Session.IsValid())Session->HostCity(CityName,CityLanguage,CityActivity,CityNickname);},!Net.Busy())];
        }
    }
    else
    {
        Box->AddSlot().AutoHeight().Padding(0,0,0,10)[Label(FString::Printf(TEXT("住人　%d / 8人"),Session->Players().Num()),20)];
        auto Actions=SNew(SHorizontalBox);
        Actions->AddSlot().FillWidth(1).Padding(0,0,6,0)[Button(TEXT("街に戻る"),[this]{if(Owner.IsValid())Owner->SetMenu(EEWMenu::None);})];
        Actions->AddSlot().FillWidth(1)[Button(TEXT("招待コードをコピー"),[this,Session]{if(Session.IsValid()){FPlatformApplicationMisc::ClipboardCopy(*Session->Invite());if(Owner.IsValid())Owner->Notify(TEXT("招待コードをコピーしました。"));}})];
        Box->AddSlot().AutoHeight().Padding(0,0,0,12)[Actions];
        Box->AddSlot().AutoHeight().Padding(0,0,0,8)[SNew(STextBlock).Font(Font(14)).ColorAndOpacity(Muted).AutoWrapText(true)
            .Text_Lambda([Session]{return FText::FromString(Session.IsValid()?Session->Network().VoiceStatus():FString());})];
        Box->AddSlot().AutoHeight().Padding(0,0,0,8)[Button(Session->IsQuiet()?TEXT("会話の音を戻す"):TEXT("会話をすべてミュート"),[Session]{if(Session.IsValid())Session->ToggleQuiet();})];
        for(const auto& Device:Net.Microphones())
            Box->AddSlot().AutoHeight().Padding(0,0,0,5)[Button(TEXT("マイク：")+Device.Value,[Session,Id=Device.Key]{if(Session.IsValid())Session->Network().SelectMicrophone(Id);})];
        Box->AddSlot().AutoHeight().Padding(0,12,0,6)[Label(TEXT("文字で話す"),18)];
        Box->AddSlot().AutoHeight().Padding(0,0,0,8)[SNew(SBox).MaxDesiredHeight(200)
            [SNew(SScrollBox)+SScrollBox::Slot()[SNew(STextBlock).Font(Font(15)).ColorAndOpacity(Ink).AutoWrapText(true)
                .Text_Lambda([Session]{return FText::FromString(Session.IsValid()?Session->ChatText():FString());})]]];
        Box->AddSlot().AutoHeight().Padding(0,0,0,7)[Input(CityChat,240,TEXT("マイクを使わず会話できます"))];
        Box->AddSlot().AutoHeight().Padding(0,0,0,15)[Button(TEXT("送信"),[this,Session]{if(Session.IsValid()){Session->SendChat(CityChat);CityChat.Reset();if(Owner.IsValid())Owner->RefreshUI();}})];
        for(const auto& Pair:Session->Players())
        {
            const FString Id=Pair.Key;Box->AddSlot().AutoHeight().Padding(0,10,0,6)[Label(Pair.Value.Name+(Id==Net.SelfId()?TEXT("（自分）"):Id==Net.HostId()?TEXT("（ホスト）"):TEXT("")),17)];
            if(Id==Net.SelfId())continue;
            auto Row=SNew(SHorizontalBox);
            Row->AddSlot().FillWidth(1).Padding(0,0,5,0)[Button(Session->IsMuted(Id)?TEXT("ミュート解除"):TEXT("ミュート"),[Session,Id]{if(Session.IsValid())Session->ToggleMute(Id);})];
            Row->AddSlot().FillWidth(1).Padding(0,0,5,0)[Button(Session->IsBlocked(Id)?TEXT("ブロック解除"):TEXT("ブロック"),[Session,Id]{if(Session.IsValid())Session->ToggleBlock(Id);})];
            const float Volume=Session->PlayerVolume(Id);
            Row->AddSlot().FillWidth(1)[Button(FString::Printf(TEXT("音量 %d%%"),FMath::RoundToInt(Volume*100)),[this,Session,Id,Volume]{if(Session.IsValid()){Session->SetPlayerVolume(Id,Volume<.75f?1.f:Volume<1.25f?1.5f:.5f);if(Owner.IsValid())Owner->RefreshUI();}})];
            Box->AddSlot().AutoHeight().Padding(0,0,0,6)[Row];
            if(Net.Hosting())Box->AddSlot().AutoHeight().Padding(0,0,0,6)[Button(TEXT("退出・再入場を拒否"),[Session,Id]{if(Session.IsValid())Session->KickPlayer(Id);})];
            Box->AddSlot().AutoHeight().Padding(0,0,0,6)[Button(TEXT("この住人を通報（下の理由を送信）"),[this,Session,Id]{if(Session.IsValid())Session->ReportPlayer(Id,CityReport);})];
        }
        Box->AddSlot().AutoHeight().Padding(0,12,0,6)[Input(CityReport,240,TEXT("通報理由：困った行動と状況を記入"))];
        Box->AddSlot().AutoHeight().Padding(0,0,0,12)[Label(TEXT("通報理由は運営の確認窓口に送信します。ミュートとブロックは自分のPCに保存されます。"),12,Muted)];
        Box->AddSlot().AutoHeight().Padding(0,0,0,12)[Button(Net.Hosting()?TEXT("街を閉じる"):TEXT("街を退出する"),[Session]{if(Session.IsValid())Session->LeaveCity();})];
    }
    Box->AddSlot().AutoHeight().Padding(0,10,0,0)[Button(TEXT("戻る"),[this]{if(Owner.IsValid())Owner->SetMenu(EEWMenu::Main);})];
    return SNew(SScrollBox).ScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll)+SScrollBox::Slot()[Box];
}
