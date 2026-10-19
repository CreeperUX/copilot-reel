#include "iconography.h"

#include <winrt/base.h>
#include <algorithm>
#include <cmath>
#include <cwctype>
#include <numbers>
#include <optional>
#include <string_view>

namespace reel {
namespace {
enum class Symbol { ChatGpt, Claude, Gemini, Codex, Perplexity, Settings, Document, Assistant, App };

std::wstring Lower(std::wstring value) {
    std::transform(value.begin(), value.end(), value.begin(),
        [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return value;
}

std::optional<Symbol> IdentifyText(const std::wstring& value) {
    const auto identity = Lower(value);
    auto contains = [&](std::wstring_view value) { return identity.find(value) != std::wstring::npos; };
    // Compound names such as ChatGPT Codex identify the coding application.
    if (contains(L"codex")) return Symbol::Codex;
    if (contains(L"chatgpt") || contains(L"chat.openai.com")) return Symbol::ChatGpt;
    if (contains(L"claude") || contains(L"anthropic")) return Symbol::Claude;
    if (contains(L"gemini")) return Symbol::Gemini;
    if (contains(L"perplexity")) return Symbol::Perplexity;
    if (contains(L"ms-settings:") || contains(L"windows settings") ||
        contains(L"systemsettings.exe")) return Symbol::Settings;
    if (contains(L"notepad")) return Symbol::Document;
    if (contains(L"copilot") || contains(L"deepseek") || contains(L"grok") ||
        contains(L"qwen") || contains(L"mistral") || contains(L"poe.com")) return Symbol::Assistant;
    return {};
}

Symbol Identify(const AppTarget& app) {
    // Prefer the visible identity. Legacy package names can differ from the
    // product name (for example, ChatGPT in an OpenAI.Codex package).
    if (auto symbol = IdentifyText(app.name)) return *symbol;
    if (auto symbol = IdentifyText(app.target)) return *symbol;
    return Symbol::App;
}

struct SavedTransform {
    explicit SavedTransform(ID2D1RenderTarget* value) : target(value) { target->GetTransform(&matrix); }
    ~SavedTransform() { target->SetTransform(matrix); }
    ID2D1RenderTarget* target;
    D2D1_MATRIX_3X2_F matrix{};
};

template <typename Populate>
void Path(ID2D1RenderTarget* target, ID2D1Factory* factory, ID2D1Brush* brush,
          ID2D1StrokeStyle* style, float stroke, bool filled, Populate populate) {
    winrt::com_ptr<ID2D1PathGeometry> geometry;
    winrt::com_ptr<ID2D1GeometrySink> sink;
    winrt::check_hresult(factory->CreatePathGeometry(geometry.put()));
    winrt::check_hresult(geometry->Open(sink.put()));
    populate(sink.get());
    winrt::check_hresult(sink->Close());
    if (filled) target->FillGeometry(geometry.get(), brush);
    else target->DrawGeometry(geometry.get(), brush, stroke, style);
}
} // namespace

void DrawAppSymbol(ID2D1RenderTarget* target, const AppTarget& app,
                   const D2D1_RECT_F& bounds, ID2D1Brush* brush, float strokeWidth) {
    if (!target || !brush || strokeWidth <= 0) return;
    const float extent = (std::min)(bounds.right - bounds.left, bounds.bottom - bounds.top);
    if (extent <= 0) return;
    const float scale = extent / 24.0f;
    SavedTransform transform(target);
    target->SetTransform(D2D1::Matrix3x2F::Scale(scale, scale) *
        D2D1::Matrix3x2F::Translation((bounds.left + bounds.right) / 2,
                                    (bounds.top + bounds.bottom) / 2) * transform.matrix);
    winrt::com_ptr<ID2D1Factory> factory;
    target->GetFactory(factory.put());
    auto properties = D2D1::StrokeStyleProperties();
    properties.startCap = D2D1_CAP_STYLE_ROUND;
    properties.endCap = D2D1_CAP_STYLE_ROUND;
    properties.dashCap = D2D1_CAP_STYLE_ROUND;
    properties.lineJoin = D2D1_LINE_JOIN_ROUND;
    winrt::com_ptr<ID2D1StrokeStyle> style;
    winrt::check_hresult(factory->CreateStrokeStyle(properties, nullptr, 0, style.put()));
    const float stroke = strokeWidth / scale;
    auto line = [&](float x1, float y1, float x2, float y2) {
        target->DrawLine(D2D1::Point2F(x1, y1), D2D1::Point2F(x2, y2), brush, stroke, style.get());
    };
    auto ring = [&](float x, float y, float radius) {
        target->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(x, y), radius, radius), brush, stroke, style.get());
    };
    auto rectangle = [&](float left, float top, float right, float bottom, float radius) {
        target->DrawRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(left, top, right, bottom), radius, radius),
            brush, stroke, style.get());
    };

    switch (Identify(app)) {
    case Symbol::ChatGpt:
        ring(-4, 0, 6);
        ring(4, 0, 6);
        break;
    case Symbol::Claude:
        for (int ray = 0; ray < 8; ++ray) {
            const float angle = static_cast<float>(ray) * std::numbers::pi_v<float> / 4.0f;
            line(std::cos(angle) * 4, std::sin(angle) * 4,
                 std::cos(angle) * 10, std::sin(angle) * 10);
        }
        break;
    case Symbol::Gemini:
        Path(target, factory.get(), brush, style.get(), stroke, true, [](ID2D1GeometrySink* sink) {
            sink->BeginFigure(D2D1::Point2F(0, -10), D2D1_FIGURE_BEGIN_FILLED);
            sink->AddBezier(D2D1::BezierSegment({2, -3}, {3, -2}, {10, 0}));
            sink->AddBezier(D2D1::BezierSegment({3, 2}, {2, 3}, {0, 10}));
            sink->AddBezier(D2D1::BezierSegment({-2, 3}, {-3, 2}, {-10, 0}));
            sink->AddBezier(D2D1::BezierSegment({-3, -2}, {-2, -3}, {0, -10}));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
        });
        break;
    case Symbol::Codex:
        line(-4, -7, -10, 0); line(-10, 0, -4, 7);
        line(4, -7, 10, 0); line(10, 0, 4, 7);
        line(2, -9, -2, 9);
        break;
    case Symbol::Perplexity:
        rectangle(-8, -8, 8, 8, 2.5f);
        line(0, -10, 0, 10);
        line(-10, 0, 10, 0);
        break;
    case Symbol::Settings:
        line(-10, -7, -5, -7); line(1, -7, 10, -7); ring(-2, -7, 3);
        line(-10, 0, 1, 0); line(7, 0, 10, 0); ring(4, 0, 3);
        line(-10, 7, -6, 7); line(0, 7, 10, 7); ring(-3, 7, 3);
        break;
    case Symbol::Document:
        Path(target, factory.get(), brush, style.get(), stroke, false, [](ID2D1GeometrySink* sink) {
            sink->BeginFigure({-7, -10}, D2D1_FIGURE_BEGIN_HOLLOW);
            sink->AddLine({3, -10}); sink->AddLine({8, -5}); sink->AddLine({8, 10});
            sink->AddLine({-7, 10}); sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            sink->BeginFigure({3, -10}, D2D1_FIGURE_BEGIN_HOLLOW);
            sink->AddLine({3, -5}); sink->AddLine({8, -5}); sink->EndFigure(D2D1_FIGURE_END_OPEN);
        });
        line(-3, 0, 4, 0); line(-3, 4, 4, 4); line(-3, 8, 1, 8);
        break;
    case Symbol::Assistant:
        rectangle(-10, -8, 10, 7, 4);
        line(-5, 7, -5, 10); line(-5, 10, -1, 7);
        ring(-4, -1, 1); ring(4, -1, 1);
        break;
    case Symbol::App:
        rectangle(-10, -9, 10, 9, 3);
        line(-10, -3, 10, -3);
        line(-5, -6, -4, -6); line(-1, -6, 0, -6);
        line(-3, 3, 3, 3); line(0, 0, 0, 6);
        break;
    }
}
} // namespace reel
