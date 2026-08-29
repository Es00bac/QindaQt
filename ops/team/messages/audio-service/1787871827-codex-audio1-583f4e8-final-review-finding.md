# Audio1 exact final-descendant review: FINDING on `583f4e8`

- Reviewer: Codex Audio1 exact final-descendant reviewer
- Time: 2026-08-27T17:03:47-06:00
- Exact candidate: `583f4e84d2a602a99b388b391e10a1db0df34d84`
- Exact tree: `390ffdeffb3b52f5e1170d0bbefa1b993333c4ac`
- Exact parent: `fac2756a65572f37296c0fb6bd38b74aa68574d3`
- Verdict: **REPAIR REQUIRED**
- Findings: **P1 0 / P2 1 / P3 0**

## P2: containment sentence denies the private real graph that the accepted test runs

`docs/HANDOFF.md:47-48` says:

> No active desktop, user session bus, global input, real audio graph, physical
> display, or physical screen lock was touched by this evidence.

The “real audio graph” clause is stronger than, and conflicts with, the direct
accepted evidence. The canonical testing harness at lines 648-653 says that
`qindaqt.audio-wireplumber-runtime` creates a private PipeWire socket, starts a
WirePlumber `policy` profile, creates disposable null sinks/sources and a
synthetic playback stream, and exercises **real libwireplumber graph
discovery**. The integrated Debug, Release, and sanitizer logs all pass that
test. This is a real but fully private/synthetic PipeWire graph. What the
evidence proves was untouched is the **host** audio graph/device and physical
audio hardware.

This is release-source-of-truth text, and the review request explicitly
requires no inaccurate completion or caveat claim. Repair the phrase to the
bounded fact, for example `host audio graph/device`, without weakening the
other no-host-contact clauses. The owning Audio architecture/testing pages and
the completed TASK_LIST entry already make the private null-device runtime and
physical/hardware caveat clear; no source or broader documentation change is
needed.

## Exact descendant and cumulative identity

The isolated reviewer worktree was clean and detached at the exact candidate.
The candidate has the required exact tree and direct parent. Its parent delta
is exactly:

- `docs/HANDOFF.md`
- `docs/TASK_LIST.md`

Both are mode `100644`; the delta is 43 insertions and 57 deletions. There are
zero other paths, deletions, non-doc changes, unmerged entries, conflict
markers, or whitespace errors.

The cumulative Audio identity remains accepted. The accepted Audio range
`dc29c889..1eed5b1` and integrated range `a083a20..fac2756` each contain the
same 57-path set with symmetric difference zero and no deletion. The following
accepted/integrated Git objects match exactly:

- all six Audio protocol/client/service source and test directory trees;
- `docs/wiki/architecture/audio-service.md`;
- `docs/wiki/reference/audio1-v1.md`; and
- `docs/wiki/adr/0014-confine-wireplumber-to-glib-worker.md`.

The candidate changes no object outside the two milestone documents, so the
accepted functional tree remains unchanged.

## Direct evidence audit

The manager's direct logs under `build/` corroborate the numerical claims:

- fresh Debug and Release builds each reach their exact final `749/749` step;
- focused discovery is exactly 7 Audio tests and both focused logs pass 7/7;
- complete discovery is exactly 108 tests and both complete logs pass 108/108;
- Debug and Release lifecycle logs each contain 30 passing executions: 10
  activation, 10 full private runtime, and 10 reset-lifecycle executions;
- sanitizer build reaches 59/59, discovery is exactly 7, and the direct rerun
  passes 7/7;
- the direct sanitizer command record at
  `build/audio1-integrated-sanitizer-tests-command-recorded.log` has SHA-256
  `b5255a41c13c7577f8a185836c6fab66fbae56cc978fa632bef547dec9a09bf9`;
  line 1 records `ASAN_OPTIONS=detect_leaks=1:halt_on_error=1:abort_on_error=1`
  and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`, and line 18 proves
  7/7;
- production build reaches 485/485, QML lint reaches 4/4, and the production
  install manifest contains 186 unique records;
- final manager logs record 47 Markdown documents plus navigation and 831
  source files with zero skips.

The installed lifecycle log is pinned to exact `fac2756`, tree `2f129e0e`, and
has SHA-256
`a310a16d372816ce6651ec8afea1e827dc7e80fb81477b3d075f1091fbb9411b`.
Its final line proves 10/10 cycles, 20/20 activations, 20/20 exact exits, 10/10
distinct replacements, 20/20 exact PID absence, zero staged services, and zero
fixture roots. The corresponding append-only installed handoff records the
private `env -i` containment and all 18 staged Audio artifacts.

No compile, configure, product test, source edit, host session bus, host audio,
desktop, display, input, or configuration access was performed by this final
review.

## Independently rerun gates on exact `583f4e8`

All requested gates passed:

```sh
./tools/validate-docs
# Validated 47 Markdown documents and mkdocs.yml navigation.

./tools/check-source-shape --largest 30
# Checked 831 source files; skipped 0 allowlisted files.

uvx --offline --from mkdocs==1.6.1 mkdocs build --strict \
  --site-dir build/audio1-final-exact-review/site
# Documentation built successfully in strict mode.

git diff --check fac2756a65572f37296c0fb6bd38b74aa68574d3..583f4e84d2a602a99b388b391e10a1db0df34d84
git log --check fac2756a65572f37296c0fb6bd38b74aa68574d3..583f4e84d2a602a99b388b391e10a1db0df34d84
# Both exit 0.
```

Reviewer gate log hashes:

- validate-docs: `241b8287e63debf09f5d71c4acb164ad7217914edc1f41eb1b0ebefc95b61229`
- source shape: `cc869f4cdecad7f2599ff75e88a2b6ca7189d76451091393ca881b4e159e0f1c`
- strict MkDocs: `0e8a9849f7234121349a2e4573d4c72d93e84ad1042cdbcc81a052bfb21e4902`

Final reviewer state is detached at exact `583f4e8`, tree `390ffdef`, with an
empty porcelain status.

## Requested action

Do not publish exact `583f4e8` as the final milestone descendant. Make only the
bounded containment-wording repair in a direct docs-only descendant, then
request exact commit/tree re-review. The functional Audio1 milestone and every
dynamic/static gate otherwise remain accepted; do not rebuild product code for
this prose-only repair.
