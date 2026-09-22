#include "EWMediaTabletView.h"
#include "EWMediaScreen.h"
#include "EWBrowserSurface.h"
#include "EWGameInstance.h"
#include "Misc/Paths.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSlider.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"

namespace
{
const FLinearColor Ink(.79,.95,.97,1),Muted(.35,.58,.65,1),Dark(.006,.020,.029,1);
const FSlateRoundedBoxBrush Rim(FLinearColor::White,40.f),Glass(FLinearColor::White,36.f);
FSlateFontInfo Font(int Size){return FSlateFontInfo(FPaths::ProjectContentDir()/TEXT("Fonts/DroidSansFallback.ttf"),Size);}
FText Txt(const TCHAR* S){return FText::FromString(S);}
TSharedRef<STextBlock> Text(const FString& S,int Size,FLinearColor Color=Ink)
{return SNew(STextBlock).Text(FText::FromString(S)).Font(Font(Size)).ColorAndOpacity(Color);}
const FButtonStyle& Touch()
{
    static const FButtonStyle S=FButtonStyle().SetNormal(FSlateRoundedBoxBrush(FLinearColor(.027,.10,.135,1),16.f))
        .SetHovered(FSlateRoundedBoxBrush(FLinearColor(.055,.23,.29,1),16.f))
        .SetPressed(FSlateRoundedBoxBrush(FLinearColor(.08,.34,.40,1),16.f))
        .SetDisabled(FSlateRoundedBoxBrush(FLinearColor(.013,.035,.044,1),16.f));return S;
}
const FEditableTextBoxStyle& InputStyle()
{
    static const FEditableTextBoxStyle S=FEditableTextBoxStyle()
        .SetBackgroundImageNormal(FSlateRoundedBoxBrush(FLinearColor(.035,.075,.095,1),15.f))
        .SetBackgroundImageHovered(FSlateRoundedBoxBrush(FLinearColor(.05,.10,.13,1),15.f))
        .SetBackgroundImageFocused(FSlateRoundedBoxBrush(FLinearColor(.05,.14,.18,1),15.f))
        .SetBackgroundImageReadOnly(FSlateRoundedBoxBrush(Dark,15.f)).SetForegroundColor(Ink).SetPadding(FMargin(16,4));return S;
}
}
void SEWMediaTabletView::Construct(const FArguments& A){Owner=A._Owner;Rebuild();}
TSharedRef<SWidget> SEWMediaTabletView::Button(TAttribute<FText> Label,TFunction<void()> Action,bool Playback)
{
    return SNew(SButton).ButtonStyle(&Touch()).ContentPadding(FMargin(12,10))
        .IsEnabled_Lambda([this,Playback]{return Owner.IsValid() && (!Playback || Owner->CanControlPlayback());})
        .OnClicked_Lambda([this,Action,Playback]{if(Owner.IsValid() && (!Playback || Owner->CanControlPlayback()))Action();return FReply::Handled();})
        [SNew(STextBlock).Text(Label).Font(Font(24)).ColorAndOpacity(Ink).Justification(ETextJustify::Center)];
}
void SEWMediaTabletView::Rebuild()
{
    if(!Owner.IsValid())return;
    const auto Screen=Owner;const auto Browser=Owner->Browser();
    auto Body=SNew(SVerticalBox);
    Body->AddSlot().AutoHeight().Padding(2,0,2,12)[SNew(SBox).HeightOverride(48)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[Text(Owner->Title(),32)]
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Font(Font(20)).ColorAndOpacity(Muted)
            .Text_Lambda([Screen]{return Txt(Screen.IsValid() && !Screen->CanControlPlayback()?TEXT("ホストの上映に同期"):TEXT("映写コントロール"));})]]];
    auto Search=[this]{if(Owner.IsValid() && SearchInput)Owner->Search(SearchInput->GetText().ToString());};
    Body->AddSlot().AutoHeight().Padding(0,0,0,12)[SNew(SBox).HeightOverride(54)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,14,0)[SAssignNew(SearchInput,SEditableTextBox).Style(&InputStyle()).Font(Font(28))
            .HintText(Txt(TEXT("YouTubeで検索 / 動画URLを貼り付け"))).Text(FText::FromString(Query))
            .IsEnabled_Lambda([Screen]{return Screen.IsValid() && Screen->CanControlPlayback();})
            .OnTextChanged_Lambda([this](const FText& T){Query=T.ToString();})
            .OnTextCommitted_Lambda([Search](const FText&,ETextCommit::Type T){if(T==ETextCommit::OnEnter)Search();})]
        +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(150)[Button(Txt(TEXT("検索")),Search,true)]]]];
    auto Controls=SNew(SVerticalBox);
    auto Control=[&](TAttribute<FText> Label,FName Action)
    {Controls->AddSlot().AutoHeight().Padding(0,0,0,12)[SNew(SBox).HeightOverride(68)[Button(Label,[Screen,Action]{if(Screen.IsValid())Screen->ControlPlayback(Action);},true)]];};
    Control(TAttribute<FText>::CreateLambda([Browser]{return Txt(Browser && Browser->VideoPaused()?TEXT("▶  再生"):TEXT("Ⅱ  一時停止"));}),TEXT("play"));
    Control(Txt(TEXT("映像を全画面")),TEXT("fullscreen"));
    Control(Txt(TEXT("ページ表示")),TEXT("page"));
    Control(Txt(TEXT("前のページ")),TEXT("back"));
    Control(Txt(TEXT("音声を有効にする")),TEXT("sound"));
    Control(Txt(TEXT("再生を終了")),TEXT("stop"));
    auto Preview=SNew(SOverlay);
    Preview->AddSlot()[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.01,.03,.044,1))
        .HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SVerticalBox)
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Text(TEXT("▷"),96,FLinearColor(.22,.74,.81,1))]
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center).Padding(0,22,0,10)[Text(TEXT("この場所で、映像を選ぶ"),30)]
            +SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)[Text(TEXT("検索した動画を選ぶと、大画面へ映ります"),22,Muted)]]];
    Preview->AddSlot()[SNew(SEWBrowserView).Browser(Browser)
        .IsEnabled_Lambda([Screen]{return Screen.IsValid() && Screen->CanControlPlayback();})];
    Body->AddSlot().AutoHeight()[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(960).HeightOverride(540)[Preview]]
        +SHorizontalBox::Slot().FillWidth(1).Padding(16,0,0,0)[Controls]];
    Body->AddSlot().AutoHeight().Padding(4,14,4,12)[SNew(SBox).HeightOverride(44)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0,0,20,0)[Text(TEXT("自分の音量"),22,Muted)]
        +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(SSlider).StepSize(.05f).SliderBarColor(FLinearColor(.10,.28,.32,1)).SliderHandleColor(Ink).Value_Lambda([Screen]{return Screen.IsValid()?Screen->Volume():0.f;})
            .OnValueChanged_Lambda([Screen](float V){if(Screen.IsValid())Screen->SetVolume(V);})]
        +SHorizontalBox::Slot().AutoWidth().Padding(20,0,0,0)[Button(TAttribute<FText>::CreateLambda([Screen]{return Txt(Screen.IsValid() && Screen->Volume()>.001?TEXT("消音"):TEXT("消音を解除"));}),
            [Screen]{if(Screen.IsValid())Screen->ToggleLocalMute();})]]];
    Body->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(54)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,14,0)[Button(Txt(TEXT("大画面を見る")),[Screen]{if(Screen.IsValid())Screen->LookAtScreen();})]
        +SHorizontalBox::Slot().AutoWidth()[Button(Txt(TEXT("操作を終える  /  Esc")),[Screen]{if(Screen.IsValid())if(auto* G=Screen->GetGameInstance<UEWGameInstance>())G->SetMenu(EEWMenu::None);})]]];
    Body->AddSlot().AutoHeight().Padding(4,12,4,0)[SNew(STextBlock).Font(Font(19)).ColorAndOpacity(Muted).AutoWrapText(true)
        .Text_Lambda([Browser]{return FText::FromString(Browser?Browser->Status():TEXT("音は大型スクリーンから聞こえます"));})];
    auto Idle=SNew(SVerticalBox);
    Idle->AddSlot().AutoHeight().HAlign(HAlign_Center)[Text(TEXT("▷"),132,FLinearColor(.29,.89,.93,1))];
    Idle->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0,30,0,12)[Text(Owner->Title(),47)];
    Idle->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0,0,0,36)[Text(TEXT("上映する動画を選ぶ"),30,Muted)];
    Idle->AddSlot().AutoHeight().HAlign(HAlign_Center)[SNew(STextBlock).Font(Font(28)).ColorAndOpacity(Ink)
        .Text_Lambda([Screen]{if(!Screen.IsValid())return FText();const auto* G=Screen->GetGameInstance<UEWGameInstance>();return FText::FromString(Screen->Nearby()?G->InputHint(TEXT("E  端末を操作"),TEXT("X  端末を操作"),TEXT("□  端末を操作")):TEXT("近づいて操作"));})];
    ChildSlot[SNew(SBorder).Padding(3).BorderImage(&Rim).BorderBackgroundColor(FLinearColor(.16,.58,.64,1))
        [SNew(SBorder).Padding(21).BorderImage(&Glass).BorderBackgroundColor(Dark)[SNew(SOverlay)
            +SOverlay::Slot()[SNew(SBox).Visibility_Lambda([Screen]{return Screen.IsValid() && Screen->TabletOpen()?EVisibility::Visible:EVisibility::Collapsed;})[Body]]
            +SOverlay::Slot().HAlign(HAlign_Center).VAlign(VAlign_Center)[SNew(SBox).Visibility_Lambda([Screen]{return Screen.IsValid() && !Screen->TabletOpen()?EVisibility::HitTestInvisible:EVisibility::Collapsed;})[Idle]]]]];
}
void SEWMediaTabletView::FocusFirst()
{if(SearchInput && SearchInput->IsEnabled())FSlateApplication::Get().SetKeyboardFocus(SearchInput);else FSlateApplication::Get().SetKeyboardFocus(SharedThis(this));}
FReply SEWMediaTabletView::OnPreviewKeyDown(const FGeometry&,const FKeyEvent& E)
{
    if(E.GetKey()==EKeys::Escape || E.GetKey()==EKeys::Gamepad_FaceButton_Right)
    {if(Owner.IsValid())if(auto* G=Owner->GetGameInstance<UEWGameInstance>())G->SetMenu(EEWMenu::None);return FReply::Handled();}
    return FReply::Unhandled();
}
FReply SEWMediaTabletView::OnKeyDown(const FGeometry&,const FKeyEvent&){return FReply::Unhandled();}
