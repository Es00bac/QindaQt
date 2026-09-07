// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/compositor/shelltaskfacts.h"

#include "qindaqt/compositor/containerappearance.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <utility>

namespace QindaQt::Compositor {
namespace {

void setError(QString *error, QString message)
{
    if (error) {
        *error = std::move(message);
    }
}

bool exactKeys(const QJsonObject &object, std::initializer_list<const char *> keys)
{
    if (object.size() != static_cast<qsizetype>(keys.size())) {
        return false;
    }
    for (const char *key : keys) {
        if (!object.contains(QString::fromLatin1(key))) {
            return false;
        }
    }
    return true;
}

bool canonicalCounter(const QJsonValue &value, quint64 *result, bool positive)
{
    if (!value.isString()) {
        return false;
    }
    bool converted = false;
    const quint64 parsed = value.toString().toULongLong(&converted, 10);
    if (!converted || (positive && parsed == 0)
        || value.toString() != QString::number(parsed)) {
        return false;
    }
    *result = parsed;
    return true;
}

bool readIdObject(const QJsonValue &value, QString *id)
{
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject object = value.toObject();
    if (!exactKeys(object, {"id"})
        || !object.value(QStringLiteral("id")).isString()) {
        return false;
    }
    *id = object.value(QStringLiteral("id")).toString();
    return true;
}

bool readStringList(const QJsonValue &value, QStringList *result)
{
    if (!value.isArray()
        || value.toArray().size() > ShellTaskFactsMaximumWorkspaces) {
        return false;
    }
    for (const QJsonValue &entry : value.toArray()) {
        if (!entry.isString()) {
            return false;
        }
        result->append(entry.toString());
    }
    return true;
}

bool readContainer(const QJsonValue &value, ShellTaskContainer *container)
{
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject object = value.toObject();
    if (!exactKeys(object, {"id", "revision", "authority"})
        || !object.value(QStringLiteral("id")).isString()
        || !canonicalCounter(object.value(QStringLiteral("revision")),
                             &container->revision, true)
        || !object.value(QStringLiteral("authority")).isString()) {
        return false;
    }
    container->id = object.value(QStringLiteral("id")).toString();
    const QString authority = object.value(QStringLiteral("authority")).toString();
    if (authority == QLatin1StringView("control-bridge")) {
        container->authority = ShellTaskContainerAuthority::ControlBridge;
    } else if (authority == QLatin1StringView("hybrid-process")) {
        container->authority = ShellTaskContainerAuthority::HybridProcess;
    } else {
        return false;
    }
    return true;
}

bool readRole(const QJsonValue &value, ShellTaskWindowRole *role)
{
    if (!value.isString()) {
        return false;
    }
    const QString text = value.toString();
    if (text == QLatin1StringView("standalone")) {
        *role = ShellTaskWindowRole::Standalone;
    } else if (text == QLatin1StringView("container-primary")) {
        *role = ShellTaskWindowRole::ContainerPrimary;
    } else if (text == QLatin1StringView("container-member")) {
        *role = ShellTaskWindowRole::ContainerMember;
    } else {
        return false;
    }
    return true;
}

bool readWindowType(const QJsonValue &value, ShellTaskWindowType *type)
{
    if (!value.isString()) {
        return false;
    }
    if (value.toString() == QLatin1StringView("normal")) {
        *type = ShellTaskWindowType::Normal;
    } else if (value.toString() == QLatin1StringView("non-normal")) {
        *type = ShellTaskWindowType::NonNormal;
    } else {
        return false;
    }
    return true;
}

bool readWindowOwner(const QJsonValue &value, ShellTaskWindowOwner *owner)
{
    if (!value.isString()) {
        return false;
    }
    if (value.toString() == QLatin1StringView("application")) {
        *owner = ShellTaskWindowOwner::Application;
    } else if (value.toString() == QLatin1StringView("bound-shell")) {
        *owner = ShellTaskWindowOwner::BoundShell;
    } else {
        return false;
    }
    return true;
}

bool readWindow(const QJsonValue &value, ShellTaskWindow *window)
{
    if (!value.isObject()) {
        return false;
    }
    const QJsonObject object = value.toObject();
    if (!exactKeys(object, {"id", "applicationId", "applicationName", "title",
                            "colorHex", "role", "windowType", "ownerRole", "active",
                            "minimized", "maximized", "fullscreen", "demandsAttention",
                            "outputId", "workspaceIds", "onAllWorkspaces",
                            "containerId"})) {
        return false;
    }
    const auto string = [&object](const char *key, QString *destination) {
        const QJsonValue entry = object.value(QString::fromLatin1(key));
        if (!entry.isString()) {
            return false;
        }
        *destination = entry.toString();
        return true;
    };
    const auto boolean = [&object](const char *key, bool *destination) {
        const QJsonValue entry = object.value(QString::fromLatin1(key));
        if (!entry.isBool()) {
            return false;
        }
        *destination = entry.toBool();
        return true;
    };
    return string("id", &window->windowId)
        && string("applicationId", &window->applicationId)
        && string("applicationName", &window->applicationName)
        && string("title", &window->title)
        && string("colorHex", &window->colorHex)
        && (window->colorHex.isEmpty() || isValidContainerColor(window->colorHex))
        && readRole(object.value(QStringLiteral("role")), &window->role)
        && readWindowType(object.value(QStringLiteral("windowType")),
                          &window->type)
        && readWindowOwner(object.value(QStringLiteral("ownerRole")),
                           &window->owner)
        && boolean("active", &window->active)
        && boolean("minimized", &window->minimized)
        && boolean("maximized", &window->maximized)
        && boolean("fullscreen", &window->fullscreen)
        && boolean("demandsAttention", &window->demandsAttention)
        && string("outputId", &window->outputId)
        && readStringList(object.value(QStringLiteral("workspaceIds")),
                          &window->workspaceIds)
        && boolean("onAllWorkspaces", &window->onAllWorkspaces)
        && string("containerId", &window->containerId);
}

template<typename Value, typename Reader>
bool readBoundedArray(const QJsonObject &object, const char *key,
                      qsizetype maximum, QVector<Value> *result, Reader reader)
{
    const QJsonValue value = object.value(QString::fromLatin1(key));
    if (!value.isArray() || value.toArray().size() > maximum) {
        return false;
    }
    result->reserve(value.toArray().size());
    for (const QJsonValue &entry : value.toArray()) {
        Value decoded;
        if (!reader(entry, &decoded)) {
            return false;
        }
        result->append(std::move(decoded));
    }
    return true;
}

std::optional<ShellTaskFactsSnapshot> decodeFailure(
    const QJsonObject &root, ShellTaskFactsStatus status, QString *error)
{
    const QJsonObject failure = root.value(QStringLiteral("failure")).toObject();
    if (!exactKeys(failure, {"code", "message"})
        || failure.value(QStringLiteral("code")).toString().isEmpty()
        || !failure.value(QStringLiteral("message")).isString()) {
        setError(error, QStringLiteral("task facts failure is malformed"));
        return std::nullopt;
    }
    ShellTaskFactsSnapshot result;
    result.status = status;
    result.failureCode = failure.value(QStringLiteral("code")).toString();
    result.message = failure.value(QStringLiteral("message")).toString();
    if (status == ShellTaskFactsStatus::Unauthorized) {
        if (!exactKeys(root, {"status", "schemaVersion", "failure"})) {
            setError(error, QStringLiteral("unauthorized task facts leaked state"));
            return std::nullopt;
        }
        return result;
    }
    if (!exactKeys(root, {"status", "schemaVersion", "epoch", "revision", "failure"})
        || !root.value(QStringLiteral("epoch")).isString()
        || !canonicalCounter(root.value(QStringLiteral("revision")),
                             &result.revision, false)) {
        setError(error, QStringLiteral("unavailable task facts lineage is invalid"));
        return std::nullopt;
    }
    result.epoch = root.value(QStringLiteral("epoch")).toString();
    if (!ShellWindowGeneration{result.epoch, 1}.isValid()) {
        setError(error, QStringLiteral("unavailable task facts epoch is invalid"));
        return std::nullopt;
    }
    return result;
}

} // namespace

std::optional<ShellTaskFactsSnapshot> decodeShellTaskFactsSnapshot(
    const QByteArray &payload, QString *error)
{
    if (payload.isEmpty() || payload.size() > ShellTaskFactsMaximumPayloadBytes) {
        setError(error, QStringLiteral("task facts reply size is invalid"));
        return std::nullopt;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(error, QStringLiteral("task facts reply is not an object"));
        return std::nullopt;
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("schemaVersion")).toInt(-1) != 1
        || !root.value(QStringLiteral("status")).isString()) {
        setError(error, QStringLiteral("task facts schema or status is invalid"));
        return std::nullopt;
    }
    const QString status = root.value(QStringLiteral("status")).toString();
    if (status == QLatin1StringView("unauthorized")) {
        return decodeFailure(root, ShellTaskFactsStatus::Unauthorized, error);
    }
    if (status == QLatin1StringView("unavailable")) {
        return decodeFailure(root, ShellTaskFactsStatus::Unavailable, error);
    }
    if (status != QLatin1StringView("ok")
        || !exactKeys(root, {"status", "schemaVersion", "epoch", "revision",
                             "actionRevision", "outputs", "workspaces",
                             "containers", "windows"})) {
        setError(error, QStringLiteral("task facts success envelope is malformed"));
        return std::nullopt;
    }
    ShellTaskFactsSnapshot result;
    result.status = ShellTaskFactsStatus::Ok;
    if (!root.value(QStringLiteral("epoch")).isString()
        || !canonicalCounter(root.value(QStringLiteral("revision")),
                             &result.revision, true)
        || !canonicalCounter(root.value(QStringLiteral("actionRevision")),
                             &result.facts.actionGeneration.revision, true)) {
        setError(error, QStringLiteral("task facts lineage is invalid"));
        return std::nullopt;
    }
    result.epoch = root.value(QStringLiteral("epoch")).toString();
    result.facts.actionGeneration.epoch = result.epoch;
    if (!readBoundedArray(root, "outputs", ShellTaskFactsMaximumOutputs,
                          &result.facts.outputs,
                          [](const QJsonValue &entry, ShellTaskOutput *output) {
                              return readIdObject(entry, &output->id);
                          })
        || !readBoundedArray(root, "workspaces", ShellTaskFactsMaximumWorkspaces,
                             &result.facts.workspaces,
                             [](const QJsonValue &entry, ShellTaskWorkspace *workspace) {
                                 return readIdObject(entry, &workspace->id);
                             })
        || !readBoundedArray(root, "containers", ShellTaskFactsMaximumContainers,
                             &result.facts.containers, readContainer)
        || !readBoundedArray(root, "windows", ShellTaskFactsMaximumWindows,
                             &result.facts.windows, readWindow)
        || !validateShellTaskFactsCandidate(result.facts, error)) {
        if (error && error->isEmpty()) {
            *error = QStringLiteral("task facts collections are malformed");
        }
        return std::nullopt;
    }
    setError(error, {});
    return result;
}

} // namespace QindaQt::Compositor
