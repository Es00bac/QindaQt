# Notification Live non-runtime closure and private-runtime lane request

- **To:** Manager / runtime-lane allocator
- **From:** Soren Pike, Notification Live qualification
- **Timestamp:** 2026-08-28T05:38:57Z
- **Exact source:** `worker/notification-live` at
  `c4982697858c083828bd406f1aa56c4e942bcc10`, preserved uncommitted 70-path
  candidate (38 tracked plus 32 untracked); `.omc/` remains excluded
- **State:** all safe prerequisites passed; no private nested/session runtime
  started

## Peer-finding reconciliation

Omar's fresh six-axis audit `1787894141` is a bounded containment/teardown
pass with no source repair requested. Its remaining KWin environment,
KScreenLocker, shell-name release, fractional-scale, and race timing facts are
exactly the runtime evidence this matrix must collect.

Theo's provenance audit `1787894227` accurately captured an earlier static
snapshot. Its compiler, Debug/Release, sanitizer, and staged-install gaps are
now closed below. The six installed live rows, resident survival, live lock
privacy, and timing evidence remain explicitly unclaimed.

## Fresh non-runtime evidence

Every build in this worktree used `cmake --build ... --parallel 1` after a
headroom check.

- Python live-driver unit: 10/10 pass; documentation validation: 44 documents;
  source shape: 799 files with zero skips/warnings/errors; D-Bus descriptor
  parse and `git diff --check`: pass.
- Complete current-source Debug and Release builds: pass. The exact focused
  notification/settings/session/shell selector passes 50/50 in each. The safe
  broad non-runtime registry passes 148/148 in each (102/102 plus 46/46), with
  registered runtime rows 103–122 deliberately excluded.
- `all_qmllint`: 3/3 pass. Existing warnings are outside the changed
  Notification card/center files.
- Release staged install: 161 files; all seven required installed artifacts
  exist and are nonempty (`qindaqt-wm`, `qindaqt-session`, notification host,
  Settings1 service/app, production shell, and exact KWin plugin). This tree
  registers no CPack/package target, so no package command is claimed.
- ASan+UBSan: the changed supervisor, shell evidence/QML, compositor input,
  and live-probe target graph builds serially (445/445 steps). Six exact
  focused tests pass 6/6 with leak detection and halt-on-error, with no
  sanitizer diagnostic.
- `mkdocs build --strict`: pass from a disposable worktree-local virtualenv;
  the repository documentation validator also passes.

One supplemental check, not part of the registered notification matrix,
attempted D-Bus activation of the relocatable Release staging prefix. It failed
because the generated `org.qindaqt.Settings1.service` contains the
configure-time `Exec=/usr/bin/qindaqt-settings-service`; prefix installation
does not rewrite that absolute path. This adjacent packaging limitation is
outside Notification Live's owned Settings1 module. It does not affect the
matrix: `test_notification_live_nested.py` launches the resolved exact staged
Settings1 executable directly and authenticates its PID/owner.

## Exact requested lane

Please explicitly allocate the single private runtime lane to Soren for the
Release production matrix, first the five core rows and then the isolated
stress row:

```sh
ctest --test-dir build/notification-live-release-current \
  -R '^shell\.notification-live\.(1080p|wuxga|1440p|scale-125|scale-150)$' \
  --output-on-failure
ctest --test-dir build/notification-live-release-current \
  -R '^shell\.notification-live\.race-10x$' --output-on-failure
```

All six rows are registered `RUN_SERIAL`; the first five have 240-second
budgets and race-10x has 2400 seconds. Each row installs its own production
artifacts into a build-tree child and uses a new private runtime, HOME/XDG tree,
bus, socket, process group, and scenario-gated KWin input device. No host
display, input, session bus, shortcuts, locker, password, or configuration is
in scope. If duplicate Debug live execution is an acceptance requirement,
please state that with the allocation; the focused Debug product evidence is
already current.

Until that explicit allocation is posted, I will not launch any private
nested/session runtime and will not create a completion commit that could imply
the installed live outcome passed.
