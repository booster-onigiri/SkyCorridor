#include "EWHudLayer.h"
#include "EWGameInstance.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/SWindow.h"
#include "GenericPlatform/GenericWindow.h"
#include "Blueprint/WidgetLayoutLibrary.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include "Windows/HideWindowsPlatformTypes.h"

namespace
{
template<class T> void EWRelease(T*& P) { if (P) { P->Release(); P=nullptr; } }
LRESULT CALLBACK EWHudWindowProc(HWND Window, UINT Message, WPARAM W, LPARAM L)
{
    if (Message==WM_NCHITTEST) return HTTRANSPARENT;
    if (Message==WM_MOUSEACTIVATE) return MA_NOACTIVATE;
    if (Message==WM_ERASEBKGND) return 1;
    return DefWindowProcW(Window, Message, W, L);
}
}

struct FEWHudLayer::FState
{
    HWND Window=nullptr, Owner=nullptr;
    HDC DC=nullptr;
    HBITMAP Bitmap=nullptr;
    HGDIOBJ PreviousBitmap=nullptr;
    ID2D1Factory* Factory=nullptr;
    ID2D1DCRenderTarget* Target=nullptr;
    IDWriteFactory* TextFactory=nullptr;
    ID2D1SolidColorBrush* Brush=nullptr;
    int32 Width=0, Height=0;
    POINT Position{};
    bool Visible=false, Failed=false;
    double NextTextUpdate=0;
    FString LastContent;
    float LastScale=0;
    uint64 PaintCount=0;
    bool LayoutFailed=false;

