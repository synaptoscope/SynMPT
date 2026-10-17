# SynMPT editor switching — 2026-10-17

Branch: `feature/hover-chrome`

Implementation commit: `2a2a6492fb5aac2137d7d72ef0b30223903a1198` — Stabilize editor tab switching and bound theme redraw work.

## Built / decisions

- Scrollbar skins now change position, z-order and visibility only when required. Unchanged paints no longer rewrite the owner's window style or synchronously redraw every overlay. Dirty/hover changes invalidate the skin for normal painting, retaining native input, accessibility, range and tracking behavior.
- Theme registration stops adding redundant descendant roots to the idle scan. Child editor registration applies its controls immediately without running top-level caption/menu/DWM work.
- New lower views and dynamically created upper controls receive their theme before display. Splitter painting is deferred while the replacement is created and laid out, then backgrounds/children are invalidated together. Default control/form/scroll-view backgrounds are gray; custom pattern, waveform and envelope painting still overrides the base fallback.
- Editor tab controls clip sibling pages. Upper page containers and form views clip their child controls, preventing background erases from covering controls. The Song form no longer requests the native light tab texture.
- Superseded queued view replacements are ignored. Cached upper pages bind only to their associated lower editor class. This prevents typed view-state messages from being delivered to another kind of editor during overlapping tab requests. Asynchronous view replacement is preserved so a lower view cannot destroy itself while requesting a different tab.

## Reproduction and verification

- The new real-editor stress fixture initially completed normal switches, then stalled/crashed during rapidly queued switches; one run exited with `0xC0000374` (heap corruption). Inspection found Sample-view state being sent to the still-live Pattern view, whose state contains vectors. The matching-view binding correction made the same fixture pass. Temporary dispatch tracing was removed.
- Final Windows 10 x64 Release build passed using v145 and SDK 10.0.26100.0. No compiler warnings/errors; the existing optional `subwcrev`/`svn_version.h` notices remain. Log: `bin/synmpt-tabs-fix-build.log`.
- `/synmptEditorChecks` exercises the actual MFC pages/views in a separate process with Yuzu. Passed 40 settled switches through all five editors and 24 rapidly queued selections. Assertions cover matching upper/lower views and message targets, usable layout, restored splitter visibility and an unchanged document dirty flag. This test bypasses single-instance forwarding and does not open an IPC endpoint.
- Hidden-process first-open measurements: Song 202 ms, Patterns 23 ms, Samples 226 ms, Instruments 229 ms, Comments 33 ms. Longest settled switch was 229 ms. These are functional-test timings, not a physical desktop smoothness measurement or a before/after benchmark.
- `/synmptChecks` passed: 64 unchanged viewport redraw/layout requests produce zero scrollbar repositionings and zero style rewrites; persistent skins, hit-test passthrough and 32-bit ranges remain correct. Existing caption/selector/MDI, colors/fonts/buttons, composite/audio/bus routing, MPTM round trips and v1 compatibility checks passed. Yuzu advanced to order 2 with audio.
- Final logs: `bin/synmpt-tabs-fix-preview/synmpt-editor-checks.log` and `bin/synmpt-tabs-fix-preview/synmpt-runtime-checks.log`.
- CRLF-aware whitespace check passed.

## Preview / owner test

`C:/Users/Nate/Documents/GitHub/SynMPT/bin/synmpt-tabs-fix-preview/OpenMPT.exe`

Separate portable profile; the earlier preview binary/profile and the owner's running song were not targeted.

1. Open Yuzu or your current test song. Cycle Song → Patterns → Samples → Instruments → Comments and back, then make several quick tab changes. Check that there is no white flash and every page is complete.
2. Return to Patterns and play the song. Check that the cursor, track settings and normal scrolling still behave as expected.

Open question: physical desktop flash/smoothness and visual acceptance still need owner confirmation; automated checks use an isolated hidden process and do not capture the desktop.

Footer: One handoff file for this pass. No canon documents, synced project references, user images or songs were edited. No file-format or audio-engine behavior changed.
