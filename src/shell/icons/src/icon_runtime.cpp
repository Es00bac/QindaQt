// SPDX-License-Identifier: LGPL-3.0-or-later

#include <qindaqt/shell/icons/icon_runtime.h>

#include <qindaqt/shell/icons/icon_image_provider.h>
#include <qindaqt/shell/icons/icon_lookup.h>
#include <qindaqt/shell/icons/icon_theme_locator.h>

#include <QQmlEngine>
#include <QStandardPaths>

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

QString wineCacheIconRoot()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
        + QStringLiteral("/qindaqt/wine-icons");
}

QString wineCacheIconNameForAppId(const QString &applicationId)
{
    // AGENT-CONTRACT: byte-identical to
    // QindaQt::Compositor::WineIdentityCache::cacheIconNameForApplicationId.
    // The grammar is the freedesktop icon-name subset the locator confines to
    // ([a-z0-9._-], no ".."); anything outside maps to '-', a `.exe` suffix
    // is dropped, and the result carries the reserved prefix so it can never
    // shadow a real themed icon.
    QString base = applicationId.trimmed();
    if (base.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
        base.chop(4);
    }
    QString mapped;
    mapped.reserve(base.size());
    for (const QChar character : base.toLower()) {
        const char16_t code = character.unicode();
        const bool acceptable = (code >= 'a' && code <= 'z')
            || (code >= '0' && code <= '9') || character == QLatin1Char('_')
            || character == QLatin1Char('-') || character == QLatin1Char('.');
        mapped.append(acceptable ? character : QLatin1Char('-'));
    }
    const QString prefix = QStringLiteral("qindaqt-wine-");
    if (mapped.isEmpty() || mapped.contains(QStringLiteral(".."))) {
        return {};
    }
    // The locator refuses names over 128 UTF-8 bytes; the mapping is pure
    // ASCII, so character count equals byte count.
    const qsizetype budget = 128 - prefix.size();
    if (mapped.size() > budget) {
        return {};
    }
    return prefix + mapped;
}

} // namespace QindaQt::Shell::Icons::IconRuntime
