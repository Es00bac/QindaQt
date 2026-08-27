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
        QStringLiteral("qindaqt.applets.clock"),
        QStringLiteral("qindaqt.applets.notification-center"),
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
