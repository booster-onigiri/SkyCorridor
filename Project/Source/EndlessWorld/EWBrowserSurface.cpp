#include "EWBrowserSurface.h"
#include "EWLocalization.h"
#include "EWYouTubeQuality.h"
#include "EWShutdownTrace.h"
#include "Misc/FileHelper.h"
#include "Misc/Base64.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Engine/Texture2D.h"
#include "UObject/StrongObjectPtr.h"
#include "Rendering/DrawElements.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "HAL/FileManager.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "RHI.h"
#include "CEF3Utils.h"
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#include "include/cef_app.h"
#include "include/cef_client.h"
#include "include/cef_parser.h"
#include "include/cef_audio_handler.h"
THIRD_PARTY_INCLUDES_END
#include "Windows/HideWindowsPlatformTypes.h"

namespace
{
struct FBrowserPackets
{
    FCriticalSection Mutex;
    TArray<uint8> Pixels;
    TArray<int16> Audio;
    FString Message=TEXT("キーワードかYouTubeのURLを入力してください。");
    uint64 Paints=0,Packets=0,Samples=0,DroppedSamples=0;
    float Peak=0;
    float RecentPeak=0;
    double LastAudioAt=0,VideoVolume=1,VideoTime=0,VideoStateAt=0;
    bool Video=false,VideoMuted=false,VideoPaused=true,Fullscreen=false,FullscreenFillsView=false;
    double PlayerWidth=0,PlayerHeight=0;
    FString FullscreenNonce,SoundNonce,VideoId,QualityStatus,QualityLabel,QualityOptions;
    double QualityHeight=0,VideoWidth=0,VideoHeight=0,QualityChanges=0;
    double LastFullscreenRequest=0;
    bool VideoAd=false;
    bool Page=false,AudioActive=false;
    TAtomic<bool> Closed{false};
};
using PacketsPtr=TSharedPtr<FBrowserPackets,ESPMode::ThreadSafe>;
bool AllowedAddress(const FString& URL)
{
    if(URL==TEXT("about:blank"))return true;
    CefURLParts Parts;if(!CefParseURL(TCHAR_TO_WCHAR(*URL),Parts))return false;
    const FString Scheme(CefString(&Parts.scheme).ToWString().c_str());
    const FString Host(CefString(&Parts.host).ToWString().c_str());
    return Scheme==TEXT("https") && (Host==TEXT("youtube.com") || Host.EndsWith(TEXT(".youtube.com")) ||
        Host==TEXT("youtu.be") || Host==TEXT("youtube-nocookie.com") || Host.EndsWith(TEXT(".youtube-nocookie.com")));
}
class FMonitorApp final:public CefApp
{
public:
    void OnBeforeCommandLineProcessing(const CefString&,CefRefPtr<CefCommandLine> Line) override
    {
        Line->AppendSwitch("disable-background-networking");
        Line->AppendSwitch("disable-component-update");
        Line->AppendSwitch("disable-extensions");
        Line->AppendSwitch("disable-sync");
        Line->AppendSwitchWithValue("autoplay-policy","no-user-gesture-required");
        Line->AppendSwitch("disable-gpu-shader-disk-cache");
    }
    IMPLEMENT_REFCOUNTING(FMonitorApp);
};
class FMonitorClient final:public CefClient,public CefRenderHandler,public CefAudioHandler,
    public CefLifeSpanHandler,public CefLoadHandler,public CefRequestHandler,public CefDisplayHandler
{
public:
    explicit FMonitorClient(PacketsPtr In):Data(In){}
    CefRefPtr<CefRenderHandler> GetRenderHandler() override{return this;}
    CefRefPtr<CefAudioHandler> GetAudioHandler() override{return this;}
    CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override{return this;}
    CefRefPtr<CefLoadHandler> GetLoadHandler() override{return this;}
    CefRefPtr<CefRequestHandler> GetRequestHandler() override{return this;}
    CefRefPtr<CefDisplayHandler> GetDisplayHandler() override{return this;}
    void GetViewRect(CefRefPtr<CefBrowser>,CefRect& Rect) override{Rect=CefRect(0,0,1280,720);}
    bool GetScreenInfo(CefRefPtr<CefBrowser>,CefScreenInfo& Info) override
    {Info.device_scale_factor=1.f;Info.rect=CefRect(0,0,1280,720);Info.available_rect=Info.rect;return true;}
    void OnPaint(CefRefPtr<CefBrowser>,PaintElementType Type,const RectList&,const void* Buffer,int W,int H) override
    {
        if(Type!=PET_VIEW || W!=1280 || H!=720)return;
        FScopeLock Lock(&Data->Mutex);Data->Pixels.SetNumUninitialized(W*H*4);
        FMemory::Memcpy(Data->Pixels.GetData(),Buffer,W*H*4);++Data->Paints;
    }
    bool GetAudioParameters(CefRefPtr<CefBrowser>,CefAudioParameters& P) override
    {P.channel_layout=CEF_CHANNEL_LAYOUT_STEREO;P.sample_rate=48000;P.frames_per_buffer=480;return true;}
    void OnAudioStreamStarted(CefRefPtr<CefBrowser>,const CefAudioParameters& P,int Channels) override
    {FScopeLock Lock(&Data->Mutex);AudioChannels=Channels;Data->AudioActive=true;Data->Audio.Reset();Data->Message=P.sample_rate==48000?TEXT("モニターから立体音響で再生中"):TEXT("音声の形式を確認しています。");}
    void OnAudioStreamPacket(CefRefPtr<CefBrowser>,const float** PCM,int Frames,int64_t) override
    {
        if(Frames<=0 || Frames>48000 || AudioChannels<=0)return;
        FScopeLock Lock(&Data->Mutex);
        if(!Data->Page)return;
        Data->AudioActive=true;Data->LastAudioAt=FPlatformTime::Seconds();Data->RecentPeak=0;
        // Bound pending audio to 200ms. A stalled render/game thread must never
        // turn into seconds of stale sound after resuming the application.
        if(Data->Audio.Num()+Frames*2>19200){Data->DroppedSamples+=Data->Audio.Num()/2;Data->Audio.Reset();}
        const int32 Offset=Data->Audio.AddUninitialized(Frames*2);
        // Preserve the browser's stereo pair; the cinema locates each channel
        // at its corresponding screen speaker. Mono pages duplicate equally.
        for(int32 I=0;I<Frames;++I)for(int32 C=0;C<2;++C)
        {
            const float V=PCM[FMath::Min(C,AudioChannels-1)][I];
            Data->Peak=FMath::Max(Data->Peak,FMath::Abs(V));Data->RecentPeak=FMath::Max(Data->RecentPeak,FMath::Abs(V));
            Data->Audio[Offset+I*2+C]=int16(FMath::Clamp(V,-1.f,1.f)*32767.f);
        }
        ++Data->Packets;Data->Samples+=Frames;
    }
    void OnAudioStreamStopped(CefRefPtr<CefBrowser>) override{FScopeLock Lock(&Data->Mutex);Data->AudioActive=false;Data->Audio.Reset();if(Data->Page)Data->Message=TEXT("音声は停止中です。プレーヤーから再生できます。");}
    void OnAudioStreamError(CefRefPtr<CefBrowser>,const CefString&) override
    {FScopeLock Lock(&Data->Mutex);Data->AudioActive=false;Data->Audio.Reset();Data->Message=TEXT("音声を再生できませんでした。動画を開き直してください。");}
    void OnBeforeClose(CefRefPtr<CefBrowser>) override{FScopeLock Lock(&Data->Mutex);Data->Closed=true;}
    bool OnBeforePopup(CefRefPtr<CefBrowser>,CefRefPtr<CefFrame>,const CefString&,
        const CefString&,CefLifeSpanHandler::WindowOpenDisposition,bool,const CefPopupFeatures&,CefWindowInfo&,
        CefRefPtr<CefClient>&,CefBrowserSettings&,CefRefPtr<CefDictionaryValue>&,bool*) override{return true;}
    bool OnBeforeBrowse(CefRefPtr<CefBrowser>,CefRefPtr<CefFrame> Frame,CefRefPtr<CefRequest> Request,bool,bool) override
    {
        if(!Frame->IsMain())return false;
        const FString URL(Request->GetURL().ToWString().c_str());
        if(URL.StartsWith(TEXT("data:text/html")) && bTestPage)return false;
        if(AllowedAddress(URL))return false;
        FScopeLock Lock(&Data->Mutex);Data->Message=TEXT("このモニターではYouTubeのページを開けます。");return true;
    }
    void OnLoadingStateChange(CefRefPtr<CefBrowser>,bool Loading,bool,bool) override
    {FScopeLock Lock(&Data->Mutex);if(!Data->Page)return;if(Loading)Data->Message=TEXT("YouTubeを読み込んでいます…");
     else Data->Message=TEXT("動画を選び、プレーヤーの再生ボタンを押してください。");}
    void OnLoadError(CefRefPtr<CefBrowser>,CefRefPtr<CefFrame> Frame,ErrorCode Code,const CefString&,const CefString&) override
    {if(!Frame->IsMain() || Code==ERR_ABORTED)return;FScopeLock Lock(&Data->Mutex);Data->Message=TEXT("ページを読み込めませんでした。通信状態を確認して検索し直してください。");}
    void OnFullscreenModeChange(CefRefPtr<CefBrowser>,bool Fullscreen)override
    {FScopeLock Lock(&Data->Mutex);Data->Fullscreen=Fullscreen;}
    bool OnConsoleMessage(CefRefPtr<CefBrowser> Browser,cef_log_severity_t,const CefString& Message,const CefString&,int) override
    {
        const FString Text(Message.ToWString().c_str());
        const bool State=Text.StartsWith(TEXT("__EW_MEDIA_STATE__")),Button=Text.StartsWith(TEXT("__EW_FULLSCREEN_BUTTON__"));
        if(!State && !Button)return true;
        TSharedPtr<FJsonObject> O;
        if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text.Mid(State?18:24)),O) || !O)return true;
        if(State)
        {
            FScopeLock Lock(&Data->Mutex);if(!Data->Page)return true;
            Data->Video=O->GetBoolField(TEXT("video"));Data->VideoMuted=O->GetBoolField(TEXT("muted"));
            Data->VideoPaused=O->GetBoolField(TEXT("paused"));Data->VideoVolume=O->GetNumberField(TEXT("volume"));
            Data->VideoTime=O->GetNumberField(TEXT("time"));Data->VideoStateAt=FPlatformTime::Seconds();Data->Fullscreen=O->GetBoolField(TEXT("fullscreen"));
            O->TryGetBoolField(TEXT("fillsView"),Data->FullscreenFillsView);
            O->TryGetNumberField(TEXT("playerWidth"),Data->PlayerWidth);O->TryGetNumberField(TEXT("playerHeight"),Data->PlayerHeight);
            O->TryGetStringField(TEXT("id"),Data->VideoId);O->TryGetBoolField(TEXT("ad"),Data->VideoAd);
            O->TryGetStringField(TEXT("qualityStatus"),Data->QualityStatus);O->TryGetStringField(TEXT("qualityLabel"),Data->QualityLabel);O->TryGetStringField(TEXT("qualityOptions"),Data->QualityOptions);
            O->TryGetNumberField(TEXT("qualityHeight"),Data->QualityHeight);O->TryGetNumberField(TEXT("videoWidth"),Data->VideoWidth);O->TryGetNumberField(TEXT("videoHeight"),Data->VideoHeight);O->TryGetNumberField(TEXT("qualityChanges"),Data->QualityChanges);
        }
        else
        {
            {FScopeLock Lock(&Data->Mutex);const FString Token=O->GetStringField(TEXT("token"));
                if(!Data->FullscreenNonce.IsEmpty() && Data->FullscreenNonce==Token)Data->FullscreenNonce.Reset();
                else if(!Data->SoundNonce.IsEmpty() && Data->SoundNonce==Token)Data->SoundNonce.Reset();else return true;}
            const double X=O->GetNumberField(TEXT("x")),Y=O->GetNumberField(TEXT("y"));
            if(X<0 || X>=1280 || Y<0 || Y>=720)return true;
            CefMouseEvent E;E.x=int(X);E.y=int(Y);
            Browser->GetHost()->SetFocus(true);Browser->GetHost()->SendMouseMoveEvent(E,false);
            Browser->GetHost()->SendMouseClickEvent(E,MBT_LEFT,false,1);Browser->GetHost()->SendMouseClickEvent(E,MBT_LEFT,true,1);
        }
        return true;
    }
    bool bTestPage=false;
