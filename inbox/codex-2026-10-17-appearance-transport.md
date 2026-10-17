# SynMPT gray appearance and floating transport

Date: 2026-10-17 (America/New_York)
Branch: `feature/track-workspace`
Implementation commit: `1b8f4af337f9b50ac0444d318d9b9c772ea727d9`
Base: `d17ef5502`

## Owner scope

The owner authorized the appearance work discussed in this chat: a gray dark default, subtle track-body colors, distinct musical-value colors, and modern fonts. They also requested reliable floating transport recovery, a single View entry, T to summon/raise it, full-button active Mute/Solo colors, and a round red record-arm dot. Other functional ideas were to be logged for later. Work resumed from its saved edits after the owner reported a power outage.

## Built

- A shared gray palette now covers app-owned editor surfaces, headers, native dialog controls, toolbars, document tabs, trees/lists and menus. Native app controls are themed within registered SynMPT windows; Windows color settings are not changed. Windows-owned file pickers, non-client scrollbars and some native control details retain Windows styling. There is no full light-theme switch in this pass.
- Existing track colors tint the header's unused body space at 18% and normal/beat/measure grid backgrounds at 16%. Note/instrument/volume/effect colors are preserved. Selected cells and current/playback-row backgrounds retain their own colors. Automation uses gray-based backgrounds and lighter orange/green curves.
- The default pattern palette uses warm-white notes, blue instrument numbers, green volume, violet pan, amber pitch and warm effect colors, with quieter empty placeholders. Existing color/font configuration remains available. The first run migrates the profile once, retaining its previously read palette/font values in SynMPT backup settings. Explicit palette updates are written before the appearance-version marker. Old preview profiles are retained separately.
- Dialog resources and shared interface fonts use Segoe UI 9 pt. Pattern/comments default to Consolas 9 pt. Custom pattern fonts now rasterize to 32-bit grayscale and retain 16 coverage levels; the final compositor blends those edges against the actual selected/tinted backgrounds. Legacy bitmap fonts remain usable. An acquired window DC is now released after creating the compatible font DC.
- Mute fills its active button blue, Solo fills yellow, and armed Record fills red with a contrasting circular dot. Unarmed Record retains a smaller red-looking dot on a neutral face. Existing multi-mute/solo and composite/native record routing are retained; both native record groups share the dot presentation.
- View and toolbar context menus expose one Floating Transport entry. T summons it near the cursor when the cursor is inside the main window, otherwise near the editor, clamped to the monitor work area, and raises the owned floating frame without taking editor focus. Repeats are consumed; editable text fields and disabled-main-frame/modal contexts are excluded. T is presently a fixed shortcut and is reserved from note entry outside text fields.
- Old docked transport states float on startup. Docking is disabled for the transport; double-clicking its toolbar or floating caption cannot redock it. Application activation restores a wanted transport that was hidden by window activation. View can still deliberately hide it, and T recalls it. Main-toolbar docking remains separate.
- `FEATURES.md` now records the deferred composite keyboard GUI/independent mapping colors/presets, `.synmpt` identity and recoverable conversion, zero-track document/add-track/preset workflow, drag grouping/insertion indicators, ordered keyword auto-color rules, and earlier parked tasks. These are requirements/proposals for later work, with current implementation boundaries called out.

## Decisions and boundaries

The appearance uses app-local Win32/MFC painting and existing controls. No system-wide palette changes, new audio engine or song-format migration were added. Theme implementation lives with the existing application translation unit so supported generated project files continue to build without additional source registrations. The indexed pattern buffer retains palette semantics; grayscale glyph coverage and per-track background tint are combined into a temporary RGB row when blitting.

Native `.synmpt`, conversion prompts/recovery, a piano mapping editor, fixed-pitch drum mapping, standalone composite presets, true zero-track documents, drag-to-parent changes and keyword auto-color rules remain deferred. Existing MPTM composite routing still permits layers/overlaps. Native Windows visual details and final contrast/font density remain owner-review items.

## Verification

- Final Windows 10 x64 Release build passed with MSVC v145 and SDK 10.0.26100.0. No compiler warnings/errors in the final `bin/synmpt-appearance-build.log`. Existing optional subwcrev/svn_version notices remain.
- Focused automation/grouping/header tests passed: native storage and occupied-cell protection, automation lines, nested bus gain/pan/mute and solo, multi-selection/solo, folder order/collapse, ownership corners, hit boundaries and fractional-DPI header sizes. Log: `bin/synmpt-automation-check.log`. Its existing standalone preprocessor/MBCS notices remain.
- Final-executable memory-bitmap checks passed for track tint staying in its column, selection colors winning over tint, syntax text retaining its color, glyph edges blending against tinted backgrounds, clearing stale coverage, full active blue/yellow button backgrounds, armed/unarmed round record-dot colors, and creating the modern coverage atlas. Source: `test/synmpt/runtime.h`.
- Final native audio checks passed for composite selection/transposition/layers, bus gain/pan/mute/automation, instrument overrides, MPTM save/reload and v1 extension compatibility. Yuzu - Yu-Lib reached order 2 with audible output. Log: `bin/synmpt-appearance-preview/synmpt-runtime-checks.log`.
- The isolated preview launched. Accessibility exposed a separate floating Transport and the simplified View menu. View hid the transport; T brought it back with workspace focus retained. The preview closed cleanly once, persisted AppearanceVersion=1 and the palette backup, then restarted with its floating Transport present. The final build also launched after the last modal/profile safeguard changes.
- Screenshot capture failed with `FrameArrived timed out: timed out waiting on channel`. The native file chooser could be observed but file-field input failed with `coordinate input geometry is unavailable`; subsequent Escape input did not reliably dismiss it. Only the empty appearance test process, with no song opened, was stopped to leave no stuck chooser. No owner song was edited. Full-grid visual appearance, mouse docking resistance, control overlap, actual multi-monitor placement and font feel are not claimed as GUI passes.
- Whitespace checks passed with CRLF recognized.

## Preview and owner checks

Executable: `C:/Users/Nate/Documents/GitHub/SynMPT/bin/synmpt-appearance-preview/OpenMPT.exe`

It has its own portable profile with single-instance forwarding disabled. Earlier preview binaries/profiles and the owner's song files are retained. The native checks also generate the existing disposable composite demo beside this executable.

1. Open a song with colored tracks. Check header/body tint, note-value differentiation, selection/current/playback rows and font readability; widen/scroll tracks and open automation to check alignment.
2. Toggle individual and selected-track Mute/Solo and record arming. Check the whole blue/yellow/red active face, circular Record symbol and existing audio/arming behavior.
3. Hide the Transport using View, press T over the editor, drag it near a screen edge, switch apps and restart. Check recall/placement and that caption/toolbar double-clicking or dragging over a dock edge cannot accidentally dock it. Type T into a track name/text field and confirm normal text input.

Footer: This is the single handoff file for this pass. Deferred requirements are in the existing FEATURES.md working notes. No synced project sources, owner images/songs or SynCAD canon documents were edited. Source, engine and renderer checks passed; visual acceptance remains with the owner.
