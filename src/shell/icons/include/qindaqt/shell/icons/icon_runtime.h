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
// locator). Calling install() twice on one engine replaces the lookup
// locator; the first provider stays owned by the engine and must not be
// re-added, so install() refuses a second call and returns false. With no
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

} // namespace QindaQt::Shell::Icons::IconRuntime
