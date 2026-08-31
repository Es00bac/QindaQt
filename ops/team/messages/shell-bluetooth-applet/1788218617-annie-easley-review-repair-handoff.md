# Annie Easley — Bluetooth B1 review-repair exact handoff

- Timestamp: 2026-08-31T17:23:37-06:00
- Exact repaired candidate: `0eb14565ecbd2e96cfbef70482e1c8542276c35a`
- Exact tree: `0559bf1f781bd90fc7bffe053283023f78e7b99b`
- Sole parent: `11e2396574b015233ed6c730fbe5ed097519d7af`
- Rejected candidate ancestor: `ecadc745fdea1e22cbbcfcbcbab1738507231b8c`
- Integrated-main ancestor: `74da46345c7a5094d45c756ad8b23ca87591fcd3`
- Branch/worktree: `worker/bluetooth-applet-b1` at `/home/cabewse/work_SPaC3/container-wm-workers/bluetooth-applet-b1`
- Worktree/process state at freeze: clean; no compiler, CMake, Ninja, CTest, KWin, Weston, shell, or Bluetooth test process survives beyond the inspection shell.
- Requested action: independent exact-descendant rereview before any manager integration.

## Blocking findings repaired

1. A non-success automatic `ReleaseDiscovery` now consumes the current
   close/shutdown intent. Exactly one request is submitted; the lease and typed
   failure/uncertainty feedback remain. Only a later explicit Stop, a new
   open/close transition, final shutdown, or authoritative lease-ending truth
   may move the lifecycle forward. No completion is replayed.
2. Adapter/device names that equal a canonical Bluetooth address after trimming
   use the existing ordinal/class fallback. Labels and accessibility text never
   receive the address-shaped name, and production code does not access the raw
   address field.
3. The pure boundary now permits only the exact actual headers used by its five
   files and independently rejects public-client, persistence, filesystem, and
   adjacent-network mutations.
4. The runtime boundary now permits only the exact actual headers used by its
   C++ files, positively compares the complete controller `Q_PROPERTY` and
   `Q_INVOKABLE` surface, and independently rejects service, renamed pairing,
   direct address access, persistence, file, and standard-path mutations.
5. The installed-package test leaves the existing relative RUNPATH policy
   unchanged, clears ambient loader variables, resolves the executable's
   runtime dependencies, and requires the KF6 SONAME to map to the exact
   copied artifact in the disposable stage.

## Exact product paths

The final product repair commit changes exactly these ten paths relative to its
sole parent:

```text
M docs/wiki/development/testing-harness.md
M docs/wiki/shell/bluetooth-applet.md
M src/shell/bluetooth_applet/src/bluetooth_applet_controller.cpp
M src/shell/bluetooth_applet/src/bluetooth_applet_controller.h
M src/shell/bluetooth_applet/src/bluetooth_applet_presentation.cpp
M tests/shell/bluetooth_applet/check_boundary.cmake
M tests/shell/bluetooth_applet/check_runtime_boundary.cmake
M tests/shell/bluetooth_applet/run_installed_bluetooth_applet.cmake
M tests/shell/bluetooth_applet/tst_bluetooth_applet_controller.cpp
M tests/shell/bluetooth_applet/tst_bluetooth_applet_presentation.cpp
```

Relative to rejected `ecadc745fdea1e22cbbcfcbcbab1738507231b8c`, the
complete 19-path descendant additionally contains only Annie's prior handoff,
three repair claims, three verification updates, loader-path update, and worker
record under `ops/team/**`. Manager-owned queues, `docs/HANDOFF.md`,
`docs/TASK_LIST.md`, and `ops/team/features.json` are unchanged.

## Exact executable evidence

- Strict Debug root `/tmp/qindaqt-bluetooth-b1-debug-5714b2f`:
  repaired presentation/controller graph rebuilt serially with final displayed
  16/16 actions; exact presentation/controller selector passed 2/2; full
  `^qindaqt\.bluetooth-applet-` passed 7/7.
- Strict Release root `/tmp/qindaqt-bluetooth-b1-release-cedb13d`:
  identical repaired graph rebuilt serially with final displayed 16/16 actions;
  exact presentation/controller selector passed 2/2; full B1 passed 7/7.
- After loader-path hardening, the exact installed-package row passed 1/1 in
  each profile, then the full B1 selector passed 7/7 again in each profile.
- The Debug resolver authenticated
  `/tmp/qindaqt-bluetooth-b1-debug-5714b2f/tests/shell/bluetooth_applet/installed-bluetooth-applet/lib64/libKF6GlobalAccel.so.6`.
- The Release resolver authenticated
  `/tmp/qindaqt-bluetooth-b1-release-cedb13d/tests/shell/bluetooth_applet/installed-bluetooth-applet/lib64/libKF6GlobalAccel.so.6`.

The controller regression has Failed and Uncertain rows. Both prove preserved
feedback, a held lease, no third submission after completion or same-authority
discovering truth, and then separately prove explicit later close retry or
authoritative non-discovering truth. The presentation regression proves ordinal
adapter and class device fallbacks plus non-address accessibility text.

## Exact static and provenance evidence

- Direct pure boundary: five files, exact actual-header allowlist, four
  independent poison rejections; exit 0.
- Direct runtime boundary: seven files, positive complete QML surface, exact
  actual-header allowlist, six independent poison rejections; exit 0.
- JSON parse: Bluetooth manifest, stock profile, and feature ledger 3/3.
- `tools/validate-docs`: 117 Markdown documents/navigation; exit 0.
- Pinned MkDocs strict build: exit 0 in 1.26 seconds.
- `python3 tools/check-source-shape --largest 20`: 1,779 files; exit 0. The
  repaired controller is 487 nonblank lines, below threshold; only the two
  unrelated pre-existing 500/539 warnings remain.
- `git diff --check`, cached diff, conflict-marker scan, rejected-candidate and
  manager-main ancestry, exact product/descendant path inventories,
  prohibited-manager-path audit, final clean status, and process audit pass.

## Bounded remainder and nonclaims

The K3 premise that the executable lacked relative install RUNPATH was not
reproduced: both generated installs already use
`$ORIGIN:$ORIGIN/../lib64`. This repair adds the missing exact staged resolution
assertion and does not add a redundant target property.

The following nonblocking reviewer observations were not expanded into this
P2 repair: whether successful mutations must always advance beyond an equal
revision rather than follow the public protocol's current `>=` rule; an unused
pending-release presentation key/label; broader semantic token-scan evasion
beyond the now-positive controller surface and exact includes; composition
destruction-order wording; convergence feedback replacement; and additional
compiled-QML accessibility depth. They require separate contract/scope review
rather than incidental changes here.

Evidence remains deterministic public-client/fake-transport, offscreen,
compiled-QML, source-poison, and relocated-package coverage. It does not claim
production BluezQt, live BlueZ, host or physical Bluetooth, pairing/trust/keys,
Agent1, audio correlation/routing, AT-SPI bridge behavior, compositor runtime,
hardware, or physical qualification. No private bus/runtime, nested
compositor, BlueZ, host Bluetooth, hardware, network, or input lane was used.

Please have independent reviewers recheck exact immutable descendant
`0eb14565ecbd2e96cfbef70482e1c8542276c35a`, especially the two no-replay data
rows, address-shaped-name fallback, all ten independent poison mutations, and
the exact staged KF6 dependency resolution. Return any blocking reproduction
to this worktree; otherwise route the accepted descendant to manager
integration without rebasing or cherry-picking its preserved merge lineage.
