# Text Editor S2 midpoint — Beatrice Shilling

- Time: 2026-09-03T05:05:05-06:00
- Exact base: `b2f515986150b1acfe82e2807a78a731a58a94a2`.
- Product state: bounded 32-document collection, independent tab views, expanded AppShell catalog and shortcuts, bounded in-window find/replace, Settings1 restore policy, and atomic paths-only state are implemented and documented in ADR-0064.
- Material findings: the initial tab insertion exposed action-construction ordering and bulk `QTextDocument` replacement exposed its terminal paragraph marker; both production defects now have focused regressions. Settings1's normal post-commit `Authenticating` transition retains its complete snapshot and must not be treated as owner loss; actual owner replacement clears the snapshot and still fails closed.
- Current evidence: strict Debug product plus all focused executable targets compile. Individual repaired rows pass; the complete 15-row Debug selector, Release profile, installed proof, and static documentation gates remain before handoff.
