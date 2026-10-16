# SynMPT document tabs and toolbar layout

Date: 2026-10-16 (America/New_York)
Branch: `feature/track-workspace`
Implementation commit: `21e2cbb913af75f0257fa592f1096af7624ff217`
Base: `94663e4c6`

## Built

- Added a permanent, single-row song document tab strip above the docked toolbars. It uses the existing MDI documents, views and activation rather than introducing another document model.
- Each open song has a filename/title tab, active highlight, unsaved marker, a close cross and a full-path tooltip. Long names use ellipsis; excess tabs use the native horizontal scroll control. Tab width is 200 DIP and the strip is 30 DIP high. Both scale with display DPI.
- Clicking a tab activates its existing song frame. Cross and middle-click close the corresponding native frame, preserving its normal Save / Discard / Cancel path. Refreshes are deferred and coalesced after native activation, title and destruction events.
- The editor toolbar and transport are docked beside each other by default, below the song tabs. This is a one-time layout migration; later user placement is retained. At insufficient width, MFC can wrap the bars normally.
- Added View > Dock Transport Beside Toolbar to restore this compact arrangement. Existing floating transport, visibility and Bring Transport Here controls remain available.

## Verification

- Windows 10 x64 Release build passed with MSVC v145 and SDK 10.0.26100.0. Log: `bin/synmpt-tabs-build.log`. No compiler errors or compiler warnings were found; the existing optional subwcrev notice remains.
- Isolated engine checks passed for rendered group gain, nested buses, stereo balance, mute, native automation, instrument override and MPTM save/reload. Yuzu - Yu-Lib advanced to order 2 with audible output. Log: `bin/synmpt-tabs-preview/synmpt-runtime-checks.log`.
- The new preview launched successfully. Accessibility inspection confirmed the Song tabs control and tabs for two disposable documents, Module1 and Module2. Closing Module2 with the native Close command removed its tab and left Module1. No test song files were saved.
- Screen capture failed with `FrameArrived timed out`; indexed mouse interaction reported `coordinate input geometry is unavailable`. Visual spacing, direct tab clicks, close-cross/middle-click hit areas, overflow and mixed DPI remain for hands-on verification. A later cleanup keystroke reported user input in the preview, so UI interaction stopped and the preview was retained for the owner.
- Staged whitespace check passed with CRLF recognized.

## Preview and review

Executable: `bin/synmpt-tabs-preview/OpenMPT.exe`. It has its own portable settings copied from the previous preview, with single-instance forwarding disabled. The owner's earlier selection-preview process and its unsaved song were retained.

1. Open two songs and check the tabs sit above one shared toolbar row at the normal window width.
2. Switch tabs; change a song and check its unsaved marker; cancel closing it, then save/close normally. Try the close cross and middle-click.
3. Float the transport and use View > Dock Transport Beside Toolbar to restore the compact row. Check long filenames and several open songs as convenient.

Footer: This is the single handoff file for this pass. No synced project sources or SynCAD canon documents were edited. Final visual acceptance remains with the owner.
