#include "EWGamepadModule.h"
#include "Interfaces/IPluginManager.h"
#include "GenericPlatform/GenericPlatformInputDeviceMapper.h"
#include "GenericPlatform/GenericApplication.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "SDL3/SDL.h"

namespace
{
struct FButtonBinding { SDL_GamepadButton Source; FGamepadKeyNames::Type Target; };
const FButtonBinding Bindings[] = {
    {SDL_GAMEPAD_BUTTON_SOUTH, FGamepadKeyNames::FaceButtonBottom},
    {SDL_GAMEPAD_BUTTON_EAST, FGamepadKeyNames::FaceButtonRight},
    {SDL_GAMEPAD_BUTTON_WEST, FGamepadKeyNames::FaceButtonLeft},
    {SDL_GAMEPAD_BUTTON_NORTH, FGamepadKeyNames::FaceButtonTop},
    {SDL_GAMEPAD_BUTTON_BACK, FGamepadKeyNames::SpecialLeft},
    {SDL_GAMEPAD_BUTTON_START, FGamepadKeyNames::SpecialRight},
    {SDL_GAMEPAD_BUTTON_LEFT_STICK, FGamepadKeyNames::LeftThumb},
    {SDL_GAMEPAD_BUTTON_RIGHT_STICK, FGamepadKeyNames::RightThumb},
    {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, FGamepadKeyNames::LeftShoulder},
    {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, FGamepadKeyNames::RightShoulder},
    {SDL_GAMEPAD_BUTTON_DPAD_UP, FGamepadKeyNames::DPadUp},
    {SDL_GAMEPAD_BUTTON_DPAD_DOWN, FGamepadKeyNames::DPadDown},
    {SDL_GAMEPAD_BUTTON_DPAD_LEFT, FGamepadKeyNames::DPadLeft},
    {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, FGamepadKeyNames::DPadRight}
};
const FGamepadKeyNames::Type AxisNames[] = {FGamepadKeyNames::LeftAnalogX, FGamepadKeyNames::LeftAnalogY,
    FGamepadKeyNames::RightAnalogX, FGamepadKeyNames::RightAnalogY, FGamepadKeyNames::LeftTriggerAnalog, FGamepadKeyNames::RightTriggerAnalog};
}

