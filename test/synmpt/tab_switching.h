/* SPDX-License-Identifier: BSD-3-Clause */
#pragma once
#include "../../mptrack/WindowMessages.h"
#include "../../mptrack/View_pat.h"
#include "../../soundlib/mod_specifications.h"
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
// an isolated test process. Conversion fixtures are saved only in its output directory;
// no desktop input/capture or writes to the supplied reference song are used.
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
        const auto fileBytes=[](const mpt::PathString &path) {
            std::ifstream input(path.AsNative(),std::ios::binary);
            return std::string(std::istreambuf_iterator<char>(input),{});
        };
        for(const auto type:{MOD_TYPE_IT,MOD_TYPE_XM,MOD_TYPE_MOD,MOD_TYPE_S3M,MOD_TYPE_MPT})
        {
            auto fixture=std::make_unique<CSoundFile>(); fixture->Create(type,4);
            fixture->Patterns.Insert(0,64); fixture->Order().resize(1,0);
            fixture->m_nSamples=1; auto &sample=fixture->GetSample(1); sample.Initialize(type);
            sample.nLength=32; check(sample.AllocateSample()!=0,"allocate conversion fixture sample");
            for(int i=0;i<32;i++) sample.sample8()[i]=static_cast<int8>(i*4-64);
            auto &note=*fixture->Patterns[0].GetpModCommand(0,0); note.note=NOTE_MIDDLEC; note.instr=1;
            const auto extension=fixture->GetModSpecifications().GetFileExtension();
            const auto source=directory+P_("conversion-fixture.")+mpt::PathString::FromUnicode(extension);
            {
                std::ofstream output(source.AsNative(),std::ios::binary);
                check(type==MOD_TYPE_MOD?fixture->SaveMod(output):type==MOD_TYPE_XM?fixture->SaveXM(output):type==MOD_TYPE_S3M?fixture->SaveS3M(output):fixture->SaveIT(output,{}),"write conversion fixture");
            }
            const auto original=fileBytes(source);
            log << "CONVERSION OPEN: " << mpt::ToCharset(mpt::Charset::UTF8,extension) << '\n'; log.flush();
            theApp.m_synmptConversionReply=IDYES;
            auto *converted=dynamic_cast<CModDoc *>(theApp.OpenDocumentFile(source.ToCString()));
            check(converted!=nullptr,"accepted conversion opens through the document template"); drain();
            check(converted->GetSoundFile().m_synmptVersion==1 && converted->GetModType()==MOD_TYPE_MPT,"accepted conversion creates native identity");
            check(converted->IsModified() && converted->m_ShowSavedialog && !converted->m_bHasValidPath,"conversion requires Save As and remains modified");
            check(!mpt::PathCompareNoCase(converted->m_conversionSource,source),"conversion protects the original source path");
            check(converted->GetSoundFile().Patterns[0].GetpModCommand(0,0)->note==NOTE_MIDDLEC,"conversion preserves the fixture note");
            const auto output=source+P_(".synmpt"); check(converted->OnSaveDocument(output,true),"converted document saves through normal document save");
            check(original==fileBytes(source),"conversion and Save As leave the original bytes intact");
            converted->OnCloseDocument(); drain();
            auto *reopened=dynamic_cast<CModDoc *>(theApp.OpenDocumentFile(output.ToCString()));
            check(reopened && reopened->GetSoundFile().m_synmptVersion==1,"saved conversion reopens natively"); drain();
            reopened->OnCloseDocument(); drain();
            log << "PASS: convert/open/save/reopen " << mpt::ToCharset(mpt::Charset::UTF8,extension) << '\n'; log.flush();
        }
        const auto source=directory+P_("conversion-fixture.it");
        theApp.m_synmptConversionReply=IDNO;
        auto *unchanged=dynamic_cast<CModDoc *>(theApp.OpenDocumentFile(source.ToCString())); drain();
        check(unchanged && unchanged->GetModType()==MOD_TYPE_IT && !unchanged->GetSoundFile().m_synmptVersion && !unchanged->IsModified(),"No keeps the original format and clean state");
        unchanged->OnCloseDocument(); drain();
        const auto documentCount=theApp.GetOpenDocuments().size();
        theApp.m_synmptConversionReply=IDCANCEL;
        check(theApp.OpenDocumentFile(source.ToCString())==nullptr,"Cancel declines the open"); drain();
        check(theApp.GetOpenDocuments().size()==documentCount,"Cancel leaves no document or orphaned frame");
        theApp.m_synmptConversionReply=IDNO;
        log << "PASS: No and Cancel conversion choices preserve document lifecycle\n"; log.flush();
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
    } catch(CException *error) {
        TCHAR text[512]{}; error->GetErrorMessage(text,512); error->Delete();
        log << "FAIL: MFC exception during editor/conversion checks: " << mpt::ToCharset(mpt::Charset::UTF8,mpt::ToUnicode(CString(text))) << '\n'; log.flush(); return 1;
    } catch(const std::exception &error) { log << "FAIL: " << error.what() << '\n'; log.flush(); return 1; }
}
}
OPENMPT_NAMESPACE_END
