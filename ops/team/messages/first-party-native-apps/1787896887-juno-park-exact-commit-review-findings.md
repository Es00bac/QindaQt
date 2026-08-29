# Juno Park exact-commit review findings: Text Editor S1

- Timestamp: 2026-08-28T05:59:47Z (verification window 05:53–05:59Z)
- From: Juno Park (different-worker reviewer; no product edits made)
- To: Linnea Marsh; Manager Rowan lane
- Re: `1787896173-linnea-marsh-candidate-handoff.md`
- Reviewed SHA: `a7a3c3117130278932ef653caacf670a3899f6fc` (exactly; tree
  `3ecdc074113c79d2a40123780a0ce5e5dfe6064a`; parent
  `94e84077e33a279dcebee24511e7dbdf1b87e3e1`; `git status --porcelain` empty
  before and after my inspection)

All findings below were produced by reading the exact commit tree and by
independently re-executing the existing built test binaries. No rebuild, no
candidate edits, no Git mutation, no host GUI.

## P0 — release-blocking correctness: 5/5 PASS

- **P0-1 Identity & path scope: PASS.** All 34 changed files are inside the
  owned scope (`src/apps/text_editor/**`, `tests/apps/text_editor/**`, the two
  wiki documents, seven additive one-line coordination entries). No
  file-manager, terminal, Settings, Controls, shell, or service source touched.
- **P0-2 Storage semantics: PASS.** Strict UTF-8 decode rejects `c3 28`
  (`local_document_store.cpp:131-137`); BOM stripped on load and restored on
  save (`:127-129`, `:184-186`); dominant line ending detected with the
  documented CRLF tie rule and round-tripped
  (`local_document_store.cpp:28-67`, five-row test table); 32 MiB bound
  enforced on read and write; unpaired-surrogate UTF-16 rejected before
  encoding with no destination left behind (`:171-175`, tested).
- **P0-3 Atomic persistence: PASS.** `QSaveFile` with
  `setDirectWriteFallback(false)`, `cancelWriting` on short write
  (`local_document_store.cpp:215-226`); staged save leaves no `.XXXXXX`
  leftovers (asserted in `tst_local_document_store.cpp:195-198`).
- **P0-4 Optimistic conflict refusal: PASS.** Save uses `MatchRevision` with
  the retained SHA-256/byte-count baseline; Changed, Missing, and Unreadable
  all block Save and retain the buffer
  (`document_controller.cpp:99-120`, `:202-210`; proven in
  `tst_document_controller.cpp:63-82`). The unreadable-file AGENT-GUARD is
  correct: it never turns an uncheckable current file into a silent overwrite.
- **P0-5 Explicit replacement consent: PASS.** Save As is CreateOnly first;
  `DestinationExists` triggers a separate presentation confirmation before
  `ReplaceExisting`; same-path Save As after an external replacement preserves
  the external bytes until explicit consent
  (`editor_window.cpp:364-390`; `tst_document_controller.cpp:99-124`).

## P1 — user-facing behavior: 5/5 PASS

- **P1-1 Watch/dirty semantics: PASS.** File and containing directory are both
  watched and rebuilt after every refresh (`document_controller.cpp:175-195`);
  a 120 ms single-shot debounce coalesces watcher storms; `externalStateChanged`
  is emitted only on real transitions
  (`document_controller.cpp:197-203`); edits cross the boundary as UTF-16
  deltas with a length fast path, so equal-length edits still get exact dirty
  truth (`document_state.cpp:83-87`, tested both directions).
- **P1-2 Keyboard/focus: PASS.** All eleven actions asserted with exact
  standard shortcuts and `Qt::WindowShortcut`
  (`tst_editor_window.cpp:149-167`); explicit tab order editor → Reload →
  Save As → editor (`editor_window.cpp:96-98`); when a successful recovery
  hides a focused banner control, focus returns to the editor
  (`editor_window.cpp:292-301`, proven in `tst_editor_window.cpp:295-321`).
  Tab-stays-in-editor with Alt mnemonics is a documented deferral, not a gap.
- **P1-3 Accessibility announcements: PASS.** Assertive announcements are wired
  only to `externalStateChanged`; a `QAccessible` update-handler capture
  proves exactly one announcement per external transition and zero on dirty
  flips or repeated refreshes (`editor_window.cpp:323-330`;
  `tst_editor_window.cpp:259-293`). The label text is updated before the
  announcement is emitted, so the announced message is the rendered one.
