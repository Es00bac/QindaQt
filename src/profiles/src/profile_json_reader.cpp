// SPDX-License-Identifier: LGPL-3.0-or-later
#include "profile_json_reader_p.h"

#include "profile_path_p.h"
#include "qindaqt/profiles/profile_types.h"

#include <QJsonArray>

#include <cmath>
#include <limits>
#include <utility>

namespace QindaQt::Profiles::Internal {
namespace {

ProfileError fieldError(ProfileErrorCode code,
                        const QString &origin,
                        QString path,
                        QString message)
{
    return {.code = code,
            .origin = origin,
            .path = std::move(path),
            .panelId = {},
            .appletId = {},
            .message = std::move(message),
            .byteOffset = -1};
}

bool requiredString(const QJsonObject &object,
                    const QString &field,
                    const QString &path,
                    const QString &origin,
                    QString *value,
                    ProfileError *error)
{
    const auto iterator = object.constFind(field);
    const QString fieldPath = jsonPointerChild(path, field);
    if (iterator == object.constEnd()) {
        *error = fieldError(ProfileErrorCode::MissingRequiredField,
                            origin,
                            fieldPath,
                            QStringLiteral("required field is absent"));
        return false;
    }
    if (!iterator->isString()) {
        *error = fieldError(ProfileErrorCode::InvalidFieldType,
                            origin,
                            fieldPath,
                            QStringLiteral("field must be a string"));
        return false;
    }
    *value = iterator->toString();
    return true;
}

bool optionalString(const QJsonObject &object,
                    const QString &field,
                    const QString &path,
                    const QString &origin,
                    QString *value,
                    ProfileError *error)
{
    const auto iterator = object.constFind(field);
    if (iterator == object.constEnd()) {
        return true;
    }
    const QString fieldPath = jsonPointerChild(path, field);
    if (!iterator->isString()) {
        *error = fieldError(ProfileErrorCode::InvalidFieldType,
                            origin,
                            fieldPath,
                            QStringLiteral("field must be a string when present"));
        return false;
    }
    *value = iterator->toString();
    return true;
}

bool optionalBoolean(const QJsonObject &object,
                     const QString &field,
                     const QString &path,
                     const QString &origin,
                     bool *value,
                     ProfileError *error)
{
    const auto iterator = object.constFind(field);
    if (iterator == object.constEnd()) {
        return true;
    }
    const QString fieldPath = jsonPointerChild(path, field);
    if (!iterator->isBool()) {
        *error = fieldError(ProfileErrorCode::InvalidFieldType,
                            origin,
                            fieldPath,
                            QStringLiteral("field must be a boolean when present"));
        return false;
    }
    *value = iterator->toBool();
    return true;
}

bool exactInteger(const QJsonValue &value, qint64 *integer)
{
    if (!value.isDouble()) {
        return false;
    }
    constexpr qint64 low = std::numeric_limits<qint64>::lowest();
    constexpr qint64 high = std::numeric_limits<qint64>::max();
    const qint64 withLowDefault = value.toInteger(low);
    const qint64 withHighDefault = value.toInteger(high);
    if (withLowDefault != withHighDefault) {
        return false;
    }
    *integer = withLowDefault;
    return true;
}

bool requiredInteger(const QJsonObject &object,
                     const QString &field,
                     const QString &path,
                     const QString &origin,
                     int *value,
                     ProfileError *error)
{
    const auto iterator = object.constFind(field);
    const QString fieldPath = jsonPointerChild(path, field);
    if (iterator == object.constEnd()) {
        *error = fieldError(ProfileErrorCode::MissingRequiredField,
                            origin,
                            fieldPath,
                            QStringLiteral("required field is absent"));
        return false;
    }
    qint64 integer = 0;
    if (!exactInteger(*iterator, &integer)) {
        const double number = iterator->toDouble();
        const bool integralButUnrepresentable =
            iterator->isDouble() && std::isfinite(number) && std::trunc(number) == number;
        *error = fieldError(integralButUnrepresentable ? ProfileErrorCode::OutOfRange
                                                       : ProfileErrorCode::InvalidFieldType,
                            origin,
                            fieldPath,
                            integralButUnrepresentable
                                ? QStringLiteral("integer is outside the supported signed range")
                                : QStringLiteral("field must be an exact integer"));
        return false;
    }
    if (integer < std::numeric_limits<int>::lowest()
        || integer > std::numeric_limits<int>::max()) {
        *error = fieldError(ProfileErrorCode::OutOfRange,
                            origin,
                            fieldPath,
                            QStringLiteral("integer cannot be represented by the profile model"));
        return false;
    }
    *value = static_cast<int>(integer);
    return true;
}

bool optionalInteger(const QJsonObject &object,
                     const QString &field,
                     const QString &path,
                     const QString &origin,
                     int *value,
                     ProfileError *error)
{
    if (!object.contains(field)) {
        return true;
    }
    return requiredInteger(object, field, path, origin, value, error);
}

bool optionalNumber(const QJsonObject &object,
                    const QString &field,
                    const QString &path,
                    const QString &origin,
                    double *value,
                    ProfileError *error)
{
    const auto iterator = object.constFind(field);
    if (iterator == object.constEnd()) {
        return true;
    }
    const QString fieldPath = jsonPointerChild(path, field);
    if (!iterator->isDouble()) {
        *error = fieldError(ProfileErrorCode::InvalidFieldType,
                            origin,
                            fieldPath,
                            QStringLiteral("field must be a number when present"));
        return false;
    }
    *value = iterator->toDouble();
    return true;
}

template<typename Enum, typename Parser, typename Formatter>
bool optionalEnum(const QJsonObject &object,
                  const QString &field,
                  const QString &path,
                  const QString &origin,
                  Enum *value,
                  Parser parser,
                  Formatter formatter,
                  ProfileError *error)
{
    const auto iterator = object.constFind(field);
    if (iterator == object.constEnd()) {
        return true;
    }
    const QString fieldPath = jsonPointerChild(path, field);
    if (!iterator->isString()) {
        *error = fieldError(ProfileErrorCode::InvalidFieldType,
                            origin,
                            fieldPath,
                            QStringLiteral("field must be a string when present"));
        return false;
    }

    const QString text = iterator->toString();
    Enum parsed{};
    if (!parser(text, &parsed) || formatter(parsed) != text) {
        *error = fieldError(ProfileErrorCode::InvalidEnumValue,
                            origin,
                            fieldPath,
                            QStringLiteral("field contains an unknown or non-canonical enum value"));
        return false;
    }
    *value = parsed;
    return true;
}

bool readApplet(const QJsonObject &object,
                const QString &path,
                const QString &origin,
                const QString &panelId,
                AppletSpec *applet,
                ProfileError *error)
{
    if (!requiredString(object, QStringLiteral("id"), path, origin, &applet->id, error)
        || !requiredString(object,
                           QStringLiteral("plugin"),
                           path,
                           origin,
                           &applet->plugin,
                           error)) {
        error->panelId = panelId;
        error->appletId = applet->id;
        return false;
    }

    const auto settings = object.constFind(QStringLiteral("settings"));
    if (settings != object.constEnd()) {
        if (!settings->isObject()) {
            *error = fieldError(ProfileErrorCode::InvalidFieldType,
                                origin,
                                jsonPointerChild(path, QStringLiteral("settings")),
                                QStringLiteral("field must be an object when present"));
            error->panelId = panelId;
            error->appletId = applet->id;
            return false;
        }
        applet->settings = settings->toObject().toVariantMap();
    }
    return true;
}

bool readPanel(const QJsonObject &object,
               const QString &path,
               const QString &origin,
               PanelSpec *panel,
               ProfileError *error)
{
    if (!requiredString(object, QStringLiteral("id"), path, origin, &panel->id, error)
        || !optionalString(object,
                           QStringLiteral("output"),
                           path,
                           origin,
                           &panel->output,
                           error)
        || !optionalEnum(object,
                         QStringLiteral("edge"),
                         path,
                         origin,
                         &panel->edge,
                         parseEdge,
                         [](Edge value) { return toString(value); },
                         error)
        || !optionalEnum(object,
                         QStringLiteral("layer"),
                         path,
                         origin,
                         &panel->layer,
                         parseLayer,
                         [](Layer value) { return toString(value); },
                         error)
        || !optionalEnum(object,
                         QStringLiteral("hideMode"),
                         path,
                         origin,
                         &panel->hideMode,
                         parseHideMode,
                         [](HideMode value) { return toString(value); },
                         error)
        || !optionalEnum(object,
                         QStringLiteral("alignment"),
                         path,
                         origin,
                         &panel->alignment,
                         parseAlignment,
                         [](Alignment value) { return toString(value); },
                         error)
        || !optionalInteger(object,
                            QStringLiteral("rows"),
                            path,
                            origin,
                            &panel->rows,
                            error)
        || !optionalInteger(object,
                            QStringLiteral("thickness"),
                            path,
                            origin,
                            &panel->thickness,
                            error)
        || !optionalNumber(object,
                           QStringLiteral("length"),
                           path,
                           origin,
                           &panel->length,
                           error)) {
        error->panelId = panel->id;
        return false;
    }

    const auto applets = object.constFind(QStringLiteral("applets"));
    if (applets == object.constEnd()) {
        return true;
    }
    if (!applets->isArray()) {
        *error = fieldError(ProfileErrorCode::InvalidFieldType,
                            origin,
                            jsonPointerChild(path, QStringLiteral("applets")),
                            QStringLiteral("field must be an array when present"));
        error->panelId = panel->id;
        return false;
    }

    const QJsonArray array = applets->toArray();
    panel->applets.reserve(array.size());
    for (qsizetype index = 0; index < array.size(); ++index) {
        const QString appletPath =
            jsonPointerIndex(jsonPointerChild(path, QStringLiteral("applets")), index);
        const QJsonValue value = array.at(index);
        if (!value.isObject()) {
            *error = fieldError(ProfileErrorCode::InvalidFieldType,
                                origin,
                                appletPath,
                                QStringLiteral("applet entry must be an object"));
            error->panelId = panel->id;
            return false;
        }
        AppletSpec applet;
        if (!readApplet(value.toObject(), appletPath, origin, panel->id, &applet, error)) {
            return false;
        }
        panel->applets.append(std::move(applet));
    }
    return true;
}

bool readWorkflow(const QJsonObject &root,
                  const QString &origin,
                  WorkflowSpec *workflow,
                  ProfileError *error)
{
    const auto value = root.constFind(QStringLiteral("workflow"));
    if (value == root.constEnd()) {
        return true;
    }
    const QString path = QStringLiteral("/workflow");
    if (!value->isObject()) {
        *error = fieldError(ProfileErrorCode::InvalidFieldType,
                            origin,
                            path,
                            QStringLiteral("field must be an object when present"));
        return false;
    }
    const QJsonObject object = value->toObject();
    return optionalString(object,
                          QStringLiteral("overview"),
                          path,
                          origin,
                          &workflow->overview,
                          error)
        && optionalString(object,
                          QStringLiteral("workspacePolicy"),
                          path,
                          origin,
                          &workflow->workspacePolicy,
                          error)
        && optionalString(object,
                          QStringLiteral("launcher"),
                          path,
                          origin,
                          &workflow->launcher,
                          error)
        && optionalString(object,
                          QStringLiteral("menu"),
                          path,
                          origin,
                          &workflow->menu,
                          error)
        && optionalString(object,
                          QStringLiteral("taskList"),
                          path,
                          origin,
                          &workflow->taskList,
                          error)
        && optionalBoolean(object,
                           QStringLiteral("globalMenu"),
                           path,
                           origin,
                           &workflow->globalMenu,
                           error);
}

} // namespace

ProfileJsonReadResult readProfileObject(const QJsonObject &root, const QString &origin)
{
    ProfileJsonReadResult result;
    if (!requiredInteger(root,
                         QStringLiteral("schemaVersion"),
                         {},
                         origin,
                         &result.profile.schemaVersion,
                         &result.error)
        || !requiredString(root,
                           QStringLiteral("id"),
                           {},
                           origin,
                           &result.profile.id,
                           &result.error)
        || !requiredString(root,
                           QStringLiteral("name"),
                           {},
                           origin,
                           &result.profile.name,
                           &result.error)
        || !optionalString(root,
                           QStringLiteral("description"),
                           {},
                           origin,
                           &result.profile.description,
                           &result.error)
        || !optionalString(root,
                           QStringLiteral("defaultTheme"),
                           {},
                           origin,
                           &result.profile.defaultTheme,
                           &result.error)
        || !readWorkflow(root, origin, &result.profile.workflow, &result.error)) {
        return result;
    }

    const auto panels = root.constFind(QStringLiteral("panels"));
    if (panels == root.constEnd()) {
        result.error = fieldError(ProfileErrorCode::MissingRequiredField,
                                  origin,
                                  QStringLiteral("/panels"),
                                  QStringLiteral("required field is absent"));
        return result;
    }
    if (!panels->isArray()) {
        result.error = fieldError(ProfileErrorCode::InvalidFieldType,
                                  origin,
                                  QStringLiteral("/panels"),
                                  QStringLiteral("field must be an array"));
        return result;
    }

    const QJsonArray array = panels->toArray();
    result.profile.panels.reserve(array.size());
    for (qsizetype index = 0; index < array.size(); ++index) {
        const QString panelPath = jsonPointerIndex(QStringLiteral("/panels"), index);
        const QJsonValue value = array.at(index);
        if (!value.isObject()) {
            result.error = fieldError(ProfileErrorCode::InvalidFieldType,
                                      origin,
                                      panelPath,
                                      QStringLiteral("panel entry must be an object"));
            return result;
        }
        PanelSpec panel;
        if (!readPanel(value.toObject(), panelPath, origin, &panel, &result.error)) {
            return result;
        }
        result.profile.panels.append(std::move(panel));
    }
    return result;
}

} // namespace QindaQt::Profiles::Internal
