# SynMPT header overlap and track selection

Date: 2026-10-15 (America/New_York)
Branch: `feature/track-workspace`
Implementation commit: `71c22c7011b3595e055d75c008da5f8d886dddca`
Base: `d4b83589b`

## Built

- Fixed the overlap reported after expanding track headers. Both initial and live VU meters, and the plugin row, now start after the full name/action/instrument/volume/pan area. Removed the legacy record badge that still drew over the full-width name; record group 2 is shown on its dedicated R2 button.
- Shared header geometry rounds each row once, so the drawn controls and allocated height agree at fractional display scales. No additional header rows were introduced.
- Plain-click a track name to select it, Ctrl-click to toggle individual tracks, Shift-click for an anchored range, and Ctrl+Shift-click to add a range. Selected names have a visible highlight border. Clicking the note grid returns header selection to the clicked track.
- Mute on a selected track applies a consistent mute/unmute state to the selection. Mute on an unselected track affects that track alone. Solo adds/removes selected tracks from the active solo set, retaining other soloed tracks and necessary group outputs. Buttons, existing keyboard commands, Alt-header solo and the immediate context menu use this selection.
- Selection and remembered solo targets survive switching editor pages and follow track moves/duplication. Keyboard commands retain a selected track after Ctrl-deselect. Existing contiguous pattern-selection commands remain available.
- Creating a group from a non-adjacent header selection includes only selected roots, preserves already-nested members, and leaves intervening tracks out of the group.

## Decisions and limits

Header selection belongs to editor state and adds no song-file extension. Immediate Mute/Solo use native channel mute behavior. As in the existing tracker, clearing the final solo unmutes all channels; a separate prior-mute snapshot was not added. Manual mute changes can invalidate the remembered solo set. Pending on-pattern-transition commands retain their existing scope.

The new preview is `bin/synmpt-selection-preview/OpenMPT.exe`, with a separate portable configuration copied from the earlier preview and single-instance forwarding disabled. It was built for Windows 10 x64. The owner's running `synmpt-track-preview` instance was retained.

## Verification

- Windows 10 x64 Release build passed with MSVC v145 and SDK 10.0.26100.0. Log: `bin/synmpt-selection-build.log`.
- Focused native checks passed: Ctrl/Shift selection, non-adjacent targeting, deselection, selection remapping/restoration, additive multi-solo and group ancestors, and header bounds at 100%, 125%, 150%, 175% and 200% scales. Existing native automation, effect-collision, node, group gain/pan/mute and cycle checks also passed. Source: `test/synmpt/automation.cpp`; log: `bin/synmpt-automation-check.log`.
- Isolated engine checks passed for actual rendered audio, native bus automation, instrument override and MPTM save/reload. Yuzu - Yu-Lib advanced to order 2 with audible output. Log: `bin/synmpt-selection-preview/synmpt-runtime-checks.log`.
- Staged whitespace check passed with CRLF recognized.
- Read-only capture of the existing running preview failed twice: `FrameArrived timed out` and `window capture timed out`. No UI inputs were sent. The rebuilt preview's final appearance and mouse behavior remain unverified.

## Open review

1. Check that volume and pan are fully visible with VU meters and plugin names enabled at the owner's display scale.
2. Exercise Ctrl/Shift header selection and batch M/S, including a bus and its children, and switching pages.

Footer: This is the single handoff file for this pass. No synced project sources or SynCAD canon documents were edited.
