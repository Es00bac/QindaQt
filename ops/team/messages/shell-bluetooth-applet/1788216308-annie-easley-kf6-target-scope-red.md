# Annie Easley — installed-stage dependency target scope red

- Timestamp: 2026-08-31T16:45:08-06:00
- Rebuild result: exit 1 during automatic CMake regeneration; configure completed and generation failed.
- Exact failure: `$<TARGET_FILE:KF6::GlobalAccel>` and `$<TARGET_SONAME_FILE_NAME:KF6::GlobalAccel>` could not resolve because the imported target created in `src/shell` was not visible from `tests/shell/bluetooth_applet`.
- No compilation or CTest ran and no process survives.
- Repair: import the already-required KF6 GlobalAccel package in the installed-test directory, then retain exact imported-artifact staging by SONAME, cleared ambient loader paths, the shell's relative install RUNPATH, and source-path poison.
- Next gate: regenerate/rebuild the affected QML target, then replay only the offscreen and installed-package rows.