private:
    PacketsPtr Data;
    int AudioChannels=1;
    IMPLEMENT_REFCOUNTING(FMonitorClient);
};
// CEF is process-wide. Each panel owns a browser, texture and PCM queue, while
// the application/runtime is initialized once and released by the last owner.
bool RuntimeInitialized=false,RuntimeCloseTimedOut=false;
int32 RuntimeUsers=0;
uint64 LastPumpFrame=MAX_uint64;
CefRefPtr<FMonitorApp> RuntimeApp;
}

struct FEWBrowserSurface::FImpl
{
    PacketsPtr Data=MakeShared<FBrowserPackets,ESPMode::ThreadSafe>();
    CefRefPtr<CefBrowser> Browser;
    CefRefPtr<FMonitorClient> Client;
    CefRefPtr<FMonitorApp> App;
    TStrongObjectPtr<UTexture2D> Texture;
    bool Initialized=false,Attempted=false,Paused=false;
    double LastVideoPoll=0;
    FString SyncedVideoId;
    bool PendingFullscreen=false,FullscreenGoal=false;
};
FEWBrowserSurface::FEWBrowserSurface():Impl(MakeUnique<FImpl>())
{
    Image.DrawAs=ESlateBrushDrawType::Image;Image.ImageSize=FVector2D(Width,Height);
}
FEWBrowserSurface::~FEWBrowserSurface(){Shutdown();}
bool FEWBrowserSurface::Start()
{
    if(Impl->Browser)return true;if(Impl->Attempted)return false;Impl->Attempted=true;
    if(GIsEditor || GUsingNullRHI || !CEF3Utils::LoadCEF3Modules(true))
    {FScopeLock Lock(&Impl->Data->Mutex);Impl->Data->Message=TEXT("動画はWindowsの探索用実行版で利用できます。");return false;}
    if(!RuntimeInitialized)
    {
    const FString Runtime=CEF3Utils::GetCEF3ModulePath();
    const FString Helper=FPaths::ConvertRelativePathToFull(FPaths::EngineDir()/TEXT("Binaries/Win64/EpicWebHelper.exe"));
    if(!FPaths::FileExists(Helper)){FScopeLock Lock(&Impl->Data->Mutex);Impl->Data->Message=TEXT("動画用の実行ファイルが見つかりません。");return false;}
    CefSettings Settings;Settings.no_sandbox=true;Settings.command_line_args_disabled=true;
    Settings.windowless_rendering_enabled=true;Settings.external_message_pump=false;
    Settings.multi_threaded_message_loop=false;Settings.background_color=CefColorSetARGB(255,10,20,25);
    CefString(&Settings.browser_subprocess_path)=TCHAR_TO_WCHAR(*Helper);
    CefString(&Settings.resources_dir_path)=TCHAR_TO_WCHAR(*Runtime);
    CefString(&Settings.locales_dir_path)=TCHAR_TO_WCHAR(*(Runtime/TEXT("Resources/locales")));
    CefString(&Settings.locale)=EWL::IsEnglish()?"en-US":"ja";
    CefString(&Settings.accept_language_list)=EWL::IsEnglish()?"en-US,en":"ja,en-US,en";
    // Dedicated per-run profile; it never reads the user's Chrome/Edge profile.
    const FString Cache=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("MonitorBrowser")/FGuid::NewGuid().ToString(EGuidFormats::Digits));
    IFileManager::Get().MakeDirectory(*Cache,true);
    CefString(&Settings.root_cache_path)=TCHAR_TO_WCHAR(*Cache);
    CefString(&Settings.log_file)=TCHAR_TO_WCHAR(*(Cache/TEXT("browser.log")));Settings.log_severity=LOGSEVERITY_WARNING;
    RuntimeApp=new FMonitorApp();
    RuntimeInitialized=CefInitialize(CefMainArgs(GetModuleHandle(nullptr)),Settings,RuntimeApp,nullptr);
    if(!RuntimeInitialized){RuntimeApp=nullptr;FScopeLock Lock(&Impl->Data->Mutex);Impl->Data->Message=TEXT("動画用ブラウザーを起動できませんでした。");return false;}
    }
    Impl->Initialized=true;++RuntimeUsers;
    Impl->Client=new FMonitorClient(Impl->Data);
    CefWindowInfo Window;Window.SetAsWindowless(nullptr);Window.shared_texture_enabled=false;
    CefBrowserSettings BrowserSettings;BrowserSettings.windowless_frame_rate=30;
    Impl->Browser=CefBrowserHost::CreateBrowserSync(Window,Impl->Client,"about:blank",BrowserSettings,nullptr,nullptr);
    if(!Impl->Browser){Impl->Data->Closed=true;Shutdown();return false;}
    Impl->Texture.Reset(UTexture2D::CreateTransient(Width,Height,PF_B8G8R8A8));
    Impl->Texture->SRGB=true;Impl->Texture->NeverStream=true;Impl->Texture->UpdateResource();
    Image.SetResourceObject(Impl->Texture.Get());return true;
}
void FEWBrowserSurface::Tick()
{
    if(!Impl->Initialized)return;if(LastPumpFrame!=GFrameCounter){CefDoMessageLoopWork();LastPumpFrame=GFrameCounter;}
    if(Impl->PendingFullscreen && HasPage())
    {
        bool Complete=false;{FScopeLock Lock(&Impl->Data->Mutex);Complete=Impl->FullscreenGoal?
            Impl->Data->Fullscreen && Impl->Data->FullscreenFillsView:!Impl->Data->Fullscreen;}
        if(Complete)Impl->PendingFullscreen=false;else SetVideoFullscreen(Impl->FullscreenGoal);
    }
    if(Impl->Browser && HasPage() && FPlatformTime::Seconds()-Impl->LastVideoPoll>.5)
    {
        Impl->LastVideoPoll=FPlatformTime::Seconds();
        Impl->Browser->GetMainFrame()->ExecuteJavaScript(TCHAR_TO_WCHAR(EWYouTubeQuality::Script()),"about:blank",1);
        Impl->Browser->GetMainFrame()->ExecuteJavaScript(
            "(()=>{const v=document.querySelector('video');const p=document.getElementById('movie_player');const r=p?p.getBoundingClientRect():null;const d=p&&p.getVideoData?p.getVideoData():{};const id=window.__EW_SYNC_FIXTURE?'EWTEST00001':(d.video_id||new URL(location.href).searchParams.get('v')||'');const q=window.__EW_QUALITY83||{};console.info('__EW_MEDIA_STATE__'+JSON.stringify({qualityStatus:q.phase||'',qualityLabel:q.label||'',qualityOptions:(q.available||[]).join('|'),qualityHeight:q.height||0,qualityChanges:q.changes||0,videoWidth:v?v.videoWidth:0,videoHeight:v?v.videoHeight:0,id,ad:!!document.querySelector('.ad-showing'),video:!!v,muted:v?!!v.muted:false,paused:v?!!v.paused:true,volume:v?v.volume:0,time:v&&Number.isFinite(v.currentTime)?v.currentTime:0,fullscreen:!!document.fullscreenElement,fillsView:!!r&&r.width>=innerWidth-4&&r.height>=innerHeight-4,playerWidth:r?r.width:0,playerHeight:r?r.height:0}));})();","about:blank",1);
    }
    TArray<uint8> Pixels;
    {FScopeLock Lock(&Impl->Data->Mutex);Swap(Pixels,Impl->Data->Pixels);}
    if(Pixels.Num() && Impl->Texture.IsValid())
    {
        auto* Data=static_cast<uint8*>(FMemory::Malloc(Pixels.Num()));FMemory::Memcpy(Data,Pixels.GetData(),Pixels.Num());
        auto* Region=new FUpdateTextureRegion2D(0,0,0,0,Width,Height);
        Impl->Texture->UpdateTextureRegions(0,1,Region,Width*4,4,Data,[](uint8* P,const FUpdateTextureRegion2D* R){FMemory::Free(P);delete R;});
    }
}
void FEWBrowserSurface::Shutdown()
{
    if(!Impl || !Impl->Initialized)return;
    EWShutdownTrace(TEXT("Browser.Close.begin"));
    if(Impl->Browser)Impl->Browser->GetHost()->CloseBrowser(true);
    const double Deadline=FPlatformTime::Seconds()+3.;
    while(!Impl->Data->Closed && FPlatformTime::Seconds()<Deadline){CefDoMessageLoopWork();FPlatformProcess::Sleep(.005f);}
    EWShutdownTrace(Impl->Data->Closed?TEXT("Browser.Close.closed"):TEXT("Browser.Close.timeout"));
    Impl->Browser=nullptr;Impl->Client=nullptr;Impl->App=nullptr;
    RuntimeCloseTimedOut|=!Impl->Data->Closed;--RuntimeUsers;
    if(RuntimeUsers==0 && !RuntimeCloseTimedOut){EWShutdownTrace(TEXT("CefShutdown.begin"));CefShutdown();EWShutdownTrace(TEXT("CefShutdown.end"));RuntimeInitialized=false;RuntimeApp=nullptr;}
    Impl->Initialized=false;Image.SetResourceObject(nullptr);Impl->Texture.Reset();
}
void FEWBrowserSurface::Search(const FString& Input)
{
    const FString Query=Input.TrimStartAndEnd().Left(2048);if(Query.IsEmpty())return;
    // URL parsing also uses delay-loaded CEF exports. Load the runtime before
    // validation so opening a URL as the first action cannot fault on libcef.
    if(!Start())return;
    FString URL;
    if(Query.StartsWith(TEXT("https://")) || Query.StartsWith(TEXT("http://")))
    {if(!AllowedAddress(Query)){FScopeLock Lock(&Impl->Data->Mutex);Impl->Data->Message=TEXT("YouTubeのURLを入力してください。");return;}URL=Query;}
    else URL=FString(TEXT("https://www.youtube.com/results?hl="))+EWL::Pick(TEXT("ja"),TEXT("en"))+TEXT("&search_query=")+FGenericPlatformHttp::UrlEncode(Query);
    Impl->Client->bTestPage=false;
    {FScopeLock Lock(&Impl->Data->Mutex);Impl->Data->Page=true;}
    Impl->Browser->GetMainFrame()->LoadURL(TCHAR_TO_WCHAR(*URL));
}
void FEWBrowserSurface::Stop()
{Impl->PendingFullscreen=false;if(Impl->Browser)Impl->Browser->GetMainFrame()->LoadURL("about:blank");FScopeLock Lock(&Impl->Data->Mutex);Impl->Data->Audio.Reset();Impl->Data->AudioActive=false;Impl->Data->Page=false;Impl->Data->Video=false;Impl->Data->Fullscreen=false;Impl->Data->FullscreenFillsView=false;Impl->Data->Message=TEXT("停止しました。検索またはURLの入力で再生できます。");}
void FEWBrowserSurface::Back(){if(Impl->Browser && Impl->Browser->CanGoBack())Impl->Browser->GoBack();}
void FEWBrowserSurface::SetVideoFullscreen(bool Enabled)
{
    if(!Impl->Browser || !HasPage())return;
    Impl->PendingFullscreen=true;Impl->FullscreenGoal=Enabled;
    const FString Token=FGuid::NewGuid().ToString(EGuidFormats::Digits);
    {FScopeLock Lock(&Impl->Data->Mutex);const double Now=FPlatformTime::Seconds();if(Now-Impl->Data->LastFullscreenRequest<1.)return;Impl->Data->LastFullscreenRequest=Now;Impl->Data->FullscreenNonce=Token;}
    // Ask the page for the actual player's control bounds, then send a native
    // browser click. This retains YouTube's full player, captions and controls.
    const FString Script=FString::Printf(TEXT("(()=>{const wanted=%s;if(document.fullscreenElement){const p=document.getElementById('movie_player');const r=p?p.getBoundingClientRect():null;if(!wanted||!r||r.width<innerWidth-4||r.height<innerHeight-4)document.exitFullscreen().catch(()=>{});return;}if(!wanted)return;const b=document.querySelector('.ytp-fullscreen-button');if(!b)return;const r=b.getBoundingClientRect();if(r.width&&r.height)console.info('__EW_FULLSCREEN_BUTTON__'+JSON.stringify({token:'%s',x:r.x+r.width/2,y:r.y+r.height/2}));})();"),Enabled?TEXT("true"):TEXT("false"),*Token);
    Impl->Browser->GetMainFrame()->ExecuteJavaScript(TCHAR_TO_WCHAR(*Script),"about:blank",1);
}
void FEWBrowserSurface::EnableSound()
{
    if(!Impl->Browser)return;
    Impl->Browser->GetHost()->SetAudioMuted(false);
    const FString Token=FGuid::NewGuid().ToString(EGuidFormats::Digits);
    {FScopeLock Lock(&Impl->Data->Mutex);Impl->Data->SoundNonce=Token;}
    // An Unreal button is not a Chromium user gesture. Resume by clicking the
    // real player control through CEF, including on the very first playback.
    const FString Script=FString::Printf(TEXT("(()=>{const p=document.getElementById('movie_player');if(p&&p.unMute)p.unMute();const v=document.querySelector('video');if(!v)return;v.muted=false;if(v.volume===0)v.volume=1;if(!v.paused)return;const b=document.querySelector('.ytp-play-button');if(!b)return;const r=b.getBoundingClientRect();if(r.width&&r.height)console.info('__EW_FULLSCREEN_BUTTON__'+JSON.stringify({token:'%s',x:r.x+r.width/2,y:r.y+r.height/2}));})();"),*Token);
    Impl->Browser->GetMainFrame()->ExecuteJavaScript(TCHAR_TO_WCHAR(*Script),"about:blank",1);
}
bool FEWBrowserSurface::IsVideoFullscreen()const{FScopeLock Lock(&Impl->Data->Mutex);return Impl->Data->Fullscreen && Impl->Data->FullscreenFillsView;}
bool FEWBrowserSurface::VideoPaused()const{FScopeLock Lock(&Impl->Data->Mutex);return !Impl->Data->Video || Impl->Data->VideoPaused;}
void FEWBrowserSurface::SetPlaybackPaused(bool Paused)
{
    if(!Impl->Browser)return;
    if(Paused)Impl->Browser->GetMainFrame()->ExecuteJavaScript("document.querySelectorAll('video,audio').forEach(v=>v.pause());","about:blank",1);
    else {EnableSound();Impl->Browser->GetMainFrame()->ExecuteJavaScript("document.querySelectorAll('video,audio').forEach(v=>v.play().catch(()=>{}));","about:blank",1);}
}
void FEWBrowserSurface::Pause(bool Paused)
{
    if(Impl->Paused==Paused)return;Impl->Paused=Paused;
    if(Paused && Impl->Browser)Impl->Browser->GetMainFrame()->ExecuteJavaScript("document.querySelectorAll('video,audio').forEach(v=>v.pause());","about:blank",1);
}
void FEWBrowserSurface::TestTone()
{
    if(!Start())return;Impl->Client->bTestPage=true;
    {FScopeLock Lock(&Impl->Data->Mutex);Impl->Data->Page=true;}
    const FString Html=TEXT("<!doctype html><meta charset=utf-8><body style='background:#123;color:#def;font:48px sans-serif'><h1>立体音響の検証</h1><button style='font:40px sans-serif;position:absolute;left:40px;top:190px;width:480px;height:100px' onclick=\"const a=new AudioContext();const o=a.createOscillator();const g=a.createGain();g.gain.value=.15;o.frequency.value=440;o.connect(g);g.connect(a.destination);o.start();window.tone=o;\">音を鳴らす</button></body>");
    const std::string Encoded=CefURIEncode(TCHAR_TO_UTF8(*Html),false).ToString();
    Impl->Browser->GetMainFrame()->LoadURL("data:text/html;charset=utf-8,"+Encoded);
}
void FEWBrowserSurface::Mouse(int32 X,int32 Y,int32 Action,int32 Button,int32 Wheel)
{
    if(!Impl->Browser)return;CefMouseEvent E;E.x=X;E.y=Y;
    if(Action==0)Impl->Browser->GetHost()->SendMouseMoveEvent(E,false);
    else if(Action==3)Impl->Browser->GetHost()->SendMouseWheelEvent(E,0,Wheel);
    else Impl->Browser->GetHost()->SendMouseClickEvent(E,Button==1?MBT_RIGHT:MBT_LEFT,Action==2,1);
}
void FEWBrowserSurface::Key(int32 Code,bool Up,bool Character,int32 Modifiers)
{if(!Impl->Browser)return;CefKeyEvent E;E.type=Character?KEYEVENT_CHAR:Up?KEYEVENT_KEYUP:KEYEVENT_RAWKEYDOWN;E.windows_key_code=Code;E.native_key_code=Code;E.modifiers=Modifiers;Impl->Browser->GetHost()->SendKeyEvent(E);}
void FEWBrowserSurface::DrainStereoAudio(TArray<int16>& Out){FScopeLock Lock(&Impl->Data->Mutex);Out.Reset();Swap(Out,Impl->Data->Audio);}
void FEWBrowserSurface::DrainAudio(TArray<int16>& Out)
{
    TArray<int16> Stereo;DrainStereoAudio(Stereo);Out.SetNumUninitialized(Stereo.Num()/2);
    for(int32 I=0;I<Out.Num();++I)Out[I]=int16((int32(Stereo[I*2])+int32(Stereo[I*2+1]))/2);
}
void FEWBrowserSurface::TestStereoTone(int32 Channel)
{
    if(!Start())return;Impl->Client->bTestPage=true;
    {FScopeLock Lock(&Impl->Data->Mutex);Impl->Data->Page=true;}
    const FString Html=FString::Printf(TEXT("<!doctype html><meta charset=utf-8><body style='background:#11252b;color:#ddd;font:48px sans-serif'><h1>WATERLIGHT CINEMA</h1><p>SCREEN AUDIO %s</p><button style='position:absolute;left:40px;top:190px;width:480px;height:100px;font:36px sans-serif' onclick=\"const a=new AudioContext({sampleRate:48000});const m=a.createChannelMerger(2);const o=a.createOscillator();const g=a.createGain();g.gain.value=.12;o.frequency.value=440;o.connect(g);g.connect(m,0,%d);m.connect(a.destination);o.start();window.testContext=a;\">音声テスト</button></body>"),Channel==0?TEXT("LEFT"):TEXT("RIGHT"),FMath::Clamp(Channel,0,1));
    const std::string Encoded=CefURIEncode(TCHAR_TO_UTF8(*Html),false).ToString();
    Impl->Browser->GetMainFrame()->LoadURL("data:text/html;charset=utf-8,"+Encoded);
}
FString FEWBrowserSurface::Status()const
{
    FScopeLock Lock(&Impl->Data->Mutex);
    if(Impl->Data->Page && Impl->Data->Video)
    {
        if(Impl->Data->VideoMuted || Impl->Data->VideoVolume<=0)return EWL::Translate(TEXT("動画が消音になっています。「音声を有効にする」で解除できます。"));
        if(Impl->Data->VideoPaused)return EWL::Translate(TEXT("動画は一時停止中です。"));
        if(Impl->Data->AudioActive && FPlatformTime::Seconds()-Impl->Data->LastAudioAt<1)return EWL::Translate(TEXT("動画と音声を再生中です。音は部屋のモニターから聞こえます。"));
        return EWL::Translate(TEXT("映像を再生中です。音が聞こえない場合は「音声を有効にする」を押してください。"));
    }
    return EWL::Translate(Impl->Data->Message);
}
bool FEWBrowserSurface::HasPage()const{FScopeLock Lock(&Impl->Data->Mutex);return Impl->Data->Page;}
bool FEWBrowserSurface::HasAudio()const{FScopeLock Lock(&Impl->Data->Mutex);return Impl->Data->Page && Impl->Data->AudioActive;}
TSharedRef<FJsonObject> FEWBrowserSurface::Evidence()const
{auto O=MakeShared<FJsonObject>();FScopeLock Lock(&Impl->Data->Mutex);O->SetBoolField(TEXT("initialized"),Impl->Initialized);O->SetBoolField(TEXT("page"),Impl->Data->Page);
 O->SetNumberField(TEXT("paint_frames"),double(Impl->Data->Paints));O->SetNumberField(TEXT("audio_packets"),double(Impl->Data->Packets));O->SetNumberField(TEXT("audio_samples"),double(Impl->Data->Samples));
 O->SetNumberField(TEXT("audio_peak"),Impl->Data->Peak);O->SetNumberField(TEXT("audio_recent_peak"),Impl->Data->RecentPeak);
 O->SetStringField(TEXT("quality_status"),Impl->Data->QualityStatus);O->SetStringField(TEXT("quality_label"),Impl->Data->QualityLabel);O->SetStringField(TEXT("quality_options"),Impl->Data->QualityOptions);
 O->SetNumberField(TEXT("quality_height"),Impl->Data->QualityHeight);O->SetNumberField(TEXT("quality_changes"),Impl->Data->QualityChanges);O->SetNumberField(TEXT("video_width"),Impl->Data->VideoWidth);O->SetNumberField(TEXT("video_height"),Impl->Data->VideoHeight);
 O->SetStringField(TEXT("video_id"),Impl->Data->VideoId);O->SetBoolField(TEXT("video_ad"),Impl->Data->VideoAd);
 O->SetNumberField(TEXT("audio_packet_age"),FPlatformTime::Seconds()-Impl->Data->LastAudioAt);
 O->SetBoolField(TEXT("video"),Impl->Data->Video);O->SetBoolField(TEXT("video_muted"),Impl->Data->VideoMuted);O->SetBoolField(TEXT("video_paused"),Impl->Data->VideoPaused);
 O->SetNumberField(TEXT("video_volume"),Impl->Data->VideoVolume);O->SetNumberField(TEXT("video_time"),Impl->Data->VideoTime);
 O->SetNumberField(TEXT("video_state_age"),FMath::Clamp(FPlatformTime::Seconds()-Impl->Data->VideoStateAt,0.,1.));
 O->SetNumberField(TEXT("runtime_browser_owners"),RuntimeUsers);O->SetBoolField(TEXT("video_fullscreen"),Impl->Data->Fullscreen && Impl->Data->FullscreenFillsView);O->SetBoolField(TEXT("browser_fullscreen"),Impl->Data->Fullscreen);
 O->SetNumberField(TEXT("player_width"),Impl->Data->PlayerWidth);O->SetNumberField(TEXT("player_height"),Impl->Data->PlayerHeight);O->SetBoolField(TEXT("browser_muted"),Impl->Browser && Impl->Browser->GetHost()->IsAudioMuted());
 O->SetNumberField(TEXT("dropped_samples"),double(Impl->Data->DroppedSamples));O->SetStringField(TEXT("status"),Impl->Data->Message);return O;}

