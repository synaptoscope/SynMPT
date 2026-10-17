# SynMPT dark window chrome and rounded scrollbars — 2026-10-17

Branch: `feature/track-workspace`

Implementation commit: `624cfc0a3a9832048b254b9f9ae55430b10eb3c4` — Darken native window chrome and add rounded scrollbar styling.

## Built

- App-local dark painting for captionless pane/input borders and floating tool-window frames/captions. Windows retains native caption hit testing, moving, resizing and closing. The main system title bar keeps its existing DWM dark treatment.
- Dark Song/Patterns/Samples/Instruments/Comments tabs with restrained rounded faces and a selected accent. Existing native tab selection and notifications stay in place. Owner-drawn song tabs continue to delegate their labels and close controls to the original parent renderer.
- Flat dark toolbar and library-pane borders, retaining MFC's exact border reservations. The editor splitter draws dark separators and pane outlines with its existing resizing behavior.
- Dark native list-column headers with text, image-list icons and sort indicators; existing owner-drawn header items delegate to their parent.
- Rounded inset scrollbar thumbs, dark gutters, flat arrow chevrons and hover/pressed colors. Covers both nonclient scrollbars and scrollbar controls in registered app windows. Native ranges, hit targets, wheel/keyboard/page/arrow actions and thumb tracking remain delegated to Windows; no replacement scroll message positions or song data are introduced.
- Repaint hooks after native paints/scrolls, cached geometry refresh during app idle, and a temporary repaint timer only during the native scrollbar tracking loop. Native 32-bit `SIF_TRACKPOS` behavior is retained.
- Conservative contrast lift for low-saturation legacy toolbar artwork. Colored icon tiles and icons already covered by the existing explicit inversion list keep their prior processing. A coherent replacement icon set is recorded as deferred in `FEATURES.md`.

## Decisions / limits

This pass uses the existing gray palette and does not expand control sizes. Scrollbar thumbs look slimmer while retaining the full native input area. The implementation remains in existing translation units so generated build projects do not need extra source registrations.

No full replacement icon set was created. OS-managed file dialogs and the maximized MDI child's native system buttons may still show system-controlled details. General visual acceptance, native scrollbar redraw smoothness during mouse dragging, DPI changes, tool-caption alignment, and cross-monitor placement need owner review because desktop screenshot capture is unavailable in this runtime.

## Verification

- Final Windows 10 x64 Release build passed with MSVC v145, SDK 10.0.26100.0. No compiler warnings/errors in `bin/synmpt-chrome-build.log`. Existing optional `subwcrev`/`svn_version.h` notices remain.
- Final-executable memory-bitmap checks passed for vertical/horizontal rounded thumb shapes, clipping to scrollbar bounds, hover colors and disabled-thumb handling. Existing track tint, glyph, selection and M/S/Record rendering checks passed too.
- Native audio checks passed for composite selection/transposition/layers, bus gain/pan/mute/automation, instrument overrides, MPTM save/reload and backward-compatible v1 extensions. Yuzu reached order 2 with audible output. Log: `bin/synmpt-chrome-preview/synmpt-runtime-checks.log`.
- The isolated preview launched, exposed the floating Transport, created an empty document with the five editor tabs and vertical/horizontal scrollbars, and closed cleanly. This accessibility smoke check preceded the final toolbar/list-header additions; the final build was launched again afterward.
- Desktop capture failed with `FrameArrived timed out: timed out waiting on channel`; a fresh-selection retry failed with `window capture timed out: timed out waiting on channel`. These are not visual passes. No coordinate mouse tests are claimed.
- Whitespace checks passed with CRLF recognized. No audio/storage implementation files were changed.

## Preview / owner checks

`C:/Users/Nate/Documents/GitHub/SynMPT/bin/synmpt-chrome-preview/OpenMPT.exe`

This has its own portable profile with single-instance forwarding disabled. The existing appearance preview and the owner's Suprfist document were not modified or closed.

1. Open the same song and check the transport frame, editor tabs, toolbar edges, library panes and horizontal/vertical scrollbars against the gray theme.
2. Drag both scrollbar thumbs, use arrows/page clicks/wheel, then resize and switch tabs. Check that the rounded skin stays visible and scrolling stays smooth.
3. Move/resize the floating Transport and check its caption/close target. Try your usual display scaling and monitor arrangement.

Open question: owner preference for thumb thickness/contrast and remaining icon priorities after reviewing the preview.

Footer: Single handoff file for this pass. No SynCAD canon documents, synced project sources, owner images or song files were edited. Implementation and native rendering/audio checks passed; final visual acceptance remains with the owner.
