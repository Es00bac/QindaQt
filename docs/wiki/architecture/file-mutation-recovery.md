# File mutation recovery

**Implemented ED-05 storage/controller contract with independent and integrated native qualification; installed graphical and physical journeys remain pending.** The independently reviewed failed-copy preservation repair is integrated separately under [Accepted ADR-0357](../adr/0357-preserve-failed-copy-output-without-cleanup-authority.md); it supplies no cleanup or recovery authority. [ADR-0355](../adr/0355-preserve-source-bytes-during-cross-device-moves.md) proposes a narrow replacement for [ADR-0064](../adr/0064-confine-file-mutation-to-identity-checked-local-authority.md)'s cross-device refusal. [File Manager](../apps/file-manager.md) continues to own its existing mutation coordinator; no new service, mount broker or parallel transfer queue is introduced.

## What a completed move means

The destination contains a verified copy of the admitted source snapshot. Its name was published without replacing another entry, and required filesystem durability barriers completed. The original source entry is absent from its original name and is retained, with its original inode tree, in a private recovery location on the source volume. The receipt explicitly says **Moved; source retained for recovery** and reports retained space. It does not claim the source volume gained free space.

A later writer can change a retained inode. Retention preserves those changed bytes for inspection; it does not magically update the published destination. Final manifest mismatch, unknown mount identity or a lost durability acknowledgement yields a conflict/uncertain receipt, not plain success. No guarantee covers independently deleted entries, anonymous/deleted open descriptors, arbitrary prior versions, hostile kernel/storage behavior, or external changes after observation. A filesystem snapshot or mandatory writer exclusion is not assumed.

## Ownership and bounded values

The current GUI-thread controller owns one request at a time and one worker task. The worker owns an operation UUID, pinned source/destination parent descriptors, private staging/recovery descriptors and immutable admitted facts until terminal return. No descriptor or mutable filesystem object crosses into QML.

A pure policy collaborator operates on owning values: operation ID, request generation, phase, source/destination entry identities, mount observations, manifest digest, cancellation generation, observed effects and recovery disposition. The platform collaborator alone uses no-follow descriptor-relative opens, exclusive creation, no-replace rename, enumeration, stat and durability barriers. A recovery store owns the schema, journal validation and atomic durable record replacement. The controller consumes typed receipts; text is bounded presentation, never recovery authority.

All collaborators are constructor-visible and worker-thread confined. Progress is bounded by operation ID and request generation, never a bare busy flag. Cancellation is a thread-safe flag polled between bounded chunks, entries and phase transitions; a syscall already underway is not preemptible. Expected errors return values. Injected failure hooks exist only in tests and cannot substitute fake device IDs for real cross-device qualification.

Limits: at most20,000 tree entries, depth128, manifest16MiB, individual path4096 encoded bytes and receipt diagnostic512 characters. Journals use a versioned strict schema with duplicate/unknown required field rejection, bounded integers and exact operation identity. At most128 retained operation records per configured recovery catalog are admitted; reaching the limit refuses new cross-device work without evicting anything. Retained bytes have no automatic deletion quota. Free-space estimates are advisory; ENOSPC at every write/fsync boundary is tested.

## Private locations and mount evidence

Create an unpredictable operation directory exclusively beneath the admitted source parent, on the source filesystem, with mode0700 and current-UID ownership. It contains the versioned operation record and eventually one retained payload. Create a separate exclusive mode0700 staging directory beneath the destination parent; its payload is inaccessible through its final destination name until publication. All reserved names include the operation UUID; existing names are collisions, never reusable storage.

Private directories and records are opened with no-follow traversal and revalidated by pinned identity, owner and restrictive mode. Lack of private-directory semantics or durable directory operations is a visible refusal before destructive progress. Neither a path prefix nor an existing matching name proves ownership. The small per-user File Manager recovery index lives under its XDG state directory, records discoverable operation locations and phases, and never grants filesystem authority. A durable source-side record exists before destination publication so index-write failure cannot orphan authority. Corrupt records are quarantined for read-only inspection, never executed.

