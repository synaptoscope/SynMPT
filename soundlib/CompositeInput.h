/* SPDX-License-Identifier: BSD-3-Clause */
#pragma once
#include "TrackRouting.h"
#include "modcommand.h"
#include <algorithm>

OPENMPT_NAMESPACE_BEGIN

struct CompositeNote { CHANNELINDEX channel; ModCommand::NOTE note; uint16 instrument; };

// Input mapping happens when composing. Playback reads the resulting native notes.
inline std::vector<CompositeNote> ResolveCompositeInput(const std::vector<ModChannelSettings> &tracks, CHANNELINDEX bus, ModCommand::NOTE note)
{
    std::vector<CompositeNote> result;
    if(bus >= tracks.size() || !tracks[bus].isBus || !tracks[bus].compositeInput || !ModCommand::IsNote(note)) return result;
    for(CHANNELINDEX channel = 0; channel < tracks.size(); channel++)
    {
        const auto &track = tracks[channel];
        const int mapped = track.inputFixedNote ? track.inputFixedNote : note + track.inputTranspose;
        const int key = note - NOTE_MIN;
        const bool accepts = track.inputUseKeys ? (key >= 0 && key < 120 && (track.inputKeys[key / 8] & (1u << (key % 8))))
                                               : note >= track.inputNoteMin && note <= track.inputNoteMax;
        if(track.isBus || !track.inputEnabled || !track.trackInstrument || !accepts
            || mapped < NOTE_MIN || mapped > NOTE_MAX || !TrackBelongsToBus(tracks, channel, bus)) continue;
        result.push_back({channel, static_cast<ModCommand::NOTE>(mapped), track.trackInstrument});
    }
    return result;
}

inline bool WriteCompositeNote(ModCommand &cell, const CompositeNote &event, int volume)
{
    // Group input never silently replaces notes or plugin parameter cells.
    if(cell.note != NOTE_NONE || cell.instr || event.instrument >= MAX_INSTRUMENTS) return false;
    cell.note = event.note; cell.instr = static_cast<ModCommand::INSTR>(event.instrument);
    if(volume >= 0 && volume <= 64)
    {
        if(cell.volcmd == VOLCMD_NONE || cell.volcmd == VOLCMD_VOLUME) { cell.volcmd = VOLCMD_VOLUME; cell.vol = static_cast<ModCommand::VOL>(volume); }
        else if(cell.command == CMD_NONE) { cell.command = CMD_VOLUME; cell.param = static_cast<ModCommand::PARAM>(volume); }
    }
    return true;
}

inline bool TrackHiddenByFolder(const std::vector<ModChannelSettings> &tracks, CHANNELINDEX channel, const std::vector<bool> &collapsed)
{
    bool hidden = false;
    VisitTrackBuses(tracks, channel, [&](CHANNELINDEX bus) { if(bus < collapsed.size() && collapsed[bus]) hidden = true; });
    return hidden;
}

// Stable preorder makes every family contiguous without changing events within a track.
inline std::vector<CHANNELINDEX> TrackFolderOrder(const std::vector<ModChannelSettings> &tracks)
{
    std::vector<CHANNELINDEX> result, stack;
    std::vector<bool> used(tracks.size(), false);
    auto append = [&](CHANNELINDEX root) {
        stack.push_back(root);
        while(!stack.empty())
        {
            const auto current = stack.back(); stack.pop_back();
            if(used[current]) continue;
            used[current] = true; result.push_back(current);
            for(size_t child = tracks.size(); child > 0;)
            {
                --child;
                if(!used[child] && tracks[current].isBus && tracks[child].parentBus == current) stack.push_back(static_cast<CHANNELINDEX>(child));
            }
        }
    };
    for(CHANNELINDEX track = 0; track < tracks.size(); track++)
        if(tracks[track].parentBus >= tracks.size() || !tracks[tracks[track].parentBus].isBus) append(track);
    for(CHANNELINDEX track = 0; track < tracks.size(); track++) if(!used[track]) append(track);
    return result;
}

OPENMPT_NAMESPACE_END
