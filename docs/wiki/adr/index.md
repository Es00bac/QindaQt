# Architecture decision records

Architecture decision records (ADRs) preserve durable choices and their
tradeoffs so future agents do not have to reconstruct them from code. The
[documentation policy](../contributing/documentation-policy.md) defines when to
create or supersede one; start from the [ADR template](template.md).

| ADR | Status | Decision |
| --- | --- | --- |
| [ADR-0001](0001-use-kwin-as-compositor-base.md) | Accepted | Use a small downstream KWin integration as the compositor base |
| [ADR-0002](0002-native-qindaqt-applet-api.md) | Accepted | Define a native, capability-declared QindaQt applet API |
| [ADR-0003](0003-docs-as-code.md) | Accepted | Maintain the project wiki and ADRs as repository source |

Numbers are never reused, including for rejected or superseded records.
