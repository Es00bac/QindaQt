# Rhea Calder — virtual desktop observed-identity repair claim

- Timestamp: 2026-08-28T12:57:32Z
- Exact base/HEAD: `e325f2f1e8d69d2d6e3eaa42c04df0f71d2265c7`
- Tree: `ca722256cd0dbd353ae264a571ce6d5e2171168b`
- Runtime authority: bounded-FAIL handoff `1787921728`, final run
  `26e772f23f519434ce445dca4ff51128`

Rhea claims only the two exact source-contract repairs reproduced by the live
archive. The installed Settings binary exposes compositor application ID
`qindaqt-settings`; that observed production identity becomes the expected
truth instead of `org.qindaqt.Settings`. The single-output topology will no
longer assert a guessed ordinal name. It will derive one canonical nonempty
virtual-output identity from the exact one-item Outputs inventory, then require
the ShellVisibility inventory and every consumed dock current/desired output
reference to equal that identity. One output, `(0,0)`, 1920x1080, scale 1,
equal nonzero generations, one combined development input, dock PID ownership,
and exact application/window semantics remain mandatory.

Ownership remains inside the virtual-desktop model/readiness tests and its
primary ADR/testing documentation. Elara's live analysis currently has only
claim `1787921694`; its completed handoff will be read before this repair is
finalized. Devika owns the serial compiler lane. Rhea will run only Python unit,
in-memory compilation, source-shape, documentation/navigation, descriptor/
static if affected, ancestry, whitespace, and clean-tree gates. No configure,
compile, package CTest, bubblewrap, nested/private session, compositor, UI,
display/input/config/hardware, or host endpoint will run.
