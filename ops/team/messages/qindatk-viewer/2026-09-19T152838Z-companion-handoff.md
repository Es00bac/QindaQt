# Remote committed QindaMPV companion handoff

## Exact source and ownership

- Source: **QindaMPV `6bfde6644d927989fcd157471873543c75b5b05b`** on qinda.
- `git rev-parse HEAD` and `git status --porcelain=v1` were read before archive
  and after smoke: the same exact commit and empty status, all exit 0.
- Source/archive/build/stage are under the desktop checkout's ignored
  `.cache/qqmpv-defaults-companion`, separate from the manager's desktop build.
- No QindaMPV source change, source-checkout mutation, global install or live
  session change was performed. Only the private stage was installed.

## Commands and evidence

With `companion_root` set to the remote desktop checkout's
`.cache/qqmpv-defaults-companion`, and `qqmpv_checkout` set to the inspected
clean QindaMPV checkout:

```sh
git -C "$qqmpv_checkout" archive --format=tar \
  --output="$companion_root/source-6bfde664.tar" \
  6bfde6644d927989fcd157471873543c75b5b05b
tar -xf "$companion_root/source-6bfde664.tar" -C "$companion_root/source-6bfde664"
cmake -S "$companion_root/source-6bfde664" -B "$companion_root/build-6bfde664" \
  -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_INSTALL_PREFIX="$companion_root/stage-6bfde664"
cmake --build "$companion_root/build-6bfde664" --target qqmpv -j2
cmake --install "$companion_root/build-6bfde664"
```

All commands exited **0**; build completed **30/30** steps. Configure used the
installed Qt **6.11.1**, public AppShell archives/headers and QindaTK module.
The installed toolkit package is `dev-libs/qindatk-0.1.0`; mpv is
`media-video/mpv-0.41.0-r2` and its `libmpv` USE capability query exits 0.

The staged binary is `stage-6bfde664/bin/qqmpv`. In an isolated environment
(private home/config/cache/data/state/runtime roots, offscreen/software Qt,
no session/system bus, no DISPLAY/Wayland endpoint and no audio-server endpoint):

- `qqmpv --help` and `--version`: **0**, expected usage and version 0.1.0.
- `desktop-file-validate` on staged `org.qindaqt.QQMpv.desktop`: **0**.
- `ldd` on staged binary: **0**, no missing library; installed AppShell resolves.
- `qqmpv --screenshot smoke/startup.png`: **0**, empty stderr. The PNG is
  **960×640**, **15,606 bytes**. Retrieved into ignored local output and visually
  inspected: QindaTK menus, empty state, Open File/Open URL and status UI render.
- All eight desktop-advertised audio/video MIME types exactly match the current
  desktop-scoped QindaMPV fallback policy.

Full machine-specific command outputs are retained remotely in `configure.log`,
`build.log`, `install.log`, `smoke-results.json` and `smoke/startup.png` beneath
the ignored companion root. The local inspected copy is ignored
`.cache/qqmpv-companion/qinda-qqmpv-startup.png` in this worker worktree.

## Packaging gap and next action

At inspection, `portageq match / media-video/qqmpv` returned an empty result,
and `portageq best_visible / media-video/qqmpv` exited **1**. No repository under
the remote overlay root supplied a qqmpv ebuild, so the desktop's PDEPEND was
not yet satisfiable there. The source checkout's old recipe pointed to a
nonexistent machine path and pinned the older `f02b4b9` commit.

Read-only Git config inspection found both a local origin and
`https://github.com/Es00bac/QindaMPV.git`. The actual local origin exists, is a
bare repository and directly contains `6bfde664…`; its exact path was sent to
the manager. The manager owns a corrected companion recipe pinned to that
verified source. Requested next action: include that recipe and the viewer/default
integration in the manager's combined remote verification; retain the stage
without a global install unless separately authorized.

Bounded caveats: this proves native compilation and isolated startup/metadata,
not video/audio playback, hardware acceleration, a live global-menu host, or an
installed/active default. Same reviewer independently ACCEPTed the viewer repair
`b724e5667d481b79227fd7fa22ae48104c014ecb` with zero findings. Available bounded
help: reproduce an integrated Viewer/companion staging or package-source failure.
