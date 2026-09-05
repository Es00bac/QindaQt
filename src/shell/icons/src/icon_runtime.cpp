// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/icons/icon_runtime.h>

#include <qindaqt/shell/icons/icon_image_provider.h>
#include <qindaqt/shell/icons/icon_lookup.h>
#include <qindaqt/shell/icons/icon_theme_locator.h>

#include <QQmlEngine>

#include <memory>

namespace QindaQt::Shell::Icons::IconRuntime
{

bool install(QQmlEngine &engine, const QStringList &iconRoots,
             const QStringList &themeNames)
{
    if (engine.imageProvider(QLatin1String("qindaqt-icon")) != nullptr) {
        return false; // Refuse a second provider for the same engine.
    }
    // The engine takes provider ownership; the QML singleton receives its own
    // GUI-thread locator so the provider's render-thread instance is never
    // shared across threads.
    engine.addImageProvider(QLatin1String("qindaqt-icon"),
                            new IconImageProvider(iconRoots, themeNames));
    IconLookup::installForEngine(
        &engine, std::make_shared<IconThemeLocator>(iconRoots, themeNames));
    return true;
}

QStringList freedesktopIconRoots(const QString &dataHome, const QStringList &dataDirs)
{
    QStringList roots;
    const auto appendIcons = [&roots](const QString &base) {
        const QString trimmed = base.trimmed();
        if (!trimmed.isEmpty()) {
            roots.append(trimmed + QStringLiteral("/icons"));
        }
    };
    appendIcons(dataHome);
    for (const QString &dir : dataDirs) {
        appendIcons(dir);
    }
    return roots;
}

QStringList freedesktopApplicationRoots(const QString &dataHome, const QStringList &dataDirs)
{
    QStringList roots;
    const auto appendApplications = [&roots](const QString &base) {
        const QString trimmed = base.trimmed();
        if (!trimmed.isEmpty()) {
            roots.append(trimmed + QStringLiteral("/applications"));
        }
    };
    appendApplications(dataHome);
    for (const QString &dir : dataDirs) {
        appendApplications(dir);
    }
    return roots;
}

} // namespace QindaQt::Shell::Icons::IconRuntime
