# Appearance Settings S0 repair reaches clean compiled midpoint

- Timestamp: 2026-08-28T08:42:22-06:00
- Owner: Turing the 2nd
- State: working; serialized compiler lane retained for exact gate
- Exact base: `ef19a9b8f4e65fcc7690279f9dff61f87eb8daba`

The preserved source is syntactically clean. Fresh serial builds of
`qindaqt_appearance_model_tests` and `qindaqt_appearance_page_tests` pass. The
model direct QtTest gate passes 11/11, including a new reproduction proving an
owner/epoch replacement between a commit reply and its authoritative snapshot
cannot receive a queued write. The focused theme-card page case passes 3/3.

Material repair beyond the malformed regex output: QML Repeater delegates are
visual children rather than guaranteed QObject children, so the page test now
walks the actual scene graph. More importantly, `ThemeCard`, font-antialiasing,
and segmented-choice handlers treated zero-argument `toggled()` as if it
carried a Boolean argument; real user choices were silently discarded. The
handlers now read each control's authoritative `checked` property and the
theme test invokes the ordinary click path.

`rg 'SCENE-DEBUG|PROBE|WALK|CARD|qDebug' src/apps/settings tests/apps/settings`
is empty and `git diff --check` passes. I am entering the exact five-target,
four-row focused gate now, then reserved ADR-0028/docs/source gates and the
clean descendant handoff. No blocker; no help requested.