class FEWSonyInputDevice : public IInputDevice
{
public:
    explicit FEWSonyInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& InHandler) : Handler(InHandler)
    {
        // Basic USB/Bluetooth controls do not require changing controller
        // reporting mode, player lights, pairing or installing a driver.
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5, "1");
        SDL_SetHint(SDL_HINT_JOYSTICK_ENHANCED_REPORTS, "0");
        SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI_PS5_PLAYER_LED, "0");
        SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
        bReady = SDL_InitSubSystem(SDL_INIT_GAMEPAD);
        if (!bReady) UE_LOG(LogTemp, Error, TEXT("EW_GAMEPAD SDL init failed: %s"), UTF8_TO_TCHAR(SDL_GetError()));
        if (bReady) SDL_SetGamepadEventsEnabled(false);
        auto& Mapper = IPlatformInputDeviceMapper::Get();
        User = Mapper.GetPrimaryPlatformUser(); InputId = Mapper.AllocateNewInputDeviceId();
    }
    virtual ~FEWSonyInputDevice() override { Close(); VirtualConnect(false); if (bReady) SDL_QuitSubSystem(SDL_INIT_GAMEPAD); }
    virtual void Tick(float DeltaTime) override {}
    virtual void SetMessageHandler(const TSharedRef<FGenericApplicationMessageHandler>& InHandler) override { Handler = InHandler; }
    virtual bool Exec(UWorld*, const TCHAR*, FOutputDevice&) override { return false; }
    virtual void SetChannelValue(int32, FForceFeedbackChannelType, float) override {}
    virtual void SetChannelValues(int32, const FForceFeedbackValues&) override {}
    virtual bool SupportsForceFeedback(int32) override { return false; }
    virtual bool IsGamepadAttached() const override { return Pad != nullptr; }
    double LastInput = -1000;
    uint64 Events = 0;
    FString Name;
    bool VirtualConnect(bool Connect)
    {
        if(!bReady || !FParse::Param(FCommandLine::Get(),TEXT("EWGamepadAudit")))return false;
        if(!Connect)
        {
            if(Pad && SDL_GetGamepadID(Pad)==VirtualId)Close();
            if(VirtualJoystick){SDL_CloseJoystick(VirtualJoystick);VirtualJoystick=nullptr;}
            if(VirtualId){SDL_DetachVirtualJoystick(VirtualId);VirtualId=0;}return true;
        }
        if(VirtualId)return true;
        Close();
        SDL_VirtualJoystickDesc D;SDL_INIT_INTERFACE(&D);
        D.type=SDL_JOYSTICK_TYPE_GAMEPAD;D.vendor_id=0x054c;D.product_id=0x0ce6;
        D.naxes=SDL_GAMEPAD_AXIS_COUNT;D.nbuttons=SDL_GAMEPAD_BUTTON_COUNT;
        D.axis_mask=(1u<<SDL_GAMEPAD_AXIS_COUNT)-1;D.button_mask=(1u<<SDL_GAMEPAD_BUTTON_COUNT)-1;
        D.name="Endless World SDL virtual audit (not physical)";
        VirtualId=SDL_AttachVirtualJoystick(&D);if(!VirtualId)return false;
        VirtualJoystick=SDL_OpenJoystick(VirtualId);NextScan=0;return VirtualJoystick!=nullptr;
    }
    bool VirtualAxis(int32 Axis,float V)
    {return VirtualJoystick && Axis>=0 && Axis<6 && SDL_SetJoystickVirtualAxis(VirtualJoystick,Axis,Sint16(FMath::Clamp(V,-1.f,1.f)*32767));}
    bool VirtualButton(int32 Button,bool Pressed)
    {return VirtualJoystick && Button>=0 && Button<SDL_GAMEPAD_BUTTON_COUNT && SDL_SetJoystickVirtualButton(VirtualJoystick,Button,Pressed);}

    virtual void SendControllerEvents() override
    {
        if (!bReady) return;
        SDL_UpdateGamepads();
        if (Pad && !SDL_GamepadConnected(Pad)) Close();
        const double Now = FPlatformTime::Seconds();
        if (!Pad && Now >= NextScan)
        {
            NextScan = Now + 1;
            int Count = 0; SDL_JoystickID* Ids = SDL_GetGamepads(&Count);
            for (int I = 0; I < Count; ++I)
            {
                // The opt-in virtual test must not consume a connected real
                // controller or reopen it after its synthetic unplug check.
                if (FParse::Param(FCommandLine::Get(),TEXT("EWGamepadAudit")) && Ids[I]!=VirtualId) continue;
                // Xbox devices are intentionally left to Unreal's XInput.
                // Reading them a second time would double movement and clicks.
                if (SDL_GetGamepadTypeForID(Ids[I]) != SDL_GAMEPAD_TYPE_PS5 && Ids[I]!=VirtualId) continue;
                Pad = SDL_OpenGamepad(Ids[I]);
                if (Pad)
                {
                    Name = UTF8_TO_TCHAR(SDL_GetGamepadName(Pad));
                    IPlatformInputDeviceMapper::Get().Internal_MapInputDeviceToUser(InputId, User, EInputDeviceConnectionState::Connected);
                    UE_LOG(LogTemp, Display, TEXT("EW_GAMEPAD connected Sony device: %s"), *Name); break;
                }
            }
            SDL_free(Ids);
        }
        const bool Focused = FSlateApplication::IsInitialized() && FSlateApplication::Get().IsActive();
        if (!Pad || !Focused) { ReleaseAll(); return; }
        for (int32 I = 0; I < UE_ARRAY_COUNT(Bindings); ++I)
        {
            const bool Pressed = SDL_GetGamepadButton(Pad, Bindings[I].Source) ||
                (Bindings[I].Source == SDL_GAMEPAD_BUTTON_NORTH && SDL_GetGamepadButton(Pad, SDL_GAMEPAD_BUTTON_TOUCHPAD));
            if (Pressed != Buttons[I])
            {
                LastInput = Now; ++Events;
                if (Pressed) { Handler->OnControllerButtonPressed(Bindings[I].Target, User, InputId, false); RepeatAt[I] = Now + .40; }
                else Handler->OnControllerButtonReleased(Bindings[I].Target, User, InputId, false);
                Buttons[I] = Pressed;
            }
            else if (Pressed && I >= 10 && Now >= RepeatAt[I])
            { Handler->OnControllerButtonPressed(Bindings[I].Target, User, InputId, true); RepeatAt[I] = Now + .12; }
        }
        for (int32 I = 0; I < 6; ++I)
        {
            const Sint16 Raw = SDL_GetGamepadAxis(Pad, SDL_GamepadAxis(I));
            float Value = Raw < 0 ? float(Raw) / 32768.f : float(Raw) / 32767.f;
            if (I == 1 || I == 3) Value = -Value;
            if (FMath::Abs(Value) < .01f) Value = 0;
            if (!FMath::IsNearlyEqual(Value, Axes[I], .0001f) || Value != 0)
            {
                if (FMath::Abs(Value) > .18f) { LastInput = Now; ++Events; }
                Handler->OnControllerAnalog(AxisNames[I], User, InputId, Value); Axes[I] = Value;
            }
        }
    }
