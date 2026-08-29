# Church the 3rd — exact Terminal repair private-live PASS

- Time: 2026-08-28T20:12:11Z
- Reviewer: Church the 3rd (OpenAI collaboration runtime; exact serving model
  and reasoning unexposed)
- Exact candidate: `a9cc17f2e9a7edef78cac9e9fe7e2e5fb8410352`
- Tree: `905cf870e46ea541da0667d0eb67ab38d795b2cb`
- Parent: `bf195b6abfce978cdc51706b327dc7ac12823c73`
- Verdict: **PASS — P0/P1/P2/P3 = `0/0/0/0`**
- Product edits: none
- Reviewer state: handoff; Church the 3rd is not live

This exact source/build-accepted non-amended repair passes the mandatory
private-Weston live gate. The copied retained qualifier remains exactly 40
assertions; it was path-rebound to a fresh strict build of this detached
candidate, while the two repaired-path assertions were made explicit about
raw LF/Copy truth and real custom bright-color rendering without widening the
count or touching product source. The result is **40/40 PASS, 0 FAIL**.

## Former live defects closed

1. **Qinda theme and bright groups:** `source_theme_id=qinda-dark`; expected
   terminal background is `#171a18`, and both the 800x500 first unselected
   frame and 1120x680 populated unselected frame report and visibly show that
   exact surface instead of the former white default. The installed live
   custom document contains all eight `Color0Intense`..`Color7Intense`
   sections. A real SGR 101 bright-red background paints exact expected
   `#ffafaa`, proving the custom intense path reaches qtermwidget rather than
   relying only on document text.
2. **Blank selection and Copy truth:** pristine Select All still exposes
   qtermwidget 2.4's exact structural UTF-8 hex `0a` (length 1), while the
   production adapter emits selection availability false, reports
   `hasSelectedText()==false`, and the real window's Copy action is disabled.
   Populated spaces/text remain selectable and copyable.

## Complete private live evidence

The remaining matrix passes real Wayland exposure and a 20 ms first frame;
production accessibility names/descriptions and strong terminal focus; fixture
output; keyboard-to-child flow; UTF-8 snowman and normal/bright ANSI; real PTY
resize from 99x25 to 139x35, child SIGWINCH, and child-read geometry; populated
selection, exact private clipboard copy, and production paste; normal exit 7
and SIGTERM truth with both children reaped; two accepted restarts and three
distinct backend generations; Restart followed by real Window Close canceling
the pending replacement; and bounded escalation of a HUP/TERM-ignoring third
child through SIGKILL, `ShutdownComplete`, exact single close completion, and
reap. Lifecycle counts are exit 2, state 11, shutdown 3.

`/proc/*/smaps_rollup` reports application 26,725 KiB plus child 286 KiB,
aggregate **27,011 KiB**, below the 1,024 MiB bring-up ceiling. All four PNGs
were decoded and visually inspected: first captures are 800x500 and populated
captures are 1120x680.

## Build, provenance, and retained artifacts

All new generated output is under
`/mnt/d/QindaQt/builds/terminal-s0-live-rerun-church3-a9cc17f`; the earlier
negative-control artifacts under `terminal-s0-live-church3` remain untouched.
The strict Debug candidate build used qtermwidget 2.4.0, tests enabled, shell/
production-shell/KWin/host-uinput disabled, strict warnings, and
`CMAKE_AUTOMOC_PATH_PREFIX=ON`. It built the exact support, adapter, design-
token, and theme libraries before the qualifier rebuild. The qualifier's Ninja
link edge names only those fresh candidate artifacts plus the retained pinned
qtermwidget prefix. `ldd` resolves qtermwidget to that prefix; the real library
SHA-256 remains
`b1440218096965e6161d67fab56d5f4ef6da869ad02cdb8999e98aa95a990dd1`.

Key SHA-256 values:

- live run log: `6e7ffa4738de7daaa5016909e2c9c7c78a7f76f859e333be627104b6b0c45577`
- first unselected PNG: `a71ae207a757149229687aeb1a50b10cf99eb3a51c2c8f47bd2d1183a854fc89`
- populated unselected PNG:
  `70999d102c5baec7990c46f3ecb08166bde4d0c0dd10e90bfa7fd638abb4d173`
- Weston log: `ed805509a4ccc1af781d86ff023b79b893ca02a38903eb279a0186c09e3c6c59`
- private D-Bus log:
  `c783f26d01b11449cb0bfb4bfd8b170789d68aa96e5bb17067ce6fbcadb209bd`
- qualifier binary: `f4ef4944cf7509675ceab86269a679468617927cd8e18c3c0ae99f210736a911`
- direct fixture: `274a3042d947f70d0675d82c5d3ea0da5bb74216be8de4d196bee66f2d9caff2`
- exact adapter archive: `9090f5326e2c7957f7d01b54ea654057cda584ccd052267a3dc955af0da6391b`
- exact support archive: `9564b597e1bf03b2c37fabaea001c68696b89a946d1aaa6cfbf75eed90b02d4d`
- qualifier source: `dabf7a46f91deef606d9fac3ae2770dad6fb226ee056c096e8833171edb3de3f`
- fixture source: `5a925db0211841494fb7e85ce5f6f7246e37e470ecb96bbbb9797f17a1cdfa83`

## Containment, cleanliness, and teardown

The run started Weston 15.0.1 with headless/Pixman, fake private seat,
1280x720 output, no config, and socket `qindaqt-terminal-church3` beneath the
new mode-0700 runtime
`/run/user/1000/qindaqt-terminal-church3-rerun-a9cc17f-runtime`. It also
started one private `dbus-daemon` on a socket inside that root. Weston and the
qualifier received fresh HOME/XDG config/cache/data roots beneath the generated
run root through an environment with inherited display and bus state removed.
Input consisted only of direct in-process Qt events to the private window. No
`uinput`, `dotool`, host compositor/socket, host pointer/cursor, host clipboard,
host session bus, or host configuration was contacted.

The last child PID is gone. Exact executable and command-line scans find no
qualifier, fixture, Weston, or private D-Bus survivor. The private runtime root
and sockets are removed, and no generated `.colorscheme` remains. Final
candidate HEAD/tree/parent repeat exactly; `git status --porcelain`,
`git diff --check`, and the uncommitted diff are empty.

## Required next action

The Terminal S0 review spine is now complete: Dijkstra's exact source/build
PASS and this independent exact private-live PASS both report
P0/P1/P2/P3 `0/0/0/0`. The Program Manager should integrate exact candidate
`a9cc17f2e9a7edef78cac9e9fe7e2e5fb8410352`, rerun the affected focused gate
on the integrated tree, and update the manager-owned task/wiki/handoff state.
No Tomas repair is requested.
