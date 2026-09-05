// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <QVector>

// Fixture builders for the legacy Compositor1 inventories and the atomic
// authenticated CompositorShell1 task-fact snapshot.
namespace TaskListProducerTest {

inline const QString kWindowEpoch =
    QStringLiteral("3d975df8-3ee6-4cc5-bf64-3d46cab972d0");

inline QJsonObject windowJson(const QString &id, const QString &applicationId,
                              const QString &containerId = {},
                              bool active = false, bool minimized = false,
                              bool skipTaskbar = false) {
  return QJsonObject{{QStringLiteral("id"), id},
                     {QStringLiteral("title"),
                      QStringLiteral("Title %1").arg(id)},
                     {QStringLiteral("applicationId"), applicationId},
                     {QStringLiteral("containerId"), containerId},
                     {QStringLiteral("active"), active},
                     {QStringLiteral("minimized"), minimized},
                     {QStringLiteral("skipTaskbar"), skipTaskbar},
                     {QStringLiteral("skipSwitcher"), skipTaskbar}};
}

inline QByteArray windowsPayload(const QJsonArray &windows,
                                 quint64 revision = 1,
                                 const QString &epoch = kWindowEpoch,
                                 bool generationAvailable = true) {
  return QJsonDocument(
             QJsonObject{{QStringLiteral("status"), QStringLiteral("ok")},
                         {QStringLiteral("schemaVersion"), 2},
                         {QStringLiteral("epoch"), epoch},
                         {QStringLiteral("revision"),
                          QString::number(revision)},
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

inline QJsonObject taskWindowJson(
    const QString &id, const QString &applicationId,
    const QString &role = QStringLiteral("standalone"),
    const QString &containerId = {}, bool active = false,
    bool minimized = false, bool attention = false) {
  return {{QStringLiteral("id"), id},
          {QStringLiteral("applicationId"), applicationId},
          {QStringLiteral("applicationName"), applicationId},
          {QStringLiteral("title"), QStringLiteral("Title %1").arg(id)},
          {QStringLiteral("role"), role},
          {QStringLiteral("windowType"), QStringLiteral("normal")},
          {QStringLiteral("ownerRole"), QStringLiteral("application")},
          {QStringLiteral("active"), active},
          {QStringLiteral("minimized"), minimized},
          {QStringLiteral("maximized"), false},
          {QStringLiteral("fullscreen"), false},
          {QStringLiteral("demandsAttention"), attention},
          {QStringLiteral("outputId"), QStringLiteral("WL-0")},
          {QStringLiteral("workspaceIds"),
           QJsonArray{QStringLiteral("workspace-1")}},
          {QStringLiteral("onAllWorkspaces"), false},
          {QStringLiteral("containerId"), containerId}};
}

inline QByteArray taskFactsPayload(
    const QJsonArray &windows, quint64 revision = 1,
    const QString &epoch = kWindowEpoch,
    const QVector<ContainerFixture> &containerEntries = {}) {
  QJsonArray containers;
  for (const auto &entry : containerEntries) {
    containers.append(QJsonObject{
        {QStringLiteral("id"), entry.containerId},
        {QStringLiteral("revision"), QString::number(entry.revision)},
        {QStringLiteral("authority"), entry.authority}});
  }
  return QJsonDocument(QJsonObject{
      {QStringLiteral("status"), QStringLiteral("ok")},
      {QStringLiteral("schemaVersion"), 1},
      {QStringLiteral("epoch"), epoch},
      {QStringLiteral("revision"), QString::number(revision)},
      {QStringLiteral("actionRevision"), QString::number(revision + 10)},
      {QStringLiteral("outputs"),
       QJsonArray{QJsonObject{{QStringLiteral("id"), QStringLiteral("WL-0")}}}},
      {QStringLiteral("workspaces"),
       QJsonArray{QJsonObject{{QStringLiteral("id"),
                               QStringLiteral("workspace-1")}}}},
      {QStringLiteral("containers"), containers},
      {QStringLiteral("windows"), windows},
  }).toJson(QJsonDocument::Compact);
}

struct StandardScene {
  QByteArray windows;
  QByteArray containers;
  QByteArray taskFacts;
};

inline StandardScene standardScene(quint64 revision = 1,
                                   const QString &epoch = kWindowEpoch) {
  StandardScene scene;
  scene.windows = windowsPayload(
      QJsonArray{windowJson(QStringLiteral("w1"), QStringLiteral("app.one")),
                 windowJson(QStringLiteral("w2"), QStringLiteral("app.two"),
                            QStringLiteral("c1")),
                 windowJson(QStringLiteral("w3"), QStringLiteral("app.three"),
                            QStringLiteral("c1"), false, true, true)},
      revision, epoch);
  scene.containers = containersPayload(
      {{QStringLiteral("c1"), 7, QStringLiteral("hybrid-process")}});
  scene.taskFacts = taskFactsPayload(
      QJsonArray{
          taskWindowJson(QStringLiteral("w1"), QStringLiteral("app.one")),
          taskWindowJson(QStringLiteral("w2"), QStringLiteral("app.two"),
                         QStringLiteral("container-primary"),
                         QStringLiteral("c1")),
          taskWindowJson(QStringLiteral("w3"), QStringLiteral("app.three"),
                         QStringLiteral("container-member"),
                         QStringLiteral("c1"), false, true, true)},
      revision, epoch,
      {{QStringLiteral("c1"), 7, QStringLiteral("hybrid-process")}});
  return scene;
}

inline StandardScene standaloneScene(int count, quint64 revision = 1) {
  QJsonArray windows;
  QJsonArray taskWindows;
  for (int index = 0; index < count; ++index) {
    const QString id = QStringLiteral("w%1").arg(index);
    windows.append(windowJson(id, QStringLiteral("app.%1").arg(id)));
    taskWindows.append(taskWindowJson(id, QStringLiteral("app.%1").arg(id)));
  }
  StandardScene scene;
  scene.windows = windowsPayload(windows, revision);
  scene.containers = containersPayload({});
  scene.taskFacts = taskFactsPayload(taskWindows, revision);
  return scene;
}

} // namespace TaskListProducerTest
