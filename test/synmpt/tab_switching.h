/* SPDX-License-Identifier: BSD-3-Clause */
#pragma once
#include "../../mptrack/WindowMessages.h"
#include "../../mptrack/View_pat.h"
#include <chrono>
#include "paint_ownership.h"

OPENMPT_NAMESPACE_BEGIN
namespace SynMPTChecks {
struct ScrollRedrawProbe
{
    unsigned positions = 0, styles = 0;
    static LRESULT CALLBACK Proc(HWND window, UINT message, WPARAM wParam, LPARAM lParam, UINT_PTR, DWORD_PTR data)
    {
        auto &probe = *reinterpret_cast<ScrollRedrawProbe *>(data);
        if(message == WM_WINDOWPOSCHANGED) probe.positions++;
        if(message == WM_STYLECHANGED) probe.styles++;
        return ::DefSubclassProc(window, message, wParam, lParam);
    }
};

// Exercises the actual upper pages and dynamically replaced MFC lower views in
// an isolated test process. No UI input, desktop capture or file saving is used.
inline int RunEditorSwitches(CMainFrame &main, const mpt::PathString &directory)
{
    mpt::IO::ofstream log(directory + P_("synmpt-editor-checks.log"), std::ios::binary);
    try
    {
        const auto check = [](bool pass, const char *why) { if(!pass) throw std::runtime_error(why); };
        const auto drain = [&] {
            MSG message{}; unsigned count = 0;
            const auto start = std::chrono::steady_clock::now();
            while(::PeekMessage(&message, nullptr, 0, 0, PM_REMOVE))
            {
                check(message.message != WM_QUIT && ++count < 10000
                    && std::chrono::steady_clock::now() - start < std::chrono::seconds(5), "editor message queue must settle");
                ::TranslateMessage(&message); ::DispatchMessage(&message);
            }
        };
        drain();
        auto *frame = dynamic_cast<CChildFrame *>(main.MDIGetActive());
        check(frame != nullptr, "reference document must have an MDI frame");
        auto *control = frame->GetModControlView();
        auto *document = control->GetDocument();
        const auto tab = ::GetDlgItem(control->m_hWnd, IDC_TABCTRL1);
        check(tab && TabCtrl_GetItemCount(tab) == 5, "reference module provides all five editors");
        check((::GetWindowLongPtr(tab, GWL_STYLE) & WS_CLIPSIBLINGS) != 0
            && (control->GetStyle() & WS_CLIPCHILDREN) != 0, "editor tab and page backgrounds clip sibling/child controls");
        const bool modified = document->IsModified() != FALSE;
        const auto choose = [&](int index) {
            TabCtrl_SetCurSel(tab, index);
            NMHDR event{tab, IDC_TABCTRL1, TCN_SELCHANGE};
            ::SendMessage(control->m_hWnd, WM_NOTIFY, IDC_TABCTRL1, reinterpret_cast<LPARAM>(&event));
        };
        const auto verify = [&] {
            auto *page = control->GetCurrentControlDlg();
            auto *view = dynamic_cast<CView *>(CWnd::FromHandlePermanent(frame->GetHwndView()));
            check(page && view && view->IsKindOf(page->GetAssociatedViewClass()), "tab and lower editor must agree");
            check(page->GetViewWnd() == view->m_hWnd, "page messages target its matching editor");
            check(view->GetDocument() == document, "editor keeps the same document");
            check((view->GetParent()->GetStyle() & WS_VISIBLE) != 0, "splitter redraw/visibility restored after replacement");
            CRect bounds; view->GetClientRect(bounds);
            check(bounds.Width() > 0 && bounds.Height() > 0, "replacement editor has a usable layout");
            check((page->GetStyle() & WS_VISIBLE) != 0, "selected upper page is shown");
            view->UpdateWindow(); page->UpdateWindow();
            check((document->IsModified() != FALSE) == modified, "switching editors does not change the song");
        };
        double worst = 0;
        bool paintOwnership = true;
        for(int round = 0; round < 8; round++) for(int index = 0; index < 5; index++)
        {
            const auto start = std::chrono::steady_clock::now();
            choose(index); drain(); verify();
            if(round == 0) paintOwnership = CheckEditorPaintOwnership(*control, *frame, log) && paintOwnership;
            const auto elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
            worst = std::max(worst, elapsed);
            if(round == 0) { log << "FIRST OPEN: tab " << index << " " << elapsed << " ms\n"; log.flush(); }
        }
        for(int round = 0; round < 8; round++)
        {
            choose(0); choose(2); choose(1); drain(); verify();
            check(control->GetActivePage() == CModControlView::Page::Patterns, "rapid changes settle on the final requested tab");
        }
        check(paintOwnership, "editor backgrounds must preserve the controls and tabs they overlap");
        log << "PASS: 40 settled switches and 24 queued rapid switches across Song/Patterns/Samples/Instruments/Comments; correct views, layouts, visibility and unchanged song\n";
        log.flush();
        auto *scratch=theApp.NewDocument(MOD_TYPE_MPT);
        check(scratch!=nullptr,"create disposable composer document"); drain();
        auto *scratchFrame=dynamic_cast<CChildFrame *>(main.MDIGetActive());
        check(scratchFrame!=nullptr,"create composer document frame");
        scratchFrame->ActivateView(IDD_CONTROL_PATTERNS,-1); drain();
        auto *pattern=dynamic_cast<CViewPattern *>(CWnd::FromHandlePermanent(scratchFrame->GetHwndView()));
        check(pattern!=nullptr,"create composer pattern view"); pattern->RunComposerChecks(directory);
        log << "PASS: empty document, track creation, multi-track grouping undo/redo, embedded preset save/recall, independent samples, fixed key metadata, consecutive steps, ties, unique master, auto/manual colors, detached sample/instrument views, final track deletion and undo\n";
        log << "Longest settled switch: " << worst << " ms (hidden test process, not a desktop smoothness measurement)\nALL CHECKS PASSED\n";
        log.flush(); return 0;
    } catch(const std::exception &error) { log << "FAIL: " << error.what() << '\n'; log.flush(); return 1; }
}
}
OPENMPT_NAMESPACE_END
