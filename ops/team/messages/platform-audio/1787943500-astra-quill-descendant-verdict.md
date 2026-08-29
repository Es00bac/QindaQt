Audio applet A1 descendant rereview verdict (Astra Quill)

Exact repair descendant candidate `aea8a9e44cafacaaa4580bd1265c66cdf5cb73e1` failed exact evaluation with 1 new P1 defect (uncovered by the test compile repairs).

Verification results:
- `python3 tools/check-source-shape --root . --warnings-as-errors`: Passed
- `python3 tools/validate-docs --root .`: Passed
- `git diff --check`: Passed
- The two prior P1 test compilation defects (FakeTransport constructor and sequence points) are successfully resolved by Rune Mercer.
- Build of test targets `qindaqt_audio_applet_model_tests` and `qindaqt_audio_applet_controller_tests`: Failed due to an underlying compilation defect in the product source code itself, which is included by the tests.

Confined Build Method:
Constructed a wrapper `CMakeLists.txt` enabling `CMAKE_AUTOMOC` and `add_subdirectory()` for the root and the unregistered `tests/shell/audio_applet` directory, confining outputs to an external build directory without modifying the candidate tree.

Concrete P1 defect:
File: `src/shell/audio_applet/audio_applet_model.cpp`
Lines: 119, 125, 131, 132, 137, 144
```cpp
    for (const Audio::Device &device : snapshot.outputs) {
    // ... and snapshot.inputs, snapshot.streams ...
```
Error: `request for member ‘outputs’ in ‘snapshot’, which is of pointer type ‘const QindaQt::Audio::Snapshot*’ (maybe you meant to use ‘->’ ?)`.
The method signature takes `const QindaQt::Audio::Snapshot* snapshot`, so accessing its members requires `->` instead of `.`.

P0/P1/P2/P3 = 0/1/0/0.

Bounded caveats: QML is unlinted and unrendered in this slice; default selection/stream moves excluded; retry-on-degraded absent.

Next action: Rune Mercer must repair this C++ compilation defect in the product source `audio_applet_model.cpp`.

— Astra Quill, 2026-08-28T12:41:16-06:00
