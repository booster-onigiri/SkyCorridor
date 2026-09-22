#include "EWTerminalView.h"
#include "EWTerminal.h"
#include "EWGameInstance.h"
#include "EWBrowserSurface.h"
#include "EWPhotoMode.h"
#include "EWSocialSession.h"
#include "HAL/PlatformApplicationMisc.h"
#include "Misc/Paths.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Framework/Application/SlateApplication.h"
#include "Styling/CoreStyle.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Brushes/SlateNoResource.h"
#include "Rendering/DrawElements.h"
#include "Widgets/SLeafWidget.h"

namespace
{
const FLinearColor Ink(.024,.043,.063,1),Muted(.22,.30,.35,1),Accent(.035,.27,.36,1),Paper(.88,.91,.92,1);
FSlateFontInfo TerminalFont(int32 Size){return FSlateFontInfo(FPaths::ProjectContentDir()/TEXT("Fonts/DroidSansFallback.ttf"),Size);}
const FSlateRoundedBoxBrush Round(FLinearColor::White,28.f),SmallRound(FLinearColor::White,12.f),Pill(FLinearColor::White),ScreenRound(FLinearColor::White,42.f);
const FButtonStyle& TouchStyle()
{
    static FButtonStyle S=FButtonStyle().SetNormal(FSlateNoResource()).SetHovered(FSlateRoundedBoxBrush(FLinearColor(1,1,1,.10),28.f))
        .SetPressed(FSlateRoundedBoxBrush(FLinearColor(0,0,0,.10),28.f)).SetDisabled(FSlateNoResource());return S;
}
TSharedRef<SWidget> Card(TSharedRef<SWidget> Child,FLinearColor Color=FLinearColor(.97,.98,.98,1),FMargin Pad=FMargin(24))
{return SNew(SBorder).BorderImage(&Round).BorderBackgroundColor(Color).Padding(Pad)[Child];}
FLinearColor AppColor(int32 I)
{
    const FLinearColor C[]={FLinearColor(.10,.36,.50,1),FLinearColor(.13,.45,.32,1),FLinearColor(.69,.37,.12,1),
        FLinearColor(.64,.085,.12,1),FLinearColor(.28,.22,.53,1),FLinearColor(.19,.24,.30,1)};
    return C[FMath::Clamp(I,0,5)];
}
int32 MemorySymbol(int32 I)
{
    const int32 Symbols[]={9,10,18,2,1,17,11,12,14,9,3,16,15,12,13};
    return I>=0 && I<UE_ARRAY_COUNT(Symbols)?Symbols[I]:2;
}
FLinearColor MemoryColor(int32 I)
{
    const int32 Colors[]={0,2,4,2,1,4,1,0,5,0,4,0,2,1,4};
    return AppColor(I>=0 && I<UE_ARRAY_COUNT(Colors)?Colors[I]:2)*FLinearColor(.8,.8,.8,1);
}
// Vector symbols retain sharp edges on the physical phone render target.
class SPhoneSymbol : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SPhoneSymbol){} SLATE_ARGUMENT(int32,Kind) SLATE_ARGUMENT(FLinearColor,Color) SLATE_END_ARGS()
    void Construct(const FArguments& A){Kind=A._Kind;Color=A._Color;}
    virtual FVector2D ComputeDesiredSize(float) const override{return FVector2D(100,100);}
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& O,int32 L,const FWidgetStyle&,bool) const override
    {
        const auto Size=G.GetLocalSize();const float K=FMath::Min(Size.X,Size.Y)/100.f;const FVector2D Offset=(Size-FVector2D(100*K))*.5;
        auto Line=[&](TArray<FVector2D> P,float W=4.f){for(auto& V:P)V=Offset+V*K;FSlateDrawElement::MakeLines(O,L,G.ToPaintGeometry(),P,ESlateDrawEffect::None,Color,true,W*K);};
        auto Circle=[&](FVector2D P,float R,float W=4.f){TArray<FVector2D> V;for(int I=0;I<=40;++I)V.Add(P+FVector2D(FMath::Cos(I*PI/20),FMath::Sin(I*PI/20))*R);Line(V,W);};
        if(Kind==0){TArray<FVector2D> P;for(int I=0;I<=48;++I){double A=I*PI/24;P.Add(FVector2D(50+35*FMath::Cos(A),50+20*FMath::Sin(A)));}Line(P);Circle({50,50},10);Line({{50,12},{50,21}});Line({{24,18},{29,26}});Line({{76,18},{71,26}});}
        else if(Kind==1){Line({{16,28},{37,20},{62,30},{84,22},{84,74},{62,82},{37,72},{16,80},{16,28}});Line({{37,20},{37,72}},2);Line({{62,30},{62,82}},2);Line({{28,60},{45,47},{57,53},{72,40}},5);Circle({72,38},6);}
        else if(Kind==2){Line({{21,21},{42,21},{50,28},{58,21},{79,21},{79,78},{58,78},{50,85},{42,78},{21,78},{21,21}});Line({{50,28},{50,83}},3);Line({{29,38},{39,38}},3);Line({{29,49},{39,49}},3);Line({{61,39},{72,39}},3);Line({{61,50},{72,50}},3);}
        else if(Kind==3){Line({{18,27},{82,27},{88,34},{88,68},{82,75},{18,75},{12,68},{12,34},{18,27}},4);Line({{42,38},{64,51},{42,64},{42,38}},5);}
        else if(Kind==4){Circle({36,34},12);Circle({68,40},10);Line({{13,79},{15,66},{24,57},{48,57},{58,66},{60,79}});Line({{58,62},{72,62},{83,70},{85,82}});}
        else if(Kind==5){Line({{15,34},{32,34},{39,24},{62,24},{69,34},{85,34},{85,78},{15,78},{15,34}});Circle({50,55},16);Circle({76,42},2,3);}
        else if(Kind==6){Line({{63,20},{33,50},{63,80}},5);}
        else if(Kind==7){Line({{22,51},{43,71},{79,29}},6);}
        else if(Kind==8){Circle({50,40},18);Line({{36,54},{50,81},{64,54}},4);Circle({50,40},5,3);}
        else if(Kind==9){for(int J=0;J<3;++J){TArray<FVector2D> P;for(int X=15;X<=85;++X)P.Add({double(X),double(34+J*17)+FMath::Sin((X-15)*PI/35)*4});Line(P,3);}Line({{49,13},{43,24},{49,29},{55,24},{49,13}},3);}
        else if(Kind==10){Line({{23,38},{69,38},{66,67},{59,74},{33,74},{26,67},{23,38}});Line({{69,43},{81,43},{85,51},{82,61},{67,63}});Line({{17,83},{77,83}},3);Line({{36,28},{33,21},{37,14}},3);Line({{52,28},{49,21},{53,14}},3);}
        else if(Kind==11){Line({{29,83},{42,58},{65,31}},3);Line({{40,61},{22,58},{18,45},{27,37},{43,43},{48,53}},3);Line({{48,50},{49,30},{62,18},{83,16},{79,37},{64,51},{48,50}},3);}
        else if(Kind==12){Line({{16,83},{84,83}},3);for(int X:{24,50,76})Line({{double(X),78},{double(X),40}},4);TArray<FVector2D> P;for(int J=0;J<=32;++J){double A=PI+J*PI/32;P.Add({50+26*FMath::Cos(A),40+26*FMath::Sin(A)});}Line(P,4);Line({{18,40},{82,40}},3);}
        else if(Kind==13){Line({{20,83},{20,37},{32,20},{44,37},{44,51},{57,51},{57,30},{68,12},{80,30},{80,83},{20,83}},3);Line({{19,37},{44,37}},3);Line({{57,30},{81,30}},3);Line({{46,83},{46,67},{51,61},{56,67},{56,83}},3);Line({{28,47},{34,47},{34,57},{28,57},{28,47}},2);Line({{66,41},{71,41},{71,51},{66,51},{66,41}},2);}
        else if(Kind==14){Line({{29,18},{71,18},{78,28},{78,69},{69,78},{31,78},{22,69},{22,28},{29,18}});Line({{29,30},{71,30},{71,51},{29,51},{29,30}},3);Circle({33,63},3,3);Circle({67,63},3,3);Line({{33,78},{24,88}},3);Line({{67,78},{76,88}},3);}
        else if(Kind==15){Line({{21,84},{28,65},{33,44},{39,23},{61,23},{67,44},{72,65},{79,84},{21,84}},3);for(int Y:{44,64})Line({{double(40-(Y-23)*.28),double(Y)},{double(60+(Y-23)*.28),double(Y)}},3);Line({{46,25},{43,82}},2);Line({{55,25},{59,82}},2);}
        else if(Kind==16){TArray<FVector2D> P;for(int J=0;J<=40;++J){double A=J*PI/20;P.Add({47+33*FMath::Cos(A),42+18*FMath::Sin(A)});}Line(P,3);Line({{24,59},{32,76},{58,76},{68,59}},3);Line({{75,37},{87,24},{89,56},{77,49}},3);Line({{34,64},{57,64}},2);}
        else if(Kind==17){Line({{17,22},{83,22},{83,78},{17,78},{17,22}},4);Line({{25,65},{41,46},{54,57},{63,48},{76,66}},3);Circle({64,36},5,3);}
        else if(Kind==18){Line({{30,48},{30,22},{67,22},{67,49}},4);Line({{24,48},{74,48},{78,66},{20,66},{24,48}},4);Line({{26,67},{23,83}},4);Line({{71,67},{74,83}},4);}
        return L;
    }
