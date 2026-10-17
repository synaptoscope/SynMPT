# SynMPT desired features

Working notes for the public OpenMPT fork. This describes the intended composing workflow, not a fixed implementation plan.

## Current scope and deferred work â€” 2026-10-17

The owner authorized the gray appearance/font/track-tint pass plus floating-transport recovery and header button feedback. The following functional work is recorded for later; it is not implemented by the appearance pass.

### Composite keyboard editor and reusable presets

- A small piano icon in the group's instrument row opens a compact mapping window. Default to 88 keys with Middle C explicitly identified.
- Each child has a named instrument/sample button. Selecting it selects that child for mapping and highlights its currently assigned keys. Click a key for a single-note assignment; drag across keys for a continuous range, with adjustable endpoints, clear and undo.
- Use a separate mapping palette, independent of song-view track colors: related song tracks may all have the same color. Save assignment colors with the composite. Labeled, separate range bands keep ownership clear and allow a later overlapping/layered view.
- An initial GUI may enforce exclusive assignments with explicit conflict/reassignment feedback. Existing composite routing already permits overlapping ranges/layers; changing that playback/input policy is not part of the appearance pass.
- Support fixed sounding pitch for drum mappings and root-relative pitched mapping for guitar/instrument ranges. The proposed mapping/audition toggle and compact control details still need owner hands-on review.
- Add reusable named composite presets containing the parent group, child tracks, sounds, mappings and relevant mix settings. Loading creates an instance in the current song. Arming records ordinary mapped notes into the child lanes, building on current input routing.
- Existing composites are currently stored inside MPTM songs; no standalone composite preset save/load exists yet. Whole-song starter files remain the available reuse method.

### Native .synmpt documents and recoverable conversion

- Make `.synmpt` the default native song/project format, with explicit internal identity and a format version. The first version can reuse the existing MPTM musical storage alongside versioned SynMPT metadata.
- Opening another song format prompts to convert a copy, keep the original format, or cancel. Existing native documents open directly. Conversion happens in memory; the source file stays untouched and the first native save chooses a new filename.
- Preserve recovery copies before future upgrades that overwrite an existing native file. Keep musical data separately identified from workspace/layout metadata so presentation changes do not alter the composition.
- Expose format conversion/export clearly. Converting out saves an explicitly converted copy and explains unsupported features. Groups, composite input maps and custom bus mixing cannot be assumed to round-trip through other formats; sound-preserving export needs its own treatment.
- OpenMPT's existing Song Properties format conversion remains available until this workflow is implemented.

### Empty documents and track/preset creation

- A fresh song should present an empty workspace with no visible tracks.
- Provide `+ Track` for an ordinary track or empty group, and `+ Preset` to create the required group/tracks from a saved instrument setup. Song templates can provide a complete prebuilt starting arrangement.
- Proper empty-document handling is unbuilt; current native channel-count assumptions still apply.

### Drag arrangement and group ownership

- Start a track drag from its name/header. Move a multi-selection together in its current order; a group carries its children.
- Hovering a bus body highlights its right edge and previews the moved tracks at their tentative lower child position. Releasing assigns them to that bus.
- Hovering the top insertion zone highlights the top edge and clearly identifies before/after placement without grouping. Use this to remove tracks from a group too.
- Preview order and ownership during the drag; commit a single undoable change on release. The existing menu-based group/output assignment remains the current implementation.

### Automatic song-view colors

- Follow the SWS extension's name-rule approach: keyword, enabled state, hex color/swatch, and top-to-bottom priority; the first matching rule wins.
- Proposed defaults: case-insensitive matching when a track is created/renamed, draggable rule priority, and an Auto/Manual choice per track so manual colors survive renaming.
- The appearance pass displays existing track colors through the column body. It does not add the rule engine. Keyboard mapping colors remain independent.

### Other parked work

- Replace legacy toolbar/menu icons with a coherent set designed for the gray theme. The current appearance pass only adjusts contrast of existing monochrome toolbar artwork.

- Dragging document tabs to reorder them and detaching songs for a second monitor remain explicitly low priority.
- Musical step-record note lengths, rest/advance/tie controls, a global master track/lane, and separate sample/instrument editor windows remain on the earlier desired list.

## Main workspace

- Keep the song/order view.
- Bring the General controls and pattern editor together in the main song workspace. Put per-channel controls directly above their pattern columns.
- Keep the default workspace compact: the pattern grid gets most of the space, and additional controls expand only when needed.
- Make the workspace useful across wide and multiple monitors, with resizable, movable windows.
- Provide a compact floating transport, freeing space above the pattern grid. Remember its position and provide a reliable recall shortcut; accidental docking should be prevented.
- Put sample and instrument editing in their own windows rather than hiding them behind tabs.

## Tracks and master

- Give each pattern channel a clear track identity: name, color, instrument, volume fader, pan, mute, and solo.
- Keep the track controls in a compact header aligned with the pattern column while composing. Keep song-wide controls with the master area.
- Add a master track with its own controls and automation lane.

