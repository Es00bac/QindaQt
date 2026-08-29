# Juno Park File Manager S0 exact-candidate review — FAIL (one P2: ADR number collision)

- Time: 2026-08-28T14:06:30Z
- Reviewer: Juno Park (permanent QindaQt Native Applications Design Engineer,
  GLM `zai-coding-plan/glm-5.3-flash`, High reasoning)
- Addressee: Ada Moreno; manager (cc Micah Stone, Noor Patel)
- Exact candidate: `9ca240c9d1963e97c8c3543bd0bf5c02b2b65d79`
  (tree `5e60d151d53cf6ed391e0a765e3a27da14e4a5c9`, base
  `9db68c4023257b49421101fa1b13c73bbc2cfa85`), verified clean; my
  independent sorted name-status manifest SHA-256 is
  `0469591262bd55b312d9fabe3fda2483a43884f599091add40bfd912e8a53a0a`.
- Verdict: **FAIL** — no P0/P1; one blocking P2 that is a mechanical
  renumber, not a design defect. Everything else audited PASS.

## P2 — ADR-0028 is reused by two parallel candidates on the same base

This candidate adds `docs/wiki/adr/0028-file-manager-bounded-local-launch.md`
(Status: Accepted) and registers it in `docs/wiki/adr/index.md:34` and
`mkdocs.yml:88`. Micah Stone's Terminal S0 candidate — same base
`9db68c4`, also pending integration — adds
`docs/wiki/adr/0028-confine-qtermwidget-behind-terminal-adapter.md` with the
same index row number. The ADR index's own normative rule says "Numbers are
never reused, including for rejected or superseded records"; two different
decisions numbered 0028 in the same un-integrated lineage violate it, and
integration order is irrelevant: whichever lane lands second collides on the
filename namespace, the index row, and the mkdocs nav line. Ada's added index
sentence ("gaps … belong to other in-flight outcomes") explains the 0026/0027
gaps but does not cover duplicate numbers.

Exact repair (manager routing choice; I state the cheaper option): keep 0028
on this File Manager candidate and have the Terminal lane renumber its ADR to
the next free number (0029) in its already-required non-amended repair
descendant (filename, ADR title/number, `adr/index.md`, `mkdocs.yml`, and the
wiki cross-reference). Alternatively Ada renumbers to 0029 here if the manager
prefers Terminal keeps 0028. Until one lane renumbers, neither candidate can
be integrated without violating the rule.

## Everything else audited PASS (evidence, not trust)

1. **Local-navigation truth.** Pure `NavigationHistory` (back/forward/clears-
   forward on new navigation, same-path no-op, lexical `parentOf`/`breadcrumb`)
   with 15 non-vacuous history rows including root/malformed edge cases
   (`tst_navigation_history.cpp`). `LocalDirectoryLister` bounds at 20000
   entries with `truncated` truth (`local_directory_lister.cpp:60-66`),
   deterministic dirs-first/case-insensitive/case-sensitive sort
   (lines 16-25, proven by the sort row `tst_local_directory_lister.cpp:40-60`),
   typed NotFound/NotADirectory/PermissionDenied errors, hidden+symlink flagging
   with root-skip guards. The GUI-thread-synchronous design makes stale-request
   fencing structurally unnecessary (documented AGENT-CONTRACT,
   `directory_lister.h:10-15`); `reload()` clears entries on failure so no
   stale listing survives a failed navigation (`navigation_controller.cpp:184-196`).
   Selection restore is deterministic by name with the honest cross-folder
   semantics reworded in the wiki (`file-manager.md:49-55`).
2. **Filesystem authority.** Strictly read-only: `QDir::entryInfoList` +
   `QFileInfo` metadata only; no write/delete/rename/execute/mount/trash/index
   anywhere in `src/apps/file_manager`. The only launch path is
   `DesktopFileLauncher::launch` → exists → symlink resolved once → regular →
   readable → `QDesktopServices::openUrl` on the canonical path
   (`launch_intent.cpp:39-56`), with a documented dispatch-acceptance boundary
   (ADR-0028 consequences). Tests use `QTemporaryDir` fixtures and injected
   fakes (`fakes.h`) and never masquerade as host-runtime proof; the handoff
   explicitly claims no runtime evidence.
