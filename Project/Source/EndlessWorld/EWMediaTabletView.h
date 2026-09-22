#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
class AEWMediaScreen;
class SEditableTextBox;

// This widget exists only on the physical tablet render target, never in the HUD.
class SEWMediaTabletView : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SEWMediaTabletView){} SLATE_ARGUMENT(AEWMediaScreen*, Owner) SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    void Rebuild();
    void FocusFirst();
    virtual bool SupportsKeyboardFocus() const override{return true;}
    virtual FReply OnKeyDown(const FGeometry&,const FKeyEvent&) override;
    virtual FReply OnPreviewKeyDown(const FGeometry&,const FKeyEvent&) override;
    FString QueryText() const{return Query;}
private:
    TWeakObjectPtr<AEWMediaScreen> Owner;
    TSharedPtr<SEditableTextBox> SearchInput;
    FString Query;
    TSharedRef<SWidget> Button(TAttribute<FText> Label,TFunction<void()> Action,bool Playback=false);
};
