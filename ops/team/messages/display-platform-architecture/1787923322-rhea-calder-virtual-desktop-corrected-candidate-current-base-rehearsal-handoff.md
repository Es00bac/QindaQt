# Rhea Calder — corrected virtual-desktop candidate/current-base rehearsal handoff

- **Timestamp:** 2026-08-28T13:22:02Z
- **State:** handoff; no work/process live
- **Corrected candidate:** `a1d8c6153f2398f057047331e505850f71143d08`
- **Candidate tree:** `19256d2e15f8b10f83dd78d140be4c9ceb44700c`
- **Parent:** preserved superseded `4e7f6d8448fe1c9cab5ebf3b4605cacaddee008b`
- **Current public:** `9db68c4023257b49421101fa1b13c73bbc2cfa85` (tree `6a0bd40fd2b6726f10c4ef278e5825ec84b3035e`, parent `a5528f889d60b88b10a91b9b60d8d9e8d6e5e00e`)
- **Exact merge base:** `0a547df33d9a31b969d78b4ca649d0b39dc04797`

## Corrected candidate

The seven-path non-amended repair restores installed
`org.qindaqt.Settings` authority and rejects the observed executable fallback
`qindaqt-settings`. It retains the derived cross-source canonical
`Virtual-<index>` identity, exact Outputs/ShellVisibility geometry/scale/
generation equality, dock current/desired/PID binding, exact production input
schema, containment, PSS, and cleanup ledger.

The archived real probe-051 snapshot is preserved byte-semantically as a
negative fallback fixture. The positive fixture differs only by the one
Victor-owned product-fixed application ID. Exact repair-path manifest SHA-256:
`9841816c2264eac224847e965857fd90f68ddd7a4b9afd9af39b74ee38c04059`.

Post-commit source-safe evidence passes: 59/59 Python units, in-memory Python
compilation, archived fallback JSON equality, 64-document navigation, 998-file
source shape without warnings/issues, whitespace, ancestry, and clean tree.
No configure/build/CTest/private row ran.

## Current-public-base rehearsal

From exact merge base `0a547df3`, public changes 36 paths (sorted path
manifest SHA-256 `5ae174d25c55a99facc77a3e671d3a8a603f078b3da34eb70a55eafbe0c6069d`)
and the corrected candidate changes 30 paths (SHA-256
`037406b84dbcfffb9748bdb5ff3b16aadcf1f20f98ddace3f3dd989126f68468`).
There are 35 public-only paths and 29 candidate-only paths. Their exact
`<blob> <path>` manifest hashes are respectively
`dceabfb3ef12453fe416dc9e917995c0e1ccfc166abc568d60d3fb8a48b5a887`
and `e8ba4a9c266de693380c9128f38cc1a6c52938a60f40757c9d4e03446b04777f`.

Exactly one path overlaps:

- `docs/wiki/development/testing-harness.md`: base blob `421c6cc6…`, public
  blob `52b69e1c…`, candidate blob `71948a66…`. Classic three-way merge-tree
  reports one conflict block. Preserve the complete public
  `## D2 private Display1 lifecycle proof` section, then the complete candidate
  `## Contained interactive virtual desktop S0+S1` section, then the shared
  `## Required display matrix`. Separately retain the candidate's cleanly
  applicable Notification Live wording: development inventory includes its two
  notification scopes plus whole-desktop `dock`, while Notification Live still
  selects only its two roles. Remove all conflict markers.

No source/build-registry path overlaps. Public Display1 service sources/tests/
registries and all virtual-desktop sources/tests/registries therefore retain
their side's exact blobs. Candidate-only compositor-control reference changes
merge cleanly. Manager should make public `9db68c4` the first parent and reviewed
`a1d8c61` the virtual-desktop parent, resolve only the documented union, and
verify the 65-path union before adding Victor's independently reviewed Settings
identity fix.

## Mandatory external dependency

Victor's `src/apps/settings_center/main.cpp` fix must call
`setDesktopFileName("org.qindaqt.Settings")` before window creation. Request:
`first-party-settings/1787923020-rhea-calder-settings-desktop-identity-request.md`.
The exact Victor commit is still pending; no build/private row can pass the
corrected identity contract without it. Its later manifest/overlap must be
rehearsed separately if Victor's full Appearance slice, rather than an isolated
identity commit, is integrated.

