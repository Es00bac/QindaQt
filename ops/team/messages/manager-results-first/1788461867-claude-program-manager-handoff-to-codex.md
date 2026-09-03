# Program Manager handoff — Claude to Codex (2026-09-03 12:58 local)

You (Codex) take over as Program Manager and integrator of `main` immediately. The user's standing orders, verbatim in spirit:
**make the desktop work on this machine, no new features, one funded review round per candidate, lowest token spend, highest quality, use Codex/Kimi/GLM each for what they are good at, no stagnation.** Report truthfully; never claim a verification you did not run.

## 1. Where main is

- `main` = `eb82f6f` (repo `/home/cabewse/work_SPaC3/container-wm`). Working tree clean. Every accepted candidate since `74da463` is merged and recorded in `docs/TASK_LIST.md` ("Completed outcomes"), `docs/HANDOFF.md` (delta list at the top), `ops/team/features.json`, `ops/team/queues/{shell,platform,first-party}.md`, and `ops/team/workers/claude-program-manager.md`.
- **Compositor pin = the host's system KWin 6.6.6** since `a5c1c20`. Build with the cache `/home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-system-kwin-initial-cache.cmake` (system Qt 6.11.1, KF6 6.27, KWin/KDecoration3/PlasmaActivities/KWayland/LayerShellQt 6.6.6). Weston still comes from the private Arch prefix `/home/cabewse/work_SPaC3/builds/qindaqt-deps/qindaqt-arch-665/root/usr` and is confined to the parent-compositor environment by the harness.
- Manager build roots: `/home/cabewse/work_SPaC3/builds/qindaqt/container-wm/sys-dev` (Debug) and `sys-release` (Release, **currently configured with `-DCMAKE_INSTALL_PREFIX=/usr/local -DKDE_INSTALL_USE_QT_SYS_PATHS=OFF`** for the real install, see §5).
- Last full verification on `73ac584`: focused 61/61 Debug+Release, static gates, broad safe Debug 582/582. Nested desktop boot green on the system KWin.

## 2. In flight right now (act on each EXIT marker under `/home/cabewse/work_SPaC3/builds/qindaqt/lanes/<lane>/`)

| Lane dir | What | Worker | Worktree / branch | Next step when it finishes |
|---|---|---|---|---|
| `manager-verify-panel666` | Verification of merge `b5c8d87` (panel-visibility proof on KWin 6.6.6) incl. nested boot + panel rows | manager script | — | If green: record QQ-004.02 in features/TASK_LIST/HANDOFF/queue; if red: read `run.log`, diagnose, do not merge anything else meanwhile |
| `bluetooth-pairing-repair` | Bounded repair of `43a7cb1` (0/3/2/1) | Hee Oh (Codex) | `container-wm-workers/bluetooth-pairing`, `worker/bluetooth-pairing` | Recheck by **Kathrin Bringmann (Codex)**, worktree `bluetooth-pairing-codex-review` (detach at the new SHA); prior verdicts `review-btpair-codex/verdict.md`; accept → merge + verify `ctest -R bluetooth` + broad |
| `task-list-applet-repair` | Bounded repair of `da9f2fd` (0/2/1/1) | Dusa McDuff (Kimi k3) | `task-list-applet`, `worker/task-list-applet` | Recheck by **Lauren Williams (Codex)**, worktree `task-list-applet-codex-review`; then merge; ops records are git-excluded — `git add -f` them from the worktree |
| `color-settings-route-repair` | Bounded repair of `944673b` (0/1/1/0) | Maryam Mirzakhani (Kimi k3) | `color-settings-route`, `worker/color-settings-route` | Recheck by **Maryna Viazovska (Codex)**, worktree `color-settings-route-codex-review`; accept → **integration-assistant merge** (registry conflicts; see §4) |
| `tray-applet` | Tray S2 applet presentation + registration | Sijue Wu (Kimi k3) | `tray-applet`, `worker/tray-applet` | Codex review (new persona); then merge |

Each of these is on its **last funded round**: a rejection after this recheck means shelve with a caveat in the queue, do not fund more.

## 3. Remaining user-visible blockers (in order)

1. **Task list and tray in the panel**: after both applets merge, open ONE Codex lane "shell hosting of task list + tray" mirroring `lanes/clipboard-composition/lane.md` (composition in `src/shell/runtime`, `BuiltinAppletContent.qml`, install components, DesktopVirtual staging in `tests/session/PanelVisibilityTests.cmake`, test-import stubs under `tests/shell/qml/imports/QindaQt/Shell/<Module>/`, and bump the launcher contract guard count in `tests/shell/launcher/check_launcher_contract_text.cmake` from eight to ten).
2. **Real install and SDDM login** (§5).
3. **Bluetooth pairing** (lane above).
4. Everything else (Font/Portal Settings pages, hardware qualification, nested matrices) is NOT requested now.

## 4. How to run the workflow (exact recipes)

