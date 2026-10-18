/* SPDX-License-Identifier: BSD-3-Clause */
#pragma once

OPENMPT_NAMESPACE_BEGIN
namespace SynMPTChecks {
// Render only our application's paint handlers into an owned bitmap. Repainting
// a container after its contents must not erase those contents, even when Windows
// invalidates just that container during hover, resize or a tab transition.
struct PaintSurface
{
    HDC dc = ::CreateCompatibleDC(nullptr);
    HBITMAP bitmap = nullptr;
    HGDIOBJ previous = nullptr;
    RECT bounds{};
    static constexpr COLORREF marker = RGB(217, 37, 183);
    PaintSurface(int width, int height) : bounds{0, 0, width, height}
    {
        BITMAPINFO info{}; info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = width; info.bmiHeader.biHeight = -height;
        info.bmiHeader.biPlanes = 1; info.bmiHeader.biBitCount = 32;
        void *pixels = nullptr;
        bitmap = ::CreateDIBSection(dc, &info, DIB_RGB_COLORS, &pixels, nullptr, 0);
        if(!dc || !bitmap || !pixels) throw std::runtime_error("allocate paint-ownership bitmap");
        previous = ::SelectObject(dc, bitmap);
        ::SetDCBrushColor(dc, marker); ::FillRect(dc, &bounds, GetStockBrush(DC_BRUSH));
    }
    ~PaintSurface() { ::SelectObject(dc, previous); ::DeleteObject(bitmap); ::DeleteDC(dc); }
    bool Preserved(POINT point) const { return ::GetPixel(dc, point.x, point.y) == marker; }
};

inline bool CheckEditorPaintOwnership(CModControlView &control, CChildFrame &frame, std::ostream &log)
{
    bool pass = true;
    const auto test = [&](bool ok, const char *name) { if(!ok) { pass = false; log << "PAINT FAILURE: " << name << '\n'; } };
    const auto visible = [](HWND window) { return (::GetWindowLongPtr(window, GWL_STYLE) & WS_VISIBLE) != 0; };
    const auto rectIn = [](HWND window, HWND parent) {
        RECT rect{}; ::GetWindowRect(window, &rect); ::MapWindowPoints(nullptr, parent, reinterpret_cast<POINT *>(&rect), 2); return CRect(rect);
    };
    const auto splitter = ::GetParent(control.m_hWnd);
    RECT bounds{}; ::GetClientRect(splitter, &bounds);
    PaintSurface splitPaint(bounds.right, bounds.bottom);
    ::SendMessage(splitter, WM_ERASEBKGND, reinterpret_cast<WPARAM>(splitPaint.dc), 0);
    ::SendMessage(splitter, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(splitPaint.dc), PRF_CLIENT);
    for(auto pane : {control.m_hWnd, frame.GetHwndView()})
    {
        const auto rect = rectIn(pane, splitter);
        for(POINT point : {POINT{rect.left + 8, rect.top + 8}, POINT{rect.CenterPoint()}})
            test(splitPaint.Preserved(point), "splitter erased an editor/tab area");
    }

    const auto page = control.GetCurrentControlDlg()->m_hWnd;
    const auto tab = ::GetDlgItem(control.m_hWnd, IDC_TABCTRL1);
    bool pageAboveTab = false;
    for(auto sibling = ::GetWindow(tab, GW_HWNDPREV); sibling; sibling = ::GetWindow(sibling, GW_HWNDPREV))
        if(sibling == page) pageAboveTab = true;
    test(pageAboveTab, "active page is behind its overlapping tab control");
    ::GetClientRect(tab, &bounds);
    PaintSurface tabPaint(bounds.right, bounds.bottom);
    ::SendMessage(tab, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(tabPaint.dc), PRF_CLIENT);
    test(tabPaint.Preserved(rectIn(page, tab).CenterPoint()), "tab background erased the active page");
    RECT item{}; TabCtrl_GetItemRect(tab, 0, &item);
    test(!tabPaint.Preserved(POINT{item.left + 4, item.top + 4}), "tab label area was not painted");

    unsigned groups = 0, protectedControls = 0;
    for(auto owner : {page, frame.GetHwndView()})
    {
        for(auto group = ::GetWindow(owner, GW_CHILD); group; group = ::GetWindow(group, GW_HWNDNEXT))
        {
            wchar_t cls[64]{}; ::GetClassNameW(group, cls, 64);
            if(!visible(group) || std::wstring(cls) != L"Button" || (::GetWindowLongPtr(group, GWL_STYLE) & BS_TYPEMASK) != BS_GROUPBOX) continue;
            ::GetClientRect(group, &bounds);
            if(bounds.right < 24 || bounds.bottom < 24) continue;
            PaintSurface groupPaint(bounds.right, bounds.bottom);
            ::SendMessage(group, WM_PRINTCLIENT, reinterpret_cast<WPARAM>(groupPaint.dc), PRF_CLIENT);
            groups++;
            CRect interior(bounds); interior.DeflateRect(4, 4); interior.top += 16;
            for(auto sibling = ::GetWindow(owner, GW_CHILD); sibling; sibling = ::GetWindow(sibling, GW_HWNDNEXT))
            {
                ::GetClassNameW(sibling, cls, 64);
                if(sibling == group || !visible(sibling) || std::wstring(cls) == L"SysTabControl32"
                    || (std::wstring(cls) == L"Button" && (::GetWindowLongPtr(sibling, GWL_STYLE) & BS_TYPEMASK) == BS_GROUPBOX)) continue;
                const auto rect = rectIn(sibling, group);
                if(!interior.PtInRect(rect.CenterPoint())) continue;
                protectedControls++;
                if(!groupPaint.Preserved(rect.CenterPoint()))
                {
                    pass = false;
                    log << "PAINT FAILURE: group " << ::GetDlgCtrlID(group) << " erased control " << ::GetDlgCtrlID(sibling) << '\n';
                }
            }
        }
    }
    log << "Paint ownership: editor " << static_cast<int>(control.GetActivePage()) << ", " << groups << " group frames, " << protectedControls << " enclosed controls; " << (pass ? "PASS" : "FAIL") << '\n';
    log.flush(); return pass;
}
}
OPENMPT_NAMESPACE_END
