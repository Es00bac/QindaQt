// SPDX-License-Identifier: GPL-3.0-or-later
#include "minimizedapplicationidentity.h"

#include "qindaqt/compositor/foreignwindowidentity.h"

#include <QFileInfo>

namespace QindaQt::Compositor::KWinIntegration {
namespace {

QString iconId(const QString &desktopFileName)
{
    const QString base = QFileInfo(desktopFileName).fileName();
    return base.endsWith(QStringLiteral(".desktop"), Qt::CaseInsensitive)
        ? base.left(base.size() - QStringLiteral(".desktop").size()) : base;
}

bool isGenericClass(const QString &resourceClass)
{
    const QString lowered = resourceClass.trimmed().toLower();
    return lowered.isEmpty() || lowered == QStringLiteral("wayland")
        || lowered == QStringLiteral("xwayland")
        || lowered == QStringLiteral("unknown");
}

void appendUnique(QStringList *values, const QString &value)
{
    const QString candidate = value.trimmed();
    if (!candidate.isEmpty() && !values->contains(candidate)) {
        values->append(candidate);
    }
}

} // namespace

bool isGenericMinimizedIconName(const QString &iconName)
{
    const QString name = iconName.trimmed().toLower();
    return name.isEmpty() || name == QStringLiteral("wayland")
        || name == QStringLiteral("xwayland") || name == QStringLiteral("unknown")
        || name == QStringLiteral("application")
        || name == QStringLiteral("application-x-executable")
        || name == QStringLiteral("application-x-generic")
        || name == QStringLiteral("application-x-unknown")
        || name == QStringLiteral("application-default-icon");
}

MinimizedApplicationIdentity resolveMinimizedApplicationIdentity(
    const QString &desktopFileName,
    const QString &resourceClass,
    const QString &desktopName,
    const QString &desktopIconName,
    const QString &caption)
{
    MinimizedApplicationIdentity result;
    result.applicationId = ::QindaQt::Compositor::resolveApplicationId(
        desktopFileName, resourceClass, {});
    if (!desktopName.trimmed().isEmpty()) {
        result.label = desktopName.trimmed();
    } else if (!isGenericClass(resourceClass)) {
        result.label = ::QindaQt::Compositor::resolveApplicationName(
            resourceClass, {}, result.applicationId, {});
    } else {
        result.label = iconId(desktopFileName);
    }

    if (result.label.isEmpty() || isGenericClass(result.label)) {
        result.label = caption.trimmed();
    }
    if (result.label.isEmpty()) {
        result.label = result.applicationId;
    }
    if (result.label.isEmpty()) {
        result.label = QStringLiteral("Window");
    }

    if (!isGenericMinimizedIconName(desktopIconName)) {
        appendUnique(&result.iconThemeCandidates, desktopIconName);
    }
    const QString desktopIcon = iconId(desktopFileName);
    if (!isGenericMinimizedIconName(desktopIcon)) {
        appendUnique(&result.iconThemeCandidates, desktopIcon);
    }
    if (!isGenericClass(resourceClass) && !isGenericMinimizedIconName(resourceClass)) {
        appendUnique(&result.iconThemeCandidates, resourceClass);
    }
    return result;
}

} // namespace QindaQt::Compositor::KWinIntegration
