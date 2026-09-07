# ADR-0098: Gate releases on the exact native compositor stack

- Status: Accepted
- Date: 2026-09-07

## Context

QindaQt ships a native KWin plugin. A build with that plugin disabled can prove
the LayerShellQt client boundary, but it cannot prove that the released desktop
will load against KWin's private binary ABI. Rolling CI previously built only
plugin-disabled configurations, while the source manifest, patch-series base,
and documentation could drift independently.

The Gentoo package is also a release boundary. It must install one coherent
desktop from an immutable QindaQt source and an exact KWin/Plasma patch stack;
otherwise Portage can produce a package that builds but cannot load its native
plugin after a partial system upgrade.

## Decision

Every releasable commit has one exact KWin release, annotated tag object,
peeled commit, and tree in `compositor/upstream/kwin.json`. The downstream patch
series targets that peeled commit. Documentation and the full-desktop ebuild
name the same release. `tools/check-release-contract` rejects drift among these
surfaces and rejects release builds without the plugin and production shell.

CI retains dependency-light and public LayerShellQt lanes, then adds a native
release lane on a fixed Gentoo stage and exact Portage repository revision. The
lane accepts binary packages only, requires KWin 6.6.6 and its matching Plasma
libraries, configures the native plugin, builds the complete tree with at most
two compile jobs, and checks the resulting plugin and launcher. It then runs
the ABI/dependency rows and boots both build-tree and staged-install plugin
discovery under nested KWin. Nested rows run sequentially.

`gui-wm/qindaqt-desktop` installs the complete production tree, including the
three bundled applications and their QML runtime plugins. It weakly blocks the
overlapping `gui-apps/qindaqt-apps` package so Portage can replace its ownership
during the full-desktop transition. KWin, KDecoration, LayerShellQt, and Plasma
Activities are exact patch-release dependencies.

The full package directly depends on the authorities its resident processes
contact or launch: BlueZ, UPower, power-profiles-daemon, the portal frontend and
KDE backend, KScreenLocker, Spectacle, the KDE polkit agent, KIdleTime, and
KWayland. SDDM is a site choice rather than part of the runtime contract.

## Consequences

A rolling bridge build remains useful but cannot authorize a release. A new
KWin patch release requires an explicit pin update, a clean native rebuild, and
fresh sequential nested evidence. If the dated CI image, repository snapshot,
or compatible binary packages disappear, the native lane fails visibly and the
release remains blocked until maintainers advance and requalify that environment.

The package gives Portage complete dependency and file ownership. Operators must
upgrade the compositor stack and QindaQt package as one planned transaction,
outside a running QindaQt session, and retain a prior binary package for
rollback. See the [release procedure](../development/releases.md), [KWin upgrade
procedure](../development/kwin-upgrades.md), and [Gentoo desktop
installation](../development/gentoo-desktop.md).
