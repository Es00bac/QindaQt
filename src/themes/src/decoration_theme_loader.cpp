// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/themes/decoration_theme_loader.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>

namespace QindaQt::Themes {
namespace {

DecorationThemeLoadResult failure(const QString &origin, const QString &message)
{
    return {.ok = false, .theme = {}, .error = origin + QStringLiteral(": ") + message};
}

bool isValidId(const QString &name)
{
    if (name.isEmpty() || name.size() > 128 || name.contains(QStringLiteral(".."))) {
        return false;
    }
    for (const QChar character : name) {
        const ushort value = character.unicode();
        const bool accepted = (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z')
            || (value >= '0' && value <= '9') || value == '.' || value == '_' || value == '-';
        if (!accepted) {
            return false;
        }
    }
    return true;
}

bool readColor(const QJsonObject &object, const QString &name, QColor *destination,
               QString *error)
{
    if (!object.contains(name)) {
        return true;
    }
    const QColor color(object.value(name).toString());
    if (!object.value(name).isString() || !color.isValid()) {
        *error = QStringLiteral("invalid color: %1").arg(name);
        return false;
    }
    *destination = color;
    return true;
}

bool readNumber(const QJsonObject &object, const QString &name, double minimum, double maximum,
                double *destination, QString *error)
{
    if (!object.contains(name)) {
        return true;
    }
    const QJsonValue value = object.value(name);
    if (!value.isDouble() || value.toDouble() < minimum || value.toDouble() > maximum) {
        *error = QStringLiteral("%1 must be a number between %2 and %3")
                     .arg(name).arg(minimum).arg(maximum);
        return false;
    }
    *destination = value.toDouble();
    return true;
}

bool readToken(const QJsonObject &object, const QString &name, const QStringList &accepted,
               QString *destination, QString *error)
{
    if (!object.contains(name)) {
        return true;
    }
    const auto value = object.value(name).toString();
    if (!object.value(name).isString() || !accepted.contains(value)) {
        *error = QStringLiteral("%1 must be one of: %2").arg(name, accepted.join(QLatin1String(", ")));
        return false;
    }
    *destination = value;
    return true;
}

} // namespace

DecorationThemeLoadResult DecorationThemeLoader::fromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return failure(path, file.errorString());
    }
    return fromJson(file.readAll(), path);
}

