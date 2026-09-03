# Independent exact-candidate review — File Manager S1 (local mutation, trash, recovery)

- Reviewer: **Kathleen Antonelli**, independent application reviewer (Moonshot Kimi `kimi-code/k3`), slug `kathleen-antonelli`
- Candidate SHA: `61283bf017990694a9ddc3f183f54751c3ddf849` (branch `worker/file-manager-s1`)
- Tree SHA: `242ff7ca98176af000f2a624aea6a8ea6e641346`
- Parent SHA: `d9aec19cd2eab555373d683c304a7514ba123a0f`
- Base SHA: `d9aec19cd2eab555373d683c304a7514ba123a0f`
- Review worktree: `/home/cabewse/work_SPaC3/container-wm-workers/file-manager-s1-k3-review` (detached at candidate; `git status --porcelain` empty before and after the review)
- Build root: `/home/cabewse/work_SPaC3/builds/qindaqt/review-fm-k3`

## Findings ledger

### P0 — none

No destructive, host-affecting, bus-touching, or network behavior found. All
deletion paths are descriptor-pinned and refuse symbolic links; see the attack
runs below.

### P1 — one

**P1-1. Every identity-carrying mutation dispatched from the real QML UI fails
with `changed`; the S1 user-visible outcome does not work.**

Root cause: `NavigationController::entries()` publishes the listing-time
identity as a `QVariantMap` containing `modifiedNanoseconds` as a `qint64`
(`src/apps/file_manager/model/navigation_controller.cpp:166-167`). The QML UI
hands the selected entry map back as the identity argument
(`src/apps/file_manager/ui/MutationDialogs.qml:74-75, 98-102, 122-123`). The
QML engine converts the `qint64` to a JS Number (IEEE double, 53-bit
mantissa); a current-epoch nanosecond timestamp (~1.79e18) cannot be
represented exactly, so `MutationController::identityFromMap`
(`src/apps/file_manager/mutation/mutation_controller.cpp:200-210`) rebuilds a
corrupted identity, and the backend's precondition check returns
`MutationError::Changed` for **every** UI-initiated rename, copy, move, and
trash. Consequently undo of rename/move and restore-after-trash are also
unreachable from the UI. Only actions whose identity is derived in C++
(new-folder, empty-trash, undo-of-create) work.

