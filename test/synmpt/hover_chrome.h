/* SPDX-License-Identifier: BSD-3-Clause */
#pragma once
#include <stdexcept>

OPENMPT_NAMESPACE_BEGIN
namespace SynMPTChecks {
struct ChromeFrameState { HWND client=nullptr; int nativeHover=0; int nativeDraw=0; };
inline LRESULT CALLBACK ChromeFrameProc(HWND window,UINT message,WPARAM wParam,LPARAM lParam)
{
    if(message==WM_NCCREATE)::SetWindowLongPtr(window,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCT *>(lParam)->lpCreateParams));
    auto *state=reinterpret_cast<ChromeFrameState *>(::GetWindowLongPtr(window,GWLP_USERDATA));
    if(state && message==WM_NCMOUSEMOVE)state->nativeHover++;
    if(state && message==WM_DRAWITEM)state->nativeDraw++;
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
    ::DestroyWindow(frame);::UnregisterClassW(childType.lpszClassName,AfxGetInstanceHandle());::UnregisterClassW(frameType.lpszClassName,AfxGetInstanceHandle());
    log<<"PASS: native maximized MDI button rendering/command identity, caption/selector hover ownership and instrument selection stability\n";
}
}
OPENMPT_NAMESPACE_END