private:int32 Kind=0;FLinearColor Color=FLinearColor::White;
};
class SPhoneWallpaper : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SPhoneWallpaper){} SLATE_END_ARGS()
    void Construct(const FArguments&){}
    virtual FVector2D ComputeDesiredSize(float) const override{return {720,1440};}
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& O,int32 L,const FWidgetStyle&,bool) const override
    {
        const auto Size=G.GetLocalSize();
        // Draw the wallpaper inside the same rounded outline as the display mask.
        for(int I=0;I<720;++I)
        {
            const float T=I/719.f,Y=Size.Y*I/720.f,Edge=FMath::Min(Y,Size.Y-Y-Size.Y/720.f),R=42;
            const float Inset=Edge<R?R-FMath::Sqrt(FMath::Max(0.f,R*R-FMath::Square(R-Edge))):0;
            const auto C=FMath::Lerp(FLinearColor(.026,.085,.16,1),FLinearColor(.045,.25,.29,1),T);
            FSlateDrawElement::MakeBox(O,L,G.ToPaintGeometry(FVector2D(Size.X-Inset*2,Size.Y/720+.1),FSlateLayoutTransform(FVector2D(Inset,Y))),FCoreStyle::Get().GetBrush("WhiteBrush"),ESlateDrawEffect::None,C);
        }
        for(int I=0;I<7;++I)
        {
            TArray<FVector2D> P;for(int J=0;J<=100;++J){double A=J*PI/100.;const FVector2D V(Size.X*.78+FMath::Cos(A)*Size.X*(.5+I*.075),Size.Y*.8-FMath::Sin(A)*Size.Y*(.19+I*.055));if(V.X>=2 && V.X<=Size.X-2)P.Add(V);}
            FSlateDrawElement::MakeLines(O,L+1,G.ToPaintGeometry(),P,ESlateDrawEffect::None,FLinearColor(.21,.46,.49,.10),true,2);
        }
        return L+1;
    }
};
}

