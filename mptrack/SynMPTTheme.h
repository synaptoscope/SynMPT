/* SPDX-License-Identifier: BSD-3-Clause */
#pragma once
#include "openmpt/all/BuildSettings.hpp"

OPENMPT_NAMESPACE_BEGIN
namespace SynMPTTheme
{
COLORREF Color(int index);
HBRUSH Brush(int index);
COLORREF Mix(COLORREF background, COLORREF accent, unsigned percent);
// Draws only; native scrollbar geometry, accessibility and input stay with Windows.
void DrawScrollBar(HDC dc, const SCROLLBARINFO &info, bool vertical, bool hot);
void RegisterWindow(HWND window);
void RefreshWindows();
bool HandleKeyMessage(const MSG &message);
void DrawAction(HDC dc, const RECT &rect, const wchar_t *label, COLORREF accent, bool active, bool recordDot = false);
}
OPENMPT_NAMESPACE_END
