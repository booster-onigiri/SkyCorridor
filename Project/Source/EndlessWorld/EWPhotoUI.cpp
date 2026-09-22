#include "EWInterface.h"
#include "EWGameInstance.h"
#include "EWPhotoMode.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SSlider.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

TSharedRef<SWidget> SEWOverlay::PhotoPanel()
{
    auto* Mode=Owner.IsValid()?Owner->PhotoMode.Get():nullptr;if(!Mode)return Label(TEXT("カメラを準備しています。"));
    const TWeakObjectPtr<AEWPhotoMode> P=Mode;
    auto Refresh=[this]{if(Owner.IsValid())Owner->RefreshUI();};
    auto Box=SNew(SVerticalBox);
    Box->AddSlot().AutoHeight()[Label(TEXT("街の一枚"),30)];
    Box->AddSlot().AutoHeight().Padding(0,8,0,18)[Label(TEXT("雲の動きも、今日の出会いも。\nこの操作パネルは写真に写りません。"),14)];
    Box->AddSlot().AutoHeight().Padding(0,0,0,10)[Button(Mode->Selfie()?TEXT("カメラ：自撮り"):TEXT("カメラ：景色"),[P,Refresh]{if(P.IsValid())P->ToggleSelfie();Refresh();})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,10)[Button(Mode->Portrait()?TEXT("縦長　9：16"):TEXT("横長　16：9"),[P,Refresh]{if(P.IsValid())P->TogglePortrait();Refresh();})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,10)[Button(Mode->Names()?TEXT("名前表示：あり"):TEXT("名前表示：なし"),[P,Refresh]{if(P.IsValid())P->ToggleNames();Refresh();})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,14)[Button(FString::Printf(TEXT("タイマー：%d 秒"),Mode->Timer()),[P,Refresh]{if(P.IsValid())P->CycleTimer();Refresh();})];
    Box->AddSlot().AutoHeight()[Label(TEXT("画角　近く ← → 広く"),15)];
    Box->AddSlot().AutoHeight().Padding(0,10,0,18)[SNew(SSlider).Value_Lambda([P]{return P.IsValid()?(P->FieldOfView()-35)/65.f:0.f;})
        .OnValueChanged_Lambda([P](float V){if(P.IsValid())P->SetFieldOfView(35+V*65);})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,8)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,5,0)[Button(TEXT("左へ"),[P]{if(P.IsValid())P->Turn(-10);})]
        +SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("右へ"),[P]{if(P.IsValid())P->Turn(10);})]];
    Box->AddSlot().AutoHeight().Padding(0,0,0,18)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,5,0)[Button(TEXT("上へ"),[P]{if(P.IsValid())P->Tilt(8);})]
        +SHorizontalBox::Slot().FillWidth(1)[Button(TEXT("下へ"),[P]{if(P.IsValid())P->Tilt(-8);})]];
    Box->AddSlot().AutoHeight().Padding(0,0,0,12)[Button(TEXT("シャッター　F8"),[P]{if(P.IsValid())P->Shoot();})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,16)[SNew(STextBlock).Font(Font(15)).AutoWrapText(true)
        .ColorAndOpacity(FLinearColor(.10,.30,.29,1)).Text_Lambda([P]{return FText::FromString(P.IsValid()?P->Status():FString());})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,10)[Button(TEXT("保存した写真のフォルダー"),[this]{if(Owner.IsValid() && Owner->Store())FPlatformProcess::ExploreFolder(*(Owner->Store()->Root()/TEXT("Screenshots")));})];
    Box->AddSlot().AutoHeight()[Button(TEXT("散策に戻る"),[this]{if(Owner.IsValid())Owner->SetMenu(EEWMenu::None);})];
    return SNew(SScrollBox).ScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll)+SScrollBox::Slot()[Box];
}
