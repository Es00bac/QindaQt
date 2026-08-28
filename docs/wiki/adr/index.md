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
| [ADR-0010](0010-inject-shell-notification-interruption-policy.md) | Accepted; lifetime/UI clause superseded | Inject notification interruption policy on the shell side |
| [ADR-0011](0011-gate-notifications-on-authenticated-lock-state.md) | Accepted | Gate full notification presentation on owner/PID-authenticated KWin lock state |
| [ADR-0012](0012-persist-notification-quieting-through-settings1.md) | Accepted | Persist notification quieting through an activatable Settings1 authority |
| [ADR-0013](0013-own-qst1-semantic-tokens.md) | Accepted | Own QST-1 derivation and isolate optional Kirigami reuse behind adapters |
| [ADR-0014](0014-confine-wireplumber-to-glib-worker.md) | Accepted | Confine libwireplumber/GObject ownership to a dedicated GLib worker |
| [ADR-0015](0015-qualify-function-before-resource-refinement.md) | Accepted | Qualify the isolated virtual desktop before tightening its initial resource ceiling |
| [ADR-0016](0016-display1-transaction-authority.md) | Accepted | Make Display1 the QindaQt display-transaction authority while KWin owns live state and restore |
| [ADR-0017](0017-persistent-output-identity.md) | Accepted | Derive privacy-preserving persistent output identities with explicit ambiguity |
| [ADR-0019](0019-restart-the-production-shell-once.md) | Accepted | Restart the production shell once per compositor session |
| [ADR-0020](0020-authenticate-private-live-evidence.md) | Accepted | Authenticate a private, read-only live-session evidence boundary |
| [ADR-0021](0021-isolate-controls-visual-rows.md) | Accepted | Isolate every Controls visual row in its own process |
| [ADR-0022](0022-keep-text-documents-local-and-atomic.md) | Accepted | Keep Text Editor documents local, optimistic, and atomically persisted |
| [ADR-0023](0023-split-power-authority-across-service-and-shell.md) | Accepted | Split platform power observation from shell-owned session-action authority |
| [ADR-0024](0024-route-brightness-through-power1.md) | Accepted | Route fail-closed internal brightness through a Power1 provider |
| [ADR-0025](0025-arbitrate-session-bound-power1-activation.md) | Accepted | Arbitrate session-bound Power1 activation without reciprocal takeover |
| [ADR-0026](0026-compose-appearance-settings-through-settings1.md) | Accepted | Compose the Appearance settings route through Settings1 and QST-1 |

Numbers are never reused, including for rejected or superseded records.
