# SynMPT composer workspace — 2026-10-18

Branch: `feature/composer-workspace`

Implementation commit: `cad6de7e037a103abc9d5a5ccb6552cdd0575360`

Baseline: `ee0eebf4d19f73b648b7ba6b56a93f22d8429b13`

## Built

- Native `.synmpt` documents with a version marker, default native new songs, conversion-on-open choice, protected imported originals, and MPTM copy export. Native saves write and flush a temporary file before replacement; `song.synmpt.bak` holds the exact previous saved version.
- Empty new workspaces, a compact action row in Patterns, direct track/folder creation, and deletion of the final visible track. The playback engine retains one hidden placeholder channel; undo restores removed tracks and patterns.
- An 88-key composite mapper with Middle C identified as MIDI 60. Select a child, drag a range, or Ctrl-click/drag to add/remove separate keys. Independent mapping colors and ownership strips show overlapping layers. Instrument assignment, transpose and fixed output pitch support pitched articulations and drum samples. Cancel discards dialog edits; accepting uses the existing channel undo system.
- Standalone `.synpreset` save/recall for a folder and its descendants. Presets embed sample data and instrument mappings, including externally stored sample data. Each recall copies sounds into independent instrument/sample slots. Re-saving a preset creates a `.bak` recovery copy before replacing it.
- Multiple selected tracks can be dragged into a folder body. Dragging to the top edge moves them outside the family. Folder descendants move together; cycles and moving the master into another folder are rejected. Right-edge and top-edge indicators distinguish grouping from insertion. The edit is one undo operation.
- Ordered, case-insensitive keyword auto colors with enable flags and hex values. First matching rule wins; manual colors are preserved. Rules are stored in `SynMPT-colors.txt` alongside settings. Existing track-body tint follows the resulting color.
- Musical step lengths, optional cursor advance, Advance, Rest and Tie. Consecutive steps replace the preceding automatic note end. Step entry uses composite routing when the folder is armed, and preserves unrelated occupied notes.
- A unique master track at the front, using the existing pan/volume automation editors. Its smoothed gain, balance and mute affect the final internal audio mix, including internal plugin returns and DSP. Sample folder gain is applied separately, once per ancestor.
- Independent sample/instrument windows sharing the same document. Initialization and page selection now support their own top-level frame. Closing an extra editor preserves the original song window. Editors also offers another song window.
- Document tabs can be reordered; dragging a tab outside the strip opens another song window. This is another view of the same document, with the original view retained.
- Added exclusions for native themed caption/frame paint messages and non-client cursor handling to the existing dark caption implementation.

## Use the preview

Executable: `bin/synmpt-composer-preview/OpenMPT.exe`, with its separate portable profile.

1. Start a new document. In Patterns, use **+ Track** to add tracks/folders or group selected tracks. Existing Ctrl/Shift selection, mute/solo, width gestures and automation remain available.
2. Open **Editors → Sample editor window** to import/edit sounds. Assign a sound using each child track's instrument row.
3. Click the folder's keyboard/instrument row, or **+ Preset → Edit selected composite keyboard**. Select a child on the left, assign its instrument, and draw its keys. Use a fixed output pitch for a drum; use follow-key plus transpose for pitched sounds. Overlaps deliberately layer the matching children.
4. Enable the composite, accept the mapping, and arm the folder's red record button. Input writes ordinary notes into the mapped child tracks. Choose a length under **Step…**, then use **Advance**, **Rest**, or **Tie** as needed.
5. Select the folder and use **+ Preset → Save selected folder as preset**. **Add preset** recalls an independent copy; edit that copy and explicitly save it when you want to update a preset file.
6. **Colors → Edit ordered auto-color rules** accepts one rule per line, for example `1|guitar|#89B78B`. Put higher-priority matches first. `0` disables a rule. The same menu applies automatic or manual colors to selected tracks.
7. **Master** creates or selects the master track. Use its pan/volume controls and automation like other track controls. **Export** writes a native or MPTM copy; ordinary Save uses `.synmpt` for native projects.

Recovery: open the `.synmpt.bak` file through the all-files filter, then Save As a `.synmpt` filename. Imported originals are retained. A failed final replacement can leave a `.saving-…` file for recovery; it is not silently substituted for the original.

## Verification

- Windows 10 x64 Release build passed, using the VS2022 project with v145 and Windows SDK 10.0.26100.0. Build log: `bin/synmpt-composer-build.log`. Optional upstream SubWCRev/version-header notices remain.
- `/synmptChecks` passed: native identity and mapping persistence, fixed pitch/layers, manual color state, master gain/pan/mute/automation, sampled buses, older track-extension compatibility, existing caption/scrollbar/font checks, and Yuzu progressing to order 2 with audible output.
- `/synmptEditorChecks` passed: 40 settled and 24 rapidly queued switches across all five pages, correct view bindings/layout/visibility, unchanged reference song, and the integrated composer workflow.
- 31 composer checks passed, covering empty/new tracks, multi-track grouping and undo/redo, embedded preset recall with independent sample copies, actual keyboard click/drag/Ctrl-click messages, cancellation, control bounds, consecutive steps/ties, master uniqueness, auto/manual colors, actual detached sample/instrument views, final-track deletion/undo, byte-identical save recovery and imported-source protection, and detached-window closure.
- The keyboard was rendered directly into an owned bitmap and visually inspected. Full desktop interaction was not used. Caption checks are controlled message tests; physical dragging on the owner's desktop remains an owner acceptance check.
- Final logs are in `bin/synmpt-composer-preview/`: `synmpt-runtime-checks.log`, `synmpt-editor-checks.log`, and `synmpt-composer-checks.log`. The longest settled tab switch in the final hidden run was about 256 ms; that is not a desktop smoothness measurement.
- CRLF-aware diff whitespace check passed. Test songs, logs, settings and binaries remain ignored build artifacts.

## Boundaries and open questions

- Folder buses and presets support internal sampled instruments. General plugin/FX bus chains remain outside this implementation. The final master cannot control audio produced by external MIDI hardware.
- Automation remains tracker-row commands. MPTM export preserves SynMPT extension data, but other players may ignore folder/master routing and mappings and therefore sound different. Legacy format conversion can discard these features; retain the native project.
- Step durations must fit the current row grid and pattern. Values requiring finer Rows/Beat are disabled; extension into another pattern is not automatic.
- Preset recall makes independent sounds. Undoing its track insertion can leave those newly imported assets available as unused instruments/samples. This does not overwrite existing assets.
- Drag feedback currently uses insertion/ownership edges; it does not continuously reflow all columns before release. Color-rule editing uses a compact ordered text list. A replacement icon set remains deferred.
- Additional song windows share a document rather than removing its original view. Tab order is maintained during the current session.

Footer: One public SynMPT implementation handoff. No SynCAD canonical documents, synced project sources, owner songs or images were edited. No desktop mouse/keyboard automation or external messages were used. The implementation stays on the feature branch for the owner to try and merge.
