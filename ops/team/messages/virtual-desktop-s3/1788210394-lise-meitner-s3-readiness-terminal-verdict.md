# Lise Meitner: exact S3 shell-readiness terminal verdict

- Timestamp: 2026-08-31T15:06:34-06:00
- Reviewer: Lise Meitner
- Verdict: **STATIC+DYNAMIC ACCEPT**
- Findings: **P0 0 / P1 0 / P2 0 / P3 0** in this independent review
- Exact candidate: `4d4b3dc395d80135a8f66f5ffcb2395cf1874c60`
- Exact tree: `86dbb17efba5c619c5aeb33027264b46a09fb7b9`
- Sole parent: `39c83a23143f36c3bfec8686b7cbc0121ea8a418`
- Manager boundary/merge base: `01145dcd5886861657a348b3e5b18a75fa7c6307`
- Detached review worktree:
  `/home/cabewse/work_SPaC3/container-wm-workers/virtual-desktop-s3-readiness-review`
- Fresh build root: `/tmp/qindaqt-s3-meitner-readiness-build` (preserved)
- Serialized lane: released; no live process

## Static and provenance review

Commit, full tree, sole parent, and manager ancestry were independently
verified. The handoff matches the exact 20-path `+2318/-40` delta and
`git diff --check HEAD^ HEAD` exits 0. Manual review found no P0-P3. The exact
`qindaqt-shell` / `qindaqt_toggle_notification_center` binding is qualified by
resolved active component state at the final pre-input boundary; an observer is
installed before the sole input batch. A unique-owner-bound ShellDevelopment
sample proves the dock-owning PID, private presentation, closed/hidden created
center, selected output, and stable owner immediately before input. Acceptance
then joins exact ordered activation, the same shell owner/PID with increased
open counter and visible selected-output center, and one compositor-owned
active surface. No sleep, warm-up input, direct action call, or input retry is
present. Python canonicalization fails closed on owner/PID/counter/envelope/
output contradictions and preserves the two shell phases plus activation in
the canonical interaction evidence.

Before authorization, a read-only audit of Dorothy Vaughan's preserved
archives independently confirmed the reported 4/4 activation/shell/surface,
12/12 unreachable host endpoints, PSS/capture/teardown evidence, successful
former-red 1080p@150% runs, and exact dual authority. No executable gate was
started before the Program Manager assigned the serialized lane.

## Independent build and registered gates

Fresh configure PID `720296` exited 0. Cache-selected paths were exact: Debug,
install prefix `/usr`, strict warnings ON, host uinput OFF; Qt6 and Qt tools
under `/usr/lib64/cmake`, Wayland scanner `/usr/bin/wayland-scanner`, isolated
KF6 CoreAddons/GlobalAccel under `/tmp/qindaqt-kf6-prefix`, and private
KWin/KDecoration plus runtime tools under `/tmp/qindaqt-arch-665/root/usr`.
There was no global `LD_LIBRARY_PATH`; only the previously known mixed-prefix
RPATH warnings appeared.

Strict serial build PID `720938` exited 0 with 882/882 actions for:

- `qindaqt-desktop-session-probe`
- `qindaqt-desktop-notification-binding-tests`
- `qindaqt-desktop-notification-shell-readiness-tests`

The exact registered focused selector passed 5/5 in 1.68 seconds:

- `session.python-syntax`
- `desktop.virtual.sandbox-unit` (111 Python units)
- `desktop.virtual.notification-binding-unit`
- `desktop.virtual.notification-shell-readiness-unit`
- `desktop.virtual.interaction-probe-cli-unit`

The lane-gated former-red 1080p@150% plus package invocation passed 2/2 in
8.13 seconds. Its fresh run is
`64e81d6f111397493a61024b792e9f77` (row 7.88 seconds).

The one authorized package-plus-four-row matrix invocation passed 5/5 in
33.61 seconds:

- WUXGA `e164f5f9503556702f671dba61524f76` (6.35 seconds)
- 1440p@125% `b8ab10ade9ef6fc489d81a993653dc66` (12.62 seconds)
- 1080p@150% `9d22faa53010ad6536b8cf9745488b21` (7.86 seconds)
- dual 1080p `e2ac6220449980965f4fe026d2c77334` (6.51 seconds)

All runtime invocations used
`QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop`, `--parallel 1`,
`--stop-on-failure`, and `--output-on-failure`. There was no retry.

## Fresh archive authentication

For all four matrix rows, the archived activation and interaction stdout
markers each occur exactly once and byte-decode to the canonical interaction
object. Every row proves exact component/action pressed then released, zero
pre-input active centers, stable unique shell owner, D-Bus/snapshot/dock PID
equality, closed/hidden before, open/visible with increased counter after, and
one active mapped/committed compositor surface on the same desired/actual
output. This is 4/4 canonical activation/shell/surface; the separate gated
1080p@150% run proves the same contract.

Matrix host display/input/session-bus reachability is false 12/12. Resident PSS
is respectively 184157, 181376, 174795, and 233471 KiB, each below the
1048576 KiB ceiling. The separate gated 1080p@150% run is 168909 KiB.

On-disk PNG byte counts, SHA-256 values, and header dimensions match their
canonical records. Independent full-image/content-region pixel enumeration and
visual inspection prove nontrivial open notification-center captures:

- WUXGA 1920x1200, SHA-256
  `737e9b273a1b624e848f9c96421df4720b82f69a00c85377572de70465ffd26b`,
  1615/593 actual colors;
- 1440p@125% 2560x1440, SHA-256
  `8cda1bc142d075f645d944a7e5b002719a3955c8df6c68eeec277cb2be9f2eaa`,
  1614/325 colors;
- 1080p@150% 1920x1080, SHA-256
  `e159ad5d05835dcc44b3db1f1bb4db0dbe76b1da9981f088de0199bd50a67c6f`,
  1559/491 colors;
- dual WL-1 1920x1080, SHA-256
  `4a97ab3eeea0e32b8f94bcba2204a54e8858a376e704e74ab2096e3c0ad9b8c9`,
  752/593 colors.

Cleanup is bounded 4/4, terminal roles are fully accounted for, and survivor
lists are empty 4/4. Dual canonical post-selector authority is exactly
`[WL-1 priority 1, WL-0 priority 2]`; interaction and capture are both WL-1.

Final process inspection found no KWin, Weston, nested driver, or QindaQt
session process. The runtime-created `tests/session/__pycache__` was moved to
trash after exact inspection; no repository Python cache remains. The stage
contains no symlink residue. Worktree status contains only this reviewer's
allowed live-board record; excluding it, `git diff --name-only HEAD` is empty.
Candidate product bytes remain exact and immutable.

## Next action

The exact candidate is accepted for Program Manager integration and
manager-tree replay. The serialized private-runtime lane is released.
