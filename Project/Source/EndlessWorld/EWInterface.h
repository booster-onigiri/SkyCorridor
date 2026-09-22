#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "EWWorld.h"

class UEWGameInstance;
class SEditableTextBox;
class SScrollBox;

class SEWOverlay : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SEWOverlay) {} SLATE_ARGUMENT(UEWGameInstance*, Owner) SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    void Rebuild();
    void FocusInitialControl();
    FString FocusedLabel() const;
    virtual bool SupportsKeyboardFocus() const override { return true; }
    virtual FReply OnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual FReply OnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
    virtual FReply OnAnalogValueChanged(const FGeometry& Geometry, const FAnalogInputEvent& Event) override;
private:
    TWeakObjectPtr<UEWGameInstance> Owner;
    TSharedPtr<SEditableTextBox> CodeInput;
    TSharedPtr<SEditableTextBox> OnlineNameInput,OnlineCodeInput;
    FString OnlineCode,OnlineName=TEXT("旅人");
    TSharedPtr<SScrollBox> SettingsScroll;
    float SettingsScrollOffset = 0;
    bool bRestoreSettingsScroll = false;
    bool bShowLegacyWorlds = false;
    TArray<TWeakPtr<SWidget>> FocusControls;
    TArray<TFunction<void()>> FocusActions;
    TArray<FString> FocusLabels;
    int32 FocusIndex = 0, PreviousMenu = -1;
    double NextStickNavigation = 0;
    void MoveFocus(int32 Step);
    int32 CurrentFocus() const;
    FSlateFontInfo Font(int32 Size) const;
    TSharedRef<SWidget> Button(const FString& Label, TFunction<void()> Action, bool Enabled = true);
    TSharedRef<SWidget> Label(const FString& Text, int32 Size = 16, FLinearColor Colour = FLinearColor(.09, .13, .14, 1));
    TSharedRef<SWidget> MainPanel();
    TSharedRef<SWidget> JournalPanel();
    TSharedRef<SWidget> SettingsPanel();
    TSharedRef<SWidget> LiftPanel();
    TSharedRef<SWidget> OnlinePanel();
    TSharedRef<SWidget> CityPanel();
    TSharedRef<SWidget> FishJournalPanel();
    TSharedRef<SWidget> CatchPanel();
    TSharedRef<SWidget> PhotoPanel();
    TSharedRef<SWidget> WorkshopPanel();
    TSharedRef<SWidget> ChessPanel();
    TSharedRef<SWidget> ExplorePanel();
    TSharedRef<SWidget> SkyResidencesPanel();
    int32 ChessFrom=-1,ChessPromotion=5;
    FString CityName=TEXT("雲の街"),CityLanguage=TEXT("日本語"),CityActivity=TEXT("散策"),CityNickname,CityCode,CityChat,CityReport;
    TSharedRef<SWidget> WorldsPanel();
    TSharedRef<SWidget> SaveFailurePanel();
    TSharedRef<SWidget> DiscoveryRow(const EW::PlaceBookmark& Place);
};
