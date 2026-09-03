// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QVector>

// Fixture builders for the exact public Compositor1 payload shapes consumed by
// the task-list producer (docs/wiki/reference/compositor-control-v1.md). Every
// builder emits wire-truthful JSON so tests only mutate the field under
// examination.
namespace TaskListProducerTest {

inline QJsonObject windowJson(const QString &id, const QString &applicationId,
                              const QString &containerId = {},
                              bool active = false, bool minimized = false,
                              bool skipTaskbar = false) {
  return QJsonObject{{QStringLiteral("id"), id},
                     {QStringLiteral("title"), QStringLiteral("Title %1").arg(id)},
                     {QStringLiteral("applicationId"), applicationId},
                     {QStringLiteral("containerId"), containerId},
                     {QStringLiteral("active"), active},
                     {QStringLiteral("minimized"), minimized},
                     {QStringLiteral("skipTaskbar"), skipTaskbar},
                     {QStringLiteral("skipSwitcher"), skipTaskbar}};
}

inline QByteArray windowsPayload(const QJsonArray &windows) {
  return QJsonDocument(QJsonObject{{QStringLiteral("status"),
                                    QStringLiteral("ok")},
                                   {QStringLiteral("windows"), windows}})
      .toJson(QJsonDocument::Compact);
}

inline QByteArray windowsPayload(std::initializer_list<QJsonObject> windows) {
  QJsonArray array;
  for (const QJsonObject &window : windows) {
    array.append(window);
  }
  return windowsPayload(array);
}

struct ContainerFixture {
  QString containerId;
  quint64 revision = 1;
  QString authority = QStringLiteral("hybrid-process");
};

inline QByteArray containersPayload(const QVector<ContainerFixture> &entries) {
  QJsonArray containers;
  for (const ContainerFixture &entry : entries) {
    containers.append(QJsonObject{
        {QStringLiteral("id"), entry.containerId},
        {QStringLiteral("revision"), QString::number(entry.revision)},
        {QStringLiteral("authority"), entry.authority}});
  }
  return QJsonDocument(QJsonObject{{QStringLiteral("status"),
                                    QStringLiteral("ok")},
                                   {QStringLiteral("containers"), containers}})
      .toJson(QJsonDocument::Compact);
}

inline QJsonObject scopeEntryJson(const QString &windowId,
                                  const QString &outputId,
                                  const QStringList &workspaceIds,
                                  bool onAllWorkspaces = false) {
  return QJsonObject{
      {QStringLiteral("id"), windowId},
      {QStringLiteral("outputId"), outputId},
      {QStringLiteral("workspaceIds"), QJsonArray::fromStringList(workspaceIds)},
      {QStringLiteral("onAllWorkspaces"), onAllWorkspaces}};
}

inline QByteArray scopePayload(const QJsonArray &windows,
                               quint64 revision = 1) {
  return QJsonDocument(
             QJsonObject{{QStringLiteral("status"), QStringLiteral("ok")},
                         {QStringLiteral("schemaVersion"), 1},
                         {QStringLiteral("epoch"),
                          QStringLiteral("3d975df8-3ee6-4cc5-bf64-3d46cab972d0")},
                         {QStringLiteral("revision"),
                          QString::number(revision)},
                         {QStringLiteral("outputGeneration"),
                          QStringLiteral("1")},
                         {QStringLiteral("outputs"),
                          QJsonArray{QJsonObject{
                              {QStringLiteral("id"), QStringLiteral("output-1")}}}},
                         {QStringLiteral("windows"), windows}})
      .toJson(QJsonDocument::Compact);
}

// A coherent minimal scene: standalone window "w1" plus a two-member hybrid
// container "c1" whose primary is "w2" (the single member carrying the
// collapsed native identity).
struct StandardScene {
  QByteArray windows;
  QByteArray containers;
  QByteArray scope;
};

inline StandardScene standardScene() {
  StandardScene scene;
  scene.windows = windowsPayload(
      {windowJson(QStringLiteral("w1"), QStringLiteral("app.one")),
       windowJson(QStringLiteral("w2"), QStringLiteral("app.two"),
                  QStringLiteral("c1")),
       windowJson(QStringLiteral("w3"), QStringLiteral("app.three"),
                  QStringLiteral("c1"), false, true, true)});
  scene.containers = containersPayload(
      {{QStringLiteral("c1"), 7, QStringLiteral("hybrid-process")}});
  scene.scope = scopePayload(
      {scopeEntryJson(QStringLiteral("w1"), QStringLiteral("output-1"),
                      {QStringLiteral("ws-1")}),
       scopeEntryJson(QStringLiteral("w2"), QStringLiteral("output-1"),
                      {QStringLiteral("ws-1")}),
       scopeEntryJson(QStringLiteral("w3"), QStringLiteral("output-1"),
                      {QStringLiteral("ws-1")})});
  return scene;
}

} // namespace TaskListProducerTest
