# Portal P0 exact candidate terminal verdict

- Reviewer: Frances Allen
- At: 2026-08-31T05:13:17-06:00
- Exact candidate: `b2271491239401adf1e4fffaed4fa57426e2a9ed`
- Exact tree: `20766f3658452c5e81473b7856d86717605e6624`
- Sole parent and exact merge base:
  `9b3d65542c87b2b977482ed7e72e4425c5332dd6`
- Detached review worktree:
  `/home/cabewse/work_SPaC3/container-wm-workers/portal-p0-allen-review`
- Verdict: **REJECT — do not integrate this SHA**
- Findings: **P0/P1/P2/P3 = 0/0/1/0**

## P2 — the claimed exact Settings-only hostile boundary accepts another installed portal

The committed `.portal` and selector bytes are narrow, but the executable gate
that claims to keep them exactly Settings-only is a partial denylist.
`tests/services/portal/check_boundary.cmake` rejects only FileChooser, OpenURI,
Notification, Inhibit, ScreenCast, and RemoteDesktop. The staged checker repeats
the same shape, and `check_boundary_negative.cmake` injects only OpenURI. The
installed xdg-desktop-portal 1.20.4 package exposes many other standard backend
interfaces, including `org.freedesktop.impl.portal.Background`.

Exact reproduction, performed only in a disposable copy:

1. Copy `src/services/portal` to
   `/tmp/qindaqt-portal-p0-allen-boundary-probe-1788172798`.
2. Change the copied `data/qindaqt.portal` line to:

   ```ini
   Interfaces=org.freedesktop.impl.portal.Settings;org.freedesktop.impl.portal.Background
   ```

3. Append this to the copied `data/qindaqt-portals.conf`:

   ```ini
   org.freedesktop.impl.portal.Background=qindaqt
   ```

4. Run:

   ```sh
   cmake \
     -DPORTAL_ROOT=/tmp/qindaqt-portal-p0-allen-boundary-probe-1788172798 \
     -P tests/services/portal/check_boundary.cmake
   ```

Actual result: exit 0 and
`Portal keeps appearance policy, Settings1 source, D-Bus transport, and packaging separated`.
Expected result: nonzero rejection because the module and ADR permit exactly
`org.freedesktop.impl.portal.Settings` and no other portal interface. The
poison interface is locally proven installed at
`/usr/share/dbus-1/interfaces/org.freedesktop.impl.portal.Background.xml`.

This is P2 rather than P1 because the exact candidate metadata remains
Settings-only and the production object registers no Background implementation;
the defect is in a claimed regression/hostile acceptance boundary. It still
blocks integration because the handoff explicitly claims effective source and
installed hostile poison proof.

Required repair: in Karen Spärck Jones's same isolated worktree, create a
non-amended descendant that parses or otherwise enforces the `.portal`
`Interfaces` value as exactly the singleton Settings interface and rejects
every `org.freedesktop.impl.portal.*=qindaqt` selector except Settings. Add a
representative installed 1.20.4 interface such as Background to both source
negative and staged-installed poison coverage. Return the exact descendant for
Frances's rereview; do not replace this verdict with prose.

## Passing evidence

- Installed contract: local `xdg-desktop-portal.pc` reports 1.20.4. Its
  installed backend XML exactly matches candidate `ReadAll(as) -> a{sa{sv}}`,
  `Read(s,s) -> v`, `SettingChanged(s,s,v)`, and read-only `version:u`.
- Static protocol/source audit: standard name/path/interface/version are exact;
  `color-scheme` and `contrast` are `u`, accent is `(ddd)` finite opaque sRGB;
  only the four documented Settings1 keys are scoped. Exact owner replacement,
  epoch/revision validation, old-owner reply retirement, malformed/unknown
  withdrawal, and no-loss-signal behavior follow the public SettingsClient and
  QST contracts. No private protocol, persistence, consent, chooser, OpenURI,
  notification, inhibit, screencast, remote-desktop, shell, compositor, QML,
  or platform adapter authority is present in the committed product paths.
- Strict Debug exact-target command completed exit 0 for
  `qindaqt_portal_appearance`, `qindaqt_portal_service`, all four portal test
  executables, and their explicit resident dependencies. With host display,
  Wayland, and session-bus variables removed, the serial
  `^qindaqt\.portal-` selector passed **7/7**.
- Fresh strict Release configure and the same exact-target build completed exit
  0; the same contained serial selector passed **7/7**.
- Both selectors covered policy, Settings1 source, standard private-D-Bus
  object, two-daemon process activation/loss/restart, source boundary, negative
  self-proof, and staged package/private lifecycle. No host portal or session
  bus was called.
- A retained Release `QindaQtPortalP0` stage contains exactly 17 files: the
  executable, two static libraries, five public headers, five themes, and four
  activation/unit/portal/selector metadata files. Metadata contains no build,
  source, temporary, or unresolved template path. The executable's direct
  dependencies are Qt Core/Gui/DBus plus ordinary GL/C++ runtime libraries;
  there is no Qt Quick, Wayland, shell, compositor, or non-Settings portal
  dependency.
- `./tools/validate-docs`: exit 0, 113 Markdown documents and navigation.
- pinned MkDocs 1.6.1 `build --strict`: exit 0.
- `./tools/check-source-shape`: exit 0 across 1,684 files. Its three warnings
  are pre-existing files outside the candidate paths.
- Exact identity, 37-path diff, sole parent, merge base, and `git diff --check`
  passed. Candidate and implementer worktrees are clean. No exact Debug/Release
  portal or Settings fixture process and no test private daemon remains.

## Non-evidence and integration caveat

An accidental all-target Debug build was interrupted with exit 130 and is not
acceptance evidence; only the subsequently completed exact-target command and
seven-row selector are cited. An initial discarded configure root lacked the
repository's private KDecoration3 test stub; both cited strict roots use the
same isolated stub as the candidate handoff.

Against manager HEAD `64813ef9ff05fd0110f093094630ee1567bae507`,
`git merge-tree --write-tree --messages` reports content conflicts only in
`docs/wiki/adr/index.md` and `mkdocs.yml`; other shared additions merge. These
are concurrent additive navigation/registry coordination points, not a
candidate severity finding. After the product/test repair is accepted, the
manager must resolve both additively and rerun integrated gates.
