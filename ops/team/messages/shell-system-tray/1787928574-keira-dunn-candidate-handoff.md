# Keira Dunn — exact status-notifier S0 candidate handoff

- 2026-08-28T14:09:34Z — Candidate commit:
  `637cb94ea1c2e79a6c2f541b60a64ccbbbfab54f`, tree
  `dbbb605f6535b6b4d210be97f023b2110fde245c`, directly parented by the exact
  public base `9db68c4023257b49421101fa1b13c73bbc2cfa85`. One non-amended
  commit on `worker/system-tray-s0`; `git show --check` passes and the
  worktree is clean.

## Outcome and changed paths

A bounded StatusNotifier source/static foundation: validated item/icon/menu/
status values, an exact-owner keyed registry (unique bus name + object path +
registry-issued owner generation) with replacement/removal and stale-reply
fencing, bounded icon/menu/tooltip payloads, validated activation/context-menu/
secondary-activation request intents that are never executed, pure
Loading/Ready/Empty/Degraded presentation with stable ordering and
keyboard/accessibility identities, and an injected transport seam with no D-Bus
code in the module.

- New module `src/shell/status_notifier/**` (6 headers, 3 sources, 1
  self-contained CMakeLists, Qt Core only)
- New tests `tests/shell/status_notifier/**` (3 QtTest suites, 1 CMakeLists)
- `docs/wiki/shell/status-tray.md` (new primary page),
  `docs/wiki/adr/0026-status-notifier-exact-owner-foundation.md` (new),
  `docs/wiki/adr/index.md`, `docs/wiki/architecture/module-boundaries.md`
  (one row + one dependency bullet),
  `docs/wiki/shell/applet-runtime.md` (one reciprocal link), `mkdocs.yml`
  (two nav entries)

No production shell, applet registry, service, or roadmap path changed.

## Acceptance evidence

- Hostile GUI-less QtTest suites, exit 0 each:
  `qindaqt.status-notifier-values` 14/14, `qindaqt.status-notifier-registry`
  14/14, `qindaqt.status-notifier-presentation` 8/8. Coverage includes the
  five required hostile classes: spoofed owner (well-known-name and
  never-seen-generation rejections), stale reply (post-`ownerLost`
  registration/removal/request fencing, retained-generation attack), malformed
  menu/icon (dimension, byte-count, budget, depth, parent-cycle, separator,
  unlabeled, control-character cases), duplicate identity across live owners,
  and restart (generation advance, stale-generation refusal, reacceptance).
  The stale-reply test exposed a real fencing hole — the registry accepted a
  retained generation after owner loss — and the candidate contains that fix
  (owner liveness is now required alongside the generation match).
- Every new C++ file compiles clean under the repository warning set
  (`-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow`, Qt 6.11
  headers, GCC 16; the only diagnostics are inside Qt's own headers under this
  toolchain). Modules and tests were additionally compiled and linked offline
  with `/usr/lib/qt6/moc` into a temp directory; no shared build directory was
  touched.
- `git diff --check` clean; repository `check-source-shape --warnings-as-errors`
  passed (1016 files, 0 allowlisted skips);
  `tools/validate-docs` passed (65 Markdown documents plus navigation).

## Bounded caveats and next action

- `mkdocs build --strict` could not run: MkDocs is unavailable in this
  environment; the repository documentation validator plus local link
  inspection replaced it, matching prior S0 handoffs.
- The suite execution above used a hand-driven offline compile because the
  shared CMake wiring is intentionally untouched. The integration branch needs
  exactly two additive lines:
  `add_subdirectory(src/shell/status_notifier)` in the sources CMakeLists and
  `add_subdirectory(tests/shell/status_notifier)` in the tests CMakeLists; the
  test file self-guards on `QindaQt::StatusNotifier` until both land.
- Secondary activation remains pointer-intent-only by design (truthful empty
  keyboard description); a designed keyboard route is a later presentation
  decision, not an omission of this slice.
- Please assign an independent reviewer to attack the immutable exact commit,
  then integrate only after that review passes.
