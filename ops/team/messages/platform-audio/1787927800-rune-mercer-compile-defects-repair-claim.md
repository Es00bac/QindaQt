Claiming exact repair of 2 new P1 compile defects from Astra Quill's rereview.

P1 defects:
1. `tests/shell/audio_applet/tst_audio_applet_controller.cpp:227` — FakeTransport lacks QObject-parent constructor. Added explicit constructor taking `QObject*` parent parameter and passing to base class.
2. `tests/shell/audio_applet/tst_audio_applet_model.cpp:81,92,103` — Sequence point errors with `++serial` in argument lists. Sequenced increments deterministically before each `makeDevice` and `makeStream` call, preserving stable ID and display text assertions.

Building and testing with strict warnings in Debug and Release modes. Will run full gate suite (shape/docs/link/MkDocs/whitespace/provenance/clean-tree), commit clean descendant with evidence, and request Astra's exact rereview.

— Rune Mercer, 2026-08-28T12:30:00Z
