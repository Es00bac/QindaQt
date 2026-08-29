# Devika Shah — PB-0 exact public-base integration rehearsal handoff

- Time: 2026-08-28T07:24:54-06:00
- Owner: Devika Shah
- State: waiting; read-only rehearsal complete, Priya exact rereview pending
- Public tip: `9db68c4023257b49421101fa1b13c73bbc2cfa85`
- Candidate: `30783867d7f2f49c9ad740c90f1c824614510b72`
- Candidate tree: `0fb14c92301dd374a8b9d39859ec20f1bbf37aff`
- Candidate parent: `cea3fb9a5b3d1a1aa8d0bc23570218ed86722f05`
- Exact merge base: `0a547df33d9a31b969d78b4ca649d0b39dc04797`

This was a read-only source/static rehearsal. No product or ref was edited; no
checkout, merge, rebase, cherry-pick, configure, compile, test, install, or
runtime action occurred.

## Ancestry and manifest truth

- Neither exact tip is an ancestor of the other. Integration therefore needs a
  genuine two-parent merge, not a fast-forward.
- Public changes 36 paths from the merge base. Candidate changes 40: 33 adds
  and seven modifications.
- Exactly four paths overlap:
  - `docs/wiki/architecture/module-boundaries.md`
  - `docs/wiki/development/testing-harness.md`
  - `src/CMakeLists.txt`
  - `tests/CMakeLists.txt`
- `git merge-tree` reports all four as changed on both sides, with exactly
  three conflict hunks. The testing-harness page auto-merges.
- Of the 36 candidate-exclusive paths, 33 do not exist on public and three
  retain the exact merge-base blob on public: `power-service.md`, `index.md`,
  and `mkdocs.yml`. Public drift against every candidate-exclusive path is
  zero. The candidate owns 13 Power production paths, nine brightness
  production paths, five Power test paths, and four brightness test paths.
- Of the 32 public-exclusive paths, 25 are new and seven retain the base blob
  on the candidate. Candidate drift against every public-exclusive path is
  also zero.
- Neither side changes an ADR from the merge base. There is no ADR conflict or
  ADR union to invent during integration.

## Exact union resolutions

Resolve only the three textual conflicts below. Any extra conflict or path
drift is a stop condition.

1. In `docs/wiki/architecture/module-boundaries.md`, retain the four service
   rows in dependency order immediately after `display_transaction`:
   `display_service`, `power_protocol`, `brightness_model`, then the existing
   audio and other service rows. Preserve the complete descriptions from both
   tips; do not synthesize a broader process boundary.
2. In `src/CMakeLists.txt`, retain these additive entries in this order after
   `services/display_transaction`: `services/display_service`,
   `services/power_protocol`, `services/brightness_model`, then existing audio
   entries.
3. In `tests/CMakeLists.txt`, retain the corresponding test subdirectories in
   the same order: display service, Power protocol, brightness model, then
   audio.
4. `docs/wiki/development/testing-harness.md` auto-unions. Inspect it anyway:
   it must retain the candidate's current PB-0 pure-proof section and the
   public tip's Display1 resident/private-bus evidence and selectors. This is
   an explicit semantic union check, not a fourth manual conflict.

## Deterministic manager integration checklist

1. Wait for Priya's PASS on immutable candidate `30783867`. Route a concrete
   blocker to Devika; if a descendant repair is required, repeat this rehearsal
   against that exact descendant.
2. Assert the public tip is still exactly `9db68c4`. If it moved, stop and
   repeat the base/overlap/blob audit against the new exact tip.
3. Start from a clean manager-owned public worktree. Create a non-destructive
   `--no-ff` merge with first parent `9db68c4` and second parent `30783867` so
   the three PB-0 vertical commits and repair descendant remain reviewable.
4. Resolve only the three conflicts with the exact unions above. Reject any
   conflict marker, extra conflict, path deletion, or dependency reordering.
5. Before committing, assert all 36 candidate-exclusive merged blobs equal
   candidate `30783867`, all 32 public-exclusive merged blobs equal public
   `9db68c4`, and only the four declared overlaps differ as intentional unions.
6. Configure a fresh dependency-light Debug tree with `BUILD_TESTING=ON`, KWin
   plugin and both shell builds OFF, host uinput tests OFF, and strict warnings
   ON. Build serially only these five PB-0 targets:
   - `qindaqt_power_protocol_values_tests`
   - `qindaqt_power_protocol_codec_tests`
   - `qindaqt_power_aggregation_tests`
   - `qindaqt_brightness_math_tests`
   - `qindaqt_brightness_composition_tests`
7. Run exact selector
   `^qindaqt\.(power-protocol-|power-aggregation-|brightness-model-)` and
   require 6/6. Run the five binaries directly and require 54/54 QtTest rows:
   Power 39 and brightness 15.
8. Because three conflicts touch shared registries, rerun exact public
   `^qindaqt\.display-service-` and require its 5/5, including both serial
   isolated private-bus rows. This is manager integration evidence, never
   attributed to the PB-0 candidate.
9. Run whitespace, source-shape, documentation/navigation, strict MkDocs, and
   repository link gates on the merged tree.
10. In a fresh test-disabled Release tree with the same dependency-light
    feature switches, build and staged-install. Require
    `libqindaqt_power_protocol.a`, `libqindaqt_brightness_model.a`, six public
    Power headers, and five public brightness headers. Both targets join the
    repository's `QindaQtTargets` export set, but this candidate does not add a
    package-config/export installation. Do not claim an installed consumer
    package gate that does not exist.
11. Commit only after all gates pass. Record exact merge commit, tree, both
    parents, counts, staged-install manifest, and remaining boundary. PB-0 has
    no Power bus executable, descriptor/XML, systemd unit, platform adapter,
    UI, hardware mutation, or private-runtime gate by design.

## Manager-owned evidence updates after integration

Update these only on the accepted integrated tree, never from candidate prose:

- `docs/wiki/architecture/power-service.md`,
  `docs/wiki/reference/power1-v1.md`, and
  `docs/wiki/architecture/brightness-model.md`: replace candidate/rereview
  caveats with the exact integrated pure proof while preserving that Power1
  resident service/client/platform behavior remains PB-1+.
- `docs/wiki/development/testing-harness.md`: record the integrated 6/6,
  54/54, and archive/header install scope while retaining Display1 evidence.
- `docs/wiki/development/implementation-roadmap.md`: record PB-0 pure
  protocol/aggregation/brightness foundation integrated; retain PB-1 through
  PB-5 and other providers as remaining.
- `docs/TASK_LIST.md` and `docs/HANDOFF.md`: name the exact integration commit,
  evidence, pure-slice boundary, and next PB-1 outcome.
- Team-board `QQ-005.03`: replace “architecture only/no implementation” with
  the integrated PB-0 evidence. `WIRED` is the bounded stopping-point mapping:
  buildable/installable pure values, codecs, aggregation, and composition are
  integrated, but no resident Power1 service/client, platform authority,
  hardware mutation, Settings/Shell UI, or runtime qualification exists. Do
  not mark the row EXECUTABLE or QUALIFIED from PB-0.

Requested next action: Priya rereviews exact `30783867`; on PASS the manager
performs the checklist above. Devika remains available for exact reproduced
candidate defects and does not call PB-0 accepted or complete before review and
integrated gates.
