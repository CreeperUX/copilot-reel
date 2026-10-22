#include "iconography.h"

#include <winrt/base.h>
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <numbers>
#include <string_view>
#include <vector>

namespace reel {
namespace {
std::wstring NormalizeTarget(std::wstring value) {
    const auto first=value.find_first_not_of(L" \t\r\n");
    if(first==std::wstring::npos)return {};
    value=value.substr(first,value.find_last_not_of(L" \t\r\n")-first+1);
    if(value.size()>=2 && value.front()==L'"' && value.back()==L'"')
        value=value.substr(1,value.size()-2);
    std::transform(value.begin(),value.end(),value.begin(),
        [](wchar_t c){return static_cast<wchar_t>(std::towlower(c));});
    return value;
}

std::vector<std::wstring> Words(std::wstring_view text) {
    std::vector<std::wstring> words(1);
    for(const wchar_t c:text) {
        if(std::iswalnum(c))words.back()+=c;
        else if(!words.back().empty())words.emplace_back();
    }
    if(words.back().empty())words.pop_back();
    return words;
}

// Product words as they appear in exe, shortcut and preset names. `folder` is the
// install folder of an executable; it only disambiguates generic stems.
const wchar_t* BrandFromWords(const std::vector<std::wstring>& words,std::wstring_view folder) {
    auto at=[&](size_t i){return i<words.size()?std::wstring_view(words[i]):std::wstring_view();};
    auto first=at(0);
    if(first.size()>2 && first.ends_with(L"64"))first.remove_suffix(2);  // pycharm64, idea64
    if(first==L"chatgpt")return L"chatgpt";
    if(first==L"claude")return L"claude";
    if(first==L"gemini")return L"gemini";
    if(first==L"perplexity")return L"perplexity";
    if(first==L"antigravity" || (first==L"google" && at(1)==L"antigravity"))return L"antigravity";
    if(first==L"zcode")return L"zcode";
    if(first==L"dsh" || (first==L"deepseek" && at(1)==L"harness"))return L"dsh";
    if(first==L"vscode" || (first==L"visual" && at(1)==L"studio" && at(2)==L"code") ||
       (first==L"code" && folder==L"microsoft vs code"))return L"vscode";  // not VS Code Insiders
    if(first==L"junie")return L"junie";
    if(first==L"intellij" || (first==L"idea" && folder.starts_with(L"intellij idea")))return L"intellij-idea";
    for(const auto* product:{L"pycharm",L"webstorm",L"rider",L"clion",L"goland",L"phpstorm",L"rubymine",L"rustrover"})
        if(first==product)return product;
    if(first==L"jetbrains" && at(1)==L"toolbox")return L"toolbox";
    if(first==L"jetbrains" && at(1)==L"air")return L"air";
    return nullptr;
}

std::wstring BrandFromAumid(std::wstring_view aumid) {
    const auto family=aumid.substr(0,aumid.find(L'!'));
    const auto separator=family.rfind(L'_');
    if(separator==std::wstring_view::npos)return {};
    const auto name=family.substr(0,separator),publisher=family.substr(separator+1);
    if(publisher==L"2p2nqsd0c76g0" && name.starts_with(L"openai."))return L"chatgpt";
    if(publisher==L"pzs8sxrjxfjjc" && name==L"claude")return L"claude";
    return {};
}

std::wstring BrandFromHost(std::wstring_view url) {
    const auto scheme=url.find(L"://");
    if(url.substr(0,scheme)!=L"https" && url.substr(0,scheme)!=L"http")return {};
    auto host=url.substr(scheme+3);
    host=host.substr(0,host.find_first_of(L"/?#"));
    if(const auto user=host.rfind(L'@');user!=std::wstring_view::npos)host.remove_prefix(user+1);
    host=host.substr(0,host.find(L':'));
    auto is=[&](std::wstring_view domain) {
        return host==domain || (host.size()>domain.size() && host.ends_with(domain) && host[host.size()-domain.size()-1]==L'.');
    };
    if(is(L"chatgpt.com") || is(L"chat.openai.com"))return L"chatgpt";
    if(is(L"claude.ai"))return L"claude";
    if(is(L"gemini.google.com"))return L"gemini";
    if(is(L"perplexity.ai"))return L"perplexity";
    if(is(L"antigravity.google"))return L"antigravity";
    if(is(L"zcode.z.ai"))return L"zcode";
    if(is(L"vscode.dev") || is(L"code.visualstudio.com"))return L"vscode";
    if(is(L"junie.jetbrains.com"))return L"junie";
    return {};
}

std::wstring BrandFromPath(std::wstring_view path) {
    std::vector<std::wstring_view> parts;
    for(size_t start=0;start<=path.size();) {
        const auto end=std::min(path.find_first_of(L"\\/",start),path.size());
        parts.push_back(path.substr(start,end-start));start=end+1;
    }
    const auto file=parts.back();
    if(!file.ends_with(L".exe") && !file.ends_with(L".lnk"))return {};
    // The install folder: the exe's parent, or the product folder above bin\. A
    // profile root (C:\Users\<name>) is user data and never identifies a vendor.
    size_t folder=parts.size()>=2?parts.size()-2:parts.size();
    if(folder<parts.size() && parts[folder]==L"bin" && folder>0)--folder;
    std::wstring_view install;
    if(folder<parts.size() && !(folder>0 && parts[folder-1]==L"users"))install=parts[folder];
    const auto words=Words(file.substr(0,file.size()-4));
    // Claude Science ships its own mark; its executable's Shell icon shows it.
    if(words.size()>=2 && words[0]==L"claude" && words.back()==L"science")return {};
    if(const auto* brand=BrandFromWords(words,install))return brand;
    if(install==L"anthropicclaude")return L"claude";
    if(install==L"antigravity")return L"antigravity";
    if(install==L"zcode")return L"zcode";
    if(install==L"microsoft vs code")return L"vscode";
    if(install==L"deepseek harness")return L"dsh";
    return {};
}

struct SavedTransform {
    explicit SavedTransform(ID2D1RenderTarget* value):target(value) {target->GetTransform(&matrix);}
    ~SavedTransform() {target->SetTransform(matrix);}
    ID2D1RenderTarget* target;
    D2D1_MATRIX_3X2_F matrix{};
};

void DrawSymbol(ID2D1RenderTarget* target,SystemIconKind kind,const D2D1_RECT_F& bounds,
                ID2D1Brush* brush,float strokeWidth) {
    const float extent=(std::min)(bounds.right-bounds.left,bounds.bottom-bounds.top);
    if(extent<=0)return;
    const float scale=extent/24.0f;
    SavedTransform transform(target);
    target->SetTransform(D2D1::Matrix3x2F::Scale(scale,scale)*
        D2D1::Matrix3x2F::Translation((bounds.left+bounds.right)/2,(bounds.top+bounds.bottom)/2)*transform.matrix);
    winrt::com_ptr<ID2D1Factory> factory;target->GetFactory(factory.put());
    auto properties=D2D1::StrokeStyleProperties();
    properties.startCap=D2D1_CAP_STYLE_ROUND;properties.endCap=D2D1_CAP_STYLE_ROUND;
    properties.lineJoin=D2D1_LINE_JOIN_ROUND;
    winrt::com_ptr<ID2D1StrokeStyle> style;
    winrt::check_hresult(factory->CreateStrokeStyle(properties,nullptr,0,style.put()));
    const float stroke=strokeWidth/scale;
    auto line=[&](float x1,float y1,float x2,float y2) {
        target->DrawLine({x1,y1},{x2,y2},brush,stroke,style.get());
    };
    auto rectangle=[&](float left,float top,float right,float bottom,float radius) {
        target->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(left,top,right,bottom),radius,radius),
            brush,stroke,style.get());
    };
    switch(kind) {
    case SystemIconKind::Settings: {
        winrt::com_ptr<ID2D1PathGeometry> gear;
        winrt::com_ptr<ID2D1GeometrySink> sink;
        winrt::check_hresult(factory->CreatePathGeometry(gear.put()));
        winrt::check_hresult(gear->Open(sink.put()));
        constexpr float radii[]={8.0f,10.0f,10.0f,8.0f};
        for(int vertex=0;vertex<32;++vertex) {
            const float angle=(vertex-.5f)*std::numbers::pi_v<float>/16.0f;
            const auto point=D2D1::Point2F(std::cos(angle)*radii[vertex%4],std::sin(angle)*radii[vertex%4]);
            if(vertex==0)sink->BeginFigure(point,D2D1_FIGURE_BEGIN_HOLLOW);else sink->AddLine(point);
        }
        sink->EndFigure(D2D1_FIGURE_END_CLOSED);winrt::check_hresult(sink->Close());
        target->DrawGeometry(gear.get(),brush,stroke,style.get());
        target->DrawEllipse(D2D1::Ellipse({0,0},3,3),brush,stroke,style.get());
        break;
    }
    case SystemIconKind::Notepad:
        rectangle(-8,-9,8,10,1.5f);
        line(-4,-11,-4,-7);line(0,-11,0,-7);line(4,-11,4,-7);
        line(-4,-2,4,-2);line(-4,2,4,2);line(-4,6,2,6);
        break;
    default:
        rectangle(-10,-9,10,9,3);line(-10,-3,10,-3);
        line(-5,-6,-4,-6);line(-1,-6,0,-6);
        line(-3,3,3,3);line(0,0,0,6);
        break;
    }
}
} // namespace

