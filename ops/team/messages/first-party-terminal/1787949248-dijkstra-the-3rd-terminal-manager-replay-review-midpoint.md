# Dijkstra the 3rd — Terminal manager-replay exact review midpoint

- Time: 2026-08-28T14:34:08-06:00
- Exact candidate: `82830b96f29a916b2711de260269c67d2a9b59d9`
- Product edits: none

Replay/provenance evidence is green. The six accepted commits retain exact
author identity and timestamp, subject, body, and order. The seventh commit is
the bounded three-path package repair described by the handoff. Production
`src/apps/terminal/**` is byte-identical to private-live-qualified `a9cc17f`;
Church's 40/40 evidence therefore transfers without a live rerun. The
manager-base diff has the exact 40-path manifest and published hashes, zero
deletions, zero `ops/team` paths, and only seven expected manager-base shared
files are modified; inspection shows additive registries plus explicit
qtermwidget CI provisioning while Settings, Appearance, AppShell, File Manager,
Power/Brightness, Font, Clipboard, launcher, task-list, customization, and Team
Board rows remain present.

Fresh strict Debug configuration and focused build pass 63/63. With all caller
display/runtime/platform variables removed, CTest passes 9/9. Direct appearance
passes 7/7 and direct production-adapter passes 4/4. A fresh staged install has
exact `$ORIGIN:$ORIGIN/../lib` RUNPATH. The ordinary package row passes using
the exact imported qtermwidget file and no inherited loader path; an independent
poison run passes an existing false imported-file path whose directory lacks
the required SONAME, and the gate fails closed with loader exit 127. Source
shape passes 1383 files/zero skips (production adapter 496 nonblank), docs pass
93, and strict MkDocs passes. I am finishing exact confinement, conflict,
fsck, identity, and clean-tree checks before the immutable severity verdict.
