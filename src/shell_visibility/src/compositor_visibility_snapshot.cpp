// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/shell_visibility/compositor_visibility_snapshot.h"

#include "qindaqt/shell_visibility/panel_visibility_policy.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSet>
#include <QStringView>

#include <cmath>
#include <limits>
#include <utility>

namespace QindaQt::ShellVisibility {
namespace {

using ErrorCode = CompositorSnapshotErrorCode;
using Result = CompositorSnapshotDecodeResult;

Result failure(ErrorCode code, QString path, QString message) {
  return {{}, {code, std::move(path), std::move(message)}};
}

bool hasCanonicalIdentifier(const QJsonValue &value, QString *result) {
  if (!value.isString()) {
    return false;
  }
  const QString text = value.toString();
  const QStringView view(text);
  if (view.isEmpty() || view.size() >
                            CompositorVisibilitySnapshotDecoder::
                                MaxIdentifierCharacters ||
      view != view.trimmed()) {
    return false;
  }
  for (qsizetype index = 0; index < view.size(); ++index) {
    if (view[index].isHighSurrogate()) {
      if (index + 1 >= view.size() || !view[index + 1].isLowSurrogate()) {
        return false;
      }
      ++index;
    } else if (view[index].isLowSurrogate()) {
      return false;
    }
  }
  *result = text;
  return true;
}

bool exactInteger(const QJsonValue &value, int *result) {
  if (!value.isDouble()) {
    return false;
  }
  const double number = value.toDouble();
  if (!std::isfinite(number) || std::trunc(number) != number ||
      number < std::numeric_limits<int>::min() ||
      number > std::numeric_limits<int>::max()) {
    return false;
  }
  *result = static_cast<int>(number);
  return true;
}

bool geometry(const QJsonValue &value, QRect *result) {
  if (!value.isObject()) {
    return false;
  }
  const QJsonObject object = value.toObject();
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  if (!exactInteger(object.value(QStringLiteral("x")), &x) ||
      !exactInteger(object.value(QStringLiteral("y")), &y) ||
      !exactInteger(object.value(QStringLiteral("width")), &width) ||
      !exactInteger(object.value(QStringLiteral("height")), &height) ||
      width <= 0 || height <= 0) {
    return false;
  }
  const qint64 right = static_cast<qint64>(x) + width - 1;
  const qint64 bottom = static_cast<qint64>(y) + height - 1;
  if (right > std::numeric_limits<int>::max() ||
      bottom > std::numeric_limits<int>::max()) {
    return false;
  }
  *result = QRect(x, y, width, height);
  return true;
}

bool canonicalRevision(const QJsonValue &value, quint64 *result) {
  if (!value.isString()) {
    return false;
  }
  const QString text = value.toString();
  if (text.isEmpty() || (text.size() > 1 && text.startsWith(QLatin1Char('0')))) {
    return false;
  }
  for (const QChar character : text) {
    if (!character.isDigit() || character.unicode() > u'9') {
      return false;
    }
  }
  bool ok = false;
  const quint64 revision = text.toULongLong(&ok, 10);
  // status:"ok" begins at revision one. Revision zero belongs only to the
  // compositor's explicit unavailable payload and must never reach policy.
  if (!ok || revision == 0) {
    return false;
  }
  *result = revision;
  return true;
}

bool stringArray(const QJsonValue &value, QStringList *result) {
  if (!value.isArray()) {
    return false;
  }
  const QJsonArray array = value.toArray();
  if (array.size() >
      CompositorVisibilitySnapshotDecoder::MaxScopeMemberships) {
    return false;
  }
  QStringList parsed;
  QSet<QString> unique;
  parsed.reserve(array.size());
  for (const QJsonValue &entry : array) {
    QString identifier;
    if (!hasCanonicalIdentifier(entry, &identifier) ||
        unique.contains(identifier)) {
      return false;
    }
    unique.insert(identifier);
    parsed.push_back(std::move(identifier));
  }
  *result = std::move(parsed);
  return true;
}

bool exactBoolean(const QJsonObject &object, QStringView name, bool *result) {
  const QJsonValue value = object.value(name.toString());
  if (!value.isBool()) {
    return false;
  }
  *result = value.toBool();
  return true;
}

Result decodeOutputs(const QJsonValue &value,
                     CompositorVisibilitySnapshot *snapshot) {
  if (!value.isArray()) {
    return failure(ErrorCode::InvalidField, QStringLiteral("outputs"),
                   QStringLiteral("outputs must be an array"));
  }
  const QJsonArray array = value.toArray();
  if (array.size() > CompositorVisibilitySnapshotDecoder::MaxOutputs) {
    return failure(ErrorCode::CollectionLimitExceeded,
                   QStringLiteral("outputs"),
                   QStringLiteral("output count exceeds the wire limit"));
  }
  snapshot->outputs.reserve(array.size());
  for (qsizetype index = 0; index < array.size(); ++index) {
    const QString path = QStringLiteral("outputs[%1]").arg(index);
    if (!array[index].isObject()) {
      return failure(ErrorCode::InvalidField, path,
                     QStringLiteral("output entry must be an object"));
    }
    const QJsonObject object = array[index].toObject();
    LogicalOutputSnapshot output;
    double scale = object.value(QStringLiteral("scale")).toDouble(
        std::numeric_limits<double>::quiet_NaN());
    if (!hasCanonicalIdentifier(object.value(QStringLiteral("id")),
                                &output.id) ||
        !geometry(object.value(QStringLiteral("geometry")), &output.geometry) ||
        !std::isfinite(scale) || scale <= 0.0 ||
        scale > CompositorVisibilitySnapshotDecoder::MaxOutputScale) {
      return failure(ErrorCode::InvalidField, path,
                     QStringLiteral("output fields are malformed"));
    }
    output.scale = scale;
    snapshot->outputs.push_back(std::move(output));
  }
  return {CompositorVisibilitySnapshot{}, {}};
}

Result decodeWindows(const QJsonValue &value,
                     CompositorVisibilitySnapshot *snapshot) {
  if (!value.isArray()) {
    return failure(ErrorCode::InvalidField, QStringLiteral("windows"),
                   QStringLiteral("windows must be an array"));
  }
  const QJsonArray array = value.toArray();
  if (array.size() > CompositorVisibilitySnapshotDecoder::MaxWindows) {
    return failure(ErrorCode::CollectionLimitExceeded,
                   QStringLiteral("windows"),
                   QStringLiteral("window count exceeds the wire limit"));
  }
  snapshot->windows.reserve(array.size());
  for (qsizetype index = 0; index < array.size(); ++index) {
    const QString path = QStringLiteral("windows[%1]").arg(index);
    if (!array[index].isObject()) {
      return failure(ErrorCode::InvalidField, path,
                     QStringLiteral("window entry must be an object"));
    }
    const QJsonObject object = array[index].toObject();
    LogicalWindowSnapshot window;
    if (!hasCanonicalIdentifier(object.value(QStringLiteral("id")), &window.id) ||
        !hasCanonicalIdentifier(object.value(QStringLiteral("outputId")),
                                &window.outputId) ||
        !geometry(object.value(QStringLiteral("frameGeometry")),
                  &window.frameGeometry) ||
        !stringArray(object.value(QStringLiteral("workspaceIds")),
                     &window.workspaceIds) ||
        !stringArray(object.value(QStringLiteral("activityIds")),
                     &window.activityIds) ||
        !exactBoolean(object, QStringLiteral("onAllWorkspaces"),
                      &window.onAllWorkspaces) ||
        !exactBoolean(object, QStringLiteral("active"), &window.active) ||
        !exactBoolean(object, QStringLiteral("maximized"), &window.maximized) ||
        !exactBoolean(object, QStringLiteral("minimized"), &window.minimized) ||
        !exactBoolean(object, QStringLiteral("hidden"), &window.hidden)) {
      return failure(ErrorCode::InvalidField, path,
                     QStringLiteral("window fields are malformed"));
    }
    snapshot->windows.push_back(std::move(window));
  }
  return {CompositorVisibilitySnapshot{}, {}};
}

} // namespace

CompositorSnapshotDecodeResult CompositorVisibilitySnapshotDecoder::decode(
    const QByteArray &payload) {
  if (payload.size() > MaxPayloadBytes) {
    return failure(ErrorCode::PayloadTooLarge, QStringLiteral("$"),
                   QStringLiteral("compositor snapshot exceeds the byte limit"));
  }
  QJsonParseError parseError;
  const QJsonDocument document = QJsonDocument::fromJson(payload, &parseError);
  if (parseError.error != QJsonParseError::NoError) {
    return failure(ErrorCode::InvalidJson, QStringLiteral("$"),
                   QStringLiteral("invalid compositor snapshot JSON: %1")
                       .arg(parseError.errorString()));
  }
  if (!document.isObject()) {
    return failure(ErrorCode::InvalidRoot, QStringLiteral("$"),
                   QStringLiteral("compositor snapshot root must be an object"));
  }
  const QJsonObject root = document.object();
  if (root.value(QStringLiteral("status")) != QStringLiteral("ok")) {
    return failure(ErrorCode::InvalidRoot, QStringLiteral("status"),
                   QStringLiteral("compositor snapshot status must be 'ok'"));
  }
  int schemaVersion = 0;
  if (!exactInteger(root.value(QStringLiteral("schemaVersion")),
                    &schemaVersion) ||
      schemaVersion != 1) {
    return failure(ErrorCode::UnsupportedSchema,
                   QStringLiteral("schemaVersion"),
                   QStringLiteral("only compositor snapshot schema 1 is supported"));
  }

  CompositorVisibilitySnapshot snapshot;
  if (!hasCanonicalIdentifier(root.value(QStringLiteral("epoch")),
                              &snapshot.epoch)) {
    return failure(ErrorCode::InvalidEpoch, QStringLiteral("epoch"),
                   QStringLiteral("epoch must be a canonical non-empty string"));
  }
  if (!canonicalRevision(root.value(QStringLiteral("revision")),
                         &snapshot.revision)) {
    return failure(ErrorCode::InvalidRevision, QStringLiteral("revision"),
                   QStringLiteral("revision must be a canonical decimal string"));
  }
  if (!canonicalRevision(root.value(QStringLiteral("outputGeneration")),
                         &snapshot.outputGeneration)) {
    return failure(ErrorCode::InvalidOutputGeneration,
                   QStringLiteral("outputGeneration"),
                   QStringLiteral("output generation must be a canonical non-zero decimal string"));
  }
  const QJsonValue scopeValue = root.value(QStringLiteral("scope"));
  if (!scopeValue.isObject()) {
    return failure(ErrorCode::InvalidField, QStringLiteral("scope"),
                   QStringLiteral("scope must be an object"));
  }
  const QJsonObject scope = scopeValue.toObject();
  if (!hasCanonicalIdentifier(scope.value(QStringLiteral("workspaceId")),
                              &snapshot.scope.workspaceId) ||
      !hasCanonicalIdentifier(scope.value(QStringLiteral("activityId")),
                              &snapshot.scope.activityId)) {
    return failure(ErrorCode::InvalidField, QStringLiteral("scope"),
                   QStringLiteral("scope identifiers are malformed"));
  }

  Result parsed = decodeOutputs(root.value(QStringLiteral("outputs")), &snapshot);
  if (parsed.error.hasError()) {
    return parsed;
  }
  parsed = decodeWindows(root.value(QStringLiteral("windows")), &snapshot);
  if (parsed.error.hasError()) {
    return parsed;
  }

  PanelVisibilityInventory inventory;
  inventory.outputs = snapshot.outputs;
  inventory.windows = snapshot.windows;
  inventory.scope = snapshot.scope;
  const PanelVisibilityEvaluation validation =
      PanelVisibilityPolicy::evaluate(inventory);
  if (!validation.ok()) {
    return failure(ErrorCode::InvalidInventory, QStringLiteral("$"),
                   validation.error.message);
  }
  return {std::move(snapshot), {}};
}

} // namespace QindaQt::ShellVisibility
