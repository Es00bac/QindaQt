# Rhea Calder — virtual desktop private-boot qualification claim

- Timestamp: 2026-08-28T12:21:43Z
- Accepted immutable candidate: `dc377388af530411c3c281cb0171ccfc74590b0e`
- Tree: `3d703cde297a10b5c0dfc4b6ff1009240fa2ee45`
- Parent: `478435ef10024d3747d959f5bb198e60f9277c99`
- Independent source-safe review: Dorian PASS `1787919495`, P0/P1/P2/P3 = 0/0/0/0

The manager has released and assigned the sole compiler/private-runtime lane to
Rhea after Anika's bounded serial command finished. Process inspection finds no
active CMake build, Ninja, CTest, or private QindaQt session. Current headroom is
14 GiB available RAM on 24 CPUs with 45/47 GiB swap used, so every build and
test will remain serial.

The path named in the assignment,
`/home/cabewse/work_SPaC3/container-wm-workers/virtual-desktop-s0-s1-current-base`,
does not exist. The exact clean accepted candidate is preserved at
`/home/cabewse/work_SPaC3/container-wm-workers/virtual-desktop-s0-s1`; no product
edit or commit mutation is authorized.

Qualification will use one fresh worktree-local Debug build root, run the two
documented source-safe rows, and then execute only:

```sh
QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop \
ctest --test-dir <fresh-build-root> --parallel 1 --output-on-failure \
  -R '^desktop\.virtual\.boot\.1080p$'
```

The nested harness owns a disposable empty root plus private HOME/XDG/runtime,
PID/network/IPC/UTS/user namespaces, Wayland socket, and session bus. Host
Wayland/display, session bus, HOME/config, seat, pointer/input/uinput, render,
PipeWire, network, hardware, and current host KWin/bwrap processes are not
targets. The accepted S0+S1 contract explicitly makes no screenshot claim, so
the required screenshot count is zero; required live artifacts are the fresh
authenticated result/evidence JSON, every role log, exact PSS schema/ceiling,
and cleanup ledger with no owned survivor or run-root residue. Devika remains
source-only until Rhea posts terminal release.
