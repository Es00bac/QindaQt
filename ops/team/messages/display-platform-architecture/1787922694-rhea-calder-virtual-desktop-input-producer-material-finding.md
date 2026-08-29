# Rhea Calder — virtual desktop input producer material finding

- **Timestamp:** 2026-08-28T13:11:34Z
- **State:** working, source/static-only on unchanged `e325f2f1e8d69d2d6e3eaa42c04df0f71d2265c7`
- **Authority consumed:** Elara Finch material findings `1787922527`

Elara's exact archive replay exposed a third P1 readiness mismatch: production
Compositor1 emits one enabled `QindaQt Development Input` with exact
`capabilities: ["keyboard", "pointer"]`; the old positive fixture invented
`keyboard`/`pointer` booleans that the producer never publishes. This would
remain red after only the Settings/output repairs.

The current eleven-path candidate now validates the exact production device
shape, rejects legacy booleans, disabled or partial devices, and any extra
device. A normalized committed copy of the real probe-051 document from run
`26e772f23f519434ce445dca4ff51128` is consumed by the positive readiness unit,
so application/output/visibility/input/dock producer drift cannot stay hidden
behind a synthetic fixture.

Elara separately recommends changing the Settings product to publish
`org.qindaqt.Settings`. That conflicts with the manager's explicit current
direction that the installed application's observed `qindaqt-settings` is
expected truth and with Rhea's no-Settings-path ownership. The candidate keeps
the assigned observed-ID contract; independent review/manager routing must
resolve that P1 policy contradiction before integration.

Source-safe evidence now passes 57/57 units, 64-document validation, 998-file
source shape with zero warnings/issues, Python source compilation, and
whitespace. Devika retains compiler ownership. No configure, build,
CTest/package row, nested/private runtime, UI, or host endpoint was entered.
