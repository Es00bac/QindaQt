# Babbage the 3rd — handoff: immutable Display D3 client/coordinator

- **Timestamp:** 2026-08-28T15:55:04-06:00
- **Exact candidate:** `03ff7e9f224c841a6fc1ae9909b6dfb53ec73cf2`
- **Tree:** `f0d565125f61d164bcc06587c24c04d9f0b995d5`
- **Parent/prerequisite:**
  `44f21716fae42bff1b1ba31830afae6da85bc2d2` (Gauss Meridian's accepted D2
  transaction-summary projection)
- **Original recovery base:**
  `146fc48358c2659436dec4fc6b6062d23c5ee746`
- **Branch/worktree:** `worker/display-d3-kimi-nyra` at
  `/home/cabewse/work_SPaC3/container-wm-workers/display-d3-kimi-nyra`
- **State:** immutable handoff; worktree clean; different-worker exact review
  requested

## Outcome

The candidate lands the public Display1 D3 boundary:

- an asynchronous Qt D-Bus transport with activation, exact unique-owner
  addressing, bounded adapter decode, bus-disconnect handling, normalized
  errors, and post-stop late-result suppression;
- a one-thread Client with exact owner/epoch/revision fencing, first-read
  announced-epoch protection, A/B/A replacement handling, whole-value
  validated publication, fail-closed last-known-good withdrawal, serialized
  mutations, monotonic request IDs, and exactly-once asynchronous results;
- timeout, stop, cancel, supersession, owner loss, service loss, malformed
  payload, and hostile result handling that never replays a mutation and
  retains the exact submitted lineage in uncertain completions;
- a one-transaction Coordinator whose `Confirmed`, `Reverted`, `Uncertain`,
  and `NoOp` outcomes are closed, whose point-of-no-return Confirm cannot be
  cancelled, and whose AwaitingConfirmation state comes only from the
  resident's validated server projection; and
- a public architecture page, module/navigation registration, four
  deterministic transport-seam rows, and one real resident/Qt transport
  private-session-bus lifecycle row.

Changed candidate paths are exactly the 25 files under
`src/services/display_client/**`, `tests/services/display_client/**`, plus the
single additive `src/CMakeLists.txt` and `tests/CMakeLists.txt` registrations,
`docs/wiki/architecture/display-client.md`, the module-boundary/index rows, and
`mkdocs.yml`. No `ops/team/**`, `.omc/**`, compositor writer, Settings UI, host
display/session/input/configuration, or manager state entered the commit.

Prior authorship is preserved in the commit body: Nyra Sol's transport seam,
Tara Wells's test intent, Pavel Shore's continuation, Helena March's exact
analysis, and Gauss Meridian's prerequisite repair all remain credited. Tara's
historical local message files were copied byte-for-byte into the shared board
before the obsolete worktree copies and session cache were retired.

## Final executable evidence

All commands were run on the exact committed tree with host display/session
addresses removed for private-runtime rows.

- Strict Debug: `ctest -R '^qindaqt\.display-'` — **21/21 PASS**.
- Strict Release: same selector — **21/21 PASS**.
- Exact D3 selector in Debug — **5/5 PASS**.
- Exact D3 selector in Release — **5/5 PASS**.
- Direct Release QtTest cases:
  - lineage **9/9**;
  - publication **11/11**;
  - operations **8/8**;
  - coordinator **8/8**;
  - private bus **4/4**;
  - total **40/40**, zero failures/skips.
- Generated Release install rules staged both DisplayProtocol and
  DisplayClient libraries plus all nine public headers beneath
  `/mnt/d/QindaQt/builds/display-d3-babbage/stage-44f2171`; a standalone
  source-tree-free consumer including all four DisplayClient headers compiled,
  linked, and ran exit 0.
- A staged-only poison consumer requesting the private nonexistent
  `display_client_replies_p.h` failed compilation as required.
- `python3 tools/validate-docs` — **75 documents validated**.
- `mkdocs build --strict` — PASS.
- `python3 tools/check-source-shape` — **1,148 files, 0 skipped**, no new file
  reaches the 500-nonblank-line review threshold.
- `git diff 44f2171..03ff7e9 --check`, exact path provenance, `git fsck`, and
  `git status --porcelain` — clean.
- No matching private `dbus-daemon`, Display client fixture root, or `.omc`
  residue remains.

## Bounded remaining work

This candidate does not claim a public compositor output-management writer,
durable journal, lock/logind integration, Settings display UI, nested KWin
convergence, physical hardware behavior, or resource qualification. Those are
the documented later D2/D8 consumers and platform gates; the D3 client does
not cross those boundaries.

## Requested next action

Assign a different worker to review exact immutable commit
`03ff7e9f224c841a6fc1ae9909b6dfb53ec73cf2` rather than this summary. Review
owner/epoch/revision and operation-result lineage, timeout/stop/cancel
exactly-once behavior, server-projected Coordinator outcomes, real private-bus
lifecycle, installed public surface, and candidate provenance. Route any exact
reproduction back to Babbage the 3rd for a same-worktree descendant repair and
rereview.
