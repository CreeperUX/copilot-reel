#include "iconography.h"
#include "official_icons.h"
#include <wincodec.h>
#include <d2d1_3.h>
#include <d3d11.h>
#include <dxgi.h>
#include <winrt/base.h>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>

namespace {
unsigned checks=0;
void Check(bool value,const char* description) {
    ++checks;if(!value){std::cerr<<"FAIL: "<<description<<'\n';std::exit(1);}
}
reel::AppTarget Target(const wchar_t* target,const wchar_t* name=L"Renamed entry",
                       reel::TargetKind kind=reel::TargetKind::Shell) {
    return {name,kind,target,L"",L""};
}
uint64_t Render(ID2D1Factory* factory,IWICImagingFactory* imaging,const reel::AppTarget& app,bool official) {
    winrt::com_ptr<IWICBitmap> bitmap;
    winrt::check_hresult(imaging->CreateBitmap(64,64,GUID_WICPixelFormat32bppPBGRA,WICBitmapCacheOnLoad,bitmap.put()));
    winrt::com_ptr<ID2D1RenderTarget> renderer;
    auto properties=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED));
    winrt::check_hresult(factory->CreateWicBitmapRenderTarget(bitmap.get(),properties,renderer.put()));
    winrt::com_ptr<ID2D1SolidColorBrush> brush;
    winrt::check_hresult(renderer->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White),brush.put()));
    renderer->BeginDraw();renderer->Clear(D2D1::ColorF(0,0.0f));
    const auto bounds=D2D1::RectF(4,4,60,60);
    if(official)Check(reel::DrawOfficialIcon(renderer.get(),app,bounds,true),"verified official artwork decodes and draws");
    else reel::DrawAppSymbol(renderer.get(),app,bounds,brush.get(),3.5f);
    winrt::check_hresult(renderer->EndDraw());
    winrt::com_ptr<IWICBitmapLock> locked;
    WICRect region{0,0,64,64};winrt::check_hresult(bitmap->Lock(&region,WICBitmapLockRead,locked.put()));
    UINT length=0,stride=0;BYTE* pixels=nullptr;
    winrt::check_hresult(locked->GetDataPointer(&length,&pixels));
    winrt::check_hresult(locked->GetStride(&stride));
    unsigned visible=0;uint64_t hash=14695981039346656037ull;
    for(UINT y=0;y<64;++y)for(UINT x=0;x<64;++x) {
        const auto pixel=pixels+y*stride+x*4;
        if(pixel[3])++visible;
        for(unsigned channel=0;channel<4;++channel){hash^=pixel[channel];hash*=1099511628211ull;}
    }
    Check(visible>80,"icon renders nonempty visible pixels");
    reel::ReleaseOfficialIcons(renderer.get());return hash;
}
void CheckGpuBounds(const reel::AppTarget& app) {
    winrt::com_ptr<ID3D11Device> d3d;
    winrt::check_hresult(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,
        D3D11_CREATE_DEVICE_BGRA_SUPPORT,nullptr,0,D3D11_SDK_VERSION,d3d.put(),nullptr,nullptr));
    auto dxgi=d3d.as<IDXGIDevice>();
    winrt::com_ptr<ID2D1Factory1> factory;
    winrt::check_hresult(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,
        __uuidof(ID2D1Factory1),nullptr,factory.put_void()));
    winrt::com_ptr<ID2D1Device> device;
    winrt::check_hresult(factory->CreateDevice(dxgi.get(),device.put()));
    winrt::com_ptr<ID2D1DeviceContext> baseContext;
    winrt::check_hresult(device->CreateDeviceContext(D2D1_DEVICE_CONTEXT_OPTIONS_NONE,baseContext.put()));
    auto context=baseContext.as<ID2D1DeviceContext5>();
    Check(context!=nullptr,"headless GPU test uses the SVG-capable native render path");
    const auto format=D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED);
    const auto targetProperties=D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_TARGET|D2D1_BITMAP_OPTIONS_CANNOT_DRAW,format);
    winrt::com_ptr<ID2D1Bitmap1> destination;
    winrt::check_hresult(context->CreateBitmap(D2D1::SizeU(64,64),nullptr,0,targetProperties,destination.put()));
    context->SetTarget(destination.get());
    // Use the production 28-DIP optical box with room around all four edges.
    const auto bounds=D2D1::RectF(18,18,46,46);
    context->BeginDraw();context->Clear(D2D1::ColorF(0,0.0f));
    Check(reel::DrawOfficialIcon(context.get(),app,bounds,true),"official icon draws on the SVG-capable path");
    winrt::check_hresult(context->EndDraw());
    const auto readProperties=D2D1::BitmapProperties1(D2D1_BITMAP_OPTIONS_CPU_READ|D2D1_BITMAP_OPTIONS_CANNOT_DRAW,format);
    winrt::com_ptr<ID2D1Bitmap1> readable;
    winrt::check_hresult(context->CreateBitmap(D2D1::SizeU(64,64),nullptr,0,readProperties,readable.put()));
    winrt::check_hresult(readable->CopyFromBitmap(nullptr,destination.get(),nullptr));
    D2D1_MAPPED_RECT pixels{};winrt::check_hresult(readable->Map(D2D1_MAP_OPTIONS_READ,&pixels));
    unsigned visible=0,outside=0;int minimumX=64,minimumY=64,maximumX=-1,maximumY=-1;
    for(int y=0;y<64;++y)for(int x=0;x<64;++x) {
        const auto alpha=pixels.bits[y*pixels.pitch+x*4+3];
        if(alpha) {
            ++visible;if(x<18 || x>=46 || y<18 || y>=46)++outside;
            minimumX=(std::min)(minimumX,x);minimumY=(std::min)(minimumY,y);
            maximumX=(std::max)(maximumX,x);maximumY=(std::max)(maximumY,y);
        }
    }
    winrt::check_hresult(readable->Unmap());
    Check(visible>80,"GPU icon remains visibly populated inside its optical box");
    if(outside)std::cerr<<"Out-of-bounds pixels: "<<outside<<"; extent "<<minimumX<<','<<minimumY<<" to "<<maximumX<<','<<maximumY<<'\n';
    Check(outside==0,"SVG and PNG alpha remain inside the requested 28-DIP bounds");
    Check(std::abs((minimumX+maximumX)/2.0-31.5)<=1 && std::abs((minimumY+maximumY)/2.0-31.5)<=1,
        "official artwork is centered inside the optical box");
    reel::ReleaseOfficialIcons(context.get());
}

}
int main() {
    using namespace reel;winrt::init_apartment(winrt::apartment_type::single_threaded);
    Check(IdentifySystemIcon(Target(L"ms-settings:",L"Windows 设置"))==SystemIconKind::Settings,"localized Settings target");
    Check(IdentifySystemIcon(Target(L"MS-SETTINGS:colors"))==SystemIconKind::Settings,"Settings URI page and case");
    Check(IdentifySystemIcon(Target(L"C:\\Windows\\ImmersiveControlPanel\\SystemSettings.exe"))==SystemIconKind::Settings,"Settings executable path");
    Check(IdentifySystemIcon(Target(L"windows.immersivecontrolpanel_cw5n1h2txyewy!microsoft.windows.immersivecontrolpanel",L"Renamed",TargetKind::Aumid))==SystemIconKind::Settings,"Settings package identifier");
    Check(IdentifySystemIcon(Target(L"notepad.exe",L"记事本"))==SystemIconKind::Notepad,"localized Notepad target");
    Check(IdentifySystemIcon(Target(L" \"C:\\Windows\\System32\\NOTEPAD.EXE\" "))==SystemIconKind::Notepad,"quoted absolute Notepad path");
    Check(IdentifySystemIcon(Target(L"Microsoft.WindowsNotepad_8wekyb3d8bbwe!App",L"Notes",TargetKind::Aumid))==SystemIconKind::Notepad,"Notepad package identifier");
    Check(IdentifySystemIcon(Target(L"shell:AppsFolder\\Microsoft.WindowsNotepad_8wekyb3d8bbwe!App"))==SystemIconKind::Notepad,"Shell app-folder activation");
    for(const auto& app:{Target(L"custom.exe",L"Windows Settings"),Target(L"https://example.com/notepad.exe"),
        Target(L"https://example.com/?target=ms-settings:"),Target(L"notepad.exe --fake"),
        Target(L"my-notepad.exe"),Target(L"notepad.exe",L"Name",TargetKind::Aumid)})
        Check(IdentifySystemIcon(app)==SystemIconKind::None,"non-system target cannot acquire a system identity");
    winrt::com_ptr<IWICImagingFactory> imaging;
    winrt::check_hresult(CoCreateInstance(CLSID_WICImagingFactory,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(imaging.put())));
    winrt::com_ptr<ID2D1Factory> factory;winrt::check_hresult(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,factory.put()));
    const auto settings=Render(factory.get(),imaging.get(),Target(L"ms-settings:"),false);
    const auto notepad=Render(factory.get(),imaging.get(),Target(L"notepad.exe"),false);
    const auto generic=Render(factory.get(),imaging.get(),Target(L"custom.exe"),false);
    Check(settings!=generic && notepad!=generic && settings!=notepad,"Settings and Notepad have distinct non-generic glyphs");
    Check(Render(factory.get(),imaging.get(),Target(L"custom.exe",L"Gemini"),false)==generic,"unverified vendor identity receives generic fallback, never invented logo");
    PreloadOfficialIconAssets();
    const auto gemini=Render(factory.get(),imaging.get(),Target(L"https://gemini.google.com",L"Gemini"),true);
    const auto perplexity=Render(factory.get(),imaging.get(),Target(L"https://www.perplexity.ai",L"Perplexity"),true);
    Check(gemini!=perplexity && gemini!=generic && perplexity!=generic,"Gemini and Perplexity official assets remain distinct");
    CheckGpuBounds(Target(L"https://www.perplexity.ai",L"Perplexity"));
    CheckGpuBounds(Target(L"https://gemini.google.com",L"Gemini"));
    std::cout<<"Iconography tests passed: "<<checks<<" checks; target identity and headless Direct2D rendering.\n";
}
