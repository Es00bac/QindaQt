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

    appendUnique(&result.iconThemeCandidates, desktopIconName);
    appendUnique(&result.iconThemeCandidates, iconId(desktopFileName));
    appendUnique(&result.iconThemeCandidates, resourceClass);
    return result;
}

} // namespace QindaQt::Compositor::KWinIntegration
