/* SPDX-License-Identifier: BSD-3-Clause */
#pragma once
#include "Moddoc.h"
#include "../soundlib/AudioCriticalSection.h"
#include "../soundlib/CompositeInput.h"

OPENMPT_NAMESPACE_BEGIN

struct CompositePreviewVoice
{
    CHANNELINDEX track, preview;
    ModCommand::NOTE note;
    uint16 instrument;
    uint64 id = 0;
};

// Call under the audio lock: a preview slot may have been reused since note-on.
inline bool OwnsCompositePreview(const CModDoc &doc, const CompositePreviewVoice &voice)
{
    const auto &file = doc.GetSoundFile();
    if(voice.preview >= MAX_CHANNELS || !voice.id || doc.GetPreviewNoteId(voice.preview) != voice.id) return false;
    const auto &channel = file.m_PlayState.Chn[voice.preview];
    return channel.isPreviewNote && channel.nMasterChn == voice.track + 1u
        && channel.nNewNote == voice.note
        && (file.GetNumInstruments() == 0 || (voice.instrument < MAX_INSTRUMENTS && channel.pModInstrument == file.Instruments[voice.instrument]));
}

inline CompositePreviewVoice PlayCompositePreview(CModDoc &doc, const CompositeNote &event, int volume)
{
    auto params = PlayNoteParam(event.note).Instrument(event.instrument).Volume(volume).Channel(event.channel);
    const auto preview = doc.PlayNote(params);
    if(preview < MAX_CHANNELS)
    {
        CriticalSection cs;
        auto &file = doc.GetSoundFile(); auto &voice = file.m_PlayState.Chn[preview];
        voice.nGlobalVol = file.m_PlayState.Chn[event.channel].nGlobalVol;
        voice.nPan = file.m_PlayState.Chn[event.channel].nPan;
        voice.dwFlags.set(CHN_MUTE, doc.IsChannelMuted(event.channel));
    }
    return {event.channel, preview, event.note, event.instrument, doc.GetPreviewNoteId(preview)};
}

OPENMPT_NAMESPACE_END
