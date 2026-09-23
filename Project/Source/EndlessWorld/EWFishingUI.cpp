#include "EWLocalization.h"
#include "EWInterface.h"
#include "EWGameInstance.h"
#include "EWFishing.h"
#include "EWSocialSession.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElementTypes.h"
#include "Styling/CoreStyle.h"

namespace
{
// A scalable, original field-guide illustration; no remote image or font glyph required.
class SFishPortrait final : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SFishPortrait) {} SLATE_ARGUMENT(FString,Species) SLATE_ARGUMENT(bool,Known) SLATE_END_ARGS()
    void Construct(const FArguments& A){Fish=EWFishing::Find(A._Species);bKnown=A._Known;}
    virtual FVector2D ComputeDesiredSize(float) const override{return FVector2D(150,92);}
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool) const override
    {
        if(!Fish)return Layer;
        const double W=G.GetLocalSize().X,H=G.GetLocalSize().Y,L=FMath::Min(W*.78,H*1.65),X=W*.52,Y=H*.50,R=L*.22*Fish->Slenderness;
        const FLinearColor Colour=bKnown?Fish->Colour:FLinearColor(.38,.46,.45,1);
        auto Line=[&](double A,double B,double C,double D,FLinearColor Tint,float Thickness=2.f)
        {FSlateDrawElement::MakeLines(Out,Layer,G.ToPaintGeometry(),TArray<FVector2D>{{A,B},{C,D}},ESlateDrawEffect::None,Tint,true,Thickness);};
        const int32 Steps=FMath::Max(18,FMath::CeilToInt(R));
        for(int32 I=-Steps;I<=Steps;++I)
        {
            const double T=double(I)/Steps,Span=FMath::Sqrt(FMath::Max(0.,1-T*T))*L*.38;
            Line(X-Span,Y+T*R,X+Span,Y+T*R,Colour*float(.78+.22*(1-T)*.5),float(R/Steps+1));
        }
        for(int32 I=0;I<20;++I)
        {
            const double T=I/19.;Line(X-L*.38-T*L*.17,Y-T*R*.8,X-L*.38-T*L*.17,Y+T*R*.8,Colour*.75f,2.5f);
        }
        Line(X-L*.07,Y-R*.7,X+L*.04,Y-R*1.3,Colour*.65f,4);
        Line(X+L*.05,Y+R*.25,X-L*.07,Y+R*.66,Colour*.60f,4);
        auto Eye=[&](double CX,double CY,double Radius,FLinearColor Tint)
        {for(double J=-Radius;J<=Radius;J+=.8){const double Span=FMath::Sqrt(FMath::Max(0.,Radius*Radius-J*J));Line(CX-Span,CY+J,CX+Span,CY+J,Tint,1.2f);}};
        Eye(X+L*.25,Y-R*.23,L*.030,FLinearColor(.96,.94,.78,1));
        Eye(X+L*.255,Y-R*.23,L*.016,FLinearColor(.015,.03,.035,1));
        Line(X+L*.14,Y-R*.40,X+L*.13,Y+R*.4,Colour*.65f,1.5);
        if(bKnown)for(int32 I=0;I<3;++I)Line(X-L*.15+I*L*.1,Y-R*.2,X-L*.11+I*L*.1,Y-R*.2,Colour*1.2f,2);
        return Layer+1;
    }
private:
    const EWFishing::FSpecies* Fish=nullptr;
    bool bKnown=false;
};
}