    ~FState()
    {
        if (Window && IsWindow(Window)) DestroyWindow(Window);
        EWRelease(Brush); EWRelease(Target); EWRelease(Factory); EWRelease(TextFactory);
        ReleaseBitmap(); if (DC) DeleteDC(DC);
    }
    void ReleaseBitmap()
    {
        if (DC && PreviousBitmap) SelectObject(DC, PreviousBitmap);
        PreviousBitmap=nullptr;
        if (Bitmap) DeleteObject(Bitmap);
        Bitmap=nullptr; Width=Height=0;
    }
    bool Create(HWND Parent)
    {
        if (Failed) return false;
        if (Window && IsWindow(Window) && Owner==Parent) return true;
        if (Window && IsWindow(Window)) DestroyWindow(Window);
        Window=nullptr; Visible=false; LastContent.Empty(); Owner=Parent;
        static const wchar_t* ClassName=L"EndlessWorldPassiveHud";
        WNDCLASSEXW Class{}; Class.cbSize=sizeof(Class); Class.lpfnWndProc=EWHudWindowProc;
        Class.hInstance=GetModuleHandleW(nullptr); Class.lpszClassName=ClassName;
        if (!RegisterClassExW(&Class) && GetLastError()!=ERROR_CLASS_ALREADY_EXISTS) return Fail(TEXT("window class"));
        Window=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_NOACTIVATE|WS_EX_TOOLWINDOW,
            ClassName,L"空の回廊 — 案内表示",WS_POPUP,0,0,1,1,Owner,nullptr,Class.hInstance,nullptr);
        if (!Window) return Fail(TEXT("passive window"));
        if (!Factory && FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
            __uuidof(ID2D1Factory),nullptr,reinterpret_cast<void**>(&Factory)))) return Fail(TEXT("Direct2D"));
        if (!TextFactory && FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
            __uuidof(IDWriteFactory),reinterpret_cast<IUnknown**>(&TextFactory)))) return Fail(TEXT("DirectWrite"));
        if (!Target)
        {
            const auto Properties=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),96,96);
            if (FAILED(Factory->CreateDCRenderTarget(&Properties,&Target))) return Fail(TEXT("memory render target"));
            Target->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
            if (FAILED(Target->CreateSolidColorBrush(D2D1::ColorF(1,1,1,1),&Brush))) return Fail(TEXT("text brush"));
        }
        if (!DC) DC=CreateCompatibleDC(nullptr);
        if (!DC) return Fail(TEXT("memory surface"));
        UE_LOG(LogTemp,Display,TEXT("EW_DESKTOP_HUD created passive owner-bound window; no DXGI swapchain"));
        return true;
    }
    bool Fail(const TCHAR* Operation)
    {
        Failed=true;
        if (Window) ShowWindow(Window,SW_HIDE);
        Visible=false;
        UE_LOG(LogTemp,Error,TEXT("EW_DESKTOP_HUD failed: %s; retaining Slate fallback"),Operation);
        return false;
    }
    bool Resize(int32 W,int32 H)
    {
        if (Bitmap && W==Width && H==Height) return true;
        ReleaseBitmap();
        BITMAPINFO Info{}; Info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        Info.bmiHeader.biWidth=W; Info.bmiHeader.biHeight=-H;
        Info.bmiHeader.biPlanes=1; Info.bmiHeader.biBitCount=32; Info.bmiHeader.biCompression=BI_RGB;
        void* Bits=nullptr;
        Bitmap=CreateDIBSection(DC,&Info,DIB_RGB_COLORS,&Bits,nullptr,0);
        if (!Bitmap || !Bits) return Fail(TEXT("sized memory surface"));
        PreviousBitmap=SelectObject(DC,Bitmap); Width=W; Height=H;
        return true;
    }
    IDWriteTextLayout* Layout(const FString& Text,float Size,float MaximumWidth,float MaximumHeight)
    {
        IDWriteTextFormat* Format=nullptr; IDWriteTextLayout* Result=nullptr;
        HRESULT HR=TextFactory->CreateTextFormat(L"Yu Gothic UI",nullptr,DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL,DWRITE_FONT_STRETCH_NORMAL,Size,L"ja-jp",&Format);
        if (SUCCEEDED(HR)) HR=TextFactory->CreateTextLayout(*Text,Text.Len(),Format,MaximumWidth,MaximumHeight,&Result);
        EWRelease(Format);
        if (FAILED(HR)) { LayoutFailed=true; EWRelease(Result); return nullptr; }
        return Result;
    }
    void TextAt(IDWriteTextLayout* Layout,float X,float Y,const D2D1_COLOR_F& Color)
    {
        if (!Layout) return;
        Brush->SetColor(Color);
        Target->DrawTextLayout(D2D1::Point2F(X,Y),Layout,Brush,D2D1_DRAW_TEXT_OPTIONS_CLIP);
    }
    void Panel(float X,float Y,float W,float H,float Alpha)
    {
        Brush->SetColor(D2D1::ColorF(.17f,.25f,.28f,Alpha));
        Target->FillRectangle(D2D1::RectF(X,Y,X+W,Y+H),Brush);
    }
    bool Paint(const FString& Region,const FString& Nearby,const FString& Status,const FString& Diagnostics,float Scale)
    {
        LayoutFailed=false;
        RECT Bounds{0,0,Width,Height};
        if (FAILED(Target->BindDC(DC,&Bounds))) return Fail(TEXT("surface binding"));
        const float PaddingX=20*Scale, PaddingY=12*Scale, Margin=32*Scale;
        const auto Foreground=D2D1::ColorF(.986f,.982f,.95f,1);
        Target->BeginDraw(); Target->Clear(D2D1::ColorF(0,0,0,0));
        auto* Title=Layout(Region,29.33f*Scale,FMath::Max(100.f,Width*.45f),100*Scale);
        auto* Subtitle=Layout(TEXT("空の回廊  /  ENDLESS WORLD"),14.67f*Scale,FMath::Max(100.f,Width*.45f),80*Scale);
        DWRITE_TEXT_METRICS A{},B{};
        if (Title) Title->GetMetrics(&A); if (Subtitle) Subtitle->GetMetrics(&B);
        Panel(Margin,Margin,FMath::Max(A.widthIncludingTrailingWhitespace,B.widthIncludingTrailingWhitespace)+2*PaddingX,
            A.height+B.height+6*Scale+2*PaddingY,.60f);
        TextAt(Title,Margin+PaddingX,Margin+PaddingY,Foreground);
        TextAt(Subtitle,Margin+PaddingX,Margin+PaddingY+A.height+6*Scale,D2D1::ColorF(.88f,.915f,.91f,1));
        EWRelease(Title); EWRelease(Subtitle);
        auto DrawBox=[&](const FString& Text,float Size,float MaxWidth,int32 Anchor,float Alpha)
        {
            if (Text.IsEmpty()) return;
            auto* TextLayout=Layout(Text,Size*Scale,FMath::Min(MaxWidth*Scale,Width-2*Margin-2*PaddingX),Height*.5f);
            if (!TextLayout) return;
            DWRITE_TEXT_METRICS Metrics{}; TextLayout->GetMetrics(&Metrics);
            const float BoxWidth=Metrics.widthIncludingTrailingWhitespace+2*PaddingX, BoxHeight=Metrics.height+2*PaddingY;
            const float X=Anchor==2 ? Width-24*Scale-BoxWidth : (Width-BoxWidth)*.5f;
            const float Y=Anchor==0 ? Height-30*Scale-BoxHeight : 25*Scale;
            Panel(X,Y,BoxWidth,BoxHeight,Alpha); TextAt(TextLayout,X+PaddingX,Y+PaddingY,Foreground);
            EWRelease(TextLayout);
        };
        DrawBox(Nearby,20,1400,0,.72f);
        DrawBox(Status,21.33f,800,1,.90f);
        DrawBox(Diagnostics,16,620,2,.90f);
        if (FAILED(Target->EndDraw())) return Fail(TEXT("text rendering"));
        if (LayoutFailed) return Fail(TEXT("text layout"));
        POINT Source{0,0}; SIZE Size{Width,Height}; BLENDFUNCTION Blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
        if (!UpdateLayeredWindow(Window,nullptr,&Position,&Size,DC,&Source,0,&Blend,ULW_ALPHA)) return Fail(TEXT("desktop composition"));
        ++PaintCount;
        return true;
    }
};
#else
struct FEWHudLayer::FState {};
#endif

