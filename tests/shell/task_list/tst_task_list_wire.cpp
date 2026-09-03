// SPDX-License-Identifier: GPL-3.0-or-later
#include "qindaqt/shell/task_list/producer/task_list_wire.h"

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
  void windowsInventoryRequiresSchemaTwoLineage();
  void windowsInventoryBoundIsExact();
  void rejectsMalformedContainersPayload();
  void rejectsHostileContainerEntries();
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
  QCOMPARE(windows.epoch, kWindowEpoch);
  QCOMPARE(windows.revision, quint64(1));
  QVERIFY(windows.generationAvailable);

  const auto containers = TaskListWireDecoder::decodeContainers(scene.containers);
  QVERIFY2(containers.ok(), qPrintable(containers.message));
  QCOMPARE(containers.containers.size(), 1);
  QCOMPARE(containers.containers.constFirst().containerId,
           QStringLiteral("c1"));
  QCOMPARE(containers.containers.constFirst().revision, quint64(7));
  QCOMPARE(containers.containers.constFirst().authority,
           TaskListContainerAuthority::HybridProcess);
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
}

// AGENT-NOTE: Review finding P1-1/P2-1 on rejected candidate 3a5ae17:
// Windows() is the sole task-window inventory this producer accepts. Its own
// schema-2 epoch/revision must be canonical even though it is not permission
// to join the separate panel-visibility inventory.
void TaskListWireTests::windowsInventoryRequiresSchemaTwoLineage() {
  auto payload = [](const QJsonObject &extra,
                    std::initializer_list<const char *> remove = {}) {
    QJsonObject root{{QStringLiteral("status"), QStringLiteral("ok")},
                     {QStringLiteral("schemaVersion"), 2},
                     {QStringLiteral("epoch"), kWindowEpoch},
                     {QStringLiteral("revision"), QStringLiteral("1")},
                     {QStringLiteral("generationAvailable"), true},
                     {QStringLiteral("windows"), QJsonArray{}}};
    for (const char *key : remove) {
      root.remove(QLatin1StringView(key));
    }
    for (auto it = extra.constBegin(); it != extra.constEnd(); ++it) {
      root.insert(it.key(), it.value());
    }
    return rawRoot(root);
  };
  QCOMPARE(TaskListWireDecoder::decodeWindows(payload({})).error,
           TaskListWireError::None);
  QCOMPARE(TaskListWireDecoder::decodeWindows(
               payload({{QStringLiteral("schemaVersion"), 1}}))
               .error,
           TaskListWireError::UnsupportedSchema);
  QCOMPARE(TaskListWireDecoder::decodeWindows(payload({}, {"epoch"})).error,
           TaskListWireError::InvalidLineage);
  QCOMPARE(TaskListWireDecoder::decodeWindows(
               payload({{QStringLiteral("epoch"),
                         QStringLiteral("not-a-uuid")}}))
               .error,
           TaskListWireError::InvalidLineage);
  QCOMPARE(TaskListWireDecoder::decodeWindows(
               payload({{QStringLiteral("revision"), QStringLiteral("007")}}))
               .error,
           TaskListWireError::InvalidLineage);
  QCOMPARE(TaskListWireDecoder::decodeWindows(
               payload({}, {"generationAvailable"}))
               .error,
           TaskListWireError::InvalidLineage);
  QCOMPARE(TaskListWireDecoder::decodeWindows(
               payload({{QStringLiteral("revision"), QStringLiteral("0")}}))
               .error,
           TaskListWireError::InvalidLineage);
  QCOMPARE(TaskListWireDecoder::decodeWindows(
               payload({{QStringLiteral("revision"), QStringLiteral("0")},
                        {QStringLiteral("generationAvailable"), false}}))
               .error,
           TaskListWireError::None);
}

// AGENT-NOTE: Review finding P2-1 on rejected candidate 3a5ae17: the 4,096 /
// 4,097 Windows() boundary is registered evidence, not a scratch benchmark.
void TaskListWireTests::windowsInventoryBoundIsExact() {
  const auto atLimit = TaskListWireDecoder::decodeWindows(
      standaloneScene(4096).windows);
  QVERIFY2(atLimit.ok(), qPrintable(atLimit.message));
  QCOMPARE(atLimit.windows.size(), 4096);
  QCOMPARE(TaskListWireDecoder::decodeWindows(standaloneScene(4097).windows)
               .error,
           TaskListWireError::LimitExceeded);
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
  controlTitle.insert(QStringLiteral("title"), QStringLiteral("bad\ttitle"));
  QCOMPARE(TaskListWireDecoder::decodeWindows(withWindow(controlTitle)).error,
           TaskListWireError::InvalidWindow);

  auto loneSurrogate = windowJson(QStringLiteral("w1"), QStringLiteral("app"));
  loneSurrogate.insert(QStringLiteral("applicationId"),
                       QStringLiteral("app") + QChar(0xD800));
  QCOMPARE(TaskListWireDecoder::decodeWindows(withWindow(loneSurrogate)).error,
           TaskListWireError::InvalidWindow);

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
               containersPayload({{QStringLiteral("c1"), 1,
                                   QStringLiteral("control-bridge")},
                                  {QStringLiteral("c1"), 2,
                                   QStringLiteral("hybrid-process")}}))
               .error,
           TaskListWireError::DuplicateContainerId);
}

QTEST_GUILESS_MAIN(TaskListWireTests)
#include "tst_task_list_wire.moc"
