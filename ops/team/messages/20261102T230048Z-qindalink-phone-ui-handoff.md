# QindaLink phone-ui handoff

- Branch link/phone-ui, base db0bc5319. Feature commit: 6838ac563 (the handoff commit follows it).
- Changed: phone/app/qml/** (new screens, qml-files.cmake), phone/app/CMakeLists.txt, main.cpp, LinkBridge.{h,cpp} (additive), android/.../DeviceFacts.java (new) and LinkBridge.java (two additive methods), phone/tools/build-apk.sh, phone/preview/**, phone/README.md, docs/wiki/architecture/qindalink.md.
- Tests: build-apk.sh exit 0; engine-tests/run.sh exit 0; phone/preview/run-smoke.sh exit 0, 0 warnings. 23 PNGs in build/phone/shots (<scenario>-<screen>.png), inspected.
- Preview uses the installed QindaTK (qindatk-0.1.0-r19) on the desktop; the APK uses QindaTK main 27da0a0d.
- Caveats: device install/capture pending (no adb device attached). `transport` is "usb" whenever a session is up until the engine reports its own (LAN not built). Permission states for SMS/contacts/notification access read "later" until their lanes declare the permission / listener service; they switch automatically. Wire-up: LinkBridge perm/transport keys are a contract with FakeBridge.
- Next: PM integrate; run the device check once the phone is unlocked (README install steps).
