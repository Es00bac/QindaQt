# Handoff — WYSIWYG shell-customization architecture (Liora Vale)

- Posted: 2026-08-28T13:16:09Z (unix 1787922969)
- Worker: Liora Vale — Anthropic Claude Opus 5 (`claude-opus-5`), reasoning maximum
- Base inspected: `9db68c4023257b49421101fa1b13c73bbc2cfa85`, read-only, clean detached worktree
- Status at this post: **handoff — analysis complete; this seat is not live**

## Deliverable

`1787922661-liora-vale-architecture-and-acceptance-matrix.md` in this thread.
It contains P0–P3 decisions with rejected alternatives, process/module
boundaries with proposed `module-boundaries.md` rows, the in-process and
cross-process transaction schema, the WYSIWYG interaction state machine with
six invariants, collision/rollback/persistence rules, the responsive and
multi-output/DPI matrix, the accessibility and direct-manipulation parity
contract, six phased slices with paths and acceptance commands, dependency
order, thirteen prohibited shortcuts, and the consent list.

Supporting reply: `1787922348-liora-vale-material-findings.md`.

## What changed in the picture

1. The edit engine is already built; this lane is presentation, transport,
   persistence, and reveal — not a second mutation model.
2. 34 of 63 stock applet instances have no manifest, so most first drags in a
   shipped profile fail today. Shipping the 21 missing manifests is a
   precondition inside the first slice, not a nice-to-have.
3. One drag equals one preview bracket. That is what makes a zone-crossing drop
   atomic and gives exactly one undo step per gesture.
4. Multi-row flow, floating margins, opacity, output-selector policy, and the
   desktop container are genuinely absent and need profile schema v2 plus, for
   the desktop, its own planner and ADR. None of them is required by the first
   slice.
5. Production panel windows cannot take keyboard focus, so keyboard and
   assistive parity must live in the editor window, and the outline view — not
   the canvas — is the accessible representation.
6. `minimal` and `xfce-inspired` each ship a `hideMode: always` panel with no
   reveal path, so those panels are currently unreachable in a live session.
   That is a live defect independent of the editor.

## Recommended next action

Assign **C0** — "Customize: a real drag-and-keyboard layout editor with
persistence" — to one implementer in a fresh worktree at the current
integration base. It needs no new protocol, no schema change, and no
production-shell build option, so it builds and tests in the dependency-light
CI lane. Its five ordered steps, owned paths, invariants, acceptance commands,
and required evidence are in §12 of the artifact.

**C1** (reveal, hold, hide animation) is independent and can run in parallel
with a different worker; it also closes the `always`-panel defect above.

Before C0 starts, the consent list in §15 needs answers — chiefly the additive
`data/applets` entries, the new `src/shell_targeting` module, and the new
`module-boundaries.md` rows.

## Bounded caveats

- Every interface, module, test name, matrix row, and slice in the artifact is
  a proposal. None exists at the base commit.
- I did not compile, run any test, launch any UI or session, start a
  compositor, or touch the host desktop, input devices, session bus, or user
  configuration. No command in the acceptance matrix has been executed, and
  nothing here is evidence that code behaves as documented.
- I make no product, qualification, or completion claim. `QQ-004.08` remains
  `MODELLED`; an architecture document adds zero weighted progress.
- Durable writes made by this seat: `ops/team/workers/liora-vale.md` and the
  three replies in `ops/team/messages/shell-customization/`. No product, docs,
  build, or test path was touched.

## Requested reviewer action

A different worker should check the artifact against the base commit for
citation accuracy and for any decision that would violate an accepted ADR or an
owned module boundary, and should challenge D2 (preview as gesture bracket),
D4 (profiles own persistence, not Settings1), and D8 (selector resolution
rather than solver change) specifically — those three carry the most downstream
consequence.

— Liora Vale, 2026-08-28T13:16:09Z. Handoff; not live.
