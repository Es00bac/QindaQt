// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell/task_list/producer/task_list_wire.h"

#include "qindaqt/shell/task_list/task_list_types.h"
#include "qindaqt/shell_visibility_protocol/wire_limits.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>

#include <utility>

namespace QindaQt::ShellTaskList::Producer {
namespace {

using ShellVisibilityProtocol::WireLimits;

// Containers never outnumber their members, and a published container holds
// at least two windows (docs/wiki/architecture/window-containers.md).
constexpr qsizetype kMaxWireContainers = kMaxWindowFacts / 2;

QString errorMessage(const char *context, const QString &detail) {
  return QString::fromLatin1(context) + QStringLiteral(": ") + detail;
}

bool isCleanText(const QString &value) {
  if (value.contains(QChar::Null)) {
    return false;
  }
  for (qsizetype index = 0; index < value.size(); ++index) {
    const QChar character = value.at(index);
    if (character.category() == QChar::Other_Control ||
        character.category() == QChar::Other_Format) {
      return false;
    }
    if (character.isHighSurrogate()) {
      if (index + 1 >= value.size() ||
          !value.at(index + 1).isLowSurrogate()) {
        return false;
      }
      ++index;
    } else if (character.isLowSurrogate()) {
      return false;
    }
  }
  return true;
}

bool readIdentifier(const QJsonObject &object, QLatin1StringView key,
                    bool allowEmpty, QString *destination) {
  const QJsonValue value = object.value(key);
  if (!value.isString()) {
    return false;
  }
  const QString text = value.toString();
  if ((!allowEmpty && text.isEmpty()) || text.size() > kMaxIdLength ||
      !isCleanText(text)) {
    return false;
  }
  *destination = text;
  return true;
}

bool readBoundedText(const QJsonObject &object, QLatin1StringView key,
                     QString *destination) {
  const QJsonValue value = object.value(key);
  if (!value.isString()) {
    return false;
  }
  const QString text = value.toString();
  if (text.size() > kMaxIdLength || !isCleanText(text)) {
    return false;
  }
  *destination = text;
  return true;
}

bool readBoolean(const QJsonObject &object, QLatin1StringView key,
                 bool *destination) {
  const QJsonValue value = object.value(key);
  if (!value.isBool()) {
    return false;
  }
  *destination = value.toBool();
  return true;
}

// Revisions and generations are unsigned decimal JSON strings, never numbers
// (compositor-control-v1.md); canonical form rejects leading zeros and junk.
bool readCanonicalRevision(const QJsonValue &value, quint64 *destination) {
  if (!value.isString()) {
    return false;
  }
  const QString text = value.toString();
  bool converted = false;
  const quint64 parsed = text.toULongLong(&converted, 10);
  if (!converted || parsed == 0 || QString::number(parsed) != text) {
    return false;
  }
  *destination = parsed;
  return true;
}

bool parseRoot(QByteArrayView payload, const char *context,
               QJsonObject *root, TaskListWireError *error, QString *message) {
  if (payload.size() > WireLimits::MaxPayloadBytes) {
    *error = TaskListWireError::PayloadTooLarge;
    *message = errorMessage(context, QStringLiteral("payload exceeds the shell wire limit"));
    return false;
  }
  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(
      QByteArray(payload.data(), payload.size()), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    *error = TaskListWireError::MalformedPayload;
    *message = errorMessage(context, QStringLiteral("payload is not a JSON object"));
    return false;
  }
  *root = document.object();
  return true;
}

bool requireOkStatus(const QJsonObject &root, const char *context,
                     TaskListWireError *error, QString *message) {
  const QJsonValue status = root.value(QLatin1StringView("status"));
  if (!status.isString()) {
    *error = TaskListWireError::MalformedPayload;
    *message = errorMessage(context, QStringLiteral("status is missing"));
    return false;
  }
  if (status.toString() == QLatin1StringView("unavailable")) {
    *error = TaskListWireError::Unavailable;
    *message = errorMessage(context, QStringLiteral("inventory is unavailable"));
    return false;
  }
  if (status.toString() != QLatin1StringView("ok")) {
    *error = TaskListWireError::MalformedPayload;
    *message = errorMessage(context, QStringLiteral("status is unknown"));
    return false;
  }
  return true;
}

bool readWindowEntry(const QJsonValue &value, TaskListWireWindow *window) {
  if (!value.isObject()) {
    return false;
  }
  const QJsonObject object = value.toObject();
  return readIdentifier(object, QLatin1StringView("id"), false,
                        &window->windowId) &&
         readIdentifier(object, QLatin1StringView("applicationId"), false,
                        &window->applicationId) &&
         readBoundedText(object, QLatin1StringView("title"), &window->title) &&
         readIdentifier(object, QLatin1StringView("containerId"), true,
                        &window->containerId) &&
         readBoolean(object, QLatin1StringView("active"), &window->active) &&
         readBoolean(object, QLatin1StringView("minimized"),
                     &window->minimized) &&
         readBoolean(object, QLatin1StringView("skipTaskbar"),
                     &window->skipTaskbar) &&
         readBoolean(object, QLatin1StringView("skipSwitcher"),
                     &window->skipSwitcher);
}

bool readContainerEntry(const QJsonValue &value,
                        TaskListWireContainer *container) {
  if (!value.isObject()) {
    return false;
  }
  const QJsonObject object = value.toObject();
  if (!readIdentifier(object, QLatin1StringView("id"), false,
                      &container->containerId) ||
      !readCanonicalRevision(object.value(QLatin1StringView("revision")),
                             &container->revision)) {
    return false;
  }
  const QJsonValue authority = object.value(QLatin1StringView("authority"));
  if (!authority.isString()) {
    return false;
  }
  if (authority.toString() == QLatin1StringView("control-bridge")) {
    container->authority = TaskListContainerAuthority::ControlBridge;
    return true;
  }
  if (authority.toString() == QLatin1StringView("hybrid-process")) {
    container->authority = TaskListContainerAuthority::HybridProcess;
    return true;
  }
  return false;
}

bool readScopeEntry(const QJsonValue &value, TaskListWireScope *scope) {
  if (!value.isObject()) {
    return false;
  }
  const QJsonObject object = value.toObject();
  if (!readIdentifier(object, QLatin1StringView("id"), false,
                      &scope->windowId) ||
      !readIdentifier(object, QLatin1StringView("outputId"), false,
                      &scope->outputId) ||
      !readBoolean(object, QLatin1StringView("onAllWorkspaces"),
                   &scope->onAllWorkspaces)) {
    return false;
  }
  const QJsonValue workspaces = object.value(QLatin1StringView("workspaceIds"));
  if (!workspaces.isArray()) {
    return false;
  }
  const QJsonArray entries = workspaces.toArray();
  // AGENT-GUARD: onAllWorkspaces requires an empty workspace list; accepting
  // both would make scope filtering depend on producer choice.
  if (scope->onAllWorkspaces != entries.isEmpty()) {
    return false;
  }
  if (entries.size() > WireLimits::MaxScopeMemberships) {
    return false;
  }
  QSet<QString> seen;
  for (const QJsonValue &entry : entries) {
    if (!entry.isString()) {
      return false;
    }
    const QString workspaceId = entry.toString();
    if (workspaceId.isEmpty() || workspaceId.size() > kMaxIdLength ||
        !isCleanText(workspaceId) || seen.contains(workspaceId)) {
      return false;
    }
    seen.insert(workspaceId);
    scope->workspaceIds.append(workspaceId);
  }
  return true;
}

} // namespace

TaskListWindowsResult TaskListWireDecoder::decodeWindows(
    QByteArrayView payload) {
  TaskListWindowsResult result;
  QJsonObject root;
  if (!parseRoot(payload, "compositor windows", &root, &result.error,
                 &result.message) ||
      !requireOkStatus(root, "compositor windows", &result.error,
                       &result.message)) {
    return result;
  }
  const QJsonValue windows = root.value(QLatin1StringView("windows"));
  if (!windows.isArray()) {
    result.error = TaskListWireError::MalformedPayload;
    result.message = errorMessage("compositor windows",
                                  QStringLiteral("window collection is missing"));
    return result;
  }
  const QJsonArray entries = windows.toArray();
  if (entries.size() > kMaxWindowFacts) {
    result.error = TaskListWireError::LimitExceeded;
    result.message = errorMessage("compositor windows",
                                  QStringLiteral("window count exceeds the bound"));
    return result;
  }
  QSet<QString> seenIds;
  result.windows.reserve(entries.size());
  for (const QJsonValue &entry : entries) {
    TaskListWireWindow window;
    if (!readWindowEntry(entry, &window)) {
      result.error = TaskListWireError::InvalidWindow;
      result.message = errorMessage("compositor windows",
                                    QStringLiteral("window entry is invalid"));
      return result;
    }
    if (seenIds.contains(window.windowId)) {
      result.error = TaskListWireError::DuplicateWindowId;
      result.message = errorMessage("compositor windows",
                                    QStringLiteral("duplicate window id"));
      return result;
    }
    seenIds.insert(window.windowId);
    result.windows.append(std::move(window));
  }
  return result;
}

TaskListContainersResult TaskListWireDecoder::decodeContainers(
    QByteArrayView payload) {
  TaskListContainersResult result;
  QJsonObject root;
  if (!parseRoot(payload, "compositor containers", &root, &result.error,
                 &result.message) ||
      !requireOkStatus(root, "compositor containers", &result.error,
                       &result.message)) {
    return result;
  }
  const QJsonValue containers = root.value(QLatin1StringView("containers"));
  if (!containers.isArray()) {
    result.error = TaskListWireError::MalformedPayload;
    result.message = errorMessage("compositor containers",
                                  QStringLiteral("container collection is missing"));
    return result;
  }
  const QJsonArray entries = containers.toArray();
  if (entries.size() > kMaxWireContainers) {
    result.error = TaskListWireError::LimitExceeded;
    result.message = errorMessage("compositor containers",
                                  QStringLiteral("container count exceeds the bound"));
    return result;
  }
  QSet<QString> seenIds;
  result.containers.reserve(entries.size());
  for (const QJsonValue &entry : entries) {
    TaskListWireContainer container;
    if (!readContainerEntry(entry, &container)) {
      result.error = TaskListWireError::InvalidContainer;
      result.message = errorMessage("compositor containers",
                                    QStringLiteral("container entry is invalid"));
      return result;
    }
    if (seenIds.contains(container.containerId)) {
      result.error = TaskListWireError::DuplicateContainerId;
      result.message = errorMessage("compositor containers",
                                    QStringLiteral("duplicate container id"));
      return result;
    }
    seenIds.insert(container.containerId);
    result.containers.append(std::move(container));
  }
  return result;
}

TaskListScopeResult TaskListWireDecoder::decodeScopeSnapshot(
    QByteArrayView payload) {
  TaskListScopeResult result;
  QJsonObject root;
  if (!parseRoot(payload, "compositor scope snapshot", &root, &result.error,
                 &result.message) ||
      !requireOkStatus(root, "compositor scope snapshot", &result.error,
                       &result.message)) {
    return result;
  }
  quint64 schemaVersion = 0;
  const QJsonValue version = root.value(QLatin1StringView("schemaVersion"));
  if (!version.isDouble() ||
      (schemaVersion = static_cast<quint64>(version.toDouble())) != 1 ||
      static_cast<double>(schemaVersion) != version.toDouble()) {
    result.error = TaskListWireError::UnsupportedSchema;
    result.message = errorMessage("compositor scope snapshot",
                                  QStringLiteral("schema version is unsupported"));
    return result;
  }
  if (!readIdentifier(root, QLatin1StringView("epoch"), false,
                      &result.snapshot.epoch) ||
      !readCanonicalRevision(root.value(QLatin1StringView("revision")),
                             &result.snapshot.revision)) {
    result.error = TaskListWireError::InvalidLineage;
    result.message = errorMessage("compositor scope snapshot",
                                  QStringLiteral("epoch or revision is invalid"));
    return result;
  }
  const QJsonValue windows = root.value(QLatin1StringView("windows"));
  if (!windows.isArray()) {
    result.error = TaskListWireError::MalformedPayload;
    result.message = errorMessage("compositor scope snapshot",
                                  QStringLiteral("window collection is missing"));
    return result;
  }
  const QJsonArray entries = windows.toArray();
  if (entries.size() > WireLimits::MaxWindows) {
    result.error = TaskListWireError::LimitExceeded;
    result.message = errorMessage("compositor scope snapshot",
                                  QStringLiteral("window count exceeds the bound"));
    return result;
  }
  QSet<QString> seenIds;
  result.snapshot.scopes.reserve(entries.size());
  for (const QJsonValue &entry : entries) {
    TaskListWireScope scope;
    if (!readScopeEntry(entry, &scope)) {
      result.error = TaskListWireError::InvalidScope;
      result.message = errorMessage("compositor scope snapshot",
                                    QStringLiteral("window scope entry is invalid"));
      return result;
    }
    if (seenIds.contains(scope.windowId)) {
      result.error = TaskListWireError::DuplicateScopeId;
      result.message = errorMessage("compositor scope snapshot",
                                    QStringLiteral("duplicate scope window id"));
      return result;
    }
    seenIds.insert(scope.windowId);
    result.snapshot.scopes.append(std::move(scope));
  }
  return result;
}

} // namespace QindaQt::ShellTaskList::Producer
