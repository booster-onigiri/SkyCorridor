#include "EWClock.h"
#include "Components/WidgetComponent.h"
#include "Widgets/SLeafWidget.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Rendering/DrawElements.h"
#include "Styling/CoreStyle.h"
#include "Dom/JsonObject.h"
#include "Misc/Paths.h"

namespace EWClock
{
bool IsCentralPlaza(const EW::ChunkRecipe& R)
{return R.Coord==EW::ChunkCoord{0,0} && R.World.Seed==EW::WorldDescriptor::ReferenceWorld().Seed;}
FVector ScreenCentre(const EW::ChunkRecipe& R)
{
    // The south-facing black dial from build_kit.py, including the landmark's
    // actual scale. The last +4.5 m move was reduced by half: net +2.25 m.
    if(const auto* P=R.Parts.FindByPredicate([](const EW::Part& P){return P.Mesh==TEXT("ClockTower");}))
        return P->Transform.TransformPosition(FVector(0,-218,1700))+FVector(0,0,225);
    return R.Hub+FVector(0,-91.56,769);
}
FVector AnglesAt(const FDateTime& T)
{
    const double Seconds=T.GetSecond();
    const double Minutes=T.GetMinute()+Seconds/60.;
    return FVector((T.GetHour()%12+Minutes/60.)*30.,Minutes*6.,Seconds*6.);
}
}

class SEWClockFace final:public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SEWClockFace){} SLATE_END_ARGS()
    void Construct(const FArguments&){SetCanTick(true);}
    virtual FVector2D ComputeDesiredSize(float)const override{return FVector2D(256,256);}
    virtual void Tick(const FGeometry&,double,float)override
    {const auto T=FDateTime::Now();if(T.GetTicks()/ETimespan::TicksPerSecond!=LastPaint.GetTicks()/ETimespan::TicksPerSecond)Invalidate(EInvalidateWidgetReason::Paint);}
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 L,const FWidgetStyle&,bool)const override
    {
        LastPaint=FDateTime::Now();LastAngles=EWClock::AnglesAt(LastPaint);++Paints;
        FSlateDrawElement::MakeBox(Out,L,G.ToPaintGeometry(),&Rim);
        FSlateDrawElement::MakeBox(Out,L+1,G.ToPaintGeometry(FVector2D(234,234),FSlateLayoutTransform(FVector2D(11,11))),&Dial);
        const FVector2D Centre(128,128);
        auto Point=[&](double Degrees,double Radius){const double A=FMath::DegreesToRadians(Degrees);return Centre+FVector2D(FMath::Sin(A),-FMath::Cos(A))*Radius;};
        const FLinearColor Ink(.012,.023,.025,1);
        for(int I=0;I<60;++I)
        {
            const bool Major=I%5==0;TArray<FVector2D> P{Point(I*6,Major?95:105),Point(I*6,112)};
            FSlateDrawElement::MakeLines(Out,L+2,G.ToPaintGeometry(),P,ESlateDrawEffect::None,Ink,true,Major?3.f:1.f);
        }
        const FSlateFontInfo Font(FPaths::ProjectContentDir()/TEXT("Fonts/DroidSansFallback.ttf"),20);
        for(int I=0;I<4;++I)
        {
            const FString Text=FString::FromInt(I==0?12:I*3);
            const FVector2D Pos=Point(I*90,77)-FVector2D(Text.Len()*6.2,12);
            FSlateDrawElement::MakeText(Out,L+3,G.ToPaintGeometry(FVector2D(36,28),FSlateLayoutTransform(Pos)),Text,Font,ESlateDrawEffect::None,Ink);
        }
        auto Hand=[&](double Angle,double Length,float Width,FLinearColor Colour)
        {TArray<FVector2D> P{Point(Angle,-10),Point(Angle,Length)};FSlateDrawElement::MakeLines(Out,L+4,G.ToPaintGeometry(),P,ESlateDrawEffect::None,Colour,true,Width);};
        Hand(LastAngles.X,56,7,Ink);Hand(LastAngles.Y,85,4,Ink);Hand(LastAngles.Z,95,2,FLinearColor(.38,.075,.02,1));
        FSlateDrawElement::MakeBox(Out,L+5,G.ToPaintGeometry(FVector2D(10,10),FSlateLayoutTransform(FVector2D(123,123))),&Rim);
        return L+5;
    }
    TSharedRef<FJsonObject> Evidence()const
    {
        auto O=MakeShared<FJsonObject>();O->SetStringField(TEXT("painted_local_time"),LastPaint.ToString(TEXT("%Y-%m-%d %H:%M:%S")));
        O->SetStringField(TEXT("system_local_time"),FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M:%S")));
        O->SetNumberField(TEXT("hour_degrees"),LastAngles.X);O->SetNumberField(TEXT("minute_degrees"),LastAngles.Y);
        O->SetNumberField(TEXT("second_degrees"),LastAngles.Z);O->SetNumberField(TEXT("paint_count"),double(Paints));return O;
    }
private:
    FSlateRoundedBoxBrush Rim{FLinearColor(.025,.042,.044,1),128.f};
    FSlateRoundedBoxBrush Dial{FLinearColor(.75,.79,.69,1),117.f};
    mutable FDateTime LastPaint=FDateTime::MinValue();
    mutable FVector LastAngles=FVector::ZeroVector;
    mutable uint64 Paints=0;
};

void EWClock::AddFaces(AActor* Owner,USceneComponent* Parent,const EW::Part& P,TArray<TObjectPtr<UWidgetComponent>>& Out)
{
    for(int Side=0;Side<4;++Side)
    {
        const FRotator Turn(0,Side*90,0);
        auto* Face=NewObject<UWidgetComponent>(Owner);Face->SetupAttachment(Parent);
        Face->SetWidgetSpace(EWidgetSpace::World);Face->SetDrawSize(FVector2D(256,256));Face->SetPivot(FVector2D(.5,.5));
        Face->SetBlendMode(EWidgetBlendMode::Masked);Face->SetTwoSided(false);Face->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        Face->SetCastShadow(false);Face->SetTickWhenOffscreen(false);Face->SetRedrawTime(.2f);
        Face->SetTintColorAndOpacity(FLinearColor(3000,3000,3000,1));
        const FVector Scale=P.Transform.GetScale3D();
        // Sit ahead of the 2.45m cornice as well as the baked hands, so the
        // lower dial remains visible where it crosses the tower's trim.
        const FVector Location=P.Transform.TransformPosition(Turn.RotateVector(FVector(0,-252,1700)));
        const FQuat Rotation=P.Transform.GetRotation()*FRotator(0,Side*90-90,0).Quaternion();
        const double WidthScale=Side%2?FMath::Abs(Scale.Y):FMath::Abs(Scale.X);
        Face->SetRelativeTransform(FTransform(Rotation,Location,FVector(1,288*WidthScale/256,288*FMath::Abs(Scale.Z)/256)));
        Face->SetSlateWidget(SNew(SEWClockFace));Face->RegisterComponent();Out.Add(Face);
    }
}
TSharedRef<FJsonObject> EWClock::FaceEvidence(const UWidgetComponent* Face)
{
    auto O=StaticCastSharedPtr<SEWClockFace>(Face->GetSlateWidget())->Evidence();
    O->SetStringField(TEXT("position"),Face->GetComponentLocation().ToString());O->SetStringField(TEXT("rotation"),Face->GetComponentRotation().ToString());
    O->SetBoolField(TEXT("render_target"),Face->GetRenderTarget()!=nullptr);return O;
}
