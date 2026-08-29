# Notification Live bounded runtime-lane blocker

- **To:** Manager / runtime-lane allocator
- **From:** Soren Pike, Notification Live qualification
- **Timestamp:** 2026-08-28T05:43:03Z
- **Exact source:** `worker/notification-live` at base/HEAD
  `c4982697858c083828bd406f1aa56c4e942bcc10`; preserved uncommitted candidate
- **Outcome state:** source-complete and safe-gate-complete; installed live
  qualification blocked only on the required explicit runtime allocation

## Exact candidate identity

The worktree remains the same 70 candidate paths (38 tracked modifications and
32 untracked additions) reviewed by Lyra, Omar, and Theo. No candidate source
was edited during this qualification pass. The final identity recheck gives:

- tracked binary-diff SHA-256:
  `dc5d63b9cdf117457e8d341b7e5be41cdd49fe2a7203bc4a20fbeac692683e26`
- untracked content-manifest SHA-256:
  `9454c5dc57c1f3eacd21c0680173ace6b7f87e1a88eb6f7ac661e1389cddd4c4`
- candidate path-manifest SHA-256:
  `8f0deffb84a07d2de654be6a220ab9d8decae85504e22beafe371585daf895fb`

The separate untracked `.omc/` directory is local tool state and is excluded.
Against public main `2c52c985f846b083c2aebb7a08f04aa8318a2912`, the only candidate
intersections remain `docs/wiki/adr/index.md`,
`docs/wiki/development/testing-harness.md`, `mkdocs.yml`, and
`tests/session/CMakeLists.txt`; the prior handoff records the additive merge
ordering. No product source overlaps.

## Completed evidence

- Fresh driver unit 10/10, documentation validation 44, source shape 799 with
  zero skips/warnings/errors, compositor D-Bus descriptor parse, and
  whitespace: pass.
- Complete Debug and Release serial builds: pass. Focused selector 50/50 in
  each; safe broad non-runtime registry 148/148 in each. Tests 103–122 were
  deliberately excluded because they are installed/nested/display workflows.
- QML lint 3/3, strict MkDocs, and the 161-file Release staged install: pass.
  All seven production artifacts required by the live harness exist and are
  nonempty.
- Target-limited ASan+UBSan changed graph: 445/445 serial build steps and six
  exact focused rows 6/6, with leak detection and halt-on-error and no
  diagnostic.
- Omar's fresh containment/teardown audit `1787894141`: bounded pass, no
  source blocker. Theo's earlier compiler/Release/sanitizer/stage gaps in
  `1787894227` are closed by the evidence above.
- Both Debug and Release registries contain exactly tests 108–113:
  `shell.notification-live.{1080p,wuxga,1440p,scale-125,scale-150,race-10x}`.
  None was executed.

No CPack/package target is registered. A supplemental relocated-prefix D-Bus
activation check also exposed the existing configure-time
`Exec=/usr/bin/qindaqt-settings-service` descriptor. That adjacent Settings1
packaging limitation does not affect these live rows, which resolve and start
the exact staged service executable directly; it remains a truthful bounded
caveat rather than an out-of-ownership repair.

## Bounded blocker and requested next action

The user prohibits a private nested/session run until the manager explicitly
allocates the single runtime lane. No such allocation was present through this
timestamp, including after request `1787895537`. Consequently:

- no private bus, compositor, display socket, locker, input device, or session
  was launched;
- no host display/input/session/configuration state was touched;
- the five production rows and race-10x remain the only material acceptance
  evidence gap; and
- no commit was created, because committing now could imply the requested
  installed live qualification had passed and would not satisfy the requested
  exact non-amended completion candidate.

Allocate the lane explicitly to Soren, then resume with:

```sh
ctest --test-dir build/notification-live-release-current \
  -R '^shell\.notification-live\.(1080p|wuxga|1440p|scale-125|scale-150)$' \
  --output-on-failure
ctest --test-dir build/notification-live-release-current \
  -R '^shell\.notification-live\.race-10x$' --output-on-failure
```

On a clean six-row result, the remaining actions are final cleanup/integrity,
stage only the 70 named candidate paths (never `.omc/`), create one exact
non-amended milestone commit, and request independent review of that commit.
