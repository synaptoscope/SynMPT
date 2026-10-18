# SynMPT conversion-on-open crash fix — 2026-10-18

Branch: `feature/composer-workspace`

Implementation commit: `6ddeb13e1af84abbde7c75594112fe1e22eeac4f`

## Cause and change

Accepting conversion inside `CModDoc::OnOpenDocument` called `ChangeModType`, which broadcast view updates before MFC had sent the initial update to the newly created views. The Song view tried to set a color button's text before its HWND existed. The saved crash stack passed through `ColorPickerButton::SetColor`, `CViewGlobals::UpdateView`, `ChangeModType`, `MakeNativeProject`, and `OnOpenDocument`.

Conversion now runs in `CModDocTemplate::OpenDocumentFile` after MFC has initialized the views and installed the source path, and after the document has been registered with the main frame. This also prevents MFC's subsequent path assignment from replacing conversion's Save As state. Invisible loads have no interactive conversion step. Cancel closes the new document without a misleading file-open failure notification.

The existing isolated editor checks now exercise the real document manager/template opening path with controlled Yes, No, and Cancel answers. Only the dialog response is substituted; document creation, conversion, saving, closure and reopening use normal application code. Fixture files are written to the test output directory, never over the reference song.

## Verification

- Before applying the fix, the regression check reproduced `Encountered an improper argument` on the first IT conversion and exited with failure. Log: `bin/synmpt-conversion-repro-checks.log`.
- Windows x64 Release build passed (VS2022 project, v145 toolset, Windows SDK 10.0.26100.0). Build log: `bin/synmpt-conversion-build.log`. Optional upstream SubWCRev/version-header notices remain.
- `/synmptEditorChecks` passed for IT, XM, MOD, S3M and MPTM: open with Yes, convert to native identity, retain modified/Save As state and protected source path, preserve the fixture note, save, close and reopen as native. Every original fixture remained byte-for-byte unchanged.
- No preserved the original format and clean state. Cancel returned no document and restored the previous document count without a failure popup.
- The same run passed 40 settled and 24 rapid editor switches plus the existing integrated composer checks.
- `/synmptChecks` passed, including native persistence, composite/bus/master rendering, and the Yuzu reference song advancing to order 2 with audible output.
- Whitespace/diff checks passed. No owner desktop input or screen capture was used; test processes were hidden and isolated.

## Preview and open questions

Use `bin/synmpt-conversion-fix-preview/OpenMPT.exe`. It has a separate portable profile; the previously running preview was not replaced or terminated. Final editor/runtime/composer logs are alongside this executable. Binaries, test fixtures, settings, crash diagnostics and logs remain ignored build artifacts.

The reported initialization crash is reproduced and fixed. The automated conversion cases use small generated fixtures; they do not establish musical equivalence for every imported legacy format. Existing conversion semantics and warnings remain in place.

Footer: One public SynMPT bug-fix handoff. No synced project sources, canonical SynCAD documents, private-project material, or owner song files were changed. Changes remain on the feature branch.
