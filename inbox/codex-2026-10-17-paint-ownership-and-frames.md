# SynMPT paint ownership and title-bar interaction — 2026-10-17

Branch: `feature/hover-chrome`

Implementation commit: `15319df3f83fd98919f70381cf7a680745cf93e1` — Fix editor paint ownership and isolate dark caption interaction.

## Built / decisions

- Completes the prior locally built, uncommitted editor painting pass. The splitter background explicitly excludes its visible child panes. Group-frame backgrounds exclude enclosed sibling controls. Tab backgrounds exclude pages above them, and the active upper page is explicitly stacked above the full-size tab control. The native splitter retains its existing drag tracking styles.
- Tab and group painters erase their backgrounds in their clipped foreground pass; themed buttons use the same renderer for WM_PAINT and WM_PRINTCLIENT. No repeated synchronous whole-window repaint workaround was added.
- Captioned windows now give the native nonclient painter only the menu region. The theme owns caption/frame painting, preserving the native menu's layout and owner-draw route.
- The earlier WM_NCACTIVATE lParam=-1 workaround was insufficient: the installed MFC CFrameWnd implementation changes that parameter to zero before calling its default window procedure. Short activation/title/icon/enabled-state operations now temporarily mask WS_VISIBLE while the framework handles state, then restore visibility and paint the themed frame. This does not hide the HWND, freeze the window for a drag loop, or force an initially hidden window visible.
- The theme owns minimize/maximize/restore/close press tracking and mouse capture, then posts the original WM_SYSCOMMAND on release over the same enabled button. Releasing elsewhere, Escape, capture loss and cancellation issue no command. Caption dragging, resizing and their native framework paths remain in place.
- Escape is checked in the shared application/dialog keyboard route because capture redirects mouse input but keys can still target a focused editor. The existing T transport shortcut uses that same renamed helper.
- Includes the previously verified matching-editor binding and asynchronous view replacement behavior from the preceding commit; no new audio, document format or composition features were introduced.

## Reproduction / verification

- New editor paint regression failed before the clipping/stacking fixes: the splitter overwrote editor-area marker pixels, every active page was below its tab control, the tab background covered the page, and group frames covered enclosed controls. Saved log: `bin/synmpt-paint-repro-checks.log`.
- The new caption fixture failed before this pass: native caption painting was still reachable, MFC activation still permitted classic painting, and native button tracking owned the interaction. Saved log: `bin/synmpt-frame-repro-checks.log`.
- Final Windows 10 x64 Release build passed with v145 and SDK 10.0.26100.0. No compiler warnings/errors in the final build; existing optional subwcrev/svn_version.h notices remain. Log: `bin/synmpt-frame-fix-build.log`.
- `/synmptChecks` passed for native main-frame and floating-tool caption paint exclusions, the MFC activation behavior, visibility preservation, correct button commands, release-away/Escape/capture-loss/cancel-mode cancellation, and move/size notifications. These are in-process native message fixtures, not a physical drag screenshot test.
- Existing scrollbar surface lifecycle/32-bit ranges/unchanged redraw checks, native MDI menu buttons, hover and selector stability, label/status/font/color rendering, composite routing, bus audio/automation, MPTM save/reload and v1 compatibility passed. Yuzu advanced to order 2 with output.
- `/synmptEditorChecks` passed 40 settled and 24 rapidly queued editor selections. Rendering checks protected the upper/lower panes and 282 control locations in 29 group frames across the five editor pages. The document's dirty state was unchanged. Longest settled switch in the hidden test process: 245 ms; this is not a desktop smoothness measurement.
- Final logs: `bin/synmpt-frame-fix-preview/synmpt-runtime-checks.log` and `synmpt-editor-checks.log`.
- CRLF-aware whitespace check passed.

## Preview / open questions

`C:/Users/Nate/Documents/GitHub/SynMPT/bin/synmpt-frame-fix-preview/OpenMPT.exe`

Separate portable profile. The owner's running `synmpt-paint-fix-preview` instance and song were not targeted. An empty instance of the new preview was opened solely for inspection and closed before rebuilding.

Desktop capture failed with `FrameArrived timed out: timed out waiting on channel`, then `window capture timed out: timed out waiting on channel`. The owner explained they were concurrently using the desktop and asked that live inspection not interfere. No further live inspection was attempted; use code-level checks while they are working and leave interactive acceptance to them unless coordinated explicitly.

Owner acceptance: drag the main caption and Transport, click their window buttons, then cycle the five editor tabs. Confirm the classic light buttons do not return and page controls remain visible. Physical drag appearance remains unverified by the agent.

Footer: One handoff for the combined pending editor-paint work and this frame-interaction pass. No SynCAD canon documents, synced sources, owner images or songs were edited. No external messages or pull requests were created.
