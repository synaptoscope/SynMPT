/* SPDX-License-Identifier: BSD-3-Clause
 * Composer workspace implementation, included in View_pat.cpp inside the namespace.
 */
void CViewPattern::ComposerRefresh()
{
    auto *doc=GetDocument(); doc->SetModified(); doc->UpdateAllViews(nullptr, UpdateHint().ModType());
    UpdateSizes(); UpdateScrollSize(); InvalidatePattern(true,true);
}
void CViewPattern::ComposerAdd(bool group, bool master)
{
    auto &file=*GetSoundFile(); auto *doc=GetDocument();
    if(file.GetType()!=MOD_TYPE_MPT) { Reporting::Information(_T("Convert this song to a SynMPT project from Export before adding workspace tracks.")); return; }
    if(master) for(CHANNELINDEX i=0;i<file.GetNumChannels();i++) if(file.ChnSettings[i].isMaster) { SetCurrentColumn(i); return; }
    CInputDlg name(this, group ? _T("Folder name") : _T("Track name"), master ? _T("Master") : group ? _T("Group") : _T("Track"));
    if(!master && !theApp.m_synmptQuietOpen && name.DoModal()!=IDOK) return;
    std::vector<CHANNELINDEX> order;
    if(!file.IsEmptyWorkspace()) for(CHANNELINDEX i=0;i<file.GetNumChannels();i++) order.push_back(i);
    const CHANNELINDEX added=master ? 0 : static_cast<CHANNELINDEX>(order.size());
    order.insert(order.begin()+added, CHANNELINDEX_INVALID);
    const auto oldStates=m_chnState; const auto oldArmed=m_armedComposite;
    StopCompositeVoices();
    if(doc->ReArrangeChannels(order)==CHANNELINDEX_INVALID) return;
    m_chnState.assign(order.size(),ChannelState{}); m_armedComposite=CHANNELINDEX_INVALID;
    for(CHANNELINDEX i=0;i<order.size();i++) { if(order[i]<oldStates.size()) m_chnState[i]=oldStates[order[i]]; if(order[i]==oldArmed && oldArmed!=CHANNELINDEX_INVALID) m_armedComposite=i; }
    auto &track=file.ChnSettings[added]; track=ModChannelSettings{}; track.isBus=group; track.isMaster=master;
    track.szName=master ? "Master" : theApp.m_synmptQuietOpen ? (group?"Group":"Track") : mpt::ToCharset(file.GetCharsetInternal(),mpt::ToUnicode(name.resultAsString));
    track.color=master ? RGB(141,154,173) : RGB(142,167,189); SynMPTApplyAutoColor(track);
    doc->MakeNativeProject();
    m_trackSelection.Click(added,file.GetNumChannels(),false,false); SetCurrentColumn(added); ComposerRefresh();
}

