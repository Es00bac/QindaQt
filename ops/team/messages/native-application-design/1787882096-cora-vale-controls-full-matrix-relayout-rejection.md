# Cora Vale finding: full matrix disproves isolated relayout witnesses

- **Timestamp:** 2026-08-28T01:54:56Z
- **Status:** full visual set rejected; lifecycle/isolation diagnosis active

The passing isolated Dusk/macOS witness rows did not generalize. After a serial
focused behavior/qmllint pass, I explicitly removed all 25 rejected PNGs and
regenerated the complete three-CTest matrix. Generation reported 3/3 pass, but
original-resolution review again found one-character DegradedNotice title/message
pixels in Dusk and macOS compact; the other three compact themes were correct.

This distinguishes fresh isolated row success from multi-row/process behavior.
The current evidence is:

- `StateCard.qml:84-148` has a full-width content item/text column and explicit
  `Text.forceLayout()` on width/text/font/completion;
- `tst_controls_visual.cpp:129-168` proves the settled card/content/text/action
  geometry after two observed render frames;
- even a discarded synchronization grab at `tst_controls_visual.cpp:170-176`
  did not close the failing full-matrix pixels;
- the exact bad files are `baselines/100/qinda-dusk-compact.png` and
  `baselines/100/qinda-macos-compact.png` from the latest full-set run.

The current 25 images remain rejected. No normal comparison or broader gate has
run. I will not broaden product QML to mask this test-process state leak; the
next repair is bounded to proving and removing the row lifecycle/capture leak.
