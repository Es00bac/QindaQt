# Annie Easley — Bluetooth B1 composition-proof repair handoff

- Timestamp: 2026-09-02T21:33:27-06:00
- Exact repaired product candidate: `882cc0cdbb31ee9d619c625a2856aee90c7a49b0`
- Exact tree: `47611298c1a3dd6c11027d57ecfb214d206021c5`
- Exact base and sole parent: `e251cf1fced67b9bfcae27fc9c7381e44ec21f0c`
- Rejected product ancestor: `7061dd3bf0db9c2048bfe4ec919147e12ef9563c`
- Branch/worktree: `worker/bluetooth-applet-b1` at `/home/cabewse/work_SPaC3/container-wm-workers/bluetooth-applet-b1`
- Requested next action: Kimi K2.7 and Kimi K3-256k must independently recheck exact product `882cc0cdbb31ee9d619c625a2856aee90c7a49b0`, then the Program Manager should integrate it if accepted.

## Outcome

The repaired runtime boundary restores all ten literal production-presence
contracts from base `35f2fa2` and adds the exact
`BluetoothAppletModule.BluetoothApplet {` delegate-construction token. The
compiled controller surface test is byte-unchanged. Every dependency poison
now starts from the complete copied manifest/registry/profile/QML/shell chain,
so it continues to fail for its own mutation. The new sixth runtime poison
removes the exact stock-profile Bluetooth row, Bluetooth module import, and QML
delegate, and then requires recursive failure output to contain the three
corresponding missing-token diagnostics. A generic nonzero result cannot
satisfy this control.

The owning wiki now names the eight property fields the compiled surface
actually compares and the complete method/enumerator fields. Both Bluetooth
sections replace the nonexistent adjacent "dispatcher" row with the real
`qindaqt.notification-center-applet-offscreen` row and document the restored
composition presence/poison boundary.

## Exact changed paths

Sorted product delta from exact base `e251cf1`:

```text
docs/wiki/development/testing-harness.md
docs/wiki/shell/bluetooth-applet.md
tests/shell/bluetooth_applet/check_runtime_boundary.cmake
```

No `src/` path changed.

## Debug and Release evidence

All commands used build root
`/home/cabewse/work_SPaC3/builds/qindaqt/bluetooth-applet-b1`, the prescribed
6.6.5 initial cache, strict warnings, production shell and KWin-plugin options,
and host uinput disabled. The known configure-time mixed-prefix RPATH warnings
were emitted; both configurations completed with exit 0.

- Exact prescribed Debug configure: exit 0.
- Exact prescribed Release configure: exit 0.
- `cmake --build <debug> --parallel 3 --target qindaqt_shell_bluetooth_applet qindaqt_shell_bluetooth_applet_runtime qindaqt_bluetooth_applet_presentation_tests qindaqt_bluetooth_applet_request_tests qindaqt_bluetooth_applet_controller_tests qindaqt_bluetooth_applet_qml_tests qindaqt_bluetooth_applet_surface_tests qindaqt_bluetooth_client_tests qindaqt_applet_manifest_tests qindaqt_applet_catalog_tests qindaqt_applet_instance_resolver_tests qindaqt-shell`: exit 0; incremental Ninja printed seven executed commands.
- The identical Release focused build: exit 0; incremental Ninja printed seven executed commands.
- `ctest --test-dir <debug> -R '^qindaqt\.bluetooth-applet-' --output-on-failure --no-tests=error`: exit 0, 8/8.
- The identical Release Bluetooth selector: exit 0, 8/8.
- `ctest --test-dir <debug> -R '^(qindaqt\.(bluetooth-client|applet-manifest|applet-catalog|applet-runtime-resolution|notification-center-applet-offscreen|shell-runtime-catalog))$' --output-on-failure --no-tests=error`: exit 0, 6/6.
- The identical Release adjacent selector: exit 0, 6/6.

No default whole-repository build or test suite ran.

## Direct and static evidence

- Runtime boundary without `POISON_ROOT`: exit 0, 7 files + 0 poison rejections.
- Runtime boundary with an assigned-build-root poison path and explicit `BLUETOOTH_RUNTIME_POLICY_SKIP_POISON=ON`: exit 0, 7 + 0.
- Full runtime boundary with an assigned-build-root poison path: exit 0, 7 + 6.
- Full pure boundary with an assigned-build-root poison path: exit 0, 5 + 4.
- `./tools/validate-docs`: exit 0, 117 Markdown documents; rerun after final wording, same result.
- Pinned `mkdocs build --strict --site-dir <ROOT>/site`: exit 0; rerun after final wording, same result.
- `./tools/check-source-shape`: exit 0, 1,780 files and zero allowlisted skips; rerun after final wording, same result. It reported only the two pre-existing unrelated 500/539-line decomposition warnings.
- `git diff --check`: exit 0 before and after final wording; `git diff HEAD^ HEAD --check`: exit 0 after the product commit.
- `git merge-base --is-ancestor 7061dd3 882cc0c`: exit 0.
- Exact SHA/tree/sole-parent and sorted three-path product inventory match the values above.
- `git diff -- tests/shell/bluetooth_applet/tst_bluetooth_applet_surface.cpp`: empty; the compiled surface gate is unchanged.
- No JSON changed; the per-changed-JSON parse gate is not applicable.

## Bounded caveats

This candidate proves the static production composition presence chain, the
mutation-sensitive removal control, the existing compiled/offscreen controller
surface, fake/public-client behavior, and installed package boundary. It makes
no claim for BluezQt, live BlueZ, a host or private session bus, radio or
hardware discovery/connection, hotplug, suspend/resume, pairing, trust, keys,
Agent1 prompts, Bluetooth audio routing, AT-SPI bridge behavior, network,
uinput, nested compositor, multi-user policy, or physical qualification. None
of those prohibited paths ran.
