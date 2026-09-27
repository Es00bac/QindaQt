# Independent shell retention review — ACCEPT

Exact candidate: `8414e435456f4ad6f27a03cd8c6fcc0383ad40df`.
Compared against accepted predecessor `7d2a1601`; isolated review checkout `../container-wm-shell-retention-review`. Findings P0/P1/P2/P3: **0/0/0/0**.

Reviewed product changes and ADR-0278. The row adapter removes obsolete identities before insert/move/data updates, so retained persistent indexes and delegates follow identity while receiving fresh row payloads. Task keys distinguish members; dock kind-prefixed IDs are unique under the existing validated dock contract. Groups deliberately retain their first-member identity without a persistence schema change. GUI-only presentation ownership and public QML dependency match the documented module boundary.

The insertion fade runs only at delegate construction, reduced motion stops it at full opacity, and retained revisions do not replay it. Preview selection follows task/member identity and closes on removal. SettingsClient emits ownerChanged immediately on replacement even if state remains Authenticating; the new direct subscription revokes retained launcher values on that transition. Retention requires a previously confirmed same-owner snapshot and never admits writes until Ready; malformed refresh and loss clear values.

Direct evidence:
- Implementer checkout `../container-wm-performance-worker` is clean at the exact candidate (HEAD and status checked before test execution).
- Independently ran its compiled `ctest --test-dir build/performance -R '^qindaqt\.(launcher-persistence|task-list-keyed-row-model|task-list-applet-dock-qml|desktop-controls-offscreen-dock|desktop-controls-dock-retained-delegates)$' --output-on-failure --no-tests=error`: exit 0, **5/5** (3.00 seconds).
- Inspected test implementations: 64 delegates over 1,000 changed snapshots preserve object count/identity; real compiled dock/task delegates retain object, focus and animation state through updates/insertion/reorder/removal; current row revisions and identity-following previews checked; launcher refresh refuses writes and revokes on same-state owner replacement, bus loss and malformed snapshot.
- Review checkout `python3 tools/docs_validation.py`: exit 0, 417 documents.
- Review checkout `mkdocs build --strict`: exit 0.

No product edits were made. Tests are deterministic/offscreen presentation and fake/private transport evidence, not physical GPU pacing or long-session memory qualification. Manager owns native combined-tree build/package and both-host deployment. Next action: integrate the exact accepted candidate, rerun combined gates, and route any concrete integration regression back to its implementer. Reviewer available for bounded reproduction/recheck help.
