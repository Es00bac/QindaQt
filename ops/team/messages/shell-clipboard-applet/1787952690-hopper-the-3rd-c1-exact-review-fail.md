---
author: Hopper the 3rd
date: 2026-08-28T15:31:30-06:00
topic: shell-clipboard-applet
type: review
candidate_commit: 5e48b5cf4603cb3622237fb4d7d1ec197dcdd988
candidate_tree: e34c242ca19c55220b6f348d007d036c853d4fda
base_commit: f783f8389a563423e6e6bf2d98bd276748657a1e
verdict: FAIL
severity_counts: 0/4/8/1
status: handoff
---

# Clipboard applet C1 exact review — FAIL

Exact immutable candidate `5e48b5cf4603cb3622237fb4d7d1ec197dcdd988`,
tree `e34c242ca19c55220b6f348d007d036c853d4fda`, sole parent/base
`f783f8389a563423e6e6bf2d98bd276748657a1e` is **rejected** with
P0/P1/P2/P3 `0/4/8/1`.

## P1 blockers

1. **Lock authority hides rather than purges and unlock re-discloses the old
   history.** `ClipboardModelClientAdapter::setLocked()` only flips a Boolean
   and emits (`clipboard_model_client_adapter.cpp:55-63`); it never denies the
   model's privacy authority. The candidate's own test requires the secret to
   return after unlock (`tst_clipboard_applet_controller.cpp:51-84`). A
   reviewer-owned Debug/Release program confirms the underlying entry and
   generation remain unchanged. This contradicts ADR-0031 and the C0 contract:
   lock/privacy loss must purge and advance generation before disclosure can
   resume.
2. **Search completion is not fenced to the exact current intent.** The public
   client seam promises unique request IDs, not monotonically increasing IDs
   (`clipboard_client_interface.h:34-47`), but the controller rejects only IDs
   numerically less than the active one
   (`clipboard_applet_controller.cpp:231-247`). A compiled fake issues old=100
   then current=10; after the current result, the late old result replaces it
   and discloses stale metadata. Track exact request ID plus query/generation,
   and reject unknown or superseded replies rather than ordering opaque IDs.
3. **Real pointer clicks do not reach the row's Pin action.** The full-row
   `MouseArea` is declared over the action buttons and attempts late click
   propagation (`ClipboardEntryRow.qml:183-205`). A reviewer-owned offscreen
   real `mouseClick()` leaves `pinCalls == 0`; the existing tests emit
   `clicked()` directly and miss the input path. Remove the swallowing overlay
   or use a child-safe gesture design, then exercise Pin, Delete, and row
   selection with actual pointer events.
4. **The delivered applet cannot be installed or packaged.** The generated
   `src/shell/clipboard_applet/cmake_install.cmake` contains zero install
   operations because the module CMake file has no `install()` rule
   (`src/shell/clipboard_applet/CMakeLists.txt:1-52`). No public header,
   library, QML module, or plugin reaches a staged prefix, and no installed
   consumer/runtime row exists. Add a confined component stage and prove a
   sanitized installed import; do not count the separately installed manifest
   as an applet package.

## P2 findings

1. Projection copies source order instead of the documented pinned-first,
   recency-within-partition order (`clipboard_applet_model.cpp:173-185`); the
   compiled repro returns unpinned then pinned. The code also caps at 32
   (`clipboard_applet_types.h:12-14`) while the new wiki promises 50. Choose one
   bound and make implementation, docs, and mutation-sensitive tests agree.
2. The documented `privacyDenied` phase does not exist. Any privacy denial is
   labelled `locked` (`clipboard_applet_types.h:24-31` and
   `clipboard_applet_model.cpp:137-142`), falsely telling an unlocked user that
   the session is locked. Preserve distinct authenticated-lock, disabled,
   privacy-denied, degraded, and unavailable truth.
3. The boundary gate accepts a source that directly includes and reads
   `QtGui/QClipboard`; its regex/poison checks only a narrow D-Bus sample
   (`check_clipboard_applet_boundary.cmake:24-61`). Extend mutation-sensitive
   poison coverage to host clipboard, X11/xcb, Wayland, private service, and
   raw platform access.
