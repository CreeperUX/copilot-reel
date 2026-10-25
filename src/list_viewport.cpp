#include "list_viewport.h"

#include <algorithm>

namespace reel {
int ListViewport::ViewHeight() const {
    RECT client{};
    if (viewport) GetClientRect(viewport, &client);
    return client.bottom;
}

// Near the end the LISTBOX keeps its last page full and so clamps the top index; the shift is
// measured from the index it actually shows.
void ListViewport::Apply() {
    if (!list) return;
    const int wanted = offset / row;
    if (SendMessageW(list, LB_GETTOPINDEX, 0, 0) != wanted) SendMessageW(list, LB_SETTOPINDEX, wanted, 0);
    const auto top = SendMessageW(list, LB_GETTOPINDEX, 0, 0);
    const int shift = std::clamp(offset - static_cast<int>(top == LB_ERR ? 0 : top) * row, 0, row);
    // Moving the child keeps its drawn rows; only the strip it uncovers repaints.
    SetWindowPos(list, nullptr, 0, -shift, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    UpdateWindow(list);
}

void ListViewport::ScrollTo(int pixels) {
    if (!viewport) return;
    offset = std::clamp(pixels, 0, std::max(0, content - ViewHeight()));
    SCROLLINFO info{sizeof(info), SIF_POS};
    info.nPos = offset;
    SetScrollInfo(viewport, SB_VERT, &info, TRUE);
    Apply();
}

void ListViewport::Layout() {
    if (!viewport || !list) return;
    const int rows = std::max(0, static_cast<int>(SendMessageW(list, LB_GETCOUNT, 0, 0)));
    const int view = ViewHeight();
    content = std::max(view, rows * row);
    offset = std::clamp(offset, 0, content - view);
    wheel = 0;
    SCROLLINFO info{sizeof(info), SIF_RANGE | SIF_PAGE | SIF_POS, 0, content - 1, static_cast<UINT>(view), offset};
    SetScrollInfo(viewport, SB_VERT, &info, TRUE);
    // Read the width after the bar shows or hides.
    RECT client{};
    GetClientRect(viewport, &client);
    SetWindowPos(list, nullptr, 0, 0, client.right, view + row, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    Apply();
}

void ListViewport::RevealSelection() {
    if (!viewport || !list) return;
    const auto selected = SendMessageW(list, LB_GETCURSEL, 0, 0);
    if (selected == LB_ERR) return;
    const int top = static_cast<int>(selected) * row, view = ViewHeight();
    int target = offset;
    if (top < target) target = top;
    else if (top + row > target + view) target = top + row - view;
    ScrollTo(target);
}

void ListViewport::Wheel(int delta) {
    if (!viewport) return;
    UINT lines = 3;
    SystemParametersInfoW(SPI_GETWHEELSCROLLLINES, 0, &lines, 0);
    const int view = ViewHeight();
    const double notch = lines == WHEEL_PAGESCROLL ? view : std::min<double>(view, static_cast<double>(lines) * row);
    const double pixels = -static_cast<double>(delta) * notch / WHEEL_DELTA;
    if (pixels * wheel < 0) wheel = 0;  // a reversal drops the leftover
    wheel += pixels;
    const int step = static_cast<int>(wheel);
    wheel -= step;
    if (step) ScrollTo(offset + step);
}

void ListViewport::ScrollBar(int request) {
    if (!viewport) return;
    SCROLLINFO info{sizeof(info), SIF_ALL};
    GetScrollInfo(viewport, SB_VERT, &info);
    int target = offset;
    switch (request) {
    case SB_LINEUP: target -= row; break;
    case SB_LINEDOWN: target += row; break;
    case SB_PAGEUP: target -= static_cast<int>(info.nPage); break;
    case SB_PAGEDOWN: target += static_cast<int>(info.nPage); break;
    case SB_THUMBTRACK: case SB_THUMBPOSITION: target = info.nTrackPos; break;
    case SB_TOP: target = 0; break;
    case SB_BOTTOM: target = content; break;
    default: return;
    }
    ScrollTo(target);
}

int ListViewport::PageSelection(bool down) const {
    const int count = list ? static_cast<int>(SendMessageW(list, LB_GETCOUNT, 0, 0)) : 0;
    if (count <= 0) return -1;
    const int rows = std::max(1, ViewHeight() / row - 1);
    const auto current = SendMessageW(list, LB_GETCURSEL, 0, 0);
    return std::clamp((current == LB_ERR ? 0 : static_cast<int>(current)) + (down ? rows : -rows), 0, count - 1);
}
}
