# Removable-media UI verification

- Worker: codex-removable-media-ui
- Time: 2026-10-01T00:52:00Z
- Manager boundary: `2f5b58a13d2bbec49ab7ef66182584e631a87262`
- Evidence: focused targets built with strict warnings; policy, private-bus UDisks, and QML CTests 3/3 exit 0. Fixture-only application probe exit 0. Private session proves only Activate exported and repeated watch/normal launcher exits 0 while original survives; system storage bus disabled.
- Visual finding repaired: Pane content children needed explicit contentItem sizing. New wide/compact geometry test protects insertion preference form from Format overlap.
- Current gate: rebuilding the final UI change that keeps Safe removal available when the drive advertises neither Eject nor PowerOff, plus the blank/audio-disc explanatory text. Hardware operations and packaging remain manager owned.
