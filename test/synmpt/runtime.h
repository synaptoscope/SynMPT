/* SPDX-License-Identifier: BSD-3-Clause */
#pragma once
#include "../../soundlib/Sndfile.h"
#include "../../mptrack/SynMPTTheme.h"
#include "../../mptrack/PatternFont.h"
#include "../../soundlib/TrackRouting.h"
#include "../../soundlib/CompositeInput.h"
#include "mpt/io_file/fstream.hpp"
#include "mpt/io_read/filecursor_stdstream.hpp"
#include <stdexcept>
#include <sstream>
#include <cmath>
#include "hover_chrome.h"
#include "tab_switching.h"

OPENMPT_NAMESPACE_BEGIN
namespace SynMPTChecks {
struct Fixture final : CSoundFile { Fixture() { InitializeGlobals(MOD_TYPE_MPT, 4); } };
struct EnergyTarget final : IAudioTarget
{
    double left=0, right=0;
    template <typename T> void Add(mpt::audio_span_interleaved<T> buffer)
    {
        for(size_t frame=0;frame<buffer.size_frames();frame++)
        {
            left+=std::abs(static_cast<double>(buffer(0,frame)));
            if(buffer.size_channels()>1) right+=std::abs(static_cast<double>(buffer(1,frame)));
        }
    }
    void Process(mpt::audio_span_interleaved<MixSampleInt> buffer) override { Add(buffer); }
    void Process(mpt::audio_span_interleaved<MixSampleFloat> buffer) override { Add(buffer); }
};
inline int Run(const mpt::PathString &directory, const std::vector<mpt::PathString> &songs)
{
    mpt::IO::ofstream log(directory + P_("synmpt-runtime-checks.log"),std::ios::binary);
    try {
        auto check=[](bool pass,const char *why){if(!pass) throw std::runtime_error(why);};
        // These render into an owned memory bitmap; no desktop screenshot or user file is read.
        {
            BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth=32;info.bmiHeader.biHeight=-24;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
            void *pixels=nullptr;const auto dc=::CreateCompatibleDC(nullptr);
            const auto bitmap=::CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
            check(dc && bitmap && pixels,"appearance memory bitmap allocation");
            const auto oldBitmap=::SelectObject(dc,bitmap);
            const auto pixel=[&](int x,int y){return ::GetPixel(dc,x,y);};
            CFastBitmap image;image.Init();image.SetSize(8,2);
            image.SetColor(MODCOLOR_BACKNORMAL,RGB(40,40,40));image.SetColor(MODCOLOR_NOTE,RGB(220,220,220));
            image.SetColor(MODCOLOR_BACKSELECTED,RGB(60,100,140));image.FillRect(0,0,8,2,MODCOLOR_BACKNORMAL);
            const std::vector<BitmapTintSpan> tints{{0,4,RGB(220,60,60)}};
            image.Blit(dc,0,0,8,2,tints);::GdiFlush();
            check(GetRValue(pixel(0,0))>40 && pixel(4,0)==RGB(40,40,40),"track tint stays inside its owning column");
            image.FillRect(0,0,4,1,MODCOLOR_BACKSELECTED);image.Blit(dc,0,0,8,2,tints);::GdiFlush();
            check(pixel(0,0)==RGB(60,100,140),"selected cells retain their selection color over track tint");
            uint8 glyphPixels[]{0x08,0xFF};MODPLUGDIB glyph{};glyph.bmiHeader.biWidth=4;glyph.bmiHeader.biHeight=-1;
            glyph.lpDibBits=glyphPixels;glyph.antialiased=true;
            image.SetTextColor(MODCOLOR_NOTE,MODCOLOR_BACKNORMAL);image.TextBlt(0,0,4,1,0,0,&glyph);
            image.Blit(dc,0,0,8,2,tints);::GdiFlush();
            check(pixel(0,0)==RGB(220,220,220),"syntax-colored text is not recolored by track tint");
            check(GetRValue(pixel(1,0))>GetRValue(pixel(2,0)) && GetRValue(pixel(1,0))<220,"glyph edges blend against the tinted track background");
            image.FillRect(0,0,4,1,MODCOLOR_BACKNORMAL);image.Blit(dc,0,0,8,2);::GdiFlush();
            check(pixel(1,0)==RGB(40,40,40),"redrawing a cell clears old glyph coverage");
            RECT action{0,0,32,24};SynMPTTheme::DrawAction(dc,action,L"M",RGB(104,169,224),true);::GdiFlush();
            check(pixel(3,3)==RGB(104,169,224),"mute fills the whole active button blue");
            SynMPTTheme::DrawAction(dc,action,L"S",RGB(222,191,79),true);::GdiFlush();
            check(pixel(3,3)==RGB(222,191,79),"solo fills the whole active button yellow");
            SynMPTTheme::DrawAction(dc,action,L"",RGB(198,58,80),true,true);::GdiFlush();
            check(pixel(3,3)==RGB(198,58,80) && pixel(16,12)==RGB(255,237,239),"armed record has a red body and round contrasting dot");
            SynMPTTheme::DrawAction(dc,action,L"",RGB(198,58,80),false,true);::GdiFlush();
            check(pixel(3,3)==SynMPTTheme::Color(COLOR_BTNFACE) && pixel(16,12)==RGB(174,53,70),"unarmed record retains its red dot on the neutral button");
            // Rounded skin uses the native thumb bounds in either orientation.
            ::FillRect(dc, &action, SynMPTTheme::Brush(COLOR_BTNFACE));
            SCROLLBARINFO scroll{}; scroll.cbSize=sizeof(scroll);scroll.rcScrollBar={0,0,16,24};
            scroll.xyThumbTop=3;scroll.xyThumbBottom=21;
            SynMPTTheme::DrawScrollBar(dc,scroll,true,false);::GdiFlush();
            check(pixel(8,12)==RGB(94,102,115) && pixel(4,4)==SynMPTTheme::Color(COLOR_APPWORKSPACE),"vertical scrollbar has a rounded inset thumb");
            check(pixel(17,12)==SynMPTTheme::Color(COLOR_BTNFACE),"scrollbar paint is clipped to its native bounds");
            scroll.rcScrollBar={0,0,32,16};scroll.xyThumbTop=4;scroll.xyThumbBottom=28;
            SynMPTTheme::DrawScrollBar(dc,scroll,false,true);::GdiFlush();
            check(pixel(16,8)==RGB(130,140,155) && pixel(5,4)==SynMPTTheme::Color(COLOR_APPWORKSPACE),"horizontal scrollbar rounds and responds to hover");
            scroll.rgstate[0]=STATE_SYSTEM_UNAVAILABLE;
            SynMPTTheme::DrawScrollBar(dc,scroll,false,true);::GdiFlush();
            check(pixel(16,8)==SynMPTTheme::Color(COLOR_APPWORKSPACE),"disabled scrollbar hides its unusable thumb");
            ::SelectObject(dc,oldBitmap);::DeleteObject(bitmap);::DeleteDC(dc);
        }
        {
            INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_BAR_CLASSES|ICC_STANDARD_CLASSES};::InitCommonControlsEx(&controls);
            const auto host=::CreateWindowExW(0,L"STATIC",L"Theme regression host",WS_POPUP,0,0,260,180,nullptr,nullptr,AfxGetInstanceHandle(),nullptr);
            check(host!=nullptr,"create owned hidden theme fixture");
            const auto label=::CreateWindowExW(0,L"STATIC",L"Sequence",WS_CHILD|WS_VISIBLE|WS_DISABLED|SS_LEFT,0,0,160,28,host,nullptr,AfxGetInstanceHandle(),nullptr);
            const auto status=::CreateWindowExW(0,STATUSCLASSNAMEW,L"",WS_CHILD|WS_VISIBLE|CCS_NORESIZE,0,40,240,28,host,nullptr,AfxGetInstanceHandle(),nullptr);
            const auto source=::CreateWindowExW(0,L"STATIC",L"",WS_CHILD|WS_VISIBLE|WS_HSCROLL|WS_VSCROLL,0,80,220,90,host,nullptr,AfxGetInstanceHandle(),nullptr);
            check(label && status && source,"create label/status/scroll fixtures");
            const auto font=::GetStockObject(DEFAULT_GUI_FONT);::SendMessage(label,WM_SETFONT,reinterpret_cast<WPARAM>(font),0);::SendMessage(status,WM_SETFONT,reinterpret_cast<WPARAM>(font),0);
            const int parts[]{-1};::SendMessage(status,SB_SETPARTS,1,reinterpret_cast<LPARAM>(parts));::SendMessageW(status,SB_SETTEXTW,0,reinterpret_cast<LPARAM>(L"Ready"));
            SCROLLINFO range{};range.cbSize=sizeof(range);range.fMask=SIF_RANGE|SIF_PAGE|SIF_POS;range.nMax=1000000;range.nPage=1000;range.nPos=400000;
            ::SetScrollInfo(source,SB_VERT,&range,TRUE);::SetScrollInfo(source,SB_HORZ,&range,TRUE);
            SynMPTTheme::RegisterWindow(host);
            const auto dc=::CreateCompatibleDC(nullptr);BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth=260;info.bmiHeader.biHeight=-40;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
            void *pixels=nullptr;const auto bitmap=::CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);check(dc && bitmap && pixels,"allocate native control test bitmap");
            const auto previous=::SelectObject(dc,bitmap);
            ::SendMessage(label,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);::GdiFlush();
            bool grayInk=false;
            for(int y=0;y<28;y++)for(int x=0;x<160;x++)
            {
                const auto color=::GetPixel(dc,x,y);
                check(color!=RGB(255,255,255),"disabled Sequence label does not emboss a white shadow");
                if(GetRValue(color)>100)grayInk=true;
            }
            check(grayInk,"disabled Sequence label still renders readable text");
            ::SendMessage(status,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);::GdiFlush();
            bool statusInk=false;for(int y=0;y<28;y++)for(int x=0;x<100;x++)if(GetRValue(::GetPixel(dc,x,y))>160)statusInk=true;
            check(statusInk,"status bar renders light Ready text");
            check(SynMPTTheme::Color(COLOR_ACTIVECAPTION)==SynMPTTheme::Color(COLOR_INACTIVECAPTION)
                && SynMPTTheme::Color(COLOR_ACTIVECAPTION)==SynMPTTheme::Color(COLOR_APPWORKSPACE),"active/inactive captions match the scrollbar gutter gray");
            struct SurfaceSearch{HWND owner;HWND surfaces[2]{};} search{source};
            ::EnumChildWindows(host,[](HWND child,LPARAM parameter)->BOOL{
                auto &found=*reinterpret_cast<SurfaceSearch *>(parameter);wchar_t cls[64]{};::GetClassNameW(child,cls,64);
                if(std::wstring(cls)==L"SynMPT.ScrollSurface" && reinterpret_cast<HWND>(::GetWindowLongPtr(child,GWLP_USERDATA))==found.owner)
                {const int axis=::GetDlgCtrlID(child)-0x7A10;if(axis>=0 && axis<2)found.surfaces[axis]=child;}return TRUE;
            },reinterpret_cast<LPARAM>(&search));
            for(auto surface:search.surfaces)
            {
                check(surface && ::IsWindow(surface),"each viewport scrollbar has its own persistent paint surface");
                check(::SendMessage(surface,WM_NCHITTEST,0,0)==HTTRANSPARENT,"scroll surface passes mouse hit testing to the native scrollbar");
                ::SendMessage(surface,WM_PRINTCLIENT,reinterpret_cast<WPARAM>(dc),PRF_CLIENT);::GdiFlush();
                check(::GetPixel(dc,0,0)==SynMPTTheme::Color(COLOR_APPWORKSPACE),"scroll surface paints its dark gutter");
            }
            ScrollRedrawProbe redraw;
            constexpr UINT_PTR redrawProbeID = 0x53524348;
            ::SetWindowSubclass(source, ScrollRedrawProbe::Proc, redrawProbeID, reinterpret_cast<DWORD_PTR>(&redraw));
            for(auto surface : search.surfaces)
                ::SetWindowSubclass(surface, ScrollRedrawProbe::Proc, redrawProbeID, reinterpret_cast<DWORD_PTR>(&redraw));
            for(int repeat = 0; repeat < 32; repeat++)
            {
                ::SendMessage(source, WM_SIZE, 0, MAKELPARAM(200, 70));
                ::SendMessage(source, WM_NCACTIVATE, TRUE, 0);
            }
            check(redraw.positions == 0 && redraw.styles == 0, "unchanged viewport redraws must not reposition scrollbar surfaces or rewrite window styles");
            log << "PASS: 64 unchanged viewport redraw/layout requests cause zero scrollbar repositionings and zero style rewrites\n";
            ::RemoveWindowSubclass(source, ScrollRedrawProbe::Proc, redrawProbeID);
            for(auto surface : search.surfaces) ::RemoveWindowSubclass(surface, ScrollRedrawProbe::Proc, redrawProbeID);
            // Native activation/range updates keep the same surfaces and full 32-bit positions.
            for(int position:{123456,800000})
            {
                range.nPos=position;::SetScrollInfo(source,SB_VERT,&range,TRUE);::SendMessage(source,WM_NCACTIVATE,TRUE,0);::SendMessage(source,WM_SIZE,0,MAKELPARAM(200,70));
                SCROLLINFO actual{};actual.cbSize=sizeof(actual);actual.fMask=SIF_POS;::GetScrollInfo(source,SB_VERT,&actual);
                check(actual.nPos==position && ::IsWindow(search.surfaces[1]),"native redraw/range changes retain the persistent surface and 32-bit position");
            }
            ::DestroyWindow(source);check(!::IsWindow(search.surfaces[0]) && !::IsWindow(search.surfaces[1]),"scroll surfaces are destroyed with their viewport");
            ::SelectObject(dc,previous);::DeleteObject(bitmap);::DeleteDC(dc);::DestroyWindow(host);
            log<<"PASS: plain disabled labels, readable status text, gray captions, persistent scrollbar surfaces, native hit-test passthrough, redraw lifecycle and 32-bit ranges\n";
        }
        CheckHoverChrome(log);
        PatternFont::UpdateFont(nullptr);
        check(PatternFont::currentFont && PatternFont::currentFont->dib && PatternFont::currentFont->dib->antialiased,"modern pattern font builds a coverage atlas");
        log<<"PASS: track tint ownership, selection and syntax colors, antialiased glyph compositing, active M/S/record buttons, modern pattern font and rounded/hover/disabled scrollbar skin\n";
        auto file=std::make_unique<Fixture>();
        file->m_nSamples=2;
        file->Patterns.Insert(0,64);file->Patterns.Insert(1,64);
        file->Order().resize(3);file->Order()[0]=0;file->Order()[1]=1;file->Order()[2]=PATTERNINDEX_INVALID;
        for(SAMPLEINDEX index=1;index<=2;index++)
        {
            auto &sample=file->GetSample(index);sample.Initialize(MOD_TYPE_MPT);
            sample.nLength=sample.nLoopEnd=128;sample.nLoopStart=0;sample.uFlags.set(CHN_LOOP);
            sample.AllocateSample();check(sample.HasSampleData(),"sample allocation");
            for(int frame=0;frame<128;frame++) sample.sample8()[frame]=static_cast<int8>((frame%32<16?1:-1)*(index==1?64:16));
            sample.PrecomputeLoops(*file,false);
        }
        auto &note=*file->Patterns[0].GetpModCommand(0,0);note.note=NOTE_MIDDLEC;note.instr=1;
        file->ChnSettings[2].isBus=true;file->ChnSettings[3].isBus=true;
        auto render=[&](){
            file->ResetPlayPos();MixerSettings mixer;mixer.gdwMixingFreq=8000;mixer.gnChannels=2;
            file->SetMixerSettings(mixer);file->SetRepeatCount(0);file->InitPlayer(true);
            EnergyTarget warmup,energy;file->Read(2048,warmup);file->Read(4096,energy);
            return energy;
        };
        const auto base=render();check(base.left>0 && base.right>0,"fixture must produce stereo audio");
        file->ChnSettings[2].compositeInput=true;
        file->ChnSettings[0].parentBus=2;file->ChnSettings[1].parentBus=2;
        auto &first=file->ChnSettings[0];auto &second=file->ChnSettings[1];
        first.trackInstrument=1;first.inputEnabled=true;first.inputNoteMin=NOTE_MIDDLEC;first.inputNoteMax=NOTE_MIDDLEC+11;
        second.trackInstrument=2;second.inputEnabled=true;second.inputNoteMin=NOTE_MIDDLEC+12;second.inputNoteMax=NOTE_MIDDLEC+23;second.inputTranspose=-12;
        note={};
        auto routes=ResolveCompositeInput(file->ChnSettings,2,NOTE_MIDDLEC+12);
        check(routes.size()==1 && routes[0].channel==1 && routes[0].note==NOTE_MIDDLEC,"composite articulation pitch route");
        check(WriteCompositeNote(*file->Patterns[0].GetpModCommand(0,1),routes[0],-1),"write composite articulation as native notes");
        auto articulation=render();check(std::abs(articulation.left/base.left-0.25)<0.03,"mapped articulation must render its assigned sample");
        *file->Patterns[0].GetpModCommand(0,1)={};
        second.inputNoteMin=NOTE_MIDDLEC;second.inputNoteMax=NOTE_MIDDLEC+11;second.inputTranspose=0;
        for(const auto &event:ResolveCompositeInput(file->ChnSettings,2,NOTE_MIDDLEC))
            check(WriteCompositeNote(*file->Patterns[0].GetpModCommand(0,event.channel),event,-1),"write native layered note");
        auto layered=render();check(std::abs(layered.left/base.left-1.25)<0.03,"overlapping composite ranges must render both sample lanes");
        *file->Patterns[0].GetpModCommand(0,1)={};
        second.inputNoteMin=NOTE_MIDDLEC+12;second.inputNoteMax=NOTE_MIDDLEC+23;second.inputTranspose=-12;
        first.trackInstrument=0;second.trackInstrument=0;file->ChnSettings[1].parentBus=CHANNELINDEX_INVALID;
        log<<"PASS: composite articulation selection, transposition and layered native-note audio\n";
        file->ChnSettings[0].parentBus=2;file->ChnSettings[2].nVolume=32;
        auto half=render();check(std::abs(half.left/base.left-0.5)<0.03,"bus gain must affect rendered audio");
        file->ChnSettings[2].parentBus=3;file->ChnSettings[3].nVolume=32;
        auto quarter=render();check(std::abs(quarter.left/base.left-0.25)<0.03,"nested bus gain must affect rendered audio");
        file->ChnSettings[2].nPan=0;auto panned=render();check(panned.left>0 && panned.right<panned.left*0.001,"bus pan must balance rendered audio");
        file->ChnSettings[2].dwFlags.set(CHN_MUTE);auto muted=render();check(muted.left==0 && muted.right==0,"bus mute must silence child audio");
        file->ChnSettings[2].dwFlags.reset(CHN_MUTE);file->ChnSettings[2].nPan=128;file->ChnSettings[2].nVolume=64;file->ChnSettings[2].parentBus=CHANNELINDEX_INVALID;
        auto &automation=*file->Patterns[0].GetpModCommand(0,2);automation.command=CMD_CHANNELVOLUME;automation.param=16;
        auto automated=render();check(std::abs(automated.left/base.left-0.25)<0.03,"native bus automation must affect audio");
        automation={};file->ChnSettings[0].parentBus=CHANNELINDEX_INVALID;file->ChnSettings[0].trackInstrument=2;
        auto swapped=render();check(std::abs(swapped.left/base.left-0.25)<0.03,"track instrument override must change playback");
        file->ChnSettings[0].parentBus=2;file->ChnSettings[2].parentBus=3;file->ChnSettings[2].dwFlags.set(CHN_MUTE);
        file->m_synmptVersion=1;
        file->ChnSettings[1].inputUseKeys=true; file->ChnSettings[1].inputKeys[5]=0x42;
        file->ChnSettings[1].inputFixedNote=NOTE_MIDDLEC; file->ChnSettings[1].inputColor=RGB(20,60,180);
        file->ChnSettings[1].trackAutoColor=false;
        const auto saved=directory+P_("synmpt-runtime-roundtrip.synmpt");
        {mpt::IO::ofstream output(saved,std::ios::binary);check(file->SaveIT(output,saved,false),"save MPTM");}
        auto loaded=std::make_unique<CSoundFile>();
        {mpt::IO::ifstream input(saved,std::ios::binary);auto cursor=mpt::IO::make_FileCursor<mpt::PathString>(input);check(loaded->Create(cursor),"reload MPTM");}
        check(loaded->ChnSettings[0].trackInstrument==2 && loaded->ChnSettings[0].parentBus==2 && loaded->ChnSettings[2].parentBus==3 && loaded->ChnSettings[2].isBus && loaded->ChnSettings[2].dwFlags[CHN_MUTE],"MPTM routing/instrument/bus mute round trip");
        check(loaded->ChnSettings[2].compositeInput && loaded->ChnSettings[0].inputEnabled
            && loaded->ChnSettings[1].inputNoteMin==NOTE_MIDDLEC+12 && loaded->ChnSettings[1].inputTranspose==-12,"MPTM composite routing round trip");
        check(loaded->m_synmptVersion==1 && loaded->ChnSettings[1].inputUseKeys && loaded->ChnSettings[1].inputKeys[5]==0x42
            && loaded->ChnSettings[1].inputFixedNote==NOTE_MIDDLEC && loaded->ChnSettings[1].inputColor==RGB(20,60,180) && !loaded->ChnSettings[1].trackAutoColor,"native identity and explicit keyboard maps round trip");
        file->ChnSettings[0].trackInstrument=0; file->ChnSettings[0].parentBus=CHANNELINDEX_INVALID;
        file->ChnSettings[3].isMaster=true; file->ChnSettings[3].nVolume=32; file->ChnSettings[3].nPan=128;
        const auto masterHalf=render(); check(std::abs(masterHalf.left/base.left-0.5)<0.03,"master gain controls the final mix");
        file->ChnSettings[3].nPan=0; const auto masterLeft=render(); check(masterLeft.right<masterLeft.left*0.001,"master balance controls final stereo output");
        file->ChnSettings[3].dwFlags.set(CHN_MUTE); const auto masterMuted=render(); check(masterMuted.left+masterMuted.right<base.left*0.001,"master mute controls final output");
        file->ChnSettings[3].dwFlags.reset(CHN_MUTE); file->ChnSettings[3].nPan=128;
        auto &masterAutomation=*file->Patterns[0].GetpModCommand(0,3); masterAutomation.command=CMD_CHANNELVOLUME; masterAutomation.param=16;
        const auto masterAutomated=render(); check(std::abs(masterAutomated.left/base.left-0.25)<0.03,"master automation affects final audio");
        masterAutomation={}; file->ChnSettings[3].isMaster=false;
        std::vector<ModChannelSettings> keys(2); keys[0].isBus=keys[0].compositeInput=true;
        keys[1].parentBus=0; keys[1].trackInstrument=1; keys[1].inputEnabled=keys[1].inputUseKeys=true;
        keys[1].inputKeys[60/8]=uint8(1<<(60%8)); keys[1].inputFixedNote=NOTE_MIDDLEC+12;
        check(ResolveCompositeInput(keys,0,NOTE_MIDDLEC).size()==1 && ResolveCompositeInput(keys,0,NOTE_MIDDLEC)[0].note==NOTE_MIDDLEC+12 && ResolveCompositeInput(keys,0,NOTE_MIDDLEC+1).empty(),"explicit drum keys route fixed pitch and leave unmapped notes untouched");
        log<<"PASS: native identity, explicit/fixed keyboard maps, manual color state, final master gain/pan/mute/automation, bus rendering and save/reload\n";
        // A v1 chunk still loads with composite input disabled and default ranges.
        std::string legacy;
        {mpt::IO::ifstream input(saved,std::ios::binary);legacy.assign(std::istreambuf_iterator<char>(input),{});}
        const auto chunk=legacy.find("SNTK");check(chunk!=std::string::npos && chunk+7+file->GetNumChannels()*29<=legacy.size(),"find saved track extension");
        const auto source=legacy;legacy[chunk+6]=1;
        for(size_t channel=0;channel<file->GetNumChannels();channel++)
        {std::copy_n(source.data()+chunk+7+channel*29,5,legacy.data()+chunk+7+channel*5);legacy[chunk+7+channel*5+4]&=3;}
        const auto legacyPath=directory+P_("synmpt-runtime-v1.mptm");
        {mpt::IO::ofstream output(legacyPath,std::ios::binary);output.write(legacy.data(),legacy.size());}
        auto old=std::make_unique<CSoundFile>();
        {mpt::IO::ifstream input(legacyPath,std::ios::binary);auto cursor=mpt::IO::make_FileCursor<mpt::PathString>(input);check(old->Create(cursor),"reload v1 track extension");}
        check(old->ChnSettings[2].isBus && old->ChnSettings[0].parentBus==2 && !old->ChnSettings[2].compositeInput
            && !old->ChnSettings[0].inputEnabled && old->ChnSettings[0].inputNoteMin==NOTE_MIN,"v1 songs retain buses without enabling composite input");
        log<<"PASS: backward-compatible v1 track extension\n";
        for(int variant=0;variant<3;variant++)
        {
            auto empty=std::make_unique<CSoundFile>(); empty->Create(MOD_TYPE_MPT,2);
            if(variant>=1) empty->m_songMessage.SetFormatted("Folder notes",SongMessage::leAutodetect);
            if(variant==2) { empty->Instruments[1]=new ModInstrument(); empty->m_nInstruments=1; }
            empty->m_synmptVersion=1; empty->Patterns.Insert(0,64); empty->Order().resize(1,0);
            empty->ChnSettings[0].isBus=true; empty->ChnSettings[0].szName="Empty folder";
            empty->ChnSettings[1].parentBus=0; empty->ChnSettings[1].inputEnabled=true;
            std::stringstream stream(std::ios::in|std::ios::out|std::ios::binary);
            check(empty->SaveIT(stream,{}),"save preset without assets or nonempty patterns");
            stream.seekg(0); auto reader=mpt::IO::make_FileCursor<mpt::PathString>(stream);
            auto reloadedEmpty=std::make_unique<CSoundFile>();
            check(reloadedEmpty->Create(reader) && reloadedEmpty->m_synmptVersion==1 && reloadedEmpty->GetNumChannels()==2 && reloadedEmpty->ChnSettings[0].isBus
                && reloadedEmpty->ChnSettings[1].parentBus==0 && reloadedEmpty->ChnSettings[1].inputEnabled,"assetless preset preserves its native identity, hierarchy and mapping");
            log<<"PASS: assetless preset extension loading\n";
        }
        for(const auto &song:songs)
        {
            auto playback=std::make_unique<CSoundFile>();
            mpt::IO::ifstream stream(song,std::ios::binary);auto cursor=mpt::IO::make_FileCursor<mpt::PathString>(stream);
            check(playback->Create(cursor),"load reference song");
            if(!mpt::PathCompareNoCase(song.GetFilenameExtension(),P_(".synpreset")))
            {
                check(playback->m_synmptVersion==1 && playback->ChnSettings[0].isBus,"supplied preset retains native identity and root folder");
                log<<"PASS: supplied preset loads "<<playback->GetNumChannels()<<" tracks, "<<playback->GetNumInstruments()<<" instruments\n";
                continue;
            }
            MixerSettings mixer;mixer.gdwMixingFreq=8000;mixer.gnChannels=2;playback->SetMixerSettings(mixer);
            playback->ResetPlayPos();playback->m_PlayState.m_flags.reset(SONG_PATTERNLOOP);playback->SetRepeatCount(0);playback->InitPlayer(true);
            EnergyTarget target;ORDERINDEX furthest=0;
            for(int block=0;block<4000 && furthest<2;block++)
            {if(!playback->Read(1024,target))break;furthest=std::max(furthest,playback->m_PlayState.m_nCurrentOrder);}
            check(furthest>=2,"reference song must advance past opening orders");
            check(target.left+target.right>0,"reference song must produce audio");
            log<<"PASS: reference song advances to order "<<furthest<<" with audible output\n";
        }
        // A silent, disposable module for exercising folder input in the editor.
        for(auto &track:file->ChnSettings) track=ModChannelSettings{};
        for(auto &pattern:file->Patterns) std::fill(pattern.begin(),pattern.end(),ModCommand{});
        auto &folder=file->ChnSettings[0];folder.isBus=true;folder.compositeInput=true;folder.szName="Composite Test";
        for(CHANNELINDEX channel=1;channel<=2;channel++)
        {
            auto &track=file->ChnSettings[channel];track.parentBus=0;track.trackInstrument=channel;track.inputEnabled=true;
            track.inputNoteMin=static_cast<uint8>(NOTE_MIDDLEC-12+(channel-1)*12);track.inputNoteMax=track.inputNoteMin+11;
            track.inputTranspose=channel==1?12:0;track.szName=channel==1?"Bank A (C-4)":"Bank B (C-5)";
        }
        file->ChnSettings[3].szName="Independent";
        {const auto demo=directory+P_("synmpt-composite-demo.mptm");mpt::IO::ofstream output(demo,std::ios::binary);check(file->SaveIT(output,demo,false),"save disposable editor fixture");}
        log<<"ALL CHECKS PASSED\n";log.flush();return 0;
    } catch(const std::exception &error) {log<<"FAIL: "<<error.what()<<'\n';log.flush();return 1;}
}
}
OPENMPT_NAMESPACE_END
