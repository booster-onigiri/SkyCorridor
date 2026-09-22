#pragma once
#include "CoreMinimal.h"
#include "Styling/SlateBrush.h"
#include "Widgets/SLeafWidget.h"

// Owns one offscreen browser. Audio belongs only to this browser and is handed
// to a world AudioComponent; no loopback device or desktop capture is used.
class FEWBrowserSurface : public TSharedFromThis<FEWBrowserSurface>
{
public:
    FEWBrowserSurface();
    ~FEWBrowserSurface();
    bool Start();
    void Tick();
    void Shutdown();
    void Search(const FString& Query);
    void Stop();
    void Back();
    void Pause(bool Paused);
    void TestTone();
    void SetVideoFullscreen(bool Enabled);
    void EnableSound();
    bool VideoPaused() const;
    void SetPlaybackPaused(bool Paused);
    bool IsVideoFullscreen() const;
    void Mouse(int32 X,int32 Y,int32 Action,int32 Button=0,int32 Wheel=0);
    void Key(int32 Code,bool Up,bool Character,int32 Modifiers);
    void DrainAudio(TArray<int16>& Out);
    void DrainStereoAudio(TArray<int16>& Out);
    void TestStereoTone(int32 Channel);
    void ApplyPlayback(const FString& VideoId,double Seconds,bool Paused);
    void TestSyncFilm();
    void SeekForAudit(double Seconds,bool Paused);
    FString Status() const;
    TSharedRef<class FJsonObject> Evidence() const;
    const FSlateBrush* Brush() const {return &Image;}
    bool HasPage() const;
    bool HasAudio() const;
    static constexpr int32 Width=1280,Height=720,SampleRate=48000;
private:
    struct FImpl;
    TUniquePtr<FImpl> Impl;
    FSlateBrush Image;
};

class SEWBrowserView : public SLeafWidget
{
public:
    SLATE_BEGIN_ARGS(SEWBrowserView){} SLATE_ARGUMENT(TSharedPtr<FEWBrowserSurface>,Browser) SLATE_END_ARGS()
    void Construct(const FArguments& Args){Browser=Args._Browser;}
    virtual bool SupportsKeyboardFocus() const override{return true;}
    virtual FVector2D ComputeDesiredSize(float) const override{return FVector2D(960,540);}
    virtual int32 OnPaint(const FPaintArgs&,const FGeometry&,const FSlateRect&,FSlateWindowElementList&,int32,const FWidgetStyle&,bool) const override;
    virtual FReply OnMouseButtonDown(const FGeometry&,const FPointerEvent&) override;
    virtual FReply OnMouseButtonUp(const FGeometry&,const FPointerEvent&) override;
    virtual FReply OnMouseMove(const FGeometry&,const FPointerEvent&) override;
    virtual FReply OnMouseWheel(const FGeometry&,const FPointerEvent&) override;
    virtual FReply OnKeyDown(const FGeometry&,const FKeyEvent&) override;
    virtual FReply OnKeyUp(const FGeometry&,const FKeyEvent&) override;
    virtual FReply OnKeyChar(const FGeometry&,const FCharacterEvent&) override;
private:
    TSharedPtr<FEWBrowserSurface> Browser;
    FReply Pointer(const FGeometry&,const FPointerEvent&,int32 Action);
};
