# Audio selected-screen repair source freeze

- Time: 2026-10-08T04:58:16+00:00
- Original fixed qualification: 4cf8202a6f63d51547924b6e2e0bbfccb9b97d6a.
- Unchanged-production regression boundary: d4fb740cb6ca333f44068716623a421a79d5ef7b. Adds actual outputSpace versus current screen diagnostic/assertion and two popup rows for bare unnamed offscreen and duplicate-name outputs. Production src equality to4cf exit0.
- Production repair resolves anchor->window()->screen(), validates current QGuiApplication membership and positive available geometry; optional nonempty hint must match. Empty/duplicate name alone never rejects the selected screen. No primary-screen/virtual-union choice and no retained raw screen.
- QML explicitly reads current Window.screen object identity before dimensions/name; current screen migration recomputes even for matching dimensions/names. Fixture checks actual window screen assignment and live popup outputSpace after migration.
- Density fixture waits boundedly for both minimum22-height controls BEFORE spacing measurement; thresholds/control assertions unchanged.
- Existing drag/wheel source assertions unchanged. Named offscreen override was not adopted.
- Static pure boundary/poisons, runtime boundary/poison, icon contract, docs531 and strict MkDocs all exit0; diffcheck0. Existing inherited source-shape failure remains as reviewed.
- Native remains STOPPED and laptop lease released; original six failures and raw receipts in laptop .cache/audio-popup-native-evidence preserved.
- Requested next gate: exact source review, then old d4fb unnamed/duplicate regression first; repaired strict20 targets PLUS only qindaqt-shell/qindaqt-shell-preview actual install-stage prerequisites, laptop32/16 Debug with invalid host buses/private HOME/XDG/software offscreen. All actual owning registry rows (now31) and normal/2x/fractional popup + normal/2x Settings captures. No compositor/unrelated target or host controls.
