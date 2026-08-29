Audio applet A1 handoff and review request (Elias Frost)

Candidate: exact commit `ace0265b098097cb2fc4cfeacef47339be7168fd`
("Add bounded Audio applet presentation source slice") on branch
`worker/audio-applet-a1` in worktree
`/home/cabewse/work_SPaC3/container-wm-workers/audio-applet-a1`. Tree
`d19887d6838d2c481e4998c0818399ba73280765`, parent
`9db68c4023257b49421101fa1b13c73bbc2cfa85` (the assigned public base, exact).
Working tree clean at handoff.

Changed-path manifest (all new except the one nav line):

- `src/shell/audio_applet/audio_applet_model.h/.cpp` — pure projection:
  loading/ready/degraded/unavailable phases, bounded 8 device + 8 stream rows
  with overflow counts, default output/input labels that resolve beyond the
  window, label fallbacks, and a volume clamp that rejects non-finite levels
  and clamps finite ones into [0,1]. Fail-closed AGENT-GUARD: a snapshot with
  `wireValid == false` projects as unavailable with reason
  `malformed-snapshot` and no rows.
- `src/shell/audio_applet/audio_applet_controller.h/.cpp` — shell-side facade
  over the public `QindaQt::Audio::AudioClient` (borrowed; never
  started/stopped/parented). Clamp-before-dispatch, capability-aware local
  refusals, one pending request per snapshot-unique serial, stale pending
  pruning without feedback, status/reason-code feedback mapping (diagnostics
  never parsed or shown), and AGENT-GUARD no-replay handling of late, foreign,
  or pruned request IDs.
- `src/shell/audio_applet/qml/{AudioApplet,AudioDeviceRow,AudioStreamRow}.qml`
  — `QindaQt.Shell.AudioApplet` 1.0 presentation consuming only
  `QtQuick`, `QtQuick.Layouts`, and `QindaQt.Controls 1.0 as C`; injected
  controller only, no service import, explicit accessible names/descriptions,
  keyboard-dispatchable volume (5 percent steps, dispatch on release or per
  keyboard step) and mute switches.
- `src/shell/audio_applet/CMakeLists.txt` and
  `tests/shell/audio_applet/CMakeLists.txt` — standalone, self-contained
  seams, deliberately not referenced by any parent CMakeLists.
- `tests/shell/audio_applet/tst_audio_applet_model.cpp` (10 test slots) and
  `tests/shell/audio_applet/tst_audio_applet_controller.cpp` (13 test slots)
  — hostile source tests driving the real public client through a fake
  transport: clamp-before-dispatch, NaN refusal with no dispatch, uncapable-row
  refusal, second-request refusal, rejected/uncertain/success feedback paths,
  stale-prune with ignored late reply, invalid-wire fail-closed through the
  client, degraded/unavailable phases, bounds/overflow, ordering, and label
  fallbacks.
- `docs/wiki/shell/audio-applet.md` — primary wiki page; plus the single
  additive `mkdocs.yml` nav entry under Shell.

Verification evidence, with exit status 0 for every gate:

- `python3 tools/check-source-shape --root . --warnings-as-errors` — 1013
  files checked, 0 violations; largest new file is the controller test at
  484 non-blank lines.
- `python3 tools/validate-docs --root .` — 64 Markdown documents and
  mkdocs.yml navigation validated, including the new page.
- `git diff --check` plus a trailing-whitespace/tab scan of every new file —
  clean.
- Brace/paren balance, leftover TODO/FIXME/qDebug/console.log audit — clean.
- Manual static review against the exact Audio1 rules
  (`audio_validation.cpp`): test snapshots and operation results satisfy
  epoch/serial/ascending/default/target constraints; `-Wconversion`-safe
  arithmetic; no member/free-function name collisions; signal/slot signatures
  match; Q_GADGET rows carry Q_DECLARE_METATYPE.

Not run, deliberately and per the outcome bounds: configure, compiler,
CTest, QML lint, GUI, session bus, PipeWire/WirePlumber, or any audio
runtime. The two QtTest targets (`qindaqt.audio-applet-model`,
`qindaqt.audio-applet-controller`) are unexecuted source awaiting the
integration review build; nothing here claims runtime qualification.

Requested next action: an independent reviewer inspects the exact candidate
commit above; the manager then applies the required additive seams:

1. `add_subdirectory(src/shell/audio_applet)` and
   `add_subdirectory(tests/shell/audio_applet)` (or equivalent registry
   wiring) so the static library, QML module, and both test targets build;
2. a manifest catalog entry (`qindaqt.applets.audio`), capability-policy row,
   compiled built-in registry entry, and QML dispatcher inventory entry;
3. shell composition that starts `AudioClient`, injects the controller, and
   later narrows the surface behind the shell-owned `openAudioSettings()`
   facade from the Audio service consumer boundary; and
4. optionally, a reciprocal link from `docs/wiki/architecture/audio-service.md`
   (consumer boundary) to the new page — left to that page's owner.

Bounded caveats: QML is unlinted and unrendered in this slice (no GUI
access); default selection and stream moves are intentionally outside this
slice; feedback strings are asserted exact only under the C locale in tests;
retry-on-degraded is deliberately absent because the public client exposes no
refetch seam — listed here rather than invented.

— Elias Frost, 2026-08-28T13:56:45Z
