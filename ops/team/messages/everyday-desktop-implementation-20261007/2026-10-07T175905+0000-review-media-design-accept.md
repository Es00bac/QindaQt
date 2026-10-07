# Proposed ED-04 media design review — ACCEPT

- Candidate: aecc7c85ab6f7185f71dc4efeac9fa876ea0dd2f
- Base: 7d45d2c336e5d213c7ba10f063ab384b09f4abc8
- Verdict: ACCEPT
- Reviewer: Everyday Review Sol
- Time: 2026-10-07T17:59:05+00:00
- Scope: Proposed ADR-0350 and supporting documentation/feasibility record only

The proposal is coherent with existing owning code and explicitly names its narrow supersession of ADR-0315 export-only-Activate policy. Removable Media remains the sole UDisks/persistence/insertion/credential/formatting owner; public protocol and independently injected observation client have separate module boundaries, ownership/lifetime/threading and canonical bounded data contracts. File Manager/chooser consume observed typed owner truth instead of private storage/controller types or human status strings.

Graphical recovery is deliberate Start media support/Try again through a closed injected launcher for the existing desktop entry. Passive observation cannot bus-activate remembered mounts; a launched PID is not inventory readiness. Missing launch descriptor, startup failure/timeout, live-but-unavailable backend and old unsupported owners remain visibly distinguishable through diagnosed inventory/readback and existing helper activation. There is no automatic restart/terminal fallback. Inventory publication remains present after notification dismissal and fails closed on owner/epoch/generation loss. Mutation intents are closed and serialized with existing owner operations; credentials, formatting, force, arbitrary paths/options and preference authority do not cross this boundary. Navigation/removal success waits authoritative completion and fresh attachment readback, with no replay after uncertainty; chooser cancellation and standard portal validation remain confined.

Independent review evidence:

- Exact candidate checked out before reading; diff against exact base has documentation/own coordination only, no src/tests/features changes. Existing main.cpp Activate-only export, startup inventory publication, notification absent/dismissed behavior and UDisks operation completion-before-refresh inspected directly. Existing ADR-0315 ownership/formatting/authority reviewed.
- `mkdocs build --strict --site-dir build/ed-media-review-site`: exit 0; `tools/validate-docs`: exit 0, 515 documents/navigation; `git diff --check`: exit 0.
- Authored feasibility evidence inspected, not independently rerun: four CTests pass4/4, 30 Qt checks (10 policy/10 UDisks/6 notification/4 added discovery), zero fail/skip. Ignored CMake harness SHA256 5ba61b8677689a2b67f14b128f911540368964edbe2f7dc38475e29c49dc1243 and probe SHA256 1e04519ea817425b07ffe5e0611a08478cfe330edf1b6ad8e75c1111e52b9651 match handoff. Harness source links unchanged owning sources, uses private fixture session buses and unavailable system bus; additional absent/dismissed cases verify owner-window request/inventory retention/no mount and rejection of late dismissed actions. These receipts support owner feasibility, not proposed public-client/GUI/hardware delivery.

Caveats: public API/exporter/client/sidebar/chooser changes are unimplemented; actual graphical startup/retry, complete result protocol and bounded typed projection require the listed staged implementation and adversarial gates. Hardware/polkit/physical eject/formatting remain separate authorization/qualification. No independent compiler/private-runtime or live service action was run for this documentation review. Status remains Proposed in this exact candidate; verdict does not invent feature-ledger progress or delivered behavior.

Requested next action: manager integrates the exact reviewed design, records its explicit ADR acceptance/dispatch decision, and assigns the inventory-only protocol/exporter/client packet before consumer or action delivery. R15 recipe/archive review is separately preserved at b19c6d0a1; both requested reviews are complete. Reviewer available with no resource lease and no extra task claimed.
