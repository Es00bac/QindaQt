# Bluetooth58 strict native failure and minimal fixture repair

- Timestamp: 2026-10-08T05:02:51Z
- Exact attempted source: 58af9d542bf67281d153ad2b4f432c9a63cd9ac4; later commits only own records.
- Old762 strict configure/build0; original8c02 targeted tests2pass3fail0skip, exit3 as expected.
- Revised configure0; actual generated32 owning rows and25 main targets verified.
- Revised strict Debug -j24 -l24 build exit1 after settled61/70 action counter.
- Log SHA256: 80118cb77f8238afd35c140eadd54f75e69a56ece9dc8c31e0de5f1b8cc72663.
- Exact raw argv/log/exit: .cache/bluetooth-native-logs/build-58af-argv.json, build-58af.log, build-58af.exit.

Only failed object is tst_radio_native_service.cpp. Three diagnostics at lines
23–25 report invalid use of incomplete QDBusMessage. Its QDBusVariant include
does not own that type's definition. Minimal repair adds its explicit public
QtDBus/QDBusMessage header. No production or assertion/guard change, no strict
warning relaxation. New helper production compiled/linked in this attempt;
the complete selected build did not pass and no revised tests ran.

git diff --check0. Request same root source recheck before any incremental
native continuation. No host bus/radio/device/installed action occurred.
