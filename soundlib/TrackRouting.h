/* SPDX-License-Identifier: BSD-3-Clause */
#pragma once
#include "ModChannel.h"
#include <vector>
#include <tuple>

OPENMPT_NAMESPACE_BEGIN

// Bounded parent walks also make malformed/cyclic files safe to play.
template <typename Visitor>
bool VisitTrackBuses(const std::vector<ModChannelSettings> &tracks, CHANNELINDEX track, Visitor visit)
{
    if(track >= tracks.size()) return false;
    auto parent = tracks[track].parentBus;
    for(size_t count = 0; parent != CHANNELINDEX_INVALID; count++)
    {
        if(count >= tracks.size() || parent >= tracks.size() || parent == track || !tracks[parent].isBus) return false;
        visit(parent);
        parent = tracks[parent].parentBus;
    }
    return true;
}

inline bool TrackBelongsToBus(const std::vector<ModChannelSettings> &tracks, CHANNELINDEX track, CHANNELINDEX bus)
{
    bool found = false;
    return VisitTrackBuses(tracks, track, [&](CHANNELINDEX parent) { found |= parent == bus; }) && found;
}


inline std::vector<bool> ToggleTrackSoloSet(const std::vector<bool> &current, const std::vector<bool> &targets)
{
    auto result = current; result.resize(targets.size(), false);
    bool allOn = true;
    for(size_t index = 0; index < targets.size(); index++) if(targets[index] && !result[index]) allOn = false;
    for(size_t index = 0; index < targets.size(); index++) if(targets[index]) result[index] = !allOn;
    return result;
}

inline std::vector<bool> TrackSoloSet(const std::vector<ModChannelSettings> &tracks, const std::vector<bool> &selected)
{
    std::vector<bool> audible(tracks.size(), false);
    for(CHANNELINDEX track = 0; track < tracks.size(); track++)
    {
        audible[track] = tracks[track].isMaster || (track < selected.size() && selected[track]);
        for(size_t master=0;master<tracks.size();master++) if(tracks[master].isMaster && master<selected.size() && selected[master]) audible[track]=true;
        VisitTrackBuses(tracks, track, [&](CHANNELINDEX bus) {
            if(bus < selected.size() && selected[bus]) audible[track] = true;
        });
    }
    for(CHANNELINDEX track = 0; track < tracks.size(); track++)
        if(audible[track]) VisitTrackBuses(tracks, track, [&](CHANNELINDEX bus) { audible[bus] = true; });
    return audible;
}

inline std::vector<bool> TrackSoloSet(const std::vector<ModChannelSettings> &tracks, CHANNELINDEX first, CHANNELINDEX last)
{
    std::vector<bool> selected(tracks.size(), false);
    for(size_t track = first; track <= last && track < tracks.size(); track++) selected[track] = true;
    return TrackSoloSet(tracks, selected);
}

struct TrackBusGain { uint32 left = 65536, right = 65536; };
template <typename Controls>
TrackBusGain GetTrackBusGain(const std::vector<ModChannelSettings> &tracks, CHANNELINDEX track, Controls controls)
{
    TrackBusGain gain;
    const bool valid = VisitTrackBuses(tracks, track, [&](CHANNELINDEX bus) {
        if(tracks[bus].isMaster) return; // Applied once to the final mix.
        const auto [volume, pan, muted] = controls(bus);
        gain.left = gain.left * (muted ? 0u : static_cast<uint32>(volume)) / 64u;
        gain.right = gain.right * (muted ? 0u : static_cast<uint32>(volume)) / 64u;
        gain.left = gain.left * static_cast<uint32>(pan <= 128 ? 128 : 256 - pan) / 128u;
        gain.right = gain.right * static_cast<uint32>(pan >= 128 ? 128 : pan) / 128u;
    });
    return valid ? gain : TrackBusGain{0, 0};
}

OPENMPT_NAMESPACE_END