FEWHudLayer::FEWHudLayer() : State(MakeUnique<FState>()) {}
FEWHudLayer::~FEWHudLayer()=default;
void FEWHudLayer::Hide()
{
#if PLATFORM_WINDOWS
    if (!State) return;
    if (State->Window && State->Visible) ShowWindow(State->Window,SW_HIDE);
    State->Visible=false;
#endif
}
void FEWHudLayer::Shutdown() { State.Reset(); }
bool FEWHudLayer::IsVisible() const
{
#if PLATFORM_WINDOWS
    return State && State->Visible;
#else
    return false;
#endif
}
FString FEWHudLayer::Evidence() const
{
#if PLATFORM_WINDOWS
    if (!State) return TEXT("desktop HUD shut down");
    return FString::Printf(TEXT("desktop=%d failed=%d size=%dx%d paints=%llu"),State->Visible,State->Failed,State->Width,State->Height,State->PaintCount);
#else
    return TEXT("unsupported platform");
#endif
}
void FEWHudLayer::Update(UEWGameInstance& Game)
{
#if PLATFORM_WINDOWS
    if (!State) return;
    static const bool Enabled=!FParse::Param(FCommandLine::Get(),TEXT("EWDisableDesktopHUD"));
    if (!Enabled || GIsEditor || IsRunningCommandlet() || Game.Menu()!=EEWMenu::None || !Game.SessionStarted() ||
        !GEngine || !GEngine->GameViewport) { Hide(); return; }
    const auto GameWindow=GEngine->GameViewport->GetWindow();
    const auto Native=GameWindow ? GameWindow->GetNativeWindow() : nullptr;
    HWND Owner=Native ? static_cast<HWND>(Native->GetOSWindowHandle()) : nullptr;
    if (!Owner || !IsWindow(Owner) || IsIconic(Owner) || !IsWindowVisible(Owner) || GetForegroundWindow()!=Owner) { Hide(); return; }
    RECT Client{}; POINT Position{};
    if (!GetClientRect(Owner,&Client) || !ClientToScreen(Owner,&Position) || Client.right<320 || Client.bottom<240) { Hide(); return; }
    if (!State->Create(Owner)) return;
    const bool ChangedSize=State->Width!=Client.right || State->Height!=Client.bottom;
    const bool ChangedPosition=State->Position.x!=Position.x || State->Position.y!=Position.y;
    State->Position=Position;
    if (!State->Resize(Client.right,Client.bottom)) return;
    if (ChangedSize || ChangedPosition || !State->Visible)
        SetWindowPos(State->Window,HWND_TOP,Position.x,Position.y,Client.right,Client.bottom,SWP_NOACTIVATE|SWP_NOOWNERZORDER);
    const double Now=FPlatformTime::Seconds();
    const float Scale=FMath::Clamp(UWidgetLayoutLibrary::GetViewportScale(&Game),.5f,3.f);
    if (ChangedSize || !State->Visible || Now>=State->NextTextUpdate || Scale!=State->LastScale)
    {
        State->NextTextUpdate=Now+.10;
        const FString Region=Game.RegionText(), Nearby=Game.NearbyText(), Status=Game.StatusText();
        const FString Diagnostics=Game.bDiagnostics ? Game.DiagnosticsText() : FString();
        const FString Content=Region+TEXT("\n")+Nearby+TEXT("\n")+Status+TEXT("\n")+Diagnostics;
        if (ChangedSize || !State->Visible || Content!=State->LastContent || Scale!=State->LastScale)
        {
            if (!State->Paint(Region,Nearby,Status,Diagnostics,Scale)) return;
            State->LastContent=Content; State->LastScale=Scale;
        }
    }
    if (!State->Visible) ShowWindow(State->Window,SW_SHOWNOACTIVATE);
    State->Visible=true;
#endif
}
