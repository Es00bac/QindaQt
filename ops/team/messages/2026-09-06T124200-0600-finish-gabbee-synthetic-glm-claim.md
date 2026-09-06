# finish-gabbee-synthetic-glm claim: synthetic Gabbee interop evidence

Claimed the bounded Gabbee interoperability outcome (synthetic dictation into an ordinary
app and a terminal, correct focus including a grouped member, global shortcut
registration/routing) in isolated worktree
`/home/cabewse/work_SPaC3/container-wm/.cache/finish-gabbee-synthetic`, branch
`fix/finish-gabbee-synthetic`, exact base `1beeea23`.

Scope and constraints honored:

- Read-only Gabbee checkout `/home/cabewse/gabbee`; no edits there, no shared QindaQt
  production files. New `tests/session/gabbee/` probe/helpers, focused deterministic unit
  tests, docs, board/queue records only.
- No microphone recording (synthetic recorder seam writes a fixed WAV; mock STT returns a
  fixed harmless transcript), no host typing, no host input injection, no uinput. The
  development `InjectTestInput` seam's key set is a closed enum without character keys, so
  the probe routes insertion through Gabbee's production AT-SPI EditableText and clipboard
  sinks instead of any typing tool — no input injection of any kind.
- Portal GlobalShortcuts fix `8215a8cd` is treated as independently accepted and is NOT
  redone; the probe stages a runtime copy of the portal conf (plus a fake kde backend)
  inside the private run root, mirroring that candidate's verified `kde`-only routing, and
  flips to passthrough automatically once the fix integrates.

Root owns the runtime lane: I will deliver the executable hook, artifacts, and deterministic
focused unit tests now, and hand root the exact lane invocation
(`QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop`-gated) with no attempt to run
the nested session myself.
