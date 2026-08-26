// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/applets/manifest_loader.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>

#include <optional>

namespace QindaQt::Applets {
namespace {

ManifestLoadResult failure(const QString &origin, const QString &message)
{
    return {.ok = false, .manifest = {}, .error = origin + QStringLiteral(": ") + message};
}

template<typename Enum, typename Parser>
bool readEnumArray(const QJsonValue &value,
                   const QString &field,
                   Parser parser,
                   QVector<Enum> *destination,
                   QString *error)
{
    if (!value.isArray()) {
        *error = QStringLiteral("%1 must be an array").arg(field);
        return false;
    }
    for (const QJsonValue &item : value.toArray()) {
        if (!item.isString()) {
            *error = QStringLiteral("%1 entries must be strings").arg(field);
            return false;
        }
        const std::optional<Enum> parsed = parser(item.toString());
        if (!parsed.has_value()) {
            *error = QStringLiteral("%1 contains unknown value '%2'")
                         .arg(field, item.toString());
            return false;
        }
        destination->append(*parsed);
    }
    return true;
}

bool readAxis(const QJsonValue &value,
              const QString &field,
              AxisSizing *destination,
              QString *error)
{
    if (!value.isObject()) {
        *error = QStringLiteral("%1 must be an object").arg(field);
        return false;
    }
    const QJsonObject object = value.toObject();
    const QJsonValue minimum = object.value(QStringLiteral("minimum"));
    const QJsonValue preferred = object.value(QStringLiteral("preferred"));
    if (!minimum.isDouble() || !preferred.isDouble()) {
        *error = QStringLiteral("%1 requires integer minimum and preferred values").arg(field);
        return false;
    }
    destination->minimum = minimum.toInt(-1);
    destination->preferred = preferred.toInt(-1);

    const QJsonValue maximum = object.value(QStringLiteral("maximum"));
    if (!maximum.isUndefined() && !maximum.isNull()) {
        if (!maximum.isDouble()) {
            *error = QStringLiteral("%1.maximum must be an integer or null").arg(field);
            return false;
        }
        destination->maximum = maximum.toInt(-1);
    }
    const QJsonValue stretch = object.value(QStringLiteral("stretch"));
    if (!stretch.isUndefined() && !stretch.isBool()) {
        *error = QStringLiteral("%1.stretch must be a boolean").arg(field);
        return false;
    }
    destination->stretch = stretch.toBool(false);
    return true;
}

QJsonObject axisToJson(const AxisSizing &axis)
{
    QJsonObject object{
        {QStringLiteral("minimum"), axis.minimum},
        {QStringLiteral("preferred"), axis.preferred},
        {QStringLiteral("stretch"), axis.stretch},
    };
    if (axis.maximum.has_value()) {
        object.insert(QStringLiteral("maximum"), *axis.maximum);
    }
    return object;
}

template<typename Enum>
QJsonArray enumArrayToJson(const QVector<Enum> &values)
{
    QJsonArray array;
    for (const Enum value : values) {
        array.append(toString(value));
    }
    return array;
}

} // namespace

ManifestLoadResult ManifestLoader::fromFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return failure(path, file.errorString());
    }
    return fromJson(file.readAll(), path);
}

