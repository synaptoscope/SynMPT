# Composite instrument audition — 2026-10-18

Branch: `feature/composer-workspace`

Implementation commit: `fd59a40f33228223093623b3b7dd01432a825750`

## Built and decisions

- The composite keyboard defaults to **Map + audition**: dragging assigns the selected child's keys and previews that child at its mapped pitch. Ctrl still adds/removes keys. **Play mapping** previews all accepting children without editing their assignments. Computer note shortcuts and MIDI audition the complete working mapping.
- Added **First key plays** to choose the sounding pitch of the first mapped key directly. This edits the existing semitone transposition; fixed pitch remains available for drums. The readout identifies the input note/MIDI number and each receiving lane's sounding note. Keyboard edits keep the displayed range bounds current. An existing out-of-bounds bitmap conversion for upper input notes was bounded to the stored key-mask size.
- The owner clarified that pitch control means root/fixed note and transposition. Live pitch-bend controls are outside this pass. Existing project/preset mapping fields are reused; no format revision is needed.
- Audition uses pending dialog mappings and never writes pattern notes. OK accepts the mappings; Cancel leaves document mappings unchanged. Mouse release/capture loss, computer key release, MIDI note-off, sustain release, deactivation, and closing release owned preview voices. While inactive, the mapper ignores MIDI and retains the MIDI destination so events cannot reach the recording view behind it. Closing restores the prior MIDI destination and SysEx callback.
- Preview voices have per-document generation IDs so releasing an old key cannot stop a newer voice that reused its audio slot.
- Added the visible **Play only** toggle beside Export, synchronized with the existing recording state. A selected configured composite can be played without arming it. Play only avoids note/cursor writes, automatic MIDI pattern-start, and the whole-row preview preference. Re-enabling recording before releasing an auditioned key cannot insert an orphaned note-off. Recording and composite arming continue to route written child notes as before.

## Verification

- Release x64 build passed with v145 and Windows SDK 10.0.26100.0. Existing unrelated Draw_pat.cpp local-variable-shadow warning remains.
- `/synmptEditorChecks` passed, including the new real-dialog/engine checks for selected-child audition, actual preview voice ownership, drag assignment, Ctrl add/remove, root-to-transpose conversion, layered playback at different pitches, unchanged mappings in Play mapping, independent MIDI channels, sustain, computer note shortcuts/release, capture loss, inactive-window MIDI isolation, closing/target restoration, unchanged song notes and modified flag, preset pitch save/reload, Play only state synchronization, unarmed composite audition, unchanged cursor/pattern data, recording-toggle release protection, and armed recording.
- The same run passed the existing conversion/open/save/reopen checks for five formats, editor tab switching, presets, grouping undo/redo, step entry, ties, and native save/recovery checks.
- `/synmptChecks` passed: sampled composite transposition/layering and audio output, buses/master processing, native metadata round trips, preset loading, and the stock reference song advancing to order 2 with nonzero rendered audio.
- Inspected the full dialog rendered into an owned memory bitmap; its controls, keyboard, pitch readout and confirmation buttons fit. No desktop input automation or desktop screenshot was used. MIDI tests send messages to the actual handlers; physical MIDI hardware has not been manually tested.
- Whitespace check passed with CRLF-aware settings, preserving the resource file's existing CRLF representation.

## Preview and use

Executable: `bin/synmpt-audition-preview/OpenMPT.exe`. The previous executable remains available. Build and check logs are beside the new executable.

Open a folder's composite keyboard. Select a child and drag its keys to hear it. Set **First key plays**, Transpose, or a fixed Output pitch. Switch to **Play mapping** to try the whole instrument without editing ranges. Computer keys use the song toolbar's octave; MIDI uses incoming pitches. Press OK, then save the song or resave its preset to retain mapping changes.

In the song view, enable **Play only**, select the composite folder, and play using the computer/MIDI keyboard. Turn Play only off and arm the folder when ready to write notes.

Open questions: none blocking. Live pitch bending is not requested for this pass.

Footer: One public SynMPT implementation handoff. No SynCAD canonical docs, synced sources, owner songs/presets, or private tracker-design documents were changed. Binaries, logs and rendered UI images are ignored local artifacts.
