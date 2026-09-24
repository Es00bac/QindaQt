// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/themes/theme_loader.h"

#include "qindaqt/themes/decoration_theme_spec.h"

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

bool isValidDocumentId(const QString &name);

bool readBoundedNumber(const QJsonObject &object, const QString &name, double minimum,
                       double maximum, double *destination, QString *error)
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

bool readOptionalBool(const QJsonObject &object, const QString &name, bool *destination,
                      QString *error)
{
    if (!object.contains(name)) {
        return true;
    }
    if (!object.value(name).isBool()) {
        *error = QStringLiteral("%1 must be a boolean").arg(name);
        return false;
    }
    *destination = object.value(name).toBool();
    return true;
}

// Schema v2 sections (ADR-0206). A v1 document must not carry them: the
// round-trip proof for v1 files would otherwise silently accept keys the
// v1 consumers ignore.
bool readSchemaV2(const QJsonObject &root, ThemeSpec *theme, QString *error)
{
    const QStringList v2Keys{QStringLiteral("surfaces"), QStringLiteral("radii"),
                             QStringLiteral("motion"), QStringLiteral("accent"),
                             QStringLiteral("decorationTheme")};
    if (theme->schemaVersion == 1) {
        for (const auto &key : v2Keys) {
            if (root.contains(key)) {
                *error = QStringLiteral("schema version 1 does not accept '%1'").arg(key);
                return false;
            }
        }
        return true;
    }
    const auto surfaceNames = SurfaceNames::all();
    const QJsonValue surfaces = root.value(QStringLiteral("surfaces"));
    if (!surfaces.isUndefined()) {
        if (!surfaces.isObject()) {
            *error = QStringLiteral("surfaces must be an object");
            return false;
        }
        const auto object = surfaces.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            if (!surfaceNames.contains(it.key()) || !it.value().isObject()) {
                *error = QStringLiteral("unknown or malformed surface: %1").arg(it.key());
                return false;
            }
            const auto entry = it.value().toObject();
            SurfaceMaterialSpec material;
            material.authored = true;
            if (!readBoundedNumber(entry, QStringLiteral("opacity"), 0.0, 1.0,
                                   &material.opacity, error)
                || !readOptionalBool(entry, QStringLiteral("blur"), &material.blur, error)
                || !readBoundedNumber(entry, QStringLiteral("border"), 0.0, 1.0,
                                      &material.border, error)
                || !readOptionalBool(entry, QStringLiteral("highlight"), &material.highlight,
                                     error)
                || !readBoundedNumber(entry, QStringLiteral("shadow"), 0.0, 2.0,
                                      &material.shadow, error)
                || !readOptionalColor(entry, QStringLiteral("tint"), &material.tint, error)) {
                *error = QStringLiteral("surface %1: %2").arg(it.key(), *error);
                return false;
            }
            theme->surfaces.insert(it.key(), material);
        }
    }
    const QJsonValue radii = root.value(QStringLiteral("radii"));
    if (!radii.isUndefined()) {
        if (!radii.isObject()) {
            *error = QStringLiteral("radii must be an object");
            return false;
        }
        const auto object = radii.toObject();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            const int radius = it.value().toInt(-1);
            if (!surfaceNames.contains(it.key()) || !it.value().isDouble() || radius < 0
                || radius > 32) {
                *error = QStringLiteral("radii.%1 must name a surface with a radius of 0 to 32")
                             .arg(it.key());
                return false;
            }
            theme->radii.insert(it.key(), radius);
        }
    }
    const QJsonValue motion = root.value(QStringLiteral("motion"));
    if (!motion.isUndefined()) {
        if (!motion.isObject()) {
            *error = QStringLiteral("motion must be an object");
            return false;
        }
        const auto object = motion.toObject();
        const auto motionNames = MotionNames::all();
        const auto easings = MotionNames::easings();
        for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
            if (!motionNames.contains(it.key()) || !it.value().isObject()) {
                *error = QStringLiteral("unknown or malformed motion: %1").arg(it.key());
                return false;
            }
            const auto entry = it.value().toObject();
            MotionSpec spec;
            spec.authored = true;
            spec.duration = theme->motionDuration;
            if (entry.contains(QStringLiteral("duration"))) {
                const int duration = entry.value(QStringLiteral("duration")).toInt(-1);
                if (!entry.value(QStringLiteral("duration")).isDouble() || duration < 0
                    || duration > 1000) {
                    *error = QStringLiteral("motion.%1.duration must be 0 to 1000 ms")
                                 .arg(it.key());
                    return false;
                }
                spec.duration = duration;
            }
            if (entry.contains(QStringLiteral("easing"))) {
                const auto easing = entry.value(QStringLiteral("easing")).toString();
                if (!easings.contains(easing)) {
                    *error = QStringLiteral("motion.%1.easing is not a known easing")
                                 .arg(it.key());
                    return false;
                }
                spec.easing = easing;
            }
            theme->motions.insert(it.key(), spec);
        }
    }
    const QJsonValue accent = root.value(QStringLiteral("accent"));
    if (!accent.isUndefined()) {
        const auto mode = accent.toObject().value(QStringLiteral("mode")).toString();
        if (!accent.isObject() || (mode != QLatin1String("fixed")
                                   && mode != QLatin1String("wallpaper"))) {
            *error = QStringLiteral("accent.mode must be fixed or wallpaper");
            return false;
        }
        theme->accentMode = mode;
    }
    const QJsonValue decorationTheme = root.value(QStringLiteral("decorationTheme"));
    if (!decorationTheme.isUndefined()) {
        if (!decorationTheme.isString() || !isValidDocumentId(decorationTheme.toString())) {
            *error = QStringLiteral("decorationTheme must be a decoration document id");
            return false;
        }
        theme->decorationTheme = decorationTheme.toString();
    }
    return true;
}

