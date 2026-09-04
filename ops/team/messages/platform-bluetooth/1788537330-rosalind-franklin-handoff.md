# Rosalind Franklin — Bluetooth pairing Escape repair handoff

- Timestamp: `2026-09-04T09:54:52-06:00`
- Candidate commit: `e473bbf74e9954ce064763a3daf49fcaa9eee552`
- Candidate tree: `17cbd7946ff72ecf0f9b8842f9266e78f686c298`
- Exact base: `6720a4faf325d0a66826e72813c4e8d7239b3857`
- Rejected candidate repaired: `7025a1cab90419baf07431e5880bd40ebee2afac`
  (rejected ancestor `43a7cb16d4d053b1e05ba4351d986b235676cbde`)
- Branch: `worker/bluetooth-pairing`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/bluetooth-pairing`
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/bluetooth-pairing`
  (reused configured Debug and Release trees, system-KWin initial cache)

## Outcome

Kathrin Bringmann's single P2 is repaired at the cause. Exactly one enabled
window-context Escape shortcut now exists in the production Settings host
whenever the Bluetooth route shows an active pairing prompt with a free reply
lane: `Main.qml`'s host Escape shortcut yields (is disabled) for exactly that
state, so `BluetoothPairingSection.qml`'s Escape shortcut is the sole enabled
match and delivers exactly one cancel reply regardless of which route control
holds focus. A busy reply lane or any other route keeps the host shortcut
enabled, preserving the documented route-tab focus return. Both sides read the
same existing route/host seam (`pairingPrompt.active`, `pairingReplyPending`,
`navigation.activeRouteComponent`); no new signal path, registry, or module
boundary was added.

Design choice, recorded on the owning pages: the brief's preferred
`Keys.onShortcutOverride` variant was not taken alone because Escape would
then depend on focus being inside the pairing section; the brief's own
criterion ("exactly one Escape handler must win and send the false
confirmation exactly once" while a prompt is active) holds at every focus
position only with the host-yield seam. The route's window-context shortcut
therefore stays, paired with an `AGENT-CONTRACT` on both sides.

## Changed paths

- `docs/wiki/apps/bluetooth-settings.md`
- `docs/wiki/apps/settings-center.md`
- `src/apps/settings/bluetooth/qml/BluetoothPairingSection.qml`
- `src/apps/settings_center/Main.qml`
- `tests/apps/settings/bluetooth/tst_bluetooth_window_close.cpp`

## Evidence

Negative control on the unrepaired tree (test-only working tree before the
product hunk, Debug, `QT_FATAL_WARNINGS=1`, offscreen/software,
unreachable-bus variables):

```sh
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS \
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent QT_QPA_PLATFORM=offscreen \
  QT_QUICK_BACKEND=software QT_FATAL_WARNINGS=1 \
  /home/cabewse/work_SPaC3/builds/qindaqt/bluetooth-pairing/debug/tests/apps/settings/bluetooth/qindaqt_bluetooth_window_close_tests \
  promptEscapeInRealHostSendsSingleRejection
```

Exit 1: `promptReplies: 0`, expected 1 — the new row fails on `7025a1c`'s
ambiguity. After the repair the same binary passes 9/9 (exit 0).

All rows ran under `env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS
DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent`:

- `ctest -R bluetooth`: Debug 31/31 passed, exit 0; Release 31/31, exit 0.
- `ctest -R '^qindaqt\.settings-'`: Debug 54/54 passed, exit 0; Release
  54/54, exit 0. (The settings-selector binaries were not yet built in the
  assigned build root; they were built once from this candidate before the
  run — the 30 initial "Not Run" rows were unbuilt executables, not
  failures.)
- Reviewer's warning-fatal subset
  `-R '^(qindaqt\.settings-bluetooth-(page|window-close)|qindaqt\.bluetooth-applet-(offscreen|surface))$'`:
  Debug 4/4, exit 0; Release 4/4, exit 0. The new row runs inside
  `qindaqt.settings-bluetooth-window-close`, whose registered environment
  already sets `QT_FATAL_WARNINGS=1`.
- `./tools/validate-docs`: exit 0, 140 documents.
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build
  --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/bluetooth-pairing/site`:
  exit 0.
- `./tools/check-source-shape`: exit 0.
- `git diff --check`: exit 0.
- `python3 -m json.tool`: not applicable, no JSON changed.

No nested compositor or session row, host D-Bus, hardware, radio, uinput, or
network surface was used.

## Remaining bounded caveats

- Physical BlueZ/radio interoperability, live host-bus integration, live
  AT-SPI, and nested-session screenshots remain unclaimed, as in the
  underlying candidate.
- The new row drives the prompt through the stub route model, mirroring the
  reviewer's product reproduction (real `Main.qml`, stub model); it does not
  drive a live BlueZ agent.
- The host-yield guard is Bluetooth-specific by design (the only route with
  an Escape-owning prompt today); a second Escape-owning route must extend
  the same single-owner pattern rather than add a parallel enabled shortcut.

## Requested next action

Independent exact review of `e473bbf74e9954ce064763a3daf49fcaa9eee552` by
Kathrin Bringmann (one recheck), then manager integration.