ManifestLoadResult ManifestLoader::fromJson(const QByteArray &json, const QString &origin)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        return failure(origin, QStringLiteral("invalid JSON: %1").arg(parseError.errorString()));
    }

    const QJsonObject root = document.object();
    AppletManifest manifest;
    manifest.schemaVersion = root.value(QStringLiteral("schemaVersion")).toInt(-1);
    manifest.id = root.value(QStringLiteral("id")).toString();
    manifest.name = root.value(QStringLiteral("name")).toString();
    manifest.description = root.value(QStringLiteral("description")).toString();

    const auto apiVersion = ApiVersion::parse(root.value(QStringLiteral("apiVersion")).toString());
    if (!apiVersion.has_value()) {
        return failure(origin, QStringLiteral("apiVersion must use MAJOR.MINOR notation"));
    }
    manifest.apiVersion = *apiVersion;

    const QJsonValue entryPointValue = root.value(QStringLiteral("entryPoint"));
    if (!entryPointValue.isObject()) {
        return failure(origin, QStringLiteral("entryPoint must be an object"));
    }
    const QJsonObject entryPoint = entryPointValue.toObject();
    const auto kind = entryPointKindFromString(entryPoint.value(QStringLiteral("kind")).toString());
    if (!kind.has_value()) {
        return failure(origin, QStringLiteral("entryPoint.kind is unknown"));
    }
    manifest.entryPoint.kind = *kind;
    manifest.entryPoint.value = entryPoint.value(QStringLiteral("value")).toString();

    const QJsonValue placementsValue = root.value(QStringLiteral("placements"));
    if (!placementsValue.isObject()) {
        return failure(origin, QStringLiteral("placements must be an object"));
    }
    const QJsonObject placements = placementsValue.toObject();
    QString fieldError;
    if (!readEnumArray(placements.value(QStringLiteral("zones")),
                       QStringLiteral("placements.zones"),
                       placementZoneFromString,
                       &manifest.placementZones,
                       &fieldError)
        || !readEnumArray(placements.value(QStringLiteral("orientations")),
                          QStringLiteral("placements.orientations"),
                          orientationFromString,
                          &manifest.orientations,
                          &fieldError)) {
        return failure(origin, fieldError);
    }

    const QJsonValue sizingValue = root.value(QStringLiteral("sizing"));
    if (!sizingValue.isObject()) {
        return failure(origin, QStringLiteral("sizing must be an object"));
    }
    const QJsonObject sizing = sizingValue.toObject();
    if (!readAxis(sizing.value(QStringLiteral("mainAxis")),
                  QStringLiteral("sizing.mainAxis"),
                  &manifest.sizing.mainAxis,
                  &fieldError)
        || !readAxis(sizing.value(QStringLiteral("crossAxis")),
                     QStringLiteral("sizing.crossAxis"),
                     &manifest.sizing.crossAxis,
                     &fieldError)) {
        return failure(origin, fieldError);
    }

    if (!readEnumArray(root.value(QStringLiteral("capabilities")),
                       QStringLiteral("capabilities"),
                       capabilityFromString,
                       &manifest.capabilities,
                       &fieldError)) {
        return failure(origin, fieldError);
    }

    const QJsonValue settingsSchema = root.value(QStringLiteral("settingsSchema"));
    if (!settingsSchema.isUndefined()) {
        if (!settingsSchema.isObject()) {
            return failure(origin, QStringLiteral("settingsSchema must be an object"));
        }
        manifest.settingsSchema = settingsSchema.toObject();
    }

    const ManifestValidation validation = manifest.validate();
    if (!validation.isValid()) {
        return failure(origin, validation.summary());
    }
    return {.ok = true, .manifest = manifest, .error = {}};
}

QByteArray ManifestLoader::toJson(const AppletManifest &manifest)
{
    QJsonObject root{
        {QStringLiteral("schemaVersion"), manifest.schemaVersion},
        {QStringLiteral("id"), manifest.id},
        {QStringLiteral("name"), manifest.name},
        {QStringLiteral("apiVersion"), manifest.apiVersion.toString()},
        {QStringLiteral("entryPoint"),
         QJsonObject{{QStringLiteral("kind"), toString(manifest.entryPoint.kind)},
                     {QStringLiteral("value"), manifest.entryPoint.value}}},
        {QStringLiteral("placements"),
         QJsonObject{{QStringLiteral("zones"), enumArrayToJson(manifest.placementZones)},
                     {QStringLiteral("orientations"), enumArrayToJson(manifest.orientations)}}},
        {QStringLiteral("sizing"),
         QJsonObject{{QStringLiteral("mainAxis"), axisToJson(manifest.sizing.mainAxis)},
                     {QStringLiteral("crossAxis"), axisToJson(manifest.sizing.crossAxis)}}},
        {QStringLiteral("capabilities"), enumArrayToJson(manifest.capabilities)},
    };
    if (!manifest.description.isEmpty()) {
        root.insert(QStringLiteral("description"), manifest.description);
    }
    if (!manifest.settingsSchema.isEmpty()) {
        root.insert(QStringLiteral("settingsSchema"), manifest.settingsSchema);
    }

    QByteArray serialized = QJsonDocument(root).toJson(QJsonDocument::Indented);
    if (!serialized.endsWith('\n')) {
        serialized.append('\n');
    }
    return serialized;
}

} // namespace QindaQt::Applets
