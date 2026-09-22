#pragma once
#include "CoreMinimal.h"
#include "IInputDeviceModule.h"

class FEWSonyInputDevice;

class EWGAMEPAD_API FEWGamepadModule : public IInputDeviceModule
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
    virtual TSharedPtr<IInputDevice> CreateInputDevice(const TSharedRef<FGenericApplicationMessageHandler>& Handler) override;
    static FEWGamepadModule* GetIfLoaded();
    bool SonyConnected() const;
    bool RecentSonyInput() const;
    uint64 SonyEventCount() const;
    FString DeviceStatus() const;
    // Process-local SDL virtual device; only enabled by the explicit audit flag.
    bool AuditVirtualConnect(bool Connected);
    bool AuditVirtualAxis(int32 Axis,float Value);
    bool AuditVirtualButton(int32 Button,bool Pressed);
private:
    void* SdlLibrary = nullptr;
    TSharedPtr<FEWSonyInputDevice> Device;
};
