#pragma once
#include "Widgets/SCompoundWidget.h"
class AEWTerminal;
class SVerticalBox;
class SEditableTextBox;
class SEWTerminalView : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SEWTerminalView){} SLATE_ARGUMENT(AEWTerminal*,Owner) SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    void Rebuild();
    void FocusFirst();
    virtual bool SupportsKeyboardFocus() const override {return true;}
    virtual FReply OnKeyDown(const FGeometry&,const FKeyEvent&) override;
private:
    TWeakObjectPtr<AEWTerminal> Owner;
    TSharedPtr<SEditableTextBox> Query;
    TArray<TSharedPtr<SWidget>> Buttons;
    FString SearchText;
    int32 RecordDetail=INDEX_NONE;
    bool bCodeCopied=false;
    TSharedRef<SWidget> Text(const FString& Value,int32 Size=30,FLinearColor Color=FLinearColor(.024,.043,.063,1));
    TSharedRef<SWidget> Button(const FString& Label,TFunction<void()> Action,bool Enabled=true);
    TSharedRef<SWidget> Icon(int32 Kind,float Size,FLinearColor Color=FLinearColor::White);
    void Back();
    void Home(TSharedRef<SVerticalBox> Box);
    void Map(TSharedRef<SVerticalBox> Box);
    void Records(TSharedRef<SVerticalBox> Box);
    void Video(TSharedRef<SVerticalBox> Box);
    void Friends(TSharedRef<SVerticalBox> Box);
    void Observe(TSharedRef<SVerticalBox> Box);
};
