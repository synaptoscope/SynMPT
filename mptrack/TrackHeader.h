/* SPDX-License-Identifier: BSD-3-Clause */
#pragma once
#include "../common/mptBaseTypes.h"
#include "../soundlib/TrackRouting.h"
#include <algorithm>
#include <limits>
#include <vector>

OPENMPT_NAMESPACE_BEGIN

// Use the rounded row sizes for every boundary, including meters and plugins.
struct TrackHeaderLayout
{
    int nameHeight, rowHeight, ownershipStep, cornerSize;
    explicit constexpr TrackHeaderLayout(int dpi)
        : nameHeight((20 * dpi + 48) / 96), rowHeight((18 * dpi + 48) / 96),
          ownershipStep((10 * dpi + 48) / 96), cornerSize((8 * dpi + 48) / 96) { }
    // Keep deep folders compact; the parent-color guide still identifies their owner.
    constexpr int TrackTop(size_t depth) const { return static_cast<int>(std::min<size_t>(depth, 3)) * ownershipStep; }
    constexpr int ControlsBottom() const { return nameHeight + rowHeight * 4; }
};

// The same outline governs painting and hit testing, including the empty clipped ear.
struct TrackOutline
{
    int left, top, right, bottom, corner;
    bool Contains(int x, int y) const
    {
        return x >= left && x < right && y >= top && y < bottom
            && (corner == 0 || x - (right - 1 - corner) <= y - top);
    }
};

inline size_t TrackOwnershipDepth(const std::vector<ModChannelSettings> &tracks, CHANNELINDEX channel)
{
    size_t depth = 0;
    return VisitTrackBuses(tracks, channel, [&](CHANNELINDEX) { depth++; }) ? depth : 0;
}

// Folder headers have their own cap; siblings share a square top until their last member.
inline bool TrackEndsOwnedRun(const std::vector<ModChannelSettings> &tracks, CHANNELINDEX channel, CHANNELINDEX nextVisible)
{
    if(channel >= tracks.size()) return false;
    const auto &track = tracks[channel];
    if(track.isBus || track.parentBus >= tracks.size() || !tracks[track.parentBus].isBus) return true;
    return !TrackBelongsToBus(tracks, nextVisible, track.parentBus);
}

class TrackHeaderSelection
{
public:
    static constexpr size_t Invalid = std::numeric_limits<size_t>::max();
private:
    std::vector<bool> selected;
    size_t anchor = Invalid;
public:
    size_t Size() const { return selected.size(); }
    size_t Count() const { return static_cast<size_t>(std::count(selected.begin(), selected.end(), true)); }
    bool Contains(size_t track) const { return track < selected.size() && selected[track]; }
    size_t Anchor() const { return anchor; }
    const std::vector<bool> &Mask() const { return selected; }
    void Click(size_t track, size_t count, bool control, bool shift)
    {
        if(track >= count) return;
        selected.resize(count, false);
        if(shift)
        {
            if(anchor >= count) anchor = track;
            if(!control) std::fill(selected.begin(), selected.end(), false);
            for(size_t index = std::min(anchor, track); index <= std::max(anchor, track); index++) selected[index] = true;
        } else
        {
            if(!control) std::fill(selected.begin(), selected.end(), false);
            selected[track] = control ? !selected[track] : true;
            anchor = track;
        }
    }
    std::vector<bool> Targets(size_t track, size_t count) const
    {
        if(Contains(track) && selected.size() == count) return selected;
        std::vector<bool> result(count, false);
        if(track < count) result[track] = true;
        return result;
    }
    void Restore(const std::vector<bool> &mask, size_t oldAnchor)
    {
        selected = mask; anchor = oldAnchor < selected.size() ? oldAnchor : Invalid;
    }
    template<typename Index> void Remap(const std::vector<Index> &order)
    {
        const auto previous = selected; const auto previousAnchor = anchor;
        selected.assign(order.size(), false); anchor = Invalid;
        for(size_t index = 0; index < order.size(); index++)
        {
            if(order[index] < previous.size()) selected[index] = previous[order[index]];
            if(order[index] == previousAnchor) anchor = index;
        }
    }
};

OPENMPT_NAMESPACE_END
