# Elan Frost claim: status-notifier S0 exact-review repair (Shannon FAIL 0/3/3/1)

- **Timestamp:** 2026-08-28T16:20:00Z
- **Status:** claimed; repair active
- **Exact base under repair:** preserved candidate
  `78725a95920880930acb55ca0f322c72b4148f17` (tree
  `fc52f584223d010bc4f3325de037ee14e974af42`), verified clean; the reviewed
  commit is preserved and only a non-amended descendant will be created
- **Branch:** `worker/system-tray-s0-repair-elan`
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/system-tray-s0-repair-elan`

I own one bounded outcome: repair every P1, P2, and P3 finding in Shannon the 2nd's exact FAIL verdict:
1. Canonical bounded D-Bus unique-name grammar (multi-element, alphanum + underscore + hyphen, digits allowed first) and canonical root object path `/` with exact 255-byte, boundary, invalid character, and multi-element cases, without QtDBus.
2. In-place validation of icon lists with early aggregate budget checking before any allocation/concatenation, with boundary and over-limit tests.
3. Monotonic watcher epochs stamped on population and item events, stale traffic rejection, and reconciliation of replacement baselines (unseen items pruned rather than republished).
4. Deletion of copy/move authority on mutable registry and sink implementation, compile-time type-trait assertions, and bounded generation exhaustion test seam.
5. Complete public request-intent revalidation operation with same-key replacement, removal, owner rebase, and owner loss tests.
6. Non-vacuous coverage for registry menu over-count/bad-parent, null-first attach, different-sink reattach, destructor-detach, and generation exhaustion.
7. Precise doc/ADR/testing claims reflecting actual behavior and tests.

## Path ownership

- `src/shell/status_notifier/**`
- `tests/shell/status_notifier/**`
- `src/CMakeLists.txt` and `tests/CMakeLists.txt` (existing smallest additive wiring)
- `docs/wiki/shell/status-tray.md`
- `docs/wiki/development/testing-harness.md`
- `docs/wiki/adr/0032-status-notifier-exact-owner-foundation.md`

## Constraint compliance

No contact with host D-Bus/session tray, desktop, Wayland compositor, input devices, user configuration, or hardware. All tests are pure QtCore-only / private offscreen test suites.
