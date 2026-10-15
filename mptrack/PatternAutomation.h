/* SPDX-License-Identifier: BSD-3-Clause */
#pragma once
#include "../common/mptBaseTypes.h"
#include "../soundlib/ModCommand.h"
#include <algorithm>
#include <optional>
#include <vector>
#include <cmath>

OPENMPT_NAMESPACE_BEGIN

enum class AutomationLane { Pan, Volume };
enum class AutomationEdit { Unchanged, Changed, Blocked, Unsupported };
struct AutomationFormat
{
	bool channelVolume = false, panEffect = false, panVolume = false;
	int panEffectMax = 255, panVolumeStep = 1;
};

inline std::optional<int> ReadAutomationValue(const ModCommand &m, AutomationLane lane, const AutomationFormat &format)
{
	if(m.IsPcNote()) return std::nullopt;
	if(lane == AutomationLane::Volume)
		return m.command == CMD_CHANNELVOLUME ? std::optional<int>(std::min<int>(m.param, 64)) : std::nullopt;
	if(m.command == CMD_PANNING8 && m.param <= format.panEffectMax)
		return (m.param * 64 + format.panEffectMax / 2) / format.panEffectMax;
	if(m.volcmd == VOLCMD_PANNING)
		return std::min<int>(m.vol, 64);
	return std::nullopt;
}

inline AutomationEdit WriteAutomationValue(ModCommand &m, AutomationLane lane, int value, const AutomationFormat &format)
{
	if(m.IsPcNote()) return AutomationEdit::Blocked;
	value = std::clamp(value, 0, 64);
	auto next = m;
	const int panVolume = (value / format.panVolumeStep) * format.panVolumeStep;
	if(lane == AutomationLane::Volume)
	{
		if(!format.channelVolume) return AutomationEdit::Unsupported;
		if(next.command == CMD_PANNING8 && next.param <= format.panEffectMax && format.panVolume
			&& (next.volcmd == VOLCMD_NONE || next.volcmd == VOLCMD_PANNING))
		{
			// Share a cell with Pan through the native volume column.
			next.volcmd = VOLCMD_PANNING;
			next.vol = static_cast<ModCommand::VOL>(((next.param * 64 + format.panEffectMax / 2) / format.panEffectMax / format.panVolumeStep) * format.panVolumeStep);
			next.command = CMD_NONE;
		}
		if(next.command != CMD_NONE && next.command != CMD_CHANNELVOLUME) return AutomationEdit::Blocked;
		next.command = CMD_CHANNELVOLUME;
		next.param = static_cast<ModCommand::PARAM>(value);
	} else if(next.command == CMD_PANNING8 && format.panEffect)
	{
		if(next.param > format.panEffectMax) return AutomationEdit::Blocked;
		next.param = static_cast<ModCommand::PARAM>((value * format.panEffectMax + 32) / 64);
	} else if(format.panVolume && (next.volcmd == VOLCMD_NONE || next.volcmd == VOLCMD_PANNING))
	{
		next.volcmd = VOLCMD_PANNING;
		next.vol = static_cast<ModCommand::VOL>(panVolume);
	} else if(format.panEffect && next.command == CMD_NONE)
	{
		next.command = CMD_PANNING8;
		next.param = static_cast<ModCommand::PARAM>((value * format.panEffectMax + 32) / 64);
	} else
	{
		return format.panEffect || format.panVolume ? AutomationEdit::Blocked : AutomationEdit::Unsupported;
	}
	if(next == m) return AutomationEdit::Unchanged;
	m = next;
	return AutomationEdit::Changed;
}

inline AutomationEdit EraseAutomationValue(ModCommand &m, AutomationLane lane, const AutomationFormat &format = {})
{
	if(m.IsPcNote()) return AutomationEdit::Blocked;
	auto next = m;
	if(lane == AutomationLane::Volume && next.command == CMD_CHANNELVOLUME)
	{
		next.command = CMD_NONE; next.param = 0;
	}
	if(lane == AutomationLane::Pan)
	{
		if(next.command == CMD_PANNING8 && next.param <= format.panEffectMax) { next.command = CMD_NONE; next.param = 0; }
		if(next.volcmd == VOLCMD_PANNING) { next.volcmd = VOLCMD_NONE; next.vol = 0; }
	}
	if(next == m) return AutomationEdit::Unchanged;
	m = next;
	return AutomationEdit::Changed;
}

struct AutomationNode { int row = 0, value = 0; };

inline int AutomationLineValue(AutomationNode start, AutomationNode end, int row)
{
    if(start.row == end.row) return end.value;
    return std::clamp(static_cast<int>(std::lround(start.value + static_cast<double>(end.value - start.value) * (row - start.row) / (end.row - start.row))), 0, 64);
}

// Recover editable corners from the saved native row values. A half-step of
// rounding must not turn a straight tracker ramp into a node on every row.
inline std::vector<AutomationNode> ReduceAutomationNodes(const std::vector<std::optional<int>> &points, int initial, int tolerance = 1)
{
    std::vector<AutomationNode> result;
    int first = -1, last = -1, value = initial;
    std::vector<int> values(points.size());
    for(size_t row = 0; row < points.size(); row++)
    {
        if(points[row]) { value = *points[row]; if(first < 0) first = static_cast<int>(row); last = static_cast<int>(row); }
        values[row] = value;
    }
    if(first < 0) return result;
    std::vector<bool> keep(points.size(), false); keep[first] = keep[last] = true;
    std::vector<std::pair<int, int>> pending{{first, last}};
    while(!pending.empty())
    {
        const auto [start, end] = pending.back(); pending.pop_back();
        int worst = -1; double error = tolerance;
        for(int row = start + 1; row < end; row++)
        {
            const double predicted = values[start] + static_cast<double>(values[end] - values[start]) * (row - start) / (end - start);
            const double distance = std::abs(predicted - values[row]);
            if(distance > error) { error = distance; worst = row; }
        }
        if(worst >= 0) { keep[worst] = true; pending.emplace_back(start, worst); pending.emplace_back(worst, end); }
    }
    for(int row = first; row <= last; row++) if(keep[row]) result.push_back({row, values[row]});
    return result;
}

OPENMPT_NAMESPACE_END
