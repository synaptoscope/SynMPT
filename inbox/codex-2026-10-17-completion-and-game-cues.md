# SynMPT completion checklist and game music direction

Date: 2026-10-17

Author: Codex

Branch: `feature/hover-chrome`

Code reviewed: `4e14a019d1158d055c48599e0e8a2425e5beda8a`

## Current direction

Finish the outstanding composition features in the existing SynMPT fork so it can serve as a usable composing tool. Preserve the game-focused tracker idea as a separate design direction: a level track containing reusable patterns, named sections, and musical cues that the editor and game trigger through the same playback core.

Replacing MFC with Qt and writing a fresh tracker were discussed as alternatives. Neither architecture has been selected for implementation. The future cue system is not implemented in the current fork.

## Where functional development stopped

The last functional passes added track controls and line automation, track selection, document tabs, collapsible folders, and composite instrument input. The ownership-outline pass then applied the raised group, lower child tracks, and clipped end-of-family corners from the owner's drawing.

The next requested work was the piano mapping GUI, reusable composite presets, native .synmpt documents with recoverable conversion, empty documents and add buttons, direct drag grouping, and ordered keyword auto-coloring. The owner subsequently asked to do appearance work and log those functional items for later. That is the list to resume.

The Yuzu opening-pattern issue was identified by the owner as Loop Pattern being selected. The earlier functional pass changed new/opened songs to start with pattern looping off. It was not established as an audio-engine progression defect.

### Present in the source

“Implemented” here means present in the inspected code and recorded implementation history. The latest reported rendering defects remain unresolved; this is not a new hands-on acceptance of the application.

| Feature | Current state and boundary |
| --- | --- |
| Track widths | Ctrl-wheel resizes all tracks; Shift-wheel resizes the selected track; header-edge dragging resizes one track. Width is stored in the existing editor layout cache. |
| Compact track headers | Full-width name followed by Mute, Solo, Record Arm and Output controls; separate instrument row; volume and pan below. |
| Track instrument assignment | Header selection can override pattern instrument choices in MPTM. Pattern mode restores per-row instrument use. |
| Multi-select and batch controls | Ctrl/Shift track selection, multi-mute and additive multi-solo, with necessary group outputs included. |
| Group buses and folders | Named, nested, collapsible groups with volume, pan, mute and automation. Current audio grouping covers sampled instruments; it does not provide a general plugin/FX bus chain. |
| Composite instrument input | A basic dialog assigns a child instrument, one continuous key range and transpose. Overlapping ranges layer. Arming the folder routes input into ordinary child-lane notes. |
| Automation editing | Click start/click end lines, Shift freehand, numeric node editing, value indicators and double-click reset. Adjacent Pan/Volume lanes appear only when corresponding automation exists and remain collapsible. |
| Automation storage | Native row commands with reconstructed editable corners. This is quantized tracker automation, not independently stored continuous curves. Occupied unrelated commands are protected. |
| Ownership shapes | Raised group headers, lower children, lower automation and clipped corners. The group parent is not a separate global master track. |
| Document tabs | Visible song tabs with activation, close and unsaved indication. Drag reordering and detached document windows remain deferred. |
| Floating transport | View entry, T recall, placement recovery and disabled docking are implemented. The owner still reports classic caption controls reappearing on both the main frame and Transport. |

### Important partial features

- Composites are currently saved inside MPTM songs. There is no standalone composite preset save/load workflow.
- The current mapping dialog uses From key, To key and Transpose fields. It is not the proposed 88-key graphical editor.
- Each child currently has a single contiguous range and transposition. Arbitrary key sets and explicit fixed sounding pitch require additional representation.
- Existing header dragging moves/copies individual channels. It does not implement the requested multi-selection drag onto a bus body, group-carrying drag, or ungrouping insertion zones.
- Existing song format conversion remains available through OpenMPT. Native .synmpt identity, default save behavior and recoverable conversion prompts are unbuilt.
- Track-body tint uses existing assigned colors. It is not a name-rule auto-color system.
- Normal tracker Record and Edit Step exist. Edit Step advances the cursor; it does not implement the requested musical note-length, rest and tie workflow.
- The custom MPTM track extension requires this fork's playback code for matching bus and instrument-override behavior. Upstream playback ignores those extensions.

## Completion checklist for the existing fork

These are the outstanding requests from the last functional list. The checks below describe completion, not results already achieved.

### Piano mapping editor

- [ ] Add a small piano button on a composite/group track.
- [ ] Open a compact 88-key keyboard with Middle C clearly identified.
- [ ] Give each child a named sample/instrument selector. Selecting a child highlights its assigned keys.
- [ ] Support a single-key assignment and dragging across a range, with clear/edit/undo.
- [ ] Use mapping colors independent of song-track colors. Keep labels and ownership visible when multiple children share a track color.
- [ ] Make overlap/conflict behavior explicit. Existing songs already support layered ranges; a simpler first GUI must preserve them even if creating new overlaps is initially restricted.
- [ ] Support the intended drum and guitar uses: fixed sounding pitch where needed, and pitched ranges with a defined reference pitch.

