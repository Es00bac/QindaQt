# Cora Vale lead assignment: Controls S2 read-only assistant audit

- **Timestamp:** 2026-08-27T23:27:20Z
- **Issued by:** Cora Vale, Controls S2 lead/keeper
- **Assistant:** manager-assigned same-worktree audit partner
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/controls-s2`
- **Branch / exact HEAD:** `worker/controls-s2` /
  `a083a20af14a2d7b9e954735a2d659c475a536b2`
- **Mode:** read-only product audit; no source, docs, test, board-worker-record,
  build-tree, Git-index, or process mutation

## Authority and communication

Cora retains all Controls product decisions, file edits, compilation/test
execution, baseline generation, commits, reviewer request, and candidate
handoff. The assistant must not configure, build, execute test binaries, start
renderers, touch host input/session state, or edit any path unless Cora later
posts an exact non-overlapping path delegation. Existing uncommitted files in
this worktree are Cora's owned candidate and must remain byte-for-byte intact.

Read the root `AGENTS.md`, Controls/QST/module/testing/documentation wiki
contracts, current `src/controls/**`, `tests/controls/**`, the complete current
Controls diff, and the Controls claim/source/build/repair messages in this
thread. Report each material finding or question directly to Cora as a new
timestamped append-only message in `native-application-design`; do not rewrite
this assignment or another worker's message. Distinguish verified source fact,
inference, and test coverage gap. A final audit message must name inspected
paths, exact current HEAD/diff state, findings by severity, and the requested
lead action. Silence is not approval.

## Concrete audit questions

1. **FormRow geometry:** Do explicit row/column assignments keep the editor at
   positive height, avoid GridLayout collisions, preserve wide and compact
   label/editor/error order, wrap long localization, and mirror text without
   changing logical association? Identify any binding loop or implicit-size
   risk.
2. **ThemeCard hostile totality:** Is `previewValid` always Boolean? Can null,
   partial, missing-group, wrong-typed, non-finite, or out-of-range complete
   shapes ever reach unsafe indexing, mix active and preview roles, or produce
   transient binding warnings? Does a complete real QST QColor map remain
   accepted?
3. **StateCard accessibility:** Does announcement urgency derive from the new
   status rather than a stale dependent binding? Do Information, Success, and
   Busy remain non-alert/polite while Warning and Error are alert/assertive?
   Does the public QML politeness mapping prove the exact tuple sent to
   `Accessible.announce`, with title and user-relevant message, without assuming
   scoped C++ enum numeric identity? Is static Busy text complete?
4. **TextField sizing and lint:** Does the supported `contentItem` implicit
   height expression remain positive and loop-free for the default Qt 6.11
   TextField content item, including larger text scales? Flag any reason
   qmllint may still reject it.
5. **Keyboard semantics:** Are ordinary Button, StateCard action,
   DegradedNotice retry, CheckBox, Switch, Slider, TextField, and ThemeCard
   reachable and activatable through supported native keys? Is busy/disabled
   suppression truthful without overwriting caller `available` state? Ensure
   tests do not claim unsupported Return behavior.
6. **Five-theme and scale visuals:** Does the fixture actually define all five
   built-ins at compact/ordinary/large 100% plus ordinary 125%/150%, validate
   applied DPR and physical pixels, pin fonts/locale/software rendering, and
   keep baseline names collision-free? Identify clipping, nondeterministic
   animation, or unreviewable tolerance risks before generation.
7. **PSS and installed import:** Are bare and controls probes matched enough to
   make the three-pair/five-sample median delta truthful, with exact PID and
   cleanup? Is the installed consumer confined to a freshly removed
   build-local prefix and capable of proving the installed Controls/Tokens
   module rather than falling back to build-tree imports?

Also flag any missed direct behavior/accessibility assertion among the 14
public components, theme-ID/hex/dependency escape, inaccurate wiki claim,
source-size/cohesion issue, or acceptance gate that cannot produce the evidence
claimed. Do not propose scope outside S2.
