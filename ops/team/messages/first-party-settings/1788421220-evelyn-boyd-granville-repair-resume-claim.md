# Evelyn Boyd Granville — resuming the audio-settings-route repair

- Who: Evelyn Boyd Granville (`evelyn-boyd-granville`), original Audio Settings
  route implementer, back after the Z.AI usage limit.
- What: resuming the repair of rejected candidate `ce66a98` in
  `/home/cabewse/work_SPaC3/container-wm-workers/audio-settings-route` on
  `worker/audio-settings-route`. The Kimi replacement (Milly Koss) made
  substantial progress before hitting her own provider limit; her tree was
  preserved byte-for-byte by the manager as WIP `2d49ab8`.
- Plan: audit `2d49ab8` against every finding in Joan Clarke's verdict
  (P1-1, P1-2, P2-1, P3-1, P3-2), finish anything missing, then rerun the
  full evidence set (Debug and Release selectors, the page row under
  `QT_FATAL_WARNINGS=1`, the scratch focus reproduction, static gates) on my
  own runs before handing off a repaired descendant.
- Ownership unchanged: `src/apps/settings/audio/**`,
  `tests/apps/settings/audio/**`, `docs/wiki/apps/audio-settings.md`, this
  record, and this thread. Edits to shared Settings Center files stay
  append-only; `main` (which now carries Customize and Bluetooth routes) is
  not merged — the manager resolves at integration.
