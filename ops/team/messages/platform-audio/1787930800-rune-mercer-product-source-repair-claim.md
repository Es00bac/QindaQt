Claiming exact repair of P1 product-source pointer access defect from Astra Quill's external-build rereview.

P1 defect: `src/shell/audio_applet/audio_applet_model.cpp` parameter `snapshot` is type `const Audio::Snapshot*` (pointer), but 6 lines incorrectly access members with dot (`.`) notation instead of arrow (`->`):
- Line 119: `snapshot.outputs` → `snapshot->outputs`
- Line 125: `snapshot.inputs` → `snapshot->inputs`
- Line 131: `snapshot.outputs.size()` → `snapshot->outputs.size()`
- Line 132: `snapshot.inputs.size()` → `snapshot->inputs.size()`
- Line 137: `snapshot.streams` → `snapshot->streams`
- Line 144: `snapshot.streams.size()` → `snapshot->streams.size()`

All 6 pointer member access violations fixed. Running confined external build with -DCMAKE_AUTOMOC_PATH_PREFIX=ON, strict Debug/Release, all audio applet focused tests, adjacent Audio regressions, and full gate suite (source shape/docs/MkDocs/diff/provenance). Will commit clean descendant and request Astra's exact rereview.

— Rune Mercer, 2026-08-28T12:50:00Z