SystemIconKind IdentifySystemIcon(const AppTarget& app) {
    auto target=NormalizeTarget(app.target);
    constexpr std::wstring_view appFolder=L"shell:appsfolder\\";
    if(target.starts_with(appFolder))target.erase(0,appFolder.size());
    if(target==L"windows.immersivecontrolpanel_cw5n1h2txyewy!microsoft.windows.immersivecontrolpanel" ||
       target==L"microsoft.windows.immersivecontrolpanel_cw5n1h2txyewy!microsoft.windows.immersivecontrolpanel")
        return SystemIconKind::Settings;
    if(target==L"microsoft.windowsnotepad_8wekyb3d8bbwe!app")return SystemIconKind::Notepad;
    if(app.kind==TargetKind::Aumid)return SystemIconKind::None;
    if(target.starts_with(L"ms-settings:"))return SystemIconKind::Settings;
    // Schemes and arguments are not executable paths. Display names are user
    // data, so renaming an arbitrary app to Settings cannot change its identity.
    if(target.find(L"://")!=std::wstring::npos || target.find_first_of(L"?\r\n")!=std::wstring::npos)
        return SystemIconKind::None;
    const auto separator=target.find_last_of(L"\\/");
    const auto file=std::wstring_view(target).substr(separator==std::wstring::npos?0:separator+1);
    if(file==L"systemsettings.exe")return SystemIconKind::Settings;
    if(file==L"notepad.exe")return SystemIconKind::Notepad;
    return SystemIconKind::None;
}

