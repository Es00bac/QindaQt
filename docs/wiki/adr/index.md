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
| [ADR-0004](0004-process-local-hybrid-topology.md) | Accepted; chrome portion superseded | Keep Hybrid topology process-local and compose member plus shared chrome |
| [ADR-0005](0005-scene-resident-hybrid-chrome.md) | Accepted | Render shared Hybrid chrome as a member-anchored scene item |
| [ADR-0006](0006-profile-global-applet-identity.md) | Accepted | Make applet instance identity global within a layout profile |
| [ADR-0007](0007-layer-shell-panel-surfaces.md) | Accepted | Use LayerShellQt behind a QindaQt panel-surface boundary |
| [ADR-0008](0008-lean-notification-service.md) | Accepted | Own a bounded QtDBus notification service without Plasma runtime |
| [ADR-0009](0009-use-kglobalaccel-for-shell-shortcuts.md) | Accepted | Use KGlobalAccel for user-remappable shell-wide shortcuts |

Numbers are never reused, including for rejected or superseded records.
