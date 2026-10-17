# SynMPT title/selector hover and document controls — 2026-10-17

Branch: `feature/hover-chrome`

Implementation commit: `e0d90f0d0b33354aa3ee05e44221d18d74d0e76f` — Fix caption and selector hover flicker and darken document controls.

## Built / decisions

- Removed the native/custom hover paint race for caption text. Nonclient mouse movement over a custom caption is handled locally; only changed button highlights are repainted. Dragging, double-clicking, resizing, system menus and button commands retain native handlers. Activation uses the documented `WM_NCACTIVATE` no-repaint parameter before applying the custom appearance.
- Buffered full caption and plain instrument-selector paints. The frame border painter excludes the caption, preventing a background erase before text is presented. Caption buffering includes the native button bounds. Instrument hover updates only its arrow region and leaves the selected instrument text untouched; selection/dropdown input remains native.
- Main and floating title bars now use RGB(35,38,43), the same gray as the custom scrollbar gutter, in active and inactive states.
- Corrected the upper-right maximized-document buttons. Native Windows already marks these menu items as owner drawn. They must be recognized by their documented system bitmaps, and the frame's native visual style must be disabled before attaching the app painter; otherwise Windows intercepts their drawing. The change preserves the minimize/restore/close command IDs and native button interaction.

The native MDI fixture reproduced the white buttons and failed its pixel assertion before the frame-theme correction. It then passed all three button renders with the expected dark background. Temporary paint diagnostics were removed.

## Verification

- Final Windows 10 x64 Release build passed using MSVC v145 and SDK 10.0.26100.0, with no compiler warnings/errors. Build log: `bin/synmpt-hover-build.log`. Existing optional `subwcrev`/`svn_version.h` notices remain.
- Added `test/synmpt/hover_chrome.h` to the existing native runtime check route. It creates an owned hidden MDI frame with a genuinely maximized native child and instrument dropdown; no desktop screenshot or user song is read by this fixture.
- Checks cover the three pre-existing native owner-drawn buttons, preserved command IDs, handling by the application renderer, and their dark background pixels. A lower-level message probe verifies that 32 repeated caption and selector hover messages do not reach competing native hover/text painters. Instrument selection remains unchanged.
- Existing disabled-label/status text, scrollbar lifecycle/32-bit ranges, rounded-thumb/button/font/tint rendering, composite/audio/bus routing, instrument override, MPTM save/reload and v1 compatibility checks remain in `/synmptChecks`.
- Final runtime log: `bin/synmpt-hover-preview/synmpt-runtime-checks.log`.
- Final runtime checks passed, including Yuzu reaching order 2 with audible output.
- Final preview startup passed an accessibility smoke check: the main workspace and separate floating Transport are present. No physical mouse-hover visual pass is claimed.
- Whitespace checks passed with CRLF recognized.

## Preview / owner review

`C:/Users/Nate/Documents/GitHub/SynMPT/bin/synmpt-hover-preview/OpenMPT.exe`

Separate portable profile with single-instance forwarding disabled. Earlier preview binaries and the owner's running Suprfist document were not targeted by edits or UI input.

1. Move the cursor repeatedly over the main and floating Transport captions, then over the instrument selector. Check text stability, focus appearance and the darker gray.
2. With a song maximized, check the three document buttons in the menu bar. Try restore/maximize/minimize and confirm their dark faces remain consistent.

Open question: actual desktop hover smoothness and preferred caption shade still need owner visual acceptance; the desktop capture helper has repeatedly timed out in this environment.

Footer: Single handoff file for this pass. No SynCAD canon documents, synced sources, user images or songs were edited. The code checkpoint was initially created on the checkout's current `master`; the same commit was then placed on `feature/hover-chrome` for the required feature-branch push. No existing branch history was reset or rewritten.
