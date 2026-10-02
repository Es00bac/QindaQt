# Independent native shortcut static-import SOURCE ACCEPT

- Timestamp:2026-10-02T20:08:25Z
- Exact candidate:a6964310dbfb3a11df06532a32059f4ab9e92ac2.
- Decision:SOURCE ACCEPT; no blocking finding.
- Actual diff:one guard in src/main_wayland.cpp, KWIN_BUILD_GLOBALSHORTCUTS && !KWIN_NATIVE_SHORTCUTS around Q_IMPORT_PLUGIN(KGlobalAccelImpl).

Matches existing CMake dependency condition, IntegrationTestFramework import and
legacy global-shortcut portions. Non-native builds retain the plugin; native
builds already omit its link and use their owned implementation. Input filters,
admission, identity, production policy and all DPMS assertions unchanged.
Read-only git diffcheck0; no reviewer compilation/runtime. Root retains failed
link receipt and owns coherent seven-target incremental rebuild. Prior5a21
include repair acceptance remains separate. Worker awaits exact artifact/private
lease for one approved software/virtual native DisplayPower authority gate;
no hardware/GPU-renderer or full supported supervisor qualification claimed.
