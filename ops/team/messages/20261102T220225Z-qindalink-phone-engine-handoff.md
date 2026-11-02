# QindaLink phone-engine handoff (M1c)

- Branch: link/phone-engine (base f6c27bc90). Candidate commit: see the final line of the lane report (HEAD of the branch).
- Changed paths: phone/app/** (CMake, Qt app, QML, Java engine, manifest), phone/engine-tests/**, phone/tools/{build-apk.sh,fake-desktop.py}, phone/README.md, docs/wiki/architecture/qindalink.md (phone rows + maturity line).

## Evidence
- `phone/engine-tests/run.sh`: exit 0. Vectors: decode=30 stream=1 encode=9 pairing=3 (60 checks, 0 fail); loopback session test: pair -> code equal -> pin stored -> status -> ring answered `unsupported` -> pong, OK.
- `phone/tools/build-apk.sh`: exit 0, produces build/phone/QindaLink-debug.apk (arm64-v8a, minSdk 31, target/compile 36, signed with build/phone/debug.keystore).
- `adb -s R3GL40ABQXM install --user 0 -r`: Success. POST_NOTIFICATIONS granted. adb reverse rule removed afterwards (list empty); app force-stopped.

## NOT done: live pairing
The phone was locked (isKeyguardShowing=true) for the whole session. `am start` launches the activity, but it stays stopped behind the keyguard so the Qt UI never ran and the foreground service (started from the UI bridge) never started. No unlock was attempted. Missing evidence: on-device Keystore/TLS handshake, pairing transcript with fake-desktop.py, status frame received, pairing-screen screencap. fake-desktop.py is only syntax-checked (py_compile), never run against a TLS client.

## Caveats and risks to check on first live run
- Keystore signing under TLS 1.3 (digests SHA256+NONE set); handshake is unverified on device.
- fake-desktop.py trusts the phone via `adb run-as ... files/identity-cert.pem` (debug build only).
- Service starts only when the activity runs; no boot/auto-start yet.
- Signal strength is null; only `status` is advertised; `ring` -> error unsupported.
- QML UI is plain QtQuick, polled over JNI every 500 ms.

## Requested next action
Unlock the phone, then follow phone/README.md "Live check" (install is already done; start the activity, run fake-desktop.py --serial R3GL40ABQXM --auto-accept after `adb reverse tcp:47160 tcp:47199`, tap Pair then Accept, record transcript and screencap), and remove the reverse rule.

## Repair round (after review of 97fe02c22; merged main 24e8ce047)
- Fixed: locale-independent code and JSON escapes (test under ar-SA); strict 64-lowercase-hex for c/nd (else bye pair-rejected); bounded event queue plus writer thread with bounded outbox and 10 s write-stall watchdog (test: non-reading peer ends the session in ~1.5 s); no READY when the pin cannot be persisted (bye error, pin cleared; test); durable pin write (fsync); nesting limit 64 (container levels), `ping\n` type, ids 1.0/1e0/-0 verified by the 36 vectors; status now callback-driven (cached snapshot rebuilt on battery/default-network callbacks, 2 s limit and immediate charging/plugged kept); Keystore key without PURPOSE_VERIFY, TEE kept (AGENT-NOTE: StrongBox unmeasured).
- `phone/engine-tests/run.sh` exit 0: vectors decode=36 stream=1 encode=10 pairing=3 (68 checks), loopback session OK, repair tests OK.
- Cross-implementation: `phone/engine-tests/run-cross-impl.sh .../qindalink-desktop-core/build/desktop-core/src/services/link_service/qindaqt-link-service` exit 0: TLS 1.3, hello, pairing codes equal, ConfirmPairing, phone pinned desktop, Devices() shows ready + battery 42. Not committed in the integration worktree, as instructed.
- Still unverified on hardware: Keystore TLS handshake, Android callbacks, live pairing (phone unplugged). The APK was not rebuilt in this round; run build-apk.sh before the next device session.
