# Keira Dunn midpoint: status-notifier repair implemented, gates pending commit

- 2026-08-28T14:43:00Z — All four P1, all six P2, and the P3 are implemented
  in the working tree of `worker/system-tray-s0` (no commit yet; reviewed
  `637cb94` remains HEAD until the repair descendant is created).

Material repair decisions, for Shannon's rereview attention:

1. Generation values moved to a globally monotonic quint64 counter
   (`OwnerKey::generation` widened). Owner history is bounded
   (`kMaxTrackedOwners = 256`, live owners only); capacity exhaustion and
   counter exhaustion fail closed with 0/refusal instead of wrapping or
   evicting, so no stale event can ever match a reissued generation.
2. Duplicate/live `beginOwnerGeneration` is now an explicit owner
   **rebaseline**: items drop, identity claims free, a fresh generation issues
   — no presented key can be stale or unactionable. `ownerLost` now requires
   the expected generation and refuses stale loss events. A new
   `beginWatcherEpoch()` resets the population bit for fail-closed Loading on
   watcher (re)connection.
3. Watcher loss was reconciled **to the accepted contract** (ADR-0032 kept;
   no supersession needed): presentation now retains and projects
   last-known-good items in Degraded, and the tests assert their continued
   actionability. Code, tests, and docs now agree.
4. `validateItemDescriptor` is documented via AGENT-GUARD as the single
   admission gate and now validates the menu; submenu-only parents, C1/DEL
   control rejection, and whitespace-only text rejection added; typed
   `RequestIntent` (owner+generation+identity snapshot) returned from
   `evaluateRequest` with an explicit revalidate-before-execution lifetime;
   transport narrowed to `StatusNotifierEventSink` with written
   non-null/no-re-attach/outliving/thread rules; localized presentation text
   injected via `PresentationTexts`.
5. ADR-0026 renamed to reserved **ADR-0032** across filename, index,
   navigation, and every prose link; both parent `add_subdirectory` lines are
   wired and the test CMake now `FATAL_ERROR`s on a missing target instead of
   silently skipping; the testing-harness row is added with narrowed claims
   and the tray page/ADR updated to the repaired contracts.

Static gates already green on the tree: `git diff --check`, source-shape
(1017 files, 0 skips), `tools/validate-docs` (65 documents). Per the
assignment, no compiler invocation ran this session — compile/CTest evidence
is requested from the reviewer/integrator. Exact handoff follows after the
commit.
