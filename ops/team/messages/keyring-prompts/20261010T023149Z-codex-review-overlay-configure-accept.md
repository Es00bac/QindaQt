# ACCEPT: `e3b515d20661f9fd76ffd0159b840d54fc67235b`

Exact R28 overlay descendant of accepted `80373263d87373010f93a33c0aacdce1d3ed6028`. Bounded repair review checks only the two configure flags and target closure.

`QINDAQT_BUILD_PRODUCTION_SHELL=ON` creates the existing `QindaQt::AuthenticationOverlay` static target in `src/CMakeLists.txt:34`; `src/services/keyring/CMakeLists.txt` only creates the native prompt when that target exists. The root CMake contract requires `QINDAQT_BUILD_SHELL=ON` when ProductionShell is enabled. The correction therefore admits the required target using its existing LayerShellQt/QindaTK dependency boundary.

The exact diff changes only those flags and adds two explanatory comments. `BUILD_TESTING=OFF` and `QINDAQT_BUILD_KWIN_PLUGIN=OFF` remain; `src_compile` still names only `qindaqt-keyring` and `qindaqt-keyring-prompt`. `src_install` still stages the verified signed R25 image and replaces exactly those two executables. Additional configured shell targets are not compilation or installation targets in this recipe. Accepted source `a9c9768`, actual source archive/Manifest pairing, R25 verifier/ledger, masks, runtime dependencies and compositor ABI are unchanged.

No remaining configure/source/provenance blocker. No unchanged build or test was repeated. The causal package retry uses the corrected target admission; signed artifact and two-file image parity remain manager acceptance gates. Source ACCEPT remains in its separate exact verdict. Reviewer remains available for a concrete image-parity check if requested.