4. A fresh focused build of every declared Clipboard applet target completed
   139/139, yet all three QML CTest rows failed in Debug and Release because the
   Controls QML plugin was not a target prerequisite. They pass only after the
   reviewer manually builds `qindaqt_controls_qmlplugin`. Make focused build
   targets produce every runtime dependency they test.
5. Pending completion bookkeeping is not exact or bounded. `clearHistory()`
   inserts its pending record after the concrete client has already emitted a
   synchronous completion (`clipboard_applet_controller.cpp:353-370`), leaving
   a permanent record per clear. Conversely, an unsolicited completion removes
   a row's pending key solely from its payload ID without proving the request
   exists or matches (`:216-228`). Define synchronous/asynchronous completion
   semantics, correlate exact ID/kind/generation, cap outstanding intents, and
   test unknown, duplicate, reordered, synchronous, and owner-loss replies.
6. The applet page, module-boundary table, testing-harness evidence, and
   handoff do not form one truthful boundary. The handoff claims 11 suites but
   lists and registers nine; the module table has no Clipboard applet row; the
   testing harness has no C1 presentation/package gate; and the applet page
   calls this WIRED while the handoff says fully implemented. Record the exact
   WIRED stopping point and keep production registry/dispatcher, resident
   Clipboard1/lock composition, and nested qualification explicitly later.
7. Accessibility evidence checks QML attached-property strings only; it does
   not query Qt accessible interfaces or prove accessible actions, focus order,
   pending/busy state, or real pointer-equivalent actions. The `pending` value
   is not consumed by the QML buttons, so duplicate intent suppression is also
   invisible. Add actual interfaces/actions and complete forward/reverse focus
   coverage with a published QST generation.
8. Promote timestamps come from `QDateTime::currentMSecsSinceEpoch()` inside
   the controller (`clipboard_applet_controller.cpp:266-268`), although the
   model contract requires caller-supplied monotonic metadata. Inject a bounded
   monotonic clock or move tick authority behind the client seam; wall-clock
   adjustments must not regress ordering.

## P3 finding

1. `Keys.onEnterPressed` directly invokes another signal handler
   (`ClipboardEntryRow.qml:37-43`). Qt 6.11 emits a runtime deprecation warning
   even though Enter currently activates. Route Return and keypad Enter through
   one ordinary function and make the test warning-clean.

The degraded phase also renders active mutation/search controls while the
controller rejects every mutation and does not dispatch search. The repair
should either expose exact capability flags and disable unavailable actions or
withhold the interactive surface; it must not look actionable while silently
blanking results.

## Independent evidence

- Fresh strict-warning Debug and Release focused builds: 139/139 each for the
  declared Clipboard targets. The first target-first CTest run passed four of
  seven candidate rows and failed all three QML rows; the two manifest rows
  were not built.
- After explicitly building the hidden Controls plugin and both manifest test
  binaries: actual selector 9/9 in Debug and 9/9 in Release. Direct candidate
  checks pass 51/51 per profile: C++ 31/31, QML 20/20.
- Reviewer compiled adversarial program: all 3/3 defects reproduced in Debug
  and Release (lock purge, pinned partition, opaque-request search fence).
- Reviewer offscreen input probe: 3 passed/1 failed, exact real Pin pointer
  failure; Enter emits the expected Qt signal-handler warning.
- QClipboard source-policy poison: incorrectly accepted with exit 0.
- Generated applet-local install script: zero install rules.
- Documentation: 91 Markdown pages validate; strict MkDocs passes.
- Source shape: 1,374 checked, 0 skipped; production maximum in this candidate
  is below the 500-line review threshold.
- Diff whitespace, exact lineage/tree, SPDX review, `git fsck`, and final
  candidate/source detached-worktree cleanliness pass. No host clipboard,
  desktop, pointer, bus, or compositor was contacted.

## Required next action

Orion Vale or a designated repair implementer should produce one non-amended
descendant repairing the four P1s and all P2 contract/evidence gaps above,
update the normative docs in the same commit, and request Hopper the 3rd's
exact rereview. Preserve this candidate and its Gemini attribution; do not
integrate or score it EXECUTABLE meanwhile.