void CViewPattern::ComposerPreset(bool save, mpt::PathString path)
{
    auto &song=*GetSoundFile(); auto *doc=GetDocument();
    if(song.GetType()!=MOD_TYPE_MPT) { Reporting::Information(_T("Convert to SynMPT first using Export.")); return; }
    if(save)
    {
        const auto group=GetCurrentChannel();
        if(group>=song.GetNumChannels() || !song.ChnSettings[group].isBus || song.ChnSettings[group].isMaster)
        { Reporting::Information(_T("Select the folder you want to save as a preset.")); return; }
        std::vector<CHANNELINDEX> members{group};
        for(CHANNELINDEX i=0;i<song.GetNumChannels();i++) if(TrackBelongsToBus(song.ChnSettings,i,group)) members.push_back(i);
        for(auto i:members) if(!CanGroupTrack(i)) { Reporting::Information(_T("Presets currently support internal samples and instruments. Remove external outputs from this folder first.")); return; }
        auto dialog=SaveFileDialog().DefaultExtension(U_("synpreset")).DefaultFilename(mpt::PathString::FromUnicode(U_("Instrument.synpreset")))
            .ExtensionFilter("SynMPT instrument preset (*.synpreset)|*.synpreset||");
        if(path.empty()) { if(!dialog.Show(this)) return; path=dialog.GetFirstFile(); }
        auto preset=std::make_unique<CSoundFile>(); preset->Create(MOD_TYPE_MPT,static_cast<CHANNELINDEX>(members.size()),nullptr); preset->m_synmptVersion=1;
        preset->Patterns.Insert(0,64); preset->Order().resize(1,0);
        std::map<uint16,uint16> assets;
        for(CHANNELINDEX i=0;i<members.size();i++)
        {
            auto track=song.ChnSettings[members[i]];
            const auto parent=std::find(members.begin(),members.end(),track.parentBus);
            track.parentBus=parent==members.end() ? CHANNELINDEX_INVALID : static_cast<CHANNELINDEX>(parent-members.begin());
            if(track.trackInstrument)
            {
                const auto source=track.trackInstrument;
                if(!assets.count(source))
                {
                    const uint16 target=static_cast<uint16>(assets.size()+1); bool ok=false;
                    if(song.GetNumInstruments()) ok=preset->ReadInstrumentFromSong(target,song,source);
                    else if(preset->ReadSampleFromSong(target,song,source))
                    { preset->Instruments[target]=new ModInstrument(target); preset->m_nInstruments=target; ok=true; }
                    if(!ok) { Reporting::Error(_T("The preset could not copy an instrument. Nothing was saved.")); return; }
                    assets[source]=target;
                }
                track.trackInstrument=assets[source];
            }
            preset->ChnSettings[i]=track;
        }
        for(SAMPLEINDEX i=1;i<=preset->GetNumSamples();i++) preset->GetSample(i).uFlags.reset(SMP_KEEPONDISK);
        try
        {
            const auto temporary=path+mpt::PathString::FromNative(L".saving-"+std::to_wstring(::GetCurrentProcessId())+L"-"+std::to_wstring(::GetTickCount64()));
            {
                mpt::IO::SafeOutputFile output(temporary,std::ios::binary,mpt::IO::FlushMode::Full); mpt::IO::ofstream &stream=output;
                stream.exceptions(std::ios::badbit|std::ios::failbit);
                if(!preset->SaveIT(stream,{})) throw std::runtime_error("preset save");
                stream.flush();
            }
            if(mpt::native_fs{}.is_file(path) && !::CopyFile(path.AsNative().c_str(),(path+P_(".bak")).AsNative().c_str(),FALSE))
                throw std::runtime_error("preset backup");
            if(!::MoveFileEx(temporary.AsNative().c_str(),path.AsNative().c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
                throw std::runtime_error("preset install");
        } catch(...) { Reporting::Error(_T("The preset could not be saved.")); }
        return;
    }
    auto dialog=OpenFileDialog().ExtensionFilter("SynMPT instrument preset (*.synpreset)|*.synpreset||");
    if(path.empty()) { if(!dialog.Show(this)) return; path=dialog.GetFirstFile(); }
    auto preset=std::make_unique<CSoundFile>(); mpt::IO::InputFile input(path); auto reader=GetFileReader(input);
    if(!input.IsValid() || !preset->Create(reader) || preset->m_synmptVersion!=1 || !preset->GetNumChannels() || !preset->ChnSettings[0].isBus || preset->ChnSettings[0].isMaster)
    { Reporting::Error(_T("This is not a supported SynMPT instrument preset.")); return; }
    const auto base=song.IsEmptyWorkspace()? CHANNELINDEX(0):song.GetNumChannels();
    if(base+preset->GetNumChannels()>song.GetModSpecifications().channelsMax || song.GetNumInstruments()+preset->GetNumInstruments()>=MAX_INSTRUMENTS
        || song.GetNumSamples()+preset->GetNumSamples()>=song.GetModSpecifications().samplesMax)
    { Reporting::Information(_T("This song does not have room for all tracks and assets in the preset.")); return; }
    for(CHANNELINDEX i=1;i<preset->GetNumChannels();i++) if(!TrackBelongsToBus(preset->ChnSettings,i,0))
    { Reporting::Error(_T("The preset contains tracks outside its root folder.")); return; }
    const auto instrumentsAfterConversion=song.GetNumInstruments()?song.GetNumInstruments():song.GetNumSamples();
    if(instrumentsAfterConversion+preset->GetNumInstruments()>=MAX_INSTRUMENTS)
    { Reporting::Information(_T("There are too many existing samples to convert them to instruments and load this preset.")); return; }
    for(INSTRUMENTINDEX i=1;i<=preset->GetNumInstruments();i++)
        if(!preset->Instruments[i] || preset->Instruments[i]->nMixPlug || preset->Instruments[i]->nMidiChannel)
        { Reporting::Error(_T("The preset contains an unsupported external instrument.")); return; }
    CriticalSection transaction;
    if(!song.GetNumInstruments() && !doc->ConvertSamplesToInstruments()) return;
    const auto firstInstrument=song.GetNumInstruments();
    // New slots keep every recalled instance independent of existing instruments.
    for(INSTRUMENTINDEX i=1;i<=preset->GetNumInstruments();i++)
    {
        if(!song.ReadInstrumentFromSong(firstInstrument+i,*preset,i)) { Reporting::Error(_T("Could not copy the preset's instruments.")); return; }
        for(auto sample:song.Instruments[firstInstrument+i]->Keyboard) if(sample) song.GetSample(sample).uFlags.reset(SMP_KEEPONDISK);
    }
    std::vector<CHANNELINDEX> order; for(CHANNELINDEX i=0;i<base;i++) order.push_back(i);
    order.resize(base+preset->GetNumChannels(),CHANNELINDEX_INVALID);
    if(doc->ReArrangeChannels(order)==CHANNELINDEX_INVALID) return;
    for(CHANNELINDEX i=0;i<preset->GetNumChannels();i++)
    {
        auto track=preset->ChnSettings[i]; if(track.parentBus!=CHANNELINDEX_INVALID) track.parentBus+=base;
        if(track.trackInstrument) track.trackInstrument+=firstInstrument;
        track.isPlaceholder=false; track.isMaster=false; song.ChnSettings[base+i]=track;
    }
    doc->MakeNativeProject(); transaction.Leave(); SetCurrentColumn(base); ComposerRefresh();
}

void CViewPattern::ComposerColors(UINT command)
{
    auto &file=*GetSoundFile(); auto *doc=GetDocument();
    if(command==ID_COMPOSER_COLORS)
    {
        CMenu menu; menu.CreatePopupMenu(); menu.AppendMenu(MF_STRING,1,_T("Edit ordered auto-color rules..."));
        menu.AppendMenu(MF_STRING,2,_T("Use automatic colors for selected tracks")); menu.AppendMenu(MF_STRING,3,_T("Set a manual color for selected tracks..."));
        CPoint pt; GetCursorPos(&pt); const auto choice=menu.TrackPopupMenu(TPM_RETURNCMD|TPM_NONOTIFY,pt.x,pt.y,this);
        if(choice==2) { ComposerColors(ID_COMPOSER_AUTO_COLOR); return; }
        if(choice==3) { ComposerColors(ID_COMPOSER_MANUAL_COLOR); return; }
        if(choice!=1) return;
        CComposerTextDlg dialog(this,_T("Automatic track colors"),_T("First matching enabled rule wins, from top to bottom. One rule per line:\n1|keyword|#RRGGBB    (use 0 to disable a rule). Matching ignores letter case.\nManual track colors are preserved. Move lines to change priority."),mpt::ToCString(mpt::Charset::UTF8,SynMPTColorRules()));
        for(;;)
        {
            if(dialog.DoModal()!=IDOK) return;
            const auto rules=mpt::ToCharset(mpt::Charset::UTF8,dialog.value); std::istringstream lines(rules); std::string line; bool valid=true;
            while(std::getline(lines,line))
            {
                if(!line.empty()&&line.back()=='\r') line.pop_back(); if(line.empty()) continue;
                std::string key; COLORREF color; bool enabled; if(!SynMPTParseColorRule(line,key,color,enabled)) { valid=false; break; }
            }
            if(!valid) { Reporting::Information(_T("A rule is invalid. Use 1|keyword|#RRGGBB or 0|keyword|#RRGGBB.")); continue; }
            std::ofstream output((theApp.GetConfigPath()+P_("SynMPT-colors.txt")).AsNative(),std::ios::trunc);
            if(!(output<<rules)) { Reporting::Error(_T("Could not save the color rules.")); return; }
            SynMPTColorRules()=rules; break;
        }
    }
    COLORREF manual=RGB(140,165,190);
    if(command==ID_COMPOSER_MANUAL_COLOR) { CColorDialog dialog(manual,CC_FULLOPEN,this); if(dialog.DoModal()!=IDOK) return; manual=dialog.GetColor(); }
    doc->GetPatternUndo().PrepareChannelUndo(0,file.GetNumChannels(),"Track Colors");
    for(CHANNELINDEX i=0;i<file.GetNumChannels();i++)
    {
        auto &track=file.ChnSettings[i];
        if(command!=ID_COMPOSER_COLORS && !m_trackSelection.Contains(i) && i!=GetCurrentChannel()) continue;
        if(command==ID_COMPOSER_MANUAL_COLOR) { track.trackAutoColor=false; track.color=manual; }
        else { if(command==ID_COMPOSER_AUTO_COLOR) track.trackAutoColor=true; SynMPTApplyAutoColor(track); }
    }
    ComposerRefresh();
}

void CViewPattern::ComposerExport()
{
    auto &file=*GetSoundFile(); auto *doc=GetDocument();
    CMenu menu; menu.CreatePopupMenu(); menu.AppendMenu(MF_STRING,1,_T("Save a native SynMPT copy...")); menu.AppendMenu(MF_STRING,2,_T("Export MPTM copy..."));
    CPoint point; GetCursorPos(&point); const auto choice=menu.TrackPopupMenu(TPM_RETURNCMD|TPM_NONOTIFY,point.x,point.y,this); if(!choice) return;
    const bool native=choice==1;
    if(!native && Reporting::Confirm(_T("MPTM export keeps the musical data and SynMPT extensions. Other trackers can ignore folder mixing, input mappings and workspace settings; their playback can differ. Your native project stays unchanged. Continue?"))!=cnfYes) return;
    if(native && file.GetType()!=MOD_TYPE_MPT)
    {
        if(!doc->MakeNativeProject()) return;
    }
    auto dialog=SaveFileDialog().DefaultExtension(native?U_("synmpt"):U_("mptm"))
        .ExtensionFilter(native?"SynMPT project (*.synmpt)|*.synmpt||":"MPTM module (*.mptm)|*.mptm||"); if(!dialog.Show()) return;
    if(!native && file.GetType()!=MOD_TYPE_MPT) { Reporting::Information(_T("Save a native SynMPT copy first to convert this song to MPTM.")); return; }
    if((!doc->GetPathNameMpt().empty() && !mpt::PathCompareNoCase(dialog.GetFirstFile(),doc->GetPathNameMpt()))
        || (!doc->m_conversionSource.empty() && !mpt::PathCompareNoCase(dialog.GetFirstFile(),doc->m_conversionSource)))
    { Reporting::Information(_T("Choose a different filename for the copy; the open song is kept intact.")); return; }
    const auto oldVersion=file.m_synmptVersion; file.m_synmptVersion=native?1:0;
    const bool saved=doc->SaveFile(dialog.GetFirstFile(),true); file.m_synmptVersion=oldVersion;
    if(!saved) Reporting::Error(_T("The copy could not be saved."));
}

void CViewPattern::ComposerMoveTracks(CHANNELINDEX source, CHANNELINDEX target, CPoint)
{
    auto &file=*GetSoundFile(); auto *doc=GetDocument(); const auto count=file.GetNumChannels();
    std::vector<bool> moving(count,false), roots(count,false);
    for(CHANNELINDEX i=0;i<count;i++) roots[i]=i==source || m_trackSelection.Contains(i);
    for(CHANNELINDEX i=0;i<count;i++) if(roots[i]) VisitTrackBuses(file.ChnSettings,i,[&](CHANNELINDEX parent){if(roots[parent]) roots[i]=false;});
    for(CHANNELINDEX i=0;i<count;i++)
    {
        moving[i]=roots[i]; VisitTrackBuses(file.ChnSettings,i,[&](CHANNELINDEX parent){if(roots[parent]) moving[i]=true;});
        if(moving[i] && file.ChnSettings[i].isMaster) return;
    }
    // An ungrouped drop beside a nested child lands outside its entire family.
    if(!m_composerGroupDrop) VisitTrackBuses(file.ChnSettings,target,[&](CHANNELINDEX ancestor){target=ancestor;});
    if(moving[target]) return;
    const auto parent=m_composerGroupDrop?target:CHANNELINDEX_INVALID;
    if(parent!=CHANNELINDEX_INVALID && file.ChnSettings[parent].isMaster) return;
    for(CHANNELINDEX i=0;i<count;i++) if(moving[i] && parent!=CHANNELINDEX_INVALID && !CanGroupTrack(i)) return;
    auto settings=file.ChnSettings;
    for(CHANNELINDEX i=0;i<count;i++) if(roots[i]) settings[i].parentBus=parent;
    std::vector<CHANNELINDEX> order, moved;
    for(CHANNELINDEX i=0;i<count;i++) (moving[i]?moved:order).push_back(i);
    auto position=std::find(order.begin(),order.end(),target);
    if(m_composerGroupDrop || m_composerAfter)
    {
        ++position;
        while(position!=order.end() && TrackBelongsToBus(settings,*position,target)) ++position;
    }
    order.insert(position,moved.begin(),moved.end());
    doc->PrepareUndoForAllPatterns(true,"Move Tracks and Folders");
    const auto original=file.ChnSettings; file.ChnSettings=settings;
    if(doc->ReArrangeChannels(order,false)==CHANNELINDEX_INVALID) { file.ChnSettings=original; return; }
    const auto oldStates=m_chnState; const auto oldArmed=m_armedComposite;
    std::vector<ModChannelSettings> arranged=file.ChnSettings;
    const auto folders=TrackFolderOrder(arranged);
    if(doc->ReArrangeChannels(folders,false)==CHANNELINDEX_INVALID) return;
    StopCompositeVoices(); m_armedComposite=CHANNELINDEX_INVALID;
    std::vector<CHANNELINDEX> combined; for(auto index:folders) combined.push_back(order[index]);
    m_trackSelection.Remap(combined); TrackHeaderSelection solo; solo.Restore(m_trackSoloTargets,TrackHeaderSelection::Invalid); solo.Remap(combined); m_trackSoloTargets=solo.Mask();
    m_chnState.assign(count,ChannelState{});
    for(CHANNELINDEX i=0;i<count;i++) { if(combined[i]<oldStates.size()) m_chnState[i]=oldStates[combined[i]]; if(combined[i]==oldArmed) m_armedComposite=i; }
    ComposerRefresh();
}

void CViewPattern::ComposerStepMenu()
{
    CMenu menu; menu.CreatePopupMenu(); menu.AppendMenu(MF_STRING|(m_stepLength==0?MF_CHECKED:0),1,_T("Tracker entry (no automatic length)"));
    const auto &file=*GetSoundFile(); const int beat=file.m_nDefaultRowsPerBeat;
    const int numerators[]={4,2,1,1,1,1,3,3,3,2,1,1}; const int denominators[]={1,1,1,2,4,8,2,4,8,3,3,6};
    const LPCTSTR names[]={_T("Whole"),_T("Half"),_T("Quarter"),_T("Eighth"),_T("Sixteenth"),_T("Thirty-second"),_T("Dotted quarter"),_T("Dotted eighth"),_T("Dotted sixteenth"),_T("Quarter triplet"),_T("Eighth triplet"),_T("Sixteenth triplet")};
    for(UINT i=0;i<std::size(names);i++) menu.AppendMenu(MF_STRING|((beat*numerators[i])%denominators[i]?MF_GRAYED:0),10+i,names[i]);
    menu.AppendMenu(MF_SEPARATOR); menu.AppendMenu(MF_STRING|(m_stepAdvance?MF_CHECKED:0),2,_T("Advance by note length after entry"));
    CPoint point; GetCursorPos(&point); const auto result=menu.TrackPopupMenu(TPM_RETURNCMD|TPM_NONOTIFY,point.x,point.y,this);
    if(result==1) m_stepLength=0; else if(result==2) m_stepAdvance=!m_stepAdvance;
    else if(result>=10 && result<10+std::size(names)) m_stepLength=beat*numerators[result-10]/denominators[result-10];
    CMainFrame::GetMainFrame()->SetInfoText(m_stepLength?MPT_CFORMAT("Step length: {} rows. Advance {}. Gray values need a finer Rows/Beat grid.")(m_stepLength,m_stepAdvance?_T("on"):_T("off")):CString(_T("Tracker note entry.")));
}

bool CViewPattern::ComposerStepNote(ModCommand::NOTE note,int volume)
{
    auto &file=*GetSoundFile(); auto *doc=GetDocument();
    if(file.IsEmptyWorkspace()) return true;
    if(!m_stepLength || !IsEditingEnabled() || IsLiveRecord() || !ModCommand::IsNote(note)) return false;
    const auto channel=GetCurrentChannel(); const auto row=GetCurrentRow(); if(!file.Patterns.IsValidPat(m_nPattern)) return true;
    auto &pattern=file.Patterns[m_nPattern];
    if(row+m_stepLength>=pattern.GetNumRows()) { Reporting::Information(_T("This note's end is beyond the current pattern. Add rows or use a shorter value.")); return true; }
    std::vector<CompositeNote> notes;
    if(m_armedComposite<file.GetNumChannels() && (channel==m_armedComposite || TrackBelongsToBus(file.ChnSettings,channel,m_armedComposite))) notes=ResolveCompositeInput(file.ChnSettings,m_armedComposite,note);
    else if(!file.ChnSettings[channel].isBus)
    {
        auto instrument=file.ChnSettings[channel].trackInstrument; if(!instrument) instrument=static_cast<uint16>(SendCtrlMessage(CTRLMSG_GETCURRENTINSTRUMENT));
        notes.push_back({channel,note,instrument});
    }
    if(notes.empty()) return true;
    for(const auto &event:notes) for(ROWINDEX i=row;i<=row+m_stepLength;i++)
    {
        const auto &cell=*pattern.GetpModCommand(i,event.channel);
        if((i<row+m_stepLength && cell.note!=NOTE_NONE && !(i==row && (cell.note==NOTE_KEYOFF || cell.note==NOTE_NOTECUT))) || cell.IsPcNote()) { CMainFrame::GetMainFrame()->SetInfoText(_T("Step entry kept an occupied note. Choose a clear range or edit that lane.")); return true; }
    }
    StopCompositeNote(note,false,false);
    doc->GetPatternUndo().PrepareUndo(m_nPattern,0,row,file.GetNumChannels(),m_stepLength+1,"Musical Step Entry");
    for(const auto &event:notes)
    {
        auto &cell=*pattern.GetpModCommand(row,event.channel); cell.note=event.note; cell.instr=static_cast<ModCommand::INSTR>(event.instrument);
        if(volume>=0 && cell.volcmd==VOLCMD_NONE) { cell.volcmd=VOLCMD_VOLUME; cell.vol=static_cast<ModCommand::VOL>(Clamp(volume,0,64)); }
        auto &end=*pattern.GetpModCommand(row+m_stepLength,event.channel); if(end.note==NOTE_NONE) end.note=NOTE_KEYOFF;
        const auto preview=doc->PlayNote(PlayNoteParam(event.note).Instrument(event.instrument).Channel(event.channel).Volume(volume<0?-1:volume*4));
        m_compositeVoices[note].push_back({event.channel,preview,event.note,event.instrument});
    }
    if(m_stepAdvance) SetCurrentRow(row+m_stepLength,WrapMode::LimitAtPatternEnd);
    SetSelToCursor(); SetModified(); InvalidatePattern(true,true); return true;
}

void CViewPattern::ComposerStepAction(UINT command)
{
    auto &file=*GetSoundFile(); if(file.IsEmptyWorkspace() || !file.Patterns.IsValidPat(m_nPattern)) return;
    const auto row=GetCurrentRow(); const ROWINDEX length=m_stepLength?m_stepLength:std::max<uint32>(1,m_nSpacing); auto &pattern=file.Patterns[m_nPattern];
    if(command==ID_COMPOSER_ADVANCE) { SetCurrentRow(row+length,WrapMode::LimitAtPatternEnd); SetSelToCursor(); return; }
    std::vector<CHANNELINDEX> tracks;
    const auto targets=m_trackSelection.Targets(GetCurrentChannel(),file.GetNumChannels());
    for(CHANNELINDEX i=0;i<file.GetNumChannels();i++) if(targets[i]) tracks.push_back(i);
    if(m_armedComposite<file.GetNumChannels()) { tracks.clear(); for(CHANNELINDEX i=0;i<file.GetNumChannels();i++) if(!file.ChnSettings[i].isBus && TrackBelongsToBus(file.ChnSettings,i,m_armedComposite)) tracks.push_back(i); }
    if(row+length>=pattern.GetNumRows()) { Reporting::Information(_T("Add rows before extending beyond this pattern.")); return; }
    if(command==ID_COMPOSER_TIE)
    {
        for(auto channel:tracks)
        {
            const auto &start=*pattern.GetpModCommand(row,channel); if(start.note!=NOTE_KEYOFF && start.note!=NOTE_NOTECUT) continue;
            for(ROWINDEX i=row+1;i<=row+length;i++) if(pattern.GetpModCommand(i,channel)->note!=NOTE_NONE)
            { Reporting::Information(_T("A note blocks this extension; it was kept.")); return; }
        }
    }
    GetDocument()->GetPatternUndo().PrepareUndo(m_nPattern,0,row,file.GetNumChannels(),length+1,command==ID_COMPOSER_TIE?"Extend Notes":"Step Rest");
    for(auto channel:tracks)
    {
        auto &start=*pattern.GetpModCommand(row,channel);
        if(command==ID_COMPOSER_TIE)
        { if(start.note==NOTE_KEYOFF || start.note==NOTE_NOTECUT) { pattern.GetpModCommand(row+length,channel)->note=start.note; start.note=NOTE_NONE; } }
        else if(start.note==NOTE_NONE) start.note=NOTE_KEYOFF;
    }
    SetCurrentRow(row+length,WrapMode::LimitAtPatternEnd); SetSelToCursor(); SetModified(); InvalidatePattern(true,true);
}

void CViewPattern::OnComposerCommand(UINT command)
{
    if(command==ID_COMPOSER_DETACH) { ComposerOpenEditor(ID_VIEW_PATTERNS); return; }
    if(command==ID_COMPOSER_ADD_TRACK)
    {
        CMenu menu; menu.CreatePopupMenu(); menu.AppendMenu(MF_STRING,1,_T("Add track...")); menu.AppendMenu(MF_STRING,2,_T("Add empty folder..."));
        menu.AppendMenu(MF_STRING,3,_T("Group selected tracks...")); menu.AppendMenu(MF_STRING,4,_T("Remove selected tracks"));
        CPoint pt; GetCursorPos(&pt); const auto choice=menu.TrackPopupMenu(TPM_RETURNCMD|TPM_NONOTIFY,pt.x,pt.y,this);
        if(choice==1 || choice==2) ComposerAdd(choice==2); else if(choice==3) CreateGroupBus();
        else if(choice==4) OnComposerCommand(ID_COMPOSER_DELETE);
    } else if(command==ID_COMPOSER_DELETE)
    {
        const auto &file=*GetSoundFile(); std::vector<bool> keep(file.GetNumChannels(),true);
        for(CHANNELINDEX i=0;i<file.GetNumChannels();i++)
        { if(m_trackSelection.Contains(i)||i==GetCurrentChannel()) keep[i]=false; VisitTrackBuses(file.ChnSettings,i,[&](CHANNELINDEX parent){if(m_trackSelection.Contains(parent)||parent==GetCurrentChannel())keep[i]=false;}); }
        StopCompositeVoices(); GetDocument()->RemoveChannels(keep,true); m_armedComposite=CHANNELINDEX_INVALID; ComposerRefresh();
    } else if(command==ID_COMPOSER_LOAD_PRESET)
    {
        CMenu menu; menu.CreatePopupMenu(); menu.AppendMenu(MF_STRING,1,_T("Add preset...")); menu.AppendMenu(MF_STRING,2,_T("Save selected folder as preset..."));
        menu.AppendMenu(MF_STRING,3,_T("Edit selected composite keyboard...")); CPoint pt; GetCursorPos(&pt);
        auto choice=menu.TrackPopupMenu(TPM_RETURNCMD|TPM_NONOTIFY,pt.x,pt.y,this);
        if(choice==1||choice==2) ComposerPreset(choice==2); else if(choice==3) ConfigureCompositeInput(GetCurrentChannel());
    } else if(command==ID_COMPOSER_SAVE_PRESET) ComposerPreset(true);
    else if(command==ID_COMPOSER_MASTER) ComposerAdd(true,true);
    else if(command>=ID_COMPOSER_COLORS && command<=ID_COMPOSER_MANUAL_COLOR) ComposerColors(command);
    else if(command==ID_COMPOSER_EXPORT) ComposerExport();
    else if(command==ID_COMPOSER_STEP) ComposerStepMenu();
    else if(command>=ID_COMPOSER_ADVANCE && command<=ID_COMPOSER_TIE) ComposerStepAction(command);
    else if(command==ID_COMPOSER_SAMPLES || command==ID_COMPOSER_INSTRUMENTS)
    {
        CMenu menu; menu.CreatePopupMenu(); menu.AppendMenu(MF_STRING,ID_VIEW_SAMPLES,_T("Open sample editor window")); menu.AppendMenu(MF_STRING,ID_VIEW_INSTRUMENTS,_T("Open instrument editor window"));
        menu.AppendMenu(MF_STRING,ID_VIEW_PATTERNS,_T("Open song in another window")); CPoint pt; GetCursorPos(&pt);
        const auto choice=menu.TrackPopupMenu(TPM_RETURNCMD|TPM_NONOTIFY,pt.x,pt.y,this); if(choice) ComposerOpenEditor(choice);
    }
}

class CComposerEditorFrame : public CMDIFrameWnd
{
protected:
    BOOL OnCreateClient(LPCREATESTRUCT create, CCreateContext *) override { return CreateClient(create,nullptr); }
    LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override
    {
        if(message==WM_CLOSE)
        {
            auto *child=MDIGetActive(); if(child) { const auto hwnd=child->m_hWnd; child->SendMessage(WM_CLOSE); if(::IsWindow(hwnd)) return 0; }
            DestroyWindow(); return 0;
        }
        if(message==WM_TIMER && wParam==771 && !::GetWindow(m_hWndMDIClient,GW_CHILD)) { DestroyWindow(); return 0; }
        return CMDIFrameWnd::WindowProc(message,wParam,lParam);
    }
};
void CViewPattern::ComposerOpenEditor(UINT page)
{
    auto *doc=GetDocument();
    static std::map<std::pair<CModDoc *,UINT>,HWND> windows;
    const auto key=std::make_pair(doc,page);
    if(auto found=windows.find(key);found!=windows.end() && ::IsWindow(found->second))
    {
        auto *existing=dynamic_cast<CComposerEditorFrame *>(CWnd::FromHandlePermanent(found->second));
        if(existing && existing->MDIGetActive() && existing->MDIGetActive()->GetActiveDocument()==doc)
        { if(!theApp.m_synmptQuietOpen) { existing->ShowWindow(SW_RESTORE); existing->SetForegroundWindow(); } return; }
    }
    auto *host=new CComposerEditorFrame(); CRect rect; CMainFrame::GetMainFrame()->GetWindowRect(rect);
    rect=CRect(rect.left+60,rect.top+90,rect.left+1060,rect.top+790);
    if(!host->Create(nullptr,_T("SynMPT editor"),WS_OVERLAPPEDWINDOW,rect,CMainFrame::GetMainFrame())) return;
    CCreateContext context{}; context.m_pCurrentDoc=doc; context.m_pNewDocTemplate=doc->GetDocTemplate(); context.m_pNewViewClass=RUNTIME_CLASS(CModControlView);
    auto *child=new CChildFrame();
    if(!child->LoadFrame(IDR_MODULETYPE,WS_CHILD|WS_VISIBLE|WS_OVERLAPPEDWINDOW,host,&context)) { host->DestroyWindow(); return; }
    child->InitialUpdateFrame(doc,FALSE);
    // FALSE keeps the host hidden, but MFC also skips initialization in that
    // case. Initialize every view before it can receive document broadcasts.
    child->SendMessageToDescendants(WM_INITIALUPDATE,0,0,TRUE,TRUE);
    child->GetModControlView()->SelectEditorPage(page==ID_VIEW_SAMPLES?IDD_CONTROL_SAMPLES:page==ID_VIEW_INSTRUMENTS?IDD_CONTROL_INSTRUMENTS:IDD_CONTROL_PATTERNS);
    child->ShowWindow(SW_SHOWMAXIMIZED); host->SetMenu(nullptr);
    host->SetWindowText(doc->GetTitle()+(page==ID_VIEW_SAMPLES?_T(" - Samples"):page==ID_VIEW_INSTRUMENTS?_T(" - Instruments"):_T(" - Song")));
    SynMPTTheme::RegisterWindow(host->m_hWnd); host->SetTimer(771,1000,nullptr); windows[key]=host->m_hWnd;
    if(!theApp.m_synmptQuietOpen) host->ShowWindow(SW_SHOW);
}


// Integration checks use a disposable, hidden document. Never saves the user's song.
void CViewPattern::RunComposerChecks(const mpt::PathString &directory)
{
    mpt::IO::ofstream progress(directory+P_("synmpt-composer-checks.log"),std::ios::binary);
    const auto check=[&](bool pass,const char *why){progress<<(pass?"PASS: ":"FAIL: ")<<why<<'\n';progress.flush();if(!pass) throw std::runtime_error(why);};
    auto &file=*GetSoundFile(); auto *doc=GetDocument();
    m_Status.set(psRecordingEnabled);
    check(file.IsEmptyWorkspace() && file.m_synmptVersion==1,"native new document is an empty workspace");
    ComposerAdd(true); ComposerAdd(false); ComposerAdd(false);
    check(file.GetNumChannels()==3 && file.ChnSettings[0].isBus && !file.IsEmptyWorkspace(),"create folder and children from empty workspace");
    m_trackSelection.Click(1,3,false,false); m_trackSelection.Click(2,3,true,false); m_composerGroupDrop=true; m_composerAfter=false;
    ComposerMoveTracks(1,0,{});
    check(file.ChnSettings[1].parentBus==0 && file.ChnSettings[2].parentBus==0,"multi-track drop assigns both children");
    OnEditUndo(); check(file.ChnSettings[1].parentBus==CHANNELINDEX_INVALID && file.ChnSettings[2].parentBus==CHANNELINDEX_INVALID,"one undo restores drag ownership");
    OnEditRedo(); check(file.ChnSettings[2].parentBus==0,"redo restores folder ownership");
    file.m_nSamples=1; auto &sample=file.GetSample(1); sample.Initialize(MOD_TYPE_MPT); sample.nLength=32; sample.AllocateSample();
    for(int i=0;i<32;i++) sample.sample8()[i]=static_cast<int8>(i-16);
    file.ChnSettings[0].compositeInput=true;
    for(auto ch:{1,2}) { file.ChnSettings[ch].trackInstrument=1; file.ChnSettings[ch].inputEnabled=true; }
    file.ChnSettings[1].inputUseKeys=true; file.ChnSettings[1].inputKeys.fill(0); file.ChnSettings[1].inputKeys[60/8]|=uint8(1<<(60%8));
    file.ChnSettings[1].inputFixedNote=NOTE_MIDDLEC; file.ChnSettings[1].inputColor=RGB(77,188,215);
    SetCurrentColumn(0); const auto preset=directory+P_("composer-roundtrip.synpreset"); ComposerPreset(true,preset);
    ComposerPreset(false,preset);
    check(file.GetNumChannels()==6 && file.ChnSettings[4].parentBus==3 && file.ChnSettings[5].parentBus==3,"preset restores independent folder hierarchy");
    check(file.ChnSettings[4].inputUseKeys && file.ChnSettings[4].inputKeys[60/8] && file.ChnSettings[4].inputColor==RGB(77,188,215),"preset preserves keyboard ownership and color");
    const auto recalled=file.ChnSettings[4].trackInstrument;
    check(recalled!=file.ChnSettings[1].trackInstrument && file.Instruments[recalled],"preset uses independent instrument slots");
    const auto copiedSample=file.Instruments[recalled]->Keyboard[0]; check(copiedSample!=1 && file.GetSample(copiedSample).sample8()[4]==sample.sample8()[4],"preset embeds and copies sample data");
    CCompositeInputDlg mapper(*doc,3,this);
    check(mapper.Create(IDD_COMPOSITE_INPUT,this)!=FALSE,"keyboard mapper creates its controls");
    auto *keyboard=mapper.GetDlgItem(IDC_COMPOSITE_KEYBOARD); CRect keys; keyboard->GetClientRect(keys);
    const auto keyPoint=[&](int white){return MAKELPARAM((2*white+1)*keys.Width()/104,keys.Height()*4/5);};
    keyboard->SendMessage(WM_LBUTTONDOWN,MK_LBUTTON,keyPoint(23));
    keyboard->SendMessage(WM_MOUSEMOVE,MK_LBUTTON,keyPoint(27));
    keyboard->SendMessage(WM_LBUTTONUP,0,keyPoint(27));
    const auto mapped=[&](int midi){return (mapper.tracks[4].inputKeys[midi/8]&(1<<(midi%8)))!=0;};
    check(mapped(60)&&mapped(61)&&mapped(67)&&!mapped(59)&&!mapped(68),"keyboard drag maps Middle C through G including black keys");
    keyboard->SendMessage(WM_LBUTTONDOWN,MK_LBUTTON|MK_CONTROL,keyPoint(30)); keyboard->SendMessage(WM_LBUTTONUP,0,keyPoint(30));
    check(mapped(72)&&!mapped(70),"Ctrl-click adds a separate mapped key");
    keyboard->SendMessage(WM_LBUTTONDOWN,MK_LBUTTON|MK_CONTROL,keyPoint(30)); keyboard->SendMessage(WM_LBUTTONUP,0,keyPoint(30));
    check(!mapped(72)&&mapped(60),"Ctrl-click removes a mapped key without clearing the range");
    // Check dialog geometry, then render its keyboard directly into a bitmap. No desktop capture.
    CRect bounds; mapper.GetClientRect(bounds);
    bool contained=true;
    for(auto *control=mapper.GetWindow(GW_CHILD);control;control=control->GetNextWindow())
    {
        CRect rect; control->GetWindowRect(rect); mapper.ScreenToClient(rect);
        if(control->GetStyle()&WS_VISIBLE) contained &= rect.left>=0 && rect.top>=0 && rect.right<=bounds.right && rect.bottom<=bounds.bottom;
    }
    check(contained,"mapper controls and confirmation buttons fit in the window");
    bounds=keys; CDC image; image.CreateCompatibleDC(nullptr);
    BITMAPINFO info{}; info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER); info.bmiHeader.biWidth=bounds.Width(); info.bmiHeader.biHeight=-bounds.Height();
    info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
    void *pixels=nullptr; HBITMAP bitmap=::CreateDIBSection(image, &info,DIB_RGB_COLORS,&pixels,nullptr,0);
    check(bitmap!=nullptr,"create offscreen mapper preview"); auto previous=::SelectObject(image,bitmap);
    image.FillSolidRect(bounds,SynMPTTheme::Color(COLOR_BTNFACE));
    keyboard->SendMessage(WM_PRINTCLIENT,reinterpret_cast<WPARAM>(image.m_hDC),PRF_CLIENT);
    BITMAPFILEHEADER header{}; header.bfType=0x4D42; header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER); header.bfSize=header.bfOffBits+bounds.Width()*bounds.Height()*4;
    std::ofstream preview((directory+P_("composer-keyboard.bmp")).AsNative(),std::ios::binary);
    preview.write(reinterpret_cast<const char *>(&header),sizeof(header)); preview.write(reinterpret_cast<const char *>(&info.bmiHeader),sizeof(BITMAPINFOHEADER));
    preview.write(static_cast<const char *>(pixels),bounds.Width()*bounds.Height()*4); preview.close(); ::SelectObject(image,previous); ::DeleteObject(bitmap);
    mapper.DestroyWindow();
    check(file.ChnSettings[4].inputKeys[72/8]==0,"cancelled mapping edits leave the document unchanged");
    m_Status.set(psRecordingEnabled); m_stepLength=4; m_stepAdvance=true; m_armedComposite=CHANNELINDEX_INVALID;
    SetCurrentColumn(4); SetCurrentRow(0,WrapMode::LimitAtPatternEnd); ComposerStepNote(NOTE_MIDDLEC,-1); ComposerStepNote(NOTE_MIDDLEC+2,-1);
    check(file.Patterns[m_nPattern].GetpModCommand(0,4)->note==NOTE_MIDDLEC && file.Patterns[m_nPattern].GetpModCommand(4,4)->note==NOTE_MIDDLEC+2 && file.Patterns[m_nPattern].GetpModCommand(8,4)->note==NOTE_KEYOFF,"consecutive steps replace their preceding note end");
    m_trackSelection.Click(4,6,false,false); ComposerStepAction(ID_COMPOSER_TIE);
    check(file.Patterns[m_nPattern].GetpModCommand(8,4)->note==NOTE_NONE && file.Patterns[m_nPattern].GetpModCommand(12,4)->note==NOTE_KEYOFF,"tie extends the previous note without retriggering");
    ComposerAdd(true,true); check(file.ChnSettings[0].isMaster && file.GetNumChannels()==7,"master created once at the front"); ComposerAdd(true,true); check(file.GetNumChannels()==7,"master button reuses the existing master");
    const auto savedColorRules=SynMPTColorRules(); SynMPTColorRules()="1|bass|#86A8D8\n1|test|#FFFFFF\n";
    auto &colored=file.ChnSettings[2]; colored.szName="BASS test"; colored.trackAutoColor=true; SynMPTApplyAutoColor(colored); check(colored.color==RGB(134,168,216),"ordered keyword colors ignore case");
    colored.trackAutoColor=false; colored.color=RGB(1,2,3); SynMPTApplyAutoColor(colored); check(colored.color==RGB(1,2,3),"manual color is preserved");
    SynMPTColorRules()=savedColorRules;
    ComposerOpenEditor(ID_VIEW_SAMPLES); ComposerOpenEditor(ID_VIEW_INSTRUMENTS);
    MSG queued{}; unsigned dispatched=0;
    while(::PeekMessage(&queued,nullptr,0,0,PM_REMOVE))
    {
        if(++dispatched>=10000 || queued.message==WM_QUIT) check(false,"separate editor initialization settles");
        ::TranslateMessage(&queued); ::DispatchMessage(&queued);
    }
    size_t samples=0,instruments=0;
    auto editors=doc->GetFirstViewPosition(); while(editors)
    {
        auto *view=doc->GetNextView(editors);
        samples+=view->IsKindOf(RUNTIME_CLASS(CViewSample))?1:0;
        instruments+=view->IsKindOf(RUNTIME_CLASS(CViewInstrument))?1:0;
    }
    check(samples==1 && instruments==1,"separate editors initialize the requested sample and instrument views");
    size_t views=0; auto position=doc->GetFirstViewPosition(); while(position) { doc->GetNextView(position); views++; }
    check(views>=6,"separate sample and instrument editors share the document");
    std::vector<bool> keep(file.GetNumChannels(),false); doc->RemoveChannels(keep); check(file.IsEmptyWorkspace(),"last track removal returns to empty workspace");
    OnEditUndo(); check(file.GetNumChannels()==7 && !file.IsEmptyWorkspace(),"empty workspace transition is undoable");
    const auto native=directory+P_("composer-native.synmpt");
    check(doc->SaveFile(native,true),"save native project atomically");
    std::ifstream original(native.AsNative(),std::ios::binary); const std::string before(std::istreambuf_iterator<char>(original),{}); original.close();
    file.ChnSettings[0].szName="Master renamed"; check(doc->SaveFile(native,true),"replace native project safely");
    std::ifstream recovery((native+P_(".bak")).AsNative(),std::ios::binary); const std::string backup(std::istreambuf_iterator<char>(recovery),{});
    check(before==backup,"recovery file retains the exact previous song bytes");
    {
        ScopedLogCapturer expectedWarning(*doc,{},nullptr,false);
        doc->m_conversionSource=native; check(!doc->SaveFile(native,true),"converted document cannot overwrite its source"); doc->m_conversionSource={};
    }
    doc->SetModified(false);
    std::vector<HWND> hosts;
    auto openViews=doc->GetFirstViewPosition();
    while(openViews)
    {
        const auto root=::GetAncestor(doc->GetNextView(openViews)->m_hWnd,GA_ROOT);
        if(root!=CMainFrame::GetMainFrame()->m_hWnd && std::find(hosts.begin(),hosts.end(),root)==hosts.end()) hosts.push_back(root);
    }
    for(auto host:hosts) ::SendMessage(host,WM_CLOSE,0,0);
    views=0; position=doc->GetFirstViewPosition(); while(position) { doc->GetNextView(position); views++; }
    check(views==2,"closing separate editors keeps the original song and its views alive");
}
