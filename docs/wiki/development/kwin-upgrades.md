# Releasing and upgrading qindaqt-kwin

QindaQt runs on qindaqt-kwin, its own co-installable KWin fork
([ADR-0291](../adr/0291-run-on-qindaqt-kwin.md)). The native plugin boundary is
an exact binary ABI: treat every fork release, including one that only merges
an upstream patch release, as an upgrade that needs a clean rebuild and fresh
nested evidence.

## Work in the fork

The fork's hub is `qinda:~/git/qindaqt-kwin.git`; read its `AGENTS.md` and
`qindaqt/README.md` first. QindaQt changes are small commits labelled
`qindaqt:`. Upstream enters only as a merge of a KDE release tag:

```sh
git -C <fork worktree> fetch https://invent.kde.org/plasma/kwin.git \
  refs/tags/vX.Y.Z:refs/tags/vX.Y.Z
git -C <fork worktree> merge vX.Y.Z
qindaqt/tools/rename-identity           # re-apply the identity to new upstream code
qindaqt/tools/rename-identity --check
```

Record the tag object and commit in the fork's `qindaqt/UPSTREAM.md`; a
vendored kdecoration update is a `git subtree pull --prefix=kdecoration --squash`
with its own row. Stay on the pinned minor release unless a move is planned.

## Cut a fork release

1. Bump `QINDAQT_KWIN_SERIAL` in the fork's `CMakeLists.txt` (the fork version is
   `<upstream release>.<serial>`, the Gentoo version `<release>_p<serial>`).
2. Build, stage an install and run the fork's gates:
   `qindaqt/tools/check-install-collisions <stage>` (no collisions, no stock
   names), `qindaqt/tools/smoke-test <stage> <out>` (the fork's own names answer,
   only `$XDG_CONFIG_HOME/qindaqt/` files are written) and the helper tests.
3. Push the fork branch, then in container-wm update, as one change,
   `compositor/upstream/kwin.json` (fork commit, tree, version, package), the
   literal in `src/compositor/cmake/QindaQtKWinAbi.cmake`, the fork version in
   `tests/` `find_package(... EXACT)` requests, and the pin tables in
   [compositor and session integration](../architecture/compositor-session.md)
   and ADR-0291. Run `./compositor/tools/verify-kwin-source`, `--verify` against
   a fork checkout and `--verify-archive` against the package tarball.
4. In QindaGentoo add `gui-wm/qindaqt-kwin-<release>_p<serial>` pinning the same
   commit (`git archive --format=tar.gz --prefix=qindaqt-kwin-<version>/`) and
   move `qindaqt-desktop`'s `=gui-wm/qindaqt-kwin-…:=` atom with it.

Never relax `EXACT`, keep an old plugin interface id, or reuse an old plugin
artifact to get configuration past a mismatch. Run the complete [release
procedure](releases.md) from a fresh build root.

## Upgrade and rollback on Gentoo

Build the new `gui-wm/qindaqt-kwin` and `gui-wm/qindaqt-desktop` binary packages
before touching the running system; the `:=` subslot rebuilds every consumer.
Leave the QindaQt session, merge from a text console or another desktop, and
start a fresh login; an in-process plugin cannot survive an ABI replacement.

If the new session fails its installed smoke, return to the text console and
install the retained prior fork and desktop binary packages as one rollback
transaction. Do not mix a prior plugin with a new compositor. A stock
`kde-plasma/kwin` beside it is unaffected either way.

## Corner Bar input regression

The Corner Bar cutout requires the fork's input change as well as the QindaQt
decoration (ADR-0277). A painted transparent strip alone is not acceptance
evidence. The fork's `testDecorationInput` carries
`testTransparentDecorationCutout`; configure a fork tree with
`BUILD_TESTING=ON`, build `testDecorationInput`, and run it through the
private-home/private-bus wrapper with QindaQt's decoration on the plugin path
(the fork loads decorations only from `qindaqt-kwin/decorations`):

```sh
QT_PLUGIN_PATH=<container-wm build>/plugins \
  qindaqt/tools/run-decoration-input <fork build>/bin/testDecorationInput
```

The virtual compositor maps two real Wayland clients. The upper decoration
publishes the process-local cutout contract; a pointer press and release in
that region must reach the lower client's surface without moving the upper
window. A retained title point still targets the upper decoration. Removing
the property, or supplying an invalid type, restores ordinary input. The
fixture uses `Test::waylandSync()` after motion and button delivery to flush
the private client/server transport before assertions. Never replace or restart
the physical compositor to run this test.

On 2026-09-28 the fixture passed in qindaqt-kwin 6.6.6.1 with container-wm's
`org.qindaqt` decoration built against `QindaQtKWinDecoration` (three QtTest
rows including setup/cleanup, 259 ms). Without QindaQt's decoration on the
plugin path the window gets no server-side decoration and the row fails, which
also shows that stock decorations never load in the fork. On 2026-09-27 the
same fixture had passed on the formerly patched stock KWin and failed on the
unpatched library at `!above->hitTest(cutoutPoint)`.