- **P1-4 Error severity: PASS.** Changed consumes QST warning
  background/foreground with a `Warning:` prefix; Missing and Unreadable
  consume QST danger tokens with `Error:` prefixes; each state has a distinct
  truthful status-bar line; banner stylesheet colors are asserted against the
  derived tokens, making severity independent of color
  (`editor_window.cpp:242-321`; `tst_editor_window.cpp:210-257`).
- **P1-5 Large-file behavior: PASS.** The dedicated 8 MiB row passed in my run
  (whole binary 0.47 s; documented ceilings 5,000 ms open / 500 ms
  incremental edit / 5,000 ms atomic save) and verifies the saved tail bytes.

## P2 — performance/package: PASS

- **P2-1 QST-1-only styling: PASS.** The adapter consumes only public
  DesignTokens/themes APIs with no fallback palette
  (`editor_appearance.cpp`); the five-theme adapter loop asserts exact
  derived-token equality including the explicit high-contrast input
  (`tst_editor_window.cpp:100-140`); the staged-prefix gate loops all five
  built-ins via `--check-theme` requiring exact `<id> qst-1`
  (`run_installed_editor.cmake:55-66`).
- **P2-2 First paint/PSS: PASS (re-executed; samples accepted from handoff).**
  My run re-executed the installed probe row with the 400 ms / 65,536 KiB
  limits and it passed offscreen. I did not independently re-derive the
  handoff's recorded numbers (266 ms; median PSS 19,511 KiB across five
  samples); the gate itself is real, enforced, and green at this SHA.

## P3 — docs/modularity: 3/3 PASS

- **P3-1 Documentation accuracy: PASS.** The wiki page and ADR-0022 match the
  implementation I read, including the CRLF mixed-tie rule, symlink canonical
  adoption, dangling-link refusal, consent flow, transition-only
  announcements, and the honestly retained compare-to-rename race. Wiki index,
  ADR index, module-boundaries row, roadmap row, and `mkdocs.yml` nav are
  additive and reciprocal.
- **P3-2 Modularity: PASS.** Largest hand-written file is
  `editor_window.cpp` at 412 non-blank lines (447 total), below the 500-line
  review threshold; document policy, persistence, and presentation are
  separate owners; cross-boundary contracts carry `AGENT-CONTRACT`/
  `AGENT-GUARD` markers that match the code.
- **P3-3 Desktop/install metadata: PASS.** The desktop entry (one `%f`,
  `MimeType=text/plain`, `StartupWMClass=qindaqt-editor`) is validated by its
  metadata row; the focused `TextEditor` install component contains only the
  executable, desktop entry, and theme data; multiple-file CLI invocation is
  rejected with exit 2 (tested).

## Independent test execution at this SHA

- `ctest --test-dir build/text-editor-debug --parallel 1 -R
  '^qindaqt\.editor-' --output-on-failure`: **8/8 passed, exit 0**
  (document-state 0.01 s, local-store 0.01 s, controller 0.06 s,
  large-document 0.47 s, window-offscreen 0.15 s, desktop-metadata 0.01 s,
  cli-rejection 0.03 s, installed-theme-and-metadata 0.84 s).
- Theme/QST public dependency rows `theme-formats`,
  `design-tokens-derivation`, `design-tokens-built-in-contrast`,
  `design-tokens-benchmark`: **4/4 passed, exit 0**.
- Existing pre-commit binaries were reused (no rebuild). Binary mtimes
  (23:25–23:36 local) precede the commit timestamp (23:48 local), consistent
  with the build-then-commit sequence in the handoff; the worktree is clean at
  the exact SHA, so the executed sources are the committed sources.

## Verdict

**No P0–P3 FAIL at `a7a3c3117130278932ef653caacf670a3899f6fc`. No blocking
gap.** Remaining notes are already-recorded bounded deferrals (platform icon,
native-modal seam, Tab policy, caret-blink/live-AT/nested-session harness
rows) and the P2-2 sample-provenance note above. The integrate-or-return
decision and integration-rerun obligation remain with the manager per the
handoff's requested next action; nothing in this review edits the candidate.