Live Linux mount evidence includes descriptor-observed mount identity and filesystem/device identity, plus exact revalidation of source/destination names through their admitted parents. Device number alone is insufficient: same-path mount replacement must fail the current operation's fence. Nested mount crossings in the source tree are refused. Mount IDs and journaled device/inode values are observations, not stable restart capabilities; after restart or mount loss every action needs fresh admission. The backend observes filesystem identity; it never imports a removable helper's private tokens or controls mounts.

## State and effects

| Phase | Durable/observed effects | Cancellation or failure |
| --- | --- | --- |
| Admitted | Listing/source and parent preconditions checked; no output owned | Refuse with source unchanged |
| Prepared | Private directories exclusively owned; intent durable; source untouched | Preserve source; remove only demonstrably owned empty staging or report retained partial |
| Copying | Descriptor-pinned source manifest and private partial destination | Preserve source; cleanup only owned identities, otherwise retain and report |
| Verified | Every regular destination read back and content-verified; size/type/mode/time policy checked; directory entries and source manifest revalidated; files and directories synced | Preserve source and owned stage; no retirement permission |
| Published | No-replace destination rename; destination parent synced; publication journal durable | Source remains; cancellation reports destination completed/source retained, never rolls back by deleting destination |
| Retiring | Write-ahead retirement intent durable; exact source tree renamed no-replace into recovery | Never unlink either source candidate; observe original/recovery names and report conflict or uncertainty if any identity/barrier fails |
| Retained | Recovery payload and source parent synced; complete retained manifest checked; final record durable | Completed-with-retention only when all fences hold; otherwise explicit recovery required |
| Restarted | No live operation ownership restored | Inspect records and filesystem only; no copy, rename, unlink, rollback or retry |

Publication must bind the actual captured destination entry to the operation-owned staged identity and complete verified manifest. Revalidate the stage immediately before no-replace rename and the published entry through the pinned destination parent immediately afterward. A replacement inside that syscall window can cause a foreign entry to be published; post-rename readback must classify conflict/uncertainty, preserve the foreign entry and source, and never grant retirement permission. No stat-then-rename sequence is described as atomic identity-conditional publication.

Immediately before source retirement and again after it, before completed status, revalidate destination parent/mount incarnation, published entry identity and the complete content/metadata manifest. A destination replacement or write after publication cannot borrow the earlier verification. Before retirement it leaves the source at its original name; after retirement it leaves the whole source in recovery. Both keep any published/foreign destination intact and require inspection. Readback still describes an observed instant: retention protects source bytes if a destination writer acts later.

The manifest binds relative entry names, type, device/inode, mount observation, size, modification/change metadata and verified content digest for regular files. Directories bind their complete admitted child set. Copy reading and digest comparison are bounded; every destination file is read back, rather than accepting byte counts or the writer's digest alone. Required metadata preservation failures are explicit, never ignored. Missing supported facilities or unsupported source kinds refuse before source retirement.

Retirement is a single rename of the whole top-level source entry. It is not a loop deleting children. If the name changes in the interval between the last check and rename, inspect the captured entry after rename. If it is the wrong identity, attempt no-replace restoration only while the operation still owns that exact entry and the original parent/name remains admissible. A collision or failed restoration leaves the captured entry retained and emits an explicit conflict receipt. Never discard an unexpected captured entry. Independently establish which entry was captured and whether that exact entry remains in recovery, even when destination revalidation also fails. A foreign captured entry is reported as unexpected entry retained/restored with original source unconfirmed; it must never be labeled original-source retained. Recovery-entry replacement is unknown placement, not retained-source success. Rollback checks current mount admission and an absent original name again and cannot overwrite a name recreated during the syscall window. The independently moved/deleted original is not claimed preserved.

