#include "EWNativeDisplayProbe.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/SlateRenderer.h"
#include "RenderingThread.h"
#include "DynamicRHI.h"
#include "RHIResources.h"
#include "Templates/RefCounting.h"
#include "Widgets/SWindow.h"
#include <initializer_list>

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#include "Windows/AllowWindowsPlatformTypes.h"
#include <dxgi1_6.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
void Null(const TSharedPtr<FJsonObject>& Out, const TCHAR* Name)
{
    Out->SetField(Name, MakeShared<FJsonValueNull>());
}

#if PLATFORM_WINDOWS
TArray<TSharedPtr<FJsonValue>> Numbers(std::initializer_list<double> Values)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    for (double Value : Values) Result.Add(MakeShared<FJsonValueNumber>(Value));
    return Result;
}

const TCHAR* FormatName(DXGI_FORMAT Format)
{
    switch (Format)
    {
    case DXGI_FORMAT_R10G10B10A2_UNORM: return TEXT("R10G10B10A2_UNORM");
    case DXGI_FORMAT_R16G16B16A16_FLOAT: return TEXT("R16G16B16A16_FLOAT");
    case DXGI_FORMAT_B8G8R8A8_UNORM: return TEXT("B8G8R8A8_UNORM");
    case DXGI_FORMAT_R8G8B8A8_UNORM: return TEXT("R8G8B8A8_UNORM");
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: return TEXT("B8G8R8A8_UNORM_SRGB");
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return TEXT("R8G8B8A8_UNORM_SRGB");
    default: return TEXT("OTHER_DXGI_FORMAT");
    }
}

const TCHAR* ColorSpaceName(DXGI_COLOR_SPACE_TYPE ColorSpace)
{
    switch (ColorSpace)
    {
    case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709: return TEXT("RGB_FULL_G22_NONE_P709");
    case DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709: return TEXT("RGB_FULL_G10_NONE_P709");
    case DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020: return TEXT("RGB_FULL_G2084_NONE_P2020");
    case DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P2020: return TEXT("RGB_FULL_G22_NONE_P2020");
    default: return TEXT("OTHER_DXGI_COLOR_SPACE");
    }
}

void HResult(const TSharedPtr<FJsonObject>& Out, const TCHAR* Name, HRESULT Value)
{
    Out->SetStringField(Name, FString::Printf(TEXT("0x%08X"), static_cast<uint32>(Value)));
}
#endif
}

