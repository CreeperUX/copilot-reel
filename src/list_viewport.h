#pragma once
#include <windows.h>

// A LISTBOX that scrolls by pixels. A LISTBOX scrolls by whole rows only, which turns a precision
// touchpad's small wheel deltas into row-sized jumps, and a window taller than 32767 px fails, which
// a LISTBOX as tall as all its rows reaches at about 480 rows of 34 DIP at 200%. So the LISTBOX is
// one row taller than a clipping viewport window that owns the vertical scroll bar: its top index
// shows the whole rows of the pixel offset and its position in the viewport the rest. The LISTBOX
// keeps selection, keys, double-click and accessibility.
namespace reel {
struct ListViewport {
    HWND viewport = nullptr, list = nullptr;
    int row = 1;        // row height, pixels
    int content = 0;    // all rows' height, at least the viewport's
    int offset = 0;     // content pixels above the viewport
    double wheel = 0;   // wheel travel not yet applied, a fraction of a pixel

    int ViewHeight() const;
    // After the rows or the viewport change: the scroll range follows the rows, and the LISTBOX
    // stays one row taller than the viewport and as wide as its client area.
    void Layout();
    // Shows the rows from `pixels` down, clamped to the content.
    void ScrollTo(int pixels);
    // Brings the selected row wholly into view. Also realigns the list after the LISTBOX scrolled
    // itself to follow a key.
    void RevealSelection();
    // Moves by a WM_MOUSEWHEEL delta's share of the system's lines per notch, so a touchpad's small
    // deltas move a few pixels and a mouse notch still moves three rows.
    void Wheel(int delta);
    // Answers a WM_VSCROLL request from the viewport's scroll bar.
    void ScrollBar(int request);
    // The row one viewport page below (or above) the selection, for PageDown and PageUp; the
    // LISTBOX's own page would count the extra row nobody sees. -1 for an empty list.
    int PageSelection(bool down) const;

private:
    void Apply();
};
}
