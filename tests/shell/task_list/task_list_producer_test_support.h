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

// The shared schema-2 fence: Windows() names the retained
// ShellVisibilitySnapshot generation, so both builders default to the same
// epoch/revision and a refresh joins only when they match exactly.
inline const QString kScopeEpoch =
    QStringLiteral("3d975df8-3ee6-4cc5-bf64-3d46cab972d0");

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

inline QByteArray windowsPayload(const QJsonArray &windows,
                                 quint64 fenceRevision = 1,
                                 const QString &epoch = kScopeEpoch,
                                 bool generationAvailable = true) {
  return QJsonDocument(
             QJsonObject{{QStringLiteral("status"), QStringLiteral("ok")},
                         {QStringLiteral("schemaVersion"), 2},
                         {QStringLiteral("epoch"), epoch},
                         {QStringLiteral("revision"),
                          QString::number(fenceRevision)},
                         {QStringLiteral("generationAvailable"),
                          generationAvailable},
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

inline QJsonObject outputJson(const QString &id) {
  return QJsonObject{
      {QStringLiteral("id"), id},
      {QStringLiteral("geometry"),
       QJsonObject{{QStringLiteral("x"), 0},
                   {QStringLiteral("y"), 0},
                   {QStringLiteral("width"), 1920},
                   {QStringLiteral("height"), 1080}}},
      {QStringLiteral("scale"), 1.0}};
}

inline QByteArray scopePayload(const QJsonArray &windows, quint64 revision = 1,
                               const QString &epoch = kScopeEpoch) {
  return QJsonDocument(
             QJsonObject{{QStringLiteral("status"), QStringLiteral("ok")},
                         {QStringLiteral("schemaVersion"), 1},
                         {QStringLiteral("epoch"), epoch},
                         {QStringLiteral("revision"),
                          QString::number(revision)},
                         {QStringLiteral("outputGeneration"),
                          QStringLiteral("1")},
                         {QStringLiteral("scope"),
                          QJsonObject{
                              {QStringLiteral("workspaceId"),
                               QStringLiteral("workspace-1")},
                              {QStringLiteral("activityId"),
                               QStringLiteral("00000000-0000-0000-0000-"
                                              "000000000000")}}},
                         {QStringLiteral("outputs"),
                          QJsonArray{outputJson(QStringLiteral("output-1"))}},
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

inline StandardScene standardScene(quint64 fenceRevision = 1) {
  StandardScene scene;
  scene.windows = windowsPayload(
      {windowJson(QStringLiteral("w1"), QStringLiteral("app.one")),
       windowJson(QStringLiteral("w2"), QStringLiteral("app.two"),
                  QStringLiteral("c1")),
       windowJson(QStringLiteral("w3"), QStringLiteral("app.three"),
                  QStringLiteral("c1"), false, true, true)},
      fenceRevision);
  scene.containers = containersPayload(
      {{QStringLiteral("c1"), 7, QStringLiteral("hybrid-process")}});
  scene.scope = scopePayload(
      {scopeEntryJson(QStringLiteral("w1"), QStringLiteral("output-1"),
                      {QStringLiteral("ws-1")}),
       scopeEntryJson(QStringLiteral("w2"), QStringLiteral("output-1"),
                      {QStringLiteral("ws-1")}),
       scopeEntryJson(QStringLiteral("w3"), QStringLiteral("output-1"),
                      {QStringLiteral("ws-1")})},
      fenceRevision);
  return scene;
}

// A scene of `count` standalone windows for the at-limit stress rows.
inline StandardScene standaloneScene(int count, quint64 fenceRevision = 1) {
  QJsonArray windows;
  QJsonArray scopes;
  for (int index = 0; index < count; ++index) {
    const QString id = QStringLiteral("w%1").arg(index);
    windows.append(windowJson(id, QStringLiteral("app.%1").arg(id)));
    scopes.append(scopeEntryJson(id, QStringLiteral("output-1"),
                                 {QStringLiteral("ws-1")}));
  }
  StandardScene scene;
  scene.windows = windowsPayload(windows, fenceRevision);
  scene.containers = containersPayload({});
  scene.scope = scopePayload(scopes, fenceRevision);
  return scene;
}

} // namespace TaskListProducerTest