class SMemoryCityMap : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SMemoryCityMap){} SLATE_ARGUMENT(AEWTerminal*,Owner) SLATE_END_ARGS()
    void Construct(const FArguments& A){Owner=A._Owner;}
    virtual FVector2D ComputeDesiredSize(float) const override{return {620,500};}
    static FVector2D At(int32 I)
    {
        const FVector2D P[]={{95,250},{55,332},{95,170},{270,250},{270,170},{455,250},{455,170},{95,66},{240,340},{435,340},{280,66},{470,66},{395,437},{482,437},{570,437}};
        return I>=0 && I<15?P[I]:FVector2D(570,437);
    }
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& O,int32 L,const FWidgetStyle&,bool) const override
    {
        const auto K=G.GetLocalSize()/FVector2D(620,500);
        auto Line=[&](FVector2D A,FVector2D B,FLinearColor C,float W){FSlateDrawElement::MakeLines(O,L,G.ToPaintGeometry(),TArray<FVector2D>{A*K,B*K},ESlateDrawEffect::None,C,true,W);};
        for(int X=25;X<620;X+=70)Line({double(X),0},{double(X),500},FLinearColor(.72,.81,.82,1),1);
        for(int Y=20;Y<500;Y+=70)Line({0,double(Y)},{620,double(Y)},FLinearColor(.72,.81,.82,1),1);
        Line(At(7),At(11),FLinearColor(.67,.40,.10,1),6);Line(At(8),At(9),Accent,6);Line(At(8),At(10),Muted,3);
        for(auto P:{FIntPoint(0,1),FIntPoint(0,3),FIntPoint(3,5),FIntPoint(1,2),FIntPoint(3,4),FIntPoint(5,6),FIntPoint(0,8),FIntPoint(9,12),FIntPoint(12,13),FIntPoint(13,14)})Line(At(P.X),At(P.Y),FLinearColor(.39,.54,.53,1),3);
        const TCHAR* Labels[]={TEXT("広場"),TEXT("カフェ"),TEXT("ラウンジ"),TEXT("図書館"),TEXT("地図室"),TEXT("美術館"),TEXT("植物園"),TEXT("ホテル"),TEXT("出発駅"),TEXT("水都駅"),TEXT("シアター"),TEXT("空中港"),TEXT("商店塔"),TEXT("回廊院"),TEXT("浮遊城")};
        const int32 Count=Owner.IsValid()?FMath::Min(15,Owner->Places().Num()):0;
        for(int I=0;I<Count;++I)
        {
            const bool Active=Owner->TargetIndex()==I;const FLinearColor C=Active?FLinearColor(.77,.31,.08,1):Accent;
            FSlateDrawElement::MakeBox(O,L+1,G.ToPaintGeometry(FVector2D(Active?28:18)*K,FSlateLayoutTransform((At(I)-FVector2D(Active?14:9))*K)),&Pill,ESlateDrawEffect::None,C);
            FSlateDrawElement::MakeText(O,L+2,G.ToPaintGeometry(FVector2D(130,32),FSlateLayoutTransform((At(I)+FVector2D(I>=12?-34:-44,-40))*K)),Labels[I],TerminalFont(I>=12?18:21),ESlateDrawEffect::None,Ink);
        }
        return L+2;
    }
    virtual FReply OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E) override
    {
        if(!Owner.IsValid() || E.GetEffectingButton()!=EKeys::LeftMouseButton)return FReply::Unhandled();
        const auto P=G.AbsoluteToLocal(E.GetScreenSpacePosition())/G.GetLocalSize()*FVector2D(620,500);
        int32 Best=INDEX_NONE;double Distance=40;
        for(int I=0;I<Owner->Places().Num();++I){double D=FVector2D::Distance(P,At(I));if(D<Distance){Distance=D;Best=I;}}
        if(Best!=INDEX_NONE){Owner->SelectTarget(Best);return FReply::Handled();}return FReply::Unhandled();
    }
