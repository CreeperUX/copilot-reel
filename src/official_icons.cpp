#include "official_icons.h"

#include <d2d1_3.h>
#include <shlwapi.h>
#include <wincodec.h>
#include <winrt/base.h>
#include <algorithm>
#include <array>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <string_view>
#include <vector>

namespace reel {
namespace {
struct Source {
    std::vector<BYTE> svg;
    std::vector<BYTE> pixels;
    UINT width = 0, height = 0;
};
struct Image {
    winrt::com_ptr<ID2D1Bitmap> bitmap;
    winrt::com_ptr<ID2D1SvgDocument> svg;
    D2D1_SIZE_F svgSize{};
    bool attempted = false;
};
struct DeviceImages {
    // Retaining the owner prevents pointer reuse from matching stale resources.
    winrt::com_ptr<IUnknown> owner;
    std::map<std::wstring, Image> images;
};

std::map<std::wstring, Source> sources;
std::map<IUnknown*, DeviceImages> devices;
bool loaded = false;

std::wstring Lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return value;
}

std::wstring Identify(const AppTarget& app, bool dark) {
    const auto identity = Lower(app.name + L" " + app.target);
    auto has = [&](std::wstring_view value) { return identity.find(value) != std::wstring::npos; };
    if (has(L"chatgpt") || has(L"codex") || has(L"chat.openai.com"))
        return dark ? L"chatgpt-dark" : L"chatgpt-light";
    if (has(L"claude") || has(L"anthropic")) return L"claude";
    if (has(L"gemini")) return L"gemini";
    if (has(L"perplexity")) return L"perplexity";
    if (has(L"antigravity")) return L"antigravity";
    if (has(L"zcode")) return L"zcode";
    if (has(L"deepseek harness") || has(L"deepseek-harness") || has(L"dsh")) return L"dsh";
    if (has(L"visual studio code") || has(L"vscode") || has(L"code.visualstudio.com")) return L"vscode";
    if (has(L"junie")) return L"junie";
    if (has(L"toolbox") && has(L"jetbrains")) return L"toolbox";
    if (has(L"intellij")) return L"intellij-idea";
    for (const auto* brand : {L"pycharm", L"webstorm", L"rider", L"clion", L"goland",
                              L"phpstorm", L"rubymine", L"rustrover"})
        if (has(brand)) return brand;
    if (has(L"jetbrains air")) return L"air";
    if (has(L"jetbrains")) return L"jetbrains";
    return {};
}

std::vector<BYTE> ReadBytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) return {};
    const auto end = input.tellg();
    if (end <= 0 || end > 8 * 1024 * 1024) return {};
    std::vector<BYTE> bytes(static_cast<size_t>(end));
    input.seekg(0);
    if (!input.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()))) return {};
    return bytes;
}

winrt::com_ptr<IUnknown> Owner(ID2D1RenderTarget* target) {
    winrt::com_ptr<ID2D1DeviceContext> context;
    if (SUCCEEDED(target->QueryInterface(IID_PPV_ARGS(context.put())))) {
        winrt::com_ptr<ID2D1Device> device;
        context->GetDevice(device.put());
        return device.as<IUnknown>();
    }
    winrt::com_ptr<IUnknown> result;
    target->QueryInterface(IID_PPV_ARGS(result.put()));
    return result;
}

bool SupportsSvg(const std::vector<BYTE>& bytes) {
    // Direct2D does not implement SVG filters or masks. Rendering such documents
    // partially would change the official artwork, so use the vendor PNG instead.
    const std::string_view text(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    return !bytes.empty() && text.find("<mask") == text.npos &&
        text.find("<filter") == text.npos && text.find("<image") == text.npos &&
        text.find("<text") == text.npos && text.find("<pattern") == text.npos;
}

void Decode(Source& source, IWICImagingFactory* factory, const std::filesystem::path& path) {
    if (!factory) return;
    winrt::com_ptr<IWICBitmapDecoder> decoder;
    winrt::com_ptr<IWICBitmapFrameDecode> frame;
    winrt::com_ptr<IWICFormatConverter> converter;
    if (FAILED(factory->CreateDecoderFromFilename(path.c_str(), nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, decoder.put())) || FAILED(decoder->GetFrame(0, frame.put())) ||
        FAILED(frame->GetSize(&source.width, &source.height)) ||
        source.width == 0 || source.height == 0 || source.width > 4096 || source.height > 4096) return;
    winrt::com_ptr<IWICBitmapSource> input = frame.as<IWICBitmapSource>();
    if (source.width > 256 || source.height > 256) {
        const float scale = 256.f / (std::max)(source.width, source.height);
        const UINT width = (std::max)(1u, static_cast<UINT>(source.width * scale));
        const UINT height = (std::max)(1u, static_cast<UINT>(source.height * scale));
        winrt::com_ptr<IWICBitmapScaler> scaler;
        if (SUCCEEDED(factory->CreateBitmapScaler(scaler.put())) &&
            SUCCEEDED(scaler->Initialize(frame.get(), width, height, WICBitmapInterpolationModeFant))) {
            input = scaler.as<IWICBitmapSource>();
            source.width = width;
            source.height = height;
        }
    }
    if (FAILED(factory->CreateFormatConverter(converter.put())) ||
        FAILED(converter->Initialize(input.get(), GUID_WICPixelFormat32bppPBGRA,
            WICBitmapDitherTypeNone, nullptr, 0, WICBitmapPaletteTypeCustom))) return;
    source.pixels.resize(static_cast<size_t>(source.width) * source.height * 4);
    if (FAILED(converter->CopyPixels(nullptr, source.width * 4,
        static_cast<UINT>(source.pixels.size()), source.pixels.data()))) source.pixels.clear();
}
} // namespace