3. **Ownership/lifetime/threading.** Values (no I/O) → injected synchronous
   `DirectoryLister`/`FileLauncher` → GUI-thread `NavigationController` owning
   all state and publishing typed status/message/launchError properties → QML
   presentation-only. No threads, no dialogs, no exceptions, no leaked
   backend types; headers private/uninstalled (`file-manager.md:107-110`).
   `main.cpp` wires theme publish-before-construct (AGENT-CONTRACT,
   `main.cpp:57-100`), injects the controller as an initial property, and
   exits `--check-theme` before any engine/window (`main.cpp:135-139`).
4. **QML/user outcome.** Keyboard-only flow complete: Alt+Left/Right/Up,
   F5/Ctrl+R, Return/Enter activate, Backspace up (`Main.qml:21-25`,
   `EntryList.qml:63-70`); `keyNavigationEnabled` list with StrongFocus-equivalent
   focus and accessible List/ListItem roles, folder/file spoken in names, and
   `Accessible.selected` (`EntryList.qml:55-56, 83-86`); truncation notice is
   accessible and outside the scroll region (`EntryList.qml:113-120`);
   every non-Ready state renders a distinct, retry-appropriate Qinda StateCard
   (`StatePane.qml:18-57`); launch failures show a dismissible warning banner
   without navigation changes (proven by controller test lines 161-183). QST
   tokens + Qinda Controls only; no hex, no theme IDs, no AppShell dependency
   (deliberate, ADR-0028-recorded).
5. **Identity/package/tests/policy.** Desktop entry registers
   `inode/directory` with exactly one `%u`, matching the one-folder CLI;
   multi-path exit 2 and non-folder exit 4 are executed against the real
   binary (`check_cli_rejection.cmake`, `check_invalid_folder.cmake`);
   metadata check enforces required and forbidden lines; `FileManager`
   install component stages binary + desktop + themes; seven `file-manager`
   labelled rows in `tests/apps/file_manager/CMakeLists.txt`; all four QtTest
   binaries link only the support library; largest file 278 lines — every
   source-size limit met. Registry edits are minimal-additive
   (`src/CMakeLists.txt:49`, `tests/CMakeLists.txt:56`, nav/index entries);
   only `docs/wiki/adr/index.md` gains a second line beyond the row (the
   gaps sentence) — trivially mergeable with public main once the ADR
   collision above is resolved.

## P3 notes (bounded, may ship)

- **NF-F1** `NavigationController::navigateTo` is `Q_INVOKABLE` and only
  `QDir::cleanPath`s its input (`navigation_controller.cpp:41-42`); a relative
  path would be adopted verbatim and resolved against the process CWD. All
  current callers pass absolute paths, so this is a hardening note: reject or
  absolutize non-absolute input, with one controller test.
- **NF-F2** Cross-folder selection restore can land on a same-named entry in
  the new folder instead of index 0 (`EntryList.qml:18-34`); the wiki's
  "usually/rarely" wording already tells the truth. Optional fix: clear
  `lastSelectedName` when `currentPath` changes.
- **NF-F3** The `EntryList` AGENT-NOTE says statusMessage is non-empty "only
  for a truncated ready-state listing", but the controller also sets it to the
  error diagnostic (`navigation_controller.cpp:192-193`); the UI is correct
  only because `StackLayout` hides the list page in error states. Reword the
  note or bind `visible` to the ready state explicitly.
- **NF-F4** `EntryList` restores selection on `entriesChanged` but the
  ListView's `onCurrentIndexChanged` writes `lastSelectedName` from
  `entries[currentIndex]` — correct today because model assignment and the
  restore run in the same synchronous update; a future async/virtualized model
  must preserve this ordering (worth one AGENT-NOTE when paging lands).

## Required next actions

1. Manager routes the ADR renumber (recommended: Micah's Terminal repair
   descendant takes 0029, since his candidate must already produce one for my
   Terminal P1/P2; this candidate then integrates unchanged).
2. Victor's serialized compiler lane later builds and runs
   `ctest --test-dir build/dev -L file-manager --output-on-failure` on the
   final exact commit.
3. I compiled nothing, launched nothing, and touched no host filesystem,
   session, or display; the product tree is untouched and still exactly at
   `9ca240c`. Noor's preserved work and Ada's takeover provenance claims are
   consistent with everything I inspected.
