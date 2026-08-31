---
from: frances-allen
to: karen-sparck-jones, program-manager
topic: portal-p0
at: 2026-08-31T06:05:35-06:00
---

# Portal P0 repaired exact-candidate terminal verdict

- Reviewer: Frances Allen
- Exact candidate: `9f59a77aee9cbfd2f4f541b136ecd611e0cda798`
- Exact tree: `50aa85cf07c83e0271c2e6679db0a61ab3bfdadf`
- Sole parent: rejected immutable candidate
  `b2271491239401adf1e4fffaed4fa57426e2a9ed`
- Original assigned base and exact merge base:
  `9b3d65542c87b2b977482ed7e72e4425c5332dd6`
- Detached review worktree:
  `/home/cabewse/work_SPaC3/container-wm-workers/portal-p0-allen-repair-review`
- Verdict: **ACCEPT — integrate this exact SHA**
- Findings: **P0/P1/P2/P3 = 0/0/0/0**
- Resource state: serialized compiler, CTest, and private-bus lane **released**
  at this terminal verdict.

## Repair conclusion

The repair closes the rejected candidate's blocking P2. The boundary checker
now compares the complete `.portal` file against the exact Settings-only bytes
and the complete selector against the exact Settings selection. Missing files,
duplicates, reordered/extended content, or any additional portal family fail
closed rather than depending on a partial family denylist.

Frances independently copied the candidate portal source into
`/tmp/qindaqt-portal-p0-allen-repair-background-probe-1788177167`, added the
installed `org.freedesktop.impl.portal.Background` family to both metadata
files, and ran the repaired checker. It exited 1 at the exact singleton
`.portal` comparison. The candidate negative self-proof separately rejects
OpenURI, installed xdg-desktop-portal 1.20.4 Background, and duplicate Settings
entries in each `.portal` and selector artifact while the unmodified files
pass.

The Debug and Release staged-package rows each install the exact package into
their build-local prefix, accept its complete metadata, independently add
Background to the installed `.portal` and selector and require each rejection,
restore both originals, and still require rejection of an installed private
header. A retained Release stage also passed exact source-byte comparison; an
independent copy of both installed metadata files with Background added exited
1 at the complete-byte checker. The installed Background XML is present in the
locally pinned 1.20.4 dependency.

## Changed-path and boundary audit

The repaired descendant changes exactly:

- `tests/services/portal/check_boundary.cmake`
- `tests/services/portal/check_boundary_negative.cmake`
- `tests/services/portal/run_staged_package.cmake`
- `docs/wiki/architecture/portal-service.md`
- `docs/wiki/development/testing-harness.md`

Production runtime, public API, activation metadata, package inputs, and the
standard protocol implementation are byte-identical to rejected parent
`b2271491239401adf1e4fffaed4fa57426e2a9ed`; that parent was rejected only for
the hostile proof gap. The rereview nevertheless rechecked the endpoint and
lineage boundary: the process owns only the standard backend name, object,
Settings interface and read-only version 1 property; `ReadAll`, `Read`, and
`SettingChanged` use the installed signatures; only three documented
appearance values are exported; exact Settings1 owner/epoch loss withdraws
readable truth; retired replies cannot republish it; startup rolls back object
or name acquisition failures; and bus loss terminates rather than reconnecting
an old comparison lineage. The public headers state source/service ownership,
lifetime, and thread confinement. No Settings persistence internals, QML,
shell, compositor, chooser, OpenURI, other portal authority, or host mutation
crosses the module boundary.

## Independent executable evidence

- Direct exact-source boundary check: exit 0.
- Direct negative mutation self-proof: exit 0 after proving rejection of all
  nine injected policy/presentation/product/metadata poisons, including
  OpenURI, Background, and duplicate Settings mutations in both metadata
  files.
- Fresh strict GCC 15.3 Debug configure and exact portal production/test target
  build: exit 0, **97/97 Ninja edges**.
- Debug contained serial `^qindaqt\.portal-` selector with host display,
  Wayland, and session-bus variables removed: exit 0, **7/7**.
- Fresh strict GCC 15.3 Release configure and identical exact target build:
  exit 0, **97/97 Ninja edges**.
- Release identical contained serial selector: exit 0, **7/7**.
- `./tools/validate-docs`: exit 0, **113 Markdown documents** and navigation.
- pinned MkDocs 1.6.1 `build --strict`: exit 0.
- `./tools/check-source-shape --largest 20`: exit 0 across **1,684 files**.
  Its three warnings are pre-existing files outside all candidate paths.
- Retained Release `QindaQtPortalP0` stage: exactly **17 files** comprising the
  executable, two static libraries, five public headers, five themes, and four
  activation/unit/portal/selector metadata files. The executable links Qt
  Core/Gui/DBus and ordinary graphics/C++ runtime libraries, with no Qt Quick,
  QML, Wayland, KWin, or Plasma dependency. No staged text contains a template,
  source, build, or reviewer path.
- Exact SHA/tree/sole-parent/two-commit original-base lineage, both range
  `git diff --check` gates, five-path repair scope, source/staged cleanup,
  exact portal/Settings fixture-process and private-daemon residue, candidate
  worktree tracked cleanliness, and implementer worktree tracked cleanliness:
  pass.

One post-install inspection command initially assumed `libexec` directly under
the stage and exited 1 before running dependency checks; the configured
destination is `lib64/libexec`. Repeating the inspection against that exact
installed path passed and is the evidence cited above. This was a reviewer
path assumption, not a candidate or package failure.

## Bounded caveats and requested action

The accepted scope is the repository's standard Settings v1 appearance
backend. It deliberately does not prove host portal selection, installed-host
package state, toolkit reaction, a real user session bus, or any non-Settings
portal family, and no such state was contacted or modified.

Against current manager HEAD
`ab203cac213b4bff882151de2398b4c1b46c99cb`, an exact merge-tree reports only
the expected additive content conflicts in `docs/wiki/adr/index.md` and
`mkdocs.yml`; the other shared additions merge. The Program Manager should
integrate exact candidate `9f59a77aee9cbfd2f4f541b136ecd611e0cda798`,
resolve both navigation/registry files additively, rerun the seven contained
portal rows plus documentation/shape/provenance checks on the combined tree,
and then reconcile Portal P0 product evidence. No implementer repair is
requested.
