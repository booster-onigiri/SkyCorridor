#include "EWLocalization.h"
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
    auto* Mode=Owner.IsValid()?Owner->PhotoMode.Get():nullptr;if(!Mode)return Label(EWL::Pick(TEXT("カメラを準備しています。"), TEXT("Preparing the camera.")));
    const TWeakObjectPtr<AEWPhotoMode> P=Mode;
    auto Refresh=[this]{if(Owner.IsValid())Owner->RefreshUI();};
    auto Box=SNew(SVerticalBox);
    Box->AddSlot().AutoHeight()[Label(EWL::Pick(TEXT("街の一枚"), TEXT("A Moment in the City")),30)];
    Box->AddSlot().AutoHeight().Padding(0,8,0,18)[Label(EWL::Pick(TEXT("雲の動きも、今日の出会いも。\nこの操作パネルは写真に写りません。"), TEXT("Clouds passing, and the people you meet.\nThese controls will not appear in your photo.")),14)];
    Box->AddSlot().AutoHeight().Padding(0,0,0,10)[Button(Mode->Selfie()?EWL::Pick(TEXT("カメラ：自撮り"), TEXT("Camera: Selfie")):EWL::Pick(TEXT("カメラ：景色"), TEXT("Camera: Scenery")),[P,Refresh]{if(P.IsValid())P->ToggleSelfie();Refresh();})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,10)[Button(Mode->Portrait()?EWL::Pick(TEXT("縦長　9：16"), TEXT("Portrait  9:16")):EWL::Pick(TEXT("横長　16：9"), TEXT("Landscape  16:9")),[P,Refresh]{if(P.IsValid())P->TogglePortrait();Refresh();})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,10)[Button(Mode->Names()?EWL::Pick(TEXT("名前表示：あり"), TEXT("Show names: On")):EWL::Pick(TEXT("名前表示：なし"), TEXT("Show names: Off")),[P,Refresh]{if(P.IsValid())P->ToggleNames();Refresh();})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,14)[Button(EWL::Format(TEXT("タイマー：%d 秒"), TEXT("Timer: %d seconds"),Mode->Timer()),[P,Refresh]{if(P.IsValid())P->CycleTimer();Refresh();})];
    Box->AddSlot().AutoHeight()[Label(EWL::Pick(TEXT("画角　近く ← → 広く"), TEXT("Field of view  Close ← → Wide")),15)];
    Box->AddSlot().AutoHeight().Padding(0,10,0,18)[SNew(SSlider).Value_Lambda([P]{return P.IsValid()?(P->FieldOfView()-35)/65.f:0.f;})
        .OnValueChanged_Lambda([P](float V){if(P.IsValid())P->SetFieldOfView(35+V*65);})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,8)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,5,0)[Button(EWL::Pick(TEXT("左へ"), TEXT("Left")),[P]{if(P.IsValid())P->Turn(-10);})]
        +SHorizontalBox::Slot().FillWidth(1)[Button(EWL::Pick(TEXT("右へ"), TEXT("Right")),[P]{if(P.IsValid())P->Turn(10);})]];
    Box->AddSlot().AutoHeight().Padding(0,0,0,18)[SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).Padding(0,0,5,0)[Button(EWL::Pick(TEXT("上へ"), TEXT("Up")),[P]{if(P.IsValid())P->Tilt(8);})]
        +SHorizontalBox::Slot().FillWidth(1)[Button(EWL::Pick(TEXT("下へ"), TEXT("Down")),[P]{if(P.IsValid())P->Tilt(-8);})]];
    Box->AddSlot().AutoHeight().Padding(0,0,0,12)[Button(EWL::Pick(TEXT("シャッター　F8"), TEXT("Shutter  F8")),[P]{if(P.IsValid())P->Shoot();})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,16)[SNew(STextBlock).Font(Font(15)).AutoWrapText(true)
        .ColorAndOpacity(FLinearColor(.10,.30,.29,1)).Text_Lambda([P]{return FText::FromString(P.IsValid()?EWL::Translate(P->Status()):FString());})];
    Box->AddSlot().AutoHeight().Padding(0,0,0,10)[Button(EWL::Pick(TEXT("保存した写真のフォルダー"), TEXT("Open saved photos folder")),[this]{if(Owner.IsValid() && Owner->Store())FPlatformProcess::ExploreFolder(*(Owner->Store()->Root()/TEXT("Screenshots")));})];
    Box->AddSlot().AutoHeight()[Button(EWL::Pick(TEXT("散策に戻る"), TEXT("Return to exploring")),[this]{if(Owner.IsValid())Owner->SetMenu(EEWMenu::None);})];
    return SNew(SScrollBox).ScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll)+SScrollBox::Slot()[Box];
}