A descendant write after destination verification is detected by final retained-tree verification when observed; whether detected or occurring later, its named inode remains retained. A new descendant or replaced child captured by retirement stays in that retained tree. Destination content may represent an earlier admitted snapshot, so changed/uncertain work is never labeled fully synchronized.

## Cleanup, restart and restore

Cleanup requires a current live operation and unchanged admitted mount/parent incarnation, not merely a remembered creation flag. Loss of either admission preserves the stage. Cleanup then requires proof that this operation exclusively created the candidate, retains its descriptor/identity and still owns its entry. Recursive cleanup additionally requires per-descendant creation ownership and an unchanged complete child set; owning the root directory never licenses deleting a newly inserted foreign child. A failed exclusive create conveys no ownership. Replacing an owned output name with a foreign entry must stop cleanup. The initial implementation must retain a nonempty partial stage whenever it cannot exclude an entry-replacement race during cleanup; stat-then-unlink is not an atomic conditional deletion primitive. It may conservatively retain all nonempty failed stages. Truthful leftover reporting is preferable to deletion by pathname. The model's atomic cleanup transition is an idealized policy operation, not evidence that Linux provides such a syscall.

Cleanup is limited to unpublished destination staging. Source recovery payloads and published destinations are never automatically removed. A committed destination is not rollback scratch, including cancellation, final source conflict, receipt persistence failure and restart. Any future permanent release requires its own explicit confirmation and reviewed traversal contract.

On startup, validate the bounded index/records without following links, list operations requiring inspection, and reconstruct no action authority. The user may request inspection or restore through the existing coordinator. Restore revalidates the current private recovery directory, payload tree and target parent; requires an absent target and a same-filesystem no-replace rename; never overwrites the original path or deletes the published destination. Restore changes caused by surviving writers are shown as current retained contents, not falsely rejected as damaged merely because they differ from the original copy manifest. Structural identity/ownership uncertainty refuses automatic restore and leaves an inspectable recovery path. A full byte-for-byte verify is required before any later proposed permanent release.

An unavailable source volume leaves its recovery record discoverable with **volume unavailable**, not forgotten or completed cleanup. Paths/names shown in receipts are plain text. Recovery metadata contains paths and file facts and is private user data, not telemetry. Records stay until a separately authorized lifecycle transition resolves them.

## Receipts and application integration

A terminal receipt carries operation ID, original request generation, typed status, last established phase, observed source/destination/recovery locations and identities, uncertainty flags, retained byte estimate and per-item outcomes. Locations are observations, never tokens accepted directly from QML. Distinguish: refused/no effects, cancelled with owned partial, destination complete/source still at original, source retained/recovery required, completed with retention, restored, and unknown placement requiring inspection.

Any observed destination publication or source relocation triggers affected directory refresh even if the receipt is not success. Late progress/completion from a replaced request cannot replace the current receipt. Batch processing stops on conflict/uncertainty/cancellation and preserves the ordered completed-item receipts. No catch-all error clears them. Same-filesystem rename, current Copy, home Trash and remote transfer semantics remain unchanged except the independently necessary cleanup fix and truthful partial reporting.

The first production sequence is: reproduce/repair foreign-output cleanup; add strict values and journal/platform collaborators; implement staged copy/publish with source-preserved receipts; add retained-tree retirement and restart inspection/restore; wire the existing coordinator and presentation. Each slice requires exact different-author review, focused native failure tests and docs. Installed cross-volume UI proof follows source acceptance.

## Executable evidence

`tests/design/cross_device_move_contract.py` is a finite, in-memory design proof. It models named tree identity and byte retention, foreign destination collisions, interleaved writes/replacements, cancellation, crash phase observation and restore conflicts. It does not model Linux durability or establish executable production behavior.

Native gates must use the real copier/coordinator and real descriptor-relative storage in disposable directories, inject syscall failures at each durability/rename boundary, replace the stage during publication and the destination before/after source retirement, retain opened child descriptors through retirement, and test actual different devices without mounting or touching personal data. A private installed UI journey must show retention/partial outcomes and safe restore. Hardware unplug/power-loss qualification is separate from synthetic failure injection.

