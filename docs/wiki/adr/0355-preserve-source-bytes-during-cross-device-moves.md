# ADR-0355: Preserve source bytes during cross-device moves

- **Status:** Accepted
- **Date:** 2026-10-07
- **Owners:** File Manager
- **Supersedes:** Only ADR-0064's cross-device Move refusal; Accepted ADR-0357's failed-copy preservation invariant remains
- **Implementation:** Storage/backend/controller integrated from independently accepted5bfd. Manager strict Release11/11,183Qtchecks plus12pure controls and recovery QML4/4 pass. Installed graphical and physical ED05 journeys remain open.

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

## October 8 source implementation boundary

The owning candidate selectively reuses preserved411/c398 values, strict
record/store, descriptor mount admission and unfinished manifest files. It adds
identity-bound private recovery storage, a locked128-operation immutable locator
catalog under QStandardPaths::StateLocation/file-manager/move-recovery, phased
strict-metadata descriptor copying, full readback verification and no-replace
publication/whole-entry source retention. No recursive source unlink, output
cleanup, retention expiry or mount/helper authority is added.

The existing serialized controller carries per-item receipts and fences queued
progress/results by request generation. Its graphical recovery panel discloses
retained source space before moves, inspects on startup without replay, renders
literal locations and confirms restore of a catalog UUID. Restore uses fresh
private-directory, current payload and target-parent admission with no-replace
rename, preserving the published destination and current surviving-writer bytes.
Corrupt/partial journals remain read-only and refuse automatic restore;
unavailable/replaced storage stays discoverable.

Source-only boundary/model/parser/fake presentation checks are separate from
production qualification. Authored real distinct-device, every-journal-barrier,
metadata/space, cancellation, replacement/disconnection, late child write and
child-exit/restart fixtures now have bounded author native evidence below.
ADR0355 remains Proposed and ED05 open until required independent/integrated and
installed gates are satisfied.

The October8 source successor adds the proposed best-effort return of an
unexpected captured entry: durable Required write-ahead, stable capture and
parent/name admission, no-replace rename, parent synchronization and readback.
Unsafe admission or a collision preserves all candidates; even a verified return
does not claim the independently displaced selected original restored. Status
remains Proposed pending independent native review and integration.

Author native qualification on October8 uses actual distinct-device disposable
storage, strict Debug/Release and poisoned subject buses/private XDG. Original
full Debug CTest was10/11 with182Qtpassed/1failed; its only failed record fixture
was repaired without changing production, then warm record-only Debug passed1/1
and32Qtchecks. Other original Debug passes remain retained rather than rerun.
Fresh full Release passes11/11 with183Qtchecks and12 pure runner controls,
zero failures/skips/blacklists. All30 phase/barrier rows and four actual owned
child-exit/wait/restart cases pass in original Debug and fresh Release. Current
journal/catalog version guards, growing/truncated source bounds, replacement,
cancellation and current retained-byte restore controls pass. Actual wrapper
normal0/early-error7/timeout124 controls were qualified separately and retained.
This is author evidence awaiting exact independent native review/integration;
prior-version old/fixed comparison, installed graphical journeys and physical
disconnect/power-loss remain separate. No full ED05 completion is claimed.

## Manager integration qualification — October 8

The preceding author snapshots preserve their original handoff state. Exact
5bfd source and native evidence received independent acceptancecc44; the manager
applied only45 product/test/owning-doc paths and retained all prior failures.
Strict integrated Release configuration/build exits0 (0.833/24.064seconds).
The actual registry has11 rows and full serial CTest passes11/11 in4.187seconds:
183Qtchecks plus12pure controls, zero failures/skips/blacklists. The actual
recovery QML component fixture passes4/4 with fatal warnings and offscreen
software rendering (fake controller; no installed journey is inferred).
The capped owning unit exits0/29.295seconds; five direct parent witnesses verify
eight-CPU quota,12GiB/no swap/tasks256,half physical affinity and nice10. Its
post-exit default properties are not substituted for those live witnesses.
Accepted ADR0355 governs the integrated storage contract. No startup replay,
automatic cleanup/release or physical power-loss guarantee is introduced.
Full ED05 remains open for installed GUI and physical journeys; prior-version
comparison is separately recorded as unrun.
