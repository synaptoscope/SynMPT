# SynMPT ownership outlines for track columns

Date: 2026-10-17 (America/New_York)
Branch: `feature/track-workspace`
Implementation commit: `455eef263dd72d42c44d361c2c5cd16c6536f77f`
Base: `d2cc02c05`

## Owner direction

The owner supplied `C:/Users/Nate/Documents/syncad saves/track shapes.png`: a raised group/master column, lower constituent tracks, automation another step lower, and a clipped upper-right ear marking the end of ownership. This pass applies those shapes to the existing folder/bus tracks and automation columns. The supplied image is reference material and was not edited.

## Built and decisions

- Group parents sit above their children. Sibling track tops stay square while the family continues; its last visible member has a clipped upper-right cap. Each folder parent and standalone track has its own cap. The next visible member is used when folders are collapsed. Existing folder order remains the source of membership/layout.
- Visible automation starts one height step below its owning track. Its last open lane has a clipped cap. Pan and volume retain the existing rule: only lanes with automation can appear, and each remains collapsible. Header labels now sit at the top of the lower automation step.
- The step is 10 DIP and the clipped corner is 8 DIP. Nested groups step further down, with vertical indentation capped at three levels to preserve compactness. The existing parent-color guide remains. This cap affects drawing only; nested routing remains intact.
- Name, action buttons, instrument, volume, pan, VU meters and plugin labels move together. Plugin hit testing and keyboard context-menu positions follow the moved labels. The note grid and automation time axis keep one common row boundary. Header height/scroll layout refreshes when nesting or collapse changes.
- Controls are clipped to their silhouette, with empty space above the stepped tops. Hit testing excludes that space and clipped corner. Middle-click quick properties and right-click menus respect the header geometry. Column edges continue to the common visible pattern bottom. Name labels reserve room for the corner and use ellipsis when needed.
- Here, the reference's master is the existing group/bus parent. This pass adds no separate global master channel and changes no song data, audio routing or file format.

## Verification

- Windows 10 x64 Release build passed using MSVC v145 and SDK 10.0.26100.0. No compiler warnings/errors in `bin/synmpt-shapes-build.log`. Existing optional subwcrev/svn_version notices remain.
- Focused checks passed: folder/child/nested depths, continuing sibling runs, last visible child caps, independent caps, safe cyclic-parent fallback, corner/gutter hit testing and scaled geometry at 96/120/144/168/192 DPI. Existing automation, note/effect protection, multi-select, solo, nested bus gain/pan/mute, composite input and collapse checks passed. Source: `test/synmpt/automation.cpp`; log: `bin/synmpt-automation-check.log`. Its existing standalone compiler preprocessor/MBCS notices remain.
- Native runtime checks passed for composite selection/layers/transposition, rendered bus audio and automation, instrument override, MPTM save/reload and v1 compatibility. Yuzu - Yu-Lib advanced to order 2 with audible output. Log: `bin/synmpt-shapes-preview/synmpt-runtime-checks.log`.
- The new executable launched and its editor/toolbars were exposed through accessibility. Screenshot capture failed with `FrameArrived timed out: timed out waiting on channel`. File-name selection also failed (`element 214 is not available in cached app state`); the file chooser was canceled. No GUI appearance, mouse/collapse interaction or visual overlap pass is claimed. The separate preview remains at the initial workspace for owner inspection.
- Staged whitespace check passed with CRLF recognized.

## Preview and owner checks

Executable: `C:/Users/Nate/Documents/GitHub/SynMPT/bin/synmpt-shapes-preview/OpenMPT.exe`

It has an independent portable profile with single-instance forwarding disabled. The previous `synmpt-composite-preview` process and its open `test.mptm` song were left running. The runtime checks also create a disposable `synmpt-composite-demo.mptm` beside the new executable; it contains a folder and two sample children but no automation by default.

1. Open a grouped song in the new preview. Check the raised group, lower children and clipped corner on the last member. Resize a track and try the group collapsed and expanded.
2. Open existing pan/volume automation beside a child. Check the lower lane top, last-lane corner and each collapse toggle. Confirm empty lanes still stay hidden.
3. Play the song with VU meters/plugin labels enabled. Check the name, buttons, instrument, volume, pan and meters are clear and the rows remain aligned.

Open questions: final shape proportions, border contrast and hands-on appearance remain for owner acceptance. No new scope question blocks this candidate. Document-tab reordering/detachment remains the owner's deferred work.

Footer: Single handoff file for this pass. No synced project sources, owner songs/images or SynCAD canon docs were edited. Geometry and playback checks passed; visual acceptance remains with the owner.