std::wstring IdentifyBrand(const AppTarget& app) {
    auto target=NormalizeTarget(app.target);
    if(target.empty()) {
        // Unresolved presets have no target yet; their catalog name is all there is.
        auto name=NormalizeTarget(app.name);
        const auto words=Words(name);
        if(const auto* brand=BrandFromWords(words,{}))return brand;
        return !words.empty() && words[0]==L"jetbrains"?L"jetbrains":L"";
    }
    constexpr std::wstring_view appFolder=L"shell:appsfolder\\";
    const bool packaged=target.starts_with(appFolder);
    if(packaged)target.erase(0,appFolder.size());
    if(app.kind==TargetKind::Aumid || packaged)return BrandFromAumid(target);
    if(target.find(L"://")!=std::wstring::npos)return BrandFromHost(target);
    if(target.find_first_of(L"?\r\n")!=std::wstring::npos)return {};
    const auto colon=target.find(L':');
    if(colon!=std::wstring::npos && colon!=1)return {};  // ms-settings: and other schemes
    return BrandFromPath(target);
}

void DrawAppSymbol(ID2D1RenderTarget* target,const AppTarget& app,const D2D1_RECT_F& bounds,
                   ID2D1Brush* brush,float strokeWidth) {
    if(!target || !brush || strokeWidth<=0)return;
    DrawSymbol(target,IdentifySystemIcon(app),bounds,brush,strokeWidth);
}
} // namespace reel
