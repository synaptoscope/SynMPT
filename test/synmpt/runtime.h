/* SPDX-License-Identifier: BSD-3-Clause */
#pragma once
#include "../../soundlib/Sndfile.h"
#include "../../soundlib/TrackRouting.h"
#include "mpt/io_file/fstream.hpp"
#include "mpt/io_read/filecursor_stdstream.hpp"
#include <stdexcept>
#include <sstream>
#include <cmath>

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
        const auto saved=directory+P_("synmpt-runtime-roundtrip.mptm");
        {mpt::IO::ofstream output(saved,std::ios::binary);check(file->SaveIT(output,saved,false),"save MPTM");}
        auto loaded=std::make_unique<CSoundFile>();
        {mpt::IO::ifstream input(saved,std::ios::binary);auto cursor=mpt::IO::make_FileCursor<mpt::PathString>(input);check(loaded->Create(cursor),"reload MPTM");}
        check(loaded->ChnSettings[0].trackInstrument==2 && loaded->ChnSettings[0].parentBus==2 && loaded->ChnSettings[2].parentBus==3 && loaded->ChnSettings[2].isBus && loaded->ChnSettings[2].dwFlags[CHN_MUTE],"MPTM routing/instrument/bus mute round trip");
        log<<"PASS: rendered bus gain, nested buses, pan, mute, native bus automation, instrument override and MPTM save/reload\n";
        for(const auto &song:songs)
        {
            auto playback=std::make_unique<CSoundFile>();
            mpt::IO::ifstream stream(song,std::ios::binary);auto cursor=mpt::IO::make_FileCursor<mpt::PathString>(stream);
            check(playback->Create(cursor),"load reference song");
            MixerSettings mixer;mixer.gdwMixingFreq=8000;mixer.gnChannels=2;playback->SetMixerSettings(mixer);
            playback->ResetPlayPos();playback->m_PlayState.m_flags.reset(SONG_PATTERNLOOP);playback->SetRepeatCount(0);playback->InitPlayer(true);
            EnergyTarget target;ORDERINDEX furthest=0;
            for(int block=0;block<4000 && furthest<2;block++)
            {if(!playback->Read(1024,target))break;furthest=std::max(furthest,playback->m_PlayState.m_nCurrentOrder);}
            check(furthest>=2,"reference song must advance past opening orders");
            check(target.left+target.right>0,"reference song must produce audio");
            log<<"PASS: reference song advances to order "<<furthest<<" with audible output\n";
        }
        log<<"ALL CHECKS PASSED\n";log.flush();return 0;
    } catch(const std::exception &error) {log<<"FAIL: "<<error.what()<<'\n';log.flush();return 1;}
}
}
OPENMPT_NAMESPACE_END