Acceptance: map a drum and two guitar articulations, audition/record through the armed folder, edit a range, undo, save and reload with the same routing. Verify the Middle C label against the actual input note rather than assuming tracker octave names match other software.

### Reusable composite presets

- [ ] Save a named composite with its group, children, sounds, assignments, mapping colors and relevant mix settings.
- [ ] Recall it into another song and create the required tracks.
- [ ] Modify and save the preset through an explicit workflow.
- [ ] Define sound packaging/reference behavior and missing-asset handling.

Recommended first behavior: loading makes an independent song instance; editing it does not silently alter other songs or the source preset.

Acceptance: save a guitar/drum composite, recall it into a fresh document, play it, change the local instance and confirm the original preset is unchanged until explicitly saved.

### Native documents and recoverable conversion

- [ ] Make .synmpt the default native document with explicit identity and versioning.
- [ ] Offer conversion when another song format is opened, with a way to retain the original format or cancel.
- [ ] Convert in memory and save a separate native copy; preserve the source file.
- [ ] Keep musical data and workspace/layout data distinguishable.
- [ ] Provide conversion/export out to supported formats with an explanation of what cannot be retained.
- [ ] Preserve recovery data before incompatible native upgrades overwrite a saved file.

The earlier proposal was to reuse MPTM musical storage plus versioned SynMPT metadata. That remains a proposal for the current fork, not a settled storage specification for a future tracker. Renaming an extension alone does not establish a native format. Lossless export of custom buses, mappings and other future features must not be assumed.

Acceptance: open and convert a copy, save/reopen it, recover the original, and export a separate MPTM/other supported copy with feature losses accurately described.

### Empty documents and creation buttons

- [ ] Start a fresh document with no visible tracks.
- [ ] Add + Track for an ordinary track or an empty group.
- [ ] Add + Preset to instantiate a saved instrument setup and its tracks.
- [ ] Keep song templates available for prebuilt arrangements.
- [ ] Handle empty-document save/open, adding the first track and deleting the final track safely.

The present engine/editor assumes native channel counts. Decide whether the implementation supports true zero-channel songs or represents an empty workspace separately; neither approach is implemented yet.

### Direct track arrangement and grouping

- [ ] Drag a selected set together, preserving its order.
- [ ] Move a group with its descendants.
- [ ] Drag onto a bus body to group tracks; highlight the receiving right edge and preview their lower child position.
- [ ] Drag into the top insertion zone to place tracks before/after a neighbor without grouping, including removing them from a group.
- [ ] Keep the insertion side and new ownership clear before release.
- [ ] Commit one undoable change and preserve notes, automation, instrument routes, selection and folder membership.

Acceptance: group, ungroup and reorder a nonadjacent selection and a nested folder; cancel a drag; undo/redo; save/reopen with the same sound and ownership.

### Keyword auto-color rules

- [ ] Provide keyword, enabled state and hex color/swatch.
- [ ] Evaluate rules from top to bottom; first match wins.
- [ ] Allow priority reordering.
- [ ] Apply on track creation/rename with a clear Auto/Manual setting.
- [ ] Keep mapping-editor colors independent.

Case-insensitive matching and manual-color preservation were proposed defaults. This is inspired by the separate SWS extension for REAPER.

Acceptance: create two conflicting rules, change their priority, rename a track and confirm a manual override remains intentional.

## Earlier requests still on the list

These predate the immediate six-item list and remain relevant to composition:

- Musical step entry using note values, with duration separate from cursor advance; Advance, Rest, and Extend/Tie actions. Dotted/triplet support depends on available timing resolution.
- A global master track with its own controls and automation.
- Separate sample and instrument editor windows.
- Further compact consolidation of song-wide controls.

Document-tab reordering and pulling a song into its own window were explicitly low priority. New icon artwork was also deferred.

### Recommended order

1. Establish a usable composition baseline: address rendering faults that hide controls or break navigation, with no additional appearance expansion.
2. Build the graphical composite mapper and its persistence.
3. Add reusable presets, followed by + Track / + Preset and the empty workspace.
4. Add direct drag grouping and keyword auto-coloring.
5. Complete native document/conversion handling, coordinating metadata needed by the preceding work.
6. Return to musical step entry, master automation and separate editors.

This ordering is a recommendation. Piano mapping and reusable presets are the clearest next composition features; basic routing already exists beneath them.

## Future game-focused tracker

### Level track

One level track contains the instruments/samples, reusable patterns, named musical sections and cue definitions for a level. Rooms and gameplay events select the appropriate music. Composition remains sequenced music rather than a collection of prerecorded audio loops.

