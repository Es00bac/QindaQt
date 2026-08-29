# Vera Kline frozen combined Integration QA verdict: PASS

- Timestamp: 2026-08-28T19:24:23Z
- Exact HEAD: `361e601373daf3cbf5f2874b753e7469ca467665`
- Tree: `2bcd5882788ae40b37c9797501ccd508d71c79da`
- Parent: `bff0ad1f2e06d4cdc63ffa6869a8c808778d84a3`
- Public merge-base: `146fc48358c2659436dec4fc6b6062d23c5ee746`
- Public left/right: `0/33`
- Verdict: **PASS — exact product tree is safe to integrate/publish**

## Frozen state and collision audit

HEAD, tree, and the only worktree-diff SHA-256
`e5503943ed6a58479ac19f6d998af48a0f437665b0b0cbcc5126bd5ec417504c`
were identical before and after the final build/test/static gate. There are zero
unmerged entries and zero nonignored untracked paths. The only dirty path is
manager-owned operational `ops/team/providers.json`; JSON syntax passes and it
does not overlap the accepted Appearance candidate. Its provider-status truth
remains a manager responsibility and is excluded from product evidence.

The accepted Appearance change and later public-descendant work overlap only in
the normative shared pages `docs/wiki/architecture/module-boundaries.md` and
`docs/wiki/development/testing-harness.md`. There is no overlapping uncommitted
path and no direct Appearance source/package collision. Full build/tests and
strict docs validate the combined meanings.

## Exact configuration and build

The external build is
`/mnt/d/QindaQt/builds/appearance-settings-s0-flow-integration/progress-combined-debug`,
exposed by worktree path `build/progress-combined-debug`. Final configuration:

```text
cmake -S /home/cabewse/work_SPaC3/container-wm-workers/appearance-settings-s0-flow-integration \
  -B /mnt/d/QindaQt/builds/appearance-settings-s0-flow-integration/progress-combined-debug \
  -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=ON \
  -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=OFF \
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=OFF \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF \
  -DQINDAQT_ENABLE_STRICT_WARNINGS=ON -DCMAKE_AUTOMOC_PATH_PREFIX=ON
cmake --build /mnt/d/QindaQt/builds/appearance-settings-s0-flow-integration/progress-combined-debug --parallel 2
```

Final configure emitted no warning; build exit 0. The first clean build had
1,391 total actions (106 serial, then 1,285 remaining actions at conservative
two-job concurrency). Frozen Global Menu completion rebuilt 33 affected
actions. `CMAKE_AUTOMOC_PATH_PREFIX=ON` is the documented CMake reproducible-
build setting required because this source/build topology uses symlinks.

## Exact executable evidence

- Complete registered suite:
  `ctest --test-dir .../progress-combined-debug --parallel 1 --output-on-failure`
  — **236/236 PASS**, 0 failed, exit 0, 85.13 seconds.
- Exact Appearance/Settings selector:
  `^qindaqt\.(appearance-(values|preview|settings-model|page)|settings-app-(offscreen|rejects-unknown-route|desktop-identity|route-construction|installed-routes)|settings-migration)$`
  — **10/10 PASS**, 0 failed, exit 0, 13.41 seconds.
- The complete suite also contains Global Menu 11/11, Audio applet 2/2,
  Task List 7/7, all package/install consumers, private-D-Bus services,
  isolated headless Wayland/Weston, and offscreen visual/accessibility rows.
  Host-uinput rows were disabled and no host display/input/session was touched.

## Static and documentation evidence

- `./tools/check-source-shape` — PASS, **1,300** files, zero warnings, zero
  allowlisted skips.
- `./tools/validate-docs` — PASS, **86** Markdown documents plus navigation.
- `/home/cabewse/venv/bin/mkdocs build --strict --site-dir .../mkdocs-site`
  — PASS. The repository-documented ambient `python3 -m mkdocs` is unavailable
  in the default Python, so the established project venv was used.
- `git diff --check origin/main..HEAD` and `git diff --check` — PASS.
- `ops/team/{providers,features}.json`, `mkdocs.yml` YAML, added temporary-
  marker scan, exact ancestry/status/diff fingerprints — PASS.
- `ctest -N -R 'docs|links'` registers zero rows; this is a bounded harness
  caveat, with link/navigation authority exercised by `validate-docs` and
  strict MkDocs instead.

## Material findings resolved during QA

The first external clean build stopped at generated
`moc_theme_catalog.cpp:9` because default CMake AUTOMOC emitted a relative
include incompatible with the source/build symlink topology. Enabling the
documented build-only `CMAKE_AUTOMOC_PATH_PREFIX=ON`, cleaning retained
generated output, and rebuilding resolved it without source changes.

Moving-tree checkpoints correctly invalidated exact attribution twice. During
them, QA exposed an Audio QML `OUTPUT_DIRECTORY` configure warning and a Global
Menu 296-line source-shape decomposition warning. The manager repaired both,
committed them, and froze `361e601`; the final configure and source-shape gate
are warning-free.

## Resources and boundary

Build output consumes **2,275,211,776 bytes** (`du -sh`: 2.2G). The filesystem
has **477G available** at 49% use. No source, docs, feature ledger, task/handoff,
commit, host display/input/session, or user configuration was modified by QA.
Requested next action: the Program Manager may integrate/publish exact product
tree `361e601`; handle the operational provider ledger only after separately
confirming its real provider-state evidence.
