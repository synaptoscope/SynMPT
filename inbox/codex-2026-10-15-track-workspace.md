# SynMPT track workspace and line automation

Date: 2026-10-15 (America/New_York)
Branch: `feature/track-workspace`
Implementation commit: `71814bcbf3fd51014affa793f87bf122cb734769`
Base: `dc49d5638`

## Built

- Preserved and committed the earlier compact-header/transport/automation work that was still uncommitted on master, together with this pass.
- Newly opened and new songs start with pattern looping off. The owner identified the selected Loop Pattern checkbox as the cause of Yuzu repeating its opening pattern; no audio-engine progression bug was established.
- Ctrl-wheel widens/narrows all tracks. Shift-wheel changes the selected track. Dragging a track's header edge resizes that track. Width is bounded by the native note-column width and 640 DIP. Extra width provides control space without stretching note text. Widths follow track moves and persist in the existing editor-layout cache.
- Names use the full track width. Mute, Solo, Record Arm and group/output controls occupy the next row. A separate instrument row precedes volume/pan. Track controls also have a View > Track Controls menu.
- Instrument headers show the instrument in use at the cursor/order, with a first-used fallback for an empty opening section. Choosing an instrument creates a persistent MPTM track override; Pattern mode restores the original per-row instrument behavior. Original pattern instrument numbers remain intact.
- Sample-based tracks can be assigned to named group buses. A group is an automation-bearing tracker channel with its own volume, pan and mute. Groups can nest; cycles are refused/bounded. Solo includes the necessary group outputs; soloing a group includes its children. Group routing follows channel moves/deletions and participates in existing channel/pattern undo.
- Group audio uses linear gain and stereo balance on each child voice before volume ramping; this is distributive over the sampled stereo sum. Released/NNA sample voices retain their source group. External plugin/effect, MIDI-output and FM tracks are refused by the group assignment controls because their audio does not pass through the sample mixer.
- Automation defaults to click start / click end lines, with a preview and Escape cancellation. Shift-drag retains freehand drawing; Shift-right-drag erases. Right-click a control point to edit its numeric value, reset it or delete it. Double-click resets Pan to 32 (center) or Volume to 64 (unity).
- Node values are displayed and available in hover/status text. Editable corners are reconstructed from native row values with quantization tolerance, so straight ramps remain editable after reload without adding a second automation playback format. Changing a corner redraws its neighboring segments. Existing unrelated effects, note velocity and plugin parameter notes remain protected. Live row edits use the existing audio critical section.
- Lanes remain beside their owning track, hidden without corresponding automation, and independently collapsible.

## Persistence and compatibility

Group membership, group identity/mute and instrument overrides use version 1 of the MPTM extended property `SNTK`. Automation itself remains standard native pattern commands. Group and instrument-assignment controls require MPTM.

Upstream OpenMPT/libopenmpt ignores the new track extension. The game must use this fork's playback library for group routing and instrument overrides to sound the same. Exporting to older formats does not preserve these track extensions. Widths/expansion belong to the existing editor cache, rather than audio playback data.

The group pass covers sampled audio. Later raw pattern/plugin edits outside the assignment controls can still introduce an external output into a previously grouped track; full external-effect/synth bus routing is a separate, unfinished capability. No group FX inserts were added.

Graph initialization follows prior absolute Pan/Volume commands in ordinary order-list progression. It is not a simulation of arbitrary pattern jumps, slide effects or plugin automation. Integer row resolution remains the playback precision, and redundant collinear points may be reduced when recovering visible corners.

## Verification

- Windows 10 x64 Release build passed with MSVC v145 and Windows SDK 10.0.26100.0. Log: `bin/synmpt-track-build.log`.
- Maintained native regression checks passed: shared Pan/Volume commands, effect collisions, notes/velocity protection, PC events, S3M surround protection, format mappings, descending/ascending lines, endpoint recovery, held values, nested group membership, group-aware Solo, gain/pan/mute and cycle safety. Source: `test/synmpt/automation.cpp`; log: `bin/synmpt-automation-check.log`.
- Isolated runtime checks rendered actual sampled audio and verified half gain, nested quarter gain, stereo balance, mute, native bus automation, and track instrument changes. MPTM save/reload preserved routing, instrument override, group identity and group mute. Source: `test/synmpt/runtime.h`; log: `bin/synmpt-track-preview/synmpt-runtime-checks.log`.
- The installed original `C:/Program Files/OpenMPT/ExampleSongs/Yuzu - Yu-Lib.mptm` advanced through order 2 and generated audible output in the runtime test.
- Whitespace check passed with the resource file's CRLF endings recognized.
- Added `/synmptChecks` as a diagnostic command that runs these offline engine checks, writes its log beside the executable, and exits before opening the application or forwarding to another instance. Optional song arguments are checked for progression and audio output.
- The new preview is separate: `bin/synmpt-track-preview/OpenMPT.exe`, with an independent portable settings file. The old `bin/synmpt-preview` and the owner's open, unsaved Yuzu document were retained.
- Desktop screenshot capture timed out. A subsequent attempt to open the new preview was stopped by the owner's physical Escape key. No further Computer Use actions were taken. Launch outcome and final visual/mouse behavior are not claimed as verified.

## Review / open questions

1. Review header height and minimum/wider widths in the new preview. The added action/instrument rows use 36 additional DIP compared with the previous header.
2. Exercise mouse line endpoints, numeric editing, reset, freehand mode and undo/redo. Confirm label readability at the chosen font/DPI.
3. Use a copy of a sampled MPTM song to create a group through its output selector, then test group automation, Solo and save/reload.
4. Decide whether external synth/effect buses are wanted in a subsequent pass. The current implementation explicitly supports sampler groups.

Key source areas: `mptrack/View_pat.*`, `mptrack/Draw_pat.cpp`, `mptrack/PatternAutomation.h`, `mptrack/Childfrm.*`, `mptrack/Moddoc.cpp`, `mptrack/Modedit.cpp`, `mptrack/Mptrack.*`, `soundlib/ModChannel.h`, `soundlib/TrackRouting.h`, `soundlib/Sndmix.cpp`, `soundlib/Load_it.cpp`, and the existing transport/resources.

Footer: No synced project sources or SynCAD canon documents were edited. This is the single handoff for the pass. Visual approval remains with the owner.
