// SPDX-License-Identifier: LGPL-3.0-or-later
#include "qindaqt/workspaces/workspace_store.h"
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QSaveFile>
namespace QindaQt::Workspaces {
namespace {
constexpr qint64 maxBytes = 2 * 1024 * 1024;
QString pathFor(const QString &root, const QString &id) {
  static const QRegularExpression pattern(
      QStringLiteral("^[A-Za-z0-9][A-Za-z0-9_.-]{0,127}$"));
  return pattern.match(id).hasMatch()
             ? QDir(root).filePath(id + QStringLiteral(".json"))
             : QString{};
}
void report(QString *error, const QString &message) {
  if (error)
    *error = message;
}
} // namespace
bool WorkspaceStore::save(const Workspace &workspace, QString *error) const {
  if (!workspace.validate(error))
    return false;
  const auto bytes = QJsonDocument(workspace.toJson()).toJson();
  if (bytes.size() > maxBytes) {
    report(error, QStringLiteral("Workspace is too large to save"));
    return false;
  }
  if (!QDir().mkpath(m_root)) {
    report(error, QStringLiteral("Cannot create the workspace folder"));
    return false;
  }
  QSaveFile file(pathFor(m_root, workspace.id));
  if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() ||
      !file.commit()) {
    report(error, file.errorString());
    return false;
  }
  return true;
}
std::optional<Workspace> WorkspaceStore::load(const QString &id,
                                              QString *error) const {
  if (error)
    error->clear();
  const auto path = pathFor(m_root, id);
  if (path.isEmpty()) {
    report(error, QStringLiteral("Workspace identifier is invalid"));
    return std::nullopt;
  }
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    report(error, file.errorString());
    return std::nullopt;
  }
  if (file.size() > maxBytes) {
    report(error, QStringLiteral("Workspace file is too large"));
    return std::nullopt;
  }
  QJsonParseError parseError;
  const auto document = QJsonDocument::fromJson(file.readAll(), &parseError);
  if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
    report(error, QStringLiteral("Workspace file is damaged: %1")
                      .arg(parseError.errorString()));
    return std::nullopt;
  }
  auto result = Workspace::fromJson(document.object(), error);
  if (result && result->id != id) {
    report(error, QStringLiteral("Workspace file has a different identity"));
    return std::nullopt;
  }
  return result;
}
QStringList WorkspaceStore::ids() const {
  QStringList result;
  for (const auto &name : QDir(m_root).entryList({QStringLiteral("*.json")},
                                                 QDir::Files, QDir::Name))
    result.append(name.chopped(5));
  return result;
}
} // namespace QindaQt::Workspaces
