# Native DPMS construction-time fixture repair

Candidate `11b45f5f201e0048947228072eb7c912edd5d91d`, exact base `a6964310dbfb3a11df06532a32059f4ab9e92ac2`, pushed fork hub branch `worker/pf-native-display-power-runtime-20261002`. Four approved source paths only:

- `autotests/integration/kwin_wayland_test.cpp` SHA256 `0e5ac9d7673f28a9535c271488d4fb5fe4159b50b5d2b7c4acfcb309bd117f34`
- `autotests/integration/kwin_wayland_test.h` SHA256 `1bba741f091927c0ec8132b446f20d450c2c8b324d96a3b8bab9772fc826957c`
- `qindaqt/display-power/README.md` SHA256 `4bd003333ac8217734c942724c10f508deb269405328d66154c74a7484862613`
- `qindaqt/display-power/tests/native_display_power_test.cpp` SHA256 `87bb174f225c47016ef6632f140793de5b57e56f82f5c9322c8d010d971df298`

Additional constructor overload accepts owning output backend while the original three-argument symbol remains, delegating null so original VirtualBackend/DRM initialization is selected at the original point. Only the DPMS explicit main passes the fixture backend, copying normal test-main environment/QPA setup exactly. The live/one-shot backend guard, production authority/model/output bytes and all eleven behavior bodies remain unchanged.

Light static checks: git diff --check exit0; byte comparison of initialization after backend selection and all behavior bodies PASS. No C++ compiler/runtime executed on this source. Both first actual failures/logs and their20hash receipts remain archived and immutable. Request root independent exact source review and warm tiny-target rebuild, then one private software/virtual DPMS replay. Physical DRM/DPMS, GPU-renderer and complete native-exclusive positive-supervisor qualification remain separate.
