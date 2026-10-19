// Capture one explicitly supplied application HWND with Windows Graphics Capture.
// Unlike desktop screenshots, this excludes unrelated overlapping windows.
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wincodec.h>
#include <windows.graphics.capture.interop.h>
#include <windows.graphics.directx.direct3d11.interop.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Graphics.Capture.h>
#include <winrt/Windows.Graphics.DirectX.h>
#include <winrt/Windows.Graphics.DirectX.Direct3D11.h>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>

namespace capture = winrt::Windows::Graphics::Capture;
namespace directx = winrt::Windows::Graphics::DirectX;
namespace direct3d = directx::Direct3D11;

void SavePng(const wchar_t* path, ID3D11Device* device, ID3D11DeviceContext* context,
             ID3D11Texture2D* source, UINT width, UINT height) {
    D3D11_TEXTURE2D_DESC description{};
    source->GetDesc(&description);
    width = (std::min)(width, description.Width);
    height = (std::min)(height, description.Height);
    description.Usage = D3D11_USAGE_STAGING;
    description.BindFlags = 0;
    description.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    description.MiscFlags = 0;
    winrt::com_ptr<ID3D11Texture2D> staging;
    winrt::check_hresult(device->CreateTexture2D(&description, nullptr, staging.put()));
    context->CopyResource(staging.get(), source);
    D3D11_MAPPED_SUBRESOURCE mapped{};
    winrt::check_hresult(context->Map(staging.get(), 0, D3D11_MAP_READ, 0, &mapped));
    std::vector<BYTE> pixels(static_cast<size_t>(width) * height * 4);
    for (UINT row = 0; row < height; ++row) {
        auto destination = pixels.data() + static_cast<size_t>(row) * width * 4;
        const auto input = static_cast<const BYTE*>(mapped.pData) + static_cast<size_t>(row) * mapped.RowPitch;
        std::copy_n(input, static_cast<size_t>(width) * 4, destination);
        // WGC supplies premultiplied BGRA; PNG stores straight alpha.
        for (UINT column = 0; column < width; ++column) {
            auto pixel = destination + column * 4;
            if (pixel[3] != 0 && pixel[3] != 255)
                for (int channel = 0; channel < 3; ++channel)
                    pixel[channel] = static_cast<BYTE>((std::min)(255u,
                        (static_cast<unsigned>(pixel[channel]) * 255 + pixel[3] / 2) / pixel[3]));
        }
    }
    context->Unmap(staging.get(), 0);
    winrt::com_ptr<IWICImagingFactory> factory;
    winrt::check_hresult(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(factory.put())));
    winrt::com_ptr<IWICStream> stream;
    winrt::check_hresult(factory->CreateStream(stream.put()));
    winrt::check_hresult(stream->InitializeFromFilename(path, GENERIC_WRITE));
    winrt::com_ptr<IWICBitmapEncoder> encoder;
    winrt::check_hresult(factory->CreateEncoder(GUID_ContainerFormatPng, nullptr, encoder.put()));
    winrt::check_hresult(encoder->Initialize(stream.get(), WICBitmapEncoderNoCache));
    winrt::com_ptr<IWICBitmapFrameEncode> frame;
    winrt::check_hresult(encoder->CreateNewFrame(frame.put(), nullptr));
    winrt::check_hresult(frame->Initialize(nullptr));
    winrt::check_hresult(frame->SetSize(width, height));
    WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
    winrt::check_hresult(frame->SetPixelFormat(&format));
    if (format != GUID_WICPixelFormat32bppBGRA) winrt::throw_hresult(E_NOTIMPL);
    winrt::check_hresult(frame->WritePixels(height, width * 4, static_cast<UINT>(pixels.size()), pixels.data()));
    winrt::check_hresult(frame->Commit());
    winrt::check_hresult(encoder->Commit());
    std::wcout << L"Captured " << width << L" x " << height << L" pixels to " << path << L"\n";
}

int wmain(int argc, wchar_t** argv) {
    if (argc != 3) {
        std::wcerr << L"Usage: capture-window.exe <HWND decimal or hexadecimal> <absolute output.png>\n";
        return 2;
    }
    try {
        SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
        winrt::init_apartment(winrt::apartment_type::multi_threaded);
        HWND window = reinterpret_cast<HWND>(std::wcstoull(argv[1], nullptr, 0));
        if (!IsWindow(window) || !IsWindowVisible(window)) {
            std::wcerr << L"The supplied HWND must identify a visible application window.\n";
            return 3;
        }
        if (!capture::GraphicsCaptureSession::IsSupported()) winrt::throw_hresult(E_NOTIMPL);
        auto factory = winrt::get_activation_factory<capture::GraphicsCaptureItem, IGraphicsCaptureItemInterop>();
        capture::GraphicsCaptureItem item{nullptr};
        winrt::check_hresult(factory->CreateForWindow(window,
            winrt::guid_of<capture::IGraphicsCaptureItem>(), winrt::put_abi(item)));
        winrt::com_ptr<ID3D11Device> device;
        winrt::com_ptr<ID3D11DeviceContext> context;
        winrt::check_hresult(D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr,
            D3D11_CREATE_DEVICE_BGRA_SUPPORT, nullptr, 0, D3D11_SDK_VERSION,
            device.put(), nullptr, context.put()));
        auto dxgi = device.as<IDXGIDevice>();
        winrt::com_ptr<IInspectable> inspectable;
        winrt::check_hresult(CreateDirect3D11DeviceFromDXGIDevice(dxgi.get(), inspectable.put()));
        auto runtimeDevice = inspectable.as<direct3d::IDirect3DDevice>();
        auto pool = capture::Direct3D11CaptureFramePool::CreateFreeThreaded(runtimeDevice,
            directx::DirectXPixelFormat::B8G8R8A8UIntNormalized, 2, item.Size());
        auto session = pool.CreateCaptureSession(item);
        session.IsCursorCaptureEnabled(false);
        session.StartCapture();
        capture::Direct3D11CaptureFrame frame{nullptr};
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (std::chrono::steady_clock::now() < deadline) {
            frame = pool.TryGetNextFrame();
            if (frame) break;
            Sleep(20);
        }
        if (!frame) winrt::throw_hresult(HRESULT_FROM_WIN32(WAIT_TIMEOUT));
        auto access = frame.Surface().as<Windows::Graphics::DirectX::Direct3D11::IDirect3DDxgiInterfaceAccess>();
        winrt::com_ptr<ID3D11Texture2D> texture;
        winrt::check_hresult(access->GetInterface(IID_PPV_ARGS(texture.put())));
        auto size = frame.ContentSize();
        SavePng(argv[2], device.get(), context.get(), texture.get(), size.Width, size.Height);
        frame.Close();
        session.Close();
        pool.Close();
        return 0;
    } catch (const winrt::hresult_error& error) {
        std::wcerr << L"Window capture failed: " << error.message().c_str()
                   << L" (0x" << std::hex << static_cast<uint32_t>(error.code().value) << L")\n";
        return 1;
    }
}