## Authored ED05 storage slice — October 8

Owning recovery values, record codec/store, mount admission, manifest/hash,
catalog and cross-volume move/recovery collaborators separate persistence,
Linux descriptor fences, bounded verification, discovery and transfer/recovery
coordination behind LocalMutationBackend. No service, dependency, device control
or parallel worker queue is added.

Canonical private JSON schema1 caps each record at64KiB, phase history at16
exclusive append-only0600 slots and catalog admission at128 immutable prepared
locators. The source-side journal supplies current observed phase. Files and
directories are synced and read back; recovery storage device/inode/mount/owner/
mode are bound in both records. Unknown, duplicate, malformed, partial, linked,
replaced or excessive records refuse active recovery and remain inspectable.
Catalog locking serializes capacity across application windows. Missing source
volumes preserve discovery metadata with unavailable/uncertain state. No startup
copy, rename, unlink or retry is reconstructed.

The copier adds a borrowed-descriptor recovery entry point. Its strict branch
checks permission/time syscall errors and depth128; ordinary Copy keeps previous
policy. Complete source and destination manifests bind child sets and file
readback before publication, before/after source retirement and terminal receipt
durability. Stat sequences are observations, never atomic writer exclusion.
The retained named inode tree protects surviving-descriptor writes. Destination
metadata requirements cover kind, regular size, permission bits and modification
time; cloned ownership, ACL and xattr semantics are not asserted by the content
digest. Original metadata and named bytes remain in retained storage.

The existing controller keeps one worker slot, generation-fenced callbacks and
ordered per-item recovery receipts. Graphical recovery discloses retained space,
shows literal accessible paths/byte estimates, and confirms fresh restore by
catalog UUID. Restore reacquires storage/parent, verifies the current retained
manifest before/after same-device no-replace rename and preserves the published
destination. Changed retained bytes are current contents, not falsely rejected
as damage. Corrupt/partial journals are inspection-only with no destructive
workaround. Author native fixture evidence is below; independent/integrated and
installed UI gates remain pending; no physical
unplug/power-loss or complete ED05 claim follows from this source candidate.

The source candidate now implements unexpected-capture best-effort return with
a durable Required journal record before the repair syscall. Fresh full captured
tree and exact current mount/parent/private-directory observations, cancellation
and an absent original name are admission requirements. No-replace publication
protects a name recreated during the remaining syscall interval. Successful
return synchronizes both parents and reads back the returned tree; any return
still reports the selected original source placement unconfirmed. Failed
admission preserves the captured entry and other candidates for explicit repair.
This closes the source-authoring gap; current author compiler/native fixture
evidence is below, while independent native acceptance remains pending.

The source repair restores the prior reviewed a2b625cf journal contract and
original49cd same-inode regression. Both journal and immutable catalog appends
keep a pre-file-fsync writer metadata baseline through held descriptor, named
entry and separately opened reader checks. Equal decoded bytes on a substituted
inode or an observed rewrite cannot borrow an earlier synchronization result.
These are observed stability guards, not exclusion of external writers or a
power-loss guarantee. Readback failure preserves every record candidate.
Strict recovery transfer reads at most the opened source file size and checks
one extra byte only to reject growth; shortened or changed files fail admission.
Ordinary Copy retains its existing read policy. Current guard fixtures pass;
a separate native prior-version old/fixed comparison has not run.

The restart cohort is registered through an owning bounded Python outer runner,
not merely a CTest timeout. It creates a private session, observes the exact
leader with waitid WNOWAIT so its PID cannot be recycled before cleanup, caps
the cohort at50 seconds, then retires only that owned process group and reaps
the leader. Independent source admission preceded actual normal/early-error/
timeout wrapper controls and owning native fixtures. Leader/unit/group results
do not prove all descendants absent.

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
