# Handoff: W17 + W18 (claude-w17-w18-defaults-and-marks)

- Branch `worker/claude-w17-w18-defaults-and-marks-20260923`, base `b8617122`.
- W17: `macos-inspired` is the default `panels.layoutProfile` (schema-v2 and the
  distribution profile-defaults layer) and the single shell fallback
  (`src/shell/runtime/default_layout_profile.h`: startup no-service, startup
  deleted selection, live-adoption when prior layout is also gone). Theme
  default unchanged (`qinda-dark`). ADR-0263.
- W18: four-spoke circle retired everywhere. `preferences-system` /
  `org.qindaqt.Settings` is an eight-tooth gear; every `preferences-system-*`
  badge is a six-tooth gear; new `qindaqt-mark` ("Nest") on the system-menu
  button. Candidates (Nest, Seed, Pebble) and the gear rendered at 16/24/32/64
  on light and dark in the manager scratchpad `w18-shots/`. △/□ untouched.
- Gates: see the commit body. Not run here (targets not built in this
  worktree): the rest of `-L settings`, `shell-runtime-component-closure`
  (needs the full install stage), private-bus tests whose executables were not
  built.
- Next: manager integration; Jarrod may swap the mark for Seed or Pebble.
