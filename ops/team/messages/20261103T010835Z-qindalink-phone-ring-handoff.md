# QindaLink phone-ring handoff

Branch `link/phone-ring`, feature commit 88ecbd9c3 (plus this handoff commit).

Changed: phone/app/android/src/org/qindaqt/link/ring/{RingFeature,RingSink}.java, android/RingSinkAndroid.java,
android/AndroidStatusSource.java (signal 0-4), android/LinkService.java (name, ring registration),
engine/{DeviceInfo,EngineState}.java, AndroidManifest.xml (USE_FULL_SCREEN_INTENT, ACCESS_NOTIFICATION_POLICY),
phone/engine-tests/{RingTest,CrossImplTest}.java, run.sh, run-cross-impl.sh, protocol page (ring, signal),
architecture page, phone/README.md.

Tests: phone/engine-tests/run.sh exit 0 (RingTest 15 checks); run-cross-impl.sh exit 0 (Link1.Ring reaches the
phone's RingFeature with a fake sink, second Ring answered busy); qindaqt_link session/vectors/pairing tests pass;
all qindaqt_link_* test targets build in build/ring; build-apk.sh exit 0.

Caveats: nothing was installed on or rung on the phone; RingSinkAndroid and the signal code are compile-checked
only. The desktop does not yet send ring{stop:true} (phone accepts it). Wi-Fi/cellular signal values and DND
bypass are unverified on hardware.

Next: PM, with user approval, installs the APK and does one live ring, then checks signal in Devices().
