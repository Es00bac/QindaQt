# Settings S1 installed-package gate borrows the build QML tree

- From: Noether the 4th, exact candidate reviewer
- Timestamp: 2026-08-28T15:49:43-06:00
- State: blocking material finding; candidate is not accepted
- Exact candidate: `7e6f133e280920f98fcb0ea79385d496b7871bd6`
- Exact tree: `3d0d25fd1ac995e94fafd4ba9d401db6970b6d95`
- Exact parent: `0760e08e1118d6a8b8101f6d17d271d1b766cc96`

`src/apps/settings_center/main.cpp:39-50` embeds the compiled build root and
adds its QML directory whenever the executable directory is equal to **or any
descendant of** that root. `tests/apps/settings_center/check_installed_routes.cmake:11-17`
requires its relocated stage to be a descendant of the same build root. The
installed row therefore cannot prove that the installed payload is complete:
the supposedly sanitized executable adds and may borrow the uninstalled build
QML tree itself.

## Independent contrast reproduction

Using the fresh strict Debug build, I installed `SettingsAppearanceRuntime`
twice, once to
`/mnt/d/QindaQt/builds/settings-s1-noether/debug/repro-package-inroot` and once
to the sibling
`/mnt/d/QindaQt/builds/settings-s1-noether/repro-package-outroot`. I moved the
complete installed `lib/qt6/qml/QindaQt/SettingsApp/Appearance` directory out
of both module paths and launched both copies with empty display/QML/library
overrides, an absent private bus, fresh XDG roots, offscreen Qt, and a three-
second bound.

- descendant/in-root stage: exit 124 (the incomplete app stayed resident by
  loading the build module);
- sibling/out-of-root stage: exit 3 (the same missing installed module failed
  root construction, as required).

The exact stderr artifacts are
`/mnt/d/QindaQt/builds/settings-s1-noether/repro/inroot.err` and
`outroot.err`. The executable also contains the exact embedded Debug build-root
string, confirming the branch predicate used by this contrast.

Repair must distinguish the exact build executable location from a relocated
descendant, not merely clear environment variables. Add a package-poison row
that temporarily withholds one required installed module while the build QML
tree remains present and requires construction failure; then reinstall and
prove both routes from only the complete staged prefix. Preserve the safe
bounded stage cleanup contract. This is independent of the two unavailable-
route accessibility findings already recorded in `1787953446`.
