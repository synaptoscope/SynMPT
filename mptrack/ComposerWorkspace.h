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

// A preset needs a stable instrument binding even when an imported track uses
// instrument numbers in its patterns. Do not silently choose one of several sounds.
static std::optional<uint16> ResolvePresetInstrument(const CSoundFile &song, CHANNELINDEX channel)
{
    const auto &track = song.ChnSettings[channel];
    if(track.trackInstrument || track.isBus) return track.trackInstrument;
    uint16 instrument = 0;
    for(const auto &pattern : song.Patterns)
        for(ROWINDEX row = 0; row < pattern.GetNumRows(); row++)
        {
            const auto &cell = *pattern.GetpModCommand(row, channel);
            if(cell.IsPcNote() || !cell.instr) continue;
            if(instrument && instrument != cell.instr) return std::nullopt;
            instrument = cell.instr;
        }
    return instrument;
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
        std::vector<uint16> bindings;
        for(auto channel : members)
        {
            const auto instrument = ResolvePresetInstrument(song, channel);
            if(!instrument)
            {
                Reporting::Information(MPT_CFORMAT("Track {} ({}) uses several instruments in its patterns. Choose an instrument in that track's header before saving the preset.")(
                    channel + 1, mpt::ToCString(song.GetCharsetInternal(), song.ChnSettings[channel].szName)));
                return;
            }
            bindings.push_back(*instrument);
        }
        auto dialog=SaveFileDialog().DefaultExtension(U_("synpreset")).DefaultFilename(mpt::PathString::FromUnicode(U_("Instrument.synpreset")))
            .ExtensionFilter("SynMPT instrument preset (*.synpreset)|*.synpreset||");
        if(path.empty()) { if(!dialog.Show(this)) return; path=dialog.GetFirstFile(); }
        auto preset=std::make_unique<CSoundFile>(); preset->Create(MOD_TYPE_MPT,static_cast<CHANNELINDEX>(members.size()),nullptr); preset->m_synmptVersion=1;
        preset->Patterns.Insert(0,64); preset->Order().resize(1,0);
        std::map<uint16,uint16> assets;
        for(CHANNELINDEX i=0;i<members.size();i++)
        {
            auto track=song.ChnSettings[members[i]];
            track.trackInstrument=bindings[i];
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
                    { preset->Instruments[target]=new ModInstrument(target); preset->Instruments[target]->name=song.GetSampleName(source); preset->m_nInstruments=target; ok=true; }
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
    ComposerAdd(true);
    const auto emptyPreset=directory+P_("composer-empty.synpreset"); ComposerPreset(true,emptyPreset);
    {
        auto loaded=std::make_unique<CSoundFile>(); mpt::IO::InputFile input(emptyPreset); auto reader=GetFileReader(input);
        check(loaded->Create(reader) && loaded->m_synmptVersion==1 && loaded->ChnSettings[0].isBus,"empty preset retains native identity and folder metadata without samples or pattern data");
    }
    ComposerPreset(false,emptyPreset);
    check(file.GetNumChannels()==2 && file.ChnSettings[1].isBus,"empty preset loads through the preset command");
    doc->RemoveChannels(std::vector<bool>(file.GetNumChannels(),false));
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
    // Imported lanes display their instrument from pattern notes without a header override.
    file.ChnSettings[1].trackInstrument=0;
    file.m_szNames[1]="Pattern guitar";
    auto &reference=*file.Patterns[0].GetpModCommand(0,1); reference.note=NOTE_MIDDLEC; reference.instr=1;
    auto &other=*file.Patterns[0].GetpModCommand(1,1); other.instr=2;
    check(!ResolvePresetInstrument(file,1),"multiple pattern instruments require an explicit choice");
    file.ChnSettings[1].trackInstrument=1;
    check(ResolvePresetInstrument(file,1)==1,"explicit preset binding takes precedence over pattern instruments");
    file.ChnSettings[1].trackInstrument=0; other={};
    SetCurrentColumn(0); const auto preset=directory+P_("composer-roundtrip.synpreset"); ComposerPreset(true,preset);
    check(file.ChnSettings[1].trackInstrument==0,"saving a pattern-derived preset leaves source track assignments unchanged");
    ComposerPreset(false,preset);
    check(file.GetNumChannels()==6 && file.ChnSettings[4].parentBus==3 && file.ChnSettings[5].parentBus==3,"preset restores independent folder hierarchy");
    check(file.ChnSettings[4].trackInstrument==file.ChnSettings[5].trackInstrument && file.ChnSettings[4].trackInstrument!=0,"pattern-derived and explicit references embed one shared instrument per preset instance");
    check(file.Instruments[file.ChnSettings[4].trackInstrument]->name==std::string("Pattern guitar"),"preset preserves sample-based instrument names");
    check(file.ChnSettings[4].inputUseKeys && file.ChnSettings[4].inputKeys[60/8] && file.ChnSettings[4].inputColor==RGB(77,188,215),"preset preserves keyboard ownership and color");
    const auto recalled=file.ChnSettings[4].trackInstrument;
    check(recalled!=file.ChnSettings[1].trackInstrument && file.Instruments[recalled],"preset uses independent instrument slots");
    const auto copiedSample=file.Instruments[recalled]->Keyboard[0]; check(copiedSample!=1 && file.GetSample(copiedSample).sample8()[4]==sample.sample8()[4],"preset embeds and copies sample data");
    const auto previousMidi = CMainFrame::GetMainFrame()->GetMidiRecordWnd();
    const bool wasModified = doc->IsModified() != FALSE; doc->SetModified(false);
    const auto patternBeforeAudition = std::vector<ModCommand>(file.Patterns[0].begin(), file.Patterns[0].end());
    CCompositeInputDlg mapper(*doc,3,this);
    check(mapper.Create(IDD_COMPOSITE_INPUT,this)!=FALSE,"keyboard mapper creates its controls");
    auto *keyboard=mapper.GetDlgItem(IDC_COMPOSITE_KEYBOARD); CRect keys; keyboard->GetClientRect(keys);
    const auto keyPoint=[&](int white){return MAKELPARAM((2*white+1)*keys.Width()/104,keys.Height()*4/5);};
    keyboard->SendMessage(WM_LBUTTONDOWN,MK_LBUTTON,keyPoint(23));
    check(mapper.m_voices.size()==1 && mapper.m_voices.front().voice.track==4 && mapper.m_voices.front().voice.note==NOTE_MIDDLEC,
        "mapping a key auditions only the selected child's fixed pitch");
    const auto mappedVoice=mapper.m_voices.front().voice;
    { CriticalSection cs; check(OwnsCompositePreview(*doc,mappedVoice),"mapping audition creates a real engine preview voice"); }
    keyboard->SendMessage(WM_MOUSEMOVE,MK_LBUTTON,keyPoint(27));
    keyboard->SendMessage(WM_LBUTTONUP,0,keyPoint(27));
    check(mapper.m_voices.empty(),"mouse release stops the mapping audition");
    const auto mapped=[&](int midi){return (mapper.tracks[4].inputKeys[midi/8]&(1<<(midi%8)))!=0;};
    check(mapped(60)&&mapped(61)&&mapped(67)&&!mapped(59)&&!mapped(68),"keyboard drag maps Middle C through G including black keys");
    keyboard->SendMessage(WM_LBUTTONDOWN,MK_LBUTTON|MK_CONTROL,keyPoint(30)); keyboard->SendMessage(WM_LBUTTONUP,0,keyPoint(30));
    check(mapped(72)&&!mapped(70),"Ctrl-click adds a separate mapped key");
    keyboard->SendMessage(WM_LBUTTONDOWN,MK_LBUTTON|MK_CONTROL,keyPoint(30)); keyboard->SendMessage(WM_LBUTTONUP,0,keyPoint(30));
    check(!mapped(72)&&mapped(60),"Ctrl-click removes a mapped key without clearing the range");
    mapper.m_fixed.SetCurSel(0); mapper.SendMessage(WM_COMMAND,MAKEWPARAM(IDC_COMPOSITE_FIXED,CBN_SELCHANGE),reinterpret_cast<LPARAM>(mapper.GetDlgItem(IDC_COMPOSITE_FIXED)->m_hWnd));
    mapper.m_root.SetCurSel(NOTE_MIDDLEC-12-NOTE_MIN); mapper.SendMessage(WM_COMMAND,MAKEWPARAM(IDC_COMPOSITE_ROOT,CBN_SELCHANGE),reinterpret_cast<LPARAM>(mapper.GetDlgItem(IDC_COMPOSITE_ROOT)->m_hWnd));
    check(mapper.tracks[4].inputTranspose==-12 && !mapper.tracks[4].inputFixedNote,"first-key pitch control sets semitone transposition");
    mapper.SendMessage(WM_COMMAND,MAKEWPARAM(IDC_COMPOSITE_MODE,BN_CLICKED));
    const auto keysBeforePlay=mapper.tracks[4].inputKeys;
    keyboard->SendMessage(WM_LBUTTONDOWN,MK_LBUTTON,keyPoint(23));
    check(mapper.m_voices.size()==2 && mapper.m_voices[0].voice.note==NOTE_MIDDLEC-12 && mapper.m_voices[1].voice.note==NOTE_MIDDLEC,
        "play mapping auditions overlapping lanes at their own sounding pitches");
    keyboard->SendMessage(WM_MOUSEMOVE,MK_LBUTTON,keyPoint(27)); keyboard->SendMessage(WM_LBUTTONUP,0,keyPoint(27));
    check(mapper.tracks[4].inputKeys==keysBeforePlay,"play mapping leaves key assignments unchanged while dragging");
    mapper.SendMessage(WM_MOD_MIDIMSG,0x90u | (60u<<8) | (90u<<16));
    mapper.SendMessage(WM_MOD_MIDIMSG,0x91u | (60u<<8) | (90u<<16));
    check(mapper.m_voices.size()==4,"MIDI channels can independently hold the same composite note");
    mapper.SendMessage(WM_MOD_MIDIMSG,0x80u | (60u<<8));
    check(mapper.m_voices.size()==2,"MIDI note-off releases only the owning input channel");
    mapper.SendMessage(WM_MOD_MIDIMSG,0xB1u | (64u<<8) | (127u<<16));
    mapper.SendMessage(WM_MOD_MIDIMSG,0x81u | (60u<<8));
    check(mapper.m_voices.size()==2 && !mapper.m_voices[0].held,"audition supports a held sustain pedal");
    mapper.SendMessage(WM_MOD_MIDIMSG,0xB1u | (64u<<8));
    check(mapper.m_voices.empty(),"sustain release stops deferred notes");
    mapper.m_virtualKey='Q';
    const int keyboardOffset=NOTE_MIDDLEC-doc->GetNoteWithBaseOctave(0,0);
    mapper.SendMessage(WM_MOD_KEYCOMMAND,kcInstrumentStartNotes+keyboardOffset);
    check(mapper.m_voices.size()==2,"computer note shortcuts audition the complete composite");
    MSG keyup{}; keyup.message=WM_KEYUP; keyup.wParam='Q';
    check(mapper.PreTranslateMessage(&keyup) && mapper.m_voices.empty(),"computer key release stops notes even after focus moves");
    mapper.SendMessage(WM_COMMAND,MAKEWPARAM(IDC_COMPOSITE_MODE,BN_CLICKED));
    keyboard->SendMessage(WM_LBUTTONDOWN,MK_LBUTTON,keyPoint(23));
    keyboard->SendMessage(WM_CAPTURECHANGED,0,0);
    check(mapper.m_voices.empty(),"losing mouse capture releases mapping notes");
    mapper.SendMessage(WM_MOD_MIDIMSG,0x90u | (60u<<8) | (90u<<16));
    check(!doc->IsModified() && std::equal(patternBeforeAudition.begin(),patternBeforeAudition.end(),file.Patterns[0].begin()),
        "mouse, computer and MIDI audition leave song notes and modified flag unchanged");
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
    {
        CriticalSection cs; auto settings=file.ChnSettings;
        file.ChnSettings[4]=mapper.tracks[4];
        const auto pitchPreset=directory+P_("composer-pitch.synpreset"); SetCurrentColumn(3); ComposerPreset(true,pitchPreset);
        file.ChnSettings=std::move(settings);
        auto loaded=std::make_unique<CSoundFile>(); mpt::IO::InputFile input(pitchPreset); auto reader=GetFileReader(input);
        check(loaded->Create(reader) && loaded->ChnSettings[1].inputTranspose==-12 && loaded->ChnSettings[1].inputFixedNote==0,
            "transposed keyboard mapping survives preset save and reload");
    }
    // Paint the complete dialog into owned memory so its new controls can be inspected without capturing the desktop.
    mapper.GetWindowRect(bounds); bounds.MoveToXY(0,0); info.bmiHeader.biWidth=bounds.Width(); info.bmiHeader.biHeight=-bounds.Height();
    bitmap=::CreateDIBSection(image,&info,DIB_RGB_COLORS,&pixels,nullptr,0); previous=::SelectObject(image,bitmap);
    image.FillSolidRect(bounds,SynMPTTheme::Color(COLOR_BTNFACE));
    mapper.SendMessage(WM_PRINT,reinterpret_cast<WPARAM>(image.m_hDC),PRF_NONCLIENT|PRF_CLIENT|PRF_CHILDREN|PRF_ERASEBKGND);
    header.bfSize=header.bfOffBits+bounds.Width()*bounds.Height()*4;
    std::ofstream dialogPreview((directory+P_("composer-audition.bmp")).AsNative(),std::ios::binary);
    dialogPreview.write(reinterpret_cast<const char *>(&header),sizeof(header)); dialogPreview.write(reinterpret_cast<const char *>(&info.bmiHeader),sizeof(BITMAPINFOHEADER));
    dialogPreview.write(static_cast<const char *>(pixels),bounds.Width()*bounds.Height()*4); dialogPreview.close(); ::SelectObject(image,previous); ::DeleteObject(bitmap);
    mapper.SendMessage(WM_ACTIVATE,WA_INACTIVE,0);
    mapper.SendMessage(WM_MOD_MIDIMSG,0x90u | (60u<<8) | (90u<<16));
    check(mapper.m_voices.empty() && CMainFrame::GetMainFrame()->GetMidiRecordWnd()==mapper.m_hWnd,
        "inactive mapper stops audition and keeps MIDI away from the recording view");
    mapper.SendMessage(WM_ACTIVATE,WA_ACTIVE,0);
    mapper.SendMessage(WM_MOD_MIDIMSG,0x90u | (60u<<8) | (90u<<16));
    check(mapper.m_voices.size()==2,"reactivating the mapper permits audition again");
    mapper.DestroyWindow();
    check(mapper.m_voices.empty() && CMainFrame::GetMainFrame()->GetMidiRecordWnd()==previousMidi,
        "closing mapper stops held notes and restores the previous MIDI destination");
    check(file.ChnSettings[4].inputFixedNote==NOTE_MIDDLEC && file.ChnSettings[4].inputTranspose==0,
        "cancelled pitch changes leave the document mapping unchanged");
    check(file.ChnSettings[4].inputKeys[72/8]==0,"cancelled mapping edits leave the document unchanged");
    SetCurrentColumn(3); SetCurrentRow(0,WrapMode::LimitAtPatternEnd); m_armedComposite=CHANNELINDEX_INVALID;
    SendCtrlMessage(CTRLMSG_SETRECORD,0);
    auto *controlView=dynamic_cast<CModControlView *>(CWnd::FromHandlePermanent(m_hWndCtrl));
    auto *controlPage=controlView ? controlView->GetCurrentControlDlg() : nullptr;
    check(!IsEditingEnabled() && controlPage && controlPage->IsDlgButtonChecked(IDC_COMPOSER_PLAY_ONLY)==BST_CHECKED,
        "Play only button reflects the shared recording state");
    const auto beforePlayCursor=m_Cursor;
    TempEnterNote(NOTE_MIDDLEC,-1,false);
    check(m_compositeVoices[NOTE_MIDDLEC].size()==2,"unarmed selected composite plays in Play only mode");
    TempStopNote(NOTE_MIDDLEC,true);
    check(m_compositeVoices[NOTE_MIDDLEC].empty() && m_Cursor==beforePlayCursor && !doc->IsModified()
        && std::equal(patternBeforeAudition.begin(),patternBeforeAudition.end(),file.Patterns[0].begin()),
        "Play only note-on/off leaves patterns, cursor and modified state untouched");
    TempEnterNote(NOTE_MIDDLEC,-1,false); SendCtrlMessage(CTRLMSG_SETRECORD,1); TempStopNote(NOTE_MIDDLEC,true);
    check(std::equal(patternBeforeAudition.begin(),patternBeforeAudition.end(),file.Patterns[0].begin()),
        "enabling recording before releasing an auditioned note does not write a stray note-off");
    m_armedComposite=3; SetCurrentRow(20,WrapMode::LimitAtPatternEnd); TempEnterNote(NOTE_MIDDLEC,-1,false);
    check(file.Patterns[0].GetpModCommand(20,4)->note==NOTE_MIDDLEC && file.Patterns[0].GetpModCommand(20,5)->note==NOTE_MIDDLEC,
        "armed composite still writes both child notes in record mode");
    StopCompositeVoices(); *file.Patterns[0].GetpModCommand(20,4)={}; *file.Patterns[0].GetpModCommand(20,5)={};
    doc->SetModified(wasModified);
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
    doc->RemoveChannels(std::vector<bool>(file.GetNumChannels(),false));
    ComposerPreset(false,preset);
    check(file.GetNumChannels()==3 && file.ChnSettings[0].isBus && file.ChnSettings[1].parentBus==0 && !file.IsEmptyWorkspace(),"preset replaces the empty workspace placeholder with its folder and children");
    const auto fresh=file.ChnSettings[1].trackInstrument;
    check(fresh && file.Instruments[fresh] && file.GetSample(file.Instruments[fresh]->Keyboard[0]).sample8()[4]==-12,"preset recalled into an empty workspace contains playable sample bytes");
    // Optional local reproduction files. Read-only inputs; recovered output is kept
    // in the isolated test directory. No owner paths or media enter the repository.
    const auto fixturePath=[](const wchar_t *name) {
        wchar_t value[32768]{}; const auto size=::GetEnvironmentVariableW(name,value,std::size(value));
        return size>0 && size<std::size(value) ? mpt::PathString::FromNative(value) : mpt::PathString{};
    };
    const auto suppliedPreset=fixturePath(L"SYNMPT_CHECK_PRESET");
    const auto suppliedSource=fixturePath(L"SYNMPT_CHECK_SOURCE");
    if(!suppliedPreset.empty())
    {
        const auto base=file.GetNumChannels(); ComposerPreset(false,suppliedPreset);
        check(file.GetNumChannels()>base && file.ChnSettings[base].isBus,"supplied preset loads through the actual preset command");
        if(!suppliedSource.empty())
        {
            auto source=std::make_unique<CSoundFile>(); mpt::IO::InputFile input(suppliedSource); auto reader=GetFileReader(input);
            check(source->Create(reader),"load read-only source song for preset recovery");
            std::map<uint16,uint16> recoveredAssets;
            for(CHANNELINDEX channel=base;channel<file.GetNumChannels();channel++)
            {
                auto &track=file.ChnSettings[channel]; if(track.isBus) continue;
                CHANNELINDEX match=CHANNELINDEX_INVALID; size_t matches=0;
                for(CHANNELINDEX sourceChannel=0;sourceChannel<source->GetNumChannels();sourceChannel++)
                    if(source->ChnSettings[sourceChannel].szName==track.szName) { match=sourceChannel; matches++; }
                check(matches==1,"each recovered child has exactly one source track with the same name");
                const auto instrument=ResolvePresetInstrument(*source,match);
                check(instrument && *instrument,"each recovered child has one unambiguous pattern instrument");
                if(!recoveredAssets.count(*instrument))
                {
                    const auto target=static_cast<INSTRUMENTINDEX>(file.GetNumInstruments()+1);
                    if(source->GetNumInstruments()) check(file.ReadInstrumentFromSong(target,*source,*instrument),"recover source instrument");
                    else
                    {
                        const auto recoveredSample=static_cast<SAMPLEINDEX>(file.GetNumSamples()+1);
                        check(file.ReadSampleFromSong(recoveredSample,*source,*instrument),"recover source sample");
                        file.Instruments[target]=new ModInstrument(recoveredSample); file.Instruments[target]->name=source->GetSampleName(*instrument); file.m_nInstruments=target;
                    }
                    recoveredAssets[*instrument]=target;
                }
                track.trackInstrument=recoveredAssets[*instrument];
            }
            SetCurrentColumn(base); const auto recovered=directory+P_("recovered-instrument.synpreset"); ComposerPreset(true,recovered);
            auto saved=std::make_unique<CSoundFile>(); mpt::IO::InputFile savedInput(recovered); auto savedReader=GetFileReader(savedInput);
            check(saved->Create(savedReader) && saved->m_synmptVersion==1 && saved->GetNumChannels()==file.GetNumChannels()-base,"recovered preset saves and reopens with the full hierarchy");
            for(CHANNELINDEX channel=1;channel<saved->GetNumChannels();channel++)
            {
                if(saved->ChnSettings[channel].isBus) continue;
                const auto beforeInstrument=file.ChnSettings[base+channel].trackInstrument, afterInstrument=saved->ChnSettings[channel].trackInstrument;
                check(afterInstrument && saved->Instruments[afterInstrument],"recovered preset embeds every child instrument");
                for(size_t key=0;key<saved->GetModSpecifications().noteMax;key++)
                {
                    const auto beforeSample=file.Instruments[beforeInstrument]->Keyboard[key], afterSample=saved->Instruments[afterInstrument]->Keyboard[key];
                    if(!beforeSample) { check(!afterSample,"recovery preserves unmapped keys"); continue; }
                    const auto &beforeData=file.GetSample(beforeSample), &afterData=saved->GetSample(afterSample);
                    const bool same=afterSample && beforeData.HasSampleData() && afterData.HasSampleData() && beforeData.GetSampleSizeInBytes()==afterData.GetSampleSizeInBytes()
                        && std::memcmp(beforeData.sampleb(),afterData.sampleb(),beforeData.GetSampleSizeInBytes())==0;
                    if(!same) progress<<"Sample mismatch: child "<<channel<<", key "<<key<<", instruments "<<beforeInstrument<<"/"<<afterInstrument<<", samples "<<beforeSample<<"/"<<afterSample
                        <<", lengths "<<beforeData.nLength<<"/"<<afterData.nLength<<", bytes "<<beforeData.GetSampleSizeInBytes()<<"/"<<afterData.GetSampleSizeInBytes()<<'\n';
                    check(same,"recovered preset sample bytes match the source");
                }
            }
            const auto previousTracks=file.GetNumChannels(); ComposerPreset(false,recovered);
            check(file.GetNumChannels()==previousTracks+saved->GetNumChannels(),"recovered preset reloads through the actual preset command");
        }
    }
    doc->SetModified(false);
}
