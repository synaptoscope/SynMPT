/* SPDX-License-Identifier: BSD-3-Clause */
#pragma once
#include <stdexcept>

OPENMPT_NAMESPACE_BEGIN
namespace SynMPTChecks {
struct ChromeFrameState
{
    HWND client=nullptr;
    int nativeHover=0, nativeDraw=0, captionPaint=0, nativeButtonDown=0, uahPaint=0, captionCursor=0;
    bool probeCaption=false, activationPaint=false;
    UINT command=0;
};
inline LRESULT CALLBACK ChromeFrameProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam)
{
    if(message==WM_NCCREATE)::SetWindowLongPtr(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCT *>(lParam)->lpCreateParams));
    auto *state=reinterpret_cast<ChromeFrameState *>(::GetWindowLongPtr(window,GWLP_USERDATA));
    if(state && message==WM_NCMOUSEMOVE)state->nativeHover++;
    if(state && message==WM_DRAWITEM)state->nativeDraw++;
    if(state && state->probeCaption)
    {
        if(message==0x00AE || message==0x00AF) state->uahPaint++;
        if(message==WM_SETCURSOR && LOWORD(lParam)==HTCAPTION) state->captionCursor++;
        if(message == WM_NCPAINT)
        {
            TITLEBARINFOEX title{}; title.cbSize=sizeof(title);
            ::SendMessage(window,WM_GETTITLEBARINFOEX,0,reinterpret_cast<LPARAM>(&title));
            if(wParam == 1 || (wParam && ::RectInRegion(reinterpret_cast<HRGN>(wParam), &title.rcTitleBar))) state->captionPaint++;
        }
        // MFC's CFrameWnd rewrites WM_NCACTIVATE's lParam to zero. Reproduce
        // that here; merely forwarding the documented -1 parameter is insufficient.
        if(message == WM_NCACTIVATE)
        {
            state->activationPaint = (::GetWindowLongPtr(window,GWL_STYLE) & WS_VISIBLE) != 0;
            return ::DefWindowProc(window,message,wParam,0);
        }
        if((message == WM_NCLBUTTONDOWN || message == WM_NCLBUTTONDBLCLK)
            && (wParam == HTMINBUTTON || wParam == HTMAXBUTTON || wParam == HTCLOSE))
        { state->nativeButtonDown++; return 0; } // Don't enter a blocking native tracking loop in the fixture.
        if(message == WM_SYSCOMMAND) { state->command = static_cast<UINT>(wParam) & 0xFFF0; return 0; }
    }
    return state && state->client ? ::DefFrameProcW(window,state->client,message,wParam,lParam) : ::DefWindowProcW(window,message,wParam,lParam);
}
inline LRESULT CALLBACK ChromeComboProbe(HWND window,UINT message,WPARAM wParam,LPARAM lParam,UINT_PTR,DWORD_PTR data)
{
    if(message==WM_MOUSEMOVE)(*reinterpret_cast<int *>(data))++;
    return ::DefSubclassProc(window,message,wParam,lParam);
}
inline void CheckHoverChrome(std::ostream &log)
{
    auto check=[](bool pass,const char *message){if(!pass)throw std::runtime_error(message);};
    WNDCLASSW frameType{};frameType.lpfnWndProc=ChromeFrameProc;frameType.hInstance=AfxGetInstanceHandle();frameType.lpszClassName=L"Afx.SynMPTChecks.Frame";
    check(::RegisterClassW(&frameType)!=0,"register native MDI fixture frame");
    auto childType=frameType;childType.lpfnWndProc=::DefMDIChildProcW;childType.lpszClassName=L"Afx.SynMPTChecks.Child";
    check(::RegisterClassW(&childType)!=0,"register native MDI fixture document");
    ChromeFrameState state;
    const auto frame=::CreateWindowExW(0,frameType.lpszClassName,L"Hover fixture",WS_OVERLAPPEDWINDOW,0,0,500,250,nullptr,nullptr,AfxGetInstanceHandle(),&state);
    check(frame!=nullptr,"create hidden native MDI frame");
    const auto menu=::CreateMenu(),fileMenu=::CreatePopupMenu(),windowMenu=::CreatePopupMenu();
    ::AppendMenuW(fileMenu,MF_STRING,101,L"New");::AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(fileMenu),L"File");
    ::AppendMenuW(menu,MF_POPUP,reinterpret_cast<UINT_PTR>(windowMenu),L"Window");::SetMenu(frame,menu);
    CLIENTCREATESTRUCT clientInfo{windowMenu,0x9000};
    state.client=::CreateWindowExW(0,L"MDICLIENT",L"",WS_CHILD|WS_VISIBLE,0,0,480,210,frame,reinterpret_cast<HMENU>(1),AfxGetInstanceHandle(),&clientInfo);
    check(state.client!=nullptr,"create native MDI client");
    MDICREATESTRUCTW create{};create.szClass=childType.lpszClassName;create.szTitle=L"Document";create.hOwner=AfxGetInstanceHandle();
    create.style=WS_OVERLAPPEDWINDOW|WS_VISIBLE|WS_MAXIMIZE;
    const auto child=reinterpret_cast<HWND>(::SendMessageW(state.client,WM_MDICREATE,0,reinterpret_cast<LPARAM>(&create)));
    check(child!=nullptr,"create maximized native MDI document");::SendMessage(state.client,WM_MDIMAXIMIZE,reinterpret_cast<WPARAM>(child),0);
    std::vector<std::pair<int,UINT>> buttons;
    for(int item=0;item<::GetMenuItemCount(::GetMenu(frame));item++)
    {
        MENUITEMINFOW info{};info.cbSize=sizeof(info);info.fMask=MIIM_FTYPE|MIIM_BITMAP|MIIM_ID;
        ::GetMenuItemInfoW(::GetMenu(frame),item,TRUE,&info);
        const auto bitmap=reinterpret_cast<UINT_PTR>(info.hbmpItem);
        if(bitmap==2 || bitmap==3 || bitmap==5)
        {check((info.fType&MFT_OWNERDRAW)!=0,"reproduce Windows' pre-existing owner-drawn MDI buttons");buttons.emplace_back(item,info.wID);}
    }
    check(buttons.size()==3,"reproduce all three native maximized-document buttons");
    const auto combo=::CreateWindowExW(0,L"COMBOBOX",L"",WS_CHILD|WS_VISIBLE|CBS_DROPDOWNLIST,0,0,220,180,child,reinterpret_cast<HMENU>(20),AfxGetInstanceHandle(),nullptr);
    check(combo!=nullptr,"create instrument selector fixture");::SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(L"No Instrument"));::SendMessage(combo,CB_SETCURSEL,0,0);
    const auto font=::GetStockObject(DEFAULT_GUI_FONT);::SendMessage(combo,WM_SETFONT,reinterpret_cast<WPARAM>(font),0);
    int nativeMoves=0;::SetWindowSubclass(combo,ChromeComboProbe,0x53435052,reinterpret_cast<DWORD_PTR>(&nativeMoves));
    SynMPTTheme::RegisterWindow(frame);
    const auto dc=::CreateCompatibleDC(nullptr);BITMAPINFO image{};image.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    image.bmiHeader.biWidth=24;image.bmiHeader.biHeight=-24;image.bmiHeader.biPlanes=1;image.bmiHeader.biBitCount=32;
    void *pixels=nullptr;const auto bitmap=::CreateDIBSection(dc,&image,DIB_RGB_COLORS,&pixels,nullptr,0);
    check(dc && bitmap && pixels,"allocate native MDI button render fixture");const auto previous=::SelectObject(dc,bitmap);
    for(const auto &[index,command]:buttons)
    {
        MENUITEMINFOW info{};info.cbSize=sizeof(info);info.fMask=MIIM_FTYPE|MIIM_ID|MIIM_DATA;::GetMenuItemInfoW(::GetMenu(frame),index,TRUE,&info);
        check(info.wID==command && info.dwItemData!=0,"theme claims native MDI drawing without changing its command");
        state.nativeDraw=0;
        DRAWITEMSTRUCT draw{};draw.CtlType=ODT_MENU;draw.itemID=command;draw.itemAction=ODA_DRAWENTIRE;draw.hwndItem=reinterpret_cast<HWND>(::GetMenu(frame));
        draw.hDC=dc;draw.rcItem={0,0,24,24};draw.itemData=info.dwItemData;
        check(::SendMessageW(frame,WM_DRAWITEM,0,reinterpret_cast<LPARAM>(&draw))!=0,"native MDI buttons use the theme's menu paint path");::GdiFlush();
        check(state.nativeDraw==0,"native frame-theme painting does not intercept themed MDI buttons");
        check(::GetPixel(dc,2,2)==SynMPTTheme::Color(COLOR_APPWORKSPACE),"all native MDI document buttons have a dark background");
    }
    ::SelectObject(dc,previous);::DeleteObject(bitmap);::DeleteDC(dc);
    state.nativeHover=0;
    for(int step=0;step<32;step++)
    {
        ::SendMessage(frame,WM_NCMOUSEMOVE,HTCAPTION,MAKELPARAM(30+step,10));
        ::SendMessage(combo,WM_MOUSEMOVE,0,MAKELPARAM(20+step,8));
    }
    ::SendMessage(frame,WM_NCMOUSELEAVE,0,0);::SendMessage(combo,WM_MOUSELEAVE,0,0);
    check(state.nativeHover==0 && nativeMoves==0,"themed title/selector hover does not invoke competing native text painters");
    check(::SendMessage(combo,CB_GETCURSEL,0,0)==0,"hover leaves the instrument selection unchanged");
    ::RemoveWindowSubclass(combo,ChromeComboProbe,0x53435052);
    // No desktop input or capture: exercise the caption state machine on owned
    // native main/tool windows. The lower probe deliberately rewrites activation
    // like MFC and prevents accidental blocking/native button tracking.
    const auto checkCaption = [&](HWND target, ChromeFrameState &probe, bool tool) {
        probe.probeCaption=true;
        ::SendMessage(target,WM_NCPAINT,1,0);
        const bool exclusivePaint=probe.captionPaint==0;
        log << "Caption painter exclusions: " << (exclusivePaint ? "PASS" : "FAIL") << '\n';log.flush();
        const auto style=::GetWindowLongPtr(target,GWL_STYLE);
        ::SetWindowLongPtr(target,GWL_STYLE,style | WS_VISIBLE);
        ::SendMessage(target,WM_NCACTIVATE,TRUE,0);
        const bool visibilityRestored=(::GetWindowLongPtr(target,GWL_STYLE)&WS_VISIBLE)!=0;
        ::SetWindowLongPtr(target,GWL_STYLE,style);
        log << "MFC activation repaint suppression: " << (!probe.activationPaint && visibilityRestored ? "PASS" : "FAIL") << '\n';log.flush();
        check(exclusivePaint,"native nonclient repaint must exclude the custom caption and buttons");
        check(!probe.activationPaint && visibilityRestored,"MFC activation may not repaint the classic caption; visibility must be restored");
        ::SendMessage(target,WM_NCACTIVATE,FALSE,0);
        check((::GetWindowLongPtr(target,GWL_STYLE)&WS_VISIBLE)==(style&WS_VISIBLE),"activation preserves initially hidden windows");

        TITLEBARINFOEX title{};title.cbSize=sizeof(title);::SendMessage(target,WM_GETTITLEBARINFOEX,0,reinterpret_cast<LPARAM>(&title));
        const auto dispatchCommand = [&] {
            MSG queued{};
            while(::PeekMessage(&queued,target,WM_SYSCOMMAND,WM_SYSCOMMAND,PM_REMOVE)) ::DispatchMessage(&queued);
        };
        const auto pressButton = [&](int part,int hit) {
            const auto screen=CRect(title.rgrect[part]).CenterPoint();
            ::SendMessage(target,WM_NCLBUTTONDOWN,hit,MAKELPARAM(screen.x,screen.y));
            check(::GetCapture()==target,"themed caption button owns mouse capture");
            POINT client=screen;::ScreenToClient(target,&client);return client;
        };
        for(auto [part,hit,command] : {std::tuple{2,HTMINBUTTON,SC_MINIMIZE},std::tuple{3,HTMAXBUTTON,SC_MAXIMIZE},std::tuple{5,HTCLOSE,SC_CLOSE}})
        {
            if(tool && part!=5) continue;
            probe.command=0;
            const auto client=pressButton(part,hit);
            ::SendMessage(target,WM_LBUTTONUP,0,MAKELPARAM(client.x,client.y));dispatchCommand();
            check(probe.command==static_cast<UINT>(command),"themed caption button invokes its original system command");
            check(::GetCapture()!=target,"caption button releases mouse capture after command");
        }
        // Release away from the button, Escape and capture loss must not issue Close.
        for(int cancel : {0,1,2,3})
        {
            probe.command=0;pressButton(5,HTCLOSE);
            if(cancel==0) ::SendMessage(target,WM_LBUTTONUP,0,MAKELPARAM(20,40));
            if(cancel==1)
            {
                MSG escape{}; escape.hwnd=combo; escape.message=WM_KEYDOWN; escape.wParam=VK_ESCAPE;
                check(SynMPTTheme::HandleKeyMessage(escape),"Escape from a focused child cancels caption tracking before editor shortcuts");
            }
            if(cancel==2) ::SendMessage(target,WM_CAPTURECHANGED,0,0);
            if(cancel==3) ::SendMessage(target,WM_CANCELMODE,0,0);
            dispatchCommand();
            check(probe.command==0 && ::GetCapture()!=target,"cancelled caption button never closes the window or keeps capture");
        }
        // Native movement/layout notifications must not reopen the classic painter.
        ::SendMessage(target,WM_ENTERSIZEMOVE,0,0);
        ::SendMessage(target,0x00AE,0,0); ::SendMessage(target,0x00AF,0,0);
        ::SendMessage(target,WM_SETCURSOR,reinterpret_cast<WPARAM>(target),MAKELPARAM(HTCAPTION,WM_MOUSEMOVE));
        check(probe.uahPaint==0 && probe.captionCursor==0,"native caption theme/cursor paths remain excluded during movement");
        ::SetWindowPos(target,nullptr,30,40,500,250,SWP_NOZORDER|SWP_NOACTIVATE|SWP_FRAMECHANGED);
        ::SendMessage(target,WM_NCPAINT,1,0);
        ::SendMessage(target,WM_EXITSIZEMOVE,0,0);
        check(probe.captionPaint==0 && probe.nativeButtonDown==0,"move/size and button tracking retain exclusive caption painting");
        probe.probeCaption=false;
        log << "PASS: " << (tool ? "floating tool" : "main frame") << " caption paint isolation, MFC activation, button commands, release-away/Escape/capture-loss cancellation and move/size notifications\n";
    };
    checkCaption(frame,state,false);
    ChromeFrameState toolState;
    const auto tool=::CreateWindowExW(WS_EX_TOOLWINDOW,frameType.lpszClassName,L"Transport fixture",WS_POPUP|WS_CAPTION|WS_SYSMENU,
        0,0,400,80,frame,nullptr,AfxGetInstanceHandle(),&toolState);
    check(tool!=nullptr,"create owned floating caption fixture");
    SynMPTTheme::RegisterWindow(tool);checkCaption(tool,toolState,true);::DestroyWindow(tool);
    ::DestroyWindow(frame);::UnregisterClassW(childType.lpszClassName,AfxGetInstanceHandle());::UnregisterClassW(frameType.lpszClassName,AfxGetInstanceHandle());
    log<<"PASS: native maximized MDI button rendering/command identity, caption/selector hover ownership and instrument selection stability\n";
}
}
OPENMPT_NAMESPACE_END
