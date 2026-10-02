# Independent native repeat delivery source acceptance

ACCEPT exact fork `55d1f2738723316fae4686468757aef66ce7590b`, e9base, isolated qinda review WT/branch `pf-native-shortcut-repeat-sol-20261002` / `review/pf-native-shortcut-repeat-sol-20261002`. Clean source; diff --check exit0. Three files only.

Native signal mapping now reports initial/admitted repeated press through Activated and release through Deactivated. Existing KF6 Pressed/Repeated/Released mapping and component objects unchanged. InputDispatcher passes Repeated through Registry::dispatch repeat=true; registry allows it only when binding.repeat is opted in. Existing endpoint admission/active/desktop-action guards and ownership/persistence/limits unchanged. Native introspection already declares only Activated/Deactivated; desktop QtShortcutTransport subscribes to those exact owner-bound signals and defaults repeat=false, so no signal signature or unconditional repeat admission is introduced.

New regression registers an actual native repeat binding over a separate private bus connection, asserts native signal order/count/payload/timestamp, and disconnects. Direct notifyShortcut calls intentionally test wire forwarding; they do not qualify physical/core repeat input. The unchanged actual repeatedNativeAction row remains required on coherent artifacts. README accurately records policy versus wire division. No blocking source finding; no reviewer compiler/runtime or shared cache/source changes. Input holds current private runtime; parent owns focused build and later actual gate.

- `qindaqt/README.md` SHA256 `8ffcc9866590701134ee415f442d8c4cb1a3b7341c7526f27a05f91fd056ddee`
- `qindaqt/shortcuts/compatibility_endpoint.cpp` SHA256 `11f75f9ea0b9bb13a9b2973c24c93f62c1af71cfd9a3598678204876e3e1bdcb`
- `qindaqt/shortcuts/tests/test_shortcut_native_endpoint.cpp` SHA256 `6952646019d747e9324cc28fd0e8dc7071319bdf3e2a054b825083d4c3325980`

Preserved DPMS actual all11/13Qt PASS handoff cdf9948b53bec3482399951b9b9192c4e294868e remains separate and unchanged.
