// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/applet_runtime/builtin_applet_registry.h"

#include <algorithm>
#include <utility>

namespace QindaQt::AppletRuntime {

BuiltinAppletRegistry::BuiltinAppletRegistry(QStringList auditedEntryPoints)
    : m_entryPoints(auditedEntryPoints.cbegin(), auditedEntryPoints.cend())
{
}

BuiltinAppletRegistry BuiltinAppletRegistry::firstParty()
{
    // AGENT-CONTRACT: this list asserts that executable UI exists in this
    // build, not merely that a manifest has been reviewed. Keep an entry out
    // until its concrete runtime implementation and authority path ship.
    return BuiltinAppletRegistry({
        QStringLiteral("qindaqt.applets.audio"),
        QStringLiteral("qindaqt.applets.bluetooth"),
        QStringLiteral("qindaqt.applets.clock"),
        QStringLiteral("qindaqt.applets.notification-center"),
        QStringLiteral("qindaqt.applets.power"),
        QStringLiteral("qindaqt.applets.launcher"),
        QStringLiteral("qindaqt.applets.global-menu"),
        QStringLiteral("qindaqt.applets.clipboard"),
        QStringLiteral("qindaqt.applets.task-list"),
        QStringLiteral("qindaqt.applets.status-notifier"),
        // Desktop controls (docs/wiki/shell/desktop-controls.md): each entry
        // is rendered by the compiled QindaQt.Shell.DesktopControls module
        // over an existing shell facade or the authenticated workspace adapter.
        QStringLiteral("qindaqt.applets.active-application"),
        QStringLiteral("qindaqt.applets.application-tiles"),
        QStringLiteral("qindaqt.applets.command-hud"),
        QStringLiteral("qindaqt.applets.command-palette"),
        QStringLiteral("qindaqt.applets.dashboard"),
        QStringLiteral("qindaqt.applets.overview-trigger"),
        QStringLiteral("qindaqt.applets.places-menu"),
        QStringLiteral("qindaqt.applets.quick-launch"),
        QStringLiteral("qindaqt.applets.show-desktop"),
        QStringLiteral("qindaqt.applets.system-menu"),
        QStringLiteral("qindaqt.applets.system-status"),
        QStringLiteral("qindaqt.applets.workspace-switcher"),
        QStringLiteral("qindaqt.applets.workspace-tiles"),
        // Worn Luna desktop experience (ADR-0124/ADR-0125): the start-menu
        // entry renders from the compiled QindaQt.Shell.StartMenu module; the
        // desktop-icons entry renders on the dedicated desktop surface from
        // QindaQt.Shell.DesktopSurface.
        QStringLiteral("qindaqt.applets.start-menu"),
        QStringLiteral("qindaqt.applets.desktop-icons"),
    });
}

bool BuiltinAppletRegistry::contains(QStringView entryPoint) const
{
    return m_entryPoints.contains(entryPoint.toString());
}

QStringList BuiltinAppletRegistry::entryPoints() const
{
    auto result = m_entryPoints.values();
    std::sort(result.begin(), result.end());
    return result;
}

} // namespace QindaQt::AppletRuntime
