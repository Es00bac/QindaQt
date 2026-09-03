# Annie Easley — Bluetooth B1 compiled-surface repair handoff

- Timestamp: 2026-09-02T20:58:39-06:00
- Exact candidate: `7061dd3bf0db9c2048bfe4ec919147e12ef9563c`
- Exact tree: `4ff0f7984cb1fc2bf4083fdb6e77a8b85028a0a8`
- Exact base and sole parent: `35f2fa20881437fc3ef9d85ce399dc68e12ed1d3`
- Rejected product ancestor: `af78bce23c4f57d8085d9cd6b27f8b4eeecb26bb`
- Branch/worktree: `worker/bluetooth-applet-b1` at `/home/cabewse/work_SPaC3/container-wm-workers/bluetooth-applet-b1`
- Requested next action: independent exact review by Kimi K2.7 and Kimi K3-256k, then manager integration.

## Outcome

The candidate deletes the regex-derived positive controller surface and all six
surface poison variants from `check_runtime_boundary.cmake`. That script now
owns only its seven-file inventory, explicit include allowlist, forbidden-symbol
policy, and five independent include/forbidden-symbol poisons. Clean and
explicit-skip modes truthfully report zero poison executions.

`qindaqt.bluetooth-applet-surface` is the compiled authority. It walks
`BluetoothAppletController::staticMetaObject` from all three class offsets and
compares complete ordered literal tables for property names/types/read-write-
reset/notify/constant/final attributes; normalized method signatures, return
types, Signal/Slot/Method type, access, and revision; and enumerator metadata,
keys, and values. Its empty test base matches an empty contract, while a derived
surface adds a token-pasted property, public slot, signal, and enum and must be
reported in every expanded category by the same comparison function. The same
row constructs the production `BluetoothApplet` through an offscreen
`QQmlEngine`, reflects controller and plain-`QObject` surfaces, subtracts only
the inherited baseline, and requires the remaining QML-visible property and
method names to equal the literal contract. This makes the token-paste,
splice/comment, cross-header alias, and public-slot findings moot without a
production source or behavior change.

## Exact changed paths

Sorted product delta from the exact base:

```text
docs/wiki/development/testing-harness.md
docs/wiki/shell/bluetooth-applet.md
tests/shell/bluetooth_applet/CMakeLists.txt
tests/shell/bluetooth_applet/check_runtime_boundary.cmake
tests/shell/bluetooth_applet/tst_bluetooth_applet_surface.cpp
```

## Executable evidence

All commands used the assigned build root
`/home/cabewse/work_SPaC3/builds/qindaqt/bluetooth-applet-b1`, host-uinput
disabled, and the exact 6.6.5 initial cache. No default whole-repository build
or session test ran.

- Exact prescribed Debug configure: exit 0; GCC 15.3.0, strict warnings enabled.
- Focused Debug build of both Bluetooth applet libraries, all five Bluetooth
  test executables, public client, manifest/catalog/resolver, and
  `qindaqt-shell`: exit 0, 319/319 reported actions after the new surface
  target's initial dependency build.
- Exact prescribed Release configure: exit 0; GCC 15.3.0, strict warnings enabled.
- Identical focused Release build: exit 0, 383/383 reported actions.
- Post-candidate identical Debug and Release build replay: exit 0, `ninja: no
  work to do` in both roots.
- `ctest --test-dir <debug> -R '^qindaqt\.bluetooth-applet-' --output-on-failure --no-tests=error`:
  exit 0, 8/8.
- The identical Release selector: exit 0, 8/8.
- Debug adjacent selector
  `^(qindaqt\.(bluetooth-client|applet-manifest|applet-catalog|applet-runtime-resolution|notification-center-applet-offscreen|shell-runtime-catalog))$`:
  exit 0, 6/6.
- The identical Release adjacent selector: exit 0, 6/6.
- The new surface row was also isolated after its final token-paste negative
  control in Debug and Release: exit 0, 1/1 per profile.

Two development-only red cycles preceded the green builds and are not hidden:
the first strict surface compile rejected ambiguous overloaded formatter
arguments; after disambiguation, the first isolated row reported moc's canonical
`qulonglong` type names and a collision with QQuickItem's final `baseline`
property. Both were confined to the new test and repaired before the complete
profile builds and selectors above.

## Static and documentation evidence

- Direct runtime boundary without `POISON_ROOT`: exit 0, seven files and zero
  poison rejections.
- Direct runtime boundary with a build-root poison path plus explicit skip:
  exit 0, seven files and zero poison rejections.
- Direct full runtime boundary: exit 0, seven files and five poison rejections.
- Direct pure boundary: exit 0, five files and four poison rejections.
- `./tools/validate-docs`: exit 0, 117 Markdown documents and navigation.
- Pinned `mkdocs build --strict --site-dir <ROOT>/site`: exit 0.
- `./tools/check-source-shape`: exit 0, 1,780 files, zero allowlisted skips;
  only the two pre-existing unrelated 500/539-line decomposition warnings.
  The new test is 364 non-blank lines.
- `git diff --check` and `git diff HEAD^ HEAD --check`: exit 0.
- Exact SHA/tree/parent and sorted five-path product inventory: match the
  values above.
- No JSON changed, so the per-changed-JSON parse gate is not applicable.
- A non-required `clang-format --dry-run --Werror` diagnostic returned nonzero
  because this repository has no `.clang-format` and the tool's default LLVM
  style disagrees with the established QindaQt source style. It is not a
  project acceptance gate; strict compile, source-shape, and diff checks pass.

## Bounded caveats

This candidate proves the compiled C++/moc and offscreen QML controller surface,
existing fake/public-client behavior, package relocation, and textual dependency
policy. It deliberately claims no BluezQt adapter, live BlueZ, host/session
D-Bus, radio or hardware discovery/connection, hotplug, suspend/resume, pairing,
trust, keys, Agent1 prompt UX, Bluetooth audio routing, AT-SPI bridge, network,
uinput, nested compositor, multi-user policy, or physical qualification.

No reviewer P3 is deliberately left open within the replaced positive-surface
mechanism: token pasting, splice/comment and trailing-whitespace spelling,
cross-header macro aliases, and public slots all collapse to the same compiled
meta-object comparison. Independent exact rereview should attack candidate
`7061dd3bf0db9c2048bfe4ec919147e12ef9563c` itself; after Kimi K2.7 and Kimi
K3-256k accept, the Program Manager should integrate that immutable product
commit.
