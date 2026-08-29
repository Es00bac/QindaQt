# Kellan Ward — Display D2 repair source/static midpoint

- Timestamp: 2026-08-28T11:37:32Z
- Status: working
- HEAD: failed immutable candidate `8901f23fe159263522e2e0d76278c4786c8375e5`
- Repair diff: 13 paths, +754/−50 including three new test files
- Runtime executed in this repair: none

Concrete repair state:

1. `DisplayServiceModel` now validates a bounded restart seed and derives the
   public epoch as a SHA-256 seed digest beside the next process-monotonic
   machine lineage. It retains no attacker-controlled history, rejects lineage
   exhaustion, and cannot recreate the first public epoch under seed/owner
   A/B/A. The model regression retains the first A/1 candidate, accepts three
   distinct owner lineages with factory A/B/A, proves all public epochs differ,
   and rejects that stale first candidate on the third lineage.
2. `architecture/overview.md` now describes the actual activated
   cross-process read/service foundation and its explicit fail-closed
   non-writer stopping point. The owning architecture/reference/harness pages
   state the epoch derivation and private-runtime evidence boundary.
3. Two serial isolated-runtime targets cover the exact-owner asynchronous D0
   source and the successful resident lifecycle. Their shared fixture launches
   only `dbus-daemon` beneath a disposable temporary/XDG root, removes inherited
   host bus/display variables from the daemon, and requires every participant
   to use an explicit private address. The source row covers delayed reply,
   dirty coalescing, owner replacement, stale reply, unavailability and stop
   suppression. The resident row covers name/object registration, typed
   unavailable/snapshot replies, `Changed`, injected short apply/observation
   deadlines causing one forward then one rollback request, and observer/name/
   object teardown.

Static evidence, all exit 0:

- `git diff --check`
- `python tools/docs_validation.py` — 57 documents/navigation
- `./tools/check-source-shape` — 971 files, zero findings
- forbidden dependency/host-boundary grep — only the intentional production
  `sessionBus()` and the private test's removal of inherited
  `DBUS_SESSION_BUS_ADDRESS`

No configure, build, binary, private bus, session, display/input, host config,
service, or hardware action has run in this repair yet. The manager has now
released the serial compiler/private-runtime lane. Next is a fresh worktree-
local Debug configure/build and pure model row; I will post another exact board
checkpoint immediately before executing either private-bus row, then run those
rows serially and continue Release, sanitizer, staged package, strict docs,
immutable descendant, and Dorian rereview.
