// SPDX-License-Identifier: GPL-3.0-or-later

#include "sloommenuendpointselector.h"

#include <QStringView>

namespace QindaQt::Compositor::KWinIntegration
{
namespace
{

QString normalizedIdentity(const QString &value)
{
    QString normalized = value.trimmed().toLower();
    const qsizetype slash = normalized.lastIndexOf(u'/');
    if (slash >= 0) {
        normalized = normalized.sliced(slash + 1);
    }
    if (normalized.endsWith(QStringLiteral(".desktop"))) {
        normalized.chop(QStringView(u".desktop").size());
    }
    return normalized;
}

bool isSloomIdentity(const QString &desktopFileName, const QString &resourceClass)
{
    const QString desktopId = normalizedIdentity(desktopFileName);
    const QString classId = normalizedIdentity(resourceClass);
    return desktopId == QStringLiteral("sloom-studio")
        || classId == QStringLiteral("sloom-studio")
        || classId == QStringLiteral("signal-loom")
        || classId == QStringLiteral("signal loom")
        || classId == QStringLiteral("signalloom")
        || classId == QStringLiteral("sloom studio")
        || classId == QStringLiteral("studio.sloom.signalloom");
}

} // namespace

std::optional<SloomMenuEndpoint> selectSloomMenuEndpoint(
    const QString &desktopFileName, const QString &resourceClass,
    const QString &announcedServiceName, const QString &announcedObjectPath)
{
    // AGENT-GUARD: a partially announced KDE appmenu is malformed identity
    // data, not permission to substitute Sloom's endpoint. Keep it absent so
    // the public identity contract fails closed instead of changing providers.
    if (!announcedServiceName.isEmpty() || !announcedObjectPath.isEmpty()
        || !isSloomIdentity(desktopFileName, resourceClass)) {
        return std::nullopt;
    }
    return SloomMenuEndpoint{QStringLiteral("org.signalloom.PanelMenu"),
                             QStringLiteral("/org/signalloom/menus/active")};
}

} // namespace QindaQt::Compositor::KWinIntegration
