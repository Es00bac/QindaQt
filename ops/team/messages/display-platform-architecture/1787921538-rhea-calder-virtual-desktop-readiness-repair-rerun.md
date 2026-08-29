# Rhea Calder — readiness repair and single exact rerun

- Timestamp: 2026-08-28T12:52:18Z
- Exact HEAD: `e325f2f1e8d69d2d6e3eaa42c04df0f71d2265c7`
- Tree: `ca722256cd0dbd353ae264a571ce6d5e2171168b`
- Parent: private-identity repair `3320afdb4afad1c396b85add576f60d59e1d3b57`

Every probe now receives a fixed one-second lifetime only if that full interval
fits inside the unchanged 15-second readiness cap. Every consumed stdout
snapshot is flushed into its authenticated attempt log before validation, and
a probe/outer deadline reports the retained last pending observation. The
cohesive readiness policy is a 160-nonblank-line module and the orchestration
runner is reduced to 278 lines.

Safe exact-tree evidence passes:

- hostile readiness units: **3/3**;
- complete desktop-session units: **48/48**;
- twelve Python sources compiled in memory;
- safe CTest sandbox/package rows: **2/2**;
- docs/navigation: **64**;
- source shape: **996**, zero warnings/issues;
- whitespace and clean tree.

There is no competing compiler/private process and 12 GiB RAM is available.
Rhea is starting the manager-authorized single exact 1080p rerun. After it is
terminal, Rhea will audit the authenticated artifacts, PSS if present,
containment, every log, owned survivors, and run-root cleanup, then release the
lane regardless of PASS or bounded FAIL.
