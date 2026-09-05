# Glossary

| Term | Meaning in QindaQt |
| --- | --- |
| Applet | Manifest-declared panel component admitted through host/capability policy. |
| AppShell | Shared application lifecycle/action/integration boundary, not the desktop shell. |
| Client | Compositor-managed application toplevel. |
| Container | Movable grouping with ordered pages and recursive split trees. |
| Member | Client occupying a tile in a container. |
| Page | One tab-selectable split-tree layout inside a container. |
| Split / tile | Internal layout branch / leaf holding a member. |
| Hybrid | The model and interaction architecture combining independent and grouped windows. |
| Chrome | Window/container frame, title bars, tabs, and their paint/hit layout. |
| Profile | Declarative workflow and panel arrangement. |
| Theme | Versioned appearance inputs, separate from layout and confirmed settings. |
| QST-1 | QindaQt semantic design-token contract. |
| Controls | Reusable token-driven QML primitives. |
| Exact owner | The current unique service identity rather than only a reusable bus name. |
| Epoch / revision | Protocol-specific lineage and ordering values used to reject obsolete state. |
| Intent | Requested operation; not evidence of successful authoritative mutation. |
| Lease | Bounded ownership/admission for an ongoing activity, such as discovery. |
| Fail closed | Deny exposure or action when required trust/validation cannot be established. |
| Logical pixel | Desktop coordinate unit before output-scale conversion. |
| Exclusive area | Panel-reserved work area that constrains ordinary window placement. |
| Nested session | A test desktop running under an isolated parent compositor. |
| Rootless XWayland | Legacy X application windows integrated into the Wayland desktop. |
| Portal | Standard desktop mediation interface; backend implementation scope is explicit. |
| PSS | Proportional set size: process memory accounting that apportions shared pages. |
| ADR | Versioned architecture decision record with rationale and consequences. |
| Stopping point | Exact implemented boundary and evidence attached to a feature's maturity. |
| Candidate / integration | Reviewable isolated commit / accepted commit incorporated on the manager branch. |

For normative definitions see [containers](../architecture/window-containers.md),
[module boundaries](../architecture/module-boundaries.md), [team-board
maturity](../contributing/team-board.md), and the [protocol catalog](catalog/reading.md).
Return to the [handbook index](index.md).