bool isValidIconThemeName(const QString &name)
{
    if (name.isEmpty() || name.size() > 128 || name.contains(QStringLiteral(".."))) {
        return false;
    }
    for (const QChar character : name) {
        const ushort value = character.unicode();
        const bool accepted = (value >= 'A' && value <= 'Z')
            || (value >= 'a' && value <= 'z')
            || (value >= '0' && value <= '9') || value == '.' || value == '_'
            || value == '-';
        if (!accepted) {
            return false;
        }
    }
    return true;
}

bool isValidDocumentId(const QString &name)
{
    // Same bounded grammar as icon theme names: the id becomes a file name.
    return isValidIconThemeName(name);
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
    const QJsonValue iconTheme = root.value(QStringLiteral("iconTheme"));
    if (!iconTheme.isUndefined()) {
        if (!iconTheme.isString() || !isValidIconThemeName(iconTheme.toString())) {
            return failure(origin, QStringLiteral("invalid iconTheme"));
        }
        theme.iconTheme = iconTheme.toString();
    }
    theme.cornerRadius = root.value(QStringLiteral("cornerRadius")).toInt(theme.cornerRadius);
    theme.motionDuration = root.value(QStringLiteral("motionDuration")).toInt(theme.motionDuration);
    theme.blurEnabled = root.value(QStringLiteral("blurEnabled")).toBool(theme.blurEnabled);

    const auto decoration = root.value(QStringLiteral("decoration")).toObject();
    theme.decoration.authored = root.value(QStringLiteral("decoration")).isObject();
    theme.decoration.buttonPlacement =
        decoration.value(QStringLiteral("buttonPlacement")).toString(theme.decoration.buttonPlacement);
    theme.decoration.tabDirection =
        decoration.value(QStringLiteral("tabDirection")).toString(theme.decoration.tabDirection);
    theme.decoration.buttonStyle =
        decoration.value(QStringLiteral("buttonStyle")).toString(theme.decoration.buttonStyle);
    theme.decoration.hoverGlyphs =
        decoration.value(QStringLiteral("hoverGlyphs")).toBool(theme.decoration.hoverGlyphs);

    if ((theme.schemaVersion != 1 && theme.schemaVersion != 2) || theme.id.isEmpty()
        || theme.name.isEmpty() || theme.variant.isEmpty()) {
        return failure(origin, QStringLiteral("theme requires schemaVersion 1 or 2 plus id, name, and variant"));
    }
    QString schemaError;
    if (!readSchemaV2(root, &theme, &schemaError)) {
        return failure(origin, schemaError);
    }
    if (theme.cornerRadius < 0 || theme.cornerRadius > 32 || theme.motionDuration < 0
        || theme.motionDuration > 1000) {
        return failure(origin, QStringLiteral("theme metrics are outside supported bounds"));
    }
    if (!contains(theme.decoration.buttonPlacement, buttonPlacements)
        || !contains(theme.decoration.tabDirection, tabDirections)
        || !DecorationThemeTokens::buttonStyles().contains(theme.decoration.buttonStyle)) {
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
                              &decorationError)
        || !readOptionalColor(decoration,
                              QStringLiteral("titleBarColor"),
                              &theme.decoration.titleBarColor,
                              &decorationError)
        || !readOptionalColor(decoration,
                              QStringLiteral("titleBarInactiveColor"),
                              &theme.decoration.titleBarInactiveColor,
                              &decorationError)
        || !readOptionalColor(decoration,
                              QStringLiteral("restoreColor"),
                              &theme.decoration.restoreColor,
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
