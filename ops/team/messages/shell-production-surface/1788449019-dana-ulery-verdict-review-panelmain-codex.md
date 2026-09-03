# Dana Ulery — exact-candidate review

- Persona: Dana Ulery
- Provider/model: OpenAI Codex `gpt-5.6-sol` (reasoning high)
- Exact candidate SHA: `cae66fcd5f3245c7cbea82728928d69acaee0ef1`
- Tree SHA: `1e127cb8870a101547a8f48ecfd927553600915d`
- Parent SHA: `22b31b94e0da12f0be54c5d0d3c48b639815e562`
- Base SHA (`git merge-base main HEAD`): `22b31b94e0da12f0be54c5d0d3c48b639815e562`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/panel-visibility-main-repair-codex-review`
- Review completed: `2026-09-03T08:41:13-06:00`

## Findings ledger

### P0

None.

### P1

None.

### P2

None.

### P3

None.

## Contract review

The seven-file candidate diff is limited to the installed panel-visibility harness, its focused controls, and the owning wiki page (416 insertions, 71 deletions). It does not change production policy, public APIs, registries, schemas, host integration, or runtime ownership/lineage behavior.

The archived failing capture identifies teardown, not a popup/Global Menu lease leak. In `window-close-hidden`, the left/top dock role is still mapped and committed under shell PID 52 with exclusive zone 40 and desired size 40x224, but its actual geometry is 0x0 at `(0,30)`. In `window-closed-restored`, the same role/PID is 40x224 at `(0,30)`. There is no competing popup surface in the failing hidden phase. This is the transient described by the revised wiki contract.

The repair is fail-closed. `tests/session/panelvisibilityphasewaiter.cpp:82` rejects empty, malformed, non-integral, zero-sized, and out-of-frame geometry. `waitForSettledPhase` at line 100 evaluates a phase predicate only after every published surface is settled, polling through the transient and returning false at the injected deadline. The session probe retains phase-specific nonzero exits, uses a 5,000 ms phase deadline (15,000 ms for initial authority), and its CTest rows retain a 110-second outer timeout.

The focused regression at `tests/session/tst_panelvisibilityphasewaiter.cpp:86` is non-vacuous: the first 0x0 snapshot satisfies the old hidden predicate, while the repaired waiter requires the second snapshot where the role is absent. A reviewer-only executable built under the assigned build root reproduced `candidate_reads=2 old_behavior_reads=1`; thus the new row's read-count assertion would fail under the parent implementation at `panelvisibilitysessionprobe.cpp:127-140`, which accepted the first predicate-ready snapshot.

The checked-in timeout control covers a 0x0 role that never disappears. Because there was no explicit checked-in positive-geometry leak control, I added a reviewer-only negative control under the assigned build root. A genuinely mapped 40x224 left panel remained predicate-visible for both allowed polls and returned false (`mapped_leak_reads=2 mapped_leak_waits=2`). The downstream Python validator independently rejected the same class with `left panel is not hidden in window-close-hidden`.

All four successful visibility result archives from the two Debug repetitions contain eight settled, positive, framebuffer-contained phase inventories. For both 1920x1080 and 1920x1200: the left role is absent in both hidden phases and present in both restored/visible phases; the bottom role is present for edge, shortcut, and popup-held, then absent after popup close; the notification-center surface appears only in popup-held. This confirms the waiter does not mask a genuinely mapped panel or a retained popup hold.

## Commands and results

### Identity and cleanliness

```text
git rev-parse HEAD
# cae66fcd5f3245c7cbea82728928d69acaee0ef1
git rev-parse HEAD^{tree}
# 1e127cb8870a101547a8f48ecfd927553600915d
git rev-parse HEAD^
# 22b31b94e0da12f0be54c5d0d3c48b639815e562
git merge-base main HEAD
# 22b31b94e0da12f0be54c5d0d3c48b639815e562
git status --porcelain=v1
# exit 0, empty before review
```

Reviewed `git diff 22b31b94e0da12f0be54c5d0d3c48b639815e562..HEAD` in full and inspected both implementer handoff files and the archived failing evidence.

### Configure and focused builds

Both exact configurations exited 0:

```text
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-panelmain-codex/debug -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake -S . -B /home/cabewse/work_SPaC3/builds/qindaqt/review-panelmain-codex/release -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
```

For each of Debug and Release, the following exited 0 with 1,424/1,424 Ninja actions:

```text
cmake --build <config> --parallel 3 --target qindaqt-panel-visibility-phase-settlement-tests qindaqt-panel-visibility-session-probe qindaqt-desktop-session-probe qindaqt_panel_visibility_modes_tests qindaqt_panel_visibility_scope_tests qindaqt_panel_visibility_validation_tests qindaqt_compositor_visibility_snapshot_tests qindaqt_compositor_visibility_state_tests qindaqt_compositor_visibility_wire_roundtrip_tests qindaqt_compositor_visibility_client_tests qindaqt_qt_compositor_visibility_transport_tests qindaqt_panel_visibility_producer_tests qindaqt_panel_visibility_popup_bounds_tests qindaqt_panel_visibility_settings_private_bus_tests qindaqt_shell_runtime_options_tests qindaqt-shell
```

The first focused selector run in each configuration returned 19/20 passed and one failure because `qindaqt-shell-preview` had not yet been built for the adjacent component-closure install row. This was an incomplete requested-target build, not a candidate failure. I ran the following in each configuration (exit 0, 54/54 actions), then reran the complete selector:

```text
cmake --build <config> --parallel 3 --target qindaqt-shell-preview
```

### Debug and Release focused/adjacent tests

```text
ctest --test-dir <config> -R '^(qindaqt\.shell-visibility-|qindaqt\.shell-runtime-|desktop\.virtual\.panel-visibility\.validator-unit$)' --output-on-failure --no-tests=error
```

- Debug: exit 0, 20/20 CTest rows passed, 0 failed (6.93 s).
- Release: exit 0, 20/20 CTest rows passed, 0 failed (6.49 s).

Verbose execution of `desktop.virtual.panel-visibility.validator-unit` also passed in both configurations: one CTest row, three Python validator controls, and five QtTest lifecycle/test cases (three substantive settlement cases), with zero failures.

### Private nested session rows

Prerequisite, run first:

```text
ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-panelmain-codex/debug -V -R '^desktop\.virtual\.sandbox-unit$' --no-tests=error --parallel 1
```

Exit 0: 1/1 CTest row and 114/114 Python controls passed.

Before every nested invocation and after the final invocation, `pgrep -af '[k]win_wayland'` exited 1 with no output. Each row was then run serially with:

```text
QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop ctest --test-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-panelmain-codex/debug -R '^<exact-row>$' --output-on-failure --no-tests=error --parallel 1
```

Results, including the automatically required `desktop.virtual.package-contract` fixture each time:

| Pass | Exact row | Result |
| --- | --- | --- |
| 1 | `desktop.virtual.boot.1080p` | exit 0, 2/2 passed (2.47 s) |
| 1 | `desktop.virtual.panel-visibility.single-1080p` | exit 0, 2/2 passed (41.96 s) |
| 1 | `desktop.virtual.panel-visibility.single-wuxga` | exit 0, 2/2 passed (46.94 s) |
| 2 | `desktop.virtual.boot.1080p` | exit 0, 2/2 passed (2.41 s) |
| 2 | `desktop.virtual.panel-visibility.single-1080p` | exit 0, 2/2 passed (41.87 s) |
| 2 | `desktop.virtual.panel-visibility.single-wuxga` | exit 0, 2/2 passed (45.84 s) |

No `kwin_wayland` survivor was present at any boundary. No host bus, host display, uinput, hardware, or network row was used.

### Reviewer-only negative/regression controls

Scratch files were created only in `/home/cabewse/work_SPaC3/builds/qindaqt/review-panelmain-codex/reviewer-controls`.

```text
cmake -S /home/cabewse/work_SPaC3/builds/qindaqt/review-panelmain-codex/reviewer-controls -B /home/cabewse/work_SPaC3/builds/qindaqt/review-panelmain-codex/reviewer-controls/build -G Ninja -C /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build /home/cabewse/work_SPaC3/builds/qindaqt/review-panelmain-codex/reviewer-controls/build --parallel 3
/home/cabewse/work_SPaC3/builds/qindaqt/review-panelmain-codex/reviewer-controls/build/panel-visibility-reviewer-controls
```

Exit 0: `PASS candidate_reads=2 old_behavior_reads=1 mapped_leak_reads=2 mapped_leak_waits=2`.

```text
PYTHONPATH=tests/session python3 -c '<inject a mapped left role into window-close-hidden and call _validate_interaction>'
```

Exit 0: `PASS strict-validator-rejected: left panel is not hidden in window-close-hidden`.

### Static gates

```text
./tools/validate-docs
# exit 0: 132 Markdown documents plus mkdocs.yml navigation validated

