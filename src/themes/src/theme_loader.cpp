// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/themes/theme_loader.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

#include <cstddef>

namespace QindaQt::Themes {
namespace {

constexpr const char *requiredColors[] = {
    "canvas", "surface", "surfaceRaised", "border", "text", "textMuted", "accent", "accentText", "danger"};
constexpr const char *buttonPlacements[] = {"left", "right"};
constexpr const char *tabDirections[] = {"left-to-right", "right-to-left"};
constexpr const char *buttonStyles[] = {"symbols", "traffic-lights"};

LoadResult failure(const QString &origin, const QString &message)
{
    return {.ok = false, .theme = {}, .error = origin + QStringLiteral(": ") + message};
}

template<std::size_t Size>
bool contains(const QString &value, const char *const (&accepted)[Size])
{
    for (const auto *candidate : accepted) {
        if (value == QLatin1String(candidate)) {
            return true;
        }
    }
    return false;
}

bool readOptionalColor(const QJsonObject &object,
                       const QString &name,
                       QColor *destination,
                       QString *error)
{
    if (!object.contains(name)) {
        return true;
    }
    const QColor color(object.value(name).toString());
    if (!color.isValid()) {
        *error = QStringLiteral("invalid decoration color: %1").arg(name);
        return false;
    }
    *destination = color;
    return true;
}

} // namespace

LoadResult ThemeLoader::fromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return failure(path, file.errorString());
    }
    return fromJson(file.readAll(), path);
}

LoadResult ThemeLoader::fromJson(const QByteArray &json, const QString &origin)
{
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return failure(origin, QStringLiteral("invalid JSON: %1").arg(parseError.errorString()));
    }

    const auto root = document.object();
    ThemeSpec theme;
    theme.schemaVersion = root.value(QStringLiteral("schemaVersion")).toInt(-1);
    theme.id = root.value(QStringLiteral("id")).toString();
    theme.name = root.value(QStringLiteral("name")).toString();
    theme.variant = root.value(QStringLiteral("variant")).toString();
    theme.fontFamily = root.value(QStringLiteral("fontFamily")).toString(theme.fontFamily);
    theme.monoFontFamily = root.value(QStringLiteral("monoFontFamily")).toString(theme.monoFontFamily);
    theme.cornerRadius = root.value(QStringLiteral("cornerRadius")).toInt(theme.cornerRadius);
    theme.motionDuration = root.value(QStringLiteral("motionDuration")).toInt(theme.motionDuration);
    theme.blurEnabled = root.value(QStringLiteral("blurEnabled")).toBool(theme.blurEnabled);

    const auto decoration = root.value(QStringLiteral("decoration")).toObject();
    theme.decoration.buttonPlacement =
        decoration.value(QStringLiteral("buttonPlacement")).toString(theme.decoration.buttonPlacement);
    theme.decoration.tabDirection =
        decoration.value(QStringLiteral("tabDirection")).toString(theme.decoration.tabDirection);
    theme.decoration.buttonStyle =
        decoration.value(QStringLiteral("buttonStyle")).toString(theme.decoration.buttonStyle);
    theme.decoration.hoverGlyphs =
        decoration.value(QStringLiteral("hoverGlyphs")).toBool(theme.decoration.hoverGlyphs);

    if (theme.schemaVersion != 1 || theme.id.isEmpty() || theme.name.isEmpty() || theme.variant.isEmpty()) {
        return failure(origin, QStringLiteral("theme requires schemaVersion 1 plus id, name, and variant"));
    }
    if (theme.cornerRadius < 0 || theme.cornerRadius > 32 || theme.motionDuration < 0
        || theme.motionDuration > 1000) {
        return failure(origin, QStringLiteral("theme metrics are outside supported bounds"));
    }
    if (!contains(theme.decoration.buttonPlacement, buttonPlacements)
        || !contains(theme.decoration.tabDirection, tabDirections)
        || !contains(theme.decoration.buttonStyle, buttonStyles)) {
        return failure(origin, QStringLiteral("theme decoration contains an unknown enum value"));
    }

    QString decorationError;
    if (!readOptionalColor(decoration,
                           QStringLiteral("closeColor"),
                           &theme.decoration.closeColor,
                           &decorationError)
        || !readOptionalColor(decoration,
                              QStringLiteral("minimizeColor"),
                              &theme.decoration.minimizeColor,
                              &decorationError)
        || !readOptionalColor(decoration,
                              QStringLiteral("maximizeColor"),
                              &theme.decoration.maximizeColor,
                              &decorationError)) {
        return failure(origin, decorationError);
    }

    const auto colors = root.value(QStringLiteral("colors")).toObject();
    for (const auto *token : requiredColors) {
        const auto name = QString::fromLatin1(token);
        const QColor color(colors.value(name).toString());
        if (!color.isValid()) {
            return failure(origin, QStringLiteral("missing or invalid color token: %1").arg(name));
        }
        theme.colors.insert(name, color);
    }
    return {.ok = true, .theme = theme, .error = {}};
}

QVector<LoadResult> ThemeLoader::fromDirectory(const QString &path)
{
    QDir directory(path);
    const auto names = directory.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    QVector<LoadResult> results;
    results.reserve(names.size());
    for (const auto &name : names) {
        results.append(fromFile(directory.filePath(name)));
    }
    return results;
}

} // namespace QindaQt::Themes