Reproduction (scratch, no product paths touched; sources and binary under
`/home/cabewse/work_SPaC3/builds/qindaqt/review-fm-k3/scratch/qml-roundtrip/`,
built against the candidate's own `libqindaqt_file_manager_support.a` and
driving the real `NavigationController`, `LocalDirectoryLister`,
`MutationController`, and `LocalMutationBackend` through a real `QQmlEngine`,
mirroring `MutationDialogs.qml`'s call):

```
env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS \
  QT_QPA_PLATFORM=offscreen ./build/qml_roundtrip <scratch-fixture-dir>
```

Observed:

```
true modifiedNanoseconds: 1788432117315222185
js sees modifiedNanoseconds: 1788432117315222300|number
renameItem accepted: true
failureCode: changed
failureMessage: The selected item changed before the operation began
renamed exists: no | original exists: yes
control C++ renameItem accepted: true | failureCode: none
control renamed exists: yes
```

The C++ control (same rename, identity map assembled without a JS round trip)
succeeds, isolating the QVariant→JS-number marshalling as the cause.

Why the candidate's own gates stay green: `tst_mutation_controller.cpp`
passes identity maps built in C++ (never through the engine), and
`qindaqt.file-manager-mutation-ui-offscreen` only constructs the QML root and
checks object identity — no row dispatches one mutation through the real
QML→C++ path. The lane brief required working keyboard/context actions
("undo for rename/move/create where recoverable", "keyboard parity for every
action"); the claimed behavior is absent, and the only proof offered for it
does not exercise it.

### P2 — two

**P2-1. Orphan Trash payload permanently blocks trashing a same-named item.**

`HomeTrash::trash` (`src/apps/file_manager/mutation/home_trash.cpp:196-234`)
retries the bounded suffix loop only when the `.trashinfo` write reports
`AlreadyExists`. If `files/<name>` exists without `info/<name>.trashinfo`
(left by external interference, another client, or an interrupted session),
the metadata write succeeds, `relocateLocalNoFollow` fails `already-exists`,
the metadata is removed, and the error is returned — the loop never advances
to suffix 1. Every retry fails identically. The wiki promises "Name collisions
receive a bounded numeric suffix" (`docs/wiki/apps/file-manager.md:118`).

Reproduction (`scratch/attacks/`, attack a1): trash `item` once, delete only
`info/item.trashinfo`, re-create `item`, trash again:

```
a1 second trash error: already-exists (The destination already exists)
```

Expected: allocation of `item.1` and a successful trash. Observed: permanent
`already-exists`. Bounded workaround exists (confirmed Empty Trash clears the
orphan), hence P2.

**P2-2. Rename/move can silently overwrite a destination created between the
existence check and `renameat()` (no `RENAME_NOREPLACE`).**

`relocateLocalNoFollow`
(`src/apps/file_manager/mutation/safe_path_operations.cpp:343-357`) checks the
destination with `fstatat(AT_SYMLINK_NOFOLLOW)` and then calls plain
`renameat()`, which atomically *replaces* a destination that appears in the
check/use window. The post-rename identity comparison validates the moved
source, not the clobbered destination, so the overwrite is reported as
success. The wiki claims "an existing destination returns `already-exists`"
(`docs/wiki/apps/file-manager.md:96`) and ADR-0064 states "Existing
destinations are always refused." Copy is not affected (`O_EXCL`); create is
not affected (`mkdirat` fails `EEXIST`).

Reproduction (`scratch/rename-race/`, LD_PRELOAD shim interposing `renameat`
to create the destination mid-window — a hostile runtime environment, no
in-repo edit):

```
--- without shim (control) ---
rename error: none
dest contents after operation: user-data
--- with shim (attack) ---
rename error: none
dest contents after operation: user-data     # attacker's "attacker-content" destroyed
```

Expected per contract: `already-exists` and the attacker file preserved.
Observed: success and silent data destruction. Exploitation requires a racing
writer in the destination directory, hence P2; the fix is
`renameat2(..., RENAME_NOREPLACE)` or equivalent.

### P3 — two

- P3-1. Renaming an item to its own unchanged name reaches the backend and
  fails with a user-facing `already-exists` failure card instead of being a
  no-op or a pre-dialog rejection (`MutationDialogs.qml:72-76`,
  `safe_path_operations.cpp:343-348`). Truthful but misleading presentation.
- P3-2. `HomeTrash::restore` returns `cross-device` when the restore
  destination's parent directory has vanished
  (`src/apps/file_manager/mutation/home_trash.cpp:284-290`,
  `deviceForPath` returns `nullopt`); `vanished` would be the truthful type.
  Still a typed failure, so precision only.

### Probed and found safe (no finding)

- Rename through a symlinked intermediate parent → `symlink-escape`, target
  untouched (attack a2). `..`-laden destinations resolve lexically
  (`QDir::cleanPath`) before the fd-pinned walk, and both the lexical lstat
  walk and `openAbsoluteDirectory` (`O_NOFOLLOW` per component) operate on the
  same cleaned path, so a symlink before `..` is never traversed; the a3 probe
  landed the payload at the lexically correct, root-contained destination with
  identity checks intact. Re-trashing an item already inside Trash →
  `invalid-request` (a4). Empty Trash unlinks planted symlinks rather than
  following them (product test + code: `removeEntryAt` uses
  `fstatat(AT_SYMLINK_NOFOLLOW)`/`unlinkat`, never recursing through links).
- Trash storage: `Trash`, `info/`, `files/` created owner-only `0700`;
  `.trashinfo` mode `0600`, exclusive create, `[Trash Info]` header,
  absolute percent-encoded `Path` (`%0A`/`%25` covered by product test),
  spec-format local `DeletionDate`; verified live on scratch fixtures
  (`stat`/`cat` of `scratch/attacks/fixture/a4/Trash`).
- Home redirection audit: unit tests inject fixture-local Trash roots
  (`QTemporaryDir`); the offscreen UI row sets `XDG_DATA_HOME` to the build
  dir and unsets display/session-bus; the installed-runtime row sets `HOME`,
  `XDG_CONFIG_HOME`, `XDG_CACHE_HOME`, `XDG_RUNTIME_DIR` to a private root
  (`tests/apps/file_manager/run_installed_file_manager.cmake:149-153`), and
  constructing the backend never touches the Trash path. No row can reach the
  reviewer's real home/Trash. My own runs additionally pointed `HOME`,
  `XDG_DATA_HOME`, and `TMPDIR` at scratch directories.
- Concurrency/durability: worker-thread execution proven by product test;
  cooperative cancellation removes partial copies (product tests cover
  pre-cancelled and mid-copy cancellation); changed/vanished identity produces
  typed `changed`/`vanished`; permission-denied create typed
  `permission-denied`; mode/mtime preservation applied best-effort and
  asserted by product test.
- Scope: `git diff --name-only d9aec19..61283bf` touches only owned paths
  (`src/apps/file_manager/**`, `tests/apps/file_manager/**`,
  `docs/wiki/apps/file-manager.md`, new ADR-0064) plus the lane-sanctioned
  additive shared edits (`mkdocs.yml`, ADR index, one `module-boundaries.md`
  row, testing-harness rows). No out-of-lane edits; no non-additive
  shared-registry changes.

## Commands executed (all from the candidate worktree unless noted)

- `git rev-parse HEAD` → `61283bf0...`; `git status --porcelain` empty before
  and after; tree/parent/base as in the header.
- Debug configure: `cmake -S . -B <ROOT>/debug -G Ninja -C
  /home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-665-initial-cache.cmake
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON -DQINDAQT_BUILD_KWIN_PLUGIN=ON
  -DQINDAQT_BUILD_SHELL=ON -DQINDAQT_BUILD_PRODUCTION_SHELL=ON
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF -DQINDAQT_ENABLE_STRICT_WARNINGS=ON`
  → exit 0.
- Release configure: same recipe with `-B <ROOT>/release
  -DCMAKE_BUILD_TYPE=Release` → exit 0.
- `cmake --build <ROOT>/debug --parallel 3 --target qindaqt-file-manager
  qindaqt_file_manager_history_tests qindaqt_file_manager_local_mutation_tests
  qindaqt_file_manager_home_trash_tests
  qindaqt_file_manager_mutation_controller_tests
  qindaqt_file_manager_action_catalog_tests
  qindaqt_file_manager_local_lister_tests
  qindaqt_file_manager_launch_intent_tests
  qindaqt_file_manager_controller_tests` → exit 0 (198 steps).
- Same target set for `<ROOT>/release` → exit 0 (198 steps).
- `env -u DISPLAY -u WAYLAND_DISPLAY -u DBUS_SESSION_BUS_ADDRESS
  DBUS_SYSTEM_BUS_ADDRESS=unix:path=/nonexistent HOME=<ROOT>/scratch/home
  XDG_DATA_HOME=<ROOT>/scratch/home/.local/share TMPDIR=<ROOT>/scratch
  QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software QT_FATAL_WARNINGS=1
  ctest --test-dir <ROOT>/debug -R '^qindaqt\.file-manager-'
  --output-on-failure --no-tests=error` → exit 0, **14/14 passed**.
- Identical selector against `<ROOT>/release` → exit 0, **14/14 passed**.
- `./tools/validate-docs` → exit 0 (128 Markdown documents plus navigation).
- `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs build --strict
  --site-dir <ROOT>/site` → exit 0.
- `./tools/check-source-shape` → exit 0 (2,065 files; only pre-existing
  out-of-lane review-threshold warnings, none in candidate paths).
- `git diff --check` → exit 0. No JSON files changed, so `json.tool` was not
  applicable.
- Scratch reproductions (sources under `<ROOT>/scratch/`, built against the
  candidate's Debug `libqindaqt_file_manager_support.a`):
  - `qml-roundtrip` → demonstrates P1-1 with a C++ control (output above).
  - `attacks` → a1 demonstrates P2-1; a2/a4 confirm refusals; a3 confirms
    lexical `..` handling stays contained.
  - `rename-race` (+ `LD_PRELOAD` shim) → demonstrates P2-2 (output above).
- Never run: `tests/session` rows, host buses, hardware, uinput, network.

## Verdict

The candidate's own gates pass in both profiles and the containment/durability
engineering underneath is real, but the S1 outcome — local mutation driven
from the action catalog and keyboard — does not function through the actual
QML UI (P1-1), and two bounded contract violations (P2-1, P2-2) remain.
ACCEPT requires P0 = P1 = P2 = 0.

VERDICT REJECT P0/P1/P2/P3=0/1/2/2
