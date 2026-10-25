#include "list_viewport.h"
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <string>

// The picker's pixel-scrolled LISTBOX in hidden windows: nothing is shown, focused or activated.
namespace {
unsigned checks = 0;
void Check(bool okay, const std::string& label) {
    ++checks;
    if (!okay) { std::cerr << "FAIL: " << label << '\n'; std::exit(1); }
}

LRESULT CALLBACK Plain(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
    return DefWindowProcW(window, message, wParam, lParam);
}

struct Fixture {
    HWND host = nullptr;
    reel::ListViewport scroll;
    Fixture(int count, int row, int view) {
        host = CreateWindowExW(0, L"CopilotReel.ListViewportTest", L"", WS_OVERLAPPEDWINDOW, 0, 0, 800, 600,
            nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
        const HWND viewport = CreateWindowExW(0, L"CopilotReel.ListViewportTest", L"", WS_CHILD | WS_VISIBLE | WS_VSCROLL |
            WS_CLIPCHILDREN, 10, 10, 400, view, host, nullptr, GetModuleHandleW(nullptr), nullptr);
        const HWND list = CreateWindowExW(0, L"LISTBOX", L"", WS_CHILD | WS_VISIBLE | LBS_NOTIFY | LBS_HASSTRINGS |
            LBS_OWNERDRAWFIXED | LBS_NOINTEGRALHEIGHT, 0, 0, 400, view, viewport, nullptr, GetModuleHandleW(nullptr), nullptr);
        Check(host && viewport && list, "fixture windows");
        SendMessageW(list, LB_SETITEMHEIGHT, 0, row);
        SendMessageW(list, WM_SETREDRAW, FALSE, 0);
        for (int i = 0; i < count; ++i) SendMessageW(list, LB_ADDSTRING, 0, reinterpret_cast<LPARAM>(std::to_wstring(i).c_str()));
        SendMessageW(list, WM_SETREDRAW, TRUE, 0);
        scroll.viewport = viewport;
        scroll.list = list;
        scroll.row = row;
        scroll.Layout();
    }
    ~Fixture() { DestroyWindow(host); }
    RECT ListBounds() const {
        RECT bounds{};
        GetWindowRect(scroll.list, &bounds);
        MapWindowPoints(nullptr, scroll.viewport, reinterpret_cast<POINT*>(&bounds), 2);
        return bounds;
    }
    // An item's rows in viewport pixels.
    RECT Item(int index) const {
        RECT item{};
        SendMessageW(scroll.list, LB_GETITEMRECT, index, reinterpret_cast<LPARAM>(&item));
        const RECT list = ListBounds();
        OffsetRect(&item, 0, list.top);
        return item;
    }
    // The content pixel drawn at the viewport's top edge.
    int Shown() const {
        const auto top = SendMessageW(scroll.list, LB_GETTOPINDEX, 0, 0);
        return static_cast<int>(top) * scroll.row - ListBounds().top;
    }
};

void Covers(const Fixture& f, const std::string& label) {
    const RECT list = f.ListBounds();
    const int view = f.scroll.ViewHeight();
    Check(list.top <= 0 && list.bottom >= view, label + ": the LISTBOX covers the viewport");
    Check(list.bottom - list.top == view + f.scroll.row, label + ": the LISTBOX is one row taller than the viewport");
    Check(f.Shown() == f.scroll.offset, label + ": the pixel at the top is the offset");
}

void Sweep(int count, int row, int view) {
    const std::string name = std::to_string(count) + " rows of " + std::to_string(row) + " px in " + std::to_string(view);
    Fixture f(count, row, view);
    const int max = std::max(0, count * row - view);
    Check(f.scroll.content == std::max(view, count * row), name + ": content height");
    Covers(f, name + " at rest");
    for (int offset = 0; offset <= max + 2 * row; offset += 7) {
        f.scroll.ScrollTo(offset);
        Check(f.scroll.offset == std::min(offset, max), name + ": offset clamps to the content");
        Covers(f, name + " at " + std::to_string(offset));
    }
    f.scroll.ScrollTo(max);
    Covers(f, name + " at the end");
    if (count) {
        const RECT last = f.Item(count - 1);
        Check(count * row < view || last.bottom == view, name + ": the last row ends at the viewport's bottom");
    }
    f.scroll.ScrollTo(-50);
    Check(f.scroll.offset == 0, name + ": offset never goes negative");
    if (!count) {
        Check(f.scroll.PageSelection(true) == -1, name + ": an empty list pages nowhere");
        return;
    }
    // Every selection, reached from either side, ends wholly in view.
    for (int index : {0, 1, count / 2, count - 2, count - 1, 4, 5, 6}) {
        if (index < 0 || index >= count) continue;
        for (int from : {0, max}) {
            f.scroll.ScrollTo(from);
            SendMessageW(f.scroll.list, LB_SETCURSEL, index, 0);
            f.scroll.RevealSelection();
            const RECT item = f.Item(index);
            Check(item.top >= 0 && item.bottom <= view, name + ": row " + std::to_string(index) + " revealed from " + std::to_string(from));
            Covers(f, name + " after revealing " + std::to_string(index));
        }
    }
    // The LISTBOX scrolls itself to follow keys; revealing the selection realigns it.
    if (count > 20) {
        f.scroll.ScrollTo(row * 3 + 5);
        SendMessageW(f.scroll.list, LB_SETCURSEL, 12, 0);
        SendMessageW(f.scroll.list, LB_SETTOPINDEX, 11, 0);
        f.scroll.RevealSelection();
        const RECT item = f.Item(12);
        Check(item.top >= 0 && item.bottom <= view, name + ": a self-scrolled LISTBOX is realigned");
        Covers(f, name + " after realigning");
    }
    // Paging moves by the rows that show, less one for context.
    SendMessageW(f.scroll.list, LB_SETCURSEL, 0, 0);
    const int page = std::max(1, view / row - 1);
    Check(f.scroll.PageSelection(true) == std::min(page, count - 1), name + ": PageDown moves a viewport page");
    Check(f.scroll.PageSelection(false) == 0, name + ": PageUp stops at the first row");
}

void Wheel() {
    Fixture f(400, 68, 340);
    UINT lines = 3;
    SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
    const double notch = lines == WHEEL_PAGESCROLL ? 340.0 : std::min(340.0, lines * 68.0);
    for (int i = 0; i < 8; ++i) f.scroll.Wheel(-15);
    const int expected = static_cast<int>(8 * 15 * notch / WHEEL_DELTA);
    Check(f.scroll.offset == expected || f.scroll.offset == expected - 1, "small touchpad deltas add up to their share of a notch");
    Covers(f, "after touchpad deltas");
    const int before = f.scroll.offset;
    f.scroll.Wheel(15);
    Check(f.scroll.offset < before && f.scroll.wheel <= 0, "a reversal scrolls back and drops the leftover");
    f.scroll.Wheel(WHEEL_DELTA * 1000);
    Check(f.scroll.offset == 0, "a large upward delta stops at the top");
    f.scroll.ScrollBar(SB_BOTTOM);
    Check(f.scroll.offset == 400 * 68 - 340, "the scroll bar reaches the end");
    Covers(f, "at the scroll bar's end");
}
}

int main() {
    WNDCLASSEXW plain{sizeof(plain)};
    plain.lpfnWndProc = Plain;
    plain.hInstance = GetModuleHandleW(nullptr);
    plain.lpszClassName = L"CopilotReel.ListViewportTest";
    Check(RegisterClassExW(&plain) != 0, "test window class");
    // Rows of 34 DIP at 100%, 200% and 250%, with viewports of exactly five rows and a pixel or two
    // off, as DIP rounding leaves them; 2000 rows would need a 136000 px LISTBOX the old way.
    for (int row : {34, 68, 85})
        for (int view : {5 * row, 5 * row - 2, 5 * row + 2})
            for (int count : {0, 3, 5, 6, 7, 2000})
                Sweep(count, row, view);
    Wheel();
    std::cout << "List viewport tests passed: " << checks << " checks; 2000 rows stay within one viewport-and-a-row LISTBOX.\n";
    return 0;
}
