# Rhea Calder claims Display D0

- **Timestamp:** 2026-08-28T01:15:44Z
- **Provider/model:** OpenAI Codex `gpt-5.6-sol`, reasoning high
- **Role:** Display D0 compositor-output lead
- **Exact base:** `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
- **Branch/worktree:** `worker/display-d0`, `/home/cabewse/work_SPaC3/container-wm-workers/display-d0`
- **State:** working; source/static lane only, no compiler/runtime lane

I claim the complete manager-assigned D0 outcome: one monotonic, canonical
Compositor1 output generation with truthful stable fields, plus bounded
development-only virtual output add/remove through the exact KWin 6.6.5 public
backend ABI. Production must reject both valid and hostile mutation payloads
as `control-disabled` before parsing or backend access.

Initial exact evidence:

- worktree `HEAD` is the required `94e84077e33a279dcebee24511e7dbdf1b87e3e1`
  and `git status --short` is empty;
- current output projection is still embedded in
  `src/compositor/kwin/managedwindowregistry.cpp:377-391`;
- output invalidation currently enters through
  `src/compositor/kwin/kwincontrolendpoint.cpp:59-60` and
  `src/compositor/kwin/managedwindowregistry.cpp:72-73`;
- the existing production pre-parse rule is normative in
  `docs/wiki/architecture/compositor-session.md:239-247` and the D0 assignment
  makes the new seam subject to that same gate;
- the immutable source pin is `compositor/upstream/kwin.json:3-12`, including
  KWin release `6.6.5`, commit `b04d59c...`, and tree `99868e7d...`.

I will preserve D1's disjoint service modules and shared-registry edits, will
not touch `kwinoutputconfig.json` or host display/session state, and will not
configure, compile, run CTest, or launch a nested compositor until the manager
explicitly assigns the lane. Before implementation I am tracing the exact
installed/pinned KWin signatures, current control-codec pre-parse flow, plugin
lifetime, visibility publication, descriptor parity tests, and focused CMake
fixtures.
