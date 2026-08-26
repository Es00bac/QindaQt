// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/applets/applet_manifest.h"

#include <QDir>
#include <QJsonValue>
#include <QRegularExpression>
#include <QSet>

#include <utility>

namespace QindaQt::Applets {
namespace {

bool isValidIdentifier(const QString &value)
{
    static const QRegularExpression pattern(
        QStringLiteral(R"(^[a-z][a-z0-9]*(?:[.-][a-z0-9]+)*$)"));
    return pattern.match(value).hasMatch();
}

bool containsParentTraversal(const QString &path)
{
    return path.contains(QLatin1Char('\\'))
        || path.split(QLatin1Char('/'), Qt::SkipEmptyParts).contains(QStringLiteral(".."));
}

void validateAxis(const AxisSizing &axis, const QString &field, ManifestValidation *validation)
{
    if (axis.minimum < 0 || axis.preferred < axis.minimum) {
        validation->addError(
            QStringLiteral("%1 must satisfy 0 <= minimum <= preferred").arg(field));
    }
    if (axis.maximum.has_value() && *axis.maximum < axis.preferred) {
        validation->addError(
            QStringLiteral("%1.maximum must be greater than or equal to preferred").arg(field));
    }
    if (axis.minimum > 16'384 || axis.preferred > 16'384
        || (axis.maximum.has_value() && *axis.maximum > 16'384)) {
        validation->addError(QStringLiteral("%1 exceeds the 16384-unit safety bound").arg(field));
    }
}

template<typename Value>
void validateUnique(const QVector<Value> &values,
                    const QString &field,
                    ManifestValidation *validation)
{
    QSet<Value> seen;
    for (const Value value : values) {
        if (seen.contains(value)) {
            validation->addError(QStringLiteral("%1 contains duplicate values").arg(field));
            return;
        }
        seen.insert(value);
    }
}

} // namespace

bool ManifestValidation::isValid() const
{
    return m_errors.isEmpty();
}

const QStringList &ManifestValidation::errors() const
{
    return m_errors;
}

QString ManifestValidation::summary() const
{
    return m_errors.join(QStringLiteral("; "));
}

void ManifestValidation::addError(QString error)
{
    m_errors.append(std::move(error));
}

ManifestValidation AppletManifest::validate() const
{
    ManifestValidation validation;
    if (schemaVersion != ManifestSchemaVersion) {
        validation.addError(QStringLiteral("schemaVersion must be 1"));
    }
    if (!isValidIdentifier(id)) {
        validation.addError(QStringLiteral("id is not a stable lower-case identifier"));
    }
    if (name.trimmed().isEmpty()) {
        validation.addError(QStringLiteral("name must not be empty"));
    }
    if (!apiVersion.isValid()) {
        validation.addError(QStringLiteral("apiVersion must contain a positive major version"));
    }
    if (entryPoint.value.trimmed().isEmpty()) {
        validation.addError(QStringLiteral("entryPoint.value must not be empty"));
    }

    // AGENT-GUARD: A Builtin entry point is only a symbolic lookup key. The
    // host's trusted registry, never this manifest, decides in-process access.
    if (entryPoint.kind == EntryPointKind::Builtin && !isValidIdentifier(entryPoint.value)) {
        validation.addError(
            QStringLiteral("builtin entryPoint.value must be a stable symbolic identifier"));
    } else if (entryPoint.kind != EntryPointKind::Builtin
               && (QDir::isAbsolutePath(entryPoint.value)
                   || containsParentTraversal(entryPoint.value))) {
        validation.addError(
            QStringLiteral("entryPoint.value must be a package-relative path without '..'"));
    }

    if (placementZones.isEmpty()) {
        validation.addError(QStringLiteral("placements.zones must not be empty"));
    }
    if (orientations.isEmpty()) {
        validation.addError(QStringLiteral("placements.orientations must not be empty"));
    }
    validateUnique(placementZones, QStringLiteral("placements.zones"), &validation);
    validateUnique(orientations, QStringLiteral("placements.orientations"), &validation);
    validateUnique(capabilities, QStringLiteral("capabilities"), &validation);
    validateAxis(sizing.mainAxis, QStringLiteral("sizing.mainAxis"), &validation);
    validateAxis(sizing.crossAxis, QStringLiteral("sizing.crossAxis"), &validation);

    if (!settingsSchema.isEmpty()) {
        if (settingsSchema.value(QStringLiteral("type")).toString() != QLatin1String("object")) {
            validation.addError(QStringLiteral("settingsSchema.type must be 'object'"));
        }
        const QJsonValue properties = settingsSchema.value(QStringLiteral("properties"));
        if (!properties.isUndefined() && !properties.isObject()) {
            validation.addError(QStringLiteral("settingsSchema.properties must be an object"));
        }
        const QJsonValue additional = settingsSchema.value(QStringLiteral("additionalProperties"));
        if (!additional.isUndefined() && !additional.isBool()) {
            validation.addError(
                QStringLiteral("settingsSchema.additionalProperties must be a boolean"));
        }
    }
    return validation;
}

bool AppletManifest::supportsHost(const ApiVersion &hostVersion) const
{
    return apiVersion.isSupportedBy(hostVersion);
}

} // namespace QindaQt::Applets
