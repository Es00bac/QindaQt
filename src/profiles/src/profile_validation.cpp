// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/profiles/profile_validation.h"

#include "profile_path_p.h"
#include "qindaqt/profiles/profile_types.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaType>
#include <QSet>
#include <QVariant>

#include <cmath>
#include <limits>
#include <utility>

namespace QindaQt::Profiles {
namespace {

using Internal::jsonPointerChild;
using Internal::jsonPointerIndex;

constexpr int maximumSettingsNesting = 64;

ProfileValidationResult failure(ProfileErrorCode code,
                                QString path,
                                QString message,
                                QString panelId = {},
                                QString appletId = {})
{
    return {{.code = code,
             .origin = {},
             .path = std::move(path),
             .panelId = std::move(panelId),
             .appletId = std::move(appletId),
             .message = std::move(message),
             .byteOffset = -1}};
}

bool isCanonicalIdentifier(const QString &value)
{
    return value.isValidUtf16() && !value.isEmpty() && value == value.trimmed();
}

bool isNonBlank(const QString &value)
{
    return value.isValidUtf16() && !value.trimmed().isEmpty();
}

ProfileValidationResult validatePanelLayoutValue(const PanelSpec &panel,
                                                 const QString &path)
{
    if (!isCanonicalIdentifier(panel.id)) {
        return failure(ProfileErrorCode::InvalidIdentifier,
                       jsonPointerChild(path, QStringLiteral("id")),
                       QStringLiteral("panel id must be well-formed UTF-16, non-empty, and "
                                      "contain no surrounding whitespace"),
                       panel.id);
    }
    if (!isCanonicalIdentifier(panel.output)) {
        return failure(ProfileErrorCode::InvalidIdentifier,
                       jsonPointerChild(path, QStringLiteral("output")),
                       QStringLiteral("output selector must be well-formed UTF-16, non-empty, "
                                      "and contain no surrounding whitespace"),
                       panel.id);
    }
    if (toString(panel.edge).isEmpty()) {
        return failure(ProfileErrorCode::InvalidEnumValue,
                       jsonPointerChild(path, QStringLiteral("edge")),
                       QStringLiteral("edge is not a recognized profile-v1 value"),
                       panel.id);
    }
    if (toString(panel.layer).isEmpty()) {
        return failure(ProfileErrorCode::InvalidEnumValue,
                       jsonPointerChild(path, QStringLiteral("layer")),
                       QStringLiteral("layer is not a recognized profile-v1 value"),
                       panel.id);
    }
    if (toString(panel.hideMode).isEmpty()) {
        return failure(ProfileErrorCode::InvalidEnumValue,
                       jsonPointerChild(path, QStringLiteral("hideMode")),
                       QStringLiteral("hide mode is not a recognized profile-v1 value"),
                       panel.id);
    }
    if (toString(panel.alignment).isEmpty()) {
        return failure(ProfileErrorCode::InvalidEnumValue,
                       jsonPointerChild(path, QStringLiteral("alignment")),
                       QStringLiteral("alignment is not a recognized profile-v1 value"),
                       panel.id);
    }
    if (panel.rows < 1 || panel.rows > 4) {
        return failure(ProfileErrorCode::OutOfRange,
                       jsonPointerChild(path, QStringLiteral("rows")),
                       QStringLiteral("rows must be between 1 and 4 inclusive"),
                       panel.id);
    }
    if (panel.thickness < 20 || panel.thickness > 192) {
        return failure(ProfileErrorCode::OutOfRange,
                       jsonPointerChild(path, QStringLiteral("thickness")),
                       QStringLiteral("thickness must be between 20 and 192 inclusive"),
                       panel.id);
    }
    if (!std::isfinite(panel.length) || panel.length < 0.1 || panel.length > 1.0) {
        return failure(ProfileErrorCode::OutOfRange,
                       jsonPointerChild(path, QStringLiteral("length")),
                       QStringLiteral("length must be finite and between 0.1 and 1.0 inclusive"),
                       panel.id);
    }
    return {};
}

ProfileValidationResult nonJsonValue(const QString &path,
                                     const QString &panelId,
                                     const QString &appletId,
                                     const QString &message)
{
    return failure(ProfileErrorCode::NonJsonSettingsValue,
                   path,
                   message,
                   panelId,
                   appletId);
}

ProfileValidationResult illFormedObjectKey(const QString &path,
                                           const QString &panelId,
                                           const QString &appletId)
{
    // AGENT-GUARD: an ill-formed key cannot be represented by an RFC 6901
    // token. Report its valid parent path instead of publishing an equally
    // ill-formed diagnostic string.
    return nonJsonValue(path,
                        panelId,
                        appletId,
                        QStringLiteral("settings object key contains ill-formed UTF-16"));
}

ProfileValidationResult validateJsonVariant(const QVariant &value,
                                            const QString &path,
                                            int depth,
                                            const QString &panelId,
                                            const QString &appletId);

ProfileValidationResult validateJsonValue(const QJsonValue &value,
                                          const QString &path,
                                          int depth,
                                          const QString &panelId,
                                          const QString &appletId);

ProfileValidationResult validateVariantList(const QVariantList &values,
                                            const QString &path,
                                            int depth,
                                            const QString &panelId,
                                            const QString &appletId)
{
    for (qsizetype index = 0; index < values.size(); ++index) {
        const auto result = validateJsonVariant(values.at(index),
                                                jsonPointerIndex(path, index),
                                                depth + 1,
                                                panelId,
                                                appletId);
        if (!result.succeeded()) {
            return result;
        }
    }
    return {};
}

template<typename Associative>
ProfileValidationResult validateVariantObject(const Associative &values,
                                              const QString &path,
                                              int depth,
                                              const QString &panelId,
                                              const QString &appletId)
{
    QStringList keys = values.keys();
    keys.sort();
    for (const QString &key : std::as_const(keys)) {
        if (!key.isValidUtf16()) {
            return illFormedObjectKey(path, panelId, appletId);
        }
        const auto result = validateJsonVariant(values.value(key),
                                                jsonPointerChild(path, key),
                                                depth + 1,
                                                panelId,
                                                appletId);
        if (!result.succeeded()) {
            return result;
        }
    }
    return {};
}

ProfileValidationResult validateJsonArray(const QJsonArray &values,
                                          const QString &path,
                                          int depth,
                                          const QString &panelId,
                                          const QString &appletId)
{
    for (qsizetype index = 0; index < values.size(); ++index) {
        const auto result = validateJsonValue(values.at(index),
                                              jsonPointerIndex(path, index),
                                              depth + 1,
                                              panelId,
                                              appletId);
        if (!result.succeeded()) {
            return result;
        }
    }
    return {};
}

ProfileValidationResult validateJsonObject(const QJsonObject &values,
                                           const QString &path,
                                           int depth,
                                           const QString &panelId,
                                           const QString &appletId)
{
    const QStringList keys = values.keys();
    for (const QString &key : keys) {
        if (!key.isValidUtf16()) {
            return illFormedObjectKey(path, panelId, appletId);
        }
        const auto result = validateJsonValue(values.value(key),
                                              jsonPointerChild(path, key),
                                              depth + 1,
                                              panelId,
                                              appletId);
        if (!result.succeeded()) {
            return result;
        }
    }
    return {};
}

ProfileValidationResult validateJsonValue(const QJsonValue &value,
                                          const QString &path,
                                          int depth,
                                          const QString &panelId,
                                          const QString &appletId)
{
    if (depth > maximumSettingsNesting) {
        return nonJsonValue(path,
                            panelId,
                            appletId,
                            QStringLiteral("settings nesting exceeds the limit of %1")
                                .arg(maximumSettingsNesting));
    }
    if (value.isUndefined()) {
        return nonJsonValue(path,
                            panelId,
                            appletId,
                            QStringLiteral("undefined QJsonValue is not persistable JSON"));
    }
    if (value.isDouble() && !std::isfinite(value.toDouble())) {
        return nonJsonValue(path,
                            panelId,
                            appletId,
                            QStringLiteral("JSON numbers must be finite"));
    }
    if (value.isString() && !value.toString().isValidUtf16()) {
        return nonJsonValue(path,
                            panelId,
                            appletId,
                            QStringLiteral("JSON strings must contain well-formed UTF-16"));
    }
    if (value.isArray()) {
        return validateJsonArray(value.toArray(), path, depth, panelId, appletId);
    }
    if (value.isObject()) {
        return validateJsonObject(value.toObject(), path, depth, panelId, appletId);
    }
    return {};
}

ProfileValidationResult validateJsonVariant(const QVariant &value,
                                            const QString &path,
                                            int depth,
                                            const QString &panelId,
                                            const QString &appletId)
{
    // AGENT-GUARD: editor candidates are untrusted typed graphs. Bound them
    // before recursive serialization so validation cannot exhaust the stack.
    if (depth > maximumSettingsNesting) {
        return nonJsonValue(path,
                            panelId,
                            appletId,
                            QStringLiteral("settings nesting exceeds the limit of %1")
                                .arg(maximumSettingsNesting));
    }
    if (!value.isValid()) {
        return nonJsonValue(path,
                            panelId,
                            appletId,
                            QStringLiteral("invalid QVariant is not a JSON null value"));
    }

    switch (value.metaType().id()) {
    case QMetaType::Nullptr:
    case QMetaType::Bool:
    case QMetaType::Int:
    case QMetaType::UInt:
    case QMetaType::LongLong:
        return {};
    case QMetaType::ULongLong:
        if (value.toULongLong()
            <= static_cast<qulonglong>(std::numeric_limits<qint64>::max())) {
            return {};
        }
        return nonJsonValue(path,
                            panelId,
                            appletId,
                            QStringLiteral("unsigned integer exceeds JSON's exact signed range"));
    case QMetaType::Float:
    case QMetaType::Double:
        if (std::isfinite(value.toDouble())) {
            return {};
        }
        return nonJsonValue(path,
                            panelId,
                            appletId,
                            QStringLiteral("JSON numbers must be finite"));
    case QMetaType::QString:
        if (value.toString().isValidUtf16()) {
            return {};
        }
        return nonJsonValue(path,
                            panelId,
                            appletId,
                            QStringLiteral("JSON strings must contain well-formed UTF-16"));
    case QMetaType::QStringList: {
        const QStringList strings = value.toStringList();
        for (qsizetype index = 0; index < strings.size(); ++index) {
            if (!strings.at(index).isValidUtf16()) {
                return nonJsonValue(jsonPointerIndex(path, index),
                                    panelId,
                                    appletId,
                                    QStringLiteral(
                                        "JSON strings must contain well-formed UTF-16"));
            }
        }
        return {};
    }
    case QMetaType::QVariantList:
        return validateVariantList(value.toList(), path, depth, panelId, appletId);
    case QMetaType::QVariantMap:
        return validateVariantObject(value.toMap(), path, depth, panelId, appletId);
    case QMetaType::QVariantHash:
        return validateVariantObject(value.toHash(), path, depth, panelId, appletId);
    case QMetaType::QJsonValue:
        return validateJsonValue(value.value<QJsonValue>(), path, depth, panelId, appletId);
    case QMetaType::QJsonArray:
        return validateJsonArray(value.value<QJsonArray>(),
                                 path,
                                 depth,
                                 panelId,
                                 appletId);
    case QMetaType::QJsonObject:
        return validateJsonObject(value.value<QJsonObject>(),
                                  path,
                                  depth,
                                  panelId,
                                  appletId);
    default:
        return nonJsonValue(
            path,
            panelId,
            appletId,
            QStringLiteral("QVariant type '%1' requires lossy JSON coercion")
                .arg(QString::fromLatin1(value.metaType().name())));
    }
}

ProfileValidationResult validateApplet(const AppletSpec &applet,
                                       const QString &panelId,
                                       const QString &path)
{
    if (!isCanonicalIdentifier(applet.id)) {
        return failure(ProfileErrorCode::InvalidIdentifier,
                       jsonPointerChild(path, QStringLiteral("id")),
                       QStringLiteral("applet id must be well-formed UTF-16, non-empty, and "
                                      "contain no surrounding whitespace"),
                       panelId,
                       applet.id);
    }
    if (!isCanonicalIdentifier(applet.plugin)) {
        return failure(ProfileErrorCode::InvalidIdentifier,
                       jsonPointerChild(path, QStringLiteral("plugin")),
                       QStringLiteral("plugin id must be well-formed UTF-16, non-empty, and "
                                      "contain no surrounding whitespace"),
                       panelId,
                       applet.id);
    }
    return validateVariantObject(applet.settings,
                                 jsonPointerChild(path, QStringLiteral("settings")),
                                 0,
                                 panelId,
                                 applet.id);
}

} // namespace

ProfileValidationResult ProfileValidator::validate(const LayoutProfile &profile)
{
    if (profile.schemaVersion != LayoutProfileSchemaVersion) {
        return failure(ProfileErrorCode::UnsupportedSchemaVersion,
                       QStringLiteral("/schemaVersion"),
                       QStringLiteral("schemaVersion must be exactly %1")
                           .arg(LayoutProfileSchemaVersion));
    }
    if (!isCanonicalIdentifier(profile.id)) {
        return failure(ProfileErrorCode::InvalidIdentifier,
                       QStringLiteral("/id"),
                       QStringLiteral("profile id must be well-formed UTF-16, non-empty, and "
                                      "contain no surrounding whitespace"));
    }
    if (!isNonBlank(profile.name)) {
        return failure(ProfileErrorCode::InvalidValue,
                       QStringLiteral("/name"),
                       QStringLiteral("profile name must be well-formed UTF-16 and not blank"));
    }
    if (!profile.description.isValidUtf16()) {
        return failure(ProfileErrorCode::InvalidValue,
                       QStringLiteral("/description"),
                       QStringLiteral("profile description must be well-formed UTF-16"));
    }
    if (!isCanonicalIdentifier(profile.defaultTheme)) {
        return failure(ProfileErrorCode::InvalidIdentifier,
                       QStringLiteral("/defaultTheme"),
                       QStringLiteral("default theme id must be well-formed UTF-16, non-empty, "
                                      "and contain no surrounding whitespace"));
    }

    const struct {
        const QString *value;
        const char *field;
    } workflowValues[] = {{&profile.workflow.overview, "overview"},
                          {&profile.workflow.workspacePolicy, "workspacePolicy"},
                          {&profile.workflow.launcher, "launcher"},
                          {&profile.workflow.menu, "menu"},
                          {&profile.workflow.taskList, "taskList"}};
    for (const auto &entry : workflowValues) {
        if (!isNonBlank(*entry.value)) {
            return failure(ProfileErrorCode::InvalidValue,
                           QStringLiteral("/workflow/") + QLatin1String(entry.field),
                           QStringLiteral(
                               "workflow hint must be well-formed UTF-16 and not blank"));
        }
    }
    if (profile.panels.isEmpty()) {
        return failure(ProfileErrorCode::EmptyPanelSet,
                       QStringLiteral("/panels"),
                       QStringLiteral("profile must contain at least one panel"));
    }

    QSet<QString> panelIds;
    QSet<QString> appletIds;
    for (qsizetype panelIndex = 0; panelIndex < profile.panels.size(); ++panelIndex) {
        const PanelSpec &panel = profile.panels.at(panelIndex);
        const QString panelPath = jsonPointerIndex(QStringLiteral("/panels"), panelIndex);
        const auto layoutResult = validatePanelLayoutValue(panel, panelPath);
        if (!layoutResult.succeeded()) {
            return layoutResult;
        }
        if (panelIds.contains(panel.id)) {
            return failure(ProfileErrorCode::DuplicatePanelId,
                           jsonPointerChild(panelPath, QStringLiteral("id")),
                           QStringLiteral("panel id '%1' is already used by this profile")
                               .arg(panel.id),
                           panel.id);
        }
        panelIds.insert(panel.id);

        for (qsizetype appletIndex = 0; appletIndex < panel.applets.size(); ++appletIndex) {
            const AppletSpec &applet = panel.applets.at(appletIndex);
            const QString appletPath = jsonPointerIndex(
                jsonPointerChild(panelPath, QStringLiteral("applets")), appletIndex);
            const auto appletResult = validateApplet(applet, panel.id, appletPath);
            if (!appletResult.succeeded()) {
                return appletResult;
            }
            if (appletIds.contains(applet.id)) {
                return failure(ProfileErrorCode::DuplicateAppletId,
                               jsonPointerChild(appletPath, QStringLiteral("id")),
                               QStringLiteral("applet id '%1' is already used by this profile")
                                   .arg(applet.id),
                               panel.id,
                               applet.id);
            }
            appletIds.insert(applet.id);
        }
    }
    return {};
}

ProfileValidationResult ProfileValidator::validatePanelLayout(const PanelSpec &panel)
{
    return validatePanelLayoutValue(panel, QStringLiteral("/panel"));
}

} // namespace QindaQt::Profiles