int32 SEWBrowserView::OnPaint(const FPaintArgs&,const FGeometry& G,const FSlateRect&,FSlateWindowElementList& Out,int32 Layer,const FWidgetStyle&,bool)const
{if(Browser && Browser->HasPage())FSlateDrawElement::MakeBox(Out,Layer,G.ToPaintGeometry(),Browser->Brush());return Layer;}
FReply SEWBrowserView::Pointer(const FGeometry& G,const FPointerEvent& E,int32 Action)
{if(!Browser)return FReply::Unhandled();const FVector2D P=G.AbsoluteToLocal(E.GetScreenSpacePosition())/G.GetLocalSize();Browser->Mouse(FMath::Clamp(int32(P.X*1280),0,1279),FMath::Clamp(int32(P.Y*720),0,719),Action,E.GetEffectingButton()==EKeys::RightMouseButton?1:0,FMath::RoundToInt(E.GetWheelDelta()*120));return FReply::Handled();}
FReply SEWBrowserView::OnMouseButtonDown(const FGeometry& G,const FPointerEvent& E){Pointer(G,E,1);return FReply::Handled().SetUserFocus(SharedThis(this)).CaptureMouse(SharedThis(this));}
FReply SEWBrowserView::OnMouseButtonUp(const FGeometry& G,const FPointerEvent& E){Pointer(G,E,2);return FReply::Handled().ReleaseMouseCapture();}
FReply SEWBrowserView::OnMouseMove(const FGeometry& G,const FPointerEvent& E){return Pointer(G,E,0);}
FReply SEWBrowserView::OnMouseWheel(const FGeometry& G,const FPointerEvent& E){return Pointer(G,E,3);}
FReply SEWBrowserView::OnKeyDown(const FGeometry&,const FKeyEvent& E){if(E.GetKey()==EKeys::Escape)return FReply::Unhandled();if(Browser)Browser->Key(E.GetKeyCode(),false,false,(E.IsShiftDown()?2:0)|(E.IsControlDown()?4:0)|(E.IsAltDown()?8:0));return FReply::Handled();}
FReply SEWBrowserView::OnKeyUp(const FGeometry&,const FKeyEvent& E){if(Browser)Browser->Key(E.GetKeyCode(),true,false,0);return FReply::Handled();}
FReply SEWBrowserView::OnKeyChar(const FGeometry&,const FCharacterEvent& E){if(Browser)Browser->Key(E.GetCharacter(),false,true,0);return FReply::Handled();}