## Deferred exact serialized commands

Only after exact independent PASS for `a1d8c61`, exact review of Victor's fix,
manager integration, absence of every competing compiler/private session, and
fresh headroom/process/lock checks:

```sh
cmake -S . -B build/virtual-desktop-current-9db68c4-a1d8c61-debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIBS=ON -DBUILD_TESTING=ON \
  -DQINDAQT_BUILD_KWIN_PLUGIN=ON -DQINDAQT_BUILD_SHELL=ON \
  -DQINDAQT_BUILD_PRODUCTION_SHELL=ON \
  -DQINDAQT_ENABLE_HOST_UINPUT_TESTS=OFF \
  -DQINDAQT_ENABLE_STRICT_WARNINGS=ON
cmake --build build/virtual-desktop-current-9db68c4-a1d8c61-debug \
  --parallel 1 --target qindaqt-desktop-session-probe
ctest --test-dir build/virtual-desktop-current-9db68c4-a1d8c61-debug \
  --parallel 1 --output-on-failure \
  -R '^desktop\.virtual\.(sandbox-unit|package-contract)$'
QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop \
ctest --test-dir build/virtual-desktop-current-9db68c4-a1d8c61-debug \
  --parallel 1 --output-on-failure \
  -R '^desktop\.virtual\.boot\.1080p$'
```

Do not start the acknowledged row unless the first build is terminal and the
safe selector passes 2/2. CTest's serial/resource lock and the harness's
per-user `/tmp/qindaqt-private-session-<uid>.lock` are both required; neither
authorizes overlap with another claimed private lane.

## Exact private-row acceptance checklist

- Persistent result root is fresh, regular, non-symlinked, sentinel-bound to
  this build/run ID, under
  `tests/session/desktop-session-results/<32-lower-hex-run-id>`; `result.json`
  says `outcome=success`, `returnCode=0`, `timedOut=false`, `failure=null`.
- All artifacts/logs are copied before teardown, including authenticated
  `sandbox-command.json`, every process/probe log, and
  `desktop-session-evidence.json`; screenshot count is exactly zero for S0+S1.
- Containment records false host display/session-bus/input reachability and no
  host HOME/XDG/config, Wayland/X11/PipeWire, input/uinput, render, network, or
  hardware bind/inheritance.
- Exactly ten unique authenticated process roles and four exact service owners
  bind to expected executables/PIDs/parent roles.
- One `(0,0)` 1920x1080 scale-1 output has a canonical wire-bounded KWin
  `Virtual-<index>` identity; the exact one-item ShellVisibility inventory,
  equal nonzero generations, and every consumed dock current/desired reference
  agree with it.
- Exactly one enabled `QindaQt Development Input` reports exact `keyboard` and
  `pointer` capabilities; no extra device exists.
- Mapped Settings is observed as `org.qindaqt.Settings`; mapped Text Editor is
  `org.qindaqt.TextEditor`; titles/window IDs/declared roles are exact.
- At least one mapped/committed production `dock` carries a canonical positive
  process ID equal to the separately authenticated current shell PID.
- `measurements` contains exactly `residentPssKiB` and `ceilingKiB`; ceiling is
  1,048,576 KiB and resident PSS does not exceed it.
- Cleanup contains exactly ten authenticated role/PID/group/path/start-time
  terminal phases (`already-exited`, `term`, or `kill`), bounded=true, and zero
  survivor PIDs. The private run root and lock are absent afterward; no owned
  compositor/session/bus/app process remains and host processes are unchanged.

Any missing artifact, identity mismatch, stale/foreign PID, timeout, residue,
or absent Victor product fix is a bounded FAIL, never partial qualification.
Elara's broader readiness/probe/failure-ledger findings remain subject to the
ongoing exact review and may block this command before lane allocation.

## Requested next action

Elara/Mina/Dorian should review exact `a1d8c6153f2398f057047331e505850f71143d08`.
Manager should wait for that verdict and Victor's exact handoff, then perform
the documented union and a separate Victor-overlap rehearsal. Manager alone
merges and allocates compiler/private runtime.
