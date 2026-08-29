# Rhea Calder — virtual desktop S0+S1 current-base candidate handoff

- Timestamp: 2026-08-28T11:46:56Z
- State: **handoff; no Rhea process is live**
- Candidate: `478435ef10024d3747d959f5bb198e60f9277c99`
- Tree: `a032cddcac22281d68735c1910501c4121101e12`
- First parent: public `main` `0a547df33d9a31b969d78b4ca649d0b39dc04797`
- Second parent: Dorian-accepted `f28f443b7aae2d635481f49e847a7e1e1a3b573b`
- Preserved reviewed source parent: `fd9faab5ab79017be903dafc6f0587d09c511f49`
- Branch: `worker/virtual-desktop-s0-s1-current-base`
- Worktree: `/home/cabewse/work_SPaC3/container-wm-workers/virtual-desktop-s0-s1`
- Manager alone integrates.

## Exact current-base scope

The commit is a non-destructive two-parent merge that preserves both reviewed
commits. Against its public first parent it contains exactly 23 paths,
3,628 insertions and 14 deletions, with sorted path-manifest SHA-256
`6d680f330e3bfca5135ce3a2d28eadd5d930163d70a3e7a747541f8270268eb6`.
All 20 reviewed virtual-desktop paths are byte-preserved or additively merged.
The only new paths beyond that accepted manifest are:

- `src/compositor/kwin/kwincontrolendpoint.cpp`
- `docs/wiki/architecture/compositor-session.md`
- `docs/wiki/reference/compositor-control-v1.md`

The KWin delta adds exactly `dock` to the development-only compositor surface-
evidence allowlist already gated by explicit isolated development mode. It
retains both Notification Live scopes and unrelated-surface rejection.
ADR-0026 records this narrow successor to ADR-0020. Notification Live, Display
D0/D1, Power, Controls, and Text Editor owned implementation paths compare
identically to the public first parent; shared session/docs/nav registries retain
both sides.

## Exact source-safe evidence

- `PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s tests/session -p 'test_desktop_session_*_unit.py'` — **37/37 pass**.
- `PYTHONPYCACHEPREFIX=build/virtual-desktop-current-base-python-cache python3 -m py_compile tests/session/desktop_session_*.py tests/session/test_desktop_session_*.py` — pass.
- `PYTHONDONTWRITEBYTECODE=1 python3 tests/compositor/test_dbus_contract.py compositor/dbus/org.qindaqt.Compositor1.xml compositor/dbus/service.json` — pass.
- `./tools/check-source-shape --largest 20` — 993 checked, zero skipped/issues; edited endpoint is 487 nonblank.
- `./tools/validate-docs` — 64 documents/navigation pass.
- Exact parent order, reviewed ancestry, manifest hash/count, whitespace,
  unchanged contract-owner paths, and clean worktree — pass.
- `mkdocs build --strict` — unavailable, exit 127 because `mkdocs` is not
  installed; the repository validator above is green.

An exploratory no-argument descriptor-check invocation exited 2 with usage
before the exact required-argument command passed. It executed no product or
runtime behavior.

## Deferred serial build and private boot

Display D2 still owns the compiler/private-runtime lane. No current-base CMake
configure, compile, package row, bubblewrap, bus, compositor, session, UI, or
boot row has run. After D2 posts terminal release and the manager explicitly
allocates the lane, use one fresh ignored root and serial commands:

```sh
cmake -S . -B build/virtual-desktop-current-base-debug \
  -DCMAKE_BUILD_TYPE=Debug -DQINDAQT_WARNINGS_AS_ERRORS=ON
cmake --build build/virtual-desktop-current-base-debug --parallel 1 \
  --target qindaqt-desktop-session-probe
ctest --test-dir build/virtual-desktop-current-base-debug --parallel 1 \
  --output-on-failure \
  -R '^desktop\.virtual\.(sandbox-unit|package-contract)$'
QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop \
ctest --test-dir build/virtual-desktop-current-base-debug --parallel 1 \
  --output-on-failure -R '^desktop\.virtual\.boot\.1080p$'
```

The boot claim requires the exact row to pass and one new regular
`tests/session/desktop-session-results/<32-lowercase-hex-run-id>` directory.
Its `.qindaqt-desktop-result` must bind the same build and run ID;
`result.json` must be schema 1 with matching `runId`, `outcome: success`,
`returnCode: 0`, `timedOut: false`, null failure, and ordered bounded
timestamps. `artifacts/desktop-session-evidence.json` must validate the exact
1920x1080 topology, simultaneous nonzero D0 generation, D1/service ownership,
Notification service, mapped/committed `dock` on `Virtual-1`, Settings and Text
Editor observed IDs, exact PSS ceiling, authenticated per-role terminal phases,
empty survivor PIDs, and fail-closed host display/bus/input fields. Every
process/probe log plus `artifacts/sandbox-command.json` and `sandbox.log` must
be preserved, the authenticated private run root must be absent, and no
QindaQt/private-bus/compositor descendant may survive.

## Requested action

Display D2/Kellan: post terminal compiler/private-runtime release when your
lane is empty. Manager: then allocate the sole lane to Rhea for the exact fresh
serial build/package/boot sequence above, or integrate only the source-safe
candidate boundary after independent current-base review. Do not treat this
handoff as a live-desktop qualification before that row and artifact inspection
pass.
