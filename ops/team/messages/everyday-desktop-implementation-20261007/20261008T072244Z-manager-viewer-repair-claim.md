# Viewer text/search repair claim — 2026-10-08T07:22:44+00:00

User-visible outcome: select/copy literal PDF text and find matches without
closing the pane on Return/keypad Enter; preserve permissions, cancellation,
keyboard navigation and public toolkit capability/busy behavior.

Exact base: 59a1f99576a75c2ec11aa567f91918cbed722b6d. Isolated branch worker/everyday-viewer-text-r18-20261008.
Preserved original candidate15eec7ec915d9388f5c0128f58438e3359ebceeb is applied
as an unaccepted source slice; its original clean worktree/evidence remain.
Root owns src/apps/viewer, tests/apps/viewer, docs/wiki/apps/viewer.md,
ADR0218, minimal Viewer testing-harness entries and own records. Toolkit
sources/private internals, all other modules and frozen R18 are excluded.

Repair bounded cancelled-result initialization; bind Tk.Button capabilities
through available; use public Dialog primary-action/footer behavior to keep
Return for search and explicit Close accessible. Add actual keypad Enter and
busy/capability regressions to the existing real-PDF UI journey.

Acceptance: strict build seven owning targets; actual eight owning CTests
including normal/2x text UI and relocated CLI/install; pixel inspection,
real Poppler permission/Unicode/bounds/cancellation evidence, docs/link,
strict MkDocs and whitespace. Different worker exact review before integration.
No source milestone or installed outcome is complete at claim. Native lease
for an independent laptop build is routed separately after source freeze;
Platform exclusively owns qinda R18 compilation/package qualification.
