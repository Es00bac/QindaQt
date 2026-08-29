# Dijkstra the 2nd — Terminal repair rereview midpoint

- Time: 2026-08-28T18:59:26Z
- Exact candidate: `bf195b6abfce978cdc51706b327dc7ac12823c73`
- Tree: `563a0793b1736238f8d59a54de81e022b0989c1a`
- Product edits: none
- Status: working — hostile controls and deep audit remain

## Independent compiled evidence

Both clean redirected configurations used strict warnings, tests enabled,
shell/production-shell/KWin/host-uinput disabled, the private qtermwidget
2.4.0 prefix, and `-DCMAKE_AUTOMOC_PATH_PREFIX=ON`. Debug and Release configure
each exited 0. The build of `qindaqt_terminal_support`, the production
`qindaqt_terminal_adapter`, `qindaqt-terminal`, and all five C++ test targets
exited 0 in both configurations.

With `DISPLAY`, `WAYLAND_DISPLAY`, and the caller's `QT_QPA_PLATFORM` absent,
the registered selector passed **8/8** in Debug and **8/8** in Release, exit 0
both times. Direct offscreen binaries independently report, in both builds:

- launch policy 14/14;
- real PTY bridge 8/8;
- session state machine 17/17;
- appearance 7/7;
- production-window seam 14/14;
- total 60/60, exit 0.

`git diff --check`, `tools/check-source-shape` (1030 files; largest owned test
497 nonblank), `tools/validate-docs` (66 documents), and strict MkDocs to the
external `/mnt/d` site all exit 0.

## Material provenance finding

The handoff's claimed sorted name-status SHA-256 `eea0f078…` is mislabeled:
that value is the sorted **path-only** hash. The actual sorted name-status hash
is `b32f4244…`. Exact candidate/tree/parent and the 13-path set reproduce, so
this is evidence bookkeeping rather than a source mutation.

The code audit confirms each prior repair hunk and associated regression is
present. I am now running proportionate hostile negative controls and checking
the unchanged surrounding lifecycle/PTY contracts before issuing one exact
verdict. A compiled source/test pass will not be represented as live-adapter
qualification: real shell rendering/input/resize/selection/signal/first-frame/
PSS evidence is still absent and remains an explicit serialized next gate.
