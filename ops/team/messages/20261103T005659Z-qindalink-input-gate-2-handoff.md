# QindaLink input-gate-2 handoff (follow-up to the input-macros lane)

Branch `link/input-gate-2` from `bb2e7c777`. Still no real portal prompt in any test (fakes on private buses); no adb
`input`, `screencap`, `screenrecord` or `uiautomator` was used; preview grabs were offscreen only.

## Changes

1. **Truthful gate reasons.** `InputGate` now has `reason()` (`Open`, `Locked`, `Inactive`, `Unverified`); `open()` is derived.
   `input.state` and the `begin` error message use `locked`, `inactive` or `unverified`. `NativeInputGate` reports `Locked`
   only when the authenticated monitor says Locking or Locked; every missing prerequisite (no Session1 owner, no
   `org.qindaqt.KWin` owner, no Wayland display, socket peer unreadable, attachment refused or revoked, monitor not started,
   Unknown receipt) is `Unverified`. It logs one line per transition naming the missing fact (`qindaqt-link-service: input gate: ...`,
   no content). `Inactive` is defined for a future source; the monitor cannot report it today.
2. **Phone.** `InputFeature` parses `inactive` and `unverified`. The banner says "Can't confirm the desktop is unlocked /
   QindaLink needs a newer QindaQt session to accept phone input." for `unverified`, with "Ask again". Home tiles
   (Touchpad, Keyboard, Macros) show state: Ready, Desktop must approve, Not approved on the desktop, Can't confirm unlocked,
   Desktop is locked, Desktop session not active, Another phone is in use, Not available on the desktop, Turned off on the desktop,
   Not connected; Vault keeps "Coming soon". Tiles reflect the input state last seen (refreshed only while a controller screen is open).
3. Docs: protocol page (`unverified`, `inactive`, "Gate reasons are truthful").

## Evidence

- Built every `qindaqt_link_*test(s)` target (20) plus `qindaqt-link-service` in `build/input2`; `ctest -R qindaqt.link-` 20/20.
  New case `gateReasonsAreReportedTruthfully` (unverified begin, then open, locked, unverified, inactive, active transitions).
- `phone/engine-tests/run.sh` OK (`InputTest` 66 checks). Preview smoke 0 warnings; 44 PNGs grabbed offscreen and inspected
  (`ready-touchpad-unverified`, `ready-home`). `tools/validate-docs` and `mkdocs build --strict` pass. APK not rebuilt.

## When the gate opens on a real QindaQt session (from reading main; for the PM to verify after the desktop update)

All on the **session bus** of the user. The gate (`src/services/link_service/app/native_input_gate.cpp`) is `Open` only when ALL hold:

1. `org.qindaqt.Session1` has an owner. It is registered by `qindaqt-session` (`src/session_supervisor/src/session_service.cpp`),
   once the supervisor has started its resident services. The gate retries 6 times (0.25 to 8 s) and on every owner change.
2. `org.qindaqt.KWin` has an owner. `CompositorNames::service` is owned by the QindaQt **KWin fork**. The stock-KWin plugin
   (`src/compositor/kwin/qindaqtkwinplugin.cpp`) registers `org.qindaqt.Compositor`, a different name, so a desktop running
   only that plugin keeps the gate `Unverified`. Check with `busctl --user list | grep -E 'qindaqt.(Session1|KWin)'`.
3. `$XDG_RUNTIME_DIR` and `$WAYLAND_DISPLAY` are set in the link service environment, and the socket's kernel peer PID is readable.
4. `CompositorAttachment::attach(Session1 owner, socket, {KWin owner, that PID})` succeeds and stays live.
5. The native lock monitor (`NativeLockStateMonitor` over `QtNativeLockTransport`, compositor path `/org/qindaqt/KWin/NativeLock`)
   gets an authenticated receipt whose state is Unlocked, with owner and PID matching the attachment.

Expected journal lines (`journalctl --user -u qindaqt-link-service`): `input gate: unverified: org.qindaqt.Session1 has no owner...`
or `...org.qindaqt.KWin has no owner...` while 1 or 2 fail; `unverified: waiting for a native lock receipt` then
`open: native lock receipt says unlocked`. A locked screen logs `closed: native lock receipt says locked`.
Observed on the user's r47 desktop: neither name is published, so `Unverified` is the correct result there.
