// SPDX-License-Identifier: LGPL-3.0-or-later

#pragma once

#include <QtCore/QStringList>

class QQmlEngine;

namespace QindaQt::Shell::Icons::IconRuntime
{

// AGENT-CONTRACT: The composition seam through which the shell wires shell
// iconography into one QQmlEngine. It installs the `image://qindaqt-icon/`
// provider (ownership passes to the engine) and the per-engine `IconLookup`
// singleton locator. `themeNames` is the injected theme preference chain
// (production default: `breeze`; `hicolor` is always appended last by the
// locator). Calling install() twice on one engine is refused: the second
// call changes nothing and returns false, because the first provider stays
// owned by the engine and must not be re-added. With no
// installation, or with empty roots, every lookup fails closed to the
// deterministic placeholder.
//
// The wiring lane feeds `themeNames` from the shell theme JSON's optional
// `iconTheme` key (docs/wiki/shell/iconography.md).
[[nodiscard]] bool install(QQmlEngine &engine, const QStringList &iconRoots,
                           const QStringList &themeNames);

// Pure helpers for composition roots: `<dataHome>/icons` followed by
// `<dataDirs[i]>/icons`, in order. `~/.icons` is deliberately never added;
// the XDG icon-theme specification deprecates it and confinement policy
// admits only explicit roots.
[[nodiscard]] QStringList freedesktopIconRoots(const QString &dataHome,
                                               const QStringList &dataDirs);

// Same ordering for the desktop-entry resolver: `<root>/applications`.
[[nodiscard]] QStringList freedesktopApplicationRoots(const QString &dataHome,
                                                      const QStringList &dataDirs);

// ADR-0230: the shared cache root the compositor writes PE-extracted Wine
// icons into (`<generic cache location>/qindaqt/wine-icons`). Composition
// appends it LAST to the icon roots so every real themed icon wins; the
// locator's unthemed direct-root rule then resolves the files it holds.
// App-independent on purpose: the compositor (KWin) and the shell have
// different application names.
[[nodiscard]] QString wineCacheIconRoot();

// The icon name a Wine/Proton window's cached PE icon is stored under for an
// application id (`Battle.net.exe` -> `qindaqt-wine-battle.net`), or empty
// when the id cannot yield a confined name. AGENT-CONTRACT with
// QindaQt::Compositor::WineIdentityCache::cacheIconNameForApplicationId: the
// two implementations MUST stay byte-identical, pinned by the same test
// vectors, because the compositor writes the file and the shell looks it up
// by name (ADR-0230). Pure: no filesystem reach.
[[nodiscard]] QString wineCacheIconNameForAppId(const QString &applicationId);

} // namespace QindaQt::Shell::Icons::IconRuntime
