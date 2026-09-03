// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/task_list/producer/task_list_wire.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include "task_list_producer_test_support.h"

using namespace QindaQt::ShellTaskList::Producer;
using namespace TaskListProducerTest;

namespace {

QByteArray rawRoot(const QJsonObject &root) {
  return QJsonDocument(root).toJson(QJsonDocument::Compact);
}

} // namespace

class TaskListWireTests final : public QObject {
  Q_OBJECT

private slots:
  void decodesValidInventories();
  void rejectsMalformedWindowsPayload();
  void rejectsHostileWindowEntries();
  void rejectsOversizedWindowsPayload();
  void rejectsMalformedContainersPayload();
  void rejectsHostileContainerEntries();
  void rejectsMalformedScopePayload();
  void rejectsHostileScopeEntries();
};

void TaskListWireTests::decodesValidInventories() {
  const StandardScene scene = standardScene();

  const auto windows = TaskListWireDecoder::decodeWindows(scene.windows);
  QVERIFY2(windows.ok(), qPrintable(windows.message));
  QCOMPARE(windows.windows.size(), 3);
  QCOMPARE(windows.windows.at(0).windowId, QStringLiteral("w1"));
  QCOMPARE(windows.windows.at(1).containerId, QStringLiteral("c1"));
  QVERIFY(!windows.windows.at(1).skipTaskbar);
  QVERIFY(windows.windows.at(2).skipTaskbar);
  QVERIFY(windows.windows.at(2).minimized);

  const auto containers = TaskListWireDecoder::decodeContainers(scene.containers);
  QVERIFY2(containers.ok(), qPrintable(containers.message));
  QCOMPARE(containers.containers.size(), 1);
  QCOMPARE(containers.containers.constFirst().containerId, QStringLiteral("c1"));
  QCOMPARE(containers.containers.constFirst().revision, quint64(7));
  QCOMPARE(containers.containers.constFirst().authority,
           TaskListContainerAuthority::HybridProcess);

  const auto scope = TaskListWireDecoder::decodeScopeSnapshot(scene.scope);
  QVERIFY2(scope.ok(), qPrintable(scope.message));
  QCOMPARE(scope.snapshot.revision, quint64(1));
  QCOMPARE(scope.snapshot.scopes.size(), 3);
  QCOMPARE(scope.snapshot.scopes.at(0).outputId, QStringLiteral("output-1"));
  QCOMPARE(scope.snapshot.scopes.at(0).workspaceIds,
           QStringList{QStringLiteral("ws-1")});
}

void TaskListWireTests::rejectsMalformedWindowsPayload() {
  QCOMPARE(TaskListWireDecoder::decodeWindows("not json").error,
           TaskListWireError::MalformedPayload);
  QCOMPARE(TaskListWireDecoder::decodeWindows("[1]").error,
           TaskListWireError::MalformedPayload);
  QCOMPARE(TaskListWireDecoder::decodeWindows("{}").error,
           TaskListWireError::MalformedPayload);
  QCOMPARE(TaskListWireDecoder::decodeWindows(
               rawRoot({{QStringLiteral("status"),
                         QStringLiteral("unavailable")}}))
               .error,
           TaskListWireError::Unavailable);
  QCOMPARE(TaskListWireDecoder::decodeWindows(
               rawRoot({{QStringLiteral("status"), QStringLiteral("bogus")}}))
               .error,
           TaskListWireError::MalformedPayload);
  QCOMPARE(TaskListWireDecoder::decodeWindows(
               rawRoot({{QStringLiteral("status"), QStringLiteral("ok")}}))
               .error,
           TaskListWireError::MalformedPayload);
}

void TaskListWireTests::rejectsHostileWindowEntries() {
  auto withWindow = [](QJsonObject window) {
    return windowsPayload({std::move(window)});
  };
  QCOMPARE(TaskListWireDecoder::decodeWindows(withWindow({})).error,
           TaskListWireError::InvalidWindow);

  auto oversized = windowJson(QStringLiteral("w1"), QStringLiteral("app"));
  oversized.insert(QStringLiteral("id"), QString(513, QLatin1Char('x')));
  QCOMPARE(TaskListWireDecoder::decodeWindows(withWindow(oversized)).error,
           TaskListWireError::InvalidWindow);

  auto controlTitle = windowJson(QStringLiteral("w1"), QStringLiteral("app"));
  controlTitle.insert(QStringLiteral("title"),
                      QStringLiteral("bad\ttitle"));
  QCOMPARE(TaskListWireDecoder::decodeWindows(withWindow(controlTitle)).error,
           TaskListWireError::InvalidWindow);

  auto loneSurrogate = windowJson(QStringLiteral("w1"), QStringLiteral("app"));
  loneSurrogate.insert(QStringLiteral("applicationId"),
                       QStringLiteral("app") + QChar(0xD800));
  QCOMPARE(TaskListWireDecoder::decodeWindows(withWindow(loneSurrogate)).error,
           TaskListWireError::InvalidWindow);

  auto emptyApplication = windowJson(QStringLiteral("w1"), QStringLiteral(""));
  QCOMPARE(
      TaskListWireDecoder::decodeWindows(withWindow(emptyApplication)).error,
      TaskListWireError::InvalidWindow);

  QCOMPARE(TaskListWireDecoder::decodeWindows(
               withWindow(windowJson(QStringLiteral("w1"), QStringLiteral("a"))))
               .error,
           TaskListWireError::None);
  QCOMPARE(TaskListWireDecoder::decodeWindows(windowsPayload(
                       {windowJson(QStringLiteral("w1"), QStringLiteral("a")),
                        windowJson(QStringLiteral("w1"), QStringLiteral("b"))}))
               .error,
           TaskListWireError::DuplicateWindowId);
}