void FEWBrowserSurface::ApplyPlayback(const FString& VideoId,double Seconds,bool Paused)
{
    if(VideoId.IsEmpty())
    {if(!Impl->SyncedVideoId.IsEmpty()){Stop();Impl->SyncedVideoId.Empty();}return;}
    if(VideoId.Len()!=11 || !FMath::IsFinite(Seconds) || Seconds<0 || Seconds>172800)return;
    for(const TCHAR C:VideoId)if(!FChar::IsAlnum(C) && C!='_' && C!='-')return;
    if(!Start())return;
    if(Impl->SyncedVideoId!=VideoId || !HasPage())
    {
        Impl->SyncedVideoId=VideoId;
        if(VideoId==TEXT("EWTEST00001") && FParse::Param(FCommandLine::Get(),TEXT("EWOnlineAudit")))TestSyncFilm();
        else Search(TEXT("https://www.youtube.com/watch?v=")+VideoId);
        return;
    }
    // A seek is needed only after a host seek, pause, or meaningful drift.
    // Playback stays at the video's own sample rate between corrections.
    const FString Script=FString::Printf(TEXT("(()=>{const v=document.querySelector('video');if(!v||v.readyState<2||document.querySelector('.ad-showing'))return;const t=%.3f;const paused=%s;if(Number.isFinite(v.duration)&&Math.abs(v.currentTime-t)>.65){const p=document.getElementById('movie_player');if(p&&p.seekTo)p.seekTo(t,true);else v.currentTime=Math.min(t,Math.max(0,v.duration-.05));}v.muted=false;if(paused){if(!v.paused)v.pause();}else if(v.paused)v.play().catch(()=>{});})();"),Seconds,Paused?TEXT("true"):TEXT("false"));
    Impl->Browser->GetMainFrame()->ExecuteJavaScript(TCHAR_TO_WCHAR(*Script),"about:blank",1);
    if(!Paused && !IsVideoFullscreen())SetVideoFullscreen(true);
}
void FEWBrowserSurface::TestSyncFilm()
{
    if(!(FParse::Param(FCommandLine::Get(),TEXT("EWOnlineAudit")) || FParse::Param(FCommandLine::Get(),TEXT("EWCity82Audit")) || FParse::Param(FCommandLine::Get(),TEXT("EWCity82Preview"))) || !Start())return;
    TArray<uint8> Bytes;
    if(!FFileHelper::LoadFileToArray(Bytes,*(FPaths::ProjectDir()/TEXT("CinemaOnline/sync-fixture.webm"))))return;
    Impl->Client->bTestPage=true;{FScopeLock Lock(&Impl->Data->Mutex);Impl->Data->Page=true;}
    const FString Html=TEXT("<!doctype html><meta charset=utf-8><style>body{margin:0;background:#000}video{width:100vw;height:100vh}</style><video autoplay controls playsinline src='data:video/webm;base64,")+FBase64::Encode(Bytes)+TEXT("'></video><script>window.__EW_SYNC_FIXTURE=true;</script>");
    const std::string Encoded=CefURIEncode(TCHAR_TO_UTF8(*Html),false).ToString();
    Impl->Browser->GetMainFrame()->LoadURL("data:text/html;charset=utf-8,"+Encoded);
}
void FEWBrowserSurface::SeekForAudit(double Seconds,bool Paused)
{
    if(!FParse::Param(FCommandLine::Get(),TEXT("EWOnlineAudit")) || !Impl->Browser)return;
    const FString Script=FString::Printf(TEXT("(()=>{const v=document.querySelector('video');if(!v)return;v.currentTime=%.3f;%s;})();"),Seconds,Paused?TEXT("v.pause()"):TEXT("v.play()"));
    Impl->Browser->GetMainFrame()->ExecuteJavaScript(TCHAR_TO_WCHAR(*Script),"about:blank",1);
}
