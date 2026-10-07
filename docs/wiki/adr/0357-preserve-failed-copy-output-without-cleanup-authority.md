# ADR-0357: Preserve failed copy output without cleanup authority

- **Status:** Proposed
- **Date:** 2026-10-07
- **Owners:** File Manager
- **Supersedes:** The automatic failed-copy cleanup clause of [ADR-0064](0064-confine-file-mutation-to-identity-checked-local-authority.md), only when accepted
- **Superseded by:** None

## Context

Actual old-source C++ tests reproduced four destructive outcomes: a failed
exclusive create removed a pre-existing file or tree, a progress callback
replaced a partially copied file before cancellation and the foreign file was
removed, and a completed traversal followed by a vanished-source check removed
another replacement. The helper and backend each performed pathname-based
failure cleanup. A successful creation or a preceding identity check does not
make a subsequent path-based unlink conditional on that same object.

The existing request coordinator and application-private mutation boundary
remain suitable. The immediate defect can be fixed without implementing
cross-device Move, a recovery journal, source retirement or new Trash behavior.

## Decision

Neither the recursive copy helper nor the Copy backend removes output after
failure. Failed exclusive creation grants no output ownership. Cancellation,
I/O failure, hostile content, source change or failed destination readback
preserves every remaining candidate, including any foreign replacement.
No compensating path-based recursive delete is attempted.

A synchronous worker call returns value-owned MutationOutputObservation:
None means no copy-root creation was observed; RetainedPartial means the
last descriptor and pathname metadata observations matched after incomplete
copying; RetainedCopy means those observations matched after traversal,
writes and fsync reached their end; Replaced means the observed destination
metadata differs; Unconfirmed means its location or ancestry could not be
confirmed. The copyFinished flag records traversal completion independently of
the overall typed error. It is not a content-verification or success receipt.
Written-descriptor, observed-path and parent identities are optional metadata
observations, never capabilities to reopen, remove, restore or reuse an entry.

The exclusiveCreation flag distinguishes a regular file opened with O_CREAT
and O_EXCL from a directory opened after mkdirat. The latter pair cannot
atomically prove that the opened directory is the one just created. Directory
notices therefore avoid claiming exclusive creation ownership. This repair
does not claim to close all mkdir/open or source-writer races. Identical inode
metadata is not a content manifest or a mount-incarnation lease. Readback can
become stale immediately, and an externally moved copy need not have a
discoverable path.

The GUI-thread coordinator retains request-ordered per-item value observations:
attempted success, attempted typed failure, and explicitly unattempted suffix.
The first failure still stops a batch. Between-item cancellation must not label
the previous item's successful output as the failed output. Terminal partial
effects and successful prefixes notify the existing refresh consumers even when
the operation fails; that notification is not success authority.

The existing failure card presents the full observed path as literal plain
text and accessible text, with different wording for retained partial output,
changed destination and unconfirmed location. It offers dismissal only.
Observations confer no undo, restore, open or cleanup action. They belong to the
latest admitted operation, are value-copied across the worker/GUI boundary, and
are cleared when another operation is admitted. There is no persistent receipt,
restart replay or automatic cleanup here. The private C++ boundary has no stable
external ABI; existing typed operation errors and successful-copy semantics
remain, with additive observations for consumers.

## Consequences

- Failed copies can retain incomplete output and consume destination space.
  A later user operation must acquire its own ordinary authority; there is no
  automatic eviction, deferred cleanup or failure/restart deletion.
- A retained completed traversal can still have an overall failed result after
  source postcheck. It must not be presented as a successfully completed request.
- Directory identity and mount facts remain bounded observations. Full verified
  destination publication and source-retention recovery for cross-device Move
  belong to the separate Proposed ADR-0355 package; none of that implementation
  is delivered by this repair.
- Explicit permanent-delete and existing Trash/extraction policies are outside
  this change. Their traversal is preserved while common descriptor helpers
  and copy traversal are split into cohesive application-private source files.

## Verification

The immutable pre-repair packet ran the four real C++ sentinel cases against
old production source: all four lost foreign output. Repair qualification must
rerun those cases and source-postcheck retention, parent replacement, successful
nested copy, existing local mutation/Trash and controller regressions.
Separate controller tests cover partial/replaced/unconfirmed observations,
successful batch prefixes, unattempted suffixes and between-item cancellation.
The production StatusBanners/StatusBanner QML fixture verifies literal hostile
paths, accessible text, plain-text labels and dismissal-only failure presentation.
Native and UI qualification is required before acceptance; this Proposed ADR
and the source draft alone are not execution evidence.

See [File Manager](../apps/file-manager.md#failed-copy-output-observations) and
the [owning test harness](../development/testing-harness.md#file-manager-s1-focused-proof).