/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict --site-dir /home/cabewse/work_SPaC3/builds/qindaqt/review-panelmain-codex/site
# exit 0

./tools/check-source-shape
# exit 1: tests/session/DesktopSessionTests.cmake has 621 non-blank lines, limit 600

git diff --check
git diff 22b31b94e0da12f0be54c5d0d3c48b639815e562..HEAD --check
# both exit 0

git diff --name-only 22b31b94e0da12f0be54c5d0d3c48b639815e562..HEAD -- '*.json'
# empty; python3 -m json.tool not applicable
```

The source-shape error is the brief's anticipated manager-owned unchanged-base condition. `git diff 22b31b94e0da12f0be54c5d0d3c48b639815e562..HEAD -- tests/session/DesktopSessionTests.cmake` is empty, so it is not a candidate finding. All other source-shape diagnostics were warnings on unchanged files.

Final `git status --porcelain=v1` was empty, HEAD remained the exact candidate, and the final compositor survivor check was empty.

## Verdict

The interaction repair accurately fences transient layer-surface teardown, preserves bounded fail-closed behavior, does not conceal a real mapped panel or popup hold, and passes its focused, adjacent, and repeated private-session evidence. The unchanged-base source-shape violation is left to the Program Manager as directed.

VERDICT ACCEPT P0/P1/P2/P3=0/0/0/0
