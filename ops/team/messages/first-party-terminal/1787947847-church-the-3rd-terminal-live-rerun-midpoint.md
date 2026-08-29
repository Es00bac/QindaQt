# Church the 3rd — exact Terminal repair private-live midpoint

- Time: 2026-08-28T20:10:47Z
- Exact candidate: `a9cc17f2e9a7edef78cac9e9fe7e2e5fb8410352`
- Tree: `905cf870e46ea541da0667d0eb67ab38d795b2cb`
- Parent: `bf195b6abfce978cdc51706b327dc7ac12823c73`
- Current live result: **40/40 assertions PASS; 0 failures**
- Evidence root:
  `/mnt/d/QindaQt/builds/terminal-s0-live-rerun-church3-a9cc17f/evidence`
- Status: working — final immutable verdict audit remains

The copied retained qualifier rebuilt against fresh strict exact-candidate
static libraries, and `build.ninja` names only this detached source/build plus
the retained qtermwidget 2.4.0 prefix. The private run used Weston 15.0.1
headless/Pixman, a mode-0700 runtime, one private `dbus-daemon`, a fake Weston
seat, no inherited display/session/configuration environment, and direct
in-process Qt events only.

Material repaired-path evidence is green:

- pristine qtermwidget Select All still yields raw UTF-8 hex `0a`, but the
  emitted availability is false, `hasSelectedText()` is false, and the real
  window Copy action starts disabled;
- the selected profile is `qinda-dark`, both unselected center pixels equal
  expected `#171a18`, all eight custom `ColorNIntense` groups are present,
  and a real SGR bright-red background paints exact expected `#ffafaa`;
- the published window/view expose their production accessible names,
  descriptions, and strong focus policy; and
- UTF-8/ANSI, keyboard→child, PTY resize/SIGWINCH and geometry, populated
  selection/private clipboard copy/paste, normal exit 7, SIGTERM exit truth,
  three generations, restart→close cancellation, HUP/TERM-resistant child
  SIGKILL/reap, and lifecycle signals all pass. Startup exposure is 20 ms;
  application plus child PSS is 27,011 KiB.

All four PNGs are valid at approved 800x500 or 1120x680 paths and visual
inspection agrees with the pixel probes. The last recorded child is gone,
no qualifier/fixture/Weston/private-bus process matches, the runtime root is
removed, and no generated `.colorscheme` survives. I am repeating final
candidate identity/cleanliness and hashes before the terminal verdict.
