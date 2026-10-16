# SynMPT collapsible folders and composite instruments

Date: 2026-10-16 (America/New_York)
Branch: `feature/track-workspace`
Implementation commit: `de934a78f156c16ad48e0079c5128729b962dbab`
Base: `88bc0b089`

## Owner direction and earlier list

The owner accepted document-tab reordering and pulling a song into a separate monitor window as future ideas, explicitly low priority. They asked to return to the earlier track/automation list and connect folders with the composite-instrument idea from the chat titled TRACKER MUSIC EXTRACTION. In this chat they explicitly selected: Include composite-instrument routing.

The other chat's owner messages describe keeping guitar samples/articulations on separate lanes, playing them as one instrument, and using different note ranges for qualities such as palm mutes and power chords. No guitar samples or song files in that chat were modified by this pass, and no messages were sent to it.

The earlier width controls, full-width names/action/instrument rows, track instrument overrides, sample-bus mixing and line automation are present in the source. The missing folder/composite workflow was added here. The Yuzu opening-loop report was previously traced by the owner to Loop Pattern being selected; the maintained reference playback check still passes.

## Built

- Group buses now have a header arrow that expands/collapses their descendants. Collapsing is a display change; their notes and audio remain active. Nested folders work. Hidden tracks are excluded from header, grid, automation and VU drawing/hit testing, and cursor navigation skips them. Collapsed state follows moves, survives page switches, and is appended to the existing editor-layout cache.
- Group/output changes and track moves arrange actual channels into stable folder order, preserving each track's events. Native channel rearrangement handles routing references. Group creation and routing keep their pattern/channel undo snapshot, with the subsequent arrangement sharing that action. The folder header shows its member count, and children have a thin parent-color guide.
- Clicking a folder's instrument row opens a compact Composite Instrument dialog. Each descendant leaf can have an assigned sample/instrument, an enabled input route, an inclusive key range, and a transpose amount. Overlapping ranges create layers. Spread key ranges assigns consecutive banks and transposes each bank to start at C-5; existing sample tuning remains the source of pitch calibration.
- The folder's R button arms one composite input per pattern view. Notes route while that folder or one of its members is selected, including when children are collapsed. Child R indicators show the active routes; clicking a child R returns to native individual-track arming. Arm state survives a page switch but is not restored as an armed input at app startup.
- Keyboard/MIDI entry writes ordinary native notes/instrument numbers on the matching child lanes. The step advances once per input note. Occupied notes/instrument cells and plugin parameter events are kept, with a status message for skipped lanes. Existing pan/effect commands are retained; velocity uses an available volume/effect slot.
- Preview voices retain each mapped note/track/instrument for release, follow child channel volume/pan/mute and existing parent-bus gain, and are released on key-up, disarming, focus loss or routing changes. Note-off recording uses the existing preference and free native row cells. A composite entry or release creates one row undo point for its affected lanes.
- View > Track Controls now also exposes Composite Input, Collapse / Expand Folder, and Record Arm Track / Folder. The header controls remain available.

## Persistence and boundaries

Composite rules use version 2 of the existing MPTM SNTK property: 8 bytes per channel plus the version byte. Version 1 remains readable with composite input disabled and default note ranges. Song extension data is written only to MPTM. Folder collapse is editor layout; arming is transient view state.

Recorded notes play through the existing tracker engine, with no new composite event sequencer. The earlier custom sample-bus/instrument overrides still require this fork's playback engine; upstream OpenMPT/libopenmpt ignores that extension. Standard multi-sample instruments can be assigned to child lanes too.

This pass supports sampled instruments, retaining the earlier refusal of external plugin/effect, MIDI-output and FM tracks in these sample buses. It adds no bus FX chain, velocity zones, keyswitch system, or automatic voice-track allocation. A native child channel still holds one note per row; group entry skips occupied cells rather than replacing them or allocating another voice track. Leaf input rules are shared when a leaf belongs to nested composite folders.

## Verification

- Windows 10 x64 Release build passed with MSVC v145 and SDK 10.0.26100.0. No compiler warnings/errors in the final build. Log: `bin/synmpt-composite-build.log`. The existing optional subwcrev notice remains.
- Focused maintained checks passed for articulation range selection, equal output pitches across transposed banks, overlapping layers, out-of-range/disabled routes, preservation of occupied notes/plugin events/pan, stable nested-folder ordering and collapse visibility. Existing automation, grouping, multi-select and DPI geometry checks also passed. Source: `test/synmpt/automation.cpp`; log: `bin/synmpt-automation-check.log`.
- Engine checks rendered mapped notes and verified the assigned sample and the sum of layered samples. MPTM save/reload preserved input rules. A fixture containing a v1 track extension retained its buses without enabling composite input. Existing gain, pan, mute, bus automation and instrument-override checks passed. Yuzu - Yu-Lib advanced to order 2 with audible output. Source: `test/synmpt/runtime.h`; log: `bin/synmpt-composite-preview/synmpt-runtime-checks.log`.
- The runtime check produces `bin/synmpt-composite-preview/synmpt-composite-demo.mptm`: a silent starter with a Composite Test folder, two synthesized sample lanes, C-4/C-5 input banks mapped to the same output pitch, and an independent track. It is a disposable test fixture, not a guitar pack.
- The preview launched and exposed the normal editor controls through accessibility. Screenshot capture failed with FrameArrived timed out. File-dialog keyboard/field automation was unreliable; the chooser was canceled and no user songs were edited. Direct editor input, mouse controls, fold rendering, dialog feel and undo/redo remain hands-on checks rather than claimed GUI passes.
- Staged whitespace check passed with CRLF recognized.

## Preview / hands-on checks

Executable: `bin/synmpt-composite-preview/OpenMPT.exe`. It has an independent portable profile with single-instance forwarding disabled. Earlier previews and the owner's songs were retained.

1. Open the disposable demo. Select its first track, use View > Track Controls > Composite Input, and inspect the two key ranges and transpose values. Arm the folder's R button and enable the normal Record toggle for step entry. C-4 and C-5 should write to different child lanes at the same output pitch. The two test samples have different levels.
2. Make the ranges overlap and check both lanes receive the same input. Undo/redo an entry. Existing occupied notes should be kept. Release a key, disarm, and switch pages to check for hanging previews.
3. Collapse the folder while playing, navigate past it, and expand it. Check audio continues, child lanes disappear/reappear, and volume/pan automation remains attached correctly. Test folder mute/solo and save/reload an MPTM copy.
4. In an MPTM copy of a sample-based song, select its instrument lanes and use Group / Output > Create group bus from selected tracks. Inspect grouping order and undo, then configure its own articulation ranges.

Deferred by the owner: dragging document tabs to reorder them and pulling a song into its own window for a second monitor. This pass does not start that work.

Footer: This is the single handoff file for this pass. No synced project sources or SynCAD canon documents were edited. Native editor/mouse acceptance remains with the owner.