TSharedPtr<FJsonObject> EWNativeDisplayProbe::Read()
{
    const auto Out = MakeShared<FJsonObject>();
    Out->SetStringField(TEXT("format"), TEXT("ew.native-display-probe"));
    Out->SetNumberField(TEXT("schema_version"), 1);
    Out->SetStringField(TEXT("status"), TEXT("NOT_MEASURED"));
    Out->SetBoolField(TEXT("read_only"), true);
    Out->SetBoolField(TEXT("rendering_flushed"), false);
    Out->SetBoolField(TEXT("swapchain_desc_available"), false);
    Out->SetBoolField(TEXT("output_desc_available"), false);
    Out->SetStringField(TEXT("source"), TEXT("game_window_slate_rhi_native_dxgi"));
    Out->SetStringField(TEXT("swapchain_applied_color_space_status"), TEXT("NOT_MEASURED"));
    Out->SetStringField(TEXT("swapchain_applied_color_space_reason"),
        TEXT("DXGI exposes SetColorSpace1 and CheckColorSpaceSupport, but no applied-colorspace getter. Output metadata and support flags are not applied swapchain state."));
    Out->SetStringField(TEXT("limits"),
        TEXT("Readback of native descriptors only; no pixel encoding, photometry, image quality, or presented-FPS verification. No display settings are changed."));
    for (const TCHAR* Name : {TEXT("swapchain_applied_color_space"), TEXT("swapchain_format"),
        TEXT("swapchain_format_name"), TEXT("swapchain_width"), TEXT("swapchain_height"),
        TEXT("swapchain_fullscreen"), TEXT("output_desktop_color_space"),
        TEXT("output_desktop_color_space_name"), TEXT("output_hdr_active"),
        TEXT("output_bits_per_color"), TEXT("output_min_luminance_nits"),
        TEXT("output_max_luminance_nits"), TEXT("output_max_full_frame_luminance_nits")})
        Null(Out, Name);

    if (!IsInGameThread())
    {
        Out->SetStringField(TEXT("reason"), TEXT("game_thread_required"));
        return Out;
    }

#if !PLATFORM_WINDOWS
    Out->SetStringField(TEXT("reason"), TEXT("windows_dxgi_only"));
    return Out;
#else
    if (!GDynamicRHI || (GDynamicRHI->GetInterfaceType() != ERHIInterfaceType::D3D12 &&
        GDynamicRHI->GetInterfaceType() != ERHIInterfaceType::D3D11))
    {
        Out->SetStringField(TEXT("reason"), TEXT("dxgi_rhi_unavailable"));
        return Out;
    }
    Out->SetStringField(TEXT("rhi"), GDynamicRHI->GetName());
    const TSharedPtr<SWindow> Window = GEngine && GEngine->GameViewport ? GEngine->GameViewport->GetWindow() : nullptr;
    if (!Window || !FSlateApplication::IsInitialized())
    {
        Out->SetStringField(TEXT("reason"), TEXT("game_window_or_slate_unavailable"));
        return Out;
    }
    const auto Renderer = FSlateApplication::Get().GetRenderer();
    if (!Renderer)
    {
        Out->SetStringField(TEXT("reason"), TEXT("slate_renderer_unavailable"));
        return Out;
    }

    // Slate owns the RHI viewport. Do not cache it or hold a reference across
    // frames/resizes: Slate requires its reference to be exclusive on resize.
    FlushRenderingCommands();
    Out->SetBoolField(TEXT("rendering_flushed"), true);
    TRefCountPtr<IDXGISwapChain1> SwapChain;
    {
        const FViewportRHIRef Viewport = static_cast<FRHIViewport*>(Renderer->GetViewportResource(*Window));
        if (!Viewport.IsValid())
        {
            Out->SetStringField(TEXT("reason"), TEXT("native_rhi_viewport_unavailable"));
            return Out;
        }
        auto* Native = static_cast<IDXGISwapChain*>(Viewport->GetNativeSwapChain());
        if (!Native)
        {
            Out->SetStringField(TEXT("reason"), TEXT("native_swapchain_unavailable"));
            return Out;
        }
        const HRESULT Result = Native->QueryInterface(IID_PPV_ARGS(SwapChain.GetInitReference()));
        HResult(Out, TEXT("swapchain_query_hresult"), Result);
        if (FAILED(Result) || !SwapChain)
        {
            Out->SetStringField(TEXT("reason"), TEXT("swapchain1_query_failed"));
            return Out;
        }
    }

    DXGI_SWAP_CHAIN_DESC1 Desc = {};
    const HRESULT DescResult = SwapChain->GetDesc1(&Desc);
    HResult(Out, TEXT("swapchain_desc_hresult"), DescResult);
    if (SUCCEEDED(DescResult))
    {
        Out->SetBoolField(TEXT("swapchain_desc_available"), true);
        Out->SetNumberField(TEXT("swapchain_format"), static_cast<uint32>(Desc.Format));
        Out->SetStringField(TEXT("swapchain_format_name"), FormatName(Desc.Format));
        Out->SetNumberField(TEXT("swapchain_width"), Desc.Width);
        Out->SetNumberField(TEXT("swapchain_height"), Desc.Height);
        Out->SetNumberField(TEXT("swapchain_buffer_count"), Desc.BufferCount);
        Out->SetNumberField(TEXT("swapchain_swap_effect"), static_cast<uint32>(Desc.SwapEffect));
        Out->SetNumberField(TEXT("swapchain_flags"), Desc.Flags);
    }
    // Win32 BOOL is int; TRUE/FALSE are hidden by UE's platform wrappers.
    int Fullscreen = 0;
    const HRESULT FullscreenResult = SwapChain->GetFullscreenState(&Fullscreen, nullptr);
    HResult(Out, TEXT("swapchain_fullscreen_hresult"), FullscreenResult);
    if (SUCCEEDED(FullscreenResult)) Out->SetBoolField(TEXT("swapchain_fullscreen"), Fullscreen != 0);

    TRefCountPtr<IDXGISwapChain3> SwapChain3;
    const HRESULT Query3Result = SwapChain->QueryInterface(IID_PPV_ARGS(SwapChain3.GetInitReference()));
    HResult(Out, TEXT("swapchain3_query_hresult"), Query3Result);
    TArray<TSharedPtr<FJsonValue>> Support;
    if (SUCCEEDED(Query3Result) && SwapChain3)
    {
        for (DXGI_COLOR_SPACE_TYPE ColorSpace : {DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709,
            DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709, DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020})
        {
            const auto Item = MakeShared<FJsonObject>();
            Item->SetNumberField(TEXT("color_space"), static_cast<uint32>(ColorSpace));
            Item->SetStringField(TEXT("name"), ColorSpaceName(ColorSpace));
            UINT Flags = 0;
            const HRESULT Result = SwapChain3->CheckColorSpaceSupport(ColorSpace, &Flags);
            HResult(Item, TEXT("hresult"), Result);
            if (SUCCEEDED(Result))
            {
                Item->SetNumberField(TEXT("support_flags"), Flags);
                Item->SetBoolField(TEXT("present_supported"), (Flags & DXGI_SWAP_CHAIN_COLOR_SPACE_SUPPORT_FLAG_PRESENT) != 0);
            }
            else
            {
                Null(Item, TEXT("support_flags"));
                Null(Item, TEXT("present_supported"));
            }
            Support.Add(MakeShared<FJsonValueObject>(Item));
        }
    }
    Out->SetArrayField(TEXT("swapchain_color_space_support"), Support);

    TRefCountPtr<IDXGIOutput> Output;
    const HRESULT OutputResult = SwapChain->GetContainingOutput(Output.GetInitReference());
    HResult(Out, TEXT("containing_output_hresult"), OutputResult);
    if (SUCCEEDED(OutputResult) && Output)
    {
        TRefCountPtr<IDXGIOutput6> Output6;
        const HRESULT QueryResult = Output->QueryInterface(IID_PPV_ARGS(Output6.GetInitReference()));
        HResult(Out, TEXT("output6_query_hresult"), QueryResult);
        if (SUCCEEDED(QueryResult) && Output6)
        {
            DXGI_OUTPUT_DESC1 OutputDesc = {};
            const HRESULT Result = Output6->GetDesc1(&OutputDesc);
            HResult(Out, TEXT("output_desc_hresult"), Result);
            if (SUCCEEDED(Result))
            {
                Out->SetBoolField(TEXT("output_desc_available"), true);
                Out->SetStringField(TEXT("output_device_name"), OutputDesc.DeviceName);
                Out->SetBoolField(TEXT("output_attached_to_desktop"), OutputDesc.AttachedToDesktop != 0);
                Out->SetArrayField(TEXT("output_desktop_rect"), Numbers({double(OutputDesc.DesktopCoordinates.left),
                    double(OutputDesc.DesktopCoordinates.top), double(OutputDesc.DesktopCoordinates.right), double(OutputDesc.DesktopCoordinates.bottom)}));
                Out->SetNumberField(TEXT("output_rotation"), static_cast<uint32>(OutputDesc.Rotation));
                Out->SetNumberField(TEXT("output_desktop_color_space"), static_cast<uint32>(OutputDesc.ColorSpace));
                Out->SetStringField(TEXT("output_desktop_color_space_name"), ColorSpaceName(OutputDesc.ColorSpace));
                Out->SetBoolField(TEXT("output_hdr_active"), OutputDesc.ColorSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020);
                Out->SetStringField(TEXT("output_hdr_active_source"), TEXT("containing_output_desktop_colorspace_not_swapchain"));
                Out->SetNumberField(TEXT("output_bits_per_color"), OutputDesc.BitsPerColor);
                Out->SetNumberField(TEXT("output_min_luminance_nits"), OutputDesc.MinLuminance);
                Out->SetNumberField(TEXT("output_max_luminance_nits"), OutputDesc.MaxLuminance);
                Out->SetNumberField(TEXT("output_max_full_frame_luminance_nits"), OutputDesc.MaxFullFrameLuminance);
                Out->SetArrayField(TEXT("output_red_primary_xy"), Numbers({OutputDesc.RedPrimary[0], OutputDesc.RedPrimary[1]}));
                Out->SetArrayField(TEXT("output_green_primary_xy"), Numbers({OutputDesc.GreenPrimary[0], OutputDesc.GreenPrimary[1]}));
                Out->SetArrayField(TEXT("output_blue_primary_xy"), Numbers({OutputDesc.BluePrimary[0], OutputDesc.BluePrimary[1]}));
                Out->SetArrayField(TEXT("output_white_point_xy"), Numbers({OutputDesc.WhitePoint[0], OutputDesc.WhitePoint[1]}));
            }
        }
    }
    Out->SetStringField(TEXT("status"), Out->GetBoolField(TEXT("swapchain_desc_available")) &&
        Out->GetBoolField(TEXT("output_desc_available")) ? TEXT("DESCRIPTORS_OBSERVED") : TEXT("PARTIAL_NOT_MEASURED"));
    return Out;
#endif
}
