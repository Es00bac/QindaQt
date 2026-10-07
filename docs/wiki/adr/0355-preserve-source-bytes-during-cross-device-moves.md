# ADR-0355: Preserve source bytes during cross-device moves

- **Status:** Proposed
- **Date:** 2026-10-07
- **Owners:** File Manager
- **Supersedes:** Only ADR-0064's cross-device Move refusal, when this proposal is implemented and accepted; Accepted ADR-0357's failed-copy preservation invariant remains
- **Implementation:** Contract and bounded design proof only; current production still refuses cross-device Move

## Context

A successful copy is insufficient authority to delete its source. The current tree copier checks each file before reporting progress, but a subsequent write to a copied child need not change its parent directory's identity. Checking a pathname immediately before unlink also leaves a race and cannot stop writes through an already-open descriptor.

The independently reproduced failed-copy cleanup defect is now repaired by Accepted [ADR-0357](0357-preserve-failed-copy-output-without-cleanup-authority.md), source223c8a7ae: both automatic deletion paths are removed, and unchanged old-source sentinel failures now preserve data. This proposal builds on that preservation invariant; it does not reintroduce a cleanup capability from pathname observations.

## Decision

Extend the existing single-worker File Manager mutation coordinator with a staged cross-device Move. Keep mount authority, network moves, home/per-volume Trash and shell services in their existing owners. The detailed contract is [File mutation recovery](../architecture/file-mutation-recovery.md).

The operation creates private, exclusive staging and recovery locations, records a bounded durable intent, copies through pinned descriptors, verifies the complete destination and source manifest, then publishes the destination with no replacement. Publication readback must bind the captured entry to the owned stage and full manifest; destination identity, mount and content are checked again before retirement and before completed status. Any replacement in those syscall windows produces conflict/uncertainty and preserves all candidates. Only after that publication is durably recorded may it remove the original source name, by a same-device no-replace rename of the entire source entry into its private recovery directory. It never recursively unlinks that retained source as part of Move.

Retaining the original tree preserves its named descendant inodes, including writes through surviving descriptors after retirement. This does not preserve data another process has independently unlinked before retirement, anonymous/deleted descriptor contents, arbitrary historical versions, or bytes lost by failed storage. No stat sequence is presented as a lock against other writers. A changed tree produces an explicit conflict/recovery outcome rather than an unqualified completed move.

The retained source consumes space on its original volume. The UI must disclose that fact before cross-device dispatch and provide an inspectable recovery receipt and conflict-safe restore. No time-based expiry, quota eviction, startup deletion, automatic destructive retry or reuse of the home Trash format is allowed. Permanently releasing retained bytes is a separately confirmed, separately reviewed outcome; this slice does not expose it. If retention cannot be created or bounded accounting cannot admit another operation, the source stays in place and Move refuses.

Persistent phase is evidence for inspection, never restart authority. Every restored process starts with no in-flight grant. Recovery requires a deliberate new request, fresh filesystem admission and no-replace destination checks. A lost/replaced mount, cancellation, late source change, failed durability barrier or ambiguous publication/retirement preserves all operation-owned candidates and yields a typed receipt naming what is known and what requires inspection.

## Boundaries and compatibility

The application-private backend stays synchronous on its existing worker thread. Policy values, descriptor/platform operations, bounded recovery persistence and presentation are separate collaborators. Each callback is operation-ID fenced; controllers refresh observed changes even when the terminal result is not success. Batch receipts retain each item's outcome rather than hiding completed or retained work behind the final error.

Same-device Move remains its existing rename path. Cross-device undo becomes an explicit conflict-safe recovery restore, never a silent second cross-volume move or destination deletion. Existing Copy behavior changes only where needed to prevent deletion of foreign output and to report leftover owned partial output truthfully. Symbolic links, special files and nested mounts remain refused by the first cross-device implementation; hard-linked regular entries may be copied independently, with retained source identity preserving the original links.

## Acceptance and scope

The Python design proof exercises bounded phase/interleaving and preservation invariants only. It is not production filesystem, crash, durability, mount or lifecycle qualification. Before implementation, a different worker reviews this exact proposal and its failure model.

Production gates must reproduce the existing-destination deletion on the old helper; prove the repair preserves both foreign files and trees; exercise real temporary filesystem descriptors, every journal/publication/retirement failure boundary, child late writes, source replacement and restore collision; and run the existing local mutation/controller/Trash regression suites. Actual distinct filesystem devices are required before claiming a cross-device integration pass. Mock device IDs alone do not establish it. Installed UI and removable-volume journeys remain separate delivery gates.

See [ADR-0064](0064-confine-file-mutation-to-identity-checked-local-authority.md), [File Manager](../apps/file-manager.md), and [module boundaries](../architecture/module-boundaries.md).

## Bounded contract review — October 7

Exact design/model02114070c is independently reviewed; its33 in-memory preservation cases pass, including the previously failing combined stage/destination/source/mount replacements. The unchanged model also passes33 rows on the integrated contract tree. This accepts the proposal as a reviewed implementation packet, not production behavior or Linux durability/identity authority. ADR status remains Proposed until qualified implementation is integrated.
