# Shared ComboBox popup repair handoff

- Timestamp: 2026-09-06T10:19:01-06:00
- From: finish-design-luna
- Base: `e351323ea8e63504184eeb190bde3c8401883bd8`
- Scope: `src/controls/qml/ComboBox.qml`, focused Controls behavior test, Controls wiki contract

## Outcome

The shared tokenized ComboBox now places its styled `T.ItemDelegate` on the
root `T.ComboBox.delegate`, so the popup ListView's `control.delegateModel`
renders the same tokenized rows. The popup keeps Qt Quick Controls selection,
keyboard, and type-ahead behavior, and adds an `AsNeeded` vertical scrollbar
for long catalogs. The behavior suite now opens the real popup by pointer,
checks the token-derived raised surface, selects the second row by pointer, and
verifies the popup closes with `Porcelain` selected.

## Verification

- `TMPDIR=$PWD/build/design-tmp cmake --build build/design --target qindaqt_controls_behavior_tests -j2` — exit 0.
- `QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_LINUX_ACCESSIBILITY_ALWAYS_ON=1 ./build/design/tests/controls/qindaqt_controls_behavior_tests opensAndSelectsTokenizedComboPopup` — 3 repeated runs passed.
- `ctest --test-dir build/design --output-on-failure --no-tests=error --parallel 1 -R '^qindaqt\.(controls-behavior|controls-source-policy|controls-installed-import)$'` — 3/3 passed.
- `python3 tools/validate-docs` — exit 0, 176 documents/navigation validated.
- `/home/cabewse/work_SPaC3/container-wm/.cache/handbook-docs-venv/bin/mkdocs build --strict --site-dir build/design-docs` — exit 0.
- `git diff --check` — exit 0.

Requested next action: cherry-pick this exact commit for the Appearance
consumer and rerun its selection/page gates.
