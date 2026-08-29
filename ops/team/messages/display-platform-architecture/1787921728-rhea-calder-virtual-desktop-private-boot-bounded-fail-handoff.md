# Rhea Calder — virtual desktop private-boot bounded-FAIL handoff

- Timestamp: 2026-08-28T12:55:28Z
- Verdict: **bounded FAIL**
- Findings: **P0/P1/P2/P3 = 0/2/1/0**
- Reviewed starting candidate: `dc377388af530411c3c281cb0171ccfc74590b0e`
- Exact repair descendant: `e325f2f1e8d69d2d6e3eaa42c04df0f71d2265c7`
- Tree: `ca722256cd0dbd353ae264a571ce6d5e2171168b`
- Parent: `3320afdb4afad1c396b85add576f60d59e1d3b57`
- Repair chain: `dc377388` -> `e2ab439c` -> `3320afdb` -> `e325f2f1`
- Descendant scope: 8 paths, +458/-129
- Sorted repair-path manifest SHA-256: `f03ec9cd186bda6fc255275948f3953be3f7c92166620159e33b344bdb4c6fb7`
- Fresh build root: `build/virtual-desktop-private-1787919703`
- Final run ID: `26e772f23f519434ce445dca4ff51128`

## Exact commands and source-safe result

```sh
cmake -S . -B build/virtual-desktop-private-1787919703 -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=ON \
  -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON \
  -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF \
  -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake --build build/virtual-desktop-private-1787919703 --parallel 1 \
  --target qindaqt-desktop-session-probe
ctest --test-dir build/virtual-desktop-private-1787919703 --parallel 1 \
  --output-on-failure \
  -R '^desktop\.virtual\.(sandbox-unit|package-contract)$'
QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop \
ctest --test-dir build/virtual-desktop-private-1787919703 --parallel 1 \
  --output-on-failure -R '^desktop\.virtual\.boot\.1080p$'
```

- configure: PASS;
- exact production target graph: **503/503** serial build steps PASS;
- current exact-tree safe CTest: **2/2 PASS**;
- full desktop-session Python units: **48/48 PASS**;
- focused readiness units: **3/3 PASS**;
- twelve Python sources compiled in memory: PASS;
- docs/navigation: **64 PASS**;
- source shape: **996**, zero warnings/issues;
- whitespace and clean tree: PASS.

The live lane exposed and preserved three fail-closed construction/reporting
defects as non-amended commits: merged-usr ELF loader aliases (`e2ab439c`), a
fresh authenticated private UID/GID database (`3320afdb`), and fixed-lifetime
probe/archive/last-pending behavior (`e325f2f1`). Each repair's exact safe gates
passed before its private rerun.

## Final exact runtime evidence

The final package fixture passed. `desktop.virtual.boot.1080p` executed **51**
full public-topology probes and failed after 14.53 seconds with:

```text
desktop topology readiness timed out: mapped test application was missing:
org.qindaqt.Settings; no complete probe lifetime remains
```

The sentinel-authenticated result root contains **61 regular files**, no
symlinks: result/command/sandbox metadata plus **57 process logs**, including
all **51 exact probe stdout snapshots**. The last snapshot proves:

- all four required D-Bus names owned: Compositor, Settings1, Audio1, and
  Notifications;
- one exact 1920x1080 scale-1 virtual output and equal nonzero generations
  `1/1`, but the production name is `Virtual-0`;
- exactly one combined `QindaQt Development Input`;
- two mapped, committed `scope=dock` surfaces on `Virtual-0`, both carrying
  canonical PID string `76`;
- Text Editor is correctly observed as `org.qindaqt.TextEditor` with a nonempty
  window ID and title `Untitled — QindaQt Text Editor`;
- Settings is mapped with a nonempty window ID and the correct visible title
  `QindaQt Settings — Notifications`, but its compositor-observed application
  ID is `qindaqt-settings`, not the required `org.qindaqt.Settings`.

P1 blockers are therefore exact and independent: the Settings application-ID
contract disagrees with the production executable, and the deterministic
topology expects `Virtual-1` while the pinned KWin virtual backend publishes
`Virtual-0`. The validator stops on the first mismatch, but every archived
snapshot independently preserves both facts.

The row did not reach final evidence construction, so there is no
`desktop-session-evidence.json`, `residentPssKiB`, 1,048,576-KiB ceiling result,
or authenticated per-role terminal-phase ledger. This is the bounded P2
evidence limitation; absence must not be upgraded into a PSS or graceful/role-
cleanup claim. Required screenshot count is **0** by the normative S0+S1
contract, and artifact audit also finds zero images.

## Containment and teardown

The archived command proves user/PID/network/IPC/UTS namespaces,
parent-death/new-session behavior, private runtime/HOME/XDG/bus/Wayland roots,
the two exact merged-usr aliases, and synthetic private passwd/group files.
Actual environment intersection with the forbidden-key set is empty. There are
zero host Wayland/session/input/uinput/HOME/root mounts; the evidence flags for
host input, host runtime, and render node are all false.

After the terminal row:

- authenticated result sentinel: PASS;
- result symlinks: **0**;
- private run-root children: **0**;
- run-ID/socket/stage `/opt/qindaqt` survivor processes: **0**;
- host KWin PIDs remain the pre-existing **5465/5470** and were untouched;
- worktree is clean at the exact descendant.

## Requested next action

The compiler/private-runtime lane is released. Manager: preserve this exact
descendant and assign independent source review of the three repair commits.
Route the observed `qindaqt-settings` versus `org.qindaqt.Settings` contract to
the Settings application owner, and `Virtual-0` versus `Virtual-1` to D0/KWin
architecture. Do not rerun or claim the boot row until both exact contracts are
resolved on a reviewed descendant. Manager alone integrates.