DecorationThemeLoadResult DecorationThemeLoader::fromJson(const QByteArray &json,
                                                          const QString &origin)
{
    QJsonParseError parseError;
    const auto document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return failure(origin, QStringLiteral("invalid JSON: %1").arg(parseError.errorString()));
    }
    const auto root = document.object();
    DecorationThemeSpec theme;
    theme.schemaVersion = root.value(QStringLiteral("schemaVersion")).toInt(-1);
    theme.id = root.value(QStringLiteral("id")).toString();
    theme.name = root.value(QStringLiteral("name")).toString();
    theme.description = root.value(QStringLiteral("description")).toString();
    if (theme.schemaVersion != 1 || !isValidId(theme.id) || theme.name.isEmpty()) {
        return failure(origin, QStringLiteral("decoration theme requires schemaVersion 1, "
                                              "a valid id, and a name"));
    }
    // Decoration documents never carry the schema-v1 default colors: absent
    // colors defer to the color theme, so start from invalid colors.
    theme.decoration.authored = true;
    theme.decoration.closeColor = QColor();
    theme.decoration.minimizeColor = QColor();
    theme.decoration.maximizeColor = QColor();

    QString error;
    if (!readToken(root, QStringLiteral("buttonPlacement"),
                   {QStringLiteral("left"), QStringLiteral("right")},
                   &theme.decoration.buttonPlacement, &error)
        || !readToken(root, QStringLiteral("tabDirection"),
                      {QStringLiteral("left-to-right"), QStringLiteral("right-to-left")},
                      &theme.decoration.tabDirection, &error)
        || !readToken(root, QStringLiteral("buttonStyle"),
                      {QStringLiteral("symbols"), QStringLiteral("traffic-lights"),
                       QStringLiteral("glyph")},
                      &theme.decoration.buttonStyle, &error)
        || !readToken(root, QStringLiteral("memberHandle"),
                      DecorationThemeTokens::memberHandleStyles(), &theme.memberHandleStyle,
                      &error)
        || !readToken(root, QStringLiteral("containerBadge"),
                      DecorationThemeTokens::containerBadgeStyles(),
                      &theme.containerBadgeStyle, &error)) {
        return failure(origin, error);
    }
    if (root.contains(QStringLiteral("hoverGlyphs"))) {
        if (!root.value(QStringLiteral("hoverGlyphs")).isBool()) {
            return failure(origin, QStringLiteral("hoverGlyphs must be a boolean"));
        }
        theme.decoration.hoverGlyphs = root.value(QStringLiteral("hoverGlyphs")).toBool();
    }
    for (const auto &[key, target] : {
             std::pair{QStringLiteral("closeColor"), &theme.decoration.closeColor},
             std::pair{QStringLiteral("minimizeColor"), &theme.decoration.minimizeColor},
             std::pair{QStringLiteral("maximizeColor"), &theme.decoration.maximizeColor},
             std::pair{QStringLiteral("restoreColor"), &theme.decoration.restoreColor},
             std::pair{QStringLiteral("titleBarColor"), &theme.decoration.titleBarColor},
             std::pair{QStringLiteral("titleBarInactiveColor"),
                       &theme.decoration.titleBarInactiveColor}}) {
        if (!readColor(root, key, target, &error)) {
            return failure(origin, error);
        }
    }
    const auto material = root.value(QStringLiteral("material"));
    if (!material.isUndefined()) {
        if (!material.isObject()) {
            return failure(origin, QStringLiteral("material must be an object"));
        }
        const auto object = material.toObject();
        theme.titleMaterial.authored = true;
        bool ok = readNumber(object, QStringLiteral("opacity"), 0.0, 1.0,
                             &theme.titleMaterial.opacity, &error)
            && readNumber(object, QStringLiteral("border"), 0.0, 1.0,
                          &theme.titleMaterial.border, &error)
            && readNumber(object, QStringLiteral("shadow"), 0.0, 2.0,
                          &theme.titleMaterial.shadow, &error)
            && readColor(object, QStringLiteral("tint"), &theme.titleMaterial.tint, &error);
        for (const auto &[key, target] : {std::pair{QStringLiteral("blur"), &theme.titleMaterial.blur},
                                          std::pair{QStringLiteral("highlight"),
                                                    &theme.titleMaterial.highlight}}) {
            if (ok && object.contains(key)) {
                if (!object.value(key).isBool()) {
                    error = QStringLiteral("material.%1 must be a boolean").arg(key);
                    ok = false;
                } else {
                    *target = object.value(key).toBool();
                }
            }
        }
        if (!ok) {
            return failure(origin, error);
        }
    }
    if (root.contains(QStringLiteral("cornerRadius"))) {
        const int radius = root.value(QStringLiteral("cornerRadius")).toInt(-1);
        if (!root.value(QStringLiteral("cornerRadius")).isDouble() || radius < 0 || radius > 32) {
            return failure(origin, QStringLiteral("cornerRadius must be 0 to 32"));
        }
        theme.cornerRadius = radius;
    }
    const auto shadow = root.value(QStringLiteral("shadow"));
    if (!shadow.isUndefined()) {
        if (!shadow.isObject()
            || !readNumber(shadow.toObject(), QStringLiteral("extent"), 0.0, 48.0,
                           &theme.shadowExtent, &error)
            || !readNumber(shadow.toObject(), QStringLiteral("opacity"), 0.0, 1.0,
                           &theme.shadowOpacity, &error)) {
            return failure(origin, error.isEmpty() ? QStringLiteral("shadow must be an object")
                                                   : error);
        }
    }
    return {.ok = true, .theme = theme, .error = {}};
}

QVector<DecorationThemeLoadResult> DecorationThemeLoader::fromDirectory(const QString &path)
{
    QDir directory(path);
    const auto names = directory.entryList({QStringLiteral("*.json")}, QDir::Files, QDir::Name);
    QVector<DecorationThemeLoadResult> results;
    results.reserve(names.size());
    for (const auto &name : names) {
        results.append(fromFile(directory.filePath(name)));
    }
    return results;
}

std::optional<QVector<DecorationThemeSpec>>
DecorationThemeLoader::loadDirectories(const QStringList &directories, QString *error)
{
    QVector<DecorationThemeSpec> documents;
    QSet<QString> ids;
    for (const auto &directory : directories) {
        for (const auto &result : fromDirectory(directory)) {
            if (!result.ok) {
                if (error != nullptr) {
                    *error = result.error;
                }
                return std::nullopt;
            }
            if (ids.contains(result.theme.id)) {
                continue;
            }
            ids.insert(result.theme.id);
            documents.append(result.theme);
        }
    }
    return documents;
}

std::optional<DecorationThemeSpec>
DecorationThemeLoader::find(const QVector<DecorationThemeSpec> &documents, const QString &id)
{
    for (const auto &document : documents) {
        if (document.id == id) {
            return document;
        }
    }
    return std::nullopt;
}

} // namespace QindaQt::Themes