TSharedRef<SWidget> SEWOverlay::FishJournalPanel()
{
    auto* G=Owner.Get();auto* Fishing=G?G->Fishing.Get():nullptr;
    if(!Fishing)return Label(EWL::Pick(TEXT("水辺を準備しています。"), TEXT("Preparing the waterside.")));
    const TWeakObjectPtr<AEWFishing> F=Fishing;
    auto Box=SNew(SVerticalBox);
    Box->AddSlot().AutoHeight()[Label(EWL::Pick(TEXT("水辺の魚図鑑"), TEXT("Fish Journal")),32)];
    Box->AddSlot().AutoHeight().Padding(0,8,0,14)[Label(EWL::Format(TEXT("%d / 12 種を発見　　水の流れに、少し寄り道。"), TEXT("%d / 12 species found — A quiet detour by the water."),Fishing->Records().Num()),17)];
    Box->AddSlot().AutoHeight().Padding(0,0,0,12)[Label(EWL::Pick(TEXT("竿を出す → 浮きが沈んだら一度押す。餌の購入や連打は不要です。\n釣果は自動保存され、どの街へ行っても手元に残ります。"), TEXT("Cast your line, then press once when the float sinks. No bait to buy or repeated tapping.\nYour catches save automatically and travel with you to other cities.")),15)];
    Box->AddSlot().AutoHeight().Padding(0,0,0,12)[Button(Fishing->Patient()?EWL::Pick(TEXT("ゆっくり釣る：ON（魚が食いついた後の時間制限なし）"), TEXT("Relaxed fishing: ON (no time limit after a bite)")):EWL::Pick(TEXT("ゆっくり釣る：OFF（食いついてから12秒以内）"), TEXT("Relaxed fishing: OFF (12 seconds to react)")),[F]{if(F.IsValid())F->TogglePatient();})];
    const bool CanVisit=!G->SocialSession || !G->SocialSession->Active();
    auto Locations=SNew(SHorizontalBox);
    for(int32 I=0;I<EWFishing::Spots().Num();++I)
        Locations->AddSlot().FillWidth(1).Padding(0,0,8,0)[Button(EWL::Translate(EWFishing::Spots()[I].Name)+EWL::Pick(TEXT("へ"), TEXT(" — Visit")),[F,I]{if(F.IsValid())F->VisitSpot(I);},CanVisit)];
    Box->AddSlot().AutoHeight().Padding(0,0,0,12)[Locations];
    if(!CanVisit)Box->AddSlot().AutoHeight()[Label(EWL::Pick(TEXT("街での釣りは接続確認後に対応します。一人での散策では今すぐ遊べます。"), TEXT("Online fishing awaits connection testing. You can fish now while exploring alone.")),14)];
    auto Entries=SNew(SVerticalBox);
    for(const auto& Fish:EWFishing::Species())
    {
        const auto* Record=Fishing->Records().FindByPredicate([&](const EWFishing::FRecord& R){return R.Species==Fish.Id;});
        const bool Known=Record!=nullptr;auto Detail=SNew(SVerticalBox);
        Detail->AddSlot().AutoHeight()[Label(Known?Fish.Name:EWL::Pick(TEXT("まだ出会っていない魚"), TEXT("A fish you have not met")),21)];
        Detail->AddSlot().AutoHeight().Padding(0,3,0,3)[Label(EWL::Translate(EWFishing::SiteDescription(Fish.Sites))+TEXT(" / ")+EWL::Translate(EWFishing::TimeDescription(Fish.Hours)),13)];
        Detail->AddSlot().AutoHeight()[Label(Known?Fish.Description:EWL::Pick(TEXT("この水辺と時間帯で、会えるかもしれません。"), TEXT("You may find this fish at this place and time of day.")),14)];
        if(Record)Detail->AddSlot().AutoHeight().Padding(0,5,0,0)[Label(EWL::Format(TEXT("最大 %.1f cm　　釣った数 %lld 匹"), TEXT("Largest %.1f cm   Caught %lld"),Record->Largest,Record->Count),15)];
        auto Row=SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().Padding(0,0,16,0)[SNew(SBox).WidthOverride(135).HeightOverride(96)[SNew(SFishPortrait).Species(Fish.Id).Known(Known)]]
            +SHorizontalBox::Slot().FillWidth(1)[Detail];
        if(Known)Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(10,0,0,0)
            [Button(Fishing->DisplayedFish()==Fish.Id?EWL::Pick(TEXT("展示中"), TEXT("On display")):EWL::Pick(TEXT("水槽に展示"), TEXT("Display in aquarium")),[F,Id=Fish.Id]{if(F.IsValid())F->DisplayFish(Id);},Fishing->DisplayedFish()!=Fish.Id)];
        Entries->AddSlot().AutoHeight().Padding(0,0,0,9)[SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
            .BorderBackgroundColor(Known?FLinearColor(.80,.88,.84,1):FLinearColor(.87,.90,.86,1)).Padding(14)[Row]];
    }
    Box->AddSlot().FillHeight(1)[SNew(SScrollBox).ScrollWhenFocusChanges(EScrollWhenFocusChanges::AnimatedScroll)+SScrollBox::Slot()[Entries]];
    Box->AddSlot().AutoHeight().Padding(0,12,0,7)[Label(EWL::Pick(TEXT("選んだ魚は時計広場の釣り場横にある、あなたの展示水槽へ。\n朝 5–9時 ／ 昼 9–17時 ／ 夕 17–21時 ／ 夜 21–5時（街の時刻）"), TEXT("Your chosen fish appears in your aquarium beside Clock Plaza's fishing spot.\nMorning 05–09 / Day 09–17 / Evening 17–21 / Night 21–05 (city time)")),13)];
    Box->AddSlot().AutoHeight()[Button(EWL::Pick(TEXT("散策に戻る"), TEXT("Return to exploring")),[this]{if(Owner.IsValid())Owner->SetMenu(Owner->SessionStarted()?EEWMenu::None:EEWMenu::Main);})];
    return Box;
}
TSharedRef<SWidget> SEWOverlay::CatchPanel()
{
    auto* G=Owner.Get();auto* Fishing=G?G->Fishing.Get():nullptr;
    if(!Fishing || Fishing->Phase()!=EWFishing::EPhase::Landed)return FishJournalPanel();
    const TWeakObjectPtr<AEWFishing> F=Fishing;const auto& Catch=Fishing->LastCatch();const auto* Fish=EWFishing::Find(Catch.Species);
    if(!Fish)return Label(EWL::Pick(TEXT("釣果を確認しています。"), TEXT("Checking your catch.")));
    auto Box=SNew(SVerticalBox);
    Box->AddSlot().AutoHeight()[Label(Fishing->NewSpecies()?EWL::Pick(TEXT("はじめまして、新しい魚。"), TEXT("A new fish. A first meeting.")):Fishing->NewRecord()?EWL::Pick(TEXT("自己ベストを更新！"), TEXT("A new personal best!")):EWL::Pick(TEXT("今日の水辺から。"), TEXT("From the waters today.")),28)];
    Box->AddSlot().AutoHeight().Padding(0,18)[SNew(SBox).HeightOverride(155)[SNew(SFishPortrait).Species(Fish->Id).Known(true)]];
    Box->AddSlot().AutoHeight()[Label(Fish->Name,34)];
    Box->AddSlot().AutoHeight().Padding(0,6,0,18)[Label(FString::Printf(TEXT("%.1f cm　／　%s"),Catch.Length,*EWL::Translate(EWFishing::Spots()[Catch.Site].Name)),21)];
    Box->AddSlot().AutoHeight().Padding(0,0,0,18)[Label(Fish->Description,17)];
    if(Fishing->Saved())
    {
        Box->AddSlot().AutoHeight().Padding(0,0,0,12)[Label(EWL::Pick(TEXT("図鑑とサイズ記録を保存しました。"), TEXT("Journal and size record saved.")),15)];
        Box->AddSlot().AutoHeight().Padding(0,0,0,10)[Button(EWL::Pick(TEXT("もう一度、竿を出す"), TEXT("Cast again")),[F]{if(F.IsValid())F->CastAgain();})];
        Box->AddSlot().AutoHeight().Padding(0,0,0,10)[Button(Fishing->DisplayedFish()==Fish->Id?EWL::Pick(TEXT("この魚を展示中"), TEXT("This fish is on display")):EWL::Pick(TEXT("この魚を水槽に展示"), TEXT("Display this fish in the aquarium")),[F,Id=Fish->Id]{if(F.IsValid())F->DisplayFish(Id);},Fishing->DisplayedFish()!=Fish->Id)];
    }
    else
    {
        Box->AddSlot().AutoHeight().Padding(0,0,0,12)[Label(Fishing->SaveError(),16,FLinearColor(.65,.17,.10,1))];
        Box->AddSlot().AutoHeight().Padding(0,0,0,10)[Button(EWL::Pick(TEXT("保存を再試行"), TEXT("Retry save")),[F]{if(F.IsValid())F->RetrySave();})];
    }
    Box->AddSlot().AutoHeight().Padding(0,0,0,10)[Button(EWL::Pick(TEXT("水辺の魚図鑑を開く"), TEXT("Open Fish Journal")),[F]{if(F.IsValid())F->OpenJournal();})];
    Box->AddSlot().AutoHeight()[Button(EWL::Pick(TEXT("散策に戻る"), TEXT("Return to exploring")),[this,F]{if(F.IsValid())F->Cancel();if(Owner.IsValid())Owner->SetMenu(EEWMenu::None);})];
    return SNew(SScrollBox)+SScrollBox::Slot()[Box];
}