## Step composition

- Build on the existing pattern Record toggle and Edit Step behavior, which already support entering notes while playback is stopped and advancing the cursor by a chosen number of rows.
- Offer a compact Step Record mode with musical note values (whole, half, quarter, eighth, sixteenth, etc.) for entry spacing and note length. Support dotted/triplet values where the song's timing resolution can represent them.
- Treat cursor advance and note duration as distinct: the current Edit Step controls cursor spacing, and does not by itself define the stored note's length.
- Enter a pitch using the computer keyboard or a MIDI controller, audition it, write its start and appropriate release, and advance by the selected note value.
- Advance: move by the selected value without writing an event.
- Rest: release the held note and advance by the selected value.
- Extend/Tie: extend the previous note by the selected value without retriggering it.
- Keep the controls in one compact strip and provide keyboard shortcuts; reuse existing tracker note/release commands where possible.
- This is a desired feature, not part of the completed first migration pass.
## Automation

- Show Volume/Pan automation lanes only when that track contains the corresponding automation somewhere in the song. Keep each lane independently collapsible, immediately beside its owning track, with a shorter header, clear borders, and a distinct tint.
- Edit automation as drawn lines and nodes while retaining precise tracker values and steps underneath.
- Map lanes to existing effects and controls first. Decide on new effects or EQ only when a musical need calls for them.

## Automatic track colors

- Allow user-defined name-to-color rules that color tracks when their names match, inspired by the separate SWS extension for REAPER.

## Music and game playback

- Keep tracker composition and continuous module playback at the center of the workflow.
- Make the sound heard while composing a reliable reference for how music will sound in the game.
- Support modest, musically timed reactions to game action without making prerecorded audio loops the primary method.

## Initial verification

- Build and launch the untouched fork as a baseline.
- Play bundled module files and compare playback after changes.
- Optionally import a familiar Guitar Pro MIDI export as an additional reference song; this does not define the composition workflow.

## First migration pass — 2026-10-11

Implemented:
- Songs open in the pattern workspace. The former General tab is labeled Song and keeps the existing song settings available.
- Each pattern channel has a compact name/color header, a dedicated mute button, and horizontal volume and pan faders above its pattern column. The controls add 36 pixels at normal display scaling and do not widen the pattern columns.
- A name click selects the track. Double-click the name to edit its name/color in the existing channel properties window. Existing header gestures for moving tracks, record groups, and solo remain available.
- Drag a fader to change its value. Double-click volume to restore 64, or pan to restore center (128). One drag creates one undo step. Undo/redo also restores the changed volume/pan in live playback.
- Controls that the current module format cannot store are disabled: channel volume is available for IT/MPTM, and channel pan for S3M/IT/MPTM.
- Play/record buttons, live tempo, live global volume, and a small master meter are in a separate Transport toolbar. It starts floating on its first run, can dock, and shares the existing toolbar position storage. Show it through View > Toolbars > Transport.

Next passes:
- Per-track instrument selection; the existing pattern instrument selector is still in use.
- Separate sample/instrument editor windows.
- Master track/lane and name-based color rules.
- Further consolidation of song settings after reviewing this first layout.

Verification:
- Windows 10 x64 Release build passed with MSVC v145, SDK 10.0.26100.0, and the existing Windows 10 project.
- Whitespace check passed with CRLF resource-file endings recognized.
- Automated visual inspection was blocked by a Windows app-access approval timeout. Layout, toolbar docking/restoration, and listening tests remain to be checked in the preview.
- Preview: `bin/synmpt-preview/OpenMPT.exe`. The existing baseline executable is still available separately.
- Build log: `bin/synmpt-first-pass-build.log` (ignored generated output).

Review in the preview:
1. Open `test/test.mptm`, or the familiar imported MIDI reference.
2. Play the song; try volume, pan, mute, and undo/redo. Use the MPTM file to check the stored channel controls.
3. Double-click a track name to edit name/color, and drag a name to move a track.
4. Move/dock the Transport, resize the main window, and check the layout after restarting.
5. Save any edited verification song as a separate file before reloading to check stored values.

Handoff:
- Branch: `master`; latest existing commit: `dc49d5638`. Changes are uncommitted.
- Changed source: `mptrack/Draw_pat.cpp`, `mptrack/Globals.cpp`, `mptrack/MainFrm.cpp`, `mptrack/Mainbar.cpp`, `mptrack/Mainbar.h`, `mptrack/Mainfrm.h`, `mptrack/Undo.cpp`, `mptrack/View_pat.cpp`, `mptrack/View_pat.h`, `mptrack/mptrack.rc`, `mptrack/resource.h`.
- Notes: `FEATURES.md`.
- Suggested commit message: `Add compact track headers and dockable transport`.

## Startup fix — 2026-10-11

