# Bundled-apps package installed, calendar repaired, shell refreshed (kimi)

The pre20260910 desktop package is now installed on the host and live in the
running session, closing the loop on the earlier ebuild-prep note.

- First merge (pin `0e44c65b`) succeeded, but the installed-binary smoke
  probe caught `qindaqt-calendar` unable to resolve
  `libqindaqt_app_shell.so`: the calendar target had the default RUNPATH,
  which cannot reach the AppShell backing library in the QML module dir.
  The build-tree gates could not see this because they resolve from build
  RPATHs.
- `80760ece` fixes it (INSTALL_RPATH + AppShell/Controls/Tokens component
  repeats, mirroring the File Manager AGENT-CONTRACT) and adds the staged
  `qindaqt.calendar-installed-runtime` row so the installed layout is
  covered going forward. Calendar focused rows pass 13/13.
- Ebuild re-pinned in `3534c12c`; archive/digest regenerated; second merge
  completed exit 0. All four installed binaries verified offscreen
  (usage + library resolution), calendar RUNPATH now reaches
  `/usr/lib64/qt6/qml/QindaQt/AppShell`.
- Live refresh without logout, with the user's authorization: PID-fenced
  SIGTERM replaced shell `1061879` with `1326414` on the new package via the
  paced supervisor contract (~1 s); compositor `483462` and supervisor
  `483526` unchanged; all 15 terminal processes survived; the stale
  file-manager instance was closed. New app launches pick up the installed
  versions.
- HANDOFF (same commit as this note) records the full evidence. The
  `settings-*-installed-route` rows now fail only on a staging-harness
  unix-socket path-length limit (>108 bytes) under this checkout's build
  prefix — environment, not product; the relocation poison is gone now that
  the installed tree matches.