void PreloadOfficialIconAssets() {
    if (loaded) return;
    loaded = true;
    std::array<wchar_t, 32768> executable{};
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (length == 0 || length >= executable.size()) return;
    const auto root = std::filesystem::path(executable.data()).parent_path() / L"assets" / L"brands";
    winrt::com_ptr<IWICImagingFactory> factory;
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(factory.put()));
    for (const auto* name : {L"chatgpt-light", L"chatgpt-dark", L"claude", L"gemini", L"perplexity", L"antigravity",
                             L"zcode", L"dsh", L"vscode", L"jetbrains", L"toolbox", L"junie",
                             L"intellij-idea", L"pycharm", L"webstorm", L"rider", L"clion",
                             L"goland", L"phpstorm", L"rubymine", L"rustrover", L"air"}) {
        auto& source = sources[name];
        source.svg = ReadBytes(root / (std::wstring(name) + L".svg"));
        if (!SupportsSvg(source.svg)) source.svg.clear();
        Decode(source, factory.get(), root / (std::wstring(name) + L".png"));
    }
}

bool DrawOfficialIcon(ID2D1RenderTarget* target, const AppTarget& app,
                      const D2D1_RECT_F& bounds, bool dark) {
    if (!target || !loaded) return false;
    const auto key = Identify(app, dark);
    const auto found = sources.find(key);
    if (found == sources.end()) return false;
    const auto& source = found->second;
    const float width = bounds.right - bounds.left, height = bounds.bottom - bounds.top;
    if (width <= 0 || height <= 0) return false;
    auto owner = Owner(target);
    if (!owner) return false;
    auto& cache = devices[owner.get()];
    if (!cache.owner) cache.owner = owner;
    auto& item = cache.images[key];
    winrt::com_ptr<ID2D1DeviceContext5> context;
    target->QueryInterface(IID_PPV_ARGS(context.put()));
    if (!item.attempted) {
        item.attempted = true;
        if (context && !source.svg.empty()) {
            winrt::com_ptr<IStream> stream;
            stream.attach(SHCreateMemStream(source.svg.data(), static_cast<UINT>(source.svg.size())));
            if (stream && SUCCEEDED(context->CreateSvgDocument(stream.get(),
                D2D1::SizeF(width, height), item.svg.put()))) {
                winrt::com_ptr<ID2D1SvgElement> root;
                item.svg->GetRoot(root.put());
                D2D1_SVG_VIEWBOX viewBox{};
                if (root && SUCCEEDED(root->GetAttributeValue(L"viewBox",
                    D2D1_SVG_ATTRIBUTE_POD_TYPE_VIEWBOX, &viewBox, sizeof(viewBox))) &&
                    viewBox.width > 0 && viewBox.height > 0) {
                    item.svgSize = D2D1::SizeF(viewBox.width, viewBox.height);
                    // The cached document uses viewBox units as its viewport.
                    // Vendor dimensions such as 270pt otherwise resolve to
                    // 360px and overflow the caller's 28-DIP optical box.
                    // Normalize only this in-memory document, not source files.
                    const D2D1_SVG_LENGTH intrinsicWidth{viewBox.width,D2D1_SVG_LENGTH_UNITS_NUMBER};
                    const D2D1_SVG_LENGTH intrinsicHeight{viewBox.height,D2D1_SVG_LENGTH_UNITS_NUMBER};
                    if(FAILED(root->SetAttributeValue(L"width",D2D1_SVG_ATTRIBUTE_POD_TYPE_LENGTH,
                        &intrinsicWidth,sizeof(intrinsicWidth))) ||
                       FAILED(root->SetAttributeValue(L"height",D2D1_SVG_ATTRIBUTE_POD_TYPE_LENGTH,
                        &intrinsicHeight,sizeof(intrinsicHeight))) ||
                       FAILED(item.svg->SetViewportSize(item.svgSize))) item.svg=nullptr;
                } else {
                    item.svg = nullptr;
                }
            }
        }
        if (!source.pixels.empty())
            target->CreateBitmap(D2D1::SizeU(source.width, source.height), source.pixels.data(),
                source.width * 4, D2D1::BitmapProperties(
                    D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)),
                item.bitmap.put());
    }
    if (item.svg && context) {
        D2D1_MATRIX_3X2_F transform{};
        target->GetTransform(&transform);
        const float scale = (std::min)(width / item.svgSize.width, height / item.svgSize.height);
        const float left = bounds.left + (width - item.svgSize.width * scale) * .5f;
        const float top = bounds.top + (height - item.svgSize.height * scale) * .5f;
        target->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale) *
            D2D1::Matrix3x2F::Translation(left, top) * transform);
        context->DrawSvgDocument(item.svg.get());
        target->SetTransform(transform);
        return true;
    }
    if (!item.bitmap) return false;
    const float aspect = static_cast<float>(source.width) / source.height;
    const float outputWidth = (std::min)(width, height * aspect);
    const float outputHeight = outputWidth / aspect;
    const float left = bounds.left + (width - outputWidth) * .5f;
    const float top = bounds.top + (height - outputHeight) * .5f;
    target->DrawBitmap(item.bitmap.get(), D2D1::RectF(left, top, left + outputWidth, top + outputHeight),
        1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR);
    return true;
}

void ReleaseOfficialIcons(ID2D1RenderTarget* target) {
    if (!target) devices.clear();
    else if (auto owner = Owner(target)) devices.erase(owner.get());
}
} // namespace reel
