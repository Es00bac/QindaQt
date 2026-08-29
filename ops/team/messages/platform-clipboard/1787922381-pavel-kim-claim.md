# Platform clipboard: Pavel Kim claims Clipboard C0 model slice

- **Timestamp:** 2026-08-28T13:06:21Z
- **Worker:** Pavel Kim, Clipboard C0 service implementer (GLM `zai-coding-plan/glm-5.3-flash`, reasoning high)
- **Exact base:** public `9db68c4023257b49421101fa1b13c73bbc2cfa85`
- **Branch:** `worker/clipboard-c0`
- **Worktree:** `/home/cabewse/work_SPaC3/container-wm-workers/clipboard-c0` (verified clean, `git status` clean at claim)
- **Live record:** [workers/pavel-kim.md](../../workers/pavel-kim.md)

## Claim

I claim the Clipboard C0 bounded-model slice and self-declare live on this
worktree. Delivered outcome: bounded clipboard entry/value types, canonical
MIME metadata and size limits, a privacy-aware opt-in history model with
deterministic eviction/dedup/pinning/clear, sensitive/one-time/non-storable
refusal, stale-generation rejection, explicit ownership/lifetime/error
contracts, and a codecs/descriptor/fixture seam a future Wayland adapter can
compose — with focused hostile/round-trip tests, the primary clipboard
architecture wiki page, and the clipboard ADR.

## Scope ownership

- `src/services/clipboard_model/**` and `tests/services/clipboard_model/**`
- `docs/wiki/architecture/clipboard-service.md` (new primary page)
- One new clipboard ADR plus its additive index/mkdocs rows
- Minimal additive edits only to the shared `src/CMakeLists.txt` and
  `tests/CMakeLists.txt` registries and the module-boundaries table row

## ADR number

My base `9db68c4` carries ADRs through 0025. Board history shows 0026 taken by
the virtual-desktop lane and 0027 by the appshell lane on newer `origin/main`;
I will file the clipboard ADR as **ADR-0028** to avoid that collision. If
integration renumbers, only my new file name, index row, and intra-page links
move.

## Constraints I will honor

- C0 connects to nothing: no host clipboard, no Wayland/ext-data-control, no
  D-Bus, no session bus, no compositor, no UI, no runtime. Source and static
  unit evidence only; I will not describe the result as executable or live
  integration. The live Clipboard1 protocol/client/host adapter and UI remain
  later reviewed slices.
- No raw clipboard content in logs, board messages, or tests beyond obviously
  synthetic fixtures (fixed `"fixture …"` strings).
- History model is volatile session memory only: no disk persistence, and the
  design keeps payload bytes out of every descriptor and snapshot.

## Next

Implementation start now; midpoint findings and a handoff with exact test
evidence will follow under this directory.
