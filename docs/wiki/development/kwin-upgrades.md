# Upgrading the pinned KWin release

KWin's native plugin boundary is an exact binary ABI. Treat every patch release
as an upgrade that needs a clean rebuild and fresh nested evidence.

## Resolve the authoritative source

Start from KDE's repository and resolve both the annotated tag and its peeled
commit:

```sh
git ls-remote https://invent.kde.org/plasma/kwin.git \
  refs/tags/vX.Y.Z 'refs/tags/vX.Y.Z^{}'
git init build/kwin-X.Y.Z
git -C build/kwin-X.Y.Z fetch --depth=1 \
  https://invent.kde.org/plasma/kwin.git refs/tags/vX.Y.Z
git -C build/kwin-X.Y.Z cat-file -p FETCH_HEAD
```

For an annotated tag, the first object is the tag and the second is the commit.
Resolve the tree from the peeled commit. Compare these values with the locally
installed KWin runtime and `KWinConfigVersion.cmake`; the local package proves
what can be compiled and run, while the upstream objects prove source identity.

Update `compositor/upstream/kwin.json`, then rebase every entry in
`compositor/patches/series.json` onto the peeled commit. Preserve patch order and
intent, regenerate each patch hash, and run both source-verifier modes. An empty
series still needs its `upstreamCommit` changed. Update ADR-0001 and the
compositor-session pin table with the same tag, commit, and tree.

## Advance the binary contract

Update the exact `find_package(KWin ...)` request, plugin IID, dependency
contract tests, and the full-desktop ebuild's KWin, KDecoration, LayerShellQt,
and Plasma Activities atoms as one change. Confirm the selected distribution
packages form one patch-release stack. Never relax `EXACT`, keep an old IID, or
reuse an old plugin artifact to get configuration past a mismatch.

Run the complete [release procedure](releases.md) from a fresh build root. The
required evidence includes static ABI checks, live build-tree plugin loading,
and staged installed discovery. A plugin-disabled LayerShellQt run does not
replace either native row.

## Upgrade and rollback on Gentoo

Create the updated `gui-wm/qindaqt-desktop` binary package before touching the
running system. Review the Portage plan so the four exact Plasma atoms and the
QindaQt package advance together. Leave the QindaQt session, merge from a text
console or another desktop, and start a fresh login; an in-process plugin cannot
survive a KWin ABI replacement safely.

If the new session fails its installed smoke, return to the text console and
install the retained prior KWin stack and QindaQt binary package as one rollback
transaction. Do not mix the prior plugin with the new KWin process. Capture the
failed package versions and nested reproduction before retrying the upgrade.