- Confirmed the initial preview crash from its minidump: `CStereoVU::DrawVuMeter` dereferenced the main frame before the application had registered it. Creating the floating transport during frame construction caused the early paint.
- Moved saved-toolbar restoration into the initialized main frame, and first-run transport floating into the first main-window show after restoring its placement.
- Made meter drawing and clip-reset input safe when the main frame is unavailable.
- Rebuilt the Windows 10 x64 Release preview successfully. Whitespace check passed.
- Launched the rebuilt preview and confirmed that it remained running with the Transport and pattern workspace present. Windows accessibility also showed the user's imported `Cat-Countrydevonstuff3.mid` loaded in the pattern editor.
- Screenshot capture returned a window-capture timeout; visual layout and listening checks still belong to the user's preview review.
- Updated files in this fix: `mptrack/MainFrm.cpp`, `mptrack/Mainbar.cpp`, and these notes. The preview path remains `bin/synmpt-preview/OpenMPT.exe`; build log is `bin/synmpt-startup-fix-build.log`.


## Volume/Pan automation pass — 2026-10-12

Implemented:
- Dedicated Mute and Solo buttons in the existing compact track-name row. Solo uses the existing channel solo behavior; pressing Solo again on the isolated channel unmutes all channels.
- Adjacent, independently collapsible Pan and Volume bands. No band occupies space without corresponding automation data. Existing automation starts collapsed unless the saved editor layout had it expanded.
- A small `+` beside a track's V/P fader adds its first automation point at the cursor row and opens the lane. The arrow toggles an existing lane. View > Track Automation > Pan / Volume provides the same controls for the selected track.
- Lane headers are shorter than the track headers. Tint, borders, and the track color accent identify ownership. The graph stays aligned with pattern rows and ends at the pattern boundary.
- Left-drag draws values, including intermediate rows between mouse positions. Right-drag erases the selected automation. Each gesture is one undo step. Status text reports the row/value and any skipped occupied rows.
- Volume is channel gain (`CMD_CHANNELVOLUME`), separate from note velocity. Pan uses the native pan command or volume-column pan. There is no extra playback engine or file format: ordinary pattern commands are saved, exported, undone, and played by OpenMPT.
- Where possible Pan and Volume share a row through the two native command columns. Unrelated effects, note velocities, notes, instruments, and plugin parameter events are protected. Occupied rows are marked at the lane edge and skipped if both native columns are needed elsewhere.
- The graph displays native held values between commands. Drawing writes row steps; this pass does not add persistent spline curves or a separate node format. The initial graph value comes from the track's default setting; order-dependent changes entering from earlier patterns and other effect families are not simulated by the graph.
- Lane availability follows the module format. Use IT/MPTM for the complete Pan/Volume combination. Unsupported controls report the limitation without changing the pattern.
- Expansion state follows track moves/duplicates and is stored with the existing editor layout. Horizontal scrolling and note selection account for expanded lane widths.
- View > Transport toggles the transport. View > Bring Transport Here recovers a hidden/offscreen floating transport near the main window. Existing saved toolbar settings migrate to the corrected transport control-bar ID.

Verification:
- Windows 10 x64 Release build passed with MSVC v145 and SDK 10.0.26100.0; `bin/synmpt-automation-build.log` records the build.
- Native automation checks passed for shared Pan/Volume cells, note/velocity protection, occupied effects, plugin events, erasing, S3M scaling, and XM pan precision/unsupported channel gain. Source: `test/synmpt/automation.cpp`; output: `bin/synmpt-automation-check.log`.
- Whitespace check passed with CRLF resource endings recognized.
- Opened the rebuilt portable preview with `test/test.mptm`; its process stayed running and responsive after startup.
- Automated UI inspection was unavailable because the desktop control service could not be reached. Visual layout, mouse drawing, docking/recovery, save/reload from the UI, and listening remain for preview review.

Review:
1. Open the preview at `bin/synmpt-preview/OpenMPT.exe`. Use View > Bring Transport Here if needed.
2. In an IT/MPTM song, select a free row and use the V/P `+` or View > Track Automation to add the first point.
3. Draw in the adjacent lane, collapse/reopen it, and use right-drag to erase. Check undo/redo and the native pattern commands alongside the graph.
4. Play a sampled note through the changed rows to hear gain/pan changes. Occupied effect rows remain unchanged.
5. Save an edited verification song under a new filename, reopen it, and compare its automation.

Handoff:
- Branch: `master`; latest existing commit: `dc49d5638`. Changes remain uncommitted.
- Source files for this pass: `mptrack/Childfrm.cpp`, `mptrack/Childfrm.h`, `mptrack/Draw_pat.cpp`, `mptrack/MainFrm.cpp`, `mptrack/Mainbar.cpp`, `mptrack/Mainfrm.h`, `mptrack/View_pat.cpp`, `mptrack/View_pat.h`, `mptrack/mptrack.rc`, `mptrack/resource.h`, and new `mptrack/PatternAutomation.h`.
- Regression checks: new `test/synmpt/automation.cpp`. Notes: `FEATURES.md`. Earlier migration changes also remain in the working tree.
- Suggested commit message for the accumulated work: `Add compact track workspace, recoverable transport and native automation lanes`.
