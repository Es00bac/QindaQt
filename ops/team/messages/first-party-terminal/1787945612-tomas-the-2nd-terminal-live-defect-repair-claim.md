# Tomas the 2nd — Terminal real-adapter defect repair claim

- Time: 2026-08-28T19:33:32Z
- Exact base: `bf195b6abfce978cdc51706b327dc7ac12823c73`
- Tree: `563a0793b1736238f8d59a54de81e022b0989c1a`
- Branch: `worker/terminal-s0-repair-tomas`
- Worktree:
  `/home/cabewse/work_SPaC3/container-wm-workers/terminal-s0-repair-tomas`
- Status: working

I own the non-amended repair descendant for the two exact live failures Church
the 3rd reproduced against `bf195b6`:

1. Select All on a never-started blank real qtermwidget yields LF (`0a`), so
   `hasSelectedText()` currently enables Copy for semantically empty content.
2. The generated `qinda-dark` colorscheme requests background `#171a18`, but
   the actual unselected qtermwidget surface remains white in private-Wayland
   captures; scheme lookup/application truth is therefore false.

I will preserve all accepted lifecycle, PTY, teardown, confinement, and theme
contracts; add focused automated regression controls where the real dependency
permits; build only under `/mnt/d/QindaQt/builds` with
`CMAKE_AUTOMOC_PATH_PREFIX=ON`; and never touch the host desktop or input.
Handoff requires one exact committed descendant with commands/counts, paths,
tree hash, caveats, and requests for both Dijkstra source/build rereview and
Church private-live rerun.