A section arranges patterns, specifies its entry and loop behavior, and provides musical transition points. A cue is a requestable musical action; it can select a section or trigger a temporary phrase.

Illustrative content:

| Section or cue | Purpose |
| --- | --- |
| Arrival | Opening phrase, then the normal room music |
| Exploration | Default looping section |
| Machinery | A room/area-specific section |
| Combat | Music while combat is active |
| Secret | A brief musical announcement |
| Exit | Level completion music |

Names and musical content are examples, not a required template.

### Composer cue buttons

Provide a compact set of buttons that queue the same actions the game will request. The composer can play the level track and press Secret, Combat or a room-section button to hear the musical transition.

Proposed button feedback: idle, queued, playing, finished, plus a beat/bar countdown to the scheduled start. Each cue can carry its own timing rule so the composer does not need to manually time the click.

### Transition behaviors

| Behavior | Meaning | Design status |
| --- | --- | --- |
| Switch | Change to another section at its selected musical boundary. | Part of the section-selection direction |
| Temporary replacement | Replace a beat/bar or phrase, then resume the underlying music at the position it would have reached. | Explicit owner requirement |
| Overlay | Play an additional phrase while the underlying arrangement remains audible. | Proposed complementary behavior |

Proposed scheduling choices are immediate, next beat, next bar and end of section. Cue duration can be musical time or the authored phrase length. The exact first-version set remains open.

### Secret cue with continuous musical time

The owner's specific example is a secret announcement that replaces part of the arrangement while preserving its rhythm.

| Musical time | Audible result |
| --- | --- |
| Bar 7 | Normal section |
| Bar 8 | Secret phrase replaces the bar |
| Bar 9 | Normal section continues at bar 9 |

The base section keeps advancing while replaced. It does not pause, restart, or replay bar 8 afterward. Its sequencing, automation and relevant playback state must advance consistently so re-entry lands at the expected musical state.

Replacing a melody group while drums and bass continue is a proposed option; replacing the whole arrangement is another. Cues should normally inherit the active musical clock. The composer can write their harmony and instrumentation for the level and audition them against its different sections.

### Shared editor and game playback

Use one playback core for composition preview and the game. The editor sends cue requests from its buttons; rooms and events send the same requests through a small game-facing interface.

Proposed requests include selecting a room section, starting/ending a combat override and triggering a named cue. Use stable identifiers behind editable names so renaming or reordering sections does not silently change game references.

Recommended behavior: requesting the same room section again does not restart it. When combat ends, resolve the current room's music rather than blindly returning to the room where combat began. These policies still need a concrete implementation specification.

### Open design questions

- Which existing module import/export behavior must a future tracker preserve exactly?
- Will the playback core adapt this fork's engine or use a smaller purpose-built engine? Qt was suggested for the interface; no framework has been selected.
- How do repeated and simultaneous cues queue, replace, cancel or interrupt one another?
- During a temporary replacement, how do sustained notes, note attacks, release tails and effects cross its boundaries? Continuous time is required; exact audible re-entry needs a defined policy.
- What happens if the room/combat section changes while a temporary replacement is playing?
- Which timing grids, meter changes, tempo changes and transition options belong in the first version?
- Will preset mappings allow arbitrary separate key assignments and new overlapping layers immediately?
- How are assets packaged for portable songs/presets and game deployment?

### First proof for the future system

Open a level track, play and edit it, save/reopen it, and trigger sections from buttons. Demonstrate a one-bar secret replacement returning on the correct bar. Run the same cue requests through the playback core without the editor and compare the musical result under matching settings.

This is a proposed future milestone. The current request restores the existing fork's completion list first.

## Evidence and handoff

Implementation history reviewed:

- `71814bcbf`: compact track workspace, sampled buses and line automation.
- `71c22c701`: header geometry and track multi-selection.
- `21e2cbb91`: document tabs.
- `de934a78f`: collapsible folders and composite input.
- `455eef263`: ownership outlines; final pass before the gray appearance work.
- `1b8f4af33`: gray appearance and transport recall; the remaining functional list was recorded as deferred.
- Current code baseline: `4e14a019d`.

Source checks included the composite dialog and mapping model, track header/routing/drag handlers, automation input handlers, native file filters, and MPTM track-extension serialization. Existing handoffs and FEATURES.md were read as project history; they were not edited.

Earlier handoffs record successful builds and focused engine checks for bus mixing, composite routing, automation, MPTM save/reload and Yuzu progression. This documentation pass did not rebuild or run the application. The owner's reports of caption artifacts and editor drawing problems remain open regardless of those historical checks.

Deliverable: this single repository note, containing the completion audit and future cue design. No application code, songs, synced project files or SynCAD canonical documents were changed. No live desktop interaction was performed.

Footer: Resume composition functionality in the existing fork. Preserve the level-track and cue design for subsequent architecture work; do not treat the proposed engine/framework or unresolved cue policies as completed decisions.