- **Worker launch (Codex)**: `nohup setsid bash -c "codex exec --skip-git-repo-check -C <worktree> --dangerously-bypass-approvals-and-sandbox -m gpt-5.6-sol -c model_reasoning_effort=\"high\" -o <lane>/last-message.md - < <lane>/brief.md > <lane>/run.log 2>&1; echo EXIT=\$? > <lane>/EXIT" &`. Never use Codex fast mode.
- **Worker launch (Kimi)**: `export CHOKIDAR_USEPOLLING=1 CHOKIDAR_INTERVAL=5000; cd <worktree> && kimi -m kimi-code/k3 -p "$(cat brief.md)"` (plain `-p`; no `-y`). Kimi has 5-hour limits (hit twice today); on 403 preserve WIP as a commit ("Preserve in-progress …") and reassign to Codex.
- **GLM** (`zai-coding-plan/glm-5.3` via kimi) is out until **20:25 today** (daily quota); never `glm-5.3-highspeed`. Use GLM for reviews when back.
- **Briefs**: `brief.md = lanes/COMMON.md + lane.md` (workers) or `lanes/REVIEW-COMMON.md + lane.md` (reviewers). Keep them short. Reviewers write `verdict.md` ending `VERDICT ACCEPT|REJECT P0/P1/P2/P3=a/b/c/d`; ACCEPT needs P0=P1=P2=0. Persona names must be unused (`grep -rqi "<name>" ops/team docs/HANDOFF.md lanes/*/lane.md`).
- **Integration**: `git merge --no-ff worker/<lane>`; ADR number collisions are renumbered at integration (next free is ADR-0071); registry-heavy merges (Settings routes, applet registries) go to an integration assistant persona "Hedy Lamarr-Codex" (see `lanes/integrate-clipboard-route/brief.md`) which prints `MERGE <sha>`; then `git merge --no-ff integration/<branch>`.
- **Verification** (mandatory before recording): `lanes/manager-verify.sh '<focused-regex>' '<exclude-regex>'` detached with an EXIT marker (see any `lanes/manager-verify-*/`), which reconfigures `sys-dev`/`sys-release`, builds, runs focused rows in both profiles, static gates, then the broad Debug suite. Exclude regex used today: `^(compositor\.kwin-(input|output|development|chrome|hybrid|dock|group|shell-window-actions)|desktop\.virtual\.(boot|interactive|panel-visibility\.single)|shell\.production-surface|.*notification-live-(nested|installed|shell|matrix))`. For shell-runtime merges also run nested rows serially: `QINDAQT_PRIVATE_RUNTIME_LANE=interactive-virtual-desktop ctest -R 'desktop\.virtual\.(sandbox-unit|package-contract|boot|panel-visibility\.)' -j 1` (`lanes/manager-verify-panel.sh` does both). Only one nested user at a time on this host (`pgrep -f kwin_wayland` empty first).
- **Never merge or edit the working tree while a verification is building** (it reads the tree live; two false alarms today). Never `pkill -f` with a pattern that appears in your own command line (killed the manager shell three times).
- **Ledger update after a green verification**: features.json step (state/summary/evidence/caveat via python), TASK_LIST "Completed outcomes" entry, HANDOFF delta line, queue row, manager record, verdict copies `git add -f` into `ops/team/messages/<thread>/`. `.git/info/exclude` hides `ops/team/messages/*/` and `ops/team/workers/*.md` — always `git add -f`.
- Full pitfall list: see the memory notes mirrored in `docs/HANDOFF.md` deltas (staging in the DesktopVirtual component for every new shell-linked module; static Power backend must be linked into in-process Main.qml test hosts; contract-text count guards; test-import stubs).

## 5. Real install + login (the user must run sudo)

Findings today: install rules bake the configure-time prefix into `wayland-sessions/qindaqt.desktop` and the D-Bus `.service` files, so `cmake --install --prefix /opt/...` gives wrong Exec paths. With `sys-release` configured as `-DCMAKE_INSTALL_PREFIX=/usr/local -DKDE_INSTALL_USE_QT_SYS_PATHS=OFF`, a trial install to a user prefix produced correct `Exec=/usr/local/bin/...` entries and all 31 installed-package rows passed; the KWin plugin lands in `lib64/plugins/kwin/plugins/qindaqt_compositor.so` and the decoration in `lib64/plugins/org.kde.kdecoration3/` (prefix-relative). **Open check before the user installs**: confirm `qindaqt-wm`'s `--plugin-root` default (`InstallPaths::pluginRoot()` from `QINDAQT_INSTALL_PLUGIN_DIR`) resolves to `/usr/local/lib64/plugins` in that configuration, and that the D-Bus session bus finds `/usr/local/share/dbus-1/services` (default XDG_DATA_DIRS includes `/usr/local/share`). Then the user runs:

    sudo cmake --install /home/cabewse/work_SPaC3/builds/qindaqt/container-wm/sys-release

and picks "QindaQt (Wayland)" in SDDM (Plasma stays available). Do the verification of `main` on `sys-release` **before** re-running that install so the installed tree matches a verified SHA.

## 6. Host facts

Gentoo; SDDM enabled (`/usr/share/wayland-sessions`); no weston on the host; `/tmp` is a small tmpfs (never build there); docs venv `/home/cabewse/work_SPaC3/builds/qindaqt-docs-venv/bin/mkdocs`; monitors that watched `lanes/*/EXIT` were Claude-session-local and are gone — poll `ls lanes/*/EXIT` yourself.
