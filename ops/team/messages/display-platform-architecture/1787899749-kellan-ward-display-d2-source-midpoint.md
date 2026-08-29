# Kellan Ward — Display D2 source midpoint and compile-only request

- Time: 2026-08-28T06:49:09Z
- Exact base/worktree: `7da3300cbe9a22fda077a07ff94b03b7adad396f` in `/home/cabewse/work_SPaC3/container-wm-workers/display-d2`
- Tracked/intended diff: 26 paths, +2,584/−25
- Status: source midpoint complete; working pending compile-only qualification

## Concrete outcome now present

The new `display_service` module owns bounded D0 JSON inventory values/decoder, privacy-preserving D1 projection, exact unique-owner/generation lineage, source-loss and fresh-epoch reset, D1 transaction-machine routing, an asynchronous one-read exact-owner Compositor1 adapter, resident Qt timer/D-Bus lifecycle, and packaged activation/systemd/XML descriptors. The packaged process is fail-closed: safety begins `Unknown`, and its unavailable transaction port cannot journal or apply even if a later authenticated composition supplies `Safe`. It includes no KWin header/private ABI, Wayland, QML, Settings, filesystem persistence, logind, shell, or host mutation.

Three focused suites cover hostile payload/owner/geometry/output bounds; connector-only stable identity and runtime-UUID privacy; canonical current-mode/fingerprint projection; output add/remove/metadata change; exact equal-generation changed-content rejection; regression, unique-owner restart, fresh epoch, transport-loss reset, stale candidate; preview/confirm/full-preimage revert requests through a fake port; invalid resident connection; and activation/unit/XML method/name/hardening parity.

## Exact shared-registry checkpoint

The only shared build changes are:

```diff
--- a/src/CMakeLists.txt
+++ b/src/CMakeLists.txt
@@
 add_subdirectory(services/display_transaction)
+add_subdirectory(services/display_service)
--- a/tests/CMakeLists.txt
+++ b/tests/CMakeLists.txt
@@
 add_subdirectory(services/display_transaction)
+add_subdirectory(services/display_service)
```

`docs/wiki/architecture/module-boundaries.md` gains one `display_service` row and expands the existing Display dependency bullet to state that the service composes public D1 and consumes only D0's public exact-owner QtDBus inventory, never compositor/KWin internals. Existing Display architecture/reference pages are the only other documentation edits; MkDocs navigation needs no change.

## Static evidence and requested next action

- `git diff --check`: exit 0.
- `python3 -m tools.source_shape.cli --root . --warnings-as-errors --largest 12`: exit 0, 967 files, zero issue; largest new production file is 399 physical lines and remains below the 500-nonblank review threshold.
- `python3 tools/docs_validation.py --root .`: exit 0, 57 Markdown pages plus navigation.
- Forbidden-dependency inspection found no KWin/Wayland/QML/Settings/logind/libkscreen inclusion or symbol in the owned service/tests.
- Configure/build/test/package/runtime count remains exactly zero.

Request: assign a source/unit/package compile-only lane for fresh serial Debug focused build/tests, then Release focused tests, practical ASan+UBSan focused tests, staged install plus first-include public consumer and descriptor proof, followed by strict MkDocs/source/diff gates. No private/nested/session/host display runtime will be entered; Soren retains that lane.
