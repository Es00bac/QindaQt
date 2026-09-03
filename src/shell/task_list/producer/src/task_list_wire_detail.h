// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

// Private implementation detail of the task-list wire decoders: shared
// hostile-input primitives used by task_list_wire.cpp (Windows/Containers
// inventories) and task_list_scope_wire.cpp (ShellVisibilitySnapshot). Not
// installed; never include this from outside the producer module.

#include "qindaqt/shell/task_list/producer/task_list_wire.h"
#include "qindaqt/shell/task_list/task_list_types.h"
#include "qindaqt/shell_visibility_protocol/wire_limits.h"

#include <QJsonDocument>
#include <QJsonObject>

namespace QindaQt::ShellTaskList::Producer::Detail {

inline QString errorMessage(const char *context, const QString &detail) {
  return QString::fromLatin1(context) + QStringLiteral(": ") + detail;
}

inline bool isCleanText(const QString &value) {
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

inline bool readIdentifier(const QJsonObject &object, QLatin1StringView key,
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

inline bool readBoundedText(const QJsonObject &object, QLatin1StringView key,
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

inline bool readBoolean(const QJsonObject &object, QLatin1StringView key,
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
inline bool readCanonicalRevision(const QJsonValue &value,
                                  quint64 *destination) {
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

// The schema-2 Windows() fence revision is "0" while no visibility generation
// exists, so zero is admissible here (unlike accepted-generation revisions).
inline bool readFenceRevision(const QJsonValue &value, quint64 *destination) {
  if (!value.isString()) {
    return false;
  }
  const QString text = value.toString();
  bool converted = false;
  const quint64 parsed = text.toULongLong(&converted, 10);
  if (!converted || QString::number(parsed) != text) {
    return false;
  }
  *destination = parsed;
  return true;
}

// Epochs are fresh UUIDs per compositor instance (compositor-control-v1.md);
// a non-UUID epoch is not lineage.
inline bool isUuidShape(const QString &text) {
  if (text.size() != 36) {
    return false;
  }
  for (qsizetype index = 0; index < text.size(); ++index) {
    const QChar character = text.at(index);
    if (index == 8 || index == 13 || index == 18 || index == 23) {
      if (character != QLatin1Char('-')) {
        return false;
      }
      continue;
    }
    const ushort lower = character.toLower().unicode();
    const bool hex = (lower >= '0' && lower <= '9') ||
                     (lower >= 'a' && lower <= 'f');
    if (!hex) {
      return false;
    }
  }
  return true;
}

inline bool parseRoot(QByteArrayView payload, const char *context,
                      QJsonObject *root, TaskListWireError *error,
                      QString *message) {
  if (payload.size() > ShellVisibilityProtocol::WireLimits::MaxPayloadBytes) {
    *error = TaskListWireError::PayloadTooLarge;
    *message =
        errorMessage(context, QStringLiteral("payload exceeds the shell wire limit"));
    return false;
  }
  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(
      QByteArray(payload.data(), payload.size()), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    *error = TaskListWireError::MalformedPayload;
    *message =
        errorMessage(context, QStringLiteral("payload is not a JSON object"));
    return false;
  }
  *root = document.object();
  return true;
}

inline bool requireOkStatus(const QJsonObject &root, const char *context,
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

} // namespace QindaQt::ShellTaskList::Producer::Detail
