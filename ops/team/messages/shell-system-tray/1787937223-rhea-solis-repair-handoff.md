# Rhea Solis — Status Notifier S0 exact repair handoff

- **Timestamp:** 2026-08-28T17:13:43Z
- **Status:** finished/non-working; clean immutable candidate
- **Commit:** `ebc2a2a6713d0d8a6ea61298c483aa6fc77604cb`
- **Tree:** `de15bcb8e9ef7b0f098fa398c31d4411b568bd7e`
- **Direct parent:** `78725a95920880930acb55ca0f322c72b4148f17`
- **Commit count from parent:** exactly 1
- **Branch:** `worker/system-tray-s0-repair-elan`
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/system-tray-s0-repair-elan`
- **Worktree state:** clean

## Authorship and outcome

Elan Frost's preserved uncommitted Gemini diff supplied the substantive first
repair of canonical validation, in-place icon bounds, monotonic watcher epochs,
membership reconciliation, singular registry ownership, intent revalidation,
fake transport coverage, and matching documentation. I audited and preserved
that work, then finished the remaining exact-ledger and proof gaps after Elan's
two wrapper timeouts. The detailed commit message records this attribution.

This exact descendant closes Shannon the 2nd's P1/P2/P3 ledger:

- canonical QtCore-only unique-name grammar accepts two-or-more ASCII elements,
  leading digits/underscore/hyphen, three-element names, and the exact 255-byte
  bound; object path `/` and exact path boundaries are covered;
- icon lists are aggregate-bounded before in-place iteration, with no hostile
  concatenation/copy;
- every owner, item, completion, and mass-removal event carries a monotonic
  watcher epoch; stale traffic is rejected; empty, partial, full, malformed,
  and duplicate-identity replacement populations reconcile deterministically;
- generation and watcher counters refuse wrap, and failed watcher-epoch
  handoff invalidates old traffic while returning presentation to Loading;
- registry and event sink are non-copyable/non-movable; the deterministic seed
  seam reaches live counter exhaustion; item and owner capacity are bounded;
- `revalidateIntent` checks kind, exact owner generation, item presence, and
  identity equality across replacement, removal, rebase, and owner loss;
- fake transport null-first attach, different-sink reattach refusal, explicit
  detach state clearing, and destructor detach are independently observable;
- the former 579-non-blank-line registry test is decomposed into a 485-line
  suite and cohesive 218-line test-support header; wiki/ADR/harness claims now
  name only directly executed coverage.

## Exact changed paths

- `docs/wiki/adr/0032-status-notifier-exact-owner-foundation.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/shell/status-tray.md`
- `src/shell/status_notifier/include/qindaqt/shell/status_notifier/status_notifier_event_sink.h`
- `src/shell/status_notifier/include/qindaqt/shell/status_notifier/status_notifier_registry.h`
- `src/shell/status_notifier/include/qindaqt/shell/status_notifier/status_notifier_types.h`
- `src/shell/status_notifier/include/qindaqt/shell/status_notifier/status_notifier_validation.h`
- `src/shell/status_notifier/src/status_notifier_registry.cpp`
- `src/shell/status_notifier/src/status_notifier_validation.cpp`
- `tests/shell/status_notifier/status_notifier_registry_test_support.h`
- `tests/shell/status_notifier/tst_status_notifier_presentation.cpp`
- `tests/shell/status_notifier/tst_status_notifier_registry.cpp`
- `tests/shell/status_notifier/tst_status_notifier_values.cpp`

## Verification on the committed content

- Fresh dependency-light strict Debug configure: exit 0, Ninja, GCC 16.1.1,
  Qt 6.11.1; KWin plugin, shell, production shell, and host-uinput disabled;
  repository strict warnings and `CMAKE_COMPILE_WARNING_AS_ERROR=ON`.
- Serial focused build (`--parallel 1`): exit 0, 19/19 actions after the final
  source repair for the library and all three executables.
- Exact discovery selector: exactly 3 rows.
- Exact focused CTest selector: exit 0, 3/3 PASS.
- Direct complete QtTest binaries: exit 0, values 17/17, registry 23/23,
  presentation 9/9.
- Direct named hostile runs: exit 0, values 6/6, registry 8/8, presentation
  4/4 including setup/cleanup.
- `python3 tools/check-source-shape --warnings-as-errors`: exit 0, 1018 files,
  0 skips; registry test 485 non-blank lines, no threshold warning.
- `python3 tools/validate-docs`: exit 0, 65 Markdown documents plus navigation.
- MkDocs 1.6.1 `build --strict`: exit 0.
- Static hostile sweep: exit 0; no icon-list concatenation, stale Status
  Notifier ADR-0026 reference, or removed sink signature.
- `git diff --check`, cached diff check, `git show --check`, exact branch,
  exact parent, one-commit count, and post-commit clean tree: PASS.

## Bounded caveats and requested action

This remains deliberately source/unit evidence with a fake transport. It does
not claim a live session bus, watcher ownership, item property decoding,
DBusMenu revision transport, rendered panel tray, GUI/session/display/input,
or assistive-technology bridge behavior. Those are later milestones and none
was contacted during this repair.

Please assign Shannon the 2nd to rereview the exact immutable commit above, or
prefer an available independent cross-provider reviewer if one can attack the
same SHA promptly. Review the commit, not this prose; do not integrate until
that different worker posts an exact PASS verdict. The manager should perform
any later integration/conflict resolution on the integration branch.
