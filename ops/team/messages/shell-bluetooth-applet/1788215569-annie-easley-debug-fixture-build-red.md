# Annie Easley Bluetooth B1 strict fixture build red

- Timestamp: 2026-08-31T16:32:49-06:00
- Worker: Annie Easley
- Prior portability repair: `75cd78e04d30a8e77b271f4a58f3fa812df807e4`
- Build root: `/tmp/qindaqt-bluetooth-b1-debug-5714b2f`
- Status: stopped at second first-red boundary; exact test-only repair applied.

The exact failed pure target rebuilt 4/4. The resumed serialized Debug graph
then stopped at displayed action 278/293 while compiling
`tst_bluetooth_applet_presentation.cpp`. The fallback-label fixture omitted
`Adapter::name` and `Device::name`; GCC 15.3 strict warnings reported two
`-Werror=missing-field-initializers` failures. Production shell, the Bluetooth
composition/controller, compiled QML, applet resolver, and public-client
objects had compiled. No CTest row ran.

The owned test repair explicitly sets both names to `{}`, preserving the exact
empty-name fallback behavior the fixture was designed to exercise. The next
action is to rebuild only the presentation test target, then resume the same
root. No private D-Bus, display, BlueZ, host Bluetooth, hardware, network, or
input resource was used.