private:
    TSharedRef<FGenericApplicationMessageHandler> Handler;
    SDL_Gamepad* Pad = nullptr;
    SDL_JoystickID VirtualId=0;
    SDL_Joystick* VirtualJoystick=nullptr;
    FPlatformUserId User;
    FInputDeviceId InputId;
    bool bReady = false;
    bool Buttons[UE_ARRAY_COUNT(Bindings)] = {};
    double RepeatAt[UE_ARRAY_COUNT(Bindings)] = {};
    float Axes[6] = {};
    double NextScan = 0;
    void ReleaseAll()
    {
        // The module can outlive Slate during process shutdown. Forget held
        // state without sending events to the destroyed application then.
        if(!FSlateApplication::IsInitialized())
        {
            FMemory::Memzero(Buttons);FMemory::Memzero(Axes);return;
        }
        for (int32 I = 0; I < UE_ARRAY_COUNT(Bindings); ++I) if (Buttons[I])
        { Handler->OnControllerButtonReleased(Bindings[I].Target, User, InputId, false); Buttons[I] = false; }
        for (int32 I = 0; I < 6; ++I) if (Axes[I] != 0)
        { Handler->OnControllerAnalog(AxisNames[I], User, InputId, 0); Axes[I] = 0; }
    }
    void Close()
    {
        ReleaseAll();
        if (Pad)
        {
            SDL_CloseGamepad(Pad); Pad = nullptr;
            IPlatformInputDeviceMapper::Get().Internal_MapInputDeviceToUser(InputId, User, EInputDeviceConnectionState::Disconnected);
            UE_LOG(LogTemp, Display, TEXT("EW_GAMEPAD Sony device disconnected; held input released"));
        }
    }
};

void FEWGamepadModule::StartupModule()
{
    if (IsRunningCommandlet() || FParse::Param(FCommandLine::Get(),TEXT("EWTraversalAudit")) ||
        FParse::Param(FCommandLine::Get(),TEXT("EWRuntimeAudit")) || FParse::Param(FCommandLine::Get(),TEXT("EWArtStudy")) ||
        FParse::Param(FCommandLine::Get(),TEXT("EWGraphicsAudit"))) return;
    const auto Plugin = IPluginManager::Get().FindPlugin(TEXT("EWGamepad"));
    if (Plugin) SdlLibrary = FPlatformProcess::GetDllHandle(*(Plugin->GetBaseDir() / TEXT("Binaries/ThirdParty/Win64/SDL3.dll")));
    if (SdlLibrary) IInputDeviceModule::StartupModule();
    else UE_LOG(LogTemp, Error, TEXT("EW_GAMEPAD bundled SDL3 could not be loaded"));
}
void FEWGamepadModule::ShutdownModule()
{
    IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureName(), this);
    Device.Reset();
    // WindowsApplication can retain the input device through late shutdown.
    // Keep its delay-loaded library mapped until process teardown.
}
TSharedPtr<IInputDevice> FEWGamepadModule::CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& Handler)
{
    if (!SdlLibrary) return nullptr;
    Device = MakeShared<FEWSonyInputDevice>(Handler); return Device;
}
FEWGamepadModule* FEWGamepadModule::GetIfLoaded() { return FModuleManager::GetModulePtr<FEWGamepadModule>(TEXT("EWGamepad")); }
bool FEWGamepadModule::SonyConnected() const { return Device && Device->IsGamepadAttached(); }
bool FEWGamepadModule::RecentSonyInput() const { return Device && FPlatformTime::Seconds() - Device->LastInput < .5; }
uint64 FEWGamepadModule::SonyEventCount() const { return Device ? Device->Events : 0; }
FString FEWGamepadModule::DeviceStatus() const { return SonyConnected() ? Device->Name : TEXT("DualSense未接続 / Xbox標準入力は有効"); }
bool FEWGamepadModule::AuditVirtualConnect(bool Connected){return Device && Device->VirtualConnect(Connected);}
bool FEWGamepadModule::AuditVirtualAxis(int32 Axis,float Value){return Device && Device->VirtualAxis(Axis,Value);}
bool FEWGamepadModule::AuditVirtualButton(int32 Button,bool Pressed){return Device && Device->VirtualButton(Button,Pressed);}
IMPLEMENT_MODULE(FEWGamepadModule, EWGamepad)