void TaskListWireTests::rejectsOversizedWindowsPayload() {
  QByteArray payload = windowsPayload({});
  payload.append(QByteArray(4 * 1024 * 1024, ' '));
  QCOMPARE(TaskListWireDecoder::decodeWindows(payload).error,
           TaskListWireError::PayloadTooLarge);
}

void TaskListWireTests::rejectsMalformedContainersPayload() {
  QCOMPARE(TaskListWireDecoder::decodeContainers("[]").error,
           TaskListWireError::MalformedPayload);
  QCOMPARE(TaskListWireDecoder::decodeContainers(
               rawRoot({{QStringLiteral("status"), QStringLiteral("ok")}}))
               .error,
           TaskListWireError::MalformedPayload);
  QJsonArray entries;
  for (int index = 0; index < 2049; ++index) {
    entries.append(QJsonObject{
        {QStringLiteral("id"), QStringLiteral("c%1").arg(index)},
        {QStringLiteral("revision"), QStringLiteral("1")},
        {QStringLiteral("authority"), QStringLiteral("hybrid-process")}});
  }
  QCOMPARE(TaskListWireDecoder::decodeContainers(
               rawRoot({{QStringLiteral("status"), QStringLiteral("ok")},
                        {QStringLiteral("containers"), entries}}))
               .error,
           TaskListWireError::LimitExceeded);
}

void TaskListWireTests::rejectsHostileContainerEntries() {
  auto entry = [](const QString &id, const QString &revision,
                  const QString &authority) {
    return containersPayload({{id, 1, authority}}).replace(
        QByteArrayLiteral("\"1\""), revision.toUtf8());
  };
  QCOMPARE(TaskListWireDecoder::decodeContainers(
               containersPayload({{QStringLiteral("c1"), 1,
                                   QStringLiteral("bridge")}}))
               .error,
           TaskListWireError::InvalidContainer);
  QCOMPARE(TaskListWireDecoder::decodeContainers(
               containersPayload({{QStringLiteral("c1"), 0,
                                   QStringLiteral("hybrid-process")}}))
               .error,
           TaskListWireError::InvalidContainer);
  QCOMPARE(TaskListWireDecoder::decodeContainers(
               entry(QStringLiteral("c1"), QStringLiteral("\"007\""),
                     QStringLiteral("hybrid-process")))
               .error,
           TaskListWireError::InvalidContainer);
  QCOMPARE(TaskListWireDecoder::decodeContainers(
               containersPayload({{QStringLiteral("c1"), 1,
                                   QStringLiteral("control-bridge")},
                                  {QStringLiteral("c1"), 2,
                                   QStringLiteral("hybrid-process")}}))
               .error,
           TaskListWireError::DuplicateContainerId);
}

void TaskListWireTests::rejectsMalformedScopePayload() {
  QCOMPARE(TaskListWireDecoder::decodeScopeSnapshot("{}").error,
           TaskListWireError::MalformedPayload);
  QCOMPARE(TaskListWireDecoder::decodeScopeSnapshot(
               rawRoot({{QStringLiteral("status"),
                         QStringLiteral("unavailable")}}))
               .error,
           TaskListWireError::Unavailable);
  QCOMPARE(TaskListWireDecoder::decodeScopeSnapshot(
               rawRoot({{QStringLiteral("status"), QStringLiteral("ok")},
                        {QStringLiteral("schemaVersion"), 2},
                        {QStringLiteral("epoch"), QStringLiteral("e")},
                        {QStringLiteral("revision"), QStringLiteral("1")},
                        {QStringLiteral("windows"), QJsonArray{}}}))
               .error,
           TaskListWireError::UnsupportedSchema);
  QCOMPARE(TaskListWireDecoder::decodeScopeSnapshot(
               rawRoot({{QStringLiteral("status"), QStringLiteral("ok")},
                        {QStringLiteral("schemaVersion"), 1},
                        {QStringLiteral("epoch"), QStringLiteral("e")},
                        {QStringLiteral("revision"), QStringLiteral("0")},
                        {QStringLiteral("windows"), QJsonArray{}}}))
               .error,
           TaskListWireError::InvalidLineage);
}

void TaskListWireTests::rejectsHostileScopeEntries() {
  auto bothScopes = scopeEntryJson(QStringLiteral("w1"),
                                   QStringLiteral("output-1"),
                                   {QStringLiteral("ws-1")}, true);
  QCOMPARE(TaskListWireDecoder::decodeScopeSnapshot(scopePayload({bothScopes}))
               .error,
           TaskListWireError::InvalidScope);

  auto emptyOutput = scopeEntryJson(QStringLiteral("w1"), QStringLiteral(""),
                                    {QStringLiteral("ws-1")});
  QCOMPARE(TaskListWireDecoder::decodeScopeSnapshot(scopePayload({emptyOutput}))
               .error,
           TaskListWireError::InvalidScope);

  QCOMPARE(TaskListWireDecoder::decodeScopeSnapshot(scopePayload(
                       {scopeEntryJson(QStringLiteral("w1"),
                                       QStringLiteral("output-1"),
                                       {QStringLiteral("ws-1")}),
                        scopeEntryJson(QStringLiteral("w1"),
                                       QStringLiteral("output-2"),
                                       {QStringLiteral("ws-2")})}))
               .error,
           TaskListWireError::DuplicateScopeId);
}

QTEST_GUILESS_MAIN(TaskListWireTests)
#include "tst_task_list_wire.moc"
