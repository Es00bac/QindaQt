# gap-night-light claim — 2026-09-11T10:30:00-06:00

- Status: working — ADR-0136 first, then `src/services/night_light`, then the
  Display page section, private proof, docs.
- Base: `7dad9e78f117d7fb492d381d631d7cec637ce1e5`, branch `gap/night-light`,
  worktree `/home/cabewse/work_SPaC3/container-wm-workers/gap-night-light`.
- Ground truth confirmed from installed kcfg/D-Bus XML plus upstream knighttime
  v6.6.6 sources (`kdarklightsettings.kcfg`, `kdarklightmanager.cpp`): schedule
  keys live in `knighttimerc` groups `General`/`Location`/`Times`; enum values
  are stored as choice-name strings; `TransitionDuration` is seconds. Recorded
  in the lane NOTES and will be cited in ADR-0136.
- No collisions observed; `src/apps/settings_center`, `src/shell/**`,
  `src/services/display_*/**` will not be touched by this lane.
