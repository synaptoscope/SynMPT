# SynMPT theme consistency fixes — 2026-10-17

Branch: `feature/track-workspace`

Implementation commit: `bdddfa9f6e7ca466640d99ccbe40f0484a7e49ec` — Stabilize scrollbar skins and fix gray theme text and captions.

## Built / decisions

- Replaced painting directly over native nonclient scrollbars with opaque sibling paint surfaces. Scrollbar owners use sibling clipping so Windows cannot paint the classic appearance over the rounded skin. The surfaces return `HTTRANSPARENT`, leaving native hit testing, accessibility, arrows/pages, capture and 32-bit thumb tracking on the original scrollbar. The same approach covers standalone scrollbar controls. Surfaces follow their owner's geometry/visibility and are destroyed with it. The tracking timer now refreshes the owned surface rather than racing native paint in the same area.
- Main and floating captions use the same lighter gray in active and inactive states. Kept native caption/button geometry and input behavior; drew readable labels and close/minimize/maximize/restore symbols. Disabled per-window DWM nonclient painting for this application so Windows 10 does not replace the requested caption gray with black. No system-wide setting is changed.
- Disabled static text, including Sequence, is drawn once in plain gray without Windows' white embossed shadow.
- The status bar draws light text and dark separators. MFC's deferred pane-text cache is flushed before the custom paint so status updates and underlying control text remain current.
- Applied consistent dark faces to plain instrument-selector dropdowns, numeric up/down arrows, inactive library selection, and the MDI menu-bar document buttons. MDI menu changes refresh their owner-drawn button styling. Custom/owner-drawn dropdowns keep their existing painters.

The prior icon-replacement backlog remains in FEATURES.md. No audio-engine, song-storage or track-layout changes were made in this fix.

## Verification

- Final Windows 10 x64 Release build passed with MSVC v145 and SDK 10.0.26100.0, with no compiler warnings/errors. Build log: `bin/synmpt-consistency-build.log`; existing optional `subwcrev`/`svn_version.h` notices remain.
- Extended `/synmptChecks` with owned hidden native controls and memory-bitmap rendering through the real `WM_PRINTCLIENT` routes. Checks passed for a readable disabled Sequence label without white embossing, light Ready status text, matching lighter active/inactive caption palette, persistent scrollbar surfaces, native hit-test passthrough, surface lifetime through activation/range changes, destruction with the viewport, and 32-bit positions 123456 and 800000.
- Existing rounded-thumb/hover/disabled paint, track tint/font/selection/button rendering, composite routing/layer audio, bus gain/pan/mute/automation, instrument override, MPTM save/reload, v1 compatibility and Yuzu order-2 progression checks passed. Log: `bin/synmpt-consistency-preview/synmpt-runtime-checks.log`.
- Isolated GUI preview launched with floating Transport. Created an empty Module1 and confirmed editor tabs, pattern workspace and native horizontal/vertical scrollbars via accessibility. Closed that empty test preview. This smoke check preceded the final small MDI-menu-refresh addition; the final saved implementation was rebuilt afterward.
- Screenshot capture again failed with `FrameArrived timed out: timed out waiting on channel`; a fresh-selection retry failed with `window capture timed out: timed out waiting on channel`. No visual pass or physical thumb-drag pass is claimed.
- Whitespace check passed with CRLF recognized. The owner's running preview/song was not targeted by UI input or edited.

## Preview / owner review

`C:/Users/Nate/Documents/GitHub/SynMPT/bin/synmpt-consistency-preview/OpenMPT.exe`

Own portable profile; single-instance forwarding is disabled. Earlier preview executables remain available.

1. Open the same song, drag both scrollbars and switch focus away/back. Confirm the rounded skin stays in place, and arrow/page/wheel actions remain usable.
2. Check the active main title and floating Transport caption against the inactive gray reference. Check caption dragging and window buttons.
3. Check Sequence, bottom status fields, instrument selector, numeric arrows, library selection and document-window buttons for readability. Check resizing and your normal display scaling.

Open question: any remaining inconsistent controls or preferred adjustments to the caption gray after visual review.

Footer: Single handoff file for this fix. No SynCAD canon documents, synced project sources, user screenshots or song files were edited. Native/control regression checks passed; actual desktop appearance and mouse-drag acceptance remain owner-review items.
