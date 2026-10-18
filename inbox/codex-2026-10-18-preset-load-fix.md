# SynMPT preset save/load fix — 2026-10-18

Branch: `feature/composer-workspace`

Implementation commit: `2bc5ff0beeb536914bfee66ecb6c0dbe3103070b`

## Built and decisions

- Fixed loading native metadata when a preset contains no samples or nonempty pattern payload. The IT/MPTM reader now retains the actual end of the plugin chunks, comments and instruments as a fallback location for extensions. This also covers empty folders and empty instruments with comments.
- Saving a preset now resolves an imported track's instrument from its pattern references when no explicit header assignment exists. It copies that instrument/sample into the preset, remaps the track, preserves sample-derived instrument names, and shares identical references within one preset instance.
- If a track uses multiple pattern instruments, saving explains which track needs an explicit instrument assignment. It does not silently choose one or modify the source track's binding.
- Optional local reproduction inputs for `/synmptEditorChecks` use `SYNMPT_CHECK_PRESET` and `SYNMPT_CHECK_SOURCE`. They are read-only. Recovery requires exactly one source track with each matching name and an unambiguous instrument; the output goes to the isolated preview folder. No owner file paths or media are committed.

## Verification and recovery

- Release x64 build passed using v145 and Windows SDK 10.0.26100.0; no compiler warnings from this patch. Optional upstream SubWCRev/version-header notices remain. Build log: `bin/synmpt-preset-build.log`.
- `/synmptChecks` passed: empty-preset metadata with/without comments and empty instruments, native save/reload, sampled composite/bus/master rendering, and Yuzu advancing to order 2 with audible output.
- `/synmptEditorChecks` passed: actual preset save/load commands, empty-workspace recall, inferred and explicit instrument assignments, ambiguous-binding detection, unchanged source assignments, embedded sample bytes/names, and independent recalled assets. Existing five-format conversion, tab-switching and composer checks also passed.
- The reported original preset now loads its 18 tracks, but it contains zero instruments. Its missing sounds cannot be reconstructed from that file alone.
- A separate recovered copy was rebuilt from the owner's matching source song: 18 tracks (one folder and 17 children), 10 embedded instruments. The sample bytes for every mapped key within the format's supported range matched the source. The recovered preset reopened successfully through the actual preset command.
- SHA-256 checks confirmed the original preset and source song were unchanged. The verified copy was placed beside the original as `GTR-recovered.synpreset`; the original was not replaced.
- Diff whitespace check passed. Test processes were isolated and hidden; no desktop input or screen capture was used.

## Preview and remaining boundaries

Use `bin/synmpt-preset-fix-preview/OpenMPT.exe`, then **+ Preset → Add preset…**. The recovered file is also available locally as `bin/synmpt-preset-fix-preview/recovered-instrument.synpreset`. Editor, composer and runtime logs are beside this executable.

Recovery preserves the original folder and mapping settings. It does not invent keyboard ranges or enable composite input. A track that deliberately switches between several instruments still requires an explicit preset assignment. Owner assets, recovered audio, logs and binaries remain outside version control.

Footer: One public SynMPT bug-fix handoff. No SynCAD canon, synced sources or private tracker-design documents were changed. The tested changes remain on the feature branch.
