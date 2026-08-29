# Dorian Vale — current-base virtual-desktop exact review FAIL

- Timestamp: 2026-08-28T11:54:07Z
- Reviewer: Dorian Vale, independent KWin/nested-session auditor
- Verdict: **FAIL**
- Findings: **P0/P1/P2/P3 = 0/1/0/1**
- Exact candidate: `478435ef10024d3747d959f5bb198e60f9277c99`
- Exact tree: `a032cddcac22281d68735c1910501c4121101e12`
- Ordered parents: public `0a547df33d9a31b969d78b4ca649d0b39dc04797`,
  accepted repair `f28f443b7aae2d635481f49e847a7e1e1a3b573b`

## Immutable identity, merge and retention

The commit, tree and ordered parents match the exact handoff. Its first-parent
delta is exactly 23 paths, +3,628/-14, with recomputed sorted path-manifest
SHA-256 `6d680f330e3bfca5135ce3a2d28eadd5d930163d70a3e7a747541f8270268eb6`.
The detached review worktree and `git diff --check` are clean.

The accepted virtual-desktop manifest remains exactly 20 paths with SHA-256
`9dc4cae417408377abc7436fc602edc75fb3f6ee4204f777e187db5b43621a0c`.
Fifteen paths are blob-identical to accepted `f28f443b`. The five shared paths
(`ADR-0026`, ADR index, testing harness, MkDocs nav and session CMake) are
additive combined diffs retaining the accepted S0+S1 material and the public
Notification/Power/navigation/test material. Against the public first parent,
the only production implementation delta under the audited compositor,
services, shell, Controls and application boundaries is
`src/compositor/kwin/kwincontrolendpoint.cpp`; therefore public Notification,
D0, D1, Power, Controls and Text implementation contracts are retained.

The endpoint delta itself is narrow and consistent: it adds exact scope `dock`
to the existing development-only allowlist, remains behind the pre-inspection
`m_mutationsEnabled` rejection, retains `notification-popup` and
`notification-center`, and continues rejecting unrelated scopes
(`src/compositor/kwin/kwincontrolendpoint.cpp:224-276`). ADR-0026 accurately
identifies itself as a narrow additive successor to ADR-0020's original
two-notification-role evidence allowlist
(`docs/wiki/adr/0026-contain-virtual-desktop-qualification.md:94-99`).

## P1 — a foreign process can satisfy the production-dock proof

The compositor exports each surface's client `processId`
(`src/compositor/kwin/kwincontrolendpoint.cpp:260-269`). ADR-0026 requires a
mapped and committed **production** shell dock (`docs/wiki/adr/0026-contain-
virtual-desktop-qualification.md:60-66`), and `_build_evidence()` already has
the authenticated process topology/PID map
(`tests/session/desktop_session_runtime.py:168-201`). However,
`_validate_input_and_dock()` accepts a record based only on scope, output,
mapped and committed state; it never requires or binds `processId` to
`pids["shell"]` (`tests/session/desktop_session_topology.py:261-285`). The
positive fixture omits the field entirely
(`tests/session/test_desktop_session_topology_unit.py:67-75`).

The following safe reproduction passes on the immutable candidate:

```text
evidence = valid_evidence()
evidence['dockSurfaces'][0]['processId'] = '999999'
validate_boot_evidence(evidence)
REPRO: forged dock processId=999999 accepted as production dock
```

This allows the future private boot row to pass without proving that the
production shell owns the observed dock, so the row is not yet trustworthy.

### Exact repair acceptance

Create a non-amended descendant which:

1. carries authenticated process PIDs into final dock validation and requires
   every consumed `scope=dock` record's canonical decimal-string `processId` to
   equal `pids["shell"]`;
2. rejects missing, malformed and foreign dock PIDs, with focused negative
   tests for all three plus the shell-owned positive row;
3. preserves the readiness loop's bounded behavior, but does not let the final
   artifact validate until the ownership binding passes; and
4. replays the 37 focused units, Python syntax, descriptor, source/docs and
   clean-diff gates before exact rereview.

## P3 — the compositor reference summary still says notification-only

The detailed body correctly lists all three scopes, but the normative runtime
method table still describes `DevelopmentShellSurfaces` as only “notification
layer-surface evidence,” and the section remains titled “Development
notification-surface evidence”
(`docs/wiki/reference/compositor-control-v1.md:35-45,194-203`). The same small
descendant should make the summary/heading scope-neutral and add ADR-0026 beside
ADR-0020 at the decision link (`:215-220`). This is nonblocking by itself but is
documentation drift under the repository's same-change policy.

## Fresh safe evidence

- Desktop-session focused Python units: **37/37 pass**.
- Desktop-session `py_compile`: pass.
- Exact Compositor1 XML/service descriptor check: pass.
- Source shape: **993 files**, zero skipped/issues; edited endpoint 487
  nonblank lines.
- Documentation/navigation validation: **64 documents**, pass.
- Exact identity, manifests, combined merge inspection, diff check and clean
  worktree: pass.

No CMake configure/build, package row, bubblewrap, private bus, compositor,
session, display, input, UI, host configuration or hardware endpoint ran.

## Boot decision

This exact candidate is **not safe to advance to the private boot gate**:
although its containment source and allowlist addition are bounded, the boot
can currently produce a false-positive dock-ownership result. Repair and exact
source rereview must land first. After that repair passes and the manager
releases the serial compiler/private-runtime lane, the exact isolated 1080p
boot may proceed; no live-desktop maturity is claimed here.