private:TWeakObjectPtr<AEWTerminal> Owner;
};

void SEWTerminalView::Construct(const FArguments& A){Owner=A._Owner;Rebuild();}
TSharedRef<SWidget> SEWTerminalView::Text(const FString& V,int32 Size,FLinearColor C)
{return SNew(STextBlock).Text(FText::FromString(V)).Font(TerminalFont(Size)).ColorAndOpacity(C).AutoWrapText(true);}
TSharedRef<SWidget> SEWTerminalView::Button(const FString& Label,TFunction<void()> Action,bool Enabled)
{
    auto B=SNew(SButton).ButtonStyle(&TouchStyle()).ContentPadding(FMargin(18,18)).IsEnabled(Enabled)
        .OnClicked_Lambda([A=MoveTemp(Action)]{A();return FReply::Handled();})[Text(Label,28,FLinearColor::White)];
    Buttons.Add(B);return Card(B,Enabled?Accent:Muted,FMargin(0));
}
TSharedRef<SWidget> SEWTerminalView::Icon(int32 Kind,float Size,FLinearColor C)
{return SNew(SBox).WidthOverride(Size).HeightOverride(Size)[SNew(SPhoneSymbol).Kind(Kind).Color(C)];}
void SEWTerminalView::Rebuild()
{
    if(Query)SearchText=Query->GetText().ToString();Query.Reset();Buttons.Reset();if(!Owner.IsValid())return;
    const int Page=FMath::Clamp(Owner->PageIndex(),0,5);const bool HomePage=Page==0;
    auto Stack=SNew(SVerticalBox);const auto Foreground=HomePage?FLinearColor::White:Ink;
    Stack->AddSlot().AutoHeight().Padding(26,14,26,10)
    [SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Text_Lambda([]{return FText::FromString(FDateTime::Now().ToString(TEXT("%H:%M")));}).Font(TerminalFont(25)).ColorAndOpacity(Foreground)]
        +SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(174).HeightOverride(38)[SNew(SBorder).BorderImage(&Pill).BorderBackgroundColor(FLinearColor(.006,.009,.012,1))]]
        +SHorizontalBox::Slot().FillWidth(1).HAlign(HAlign_Right).VAlign(VAlign_Center)[Text(TEXT("MEMORIA"),18,Foreground)]];
    if(!HomePage)
    {
        const TCHAR* Names[]={TEXT(""),TEXT("地図"),TEXT("記録"),TEXT("YouTube"),TEXT("友人"),TEXT("観測")};
        auto BackButton=SNew(SButton).ButtonStyle(&TouchStyle()).ContentPadding(8).OnClicked_Lambda([this]{Back();return FReply::Handled();})[Icon(6,38,Accent)];Buttons.Add(BackButton);
        Stack->AddSlot().AutoHeight().Padding(2,14,8,26)[SNew(SHorizontalBox)
            +SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[BackButton]
            +SHorizontalBox::Slot().FillWidth(1).Padding(12,0).VAlign(VAlign_Center)[Text(Names[Page],43,Ink)]];
    }
    auto Body=SNew(SVerticalBox);
    if(Page==0)Home(Body);else if(Page==1)Map(Body);else if(Page==2)Records(Body);else if(Page==3)Video(Body);else if(Page==4)Friends(Body);else Observe(Body);
    if(HomePage)Stack->AddSlot().FillHeight(1)[Body];
    else Stack->AddSlot().FillHeight(1)[SNew(SScrollBox)+SScrollBox::Slot()[Body]];
    auto HomeBar=SNew(SButton).ButtonStyle(&TouchStyle()).ContentPadding(FMargin(140,23)).OnClicked_Lambda([this]{RecordDetail=INDEX_NONE;if(Owner->PageIndex()==0)Owner->Close();else Owner->ShowPage(0);return FReply::Handled();})
        [SNew(SBox).HeightOverride(7).WidthOverride(180)[SNew(SBorder).BorderImage(&Pill).BorderBackgroundColor(Foreground)]];Buttons.Add(HomeBar);
    Stack->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0,12,0,5)[HomeBar];
    auto Overlay=SNew(SOverlay);
    if(HomePage)Overlay->AddSlot()[SNew(SPhoneWallpaper)];
    else Overlay->AddSlot()[SNew(SBorder).BorderImage(&ScreenRound).BorderBackgroundColor(Paper)];
    Overlay->AddSlot().Padding(30,14,30,12)[Stack];
    ChildSlot[SNew(SBorder).BorderImage(&ScreenRound).BorderBackgroundColor(FLinearColor(.006,.009,.012,1)).Padding(3)[Overlay]];
}
void SEWTerminalView::Home(TSharedRef<SVerticalBox> Box)
{
    const auto* G=Owner->GetGameInstance<UEWGameInstance>();
    const bool FriendsAvailable=G && G->IsMenuAvailable(EEWMenu::City);
    const TCHAR* Labels[]={TEXT("観測"),TEXT("地図"),TEXT("記録"),TEXT("YouTube"),TEXT("友人"),TEXT("カメラ")};
    const int Pages[]={5,1,2,3,4,-1};Box->AddSlot().FillHeight(.6);
    for(int Row=0;Row<3;++Row)
    {
        auto Line=SNew(SHorizontalBox);
        for(int Col=0;Col<2;++Col)
        {
            const int I=Row*2+Col,Page=Pages[I];auto Tile=SNew(SVerticalBox);
            const bool Available=Page!=4 || FriendsAvailable;
            const FLinearColor Foreground=Available?FLinearColor::White:FLinearColor(.60,.64,.66,1);
            Tile->AddSlot().AutoHeight().HAlign(HAlign_Center)[SNew(SBox).WidthOverride(188).HeightOverride(188)[Card(Icon(I,108,Foreground),Available?AppColor(I):FLinearColor(.16,.19,.21,1),FMargin(20))]];
            Tile->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0,20,0,0)[Text(Labels[I],29,Foreground)];
            if(!Available)Tile->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0,5,0,0)[Text(TEXT("開発中"),22,Foreground)];
            auto B=SNew(SButton).ButtonStyle(&TouchStyle()).ContentPadding(FMargin(12,14)).IsEnabled(Available).OnClicked_Lambda([this,Page]
            {if(Owner.IsValid()){RecordDetail=INDEX_NONE;if(Page>=0)Owner->ShowPage(Page);else if(auto* G=Owner->GetGameInstance<UEWGameInstance>()){Owner->Close();if(G->PhotoMode)G->PhotoMode->Open();}}return FReply::Handled();})[Tile];Buttons.Add(B);
            Line->AddSlot().FillWidth(1).HAlign(HAlign_Center)[B];
        }
        Box->AddSlot().AutoHeight()[Line];Box->AddSlot().FillHeight(Row==2?.7:1.);
    }
}
void SEWTerminalView::Map(TSharedRef<SVerticalBox> Box)
{
    auto* T=Owner.Get();const int Target=T->TargetIndex();
    Box->AddSlot().AutoHeight()[Card(SNew(SBox).HeightOverride(500)[SNew(SMemoryCityMap).Owner(T)],FLinearColor(.76,.85,.85,1),FMargin(8))];
    Box->AddSlot().AutoHeight().Padding(6,15,0,22)[Text(TEXT("上層線  ━   水都線  ━   徒歩・昇降機  ─"),21,Muted)];
    if(T->Places().IsValidIndex(Target))
    {
        auto Guide=SNew(SVerticalBox);Guide->AddSlot().AutoHeight()[Text(TEXT("目的地"),23,Muted)];
        Guide->AddSlot().AutoHeight().Padding(0,8,0,14)[Text(T->Places()[Target].Area,34,Ink)];
        Guide->AddSlot().AutoHeight()[Text(T->RouteText(Target),27,Muted)];
        Guide->AddSlot().AutoHeight().Padding(0,22,0,0)[Button(TEXT("案内を見ながら歩く"),[this]{Owner->Close();})];
        Box->AddSlot().AutoHeight().Padding(0,0,0,26)[Card(Guide)];
    }
    Box->AddSlot().AutoHeight().Padding(4,0,0,18)[Text(TEXT("行き先を選ぶ"),30,Ink)];
    for(int I=0;I<T->Places().Num();++I)
    {
        auto Row=SNew(SHorizontalBox);Row->AddSlot().AutoWidth().VAlign(VAlign_Center)[Icon(I==Target?8:T->Recorded(I)?7:1,42,Accent)];
        Row->AddSlot().FillWidth(1).Padding(20,0)[Text(T->Places()[I].Area,27,Ink)];
        auto B=SNew(SButton).ButtonStyle(&TouchStyle()).ContentPadding(20).OnClicked_Lambda([this,I]{Owner->SelectTarget(I);return FReply::Handled();})[Row];Buttons.Add(B);
        Box->AddSlot().AutoHeight().Padding(0,0,0,8)[Card(B,FLinearColor(.97,.98,.98,1),FMargin(0))];
    }
}
void SEWTerminalView::Records(TSharedRef<SVerticalBox> Box)
{
    auto* T=Owner.Get();
    if(T->Places().IsValidIndex(RecordDetail) && T->Recorded(RecordDetail))
    {
        const auto& P=T->Places()[RecordDetail];
        Box->AddSlot().AutoHeight()[Card(SNew(SBox).HeightOverride(220)[Icon(MemorySymbol(RecordDetail),140,FLinearColor(.9,.94,.91,1))],MemoryColor(RecordDetail))];
        Box->AddSlot().AutoHeight().Padding(4,30,4,10)[Text(P.Area,25,Muted)];
        Box->AddSlot().AutoHeight().Padding(4,0,4,28)[Text(P.Place.Name,41,Ink)];
        Box->AddSlot().AutoHeight().Padding(4,0,4,34)[Text(P.Record,32,Ink)];
        Box->AddSlot().AutoHeight()[Button(TEXT("この場所を地図で見る"),[this]{Owner->SelectTarget(RecordDetail);Owner->ShowPage(1);})];return;
    }
    Box->AddSlot().AutoHeight().Padding(4,0,0,22)[Text(FString::Printf(TEXT("見つけた時間   %d / %d"),T->RecordCount(),T->Places().Num()),27,Muted)];
    if(!T->RecordCount())Box->AddSlot().AutoHeight().Padding(0,0,0,25)[Card(Text(TEXT("「観測」で街に残る気配を探すと、ここに記録が届きます。"),27,Muted))];
    for(int R=0;R<T->Places().Num();R+=2)
    {
        auto Row=SNew(SHorizontalBox);
        for(int C=0;C<2;++C)
        {
            int I=R+C;if(!T->Places().IsValidIndex(I)){Row->AddSlot().FillWidth(1);continue;}
            bool Found=T->Recorded(I);auto Cover=SNew(SVerticalBox);
            Cover->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(160)[Card(Icon(Found?MemorySymbol(I):0,82,Found?FLinearColor(.9,.94,.91,1):FLinearColor(.44,.56,.6,1)),Found?MemoryColor(I):FLinearColor(.64,.71,.73,1),FMargin(0))]];
            Cover->AddSlot().AutoHeight().Padding(2,14,2,6)[Text(Found?T->Places()[I].Place.Name:TEXT("まだ見ぬ時間"),27,Ink)];
            Cover->AddSlot().AutoHeight().Padding(2,0,2,6)[Text(T->Places()[I].Area,22,Muted)];
            auto B=SNew(SButton).ButtonStyle(&TouchStyle()).ContentPadding(12).OnClicked_Lambda([this,I,Found]{if(Found){RecordDetail=I;Rebuild();}else{Owner->SelectTarget(I);Owner->ShowPage(1);}return FReply::Handled();})[Cover];Buttons.Add(B);
            Row->AddSlot().FillWidth(1).Padding(C?8:0,0,C?0:8,0)[Card(B,FLinearColor(.97,.98,.98,1),FMargin(0))];
        }
        Box->AddSlot().AutoHeight().Padding(0,0,0,18)[Row];
    }
}
void SEWTerminalView::Video(TSharedRef<SVerticalBox> Box)
{
    auto Browser=Owner->Browser();
    static const FEditableTextBoxStyle SearchStyle=FEditableTextBoxStyle(FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox"))
        .SetBackgroundImageNormal(FSlateRoundedBoxBrush(FLinearColor::White,14.f))
        .SetBackgroundImageHovered(FSlateRoundedBoxBrush(FLinearColor::White,14.f))
        .SetBackgroundImageFocused(FSlateRoundedBoxBrush(FLinearColor::White,14.f,Accent,2.f))
        .SetForegroundColor(Ink).SetFocusedForegroundColor(Ink);
    Query=SNew(SEditableTextBox).Style(&SearchStyle).Font(TerminalFont(28)).Text(FText::FromString(SearchText)).ForegroundColor(Ink).BackgroundColor(FLinearColor::White).HintText(FText::FromString(TEXT("動画名・YouTube URL")))
        .Padding(FMargin(14,18)).OnTextCommitted_Lambda([this](const FText& V,ETextCommit::Type How){if(How==ETextCommit::OnEnter)Owner->SearchVideo(V.ToString());});
    Box->AddSlot().AutoHeight().Padding(0,0,0,15)[Query.ToSharedRef()];
    Box->AddSlot().AutoHeight().Padding(0,0,0,28)[Button(TEXT("検索"),[this]{if(Query)Owner->SearchVideo(Query->GetText().ToString());})];
    auto Player=SNew(SOverlay);
    Player->AddSlot()[SNew(SBorder).BorderImage(&SmallRound).BorderBackgroundColor(FLinearColor(.006,.009,.014,1))];
    Player->AddSlot().HAlign(HAlign_Center).VAlign(VAlign_Center)[Icon(3,96,FLinearColor(.70,.75,.77,1))];
    Player->AddSlot()[SNew(SEWBrowserView).Browser(Browser)];
    // Keep the 16:9 surface aspect ratio within the portrait screen.
    Box->AddSlot().AutoHeight()[SNew(SBox).HeightOverride(362)[Player]];
    Box->AddSlot().AutoHeight().Padding(4,20,4,20)[SNew(STextBlock).Text_Lambda([Browser]{return FText::FromString(Browser && Browser->HasPage()?Browser->Status():TEXT("見たい動画を探そう"));}).Font(TerminalFont(25)).ColorAndOpacity(Muted).AutoWrapText(true)];
    Box->AddSlot().AutoHeight().Padding(0,0,0,12)[Button(TEXT("映像のみ / ページ表示"),[Browser]{if(Browser)Browser->SetVideoFullscreen(!Browser->IsVideoFullscreen());})];
    Box->AddSlot().AutoHeight()[Button(TEXT("再生を止める"),[this]{Owner->StopVideo();})];
    Box->AddSlot().AutoHeight().Padding(4,32,4,0)[Text(TEXT("端末をしまうと一時停止"),24,Muted)];
}
void SEWTerminalView::Friends(TSharedRef<SVerticalBox> Box)
{
    auto* G=Owner->GetGameInstance<UEWGameInstance>();auto* S=G?G->SocialSession.Get():nullptr;auto Summary=SNew(SVerticalBox);
    const bool Available=G && G->IsMenuAvailable(EEWMenu::City);
    Summary->AddSlot().AutoHeight().HAlign(HAlign_Center)[Icon(4,116,Accent)];
    Summary->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0,18,0,12)[Text(S && S->Active()?TEXT("同じ街を歩いています"):TEXT("いまは、ひとりで散策中"),30,Ink)];
    if(S && S->Active())Summary->AddSlot().AutoHeight().HAlign(HAlign_Center)[Text(FString::Printf(TEXT("%d 人が滞在中"),S->Players().Num()),25,Muted)];
    Box->AddSlot().AutoHeight().Padding(0,0,0,26)[Card(Summary)];
    Box->AddSlot().AutoHeight().Padding(0,0,0,18)[Button(Available?TEXT("街を探す・開く"):TEXT("街を探す・開く　開発中"),[this]{Owner->InviteFriends();},Available)];
    if(Available && S && !S->Invite().IsEmpty())
    {
        const auto Code=S->Invite();Box->AddSlot().AutoHeight().Padding(0,0,0,18)[Card(Text(Code,27,Ink))];
        Box->AddSlot().AutoHeight().Padding(0,0,0,26)[Button(bCodeCopied?TEXT("コピーしました"):TEXT("招待コードをコピー"),[this,Code]{FPlatformApplicationMisc::ClipboardCopy(*Code);bCodeCopied=true;Rebuild();})];
    }
    if(S && S->Active())for(const auto& Pair:S->Players())
    {
        auto Row=SNew(SHorizontalBox);Row->AddSlot().AutoWidth()[Icon(4,52,Accent)];
        Row->AddSlot().FillWidth(1).VAlign(VAlign_Center).Padding(18,0)[Text(Pair.Value.Name,28,Ink)];
        Box->AddSlot().AutoHeight().Padding(0,0,0,10)[Card(Row)];
    }
    Box->AddSlot().AutoHeight().Padding(4,24,4,0)[Text(Available?TEXT("友人には招待コードを共有。\n会話やマイクの設定は、街の参加画面から。"):TEXT("友人機能は開発中です。\nこの公開版では利用できません。"),26,Muted)];
}
void SEWTerminalView::Observe(TSharedRef<SVerticalBox> Box)
{
    auto* T=Owner.Get();auto Hero=SNew(SVerticalBox);
    Hero->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0,34)[Icon(0,180,FLinearColor(.72,.9,.92,1))];
    Hero->AddSlot().AutoHeight().HAlign(HAlign_Center).Padding(0,0,0,30)[Text(T->Observing()?TEXT("観測中"):TEXT("過去の気配を探す"),36,FLinearColor::White)];
    Box->AddSlot().AutoHeight().Padding(0,0,0,24)[Card(Hero,AppColor(0))];
    Box->AddSlot().AutoHeight().Padding(4,8,4,30)[Text(TEXT("端末を下ろすと、街のどこかに薄い人影。\n近くで眺め、E でその時間を記録する。"),29,Muted)];
    Box->AddSlot().AutoHeight().Padding(0,0,0,26)[Button(T->Observing()?TEXT("観測を終える"):TEXT("観測を始める"),[this]{Owner->ToggleObservation();})];
    if(T->Places().IsValidIndex(T->TargetIndex()))
    {
        auto Next=SNew(SVerticalBox);Next->AddSlot().AutoHeight()[Text(TEXT("次に訪ねる場所"),24,Muted)];
        Next->AddSlot().AutoHeight().Padding(0,12,0,22)[Text(T->Places()[T->TargetIndex()].Area,34,Ink)];
        Next->AddSlot().AutoHeight()[Button(TEXT("地図で見る"),[this]{Owner->ShowPage(1);})];Box->AddSlot().AutoHeight()[Card(Next)];
    }
}
void SEWTerminalView::Back(){if(Owner.IsValid()){if(Owner->PageIndex()==2 && RecordDetail!=INDEX_NONE){RecordDetail=INDEX_NONE;Rebuild();}else Owner->ShowPage(0);}}
void SEWTerminalView::FocusFirst(){if(Buttons.Num())FSlateApplication::Get().SetKeyboardFocus(Buttons[0]);}
FReply SEWTerminalView::OnKeyDown(const FGeometry&,const FKeyEvent& E)
{
    if(!Owner.IsValid())return FReply::Unhandled();
    if(E.GetKey()==EKeys::Q){Owner->Close();return FReply::Handled();}
    if(E.GetKey()==EKeys::Escape || E.GetKey()==EKeys::Gamepad_FaceButton_Right){if(Owner->PageIndex()==0)Owner->Close();else Back();return FReply::Handled();}
    return FReply::Unhandled();
}

